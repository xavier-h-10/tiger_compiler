#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  //  std::cout << "FlowGraphFactory::AssemFlowGraph called" << std::endl;
  //  flowgraph_ = new graph::Graph<assem::Instr>();
  //  auto label_table = new tab::Table<temp::Label,
  //  graph::Node<assem::Instr>>(); auto instr_table = new
  //  tab::Table<assem::Instr, graph::Node<assem::Instr>>();

  int edges = 0;

  std::list<assem::Instr *> instr_list = instr_list_->GetList();
  graph::Node<assem::Instr> *now = nullptr;
  graph::Node<assem::Instr> *prev = nullptr;
  std::vector<graph::Node<assem::Instr> *> jump_nodes;

  // jump单独处理 其余加边
  for (auto instr : instr_list) {
    bool is_jmp=false;
    now = flowgraph_->NewNode(instr);

    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_table->Enter(((assem::LabelInstr *)instr)->label_, now);
    }

    if (typeid(*instr) == typeid(assem::OperInstr)) {
      auto oper_instr = ((assem::OperInstr *)instr);
      if (oper_instr->jumps_) {
        jump_nodes.push_back(now);
        is_jmp = oper_instr->assem_.find("jmp") != std::string::npos;
      }
    }

    if (prev) {
      edges++;
      flowgraph_->AddEdge(prev, now);
    }

    if (is_jmp) {
      prev = nullptr;
    } else {
      prev = now;
    }
  }

  for (auto node : jump_nodes) {
    auto instr = (assem::OperInstr *)(node->NodeInfo());
    auto label = (*instr->jumps_->labels_)[0];
    flowgraph_->AddEdge(node, label_table->Look(label));
    edges++;
  }

  //  std::cout << "edges=" << edges << std::endl;
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Def() const {
  if (dst_) {
    return dst_;
  } else {
    return new temp::TempList();
  }
}

temp::TempList *OperInstr::Def() const {
  if (dst_) {
    return dst_;
  } else {
    return new temp::TempList();
  }
}

temp::TempList *LabelInstr::Use() const { return new temp::TempList(); }

temp::TempList *MoveInstr::Use() const {
  if (src_) {
    return src_;
  } else {
    return new temp::TempList();
  }
}

temp::TempList *OperInstr::Use() const {
  if (src_) {
    return src_;
  } else {
    return new temp::TempList();
  }
}
} // namespace assem

//
// for (auto instr : instr_list) {
//  if (typeid(*instr) != typeid(assem::OperInstr))
//    continue;
//  assem::OperInstr *oper_instr = (assem::OperInstr *)instr;
//  if (oper_instr->jumps_ == nullptr)
//    continue;
//
//  std::vector<temp::Label *> labels = *(oper_instr->jumps_->labels_);
//  for (auto label : labels) {
//    edges++;
//    flowgraph_->AddEdge(instr_table->Look(instr), label_table->Look(label));
//  }
//}