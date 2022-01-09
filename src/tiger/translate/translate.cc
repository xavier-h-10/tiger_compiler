#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

#include <iostream>

#define word_size 8

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  frame::Frame *frame = level->frame_;
  frame::Access *access = frame->AllocLocal(escape);
  return new Access(level, access);
}

using PatchList = std::list<temp::Label **>;
void DoPatch(PatchList t_list, temp::Label *label) {
  for (auto t : t_list) {
    *t = label;
  }
}

PatchList JoinPatch(PatchList first, PatchList second) {
  /* use std::list function can simply implement this */
  first.splice(first.end(), second);
  return first;
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)),
                           args);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx() = default;
  Cx(const PatchList &trues, const PatchList &falses, tree::Stm *stm) //修改
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

  [[nodiscard]] tree::Exp *UnEx() const override { return exp_; }

  [[nodiscard]] tree::Stm *UnNx() const override {
    return new tree::ExpStm(exp_);
  }

  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    // think
    tree::CjumpStm *stm = new tree::CjumpStm(
        tree::RelOp::EQ_OP, exp_, new tree::ConstExp(1), nullptr, nullptr);
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
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }

  [[nodiscard]] tree::Stm *UnNx() const override { return stm_; }

  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    errormsg->Error(0, "NxExp: UnCx called");
    return Cx();
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(const PatchList &trues, const PatchList &falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
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
    // think
    return new tree::ExpStm(UnEx());
  }

  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { return cx_; }
};

void ProgTr::Translate() {
  /* std::cout << "ProgTr::Translate called" << std::endl;*/
  //  temp::Label *label = temp::LabelFactory::newLabel();
  FillBaseTEnv();
  FillBaseVEnv();

  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(),
                         temp::LabelFactory::NamedLabel("tigermain"),
                         errormsg_.get());
}

