#include "tiger/regalloc/regalloc.h"
#include "tiger/output/logger.h"
#include <regex>
#include <set>

#define K 15
#define word_size 8

extern frame::RegManager *reg_manager;

namespace ra {

// ok
void RegAllocator::RegAlloc() {
  std::cout << "RegAllocator::RegAlloc called" << std::endl;
  while (true) {
    LivenessAnalysis();
    Build();
    MakeWorkList();
    while (true) {
      if (!simplify_worklist->Empty()) {
        Simplify();
      } else if (!worklist_moves->Empty()) {
        Coalesce();
      } else if (!freeze_worklist->Empty()) {
        Freeze();
      } else if (!spill_worklist->Empty()) {
        SelectSpill();
      }
      if (simplify_worklist->Empty() && worklist_moves->Empty() &&
          freeze_worklist->Empty() && spill_worklist->Empty()) {
        break;
      }
    }
    AssignColors();
    if (!spilled_nodes->Empty()) {
      RewriteProgram();
    } else {
      break;
    }
    std::cout << "Color: finish one time" << std::endl;
  }
  std::cout << "color finish" << std::endl;

  // 回填framesize 删除多余的move
  auto il = assem_instr_->GetInstrList();
  auto &instr_list = il->GetList();
  std::string assem = frame->name_->Name() + "_framesize";
  for (auto it = instr_list.begin(); it != instr_list.end();) {
    auto instr = *it;
    instr->assem_ =
        std::regex_replace(instr->assem_, std::regex(assem),
                           std::to_string(frame->locals * word_size));
    if (typeid(*instr) == typeid(assem::OperInstr)) {
      auto tmp = (assem::OperInstr *)instr;
      SetColor(tmp->src_);
      SetColor(tmp->dst_);
    } else if (typeid(*instr) == typeid(assem::MoveInstr)) {
      auto tmp = (assem::MoveInstr *)instr;
      auto src = tmp->src_;
      auto dst = tmp->dst_;
      if (SameMove(src, dst)) {
        it = instr_list.erase(it);
        continue;
      } else {
        SetColor(src);
        SetColor(dst);
      }
    }
    it++;
  }
  result->il_ = il;
}

// ok
void RegAllocator::LivenessAnalysis() {
  auto fg_factory =
      new fg::FlowGraphFactory(assem_instr_.get()->GetInstrList());
  fg_factory->AssemFlowGraph();
  auto fg = fg_factory->GetFlowGraph();

  auto lg_factory = new live::LiveGraphFactory(fg);
  lg_factory->Liveness();
  auto live_graph = lg_factory->GetLiveGraph();

  worklist_moves = live_graph.moves;
  move_list = live_graph.move_list;
  interf_graph = live_graph.interf_graph;
}

// ok
void RegAllocator::Build() {
  precolored->Clear();
  initial->Clear();
  degree.clear();
  alias.clear();
  color.clear();

  std::list<graph::Node<temp::Temp> *> node_list =
      interf_graph->Nodes()->GetList();

  for (auto node : node_list) {
    degree[node] = node->OutDegree();
    alias[node] = node;
    auto temp = node->NodeInfo();
    if (temp->num_ >= 100) { // 不是寄存器的情况
      initial->Append(node);
    } else {
      precolored->Append(node);
      color[node->NodeInfo()] = node->NodeInfo()->Int();
    }
  }

  //  std::list<std::pair<INodePtr, INodePtr>> moves =
  //      live_graph.moves->GetList(); // std::list<std::pair<INodePtr,
  //      INodePtr>>
  //  for (std::pair<INodePtr, INodePtr> move : moves) {
  //    auto src = move.first;
  //    auto dst = move.second;
  //    move_list[src]->Append(src, dst);
  //    move_list[dst]->Append(src, dst);
  //  }
  //  worklist_moves = live_graph.moves;
  //  std::list<graph::Node<temp::Temp> *> node_list =
  //      live_graph.interf_graph->Nodes()->GetList();
  //  for (auto node : node_list) {
  //    auto adj_list = node->Adj()->GetList();
  //    for (auto adj : adj_list) {
  //      AddEdge(node, adj);
  //    }
  //  }
}

// ok
void RegAllocator::AddEdge(graph::Node<temp::Temp> *u,
                           graph::Node<temp::Temp> *v) {
  if (u->Succ()->Contain(v) || v->Succ()->Contain(u) || u == v) {
    return;
  }
  if (!precolored->Contain(u)) {
    interf_graph->AddEdge(u, v);
    degree[u]++;
  }
  if (!precolored->Contain(v)) {
    interf_graph->AddEdge(v, u);
    degree[v]++;
  }
}

// ok
void RegAllocator::MakeWorkList() {
  std::list<graph::Node<temp::Temp> *> node_list = initial->GetList();
  for (auto node : node_list) {
    if (degree[node] >= K) {
      spill_worklist->Append(node);
    } else if (MoveRelated(node)) {
      freeze_worklist->Append(node);
    } else {
      simplify_worklist->Append(node);
    }
  }
  initial->Clear();
}

// ok
live::MoveList *RegAllocator::NodeMoves(graph::Node<temp::Temp> *n) {
  auto tmp = move_list->Look(n);
  if (tmp == nullptr) {
    tmp = new live::MoveList();
  }
  return tmp->Intersect(active_moves->Union(worklist_moves));
}

// ok
bool RegAllocator::MoveRelated(graph::Node<temp::Temp> *n) {
  return !NodeMoves(n)->Empty();
}

// ok
void RegAllocator::Simplify() {
  if (simplify_worklist->Empty()) {
    return;
  }
  auto n = simplify_worklist->Pop();
  select_stack.push(n);
  interf_graph->DeleteNode(n);
  auto list = n->Succ()->GetList();
  for (auto m : list) {
    DecrementDegree(m);
  }
}

// ok
void RegAllocator::DecrementDegree(graph::Node<temp::Temp> *m) {
  degree[m]--;
  if (precolored->Contain(m)) {
    return;
  }
  if (degree[m] == K - 1) {
    auto tmp = new graph::NodeList<temp::Temp>(m);
    EnableMoves(m->Succ()->Union(tmp));
    spill_worklist->DeleteNode(m);
    if (MoveRelated(m)) {
      freeze_worklist->Append(m); // TODO: 考虑去重
    } else {
      simplify_worklist->Append(m);
    }
  }
}

// ok
void RegAllocator::EnableMoves(graph::NodeList<temp::Temp> *nodes) {
  auto node_list = nodes->GetList();
  for (auto n : node_list) {
    auto move_list = NodeMoves(n)->GetList();
    for (auto move : move_list) {
      if (active_moves->Contain(move.first, move.second)) {
        active_moves->Delete(move.first, move.second);
        worklist_moves->Append(move.first, move.second);
      }
    }
  }
}

// ok
void RegAllocator::Coalesce() {
  auto m = worklist_moves->Pop();
  graph::Node<temp::Temp> *x, *y, *u, *v;
  x = m.first;
  y = m.second;
  x = GetAlias(x);
  y = GetAlias(y);
  if (precolored->Contain(y)) {
    u = y;
    v = x;
  } else {
    u = x;
    v = y;
  }
  if (u == v) {
    coalesced_moves->Append(x, y);
    AddWorkList(u);
  } else if (precolored->Contain(v) || v->Succ()->Contain(u)) {
    constrained_moves->Append(x, y);
    AddWorkList(u);
    AddWorkList(v);
  } else {
    auto tmp_list = u->Succ()->Union(v->Succ());
    if ((precolored->Contain(u) && judge_ok(u, v)) ||
        (!precolored->Contain(u)) && Conservative(tmp_list)) {
      coalesced_moves->Append(x, y);
      Combine(u, v);
      AddWorkList(u);
    } else {
      active_moves->Append(x, y);
    }
  }
}

// ok
void RegAllocator::AddWorkList(graph::Node<temp::Temp> *u) {
  if (!precolored->Contain(u) && !MoveRelated(u) && degree[u] < K) {
    freeze_worklist->DeleteNode(u);
    simplify_worklist->Append(u);
  }
}

// ok
bool RegAllocator::OK(graph::Node<temp::Temp> *t, graph::Node<temp::Temp> *r) {
  return (degree[t] < K || precolored->Contain(t) || r->Succ()->Contain(t));
}

// ok
bool RegAllocator::Conservative(graph::NodeList<temp::Temp> *nodes) {
  int k = 0;
  auto node_list = nodes->GetList();
  for (auto n : node_list) {
    if (degree[n] >= k)
      k++;
  }
  return k < K;
}

// ok
graph::Node<temp::Temp> *RegAllocator::GetAlias(graph::Node<temp::Temp> *n) {
  if (coalesced_nodes->Contain(n)) {
    return GetAlias(alias[n]);
  } else {
    return n;
  }
}

// ok
void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  if (freeze_worklist->Contain(v)) {
    freeze_worklist->DeleteNode(v);
  } else {
    spill_worklist->DeleteNode(v);
  }
  coalesced_nodes->Append(v);
  alias[v] = u;
  auto tmp = move_list->Look(u)->Union(move_list->Look(v));
  move_list->Set(u, tmp);
  EnableMoves(new graph::NodeList<temp::Temp>(v));

