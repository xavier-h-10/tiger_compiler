//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
private:
  temp::TempList *registers;
  temp::TempList *arg_regs;
  temp::TempList *caller_saves;
  temp::TempList *callee_saves;
  temp::TempList *return_sink;

public:
  X64RegManager() {
    temp::Temp *rax = temp::TempFactory::NewTemp(0);
    temp::Temp *rbx = temp::TempFactory::NewTemp(1);
    temp::Temp *rcx = temp::TempFactory::NewTemp(2);
    temp::Temp *rdx = temp::TempFactory::NewTemp(3);
    temp::Temp *rsp = temp::TempFactory::NewTemp(4);
    temp::Temp *rbp = temp::TempFactory::NewTemp(5);
    temp::Temp *rsi = temp::TempFactory::NewTemp(6);
    temp::Temp *rdi = temp::TempFactory::NewTemp(7);
    temp::Temp *r8 = temp::TempFactory::NewTemp(8);
    temp::Temp *r9 = temp::TempFactory::NewTemp(9);
    temp::Temp *r10 = temp::TempFactory::NewTemp(10);
    temp::Temp *r11 = temp::TempFactory::NewTemp(11);
    temp::Temp *r12 = temp::TempFactory::NewTemp(12);
    temp::Temp *r13 = temp::TempFactory::NewTemp(13);
    temp::Temp *r14 = temp::TempFactory::NewTemp(14);
    temp::Temp *r15 = temp::TempFactory::NewTemp(15);

    temp::Map::Name()->Enter(rax, new std::string("%rax"));
    temp::Map::Name()->Enter(rbx, new std::string("%rbx"));
    temp::Map::Name()->Enter(rcx, new std::string("%rcx"));
    temp::Map::Name()->Enter(rdx, new std::string("%rdx"));
    temp::Map::Name()->Enter(rsp, new std::string("%rsp"));
    temp::Map::Name()->Enter(rbp, new std::string("%rbp"));
    temp::Map::Name()->Enter(rsi, new std::string("%rsi"));
    temp::Map::Name()->Enter(rdi, new std::string("%rdi"));
    temp::Map::Name()->Enter(r8, new std::string("%r8"));
    temp::Map::Name()->Enter(r9, new std::string("%r9"));
    temp::Map::Name()->Enter(r10, new std::string("%r10"));
    temp::Map::Name()->Enter(r11, new std::string("%r11"));
    temp::Map::Name()->Enter(r12, new std::string("%r12"));
    temp::Map::Name()->Enter(r13, new std::string("%r13"));
    temp::Map::Name()->Enter(r14, new std::string("%r14"));
    temp::Map::Name()->Enter(r15, new std::string("%r15"));

    regs_ = {rax, rbx, rcx, rdx, rsp, rbp, rsi, rdi,
             r8,  r9,  r10, r11, r12, r13, r14, r15};

    registers = new temp::TempList({rax, rbx, rcx, rdx, rsp, rbp, rsi, rdi, r8,
                                    r9, r10, r11, r12, r13, r14, r15});
    arg_regs = new temp::TempList({rdi, rsi, rdx, rcx, r8, r9});
    caller_saves =
        new temp::TempList({rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11});
    callee_saves = new temp::TempList({rbx, rbp, r12, r13, r14, r15});

    return_sink = new temp::TempList({rbx, rbp, r12, r13, r14, r15, rsp, rax});
  }
  temp::TempList *Registers() override { return registers; }

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  temp::TempList *ArgRegs() override { return arg_regs; };

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  temp::TempList *CallerSaves() override { return caller_saves; };

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  temp::TempList *CalleeSaves() override { return callee_saves; };

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  temp::TempList *ReturnSink() override { return return_sink; };

  /**
   * Get word size
   */
  int WordSize() { return 8; };

  temp::Temp *FramePointer() override { return regs_[5]; };
  temp::Temp *StackPointer() override { return regs_[4]; };
  temp::Temp *ReturnValue() override { return regs_[0]; };
  temp::Temp *rax() override { return regs_[0]; };
  temp::Temp *rdx() override { return regs_[3]; };
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
