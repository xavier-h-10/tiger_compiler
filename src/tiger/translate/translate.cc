#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

tree::Exp *ExternalCall(std::string_view func, tree::ExpList *args) {
  return new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel(func)), args);
}

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  //  return
  auto frame = level->frame_;
  auto access = frame->AllocLocal(escape);
  return new Access(level, access);
  /// TODO alloclocal for vardec
}

using PatchList = std::list<temp::Label **>;
void DoPatch(PatchList t_list, temp::Label *label) {
  for (auto t : t_list)
    *t = label;
}

PatchList JoinPatch(PatchList first, PatchList second) {
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_ = nullptr;

  Cx() = default;

  Cx(const PatchList &trues, const PatchList &falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *stm = new tree::CjumpStm(
        tree::RelOp::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    PatchList trues = {&stm->true_label_};
    PatchList falses = {&stm->false_label_};
    return Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    return Cx();
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList &trues, PatchList &falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    tr::DoPatch(cx_.trues_, t);
    tr::DoPatch(cx_.falses_, f);
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }

  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    auto label = temp::LabelFactory::NewLabel();
    DoPatch(cx_.trues_, label);
    DoPatch(cx_.falses_, label);
    return new tree::SeqStm(cx_.stm_, new tree::LabelStm(label));
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

void ProgTr::Translate() { /* TODO: Put your lab5 code here */
  FillBaseTEnv();
  FillBaseVEnv();

  auto trExpAndTy = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(),
      temp::LabelFactory::NamedLabel("tigermain"), errormsg_.get());
//  auto trStm = trExpAndTy->exp_->UnNx();
//  trStm->Print(stdout, 0);
//  auto fragList = frags->GetList();
//  std::cout << "Frags: " << std::endl;
//  for (auto frag : fragList) {
//    if (typeid(*frag) == typeid(frame::ProcFrag)) {
//      std::cout << "ProcFrag: " << std::endl;
//      dynamic_cast<frame::ProcFrag *>(frag)->body_->Print(stdout, 0);
//      std::cout << std::endl;
//    } else {
//      auto strfrag = dynamic_cast<frame::StringFrag *>(frag);
//      std::cout << "label: " << strfrag->label_->Name()
//                << " and string: " << strfrag->str_ << std::endl;
//    }
//  }
}

/**
 * MEM( + ( CONST k, MEM( +(CONST 2*wordsize, ( …MEM(+(CONST 2*wordsize, TEMP
 * FP) …))))
 */
tree::Exp *FramePtr(Level *level, Level *acc_level) {
  tree::Exp *framePtr = new tree::TempExp(reg_manager->FramePointer());
  while (level != acc_level) {
    framePtr = level->frame_->Formals().front()->ToExp(framePtr);
    level = level->parent_;
  }
  return framePtr;
}
} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */

  auto varEntry = dynamic_cast<env::VarEntry *>(venv->Look(sym_));
  auto access = varEntry->access_;
  tree::Exp *framePtr = FramePtr(level, access->level_);
