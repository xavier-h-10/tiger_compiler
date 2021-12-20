#include "tiger/regalloc/regalloc.h"
#include "tiger/output/logger.h"
#include <regex>
#include <set>
extern frame::RegManager *reg_manager;

namespace ra {


/* TODO: Put your lab6 code here */
void RegAllocator::RegAlloc() {
  Color();
  AssignRegisters();
}

void RegAllocator::Color() {
  bool done;
  int x = 0;
  std::cout << "Called" << std::endl;
  do {
    done = true;
    LivenessAnalysis();
    Build();
    MakeWorkList();
    do {
      if (!simplify_worklist->Empty()) {
        Simplify();
      } else if (!worklist_moves->Empty()) {
        Coalesce();
      } else if (!freeze_worklist->Empty()) {
        Freeze();
      } else if (!spill_worklist->Empty()) {
        SelectSpill();
      }
    } while (!(simplify_worklist->Empty() && worklist_moves->Empty() &&
               freeze_worklist->Empty() && spill_worklist->Empty()));
    AssignColors();
    if (!spilled_nodes->Empty()) {
      RewriteProgram();
      done = false;
    }
    ++x;
    std::cout << "Finished " << x << std::endl;
    //  } while (0);
  } while (!done);

  std::cout << "Finished All\n\n";
}

void RegAllocator::Build() {
  initial->Clear();
  precolored->Clear();
  alias.clear();
  colors.clear();
  degree.clear();
  std::list<graph::Node<temp::Temp> *> node_list = interf_graph->Nodes()->GetList();
  for (auto node : node_list) {
    degree[node] = node->OutDegree();
    alias[node] = node;
    auto temp = node->NodeInfo();
    if (!temp->isBuiltin()) {
      initial->Append(node);
    } else {
      precolored->Append(node);
      colors[node->NodeInfo()] = node->NodeInfo()->Int();
    }
  }
}

void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (!u->Succ()->Contain(v) && u != v) {
    if (!precolored->Contain(u)) {
      interf_graph->AddEdge(u, v);
      degree[u]++;
    }
    if (!precolored->Contain(v)) {
      interf_graph->AddEdge(v, u);
      degree[v]++;
    }
  }
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
  return !NodeMoves(node)->Empty();
}

void RegAllocator::MakeWorkList() {
  auto &nodes = initial->GetList();
  for (auto n : nodes) {
    if (degree[n] >= K) {
      spill_worklist->Append(n);
    } else if (MoveRelated(n)) {
      freeze_worklist->Append(n);
    } else {
      simplify_worklist->Append(n);
    }
  }
  initial->Clear();
}

live::MoveList *RegAllocator::NodeMoves(live::INodePtr node) {
  live::MoveList *moveList_n = moveList->Look(node);
  moveList_n = moveList_n ? moveList_n : new live::MoveList();
  return moveList_n->Intersect(active_moves->Union(worklist_moves));
}

bool RegAllocator::MoveRelated(live::INodePtr node) {
  return !NodeMoves(node)->Empty();
}

void RegAllocator::Simplify() {
  if (simplify_worklist->Empty())
    return;

  auto node = simplify_worklist->PopFront();
  select_stack.push(node);

  interf_graph->DeleteNode(node);

  for (auto adj : node->Succ()->GetList()) {
    DecrementDegree(adj);
  }
}

void RegAllocator::DecrementDegree(live::INodePtr m) {
  --degree[m];
  if (degree[m] == K - 1 && !precolored->Contain(m)) {
    EnableMoves(m->Succ()->Union(new graph::NodeList<temp::Temp>({m})));
    spill_worklist->DeleteNode(m);
    if (MoveRelated(m)) {
      freeze_worklist->Append(m);
    } else {
      simplify_worklist->Append(m);
    }
  }
}

void RegAllocator::EnableMoves(graph::NodeList<temp::Temp> *nodes) {
  for (auto n : nodes->GetList()) {
    auto nodeMvs = NodeMoves(n);
    for (auto m : nodeMvs->GetList()) {
      auto src = m.first;
      auto dst = m.second;
      if (active_moves->Contain(src, dst)) {
        active_moves->Delete(src, dst);
        worklist_moves->Append(src, dst);
      }
    }
  }
}

