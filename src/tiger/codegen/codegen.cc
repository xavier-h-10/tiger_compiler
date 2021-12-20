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

void CodeGen::Codegen() { /* TODO: Put your lab5 code here */
  auto stmList = traces_->GetStmList()->GetList();
  auto *instr_list = new assem::InstrList;
  std::string_view fs;
  sprintf(framesize, "%s_framesize", frame_->func_->Name().c_str());
  for (auto stm : stmList) {
    stm->Munch(*instr_list, fs);
  }
  frame::ProcEntryExit2(*instr_list);
  assem_instr_ = std::make_unique<cg::AssemInstr>(instr_list);
  //  assem_instr_->Print(stdout, temp::Map::Name());
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(jumps_)));
}

/**
  EQ_OP,
  NE_OP,
  LT_OP,
  GT_OP,
  LE_OP,
  GE_OP,
 */
void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto leftTemp = left_->Munch(instr_list, fs);
  auto rightTemp = right_->Munch(instr_list, fs);
  auto cmpInstr =
      new assem::OperInstr("cmp `s0, `s1", nullptr,
                           new temp::TempList({rightTemp, leftTemp}), nullptr);

  assem::Instr *cjumpStr = nullptr;
  std::string assem;
  switch (op_) {
  case EQ_OP: {
    assem = "je `j0";
    break;
  }
  case NE_OP: {
    assem = "jne `j0";
    break;
  }
  case LT_OP: {
    assem = "jl `j0";
    break;
  }
  case GT_OP: {
    assem = "jg `j0";
    break;
  }
  case LE_OP: {
    assem = "jle `j0";
    break;
  }
  case GE_OP: {
    assem = "jge `j0";
    break;
  }
  default:
    break;
  }
  auto cjumpInstr = new assem::OperInstr(
      assem, nullptr, nullptr,
      new assem::Targets(new std::vector<temp::Label *>({true_label_})));

  instr_list.Append(cmpInstr);
  instr_list.Append(cjumpInstr);
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (typeid(*dst_) == typeid(tree::MemExp)) {
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)", nullptr,
        new temp::TempList(
            {src_->Munch(instr_list, fs),
             dynamic_cast<tree::MemExp *>(dst_)->exp_->Munch(instr_list, fs)}),
        nullptr));
  } else {
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({dst_->Munch(instr_list, fs)}),
        new temp::TempList({src_->Munch(instr_list, fs)})));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

/**
  PLUS_OP,
  MINUS_OP,
  MUL_OP,
  DIV_OP,
  AND_OP,
  OR_OP,
 */
temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
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

// r <- M[xxx]
temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto expR = exp_->Munch(instr_list, fs);
  auto r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList({r}),
                                         new temp::TempList({expR}), nullptr));
  return r;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  //  return temp_;
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
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  auto r = temp::TempFactory::NewTemp();
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList({r}),
                           new temp::TempList({exp_->Munch(instr_list, fs)})));
  return r;
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto r = temp::TempFactory::NewTemp();
  char assem[100];
  sprintf(assem, "leaq %s(%%rip), `d0", name_->Name().c_str());
  instr_list.Append(new assem::OperInstr(
      std::string(assem), new temp::TempList({r}), nullptr, nullptr));
  return r;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  auto r = temp::TempFactory::NewTemp();
  char assem[100];
  sprintf(assem, "movq $%d, `d0", consti_);
  instr_list.Append(
      new assem::MoveInstr(assem, new temp::TempList({r}), nullptr));
  return r;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
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

  // add back the spill offsets
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
  /* TODO: Put your lab5 code here */
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
    ++i;
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

    // replace pushq with this one.
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
