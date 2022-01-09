#include "tiger/frame/x64frame.h"
#include <iostream>

extern frame::RegManager *reg_manager;

namespace frame {
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}

  // If acc is inFrame(k), the result is MEM(BINOP(PLUS, TEMP(fp), CONST(k)))
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    tree::BinopExp *exp =
        new tree::BinopExp(tree::PLUS_OP, framePtr, new tree::ConstExp(offset));
    return new tree::MemExp(exp);
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
  Access *AllocLocal(bool escape) override {
    Access *access;
    if (escape) {
      locals++;
      access =
          new InFrameAccess((-1) * locals * reg_manager->WordSize()); // think
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }

  inline int size() const override {
    return locals * reg_manager->WordSize();
  }
};

Frame *NewFrame(temp::Label *label, const std::list<bool> formals) {
  Frame *frame = new X64Frame;
  for (bool escape : formals) {
    frame->Append(frame->AllocLocal(escape));
  }
  frame->name_ = label;
  return frame;
}

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm) {
  temp::TempList *callee_saves = reg_manager->CalleeSaves();
  temp::TempList *temp_list = new temp::TempList();
  tree::Stm *seq_stm = new tree::ExpStm(new tree::ConstExp(0));
  for (int i = 0; i < 6; i++) {
    temp_list->Append(temp::TempFactory::NewTemp());
  }

  // save callee-save registers
  for (int i = 0; i < 6; i++) {
    seq_stm = new tree::SeqStm(
        seq_stm,
        new tree::MoveStm(new tree::TempExp(temp_list->NthTemp(i)),
                          new tree::TempExp(callee_saves->NthTemp(i))));
  }

  // view shift
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  temp::Temp *fp = reg_manager->FramePointer();
  std::list<Access *> formals = frame->GetFormals();
  std::list<Access *>::iterator it = formals.begin();
  int now = 0;
  int word_size = reg_manager->WordSize();
  for (; it != formals.end(); it++) {
    if (now >= 6)
      break;
    Access *access = *it;
    seq_stm = new tree::SeqStm(
        seq_stm, new tree::MoveStm(access->ToExp(new tree::TempExp(fp)),
                                   new tree::TempExp(arg_regs->NthTemp(now))));
    now++;
  }
  now = 1;
  for (; it != formals.end(); it++) {
    Access *access = *it;
    seq_stm = new tree::SeqStm(
        seq_stm, new tree::MoveStm(access->ToExp(new tree::TempExp(fp)),
                                   new tree::MemExp(new tree::BinopExp(
                                       tree::PLUS_OP, new tree::TempExp(fp),
                                       new tree::ConstExp(now * word_size)))));
    now++;
  }

  seq_stm = new tree::SeqStm(seq_stm, stm); // body
  // restore callee-save registers
  for (int i = 0; i < 6; i++) {
    seq_stm = new tree::SeqStm(
        seq_stm, new tree::MoveStm(new tree::TempExp(callee_saves->NthTemp(i)),
                                   new tree::TempExp(temp_list->NthTemp(i))));
  }
  return seq_stm;
}

void ProcEntryExit2(assem::InstrList &body) {
  body.Append(
      new assem::OperInstr("", nullptr, reg_manager->ReturnSink(), nullptr));
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body) {
  std::string prolog = frame->name_->Name() + ":\n";
  prolog = prolog + "subq $" + std::to_string(frame->size()) + ", %rsp\n";

  std::string epilog = "addq $" + std::to_string(frame->size()) + ", %rsp\n";
  epilog = epilog + "retq\n";
  return new assem::Proc(prolog, body, epilog);
}

} // namespace frame