#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/semant/types.h"

#include <iostream>

namespace tr {

class Exp;
class ExpAndTy;
class Level;

class Access {
public:
  Level *level_;
  frame::Access *access_;

  Access(Level *level, frame::Access *access)
      : level_(level), access_(access) {}
  static Access *AllocLocal(Level *level, bool escape);
};

class Level {
private:
  Level(frame::Frame *frame_, Level *parent_)
      : frame_(frame_), parent_(parent_){};

public:
  frame::Frame *frame_;
  Level *parent_;

  static Level *Outermost() {
    static Level *outermost = nullptr;
    if (outermost == nullptr) {
      auto newFrame =
          frame::NewFrame(temp::LabelFactory::NamedLabel("tigermain"), {});
      outermost = new Level(newFrame, nullptr);
    }
    return outermost;
  }

  /* TODO: Put your lab5 code here */

  static Level *NewLevel(Level *parent, temp::Label *func,
                         std::list<bool> formals) {
    formals.push_front(true);
    auto frame = frame::NewFrame(func, formals);
    return new Level(frame, parent);
  }
};

class ProgTr {
public:
  // TODO: Put your lab5 code here */
  ProgTr(std::unique_ptr<absyn::AbsynTree> absyn,
         std::unique_ptr<err::ErrorMsg> erromsg)
      : absyn_tree_(std::move(absyn)), errormsg_(std::move(erromsg)),
        main_level_(Level::Outermost()), tenv_(std::make_unique<env::TEnv>()),
        venv_(std::make_unique<env::VEnv>()) {}
  /**
   * Translate IR tree
   */
  void Translate();

  /**
   * Transfer the ownership of errormsg to outer scope
   * @return unique pointer to errormsg
   */
  std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

private:
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;
  std::unique_ptr<err::ErrorMsg> errormsg_;
  std::unique_ptr<frame::RegManager> reg_manager;
  std::unique_ptr<Level> main_level_;
  std::unique_ptr<env::TEnv> tenv_;
  std::unique_ptr<env::VEnv> venv_;

  // Fill base symbol for var env and type env
  void FillBaseVEnv();
  void FillBaseTEnv();
};

tree::Exp *FramePtr(Level *level, Level *acc_level,
                    frame::RegManager *reg_manager);

} // namespace tr

#endif