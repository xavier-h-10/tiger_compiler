#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"
#include <map>
#include <stack>

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result(){};
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr_)
      : frame(frame), assem_instr_(std::move(assem_instr_)),
        result(std::make_unique<Result>()) {
    initial = new graph::NodeList<temp::Temp>();
    precolored = new graph::NodeList<temp::Temp>();

    simplify_worklist = new graph::NodeList<temp::Temp>();
    freeze_worklist = new graph::NodeList<temp::Temp>();
    spill_worklist = new graph::NodeList<temp::Temp>();
    coalesced_nodes = new graph::NodeList<temp::Temp>();
    spilled_nodes = new graph::NodeList<temp::Temp>();
    colored_nodes = new graph::NodeList<temp::Temp>();

    constrained_moves = new live::MoveList();
    coalesced_moves = new live::MoveList();
    constrained_moves = new live::MoveList();
    frozen_moves = new live::MoveList();
    active_moves= new live::MoveList();

    already_spill = new temp::TempList();
  }

  void RegAlloc();

  inline std::unique_ptr<Result> TransferResult() { return std::move(result); }

private:
  frame::Frame *frame;

  std::unique_ptr<cg::AssemInstr> assem_instr_;

  live::LiveGraphFactory *lg_factory;
  std::unique_ptr<Result> result;

  std::map<temp::Temp *, int> color;
  std::map<live::INodePtr, int> degree;
  std::map<live::INodePtr, live::INodePtr> alias;

  graph::NodeList<temp::Temp> *initial;
  graph::NodeList<temp::Temp> *precolored;
  graph::NodeList<temp::Temp> *spill_worklist;
  graph::NodeList<temp::Temp> *freeze_worklist;
  graph::NodeList<temp::Temp> *simplify_worklist;

  graph::NodeList<temp::Temp> *spilled_nodes;
  graph::NodeList<temp::Temp> *colored_nodes;
  graph::NodeList<temp::Temp> *coalesced_nodes;

  live::MoveList *coalesced_moves;
  live::MoveList *constrained_moves;
  live::MoveList *frozen_moves;
  live::MoveList *worklist_moves;
  live::MoveList *active_moves;

  std::stack<graph::Node<temp::Temp> *> select_stack;

  graph::Graph<temp::Temp> *interf_graph;
  live::MvList *move_list;

  temp::TempList *already_spill;

  void LivenessAnalysis();
  void Build();
  void AddEdge(graph::Node<temp::Temp> *u, graph::Node<temp::Temp> *v);
  void MakeWorkList();
  live::MoveList *NodeMoves(graph::Node<temp::Temp> *n);
  bool MoveRelated(graph::Node<temp::Temp> *n);
  void Simplify();
  void DecrementDegree(graph::Node<temp::Temp> *m);
  void EnableMoves(graph::NodeList<temp::Temp> *nodes);
  void Coalesce();
  void AddWorkList(graph::Node<temp::Temp> *u);
  bool OK(graph::Node<temp::Temp> *t, graph::Node<temp::Temp> *r);
  bool Conservative(graph::NodeList<temp::Temp> *nodes);
  graph::Node<temp::Temp> *GetAlias(graph::Node<temp::Temp> *n);
  void Combine(graph::Node<temp::Temp> *u, graph::Node<temp::Temp> *v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  void AssignColors();
  void RewriteProgram();
  void Color();
  void AssignRegisters();
  void AssignTemps(temp::TempList *temps);
  void RemoveMoves();
  bool SameMove(temp::TempList *src, temp::TempList *dst);
  bool judge_ok(graph::Node<temp::Temp> *u, graph::Node<temp::Temp> *v);
};

} // namespace ra

#endif

//
////  tab::Table<graph::Node<temp::Temp>, std::string> *color;
//
//std::map<graph::Node<temp::Temp> *, int> degree;
//std::set<std::pair<graph::Node<temp::Temp> *, graph::Node<temp::Temp> *>>
//    adj_set;
//std::map<graph::Node<temp::Temp> *, graph::NodeList<temp::Temp> *> adj_list;
//std::map<graph::Node<temp::Temp> *, graph::Node<temp::Temp> *> alias;
//std::map<graph::Node<temp::Temp> *, live::MoveList *> move_list;
//
////  std::map<graph::Node<temp::Temp> *, std::string> color;