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

//  graph::Table<temp::Temp, live::INode> *alias =
//      new graph::Table<temp::Temp, live::INode>;

  graph::NodeList<temp::Temp> *initial = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *precolored = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *spill_worklist = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *freeze_worklist = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *simplify_worklist =
      new graph::NodeList<temp::Temp>;

  graph::NodeList<temp::Temp> *spilled_nodes = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *colored_nodes = new graph::NodeList<temp::Temp>;
  graph::NodeList<temp::Temp> *coalesced_nodes = new graph::NodeList<temp::Temp>;

  live::MoveList *active_moves = new live::MoveList();
  live::MoveList *coalesced_moves = new live::MoveList();
  live::MoveList *constrained_moves = new live::MoveList();
  live::MoveList *frozen_moves = new live::MoveList();

  std::stack<live::INodePtr> select_stack;

  live::IGraphPtr interf_graph;
  live::MoveList *worklist_moves;
  live::MvList *moveList;

  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void LivenessAnalysis();
  live::MoveList *NodeMoves(live::INodePtr node);
  bool MoveRelated(live::INodePtr node);
  void DecrementDegree(live::INodePtr m);
  void EnableMoves(graph::NodeList<temp::Temp> *nodes);
  void Simplify();
  void MakeWorkList();
  void Coalesce();
  // Briggs algorithm
  bool Conservative(graph::NodeList<temp::Temp> *nodes);
  // George algorithm
  bool CoalesceOK(live::INodePtr u, live::INodePtr v);
  void Combine(live::INodePtr u, live::INodePtr v);
  bool OK(live::INodePtr t, live::INodePtr r);
  void AddWorkList(live::INodePtr node);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  live::INodePtr GetAlias(live::INodePtr node);
  void SelectSpill();
  void AssignColors();
  void Color();
  void AssignRegisters();
  void AssignTemps(temp::TempList *temps);
  void RemoveMoves();

  bool MeaninglessMove(temp::TempList *src, temp::TempList *dst);

  void RewriteProgram();
};

} // namespace ra

#endif