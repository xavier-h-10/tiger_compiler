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
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assemInstr)
      : frame(frame), assemInstr(std::move(assemInstr)),
        result(std::make_unique<Result>()) {}

  void RegAlloc();

  inline std::unique_ptr<Result> TransferResult() { return std::move(result); }

private:
  frame::Frame *frame;
  std::unique_ptr<cg::AssemInstr> assemInstr;

  live::LiveGraphFactory *liveGraphFactory;
  std::unique_ptr<Result> result;
  static constexpr int K = 15;

  std::map<temp::Temp *, int> colors;
  std::map<live::INodePtr, int> degree;
  std::map<live::INodePtr, live::INodePtr> alias;
  temp::TempList *alreadySpilled = new temp::TempList;

  graph::NodeList<temp::Temp> *initial = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *precolored = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *spill_worklist = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *freeze_worklist =
      new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *simplify_worklist =
      new graph::NodeList<temp::Temp>;

  graph::NodeList<temp::Temp> *spilled_nodes = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *colored_nodes = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *coalesced_nodes =
      new graph::NodeList<temp::Temp>;

  live::MoveList *active_moves = new live::MoveList();
  live::MoveList *coalesced_moves = new live::MoveList();
  live::MoveList *constrained_moves = new live::MoveList();
  live::MoveList *frozen_moves = new live::MoveList();

  std::stack<live::INodePtr> select_stack;

  live::IGraphPtr interf_graph;
  live::MoveList *worklist_moves;
  live::MvList *move_list;

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
  void AddWorkList(live::INodePtr node);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservative(graph::NodeList<temp::Temp> *nodes);
  graph::Node<temp::Temp> *GetAlias(graph::Node<temp::Temp> *n);
  bool judge_ok(live::INodePtr u, live::INodePtr v);
  void Combine(live::INodePtr u, live::INodePtr v);
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
};

} // namespace ra

#endif