  //  auto node_list = v->Succ()->GetList();
  for (auto node : v->Succ()->GetList()) {
    AddEdge(node, u);
    DecrementDegree(node);
  }

  if (degree[u] >= K && freeze_worklist->Contain(u)) {
    freeze_worklist->DeleteNode(u);
    spill_worklist->Append(u);
  }
}

// ok
void RegAllocator::Freeze() {
  auto u = freeze_worklist->Pop();
  simplify_worklist->Append(u);
  FreezeMoves(u);
}

// ok
void RegAllocator::FreezeMoves(graph::Node<temp::Temp> *u) {
  auto move_list = NodeMoves(u)->GetList();
  graph::Node<temp::Temp> *v, *x, *y;
  for (auto move : move_list) {
    x = move.first;
    y = move.second;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    active_moves->Delete(x, y);
    frozen_moves->Append(x, y);
    if (NodeMoves(v)->Empty() && degree[v] < K) {
      freeze_worklist->DeleteNode(v);
      simplify_worklist->Append(v);
    }
  }
}

// ok
void RegAllocator::SelectSpill() {
  graph::Node<temp::Temp> *node = nullptr;
  int maxDegree = 0;
  auto spill_list = spill_worklist->GetList();
  //启发式: 选度数最大的 选不到就选worklist最前面的
  for (auto n : spill_list) {
    if (already_spill->Contain(n->NodeInfo())) {
      continue;
    }
    if (degree[n] > maxDegree) {
      node = n;
      maxDegree = degree[n];
    }
  }
  if (node) {
    spill_worklist->DeleteNode(node);
  } else {
    node = spill_worklist->Pop();
  }
  simplify_worklist->Append(node);
  FreezeMoves(node);

  //  double chosen_weight=-1;
  //  std::list<Node<temp::Temp> *> node_list = spill_worklist->GetFirst();
  //  for(auto node: node_list) {
  //    if(spill_temp.find(node->NodeInfo())!=spill_temp.end()) {
  //      continue;
  //    }
  //    if(chose_weight==-1 || live_graph.weight[node->NodeInfo())]<res]) {
  //        chosen_node=node;
  //        chosen_weight=live_graph.weight[node->NodeInfo()];
  //      }
  //  }
}

