#include "tiger/liveness/liveness.h"
#include <iostream>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!Contain(move.first, move.second))
      if (!res->Contain(move.first, move.second))
        res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Diff(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    if (!list->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *LiveGraphFactory::getMoveList(INodePtr node) {
  return live_graph_.move_list->Look(node);
}

void LiveGraphFactory::init() {
  auto instrNodes = flowgraph_->Nodes()->GetList();
  for (auto instrNode : instrNodes) {
    in_->Enter(instrNode, new temp::TempList({}));
    out_->Enter(instrNode, new temp::TempList({}));
  }
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  std::cout << "LiveGraphFactory::LiveMap called" << std::endl;
  bool no_change = false;
  init();
  auto instrNodes = flowgraph_->Nodes()->GetList();
//  std::cout<<"instr size="<<instrNodes.size()<<std::endl;
  while (!no_change) {
    auto iter = instrNodes.rbegin();
    for (; iter != instrNodes.rend(); ++iter) {
      auto instrNode = *iter;
      
      int before_out_size, before_in_size, after_out_size, after_in_size;
      // out[i] = out[i] \cup in[s]
      auto succ = instrNode->Succ()->GetList();
      auto my_out = out_->Look(instrNode);
      before_out_size = my_out->Size();
      for (auto succNode : succ) {
        auto succ_in = in_->Look(succNode);
        my_out->Union(succ_in);
      }
      after_out_size = my_out->Size();

      // in[i] = use[i] \cup (out[i] - def[i])
      auto my_in = in_->Look(instrNode);
      before_in_size = my_in->Size();

      auto uses = instrNode->NodeInfo()->Use();
      auto defs = instrNode->NodeInfo()->Def();
      auto diff = temp::TempList::Diff(my_out, defs);
      diff->Union(uses);
      my_in->Assign(diff);
      after_in_size = my_in->Size();
      delete diff;

      std::cout << "before:" << before_in_size << " " << before_out_size
                << std::endl;
      std::cout << "after:" << after_in_size << " " << after_out_size
                << std::endl;

      no_change =
          after_out_size == before_out_size && after_in_size == before_in_size;

      assert(after_out_size >= before_out_size &&
             after_in_size >= before_in_size);
    }
  }
}

INodePtr LiveGraphFactory::getNode(temp::Temp *temp) {
  INodePtr node;
  if ((node = temp_node_map_->Look(temp)) == nullptr) {
    node = live_graph_.interf_graph->NewNode(temp);
    temp_node_map_->Enter(temp, node);
  }
  return node;
}

void LiveGraphFactory::addMoveList(MvList *move_list, INodePtr node,
                                   INodePtr src, INodePtr dst) {
  auto mine = move_list->Look(node);
  if (mine == nullptr) {
    mine = new MoveList();
    move_list->Enter(node, mine);
  }
  if (!mine->Contain(src, dst))
    mine->Append(src, dst);
}

void LiveGraphFactory::InterfGraph() {
  std::cout << "LiveGraphFactory::InterfGraph called" << std::endl;
  auto interf_graph = live_graph_.interf_graph;
  auto moves = live_graph_.moves;
  auto move_list = live_graph_.move_list;
  auto instrNodes = flowgraph_->Nodes()->GetList();
  auto instr_iter = instrNodes.rbegin();
  for (; instr_iter != instrNodes.rend(); ++instr_iter) {
    auto instrNode = *instr_iter;
    auto instr = instrNode->NodeInfo();
    auto def = instr->Def();
    auto use = instr->Use();
    auto live = out_->Look(instrNode);
    auto orgLive = new temp::TempList({});
    orgLive->Assign(live);

    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      auto dstTemp = def->NthTemp(0);
      auto dstNode = getNode(dstTemp);
      if (!use->Empty()) {
        auto srcTemp = use->NthTemp(0);
        auto srcNode = getNode(srcTemp);
        moves->Append(srcNode, dstNode);
        addMoveList(move_list, srcNode, srcNode, dstNode);
        addMoveList(move_list, dstNode, srcNode, dstNode);
      }
      // live = live \ use
      live->Diff(use);
    }

    // live = live U def
    live->Union(def);

    for (auto defTemp : def->GetList()) {
      auto defNode = getNode(defTemp);
      for (auto liveTemp : live->GetList()) {
        auto liveNode = getNode(liveTemp);
        interf_graph->AddEdge(defNode, liveNode);
        interf_graph->AddEdge(liveNode, defNode);
      }
    }

    // recover live;
    live->Assign(orgLive);
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live