void RegAllocator::Coalesce() {
  auto m = worklist_moves->PopFront();
  auto x = GetAlias(m.first);
  auto y = GetAlias(m.second);
  live::INodePtr u, v;
  if (precolored->Contain(y)) {
    u = y;
    v = x;
    //    (u, v) = (y, x);
  } else {
    u = x;
    v = y;
    //    (u, v) = (x, y);
  }
  if (u == v) {
    coalesced_moves->Append(x, y);
    AddWorkList(u);
  } else if (precolored->Contain(v) || v->Succ()->Contain(u)) {
    constrained_moves->Append(x, y);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((precolored->Contain(u) && CoalesceOK(u, v)) ||
             (!precolored->Contain(u)) &&
                 Conservative(u->Succ()->Union(v->Succ()))) {
    coalesced_moves->Append(x, y);
    Combine(u, v);
    AddWorkList(u);
  } else {
    active_moves->Append(x, y);
  }
}

void RegAllocator::AddWorkList(live::INodePtr node) {
  if (!precolored->Contain(node) && !MoveRelated(node) && degree[node] < K) {
    freeze_worklist->DeleteNode(node);
    simplify_worklist->Append(node);
  }
}

bool RegAllocator::OK(live::INodePtr t, live::INodePtr r) {
  return degree[t] < K || precolored->Contain(t) || r->Succ()->Contain(t);
}

bool RegAllocator::Conservative(graph::NodeList<temp::Temp> *nodes) {
  int k = 0;
  for (auto n : nodes->GetList()) {
    if (degree[n] >= k)
      ++k;
  }
  return k < K;
}

live::INodePtr RegAllocator::GetAlias(live::INodePtr node) {
  return coalesced_nodes->Contain(node) ? GetAlias(alias[node]) : node;
}

void RegAllocator::Combine(live::INodePtr u, live::INodePtr v) {
  if (freeze_worklist->Contain(v)) {
    freeze_worklist->DeleteNode(v);
  } else {
    spill_worklist->DeleteNode(v);
  }
  coalesced_nodes->Append(v);
  alias[v] = u;
  moveList->Set(u, moveList->Look(u)->Union(moveList->Look(v)));
  EnableMoves(new graph::NodeList<temp::Temp>({v}));

  for (auto t : v->Succ()->GetList()) {
    AddEdge(t, u);
    DecrementDegree(t);
  }

  if (degree[u] >= K && freeze_worklist->Contain(u)) {
    freeze_worklist->DeleteNode(u);
    spill_worklist->Append(u);
  }
}

void RegAllocator::Freeze() {
  auto u = freeze_worklist->PopFront();
  simplify_worklist->Append(u);
  FreezeMoves(u);
}

void RegAllocator::FreezeMoves(live::INodePtr u) {
  for (auto m : NodeMoves(u)->GetList()) {
    live::INodePtr v;
    auto x = m.first;
    auto y = m.second;
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

void RegAllocator::SelectSpill() {
  live::INodePtr m = nullptr;
  int maxDegree = 0;
  for (auto n : spill_worklist->GetList()) {
    if (alreadySpilled->Contain(n->NodeInfo())) {
      continue;
    }
    if (degree[n] > maxDegree) {
      m = n;
      maxDegree = degree[n];
    }
  }
  if (m)
    spill_worklist->DeleteNode(m);
  else
    m = spill_worklist->PopFront();
  simplify_worklist->Append(m);
  FreezeMoves(m);
}

void RegAllocator::AssignColors() {
  while (!select_stack.empty()) {
    auto n = select_stack.top();
    select_stack.pop();
    if (precolored->Contain(n)) {
      continue;
    }

    std::set<int> okColors;
    auto rsp = reg_manager->StackPointer()->Int();
    auto rbp = rsp + 1;
    for (int i = 0; i <= K; ++i) {
      if (i != rsp && i != rbp)
        okColors.insert(i);
    }

    for (auto w : n->Succ()->GetList()) {
      auto wAlias = GetAlias(w);

      if (colors.find(wAlias->NodeInfo()) != colors.end()) {
        okColors.erase(colors[wAlias->NodeInfo()]);
      }
    }

    if (okColors.empty()) {
      spilled_nodes->Append(n);
    } else {
      colored_nodes->Append(n);
      int c = *okColors.begin();
      colors[n->NodeInfo()] = c;
    }
  }
  for (auto n : coalesced_nodes->GetList()) {
    colors[n->NodeInfo()] = colors[GetAlias(n)->NodeInfo()];
  }
}

void RegAllocator::RewriteProgram() {
  graph::NodeList<temp::Temp> *newTemps = new graph::NodeList<temp::Temp>;
  alreadySpilled->Clear();
  auto il = assemInstr->GetInstrList();
  char buf[256];
  const int wordSize = reg_manager->WordSize();
  for (auto v : spilled_nodes->GetList()) {
    auto newTemp = temp::TempFactory::NewTemp(); // namely vi
    alreadySpilled->Append(newTemp);
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

bool RegAllocator::CoalesceOK(live::INodePtr u, live::INodePtr v) {
  for (auto t : v->Succ()->GetList()) {
    if (!OK(t, u))
      return false;
  }
  return true;
}

void RegAllocator::LivenessAnalysis() {
  auto il = assemInstr.get()->GetInstrList();
  fg::FlowGraphFactory flowGraphFactory(il);
  flowGraphFactory.AssemFlowGraph();
  auto flowGraph = flowGraphFactory.GetFlowGraph();
  liveGraphFactory = new live::LiveGraphFactory(flowGraph);
  liveGraphFactory->Liveness();
  auto liveGraph = liveGraphFactory->GetLiveGraph();
  interf_graph = liveGraph.interf_graph;
  worklist_moves = liveGraph.moves;
  moveList = liveGraph.moveList;
}

void RegAllocator::AssignTemps(temp::TempList *temps) {
  if (!temps)
    return;
  static auto regs = reg_manager->Registers();
  auto map = temp::Map::Name();
  for (auto temp : temps->GetList()) {
    //    std::cout << "color is: " << colors[temp] << std::endl;
    auto tgt = map->Look(regs->NthTemp(colors[temp]));
    //    std::cout << "assign: " << *map->Look(temp) << " to "
    //              << *map->Look(regs->NthTemp(colors[temp])) << std::endl;
    map->Set(temp, tgt);
  }
}

bool RegAllocator::SameMove(temp::TempList *src, temp::TempList *dst) {
  static auto map = temp::Map::Name();
  if (src && dst) {
    return colors[src->GetList().front()] == colors[dst->GetList().front()];
  }
  return false;
}

void RegAllocator::AssignRegisters() {
  auto il = assemInstr.get()->GetInstrList();
  //  /// TODO: something wrong with it.
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

} // namespace ra