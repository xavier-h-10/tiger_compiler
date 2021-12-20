#include "tiger/regalloc/regalloc.h"
#include "tiger/output/logger.h"
#include <regex>
#include <set>

#define K 15
extern frame::RegManager *reg_manager;

namespace ra {

void RegAllocator::RegAlloc() {
  std::cout << "RegAllocator::RegAlloc called" << std::endl;
  Color();
  AssignRegisters();
}

void RegAllocator::Color() {
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

  std::cout << "Color: finish!" << std::endl;
}

//ok
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

void RegAllocator::Build() {
  initial->Clear();
  precolored->Clear();
  alias.clear();
  colors.clear();
  degree.clear();
  std::list<graph::Node<temp::Temp> *> node_list =
      interf_graph->Nodes()->GetList();
  for (auto node : node_list) {
    degree[node] = node->OutDegree();
    alias[node] = node;
    auto temp = node->NodeInfo();
    if (temp->num_ >= 100) {
      initial->Append(node);
    } else {
      precolored->Append(node);
      colors[node->NodeInfo()] = node->NodeInfo()->Int();
    }
  }
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
      if (colors.find(GetAlias(w)->NodeInfo()) != colors.end()) {
        ok_colors.erase(colors[GetAlias(w)->NodeInfo()]);
      }
    }
    if (ok_colors.empty()) {
      spilled_nodes->Append(n);
    } else {
      colored_nodes->Append(n);
      int c = *(ok_colors.begin()); // ok_colors里随意选一个
      colors[n->NodeInfo()] = c;
    }
  }

  auto coalesced_list = coalesced_nodes->GetList();
  for (auto n : coalesced_list) {
    colors[n->NodeInfo()] = colors[GetAlias(n)->NodeInfo()];
  }
}

void RegAllocator::RewriteProgram() {
  graph::NodeList<temp::Temp> *newTemps = new graph::NodeList<temp::Temp>;
  already_spill->Clear();
  auto il = assem_instr_->GetInstrList();
  char buf[256];
  const int wordSize = reg_manager->WordSize();
  for (auto v : spilled_nodes->GetList()) {
    auto newTemp = temp::TempFactory::NewTemp(); // namely vi
    already_spill->Append(newTemp);
    auto oldTemp = v->NodeInfo();
    ++frame->localNumber;
    temp::TempList *src = nullptr, *dst = nullptr;
    auto &instrList = il->GetList();
    auto iter = instrList.begin();
    for (; iter != instrList.end(); ++iter) {
      auto instr = *iter;
      if (typeid(*instr) == typeid(assem::MoveInstr)) {
        auto moveInstr = dynamic_cast<assem::MoveInstr *>(instr);
        src = moveInstr->src_;
        dst = moveInstr->dst_;
      } else if (typeid(*instr) == typeid(assem::OperInstr)) {
        auto operInstr = dynamic_cast<assem::OperInstr *>(instr);
        src = operInstr->src_;
        dst = operInstr->dst_;
      }

      if (src && src->Contain(oldTemp)) {
        src->Replace(oldTemp, newTemp);
        sprintf(buf, "movq (%s_framesize-%d)(`s0), `d0",
                frame->func_->Name().c_str(), frame->localNumber * wordSize);
        auto newInstr = new assem::OperInstr(
            buf, new temp::TempList({newTemp}),
            new temp::TempList({reg_manager->StackPointer()}), nullptr);
        iter = instrList.insert(iter, newInstr);
        ++iter;
      }

      if (dst && dst->Contain(oldTemp)) {
        dst->Replace(oldTemp, newTemp);
        sprintf(buf, "movq `s0, (%s_framesize-%d)(`d0)",
                frame->func_->Name().c_str(), frame->localNumber * wordSize);
        auto newInstr = new assem::OperInstr(
            buf, new temp::TempList({reg_manager->StackPointer()}),
            new temp::TempList({newTemp}), nullptr);
        iter = instrList.insert(std::next(iter), newInstr);
      }
    }
  }

  spilled_nodes->Clear();
  colored_nodes->Clear();
  coalesced_nodes->Clear();
}

void RegAllocator::AssignTemps(temp::TempList *temps) {
  if (!temps)
    return;
  static auto regs = reg_manager->Registers();
  auto map = temp::Map::Name();
  for (auto temp : temps->GetList()) {
    auto tgt = map->Look(regs->NthTemp(colors[temp]));
    map->Set(temp, tgt);
  }
}

// ok
// p175 去除src和dst相同的情况
bool RegAllocator::SameMove(temp::TempList *src, temp::TempList *dst) {
  if (src && dst) {
    return colors[src->GetList().front()] == colors[dst->GetList().front()];
  }
  return false;
}

void RegAllocator::AssignRegisters() {
  auto il = assem_instr_.get()->GetInstrList();
  auto &instrList = il->GetList();
  auto iter = instrList.begin();
  char framesize_buf[256];
  sprintf(framesize_buf, "%s_framesize", frame->func_->Name().c_str());
  std::string framesize(framesize_buf);
  for (; iter != instrList.end();) {
    auto instr = *iter;
    instr->assem_ = std::regex_replace(instr->assem_, std::regex(framesize),
                                       std::to_string(frame->frameSize()));
    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      auto moveInstr = dynamic_cast<assem::MoveInstr *>(instr);
      auto src = moveInstr->src_;
      auto dst = moveInstr->dst_;
      if (!SameMove(src, dst)) {
        AssignTemps(src);
        AssignTemps(dst);
      } else {
        iter = instrList.erase(iter);
        continue;
      }
    } else if (typeid(*instr) == typeid(assem::OperInstr)) {
      auto operInstr = dynamic_cast<assem::OperInstr *>(instr);
      AssignTemps(operInstr->src_);
      AssignTemps(operInstr->dst_);
    }
    ++iter;
  }
  result->il_ = il;
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