#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"

#include <map>

namespace live {

class MoveList;

using INode = graph::Node<temp::Temp>;
using INodePtr = graph::Node<temp::Temp> *;
using INodeList = graph::NodeList<temp::Temp>;
using INodeListPtr = graph::NodeList<temp::Temp> *;
using IGraph = graph::Graph<temp::Temp>;
using IGraphPtr = graph::Graph<temp::Temp> *;

using IMap = std::map<graph::Node<assem::Instr> *, temp::TempList *>;

class MoveList {
public:
  MoveList() = default;
  MoveList(std::pair<INodePtr, INodePtr> p) : move_list_({p}) {}
  [[nodiscard]] const std::list<std::pair<INodePtr, INodePtr>> &
  GetList() const {
    return move_list_;
  }
  void Append(INodePtr src, INodePtr dst) { move_list_.emplace_back(src, dst); }
  bool Contain(INodePtr src, INodePtr dst);
  void Delete(INodePtr src, INodePtr dst);
  void Prepend(INodePtr src, INodePtr dst) {
    move_list_.emplace_front(src, dst);
  }
  bool Empty() { return move_list_.empty(); }
  MoveList *Union(MoveList *list);
  MoveList *Intersect(MoveList *list);
  MoveList *Diff(MoveList *list);

  std::pair<INodePtr, INodePtr> Front() { return move_list_.front(); }
  std::pair<INodePtr, INodePtr> Pop() {
    auto tmp = move_list_.front();
    move_list_.pop_front();
    return tmp;
  }

private:
  std::list<std::pair<INodePtr, INodePtr>> move_list_;
};

struct LiveGraph {
  IGraphPtr interf_graph;
  MoveList *moves;
  graph::Table<temp::Temp, MoveList> *move_list;

  LiveGraph(IGraphPtr interf_graph, MoveList *moves,
            graph::Table<temp::Temp, MoveList> *move_list)
      : interf_graph(interf_graph), moves(moves), move_list(move_list) {}
};

class LiveGraphFactory {
public:
  explicit LiveGraphFactory(fg::FGraphPtr flowgraph)
      : flowgraph_(flowgraph),
        live_graph_(new IGraph(), new MoveList(),
                    new graph::Table<temp::Temp, MoveList>()),
        in_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        out_(std::make_unique<graph::Table<assem::Instr, temp::TempList>>()),
        temp_node_map_(new tab::Table<temp::Temp, INode>()) {}
  void Liveness();
  LiveGraph GetLiveGraph() { return live_graph_; }
  MoveList *getMoveList(INodePtr node);
  tab::Table<temp::Temp, INode> *GetTempNodeMap() { return temp_node_map_; }

private:
  fg::FGraphPtr flowgraph_;
  LiveGraph live_graph_;

  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> in_;
  std::unique_ptr<graph::Table<assem::Instr, temp::TempList>> out_;
  tab::Table<temp::Temp, INode> *temp_node_map_;

  bool Equal(temp::TempList *a, temp::TempList *b);
  bool Equal(IMap a, IMap b);
  temp::TempList *Union(temp::TempList *a, temp::TempList *b);
  temp::TempList *Diff(temp::TempList *a, temp::TempList *b);

  INodePtr GetNode(temp::Temp *temp);
  void LiveMap();
  void InterfGraph();

  void init();
  void AddMoveList(graph::Table<temp::Temp, MoveList> *move_list, INodePtr node,
                   INodePtr src, INodePtr dst);

  //  IMap in, out;
};

} // namespace live

#endif