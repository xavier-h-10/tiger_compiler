#include "tiger/codegen/codegen.h"

#include <cassert>
#include <iostream>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

static char framesize[255];

namespace cg {

void CodeGen::Codegen() {
  auto stm_list = traces_->GetStmList()->GetList();
  auto *list = new assem::InstrList;
  std::string_view fs;
  //  frame_size = frame_->size();
  sprintf(framesize, "%s_framesize", frame_->name_->Name().c_str()); //调用codegen,生成新的frame_size
  for (auto stm : stm_list) {
    stm->Munch(*list, fs);
  }
  frame::ProcEntryExit2(*list);
  assem_instr_ = std::make_unique<cg::AssemInstr>(list);
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
      "cmpq `s0, `s1",nullptr, new temp::TempList({right, left}), nullptr));  //这里改成d0,d1就会有问题
  instr_list.Append(new assem::OperInstr(op + " `j0", nullptr, nullptr,
                                         new assem::Targets(target)));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    temp::Temp *dst = ((tree::MemExp *)dst_)->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)", nullptr,
        new temp::TempList(
            {src_->Munch(instr_list, fs),
             dst}),
        nullptr));
  } else {
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({dst_->Munch(instr_list, fs)}),
        new temp::TempList({src_->Munch(instr_list, fs)})));
  }

  //  if (typeid(*dst_) == typeid(tree::MemExp)) {
  //    temp::Temp *dst = ((tree::MemExp *)dst_)->exp_->Munch(instr_list, fs);
  //    temp::Temp *src = src_->Munch(instr_list, fs);
  //    instr_list.Append(new assem::MoveInstr("movq `s0, (`d0)",
  //                                           new temp::TempList({dst}),
  //                                           new temp::TempList({src})));
  //  } else if (typeid(*src_) == typeid(tree::MemExp)) {
  //    temp::Temp *dst = dst_->Munch(instr_list, fs);
  //    temp::Temp *src = ((tree::MemExp *)src_)->exp_->Munch(instr_list, fs);
  //    instr_list.Append(new assem::MoveInstr("movq (`s0), `d0",
  //                                           new temp::TempList({dst}),
  //                                           new temp::TempList({src})));
}


void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto r = temp::TempFactory::NewTemp();
  switch (op_) {
  case PLUS_OP: {
    auto left = left_->Munch(instr_list, fs);
    auto right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r}), new temp::TempList({left})));
    instr_list.Append(
        new assem::OperInstr("addq `s0, `d0", new temp::TempList({r}),
                             new temp::TempList({right}), nullptr));
    break;
  }
  case MINUS_OP: {
    auto left = left_->Munch(instr_list, fs);
    auto right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r}), new temp::TempList({left})));
    instr_list.Append(
        new assem::OperInstr("subq `s0, `d0", new temp::TempList({r}),
                             new temp::TempList({right}), nullptr));
    break;
  }
  case MUL_OP: {
    auto r1 = temp::TempFactory::NewTemp();
    auto r2 = temp::TempFactory::NewTemp();
    auto rdx = reg_manager->GetRegister(3);
    auto rax = reg_manager->GetRegister(0);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r1}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r2}), new temp::TempList({rdx})));
    auto left = left_->Munch(instr_list, fs);
    auto right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "imulq `s0", nullptr, new temp::TempList({right}), nullptr));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({rax}), new temp::TempList({r1})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({rdx}), new temp::TempList({r2})));
    break;
  }
  case DIV_OP: {
    auto r1 = temp::TempFactory::NewTemp();
    auto r2 = temp::TempFactory::NewTemp();
    auto rdx = reg_manager->GetRegister(3);
    auto rax = reg_manager->GetRegister(0);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r1}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r2}), new temp::TempList({rdx})));
    auto left = left_->Munch(instr_list, fs);
    auto right = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({rax}),
                                           new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "cqto\nidivq `s0", nullptr, new temp::TempList({right}), nullptr));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({r}), new temp::TempList({rax})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({rax}), new temp::TempList({r1})));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({rdx}), new temp::TempList({r2})));
    break;
  }
  }
  return r;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto expR = exp_->Munch(instr_list, fs);
  auto r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList({r}),
                                         new temp::TempList({expR}), nullptr));
  return r;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ != reg_manager->FramePointer())
    return temp_;
  auto r = temp::TempFactory::NewTemp();
  char assem[100];
  sprintf(assem, "leaq %s(`s0), `d0 ", framesize);
  instr_list.Append(new assem::OperInstr(
      assem, new temp::TempList({r}),
      new temp::TempList({reg_manager->StackPointer()}), nullptr));
  return r;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  stm_->Munch(instr_list, fs);
  auto r = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList({r}),
                           new temp::TempList({exp_->Munch(instr_list, fs)})));
  return r;
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto r = temp::TempFactory::NewTemp();
  char assem[100];
  sprintf(assem, "leaq %s(%%rip), `d0", name_->Name().c_str());
  instr_list.Append(new assem::OperInstr(
      std::string(assem), new temp::TempList({r}), nullptr, nullptr));
  return r;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto r = temp::TempFactory::NewTemp();
  char assem[100];
  sprintf(assem, "movq $%d, `d0", consti_);
  instr_list.Append(
      new assem::MoveInstr(assem, new temp::TempList({r}), nullptr));
  return r;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  auto r = temp::TempFactory::NewTemp();
  auto funLabel = dynamic_cast<tree::NameExp *>(fun_)->name_->Name();
  auto tempList = args_->MunchArgs(instr_list, fs);
  int spillNumber = args_->GetList().size() - 6;
  instr_list.Append(new assem::OperInstr("callq " + funLabel,
                                         reg_manager->CallerSaves(),
                                         reg_manager->ArgRegs(), nullptr));
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList({r}),
                           new temp::TempList({reg_manager->ReturnValue()})));
  if (spillNumber > 0) {
    char assem[100];
    sprintf(assem, "addq $%d, `d0", spillNumber * reg_manager->WordSize());

    instr_list.Append(new assem::OperInstr(
        assem, new temp::TempList({reg_manager->StackPointer()}), nullptr,
        nullptr));
  }
  return r;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  int i = 0;
  auto argRegs = reg_manager->ArgRegs();
  auto tempList = new temp::TempList;
  auto exp_it = exp_list_.begin();
  for (; exp_it != exp_list_.end(); ++exp_it) {
    auto argExp = *exp_it;
    auto argR = argExp->Munch(instr_list, fs);
    tempList->Append(argR);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({argRegs->NthTemp(i)}),
        new temp::TempList({argR})));
    if (i == 5)
      break;
    i++;
  }
  if (exp_list_.size() > 6) {
    auto exp_rit = exp_list_.end();
    exp_rit--;

    auto r = temp::TempFactory::NewTemp();
    auto rsp = reg_manager->StackPointer();

    char assem[100];
    int wordSize = reg_manager->WordSize();

    sprintf(assem, "movq $%d, `d0", wordSize);

    instr_list.Append(
        new assem::OperInstr(assem, new temp::TempList({r}), nullptr, nullptr));

    for (; exp_rit != exp_it; --exp_rit) {
      auto argExp = *exp_rit;
      auto argR = argExp->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr("subq `s0, `d0",
                                             new temp::TempList({rsp}),
                                             new temp::TempList({r}), nullptr));
      instr_list.Append(new assem::OperInstr("movq `s0, (`s1)", nullptr,
                                             new temp::TempList({argR, rsp}),
                                             nullptr));
    }
  }
  return tempList;
}

} // namespace tree