tree::Exp *FramePtr(Level *level, Level *access_level) {
  tree::Exp *frame = new tree::TempExp(reg_manager->FramePointer());
  while (level != access_level) {
    frame = level->frame_->Formals().front()->ToExp(frame);
    level = level->parent_;
  }
  return frame;
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  env::VarEntry *var_entry = (env::VarEntry *)entry;
  tr::Access *access = var_entry->access_;
  tree::Exp *frame = FramePtr(level, access->level_);
  tr::Exp *exp = new tr::ExExp(access->access_->ToExp(frame));
  return new tr::ExpAndTy(exp, var_entry->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  type::RecordTy *ty = dynamic_cast<type::RecordTy *>(var->ty_->ActualTy());
  int offset = 0;
  //  if (ty && typeid(*ty) == typeid(type::RecordTy)) {
  std::list<type::Field *> field_list =
      ((type::RecordTy *)ty)->fields_->GetList();
  for (auto field : field_list) {
    if (field->name_ == sym_) {
      // p115
      tree::BinopExp *addr =
          new tree::BinopExp(tree::PLUS_OP, var->exp_->UnEx(),
                             new tree::ConstExp(offset * word_size));
      tr::ExExp *field_exp = new tr::ExExp(new tree::MemExp(addr));
      return new tr::ExpAndTy(field_exp, field->ty_);
    }
    offset++;
  }
  //  }
  errormsg->Error(pos_, "FieldVar error");
  return new tr::ExpAndTy(nullptr, type::NilTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  type::ArrayTy *ty = (type::ArrayTy *)var->ty_;

  tree::Exp *var_exp = var->exp_->UnEx();
  tree::Exp *subscript_exp =
      subscript_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  tree::Exp *offset = new tree::BinopExp(tree::MUL_OP, subscript_exp,
                                         new tree::ConstExp(word_size));

  tr::Exp *exp = new tr::ExExp(
      new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, var_exp, offset)));
  return new tr::ExpAndTy(exp, ty->ty_);
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)),
                          type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)),
                          type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *label_ = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(label_, str_));
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(label_)),
                          type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  tree::ExpList *list = new tree::ExpList();
  std::list<absyn::Exp *> arg_list = args_->GetList();
  for (auto arg : arg_list) {
    tree::Exp *tmp =
        arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    list->Append(tmp);
  }

  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
  env::FunEntry *entry = (env::FunEntry *)(venv->Look(func_));
  tr::Level *parent = entry->level_->parent_;
  if (parent) {
    list->Insert(FramePtr(level, parent));
    tree::Exp *exp = new tree::CallExp(new tree::NameExp(func_), list);
    return new tr::ExpAndTy(new tr::ExExp(exp), entry->result_);
  } else {
    tree::Exp *exp = tr::ExternalCall(func_->Name(), list);
    return new tr::ExpAndTy(new tr::ExExp(exp), entry->result_);
  }
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = nullptr;
  switch (oper_) {
  case absyn::PLUS_OP: {
    exp = new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, left->exp_->UnEx(),
                                           right->exp_->UnEx()));
    break;
  }
  case absyn::MINUS_OP: {
    exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, left->exp_->UnEx(),
                                           right->exp_->UnEx()));
    break;
  }
  case absyn::TIMES_OP: {
    exp = new tr::ExExp(new tree::BinopExp(tree::MUL_OP, left->exp_->UnEx(),
                                           right->exp_->UnEx()));
    break;
  }
  case absyn::DIVIDE_OP: {
    exp = new tr::ExExp(new tree::BinopExp(tree::DIV_OP, left->exp_->UnEx(),
                                           right->exp_->UnEx()));
    break;
  }
  case absyn::EQ_OP:
  case absyn::NEQ_OP: {
    tree::RelOp op;
    tree::CjumpStm *stm;
    if (oper_ == absyn::EQ_OP)
      op = tree::EQ_OP;
    else
      op = tree::NE_OP;
    type::Ty *left_ty = left->ty_->ActualTy();
    type::Ty *right_ty = right->ty_->ActualTy();
    //    std::cout<<"translate op_exp eq_op || neq_op
    //    called"<<typeid(*left_ty).name()<<"
    //    "<<typeid(*right_ty).name()<<std::endl;
    if (typeid(*left_ty) == typeid(type::StringTy)) {
      stm = new tree::CjumpStm(
          op,
          tr::ExternalCall(
              "string_equal",
              new tree::ExpList({left->exp_->UnEx(), right->exp_->UnEx()})),
          new tree::ConstExp(1), nullptr, nullptr);
    } else {
      stm = new tree::CjumpStm(op, left->exp_->UnEx(), right->exp_->UnEx(),
                               nullptr, nullptr);
    }
    tr::PatchList trues = {&stm->true_label_};
    tr::PatchList falses = {&stm->false_label_};
    exp = new tr::CxExp(trues, falses, stm);
    break;
  }
  case absyn::LT_OP:
  case absyn::LE_OP:
  case absyn::GT_OP:
  case absyn::GE_OP: {
    tree::RelOp op;
    tree::CjumpStm *stm;
    if (oper_ == absyn::LT_OP)
      op = tree::LT_OP;
    else if (oper_ == absyn::LE_OP)
      op = tree::LE_OP;
    else if (oper_ == absyn::GT_OP)
      op = tree::GT_OP;
    else
      op = tree::GE_OP;
    stm = new tree::CjumpStm(op, left->exp_->UnEx(), right->exp_->UnEx(),
                             nullptr, nullptr);
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
  std::list<absyn::EField *> list = fields_->GetList();
  type::Ty *ty = tenv->Look(typ_);

  // call alloc record
  int size = list.size() * word_size;
  temp::Temp *r = temp::TempFactory::NewTemp();
  tree::Stm *alloc_stm = new tree::MoveStm(
      new tree::TempExp(r),
      tr::ExternalCall("alloc_record",
                       new tree::ExpList({new tree::ConstExp(size)})));

  // translate each field
  int now = list.size() - 1;
  tree::Stm *seq_stm = nullptr;
  std::list<absyn::EField *>::reverse_iterator it;
  for (it = list.rbegin(); it != list.rend(); it++) {
    absyn::EField *field = *it;
    tree::Stm *tmp = new tree::MoveStm(
        new tree::MemExp(
            new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(r),
                               new tree::ConstExp(now * word_size))),
        field->exp_->Translate(venv, tenv, level, label, errormsg)
            ->exp_->UnEx());

    if (seq_stm) {
      seq_stm = new tree::SeqStm(tmp, seq_stm);
    } else {
      seq_stm = tmp;
    }
    now--;
  }

  seq_stm = new tree::SeqStm(alloc_stm, seq_stm);
  return new tr::ExpAndTy(
      new tr::ExExp(new tree::EseqExp(seq_stm, new tree::TempExp(r))), ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  std::list<absyn::Exp *> exp_list = seq_->GetList();
  tr::Exp *seq_exp = new tr::ExExp(new tree::ConstExp(0));
  type::Ty *ty = type::IntTy::Instance();
  for (auto exp : exp_list) {
    tr::ExpAndTy *tmp = exp->Translate(venv, tenv, level, label, errormsg);
    ty = tmp->ty_;
    if (tmp->exp_) {
      seq_exp =
          new tr::ExExp(new tree::EseqExp(seq_exp->UnNx(), tmp->exp_->UnEx()));
    } else {
      seq_exp = new tr::ExExp(
          new tree::EseqExp(seq_exp->UnNx(), new tree::ConstExp(0)));
    }
  }
  return new tr::ExpAndTy(seq_exp, ty);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);
  tree::MoveStm *stm = new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx());
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then = then_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *elsee = nullptr;
  if (elsee_) {
    elsee = elsee_->Translate(venv, tenv, level, label, errormsg);
  }
  tr::Cx cx = test->exp_->UnCx(errormsg);
  temp::Label *true_label = temp::LabelFactory::NewLabel();
  temp::Label *false_label = temp::LabelFactory::NewLabel();
  tr::DoPatch(cx.trues_, true_label);
  tr::DoPatch(cx.falses_, false_label);
  if (elsee_) {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *join = temp::LabelFactory::NewLabel();
    tr::Exp *exp = new tr::ExExp(new tree::EseqExp(
        cx.stm_,
        new tree::EseqExp(
            new tree::LabelStm(true_label),
            new tree::EseqExp(
                new tree::MoveStm(new tree::TempExp(r), then->exp_->UnEx()),
                new tree::EseqExp(
                    new tree::JumpStm(new tree::NameExp(join),
                                      new std::vector<temp::Label *>({join})),
                    new tree::EseqExp(
                        new tree::LabelStm(false_label),
                        new tree::EseqExp(
                            new tree::MoveStm(new tree::TempExp(r),
                                              elsee->exp_->UnEx()),
                            new tree::EseqExp(
                                new tree::JumpStm(
                                    new tree::NameExp(join),
                                    new std::vector<temp::Label *>({join})),
                                new tree::EseqExp(new tree::LabelStm(join),
                                                  new tree::TempExp(r))))))))));
    return new tr::ExpAndTy(exp, then->ty_);
  } else {
    tr::Exp *exp = new tr::NxExp(new tree::SeqStm(
        cx.stm_,
        new tree::SeqStm(new tree::LabelStm(true_label),
                         new tree::SeqStm(then->exp_->UnNx(),
                                          new tree::LabelStm(false_label)))));
    return new tr::ExpAndTy(exp, type::VoidTy::Instance());
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  tr::Cx test_cx = test_->Translate(venv, tenv, level, label, errormsg)
                       ->exp_->UnCx(errormsg);
  tree::Stm *body_stm =
      body_->Translate(venv, tenv, level, done_label, errormsg)->exp_->UnNx();
  tr::DoPatch(test_cx.trues_, body_label);
  tr::DoPatch(test_cx.falses_, done_label); //回填

  tr::Exp *exp = new tr::NxExp(new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::SeqStm(
          test_cx.stm_,
          new tree::SeqStm(
              new tree::LabelStm(body_label),
              new tree::SeqStm(
                  body_stm,
                  new tree::SeqStm(
                      new tree::JumpStm(
                          new tree::NameExp(test_label),
                          new std::vector<temp::Label *>({test_label})),
                      new tree::LabelStm(done_label)))))));
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  // p119
  absyn::Dec *var_i =
      new absyn::VarDec(0, var_, sym::Symbol::UniqueSymbol("int"), lo_);
  absyn::Dec *var_limit =
      new absyn::VarDec(0, sym::Symbol::UniqueSymbol("limit"),
                        sym::Symbol::UniqueSymbol("int"), hi_); //多一个limit
  absyn::DecList *var_list = new absyn::DecList(var_i, var_limit);

  // i<=limit
  absyn::Exp *test_exp = new absyn::OpExp(
      0, absyn::LE_OP, new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
      new absyn::VarExp(
          0, new absyn::SimpleVar(0, sym::Symbol::UniqueSymbol("limit"))));
  // i:=i+1
  absyn::Exp *inc_exp = new absyn::AssignExp(
      0, new absyn::SimpleVar(0, var_),
      new absyn::OpExp(0, absyn::PLUS_OP,
                       new absyn::VarExp(0, new absyn::SimpleVar(0, var_)),
                       new absyn::IntExp(0, 1)));
  // (body; i:=i+1)
  absyn::Exp *while_exp = new absyn::WhileExp(
      0, test_exp, new absyn::SeqExp(0, new absyn::ExpList(body_, inc_exp)));
  absyn::Exp *exp = new absyn::LetExp(0, var_list, while_exp);
  return exp->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>({label});
  tree::JumpStm *stm = new tree::JumpStm(new tree::NameExp(label), jumps);
  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  static bool is_main = true;
  bool enter = false;
  if (is_main) {
    is_main = false;
    enter = true;
  }
  std::list<absyn::Dec *> dec_list = decs_->GetList();
  tree::Stm *seq_stm = nullptr;
  int now = 0;
  for (auto dec : dec_list) {
    if (now) {
      seq_stm = new tree::SeqStm(
          seq_stm, dec->Translate(venv, tenv, level, label, errormsg)->UnNx());
    } else {
      seq_stm = dec->Translate(venv, tenv, level, label, errormsg)->UnNx();
    }
    now++;
  }
  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *body_exp = body->exp_;
  type::Ty *body_ty = body->ty_;
  tree::Exp *exp = new tree::EseqExp(seq_stm, body_exp->UnEx());
  if (enter) {
    frags->PushBack(new frame::ProcFrag(new tree::ExpStm(exp), level->frame_));
  }
  return new tr::ExpAndTy(new tr::ExExp(exp), body_ty);
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::ArrayTy *array_ty = (type::ArrayTy *)(tenv->Look(typ_)->ActualTy());
  tree::Exp *size =
      size_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  tree::Exp *init =
      init_->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
  tree::ExpList *list = new tree::ExpList({size, init});

  //仿照canon
  tree::Exp *ref = tr::ExternalCall("init_array", list);
  temp::Temp *t = temp::TempFactory::NewTemp();
  tree::Exp *exp = new tree::EseqExp(
      new tree::MoveStm(new tree::TempExp(t), ref), new tree::TempExp(t));
  return new tr::ExpAndTy(new tr::ExExp(exp), array_ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  std::list<FunDec *> fun_dec_list = functions_->GetList();
  for (auto fun_dec : fun_dec_list) {
    // calc escape
    std::list<bool> formals;
    std::list<Field *> field_list = fun_dec->params_->GetList();
    for (auto field : field_list) {
      formals.push_back(field->escape_);
    }
    // add new level
    type::Ty *result = type::VoidTy::Instance();
    if (fun_dec->result_) {
      result = tenv->Look(fun_dec->result_);
    }
    type::TyList *formal_ty_list =
        fun_dec->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(
        fun_dec->name_,
        new env::FunEntry(tr::Level::NewLevel(level, fun_dec->name_, formals),
                          fun_dec->name_, formal_ty_list, result));
  }

  for (auto fun_dec : fun_dec_list) {
    venv->BeginScope();
    tenv->BeginScope();
    std::list<type::Ty*> formal_ty_list =
        fun_dec->params_->MakeFormalTyList(tenv, errormsg)->GetList();
    std::list<Field *> field_list = fun_dec->params_->GetList();

    env::FunEntry *entry = (env::FunEntry *)(venv->Look(fun_dec->name_));
    std::list<frame::Access *> formals = entry->level_->frame_->Formals();

    std::list<type::Ty*>::iterator ty_list_it=formal_ty_list.begin();
    std::list<frame::Access *>::iterator formals_it=formals.begin();
    for(auto field: field_list) {
      formals_it++;
      venv->Enter(field->name_,
                  new env::VarEntry(new tr::Access(entry->level_, *formals_it),
                                    *ty_list_it));
      ty_list_it++;
    }

    tr::ExpAndTy *body = fun_dec->body_->Translate(venv, tenv, entry->level_,
                                                   entry->label_, errormsg);
    venv->EndScope();
    tenv->EndScope();

    frags->PushBack(new frame::ProcFrag(
        frame::ProcEntryExit1(
            entry->level_->frame_,
            new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()),
                              body->exp_->UnEx())),
        entry->level_->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *exp = init->exp_->UnEx();
  type::Ty *ty = init->ty_;
  tree::Exp *frame_exp = new tree::TempExp(reg_manager->FramePointer());

  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, ty, escape_));

  return new tr::NxExp(
      new ::tree::MoveStm(access->access_->ToExp(frame_exp), exp));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  std::list<absyn::NameAndTy *> list = types_->GetList();
  for (auto tmp : list) {
    tenv->Enter(tmp->name_, new type::NameTy(tmp->name_, nullptr));
  }
  for (auto tmp : list) {
    type::NameTy *name_ty = (type::NameTy *)tenv->Look(tmp->name_);
    name_ty->ty_ = tmp->ty_->Translate(tenv, errormsg); // think
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return tenv->Look(name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  // why do we need to translate
  std::list<absyn::Field *> record_list = record_->GetList();
  type::FieldList *field_list = new type::FieldList();
  for (auto field : record_list) {
    sym::Symbol *name = field->name_;
    sym::Symbol *type = field->typ_;
    type::Ty *ty = tenv->Look(type);
    field_list->Append(new type::Field(name, ty));
  }
  return new type::RecordTy(field_list);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
