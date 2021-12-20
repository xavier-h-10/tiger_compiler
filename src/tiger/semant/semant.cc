#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"
#include <iostream>

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(sym_);
  //  std::cout << "try to look up: " << sym_->Name() << std::endl;
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (varTy && typeid(*varTy) == typeid(type::RecordTy)) {
    auto recTy = static_cast<type::RecordTy *>(varTy);
    auto fieldList = recTy->fields_->GetList();
    for (auto field : fieldList) {
      if (field->name_ == sym_) {
        return field->ty_;
      }
    }
    errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  } else {
    errormsg->Error(pos_, "not a record type");
  }
  return type::NilTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  auto varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varTy) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::VoidTy::Instance();
  }
  auto subTy = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!subTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "int required");
  }
  return (static_cast<type::ArrayTy *>(varTy))->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *entry = venv->Look(func_);
  //  std::cout << "try to look up func: " << func_->Name() << std::endl;
  if (entry && typeid(*entry) == typeid(env::FunEntry)) {
    env::FunEntry *funEntry = static_cast<env::FunEntry *>(entry);
    auto argList = funEntry->formals_->GetList();
    auto argIter = argList.begin();
    auto expList = args_->GetList();
    auto expIter = expList.begin();
    for (; expIter != expList.end(); ++expIter, ++argIter) {
      if (argIter == argList.end()) {
        errormsg->Error(pos_, "too many params in function %s",
                        func_->Name().c_str());
        break;
      }
      type::Ty *expTy =
          (*expIter)->SemAnalyze(venv, tenv, labelcount, errormsg);
      type::Ty *argTy = *argIter;
      if (!expTy->IsSameType(argTy)) {
        errormsg->Error(pos_, "para type mismatch");
        break;
      }
    }
    return funEntry->result_;
  }
  //    std::cout << "Trying to look up funentry: " << func_->Name() << " "
  //              << funEntry->result_ << std::endl;
  else {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
  }
  return type::IntTy::Instance();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *leftTy = left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *rightTy = right_->SemAnalyze(venv, tenv, labelcount, errormsg);
  switch (oper_) {
  case PLUS_OP:
  case MINUS_OP:
  case TIMES_OP:
  case DIVIDE_OP:
    if (!leftTy->IsSameType(rightTy) ||
        !leftTy->IsSameType(type::IntTy::Instance())) {
      errormsg->Error(pos_, "integer required");
    }
    break;
  case EQ_OP:
  case NEQ_OP:
  case LT_OP:
  case LE_OP:
  case GT_OP:
  case GE_OP:
    if (!leftTy->IsSameType(rightTy)) {
      errormsg->Error(pos_, "same type required");
    }
  default:
    break;
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (ty && typeid(*(ty->ActualTy())) == typeid(type::RecordTy)) {
    type::RecordTy *recTy = static_cast<type::RecordTy *>(ty->ActualTy());
    auto fields = recTy->fields_->GetList();
    auto efields = this->fields_->GetList();
    auto f_iter = fields.begin();
    auto ef_iter = efields.begin();
    if (fields.size() != efields.size()) {
      errormsg->Error(pos_, "record exp arg number not match");
    } else {
      type::Ty *efieldTy;
      for (; f_iter != fields.end(); ++f_iter, ++ef_iter) {
        if ((*f_iter)->name_ != (*ef_iter)->name_) {
          errormsg->Error(pos_, "record exp arg name not match");
          break;
        } else {
          efieldTy =
              (*ef_iter)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
          if (!efieldTy->IsSameType((*f_iter)->ty_)) {
            errormsg->Error(pos_, "record exp arg type not match");
            break;
          }
        }
      }
    }
  } else {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
  }
  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto expList = seq_->GetList();
  auto last = expList.rbegin();
  for (auto iter = expList.begin(); iter != expList.end(); ++iter) {
    if (*iter == *last) {
      break;
    }
    (*iter)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return (*last)->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *expTy = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar *simpleVar = static_cast<SimpleVar *>(var_);
    env::EnvEntry *entry = venv->Look(simpleVar->sym_);
    if (entry && entry->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
    }
  } else if (!varTy->IsSameType(type::NilTy::Instance()) &&
             !varTy->IsSameType(expTy)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  type::Ty *testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *thenTy = then_->SemAnalyze(venv, tenv, labelcount, errormsg);

  type::Ty *retTy = nullptr;
  //  std::cout << "begin ifexp" << std::endl;
  if (elsee_) {
    type::Ty *elseTy = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    retTy = elseTy;
    //    std::cout << pos_ << ": judging if then is the same with else" <<
    //    std::endl;
    if (!thenTy->IsSameType(elseTy)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
  } else {
    //    std::cout << "its a if-then exp" << std::endl;
    if (!thenTy->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(pos_, "if-then exp's body must produce no value");
    }
    retTy = type::VoidTy::Instance();
  }

  tenv->EndScope();
  venv->EndScope();
  //  std::cout << "end ifexp" << std::endl;
  return retTy;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  type::Ty *testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!testTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "wrong test type");
  }

  type::Ty *bodyTy = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);

  if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
  }

  tenv->EndScope();
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *loTy = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hiTy = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

  // lo_ and hi_ should be all int type
  if (loTy != type::IntTy::Instance()) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }

  if (hiTy != type::IntTy::Instance()) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  type::Ty *bodyTy = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);

  // body exp should be void type
  if (!bodyTy->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "in for exp, body should have no value");
  }

  tenv->EndScope();
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  if (labelcount == 0) {
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  venv->BeginScope();
  tenv->BeginScope();

  auto decList = decs_->GetList();
  for (Dec *dec : decList) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  auto ret = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  tenv->EndScope();
  venv->EndScope();

  return ret;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_)->ActualTy();
  type::ArrayTy *arrayTy = nullptr;
  if (!ty) {
    errormsg->Error(pos_, "in array exp, array type is not defined");
  } else {
    arrayTy = static_cast<type::ArrayTy *>(ty);
  }

  type::Ty *sizeTy = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!sizeTy->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(size_->pos_, "in array exp, size should be int");
  }

  type::Ty *initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!initTy->IsSameType(arrayTy->ty_)) {
    errormsg->Error(pos_, "type mismatch");
  }
  return ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

