#include "tiger/frame/x64frame.h"
#include <iostream>

extern frame::RegManager *reg_manager;

namespace frame {
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}

  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    auto offsetExp = new tree::ConstExp(offset);
    auto addrExp = new tree::BinopExp(tree::PLUS_OP, framePtr, offsetExp);
    return new tree::MemExp(addrExp);
  }
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
public:
  Access *AllocLocal(bool escape) override;

  inline int size() const override {
    return locals * reg_manager->WordSize();
  }
};

Access *X64Frame::AllocLocal(bool escape) {
  Access *access = escape ? dynamic_cast<Access *>(new InFrameAccess(
                                (-(++locals)) * reg_manager->WordSize()))
                          : dynamic_cast<Access *>(
                                new InRegAccess(temp::TempFactory::NewTemp()));
  return access;
}

Frame *NewFrame(temp::Label *fun, const std::list<bool> formals) {
  Frame *frame = new X64Frame;
  int idx = 1;
  for (bool escape : formals) {
    frame->Append(frame->AllocLocal(escape));
  }
  frame->name_ = fun;
  return frame;
}
/**
  std::string prolog_;
  InstrList *body_;
  std::string epilog_;
 */

void ProcEntryExit2(assem::InstrList &instr_list) {
  auto returnSink = reg_manager->ReturnSink();
  instr_list.Append(new assem::OperInstr("", nullptr, returnSink, nullptr));
}

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  auto formals = frame->Formals();
  int i = 0;

  auto temps = new temp::TempList(
      {temp::TempFactory::NewTemp(), temp::TempFactory::NewTemp(),
       temp::TempFactory::NewTemp(), temp::TempFactory::NewTemp(),
       temp::TempFactory::NewTemp(), temp::TempFactory::NewTemp()});

  tree::Stm *seqStm = new tree::ExpStm(new tree::ConstExp(0));

  // save callee saved registers;
  auto calleeSaves = reg_manager->CalleeSaves();
  for (int i = 0; i < 6; ++i) {
    seqStm = new tree::SeqStm(
        seqStm, new tree::MoveStm(new tree::TempExp(temps->NthTemp(i)),
                                  new tree::TempExp(calleeSaves->NthTemp(i))));
  }

//  std::cout << frame->name_->Name() << " " << formals.size() << std::endl;

  // view shift
  auto argRegs = reg_manager->ArgRegs();
  auto fp = reg_manager->FramePointer();
  auto acc_it = formals.begin();
  for (; acc_it != formals.end(); ++acc_it) {
    if (i >= 6)
      break;
    auto access = *acc_it;
    auto reg = argRegs->NthTemp(i++);
    seqStm = new tree::SeqStm(
        seqStm, new tree::MoveStm(access->ToExp(new tree::TempExp(fp)),
                                  new tree::TempExp(reg)));
  }

  int spillNumber = formals.size() - 6;
  int wordSize = reg_manager->WordSize();
  int offset = wordSize;

  for (int i = 0; i < spillNumber; ++i) {
    auto access = *acc_it;
    auto memExp = new tree::MemExp(new tree::BinopExp(
        tree::PLUS_OP, new tree::TempExp(fp), new tree::ConstExp(offset)));
    seqStm = new tree::SeqStm(
        seqStm,
        new tree::MoveStm(access->ToExp(new tree::TempExp(fp)), memExp));
    offset += wordSize;
    ++acc_it;
  }

  seqStm = new tree::SeqStm(seqStm, stm);

  // restore callee saved registers;
  for (int i = 0; i < 6; ++i) {
    seqStm = new tree::SeqStm(
        seqStm, new tree::MoveStm(new tree::TempExp(calleeSaves->NthTemp(i)),
                                  new tree::TempExp(temps->NthTemp(i))));
  }

  return seqStm;
}

//assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
//  char buf[256];
//  std::string prolog;
//  sprintf(buf, ".set %s_framesize, %d\n", frame->name_->Name().c_str(),
//          frame->size());
//  prolog = std::string(buf);
//  sprintf(buf, "%s:\n", frame->name_->Name().c_str());
//  prolog.append(std::string(buf));
//  sprintf(buf, "subq $%d, %%rsp\n", frame->size());
//  prolog.append(std::string(buf));
//
//  sprintf(buf, "addq $%d, %%rsp\n", frame->size());
//  std::string epilog = std::string(buf);
//  epilog.append("retq\n");
//  return new assem::Proc(prolog, body, epilog);
//}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  std::string prolog = frame->name_->Name() + ":\n";
  prolog = prolog + "subq $" + std::to_string(frame->size()) + ", %rsp\n";

  std::string epilog = "addq $" + std::to_string(frame->size()) + ", %rsp\n";
  epilog = epilog + "retq\n";
  return new assem::Proc(prolog, body, epilog);
}

} // namespace frame