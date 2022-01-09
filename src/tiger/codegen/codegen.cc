#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

int frame_size = 0;

namespace cg {

void CodeGen::Codegen() {
  auto stm_list = traces_->GetStmList()->GetList();
  auto *list = new assem::InstrList();
  std::string_view fs;
  frame_size = frame_->size(); //调用codegen,生成新的frame_size
  for (auto stm : stm_list) {
    stm->Munch(*list, fs);
  }
  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  std::string op = "";
  switch (op_) {
  case EQ_OP: {
    op = "je";
    break;
  }
  case NE_OP: {
    op = "jne";
    break;
  }
  case LT_OP: {
    op = "jl";
    break;
  }
  case GT_OP: {
    op = "jg";
    break;
  }
  case LE_OP: {
    op = "jle";
    break;
  }
  case GE_OP: {
    op = "jge";
    break;
  }
  default: {
    break;
  }
  }

  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  std::vector<temp::Label *> *target =
      new std::vector<temp::Label *>({true_label_});
  instr_list.Append(new assem::OperInstr(
      "cmpq `d0, `d1", new temp::TempList({right, left}), nullptr, nullptr));
  instr_list.Append(new assem::OperInstr(op + " `j0", nullptr, nullptr,
                                         new assem::Targets(target)));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // think
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    temp::Temp *dst = ((tree::MemExp *)dst_)->exp_->Munch(instr_list, fs);
    temp::Temp *src = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, (`d0)",
                                           new temp::TempList({dst}),
                                           new temp::TempList({src})));
  } else if (typeid(*src_) == typeid(tree::MemExp)) {
    temp::Temp *dst = dst_->Munch(instr_list, fs);
    temp::Temp *src = ((tree::MemExp *)src_)->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq (`s0), `d0",
                                           new temp::TempList({dst}),
                                           new temp::TempList({src})));
  } else {
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({dst_->Munch(instr_list, fs)}),
        new temp::TempList({src_->Munch(instr_list, fs)})));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  temp::Temp *ret = temp::TempFactory::NewTemp();

  switch (op_) {
  case PLUS_OP: {
    // movq left, ret
    // addq right, ret
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({ret}),
                                           new temp::TempList({left})));
    instr_list.Append(
        new assem::OperInstr("addq `s0, `d0", new temp::TempList({ret}),
                             new temp::TempList({right}), nullptr));
    break;
  }
  case MINUS_OP: {
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({ret}),
                                           new temp::TempList({left})));
    instr_list.Append(
        new assem::OperInstr("subq `s0, `d0", new temp::TempList({ret}),
                             new temp::TempList({right}), nullptr));
    break;
  }
  case MUL_OP: {
    //只支持imulq xxx,需要保护
    temp::Temp *rax = reg_manager->rax;
    temp::Temp *rdx = reg_manager->rdx;
    temp::Temp *tmp1 = temp::TempFactory::NewTemp();
    temp::Temp *tmp2 = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({tmp1}),
                                           new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({tmp2}),
                                           new temp::TempList({rdx})));

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0", nullptr, new temp::TempList({right}), nullptr));

    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({ret}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({tmp1})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rdx}),
                                           new temp::TempList({tmp2})));
    break;
  }
  case DIV_OP: {
    temp::Temp *rax = reg_manager->rax;
    temp::Temp *rdx = reg_manager->rdx;
    temp::Temp *tmp1 = temp::TempFactory::NewTemp();
    temp::Temp *tmp2 = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({tmp1}),
                                           new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({tmp2}),
                                           new temp::TempList({rdx})));

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "idivq `s0", nullptr, new temp::TempList({right}), nullptr));

    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({ret}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({tmp1})));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rdx}),
                                           new temp::TempList({tmp2})));
    break;
  }
  }
  return ret;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *exp = exp_->Munch(instr_list, fs);
  temp::Temp *ret = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq (`s0), `d0", new temp::TempList({ret}), new temp::TempList({exp})));
  return ret;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 会遇到超过6个参数的情况,因此要做处理  leaq %rsp..
  if (temp_ != reg_manager->FramePointer()) {
    return temp_;
  }
  temp::Temp *ret = temp::TempFactory::NewTemp();
  std::string assem = "leaq " + std::to_string(frame_size) + "(`s0), `d0";
  instr_list.Append(new assem::OperInstr(
      assem, new temp::TempList({ret}),
      new temp::TempList({reg_manager->StackPointer()}), nullptr));
  return ret;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  // 先算stm 再算exp
  temp::Temp *ret = temp::TempFactory::NewTemp();
  stm_->Munch(instr_list, fs);
  temp::Temp *exp = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0", new temp::TempList({ret}), new temp::TempList({exp})));
  return ret;
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *ret = temp::TempFactory::NewTemp();
  std::string assem = "leaq " + name_->Name() + "(%rip), `d0"; // think
  instr_list.Append(
      new assem::OperInstr(assem, new temp::TempList({ret}), nullptr, nullptr));
  return ret;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *ret = temp::TempFactory::NewTemp();
  std::string assem = "movq $" + std::to_string(consti_) + ", `d0";
  instr_list.Append(
      new assem::MoveInstr(assem, new temp::TempList({ret}), nullptr));
  return ret;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *ret = temp::TempFactory::NewTemp();
  temp::Temp *rax = reg_manager->rax;
  std::string label = ((tree::NameExp *)fun_)->name_->Name(); // think
  temp::TempList *list = args_->MunchArgs(instr_list, fs);
  std::string assem = "callq " + std::string(label);
  instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, nullptr));
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0", new temp::TempList({ret}), new temp::TempList({rax})));
  //参数个数>6, 需要add stack pointer
  if (args_->GetList().size() > 6) {
    int size = (args_->GetList().size() - 6) * reg_manager->WordSize();
    std::string assem = "addq $" + std::to_string(size) + ", `d0";
    instr_list.Append(new assem::OperInstr(
        assem, new temp::TempList({reg_manager->StackPointer()}), nullptr,
        nullptr));
  }
  return ret;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  temp::TempList *list = new temp::TempList();
  temp::Temp *rsp = reg_manager->StackPointer();
  temp::Temp *ret = temp::TempFactory::NewTemp();
  int pos = 0;
  std::list<Exp *>::iterator it = exp_list_.begin();
  for (; it != exp_list_.end(); it++) {
    // 考虑处理参数个数>6的情况
    if (pos >= 6)
      break;
    temp::Temp *arg = (*it)->Munch(instr_list, fs);
    list->Append(arg);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({arg_regs->NthTemp(pos)}),
        new temp::TempList({arg})));
    pos++;
  }

  std::vector<Exp *> tmp_list;
  for (; it != exp_list_.end(); it++)
    tmp_list.push_back(*it);
  int size = tmp_list.size();
  //不支持pushq 用subq, movq代替
  //从后往前
  for (int i = size - 1; i >= 0; i--) {
    int word_size = reg_manager->WordSize();
    temp::Temp *arg = tmp_list[i]->Munch(instr_list, fs);
    std::string assem = "subq $" + std::to_string(word_size) + ", `d0";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({rsp}),
                                           nullptr, nullptr));
    instr_list.Append(new assem::MoveInstr("movq `s0, (`d0)",
                                           new temp::TempList({rsp}),
                                           new temp::TempList({arg})));
  }
  return list;
}

} // namespace tree
