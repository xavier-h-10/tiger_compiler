#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"
#include <iostream>
#include <list>
#include <set>

namespace temp {

using Label = sym::Symbol;

class LabelFactory {
public:
  static Label *NewLabel();
  static Label *NamedLabel(std::string_view name);
  static std::string LabelString(Label *s);

private:
  int label_id_ = 0;
  static LabelFactory label_factory;
};

class Temp {
  friend class TempFactory;

public:
  [[nodiscard]] int Int() const;

  inline bool isBuiltin() const { return num_ < 100; }

private:
  int num_;
  explicit Temp(int num) : num_(num) {}
};

class TempFactory {
public:
  static Temp *NewTemp();
  static Temp *NewTemp(int id);

private:
  int temp_id_ = 100;
  static TempFactory temp_factory;
};

class Map {
public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void Set(Temp *t, std::string *s);
  void DumpMap(FILE *out);

  static Map *Empty();
  static Map *Name();
  static Map *LayerMap(Map *over, Map *under);

private:
  tab::Table<Temp, std::string> *tab_;
  Map *under_;

  Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {}
  Map(tab::Table<Temp, std::string> *tab, Map *under)
      : tab_(tab), under_(under) {}
};

class TempList {
public:
  explicit TempList(Temp *t) : temp_list_({t}) {}
  TempList(std::initializer_list<Temp *> list) : temp_list_(list) {}
  TempList() = default;

  inline int Size() { return temp_list_.size(); }

  inline bool Empty() { return temp_list_.size() == 0; }

  void Append(Temp *t) { temp_list_.push_back(t); }
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }
  void Union(TempList *another) {
    auto anotherList = another->GetList();
    for (auto anotherTemp : anotherList) {
      auto pos = std::find(temp_list_.begin(), temp_list_.end(), anotherTemp);
      if (pos == temp_list_.end()) {
        temp_list_.push_back(anotherTemp);
      }
    }
  }

  inline void Clear() { temp_list_.clear(); }

  void Diff(TempList *another) {
    auto anotherList = another->GetList();
    for (auto anotherTemp : anotherList) {
      auto pos = std::find(temp_list_.begin(), temp_list_.end(), anotherTemp);
      if (pos != temp_list_.end()) {
        temp_list_.erase(pos);
      }
    }
  }

  void Assign(TempList *another) {
    temp_list_.clear();
    auto anotherList = another->GetList();
    for (auto anotherTemp : anotherList) {
      temp_list_.push_back(anotherTemp);
    }
  }

  bool Contain(Temp *t) {
    for (auto temp : temp_list_) {
      if (temp == t)
        return true;
    }
    return false;
  }

  void Replace(Temp *_old, Temp *_new) {
    auto iter = temp_list_.begin();
    for (; iter != temp_list_.end(); ++iter) {
      if (*iter == _old) {
        *iter = _new;
        std::cout << "replace " << *temp::Map::Name()->Look(_old) << " to "
                  << *temp::Map::Name()->Look(*iter) << std::endl;
      }
    }
  }

  // use should free it manually.
  static TempList *Diff(TempList *one, TempList *another) {
    auto diff = new TempList({});
    auto oneList = one->GetList();
    auto anotherList = another->GetList();
    for (auto oneTemp : oneList) {
      bool exist = false;
      for (auto anotherTemp : anotherList) {
        if (anotherTemp == oneTemp) {
          exist = true;
          break;
        }
      }
      if (!exist) {
        diff->Append(oneTemp);
      }
    }
    return diff;
  }

  static TempList *Union(TempList *one, TempList *another) {
    auto diff = new TempList({});
    auto oneList = one->GetList();
    auto anotherList = another->GetList();
    TempList *ret = new temp::TempList({});
    for (auto oneTemp : oneList)
      ret->Append(oneTemp);

    for (auto anotherTemp : anotherList) {
      auto pos = std::find(oneList.begin(), oneList.end(), anotherTemp);
      if (pos == oneList.end()) {
        ret->Append(anotherTemp);
      }
    }
    return ret;
  }

private:
  std::list<Temp *> temp_list_;
};

} // namespace temp

#endif
