#include "straightline/slp.h"
#include <algorithm>
#include <iostream>

namespace A {
    int A::CompoundStm::MaxArgs() const {
        return std::max(stm1->MaxArgs(), stm2->MaxArgs());
    }

    Table *A::CompoundStm::Interp(Table *t) const {
        Table* t1 = stm1->Interp(t);
        return stm2->Interp(t1);
    }

    int A::AssignStm::MaxArgs() const {
        return exp->MaxArgs();
    }

    Table *A::AssignStm::Interp(Table *t) const {
        IntAndTable *expT = exp->Interp(t);
        return expT->t->Update(id, expT->i);
    }

    int A::PrintStm::MaxArgs() const {
        return std::max(exps->MaxArgs(), exps->NumExps());
    }

    Table *A::PrintStm::Interp(Table *t) const {
        Table *curT = t;
        ExpList *curE = exps;
        while(curE)
        {
            IntAndTable *iat = curE->Interp(curT);
            curT = iat->t;
            curE = curE->Next();
            printf("%d ", iat->i);
        }
        printf("\n");
        return curT;
    }

    int A::IdExp::MaxArgs() const {
        return 0;
    }

    IntAndTable *A::IdExp::Interp(Table *t) const {
        return new IntAndTable(t->Lookup(id), t);
    }

    int A::NumExp::MaxArgs() const {
        return 0;
    }

    IntAndTable *A::NumExp::Interp(Table *t) const {
        return new IntAndTable(num, t);
    }

    int A::OpExp::MaxArgs() const {
        return 0;
    }

    IntAndTable *A::OpExp::Interp(Table *t) const {
        IntAndTable *lt = left->Interp(t);
        IntAndTable *rt = right->Interp(t);
        switch (oper) {
            case PLUS:
                return new IntAndTable(lt->i + rt->i, t);
            case MINUS:
                return new IntAndTable(lt->i - rt->i, t);
            case TIMES:
                return new IntAndTable(lt->i * rt->i, t);
            case DIV:
                return new IntAndTable(lt->i / rt->i, t);
        }
    }

    int A::EseqExp::MaxArgs() const {
        return stm->MaxArgs();
    }

    IntAndTable *A::EseqExp::Interp(Table *t) const {
        Table *stmT = stm->Interp(t);
        return exp->Interp(stmT);
    }

    int A::PairExpList::MaxArgs() const {
        int maxArgs = exp->MaxArgs();
        if (tail != nullptr) {
            maxArgs = std::max(maxArgs, tail->MaxArgs());
        }
        return maxArgs;
    }

    int A::PairExpList::NumExps() const {
        return 1 + tail->NumExps();
    }

    IntAndTable *A::PairExpList::Interp(Table *t) const {
        return exp->Interp(t);
    }

    ExpList *A::PairExpList::Next() const {
        return tail;
    }

    int A::LastExpList::MaxArgs() const {
        return exp->MaxArgs();
    }

    int A::LastExpList::NumExps() const {
        return 1;
    }

    IntAndTable *A::LastExpList::Interp(Table *t) const {
        return exp->Interp(t);
    }

    ExpList *A::LastExpList::Next() const {
        return nullptr;
    }

    int Table::Lookup(const std::string &key) const {
        if (id == key) {
            return value;
        } else if (tail != nullptr) {
            return tail->Lookup(key);
        } else {
            assert(false);
        }
    }

    Table *Table::Update(const std::string &key, int val) const {
        return new Table(key, val, this);
    }
}  // namespace A