bool existsDec(const std::list<FunDec *> &fundecList, sym::Symbol *func,
               FunDec *except) {
  for (auto fundec : fundecList) {
    if (fundec != except) {
      if (fundec->name_ == func)
        return true;
    }
  }
  return false;
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  auto fundecList = functions_->GetList();
  for (auto fundec : fundecList) {
    type::Ty *result_ty = fundec->result_ ? tenv->Look(fundec->result_)
                                          : type::VoidTy::Instance();
    type::TyList *formals = fundec->params_->MakeFormalTyList(tenv, errormsg);
    if (existsDec(fundecList, fundec->name_, fundec)) {
      errormsg->Error(fundec->pos_, "two functions have the same name");
      return;
    }
    venv->Enter(fundec->name_, new env::FunEntry(formals, result_ty));
    //    std::cout << "enter func: " << fundec->name_->Name() << std::endl;
  }

  for (auto fundec : fundecList) {
    venv->BeginScope();
    type::TyList *formals = fundec->params_->MakeFormalTyList(tenv, errormsg);
    auto formal_it = formals->GetList().begin();
    auto paramList = fundec->params_->GetList();
    auto param_it = paramList.begin();

    type::Ty *result_ty = fundec->result_ ? tenv->Look(fundec->result_)
                                          : type::VoidTy::Instance();
    for (; param_it != paramList.end(); formal_it++, param_it++)
      venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
    type::Ty *bodyTy =
        fundec->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (result_ty->IsSameType(type::VoidTy::Instance()) &&
        !bodyTy->IsSameType(result_ty)) {
      errormsg->Error(fundec->body_->pos_, "procedure returns value");
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  type::Ty *decTy = typ_ ? tenv->Look(typ_)->ActualTy() : nullptr;
  type::Ty *initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (initTy) {
    if (!decTy) {
      if (typeid(*initTy) == typeid(type::NilTy)) {
        errormsg->Error(pos_, "init should not be nil without type specified");
      }
    } else if (!decTy->IsSameType(initTy)) {
      errormsg->Error(init_->pos_, "type mismatch");
    }
  }
  type::Ty *varTy = decTy ? decTy : initTy;
  venv->Enter(var_, new env::VarEntry(varTy));
  //  std::cout << "enter var: " << var_->Name() << " " << varTy << std::endl;
}

bool existsCycle(type::Ty *ty) {
  auto curTy = ty;
  while (curTy) {
    if (typeid(*curTy) == typeid(type::NameTy)) {
      curTy = (static_cast<type::NameTy *>(curTy))->ty_;
      if (curTy == ty) {
        return true;
      }
    } else {
      return false;
    }
  }
  return false;
}

bool existsDec(const std::list<NameAndTy *> &typeList, sym::Symbol *name,
               NameAndTy *except) {
  for (auto nameNTy : typeList) {
    if (nameNTy != except) {
      if (nameNTy->name_ == name)
        return true;
    }
  }
  return false;
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  auto typeList = types_->GetList();

  for (auto nameNTy : typeList) {
    if (existsDec(typeList, nameNTy->name_, nameNTy)) {
      errormsg->Error(nameNTy->ty_->pos_, "two types have the same name");
      return;
    }
    tenv->Enter(nameNTy->name_, new type::NameTy(nameNTy->name_, nullptr));
    //    std::cout << "add namety: " << nameNTy->name_->Name() << std::endl;
  }

  for (auto nameNTy : typeList) {
    auto nameTy = static_cast<type::NameTy *>(tenv->Look(nameNTy->name_));
    nameTy->ty_ = nameNTy->ty_->SemAnalyze(tenv, errormsg);
    tenv->Set(nameNTy->name_, nameTy);
    //    std::cout << "now namety: " << nameNTy->name_->Name() << " is: " <<
    //    nameTy
    //              << std::endl;
  }

  for (auto nameNTy : typeList) {
    auto ty = tenv->Look(nameNTy->name_);
    if (existsCycle(ty)) {
      errormsg->Error(nameNTy->ty_->pos_, "illegal type cycle");
      return;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  return tenv->Look(name_);
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  auto fieldList = new type::FieldList;
  auto recordList = record_->GetList();
  auto r_iter = recordList.begin();
  for (; r_iter != recordList.end(); ++r_iter) {
    auto name = (*r_iter)->name_;
    auto type = (*r_iter)->typ_;
    type::Ty *ty = tenv->Look(type);
    //    std::cout << "append: " << name->Name() << " " << (long long)ty
    //              << std::endl;
    if (ty == nullptr) {
      errormsg->Error((*r_iter)->pos_, "undefined type %s",
                      type->Name().c_str());
    }
    fieldList->Append(new type::Field(name, ty));
  }
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  auto arrayTy = tenv->Look(array_);
  if (arrayTy)
    return new type::ArrayTy(arrayTy->ActualTy());
  else
    return nullptr;
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