// ok
void RegAllocator::AssignColors() {
  while (!select_stack.empty()) {
    auto n = select_stack.top();
    select_stack.pop();
    if (precolored->Contain(n)) {
      continue;
    }
    // rsp, rbp最好不要用
    std::set<int> ok_colors;
    int rsp = reg_manager->StackPointer()->Int();
    for (int i = 0; i <= K; i++) {
      if (i != rsp && i != rsp + 1)
        ok_colors.insert(i);
    }

    auto node_list = n->Succ()->GetList();
    for (auto w : n->Succ()->GetList()) {
      if (color.find(GetAlias(w)->NodeInfo()) != color.end()) {
        ok_colors.erase(color[GetAlias(w)->NodeInfo()]);
      }
    }
    if (ok_colors.empty()) {
      spilled_nodes->Append(n);
    } else {
      colored_nodes->Append(n);
      int c = *(ok_colors.begin()); // ok_colors里随意选一个
      color[n->NodeInfo()] = c;
    }
  }

  auto coalesced_list = coalesced_nodes->GetList();
  for (auto n : coalesced_list) {
    color[n->NodeInfo()] = color[GetAlias(n)->NodeInfo()];
  }
}

// ok
void RegAllocator::RewriteProgram() {
  graph::NodeList<temp::Temp> *new_temps = new graph::NodeList<temp::Temp>();
  auto spilled_list = spilled_nodes->GetList();
  std::string assem;
  for (auto v : spilled_list) {
    auto old_temp = v->NodeInfo();
    auto new_temp = temp::TempFactory::NewTemp();
    already_spill->Append(new_temp);

    temp::TempList *src = nullptr;
    temp::TempList *dst = nullptr;
    auto &instr_list = assem_instr_->GetInstrList()->GetList();
    frame->locals++;
    auto rsp = reg_manager->StackPointer();

    for (auto it = instr_list.begin(); it != instr_list.end(); it++) {
      auto instr = *it;
      if (typeid(*instr) == typeid(assem::MoveInstr)) {
        auto tmp = (assem::MoveInstr *)instr;
        src = tmp->src_;
        dst = tmp->dst_;
      } else if (typeid(*instr) == typeid(assem::OperInstr)) {
        auto tmp = (assem::OperInstr *)instr;
        src = tmp->src_;
        dst = tmp->dst_;
      }

      if (src && src->Contain(old_temp)) {
        src->Replace(old_temp, new_temp);
        assem = "movq (" + frame->name_->Name() + "_framesize-" +
                std::to_string(frame->locals * word_size) + ")(`s0), `d0";
        auto tmp = new assem::OperInstr(assem, new temp::TempList(new_temp),
                                        new temp::TempList(rsp), nullptr);
        it = instr_list.insert(it, tmp);
        it++;
      }

      if (dst && dst->Contain(old_temp)) {
        dst->Replace(old_temp, new_temp);
        assem = "movq `s0, (" + frame->name_->Name() + "_framesize-" +
                std::to_string(frame->locals * word_size) + ")(`d0)";
        auto tmp = new assem::OperInstr(assem, new temp::TempList(rsp),
                                        new temp::TempList(new_temp), nullptr);
        it = instr_list.insert(std::next(it), tmp);
      }
    }
  }
  // initial在build中定义
  spilled_nodes->Clear();
  colored_nodes->Clear();
  coalesced_nodes->Clear();
}

void RegAllocator::SetColor(temp::TempList *temps) {
  if (!temps) {
    return;
  }
  auto regs = reg_manager->Registers();
  auto map = temp::Map::Name();
  auto temp_list = temps->GetList();
  for (auto temp : temp_list) {
    auto tmp = map->Look(regs->NthTemp(color[temp]));
    map->Set(temp, tmp);
  }
}

// ok
// p175 去除src和dst相同的情况
bool RegAllocator::SameMove(temp::TempList *src, temp::TempList *dst) {
  if (src && dst) {
    return color[src->GetList().front()] == color[dst->GetList().front()];
  }
  return false;
}

// ok
bool RegAllocator::judge_ok(graph::Node<temp::Temp> *u,
                            graph::Node<temp::Temp> *v) {
  auto node_list = v->Succ()->GetList();
  for (auto node : node_list) {
    if (!OK(node, u))
      return false;
  }
  return true;
}

} // namespace ra