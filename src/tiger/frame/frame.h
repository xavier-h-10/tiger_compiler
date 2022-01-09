#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"

namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  [[nodiscard]] virtual temp::Temp *rax() = 0;

  [[nodiscard]] virtual temp::Temp *rdx() = 0;

  temp::Map *temp_map_;

protected:
  std::vector<temp::Temp *> regs_;

  //public:
  //  temp::Temp *rax, *rcx, *rdx, *rdi, *rsi, *r8, *r9, *r10, *r11;
  //  temp::Temp *rbx, *r12, *r13, *r14, *r15;
  //  temp::Temp *rsp, *rbp;
  //  temp::TempList *registers, *arg_regs, *caller_saves, *callee_saves;
  //
  //  std::vector<std::string> reg_names_;

};

class Access {
public:
  // If acc is inReg, the result is simply the register
  // If acc is inFrame(k), the result is MEM(BINOP(PLUS, TEMP(fp), CONST(k)))
  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;
  virtual ~Access() = default;
};

class Frame {
  /* TODO: Put your lab5 code here */
public:
  temp::Label *name_;
  std::list<Access *> formals_;

  int locals = 0;

public:
  virtual int size() const = 0;

  inline void Append(Access *access) { formals_.push_back(access); }

  inline std::list<Access *> GetFormals() const { return formals_; }

  virtual Access *AllocLocal(bool escape) = 0;
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase,
                           bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag *> &GetList() { return frags_; }

private:
  std::list<Frag *> frags_;
};

tree::Stm *ProcEntryExit1(frame::Frame *frame, tree::Stm *stm);
void ProcEntryExit2(assem::InstrList &instr_list);
assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body);
Frame *NewFrame(temp::Label *label, const std::list<bool> formals);

} // namespace frame

#endif