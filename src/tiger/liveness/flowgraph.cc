#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() { /* TODO: Put your lab6 code here */
  std::list<FNodePtr> jump_node;
 int edges=0;
  auto instrList = instr_list_->GetList();
  FNodePtr cur = nullptr;
  FNodePtr prev = nullptr;
  for (auto instr : instrList) {
    bool is_jmp = false;
    cur = flowgraph_->NewNode(instr);

    // for the case that instr is label
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      auto labelInstr = dynamic_cast<assem::LabelInstr *>(instr);
      auto label = labelInstr->label_;
      label_map_->Enter(label, cur);
    } else if (typeid(*instr) == typeid(assem::OperInstr)) {
      auto operInstr = dynamic_cast<assem::OperInstr *>(instr);
      // if its a jump instr, then handle it later
      if (operInstr->jumps_) {
        jump_node.push_back(cur);
        is_jmp = operInstr->assem_.find("jmp") != std::string::npos;
      }
    }

    // if there exists the last node, then connect prev to cur.
    if (prev) {
      edges++;
      flowgraph_->AddEdge(prev, cur);
    }

    // update prev; If current instr is jump, then prev should be null.
    prev = !is_jmp ? cur : nullptr;
  }

  // Then let's deal with those jump_node
  for (auto jumpNode : jump_node) {
    auto instr = jumpNode->NodeInfo();
    auto jmpInstr = dynamic_cast<assem::OperInstr *>(instr);
    auto label = (*jmpInstr->jumps_->labels_)[0];
    auto labelNode = label_map_->Look(label);
    // just link two.
    // since it will jump from jumpNode to the label Node.
    flowgraph_->AddEdge(jumpNode, labelNode);
    edges++;
  }
  std::cout<<"edges="<<edges<<std::endl;
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList({});
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
//  return dst_;
  auto ret = dst_ != nullptr ? dst_ : new temp::TempList({});
  return ret;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  auto ret = dst_ != nullptr ? dst_ : new temp::TempList({});
  return ret;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList({});
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  auto ret = src_ ? src_ : new temp::TempList({});
  return ret;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  auto ret = src_ ? src_ : new temp::TempList({});
  return ret;
}
} // namespace assem