//  if (level == access->level_)
//    errormsg->Error(pos_, sym_->Name() + " level equal");
  tr::Exp *varExp = new tr::ExExp(access->access_->ToExp(framePtr));
  return new tr::ExpAndTy(varExp, varEntry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // by call var->translate, we can get the address stored by the record var.
  // notice that the record var is actually a pointer, which points to the real
  // data stored in the heap.

  auto expAndTy = var_->Translate(venv, tenv, level, label, errormsg);
  auto varExp = expAndTy->exp_->UnEx();
  auto varTy = expAndTy->ty_;
  auto recordTy = dynamic_cast<type::RecordTy *>(varTy->ActualTy());
  auto fieldList = recordTy->fields_->GetList();
  int idx = 0;
  int wordSize = reg_manager->WordSize();

  // get the type of the field and count the offset
  type::Ty *fieldTy;
  for (auto field : fieldList) {
    if (field->name_ == sym_) {
      fieldTy = field->ty_;
      break;
    }
    ++idx;
  }

  auto offset = new tree::ConstExp(idx * wordSize);
  auto fieldAddr = new tree::BinopExp(tree::PLUS_OP, varExp, offset);
  auto fieldExp = new tr::ExExp(new tree::MemExp(fieldAddr));
  return new tr::ExpAndTy(fieldExp, fieldTy);
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto expAndTy = var_->Translate(venv, tenv, level, label, errormsg);
  auto varExp = expAndTy->exp_->UnEx();
  auto arrayTy = dynamic_cast<type::ArrayTy *>(expAndTy->ty_);

  int wordSize = reg_manager->WordSize();
  auto index =
      subscript_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  auto w = new tree::ConstExp(wordSize);
  auto offset = new tree::BinopExp(tree::MUL_OP, index, w);
  auto subVarAddr = new tree::BinopExp(tree::PLUS_OP, varExp, offset);
  auto subVarExp = new tr::ExExp(new tree::MemExp(subVarAddr));
  return new tr::ExpAndTy(subVarExp, arrayTy->ty_);
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto constExp = new tr::ExExp(new tree::ConstExp(val_));
  return new tr::ExpAndTy(constExp, type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto lab = temp::LabelFactory::NewLabel();
  auto labExp = new tree::NameExp(lab);
  frags->PushBack(new frame::StringFrag(lab, str_));
  return new tr::ExpAndTy(new tr::ExExp(labExp), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto argExpList = new tree::ExpList;
  auto argList = args_->GetList();
  tree::Exp *framePtr = new tree::TempExp(reg_manager->FramePointer());
  for (auto arg : argList) {
    auto argExp =
        arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    argExpList->Append(argExp);
  }
  auto funEntry = dynamic_cast<env::FunEntry *>(venv->Look(func_));
  tree::Exp *callExp = nullptr;
  auto parent = funEntry->level_->parent_;
  if (parent) {
    argExpList->Insert(FramePtr(level, parent));
    callExp = new tree::CallExp(new tree::NameExp(func_), argExpList);
  } else
    callExp = tr::ExternalCall(func_->Name(), argExpList);
  //  errormsg->Error(pos_, "func: " + func_->Name() + " and size: " +
  //                            std::to_string(argExpList->GetList().size()));
  return new tr::ExpAndTy(new tr::ExExp(callExp), funEntry->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto left = left_->Translate(venv, tenv, level, label, errormsg);
  auto right = right_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = nullptr;
  switch (oper_) {
  case absyn::PLUS_OP:
  case absyn::MINUS_OP:
  case absyn::TIMES_OP:
  case absyn::DIVIDE_OP: {
    switch (oper_) {
    case absyn::PLUS_OP:
      exp = new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, left->exp_->UnEx(),
                                             right->exp_->UnEx()));
      break;
    case absyn::MINUS_OP:
      exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, left->exp_->UnEx(),
                                             right->exp_->UnEx()));
      break;
    case absyn::TIMES_OP:
      exp = new tr::ExExp(new tree::BinopExp(tree::MUL_OP, left->exp_->UnEx(),
                                             right->exp_->UnEx()));
      break;
    case absyn::DIVIDE_OP:
      exp = new tr::ExExp(new tree::BinopExp(tree::DIV_OP, left->exp_->UnEx(),
                                             right->exp_->UnEx()));
      break;
    }
    break;
  }
  case absyn::LT_OP:
  case absyn::LE_OP:
  case absyn::GT_OP:
  case absyn::GE_OP: {
    tree::CjumpStm *stm;
    switch (oper_) {
    case absyn::LT_OP:
      stm = new tree::CjumpStm(tree::LT_OP, left->exp_->UnEx(),
                               right->exp_->UnEx(), nullptr, nullptr);
      break;
    case absyn::LE_OP:
      stm = new tree::CjumpStm(tree::LE_OP, left->exp_->UnEx(),
                               right->exp_->UnEx(), nullptr, nullptr);
      break;
    case absyn::GT_OP:
      stm = new tree::CjumpStm(tree::GT_OP, left->exp_->UnEx(),
                               right->exp_->UnEx(), nullptr, nullptr);
      break;
    case absyn::GE_OP:
      stm = new tree::CjumpStm(tree::GE_OP, left->exp_->UnEx(),
                               right->exp_->UnEx(), nullptr, nullptr);
      break;
    }
    tr::PatchList trues = {&stm->true_label_};
    tr::PatchList falses = {&stm->false_label_};
    exp = new tr::CxExp(trues, falses, stm);
    break;
  }
  case absyn::EQ_OP:
  case absyn::NEQ_OP: {
    tree::CjumpStm *stm;
    switch (oper_) {
    case absyn::EQ_OP:
      if (left->ty_->IsSameType(type::StringTy::Instance())) {
        stm = new tree::CjumpStm(
            tree::EQ_OP,
            tr::ExternalCall(
                "string_equal",
                new tree::ExpList({left->exp_->UnEx(), right->exp_->UnEx()})),
            new tree::ConstExp(1), nullptr, nullptr);
      } else {
        stm = new tree::CjumpStm(tree::EQ_OP, left->exp_->UnEx(),
                                 right->exp_->UnEx(), nullptr, nullptr);
      }
      break;
    case absyn::NEQ_OP:
      stm = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(),
                               right->exp_->UnEx(), nullptr, nullptr);
      break;
    }
    tr::PatchList trues = {&stm->true_label_};
    tr::PatchList falses = {&stm->false_label_};
    exp = new tr::CxExp(trues, falses, stm);
    break;
  }
  }

  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto recordTy = tenv->Look(typ_);
  auto efieldList = fields_->GetList();
  auto fieldLen = efieldList.size();
  int wordSize = reg_manager->WordSize();

  // alloc space
  auto r = temp::TempFactory::NewTemp();
  auto tempExp = new tree::TempExp(r);
  auto allocSize = fieldLen * wordSize;
  auto sizeExp = new tree::ConstExp(allocSize);
  auto argList = new tree::ExpList;
  argList->Append(sizeExp);
  auto callMallocExp = tr::ExternalCall("alloc_record", argList);

  auto allocStm = new tree::MoveStm(tempExp, callMallocExp);

  // assignment value
  int idx = efieldList.size() - 1;

  tree::Stm *rightStm = nullptr;
  tree::Stm *curSeqStm = nullptr;

  for (auto e_it = efieldList.rbegin(); e_it != efieldList.rend(); ++e_it) {
    auto efield = *e_it;
    auto efdExp = efield->exp_->Translate(venv, tenv, level, label, errormsg)
                      ->exp_->UnEx();
    auto offset = new tree::ConstExp(idx * wordSize);
    auto tempExp = new tree::TempExp(r);
    auto fieldExp =
        new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, tempExp, offset));
    auto leftStm = new tree::MoveStm(fieldExp, efdExp);

    if (rightStm != nullptr) {
      curSeqStm = new tree::SeqStm(leftStm, rightStm);
    } else {
      curSeqStm = leftStm;
    }
    rightStm = curSeqStm;
    --idx;
  }

  curSeqStm = new tree::SeqStm(allocStm, curSeqStm);

  auto eseq = new tree::EseqExp(curSeqStm, new tree::TempExp(r));
  return new tr::ExpAndTy(new tr::ExExp(eseq), recordTy);
}

