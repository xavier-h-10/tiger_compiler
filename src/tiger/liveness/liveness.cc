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

void LiveGraphFactory::LiveMap() {
  std::cout << "LiveGraphFactory::LiveMap called" << std::endl;

  //  in.clear();
  //  out.clear();

  std::list<graph::Node<assem::Instr> *> node_list =
      flowgraph_->Nodes()->GetList();
  std::cout << "node_list size:" << node_list.size() << std::endl;

  for (auto node : node_list) {
    in[node] = new temp::TempList();
    out[node] = new temp::TempList();
  }

  while (true) {
    //    std::map<graph::Node<assem::Instr> *, temp::TempList *> old_in = in;
    //    std::map<graph::Node<assem::Instr> *, temp::TempList *> old_out = out;
    int old_in_size, old_out_size, in_size, out_size;
    auto it = node_list.rbegin();
    bool equal = true;

    for (; it != node_list.rend(); it++) {
      auto node = *it;
      auto defs = node->NodeInfo()->Def();
      auto uses = node->NodeInfo()->Use();
      int old_in_size = in[node]->Size();
      int old_out_size = out[node]->Size();

      //      out[node] = new temp::TempList();
      auto succ_list = node->Succ()->GetList();
      for (auto succ_node : succ_list) {
        out[node] = Union(out[node], in[succ_node]);
      }

      in[node] = Union(uses, Diff(out[node], defs));

      int in_size = in[node]->Size();
      int out_size = out[node]->Size();

      std::cout << "before:" << old_in_size << " " << old_out_size << std::endl;
      std::cout << "after:" << in_size << " " << out_size << std::endl;
      if (old_in_size != in_size || old_out_size != out_size) {
        equal = false;
      }
    };
    if (equal) {
      break;
    }
  }

  //  std::list<temp::Temp *> reg_list = reg_manager->Registers()->GetList();
  //  for (auto reg_1 : reg_list) {
  //    for (auto reg_2 : reg_list) {
  //      if (reg_1 == reg_2) {
  //        continue;
  //      }
  //      auto node_1 = GetNode(live_graph_.interf_graph, reg_1);
  //      auto node_2 = GetNode(live_graph_.interf_graph, reg_2);
  //      if (node_1 != node_2) {
  //        graph::Graph<temp::Temp>::AddEdge(node_1, node_2);
  //        graph::Graph<temp::Temp>::AddEdge(node_2, node_1);
  //      }
  //    }
  //  }
}

INodePtr LiveGraphFactory::GetNode(temp::Temp *temp) {
  if (temp_node_map_.find(temp) != temp_node_map_.end()) {
    return temp_node_map_[temp];
  }
  auto node = live_graph_.interf_graph->NewNode(temp);
  temp_node_map_[temp] = node;
  return node;
}

void LiveGraphFactory::AddMoveList(
    graph::Table<temp::Temp, MoveList> *move_list, INodePtr node, INodePtr src,
    INodePtr dst) {
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
  std::list<graph::Node<assem::Instr> *> node_list =
      flowgraph_->Nodes()->GetList();
  for (auto it = node_list.rbegin(); it != node_list.rend(); it++) {
    auto node = *it;
    auto instr = node->NodeInfo();
    std::list<temp::Temp *> defs = instr->Def()->GetList();
    std::list<temp::Temp *> uses = instr->Use()->GetList();

    temp::TempList *live_list = out[node];

    if (typeid(*instr) == typeid(assem::MoveInstr) && !defs.empty() &&
        !uses.empty()) {
      auto dst = GetNode(*(defs.begin()));
      auto src = GetNode(*(uses.begin()));
      live_graph_.moves->Append(src, dst);
      AddMoveList(live_graph_.move_list, src, src, dst);
      AddMoveList(live_graph_.move_list, dst, src, dst);
      live_list->Diff(instr->Use());
    }

    live_list->Union(instr->Def());
    std::list<temp::Temp *> temp_list = live_list->GetList();
    for (auto def : defs) {
      graph::Node<temp::Temp> *dst = GetNode(def);
      for (auto temp : temp_list) {
        auto outs = GetNode(temp);
        live_graph_.interf_graph->AddEdge(dst, outs);
        live_graph_.interf_graph->AddEdge(outs, dst);
      }
    }
  }
}

MoveList *LiveGraphFactory::getMoveList(INodePtr node) {
  return live_graph_.move_list->Look(node);
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

temp::TempList *LiveGraphFactory::Union(temp::TempList *a, temp::TempList *b) {
  if (a == nullptr && b == nullptr)
    return new temp::TempList();
  if (a == nullptr)
    return b;
  if (b == nullptr)
    return a;
  std::list<temp::Temp *> a_list = a->GetList();
  std::list<temp::Temp *> b_list = b->GetList();
  auto res_list = a_list;
  for (auto b_tmp : b_list) {
    auto pos = std::find(res_list.begin(), res_list.end(), b_tmp);
    if (pos == res_list.end()) {
      res_list.push_back(b_tmp);
    }
  }
  return new temp::TempList(res_list);
}

temp::TempList *LiveGraphFactory::Diff(temp::TempList *a, temp::TempList *b) {
  if (a == nullptr)
    return new temp::TempList();
  if (b == nullptr)
    return a;
  std::list<temp::Temp *> a_list = a->GetList();
  std::list<temp::Temp *> b_list = b->GetList();
  auto res_list = a_list;
  for (auto b_tmp : b_list) {
    auto pos = std::find(res_list.begin(), res_list.end(), b_tmp);
    if (pos != res_list.end()) {
      res_list.erase(pos);
    }
  }
  return new temp::TempList(res_list);
}

bool LiveGraphFactory::Equal(temp::TempList *a, temp::TempList *b) {
  if (a == nullptr && b == nullptr)
    return true;
  if (a == nullptr || b == nullptr)
    return false;
  std::list<temp::Temp *> a_list = a->GetList();
  std::list<temp::Temp *> b_list = b->GetList();
  if (a_list.size() != b_list.size()) {
    return false;
  }
  for (auto a_tmp : a_list) {
    bool find = false;
    for (auto b_tmp : b_list) {
      if (a_tmp == b_tmp) {
        find = true;
        break;
      }
    }
    if (!find) {
      return false;
    }
  }
  return true;
}

bool LiveGraphFactory::Equal(IMap a, IMap b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (auto it = a.begin(); it != a.end(); it++) {
    auto key = it->first;
    auto val = it->second;
    if (b.find(key) == b.end()) {
      return false;
    }

    auto a_list = val->GetList();
    auto b_list = b[key]->GetList();
    for (auto temp : a_list) {
      if (std::find(b_list.begin(), b_list.end(), temp) == b_list.end()) {
        return false;
      }
    }
  }
  return true;
}

} // namespace live