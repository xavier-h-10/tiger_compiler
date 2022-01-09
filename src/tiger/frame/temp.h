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

public:
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
  TempList(std::list<Temp *> list) : temp_list_(list) {}
  TempList() = default;

  inline int Size() { return temp_list_.size(); }

  inline bool Empty() { return temp_list_.size() == 0; }

  void Append(Temp *t) { temp_list_.push_back(t); }
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }
  void Union(TempList *p) {
    auto p_list = p->GetList();
    for (auto temp : p_list) {
      auto pos = std::find(temp_list_.begin(), temp_list_.end(), temp);
      if (pos == temp_list_.end()) {
        temp_list_.push_back(temp);
      }
    }
  }

  inline void Clear() { temp_list_.clear(); }

  void Diff(TempList *p) {
    auto p_list = p->GetList();
    for (auto temp : p_list) {
      auto pos = std::find(temp_list_.begin(), temp_list_.end(), temp);
      if (pos != temp_list_.end()) {
        temp_list_.erase(pos);
      }
    }
  }

  bool Contain(Temp *temp) {
    for (auto now : temp_list_) {
      if (temp == now)
        return true;
    }
    return false;
  }

  void Replace(Temp *old_temp, Temp *new_temp) {
    for (auto it = temp_list_.begin(); it != temp_list_.end(); it++) {
      std::cout << "replace "
                << "old=" << *temp::Map::Name()->Look(old_temp)
                << " new=" << *temp::Map::Name()->Look(new_temp) << std::endl;
      if (*it == old_temp) {
        *it = new_temp;
      }
    }
  }

private:
  std::list<Temp *> temp_list_;
};

} // namespace temp

#endif