tr::Exp *TranslateSeqExp(tr::Exp *left, tr::Exp *right) {
  if (right)
    return new tr::ExExp(new tree::EseqExp(left->UnNx(), right->UnEx()));
  else
    return new tr::ExExp(
        new tree::EseqExp(left->UnNx(), new tree::ConstExp(0)));
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */

  auto expList = seq_->GetList();
  auto exp_it = expList.begin();
  tr::Exp *seqExp = new tr::ExExp(new tree::ConstExp(0));
  tr::ExpAndTy *expAndTy;
  for (; exp_it != expList.end(); ++exp_it) {
    expAndTy = (*exp_it)->Translate(venv, tenv, level, label, errormsg);
    seqExp = TranslateSeqExp(seqExp, expAndTy->exp_);
  }

  return new tr::ExpAndTy(seqExp, expAndTy->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto varExp =
      var_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  auto valueExpAndTy = exp_->Translate(venv, tenv, level, label, errormsg);
  auto valueExp = valueExpAndTy->exp_->UnEx();
  auto valueTy = valueExpAndTy->ty_;
  auto assignExp = new tree::MoveStm(varExp, valueExp);
  return new tr::ExpAndTy(new tr::NxExp(assignExp), valueTy);
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();
  auto t = temp::LabelFactory::NewLabel();
  auto f = temp::LabelFactory::NewLabel();

  auto testExp = test_->Translate(venv, tenv, level, label, errormsg)->exp_;
  tr::Cx testCx = testExp->UnCx(errormsg);
  auto thenExpAndTy = then_->Translate(venv, tenv, level, label, errormsg);
  auto thenExp = thenExpAndTy->exp_;
  auto thenTy = thenExpAndTy->ty_;
  auto elseExp =
      elsee_ ? elsee_->Translate(venv, tenv, level, label, errormsg)->exp_
             : nullptr;
  tenv->EndScope();
  venv->EndScope();

  tr::DoPatch(testCx.trues_, t);
  tr::DoPatch(testCx.falses_, f);
  auto testStm = testCx.stm_;

  // if-then exp
  if (elsee_ == nullptr) {
    auto thenStm = thenExp->UnNx();
    auto exp = new tr::NxExp(new tree::SeqStm(
        testStm,
        new tree::SeqStm(new tree::LabelStm(t),
                         new tree::SeqStm(thenStm, new tree::LabelStm(f)))));
    return new tr::ExpAndTy(exp, type::VoidTy::Instance());
  }

  auto r = temp::TempFactory::NewTemp();
  auto join = temp::LabelFactory::NewLabel();

  auto exp = new tr::ExExp(new tree::EseqExp(
      testStm,
      new tree::EseqExp(
          new tree::LabelStm(t),
          new tree::EseqExp(
              new tree::MoveStm(new tree::TempExp(r), thenExp->UnEx()),
              new tree::EseqExp(
                  new tree::JumpStm(new tree::NameExp(join),
                                    new std::vector<temp::Label *>({join})),
                  new tree::EseqExp(
                      new tree::LabelStm(f),
                      new tree::EseqExp(
                          new tree::MoveStm(new tree::TempExp(r),
                                            elseExp->UnEx()),
                          new tree::EseqExp(
                              new tree::JumpStm(
                                  new tree::NameExp(join),
                                  new std::vector<temp::Label *>({join})),
                              new tree::EseqExp(new tree::LabelStm(join),
                                                new tree::TempExp(r))))))))));
  return new tr::ExpAndTy(exp, thenTy);
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  venv->BeginScope();
  tenv->BeginScope();
  auto testLab = temp::LabelFactory::NewLabel();
  auto bodyLab = temp::LabelFactory::NewLabel();
  auto doneLab = temp::LabelFactory::NewLabel();
  auto testExp = test_->Translate(venv, tenv, level, label, errormsg)->exp_;
  auto testCx = testExp->UnCx(errormsg);

  auto bodyStm =
      body_->Translate(venv, tenv, level, doneLab, errormsg)->exp_->UnNx();

  tr::DoPatch(testCx.trues_, bodyLab);
  tr::DoPatch(testCx.falses_, doneLab);

  auto exp = new tr::NxExp(new tree::SeqStm(
      new tree::LabelStm(testLab),
      new tree::SeqStm(
          testCx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(bodyLab),
              new tree::SeqStm(
                  bodyStm, new tree::SeqStm(
                               new tree::JumpStm(
                                   new tree::NameExp(testLab),
                                   new std::vector<temp::Label *>({testLab})),
                               new tree::LabelStm(doneLab)))))));
  tenv->EndScope();
  venv->EndScope();
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto iDec = new absyn::VarDec(0, var_, sym::Symbol::UniqueSymbol("int"), lo_);
  auto limitDec =
      new absyn::VarDec(0, sym::Symbol::UniqueSymbol("__limit_var__"),
                        sym::Symbol::UniqueSymbol("int"), hi_);
  auto decList = new absyn::DecList(limitDec);
  decList->Prepend(iDec);

  auto testExp = new absyn::OpExp(
      0, absyn::LE_OP, new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
      new absyn::VarExp(0, new absyn::SimpleVar(
                               0, sym::Symbol::UniqueSymbol("__limit_var__"))));

  auto incrementExp = new absyn::AssignExp(
      0, new absyn::SimpleVar(0, var_),
      new absyn::OpExp(0, absyn::PLUS_OP,
                       new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
                       new absyn::IntExp(0, 1)));

  auto expList = new absyn::ExpList(incrementExp);
  expList->Prepend(body_);

  auto whileExp =
      new absyn::WhileExp(0, testExp, new absyn::SeqExp(0, expList));

  auto bodyExp = new absyn::SeqExp(0, new absyn::ExpList(whileExp));
  auto letExp = new absyn::LetExp(0, decList, bodyExp);
  return letExp->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto doneStm = new tree::LabelStm(label);
  auto jumpStm = new tree::JumpStm(new tree::NameExp(label),
                                   new std::vector<temp::Label *>({label}));
  return new tr::ExpAndTy(new tr::NxExp(jumpStm), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  static bool firstEnter = true;
  bool isMain = false;
  if (firstEnter) {
    isMain = true;
    firstEnter = false;
  }
  auto decList = decs_->GetList();
  auto it = decList.begin();
  auto leftStm = ((*it)->Translate(venv, tenv, level, label, errormsg))->UnNx();
  ++it;
  tree::SeqStm *curSeqStm = nullptr;
  for (; it != decList.end(); ++it) {
    auto rightStm =
        ((*it)->Translate(venv, tenv, level, label, errormsg))->UnNx();
    curSeqStm = new tree::SeqStm(leftStm, rightStm);
    leftStm = curSeqStm;
  }
  //  curSeqStm->right_ = seqbody_->Translate();
  auto bodyExpAndTy = body_->Translate(venv, tenv, level, label, errormsg);
  auto bodyExp = bodyExpAndTy->exp_->UnEx();
  auto bodyTy = bodyExpAndTy->ty_;
  auto eseqExp = new tree::EseqExp(leftStm, bodyExp);
  if (isMain) {
    frags->PushBack(
        new frame::ProcFrag(new tree::ExpStm(eseqExp), level->frame_));
  }
  return new tr::ExpAndTy(new tr::ExExp(eseqExp), bodyTy);
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::ArrayTy *arrayTy =
      static_cast<type::ArrayTy *>(tenv->Look(typ_)->ActualTy());

  auto sizeExp =
      size_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();

  auto initExp =
      init_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();

  auto argList = new tree::ExpList;
  argList->Append(sizeExp);
  argList->Append(initExp);
  auto callInitArrayExp = tr::ExternalCall("init_array", argList);
  auto a = temp::TempFactory::NewTemp();
  auto tempExp = new tree::TempExp(a);
  auto moveStm = new tree::MoveStm(tempExp, callInitArrayExp);

  auto eseqExp = new tree::EseqExp(moveStm, new tree::TempExp(a));
  return new tr::ExpAndTy(new tr::ExExp(eseqExp), arrayTy);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto voidExp = new tree::ConstExp(0);
  return new tr::ExpAndTy(new tr::ExExp(voidExp), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto fundecList = functions_->GetList();
  for (auto fundec : fundecList) {
    std::list<bool> escapeList;
    auto params = fundec->params_->GetList();
    for (auto param : params) {
      escapeList.push_back(param->escape_);
    }
    auto newLevel = tr::Level::NewLevel(level, fundec->name_, escapeList);
    type::Ty *result_ty = fundec->result_ ? tenv->Look(fundec->result_)
                                          : type::VoidTy::Instance();
    type::TyList *formalTys = fundec->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(fundec->name_, new env::FunEntry(newLevel, fundec->name_,
                                                 formalTys, result_ty));
  }

  for (auto fundec : fundecList) {
    venv->BeginScope();
    auto params = fundec->params_;
    type::TyList *formalTyList = params->MakeFormalTyList(tenv, errormsg);
    auto formalTy_it = formalTyList->GetList().begin();
    auto paramList = params->GetList();
    auto param_it = paramList.begin();

    auto funEntry = dynamic_cast<env::FunEntry *>(venv->Look(fundec->name_));
    auto funcLevel = funEntry->level_;
    auto formalAccessList = funcLevel->frame_->Formals();
    auto acc_it = formalAccessList.begin();
    ++acc_it;
    //    errormsg->Error(pos_, "size = " +
    //    std::to_string(formalAccessList.size()));

    for (; param_it != paramList.end(); formalTy_it++, param_it++, acc_it++)
      venv->Enter(
          (*param_it)->name_,
          new env::VarEntry(new tr::Access(funcLevel, *acc_it), *formalTy_it));
    auto bodyExpAndTy = fundec->body_->Translate(venv, tenv, funcLevel,
                                                 funEntry->label_, errormsg);
    venv->EndScope();
    frags->PushBack(new frame::ProcFrag(
        frame::ProcEntryExit1(
            funcLevel->frame_,
            new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()),
                              bodyExpAndTy->exp_->UnEx())),
        funcLevel->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto initExpAndTy = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *initTy = initExpAndTy->ty_;
  auto initExp = initExpAndTy->exp_->UnEx();
  type::Ty *varTy = initTy;

  auto access = tr::Access::AllocLocal(level, escape_);
  auto varEntry = new env::VarEntry(access, varTy, escape_);
  venv->Enter(var_, varEntry);
  tree::Exp *framePtr = new tree::TempExp(reg_manager->FramePointer());
  auto varExp = access->access_->ToExp(framePtr);
  auto moveStm = new tree::MoveStm(varExp, initExp);
  return new tr::NxExp(moveStm);
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto nameAndTyList = types_->GetList();
  for (auto nameAndTy : nameAndTyList) {
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, nullptr));
  }

  for (auto nameAndTy : nameAndTyList) {
    auto nameTy = static_cast<type::NameTy *>(tenv->Look(nameAndTy->name_));
    auto rightTy = nameAndTy->ty_->Translate(tenv, errormsg);
    nameTy->ty_ = rightTy;
    tenv->Set(nameAndTy->name_, nameTy);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return tenv->Look(name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto fieldList = new type::FieldList;
  auto recordList = record_->GetList();
  auto r_iter = recordList.begin();
  for (; r_iter != recordList.end(); ++r_iter) {
    auto name = (*r_iter)->name_;
    auto type = (*r_iter)->typ_;
    type::Ty *ty = tenv->Look(type);
    fieldList->Append(new type::Field(name, ty));
  }
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto arrayTy = tenv->Look(array_);
  return new type::ArrayTy(arrayTy->ActualTy());
}

} // namespace absyn
