//===- SparseObjGraph.h - Static packed Obj value-flow routes -------------===//

#ifndef INCLUDE_GRAPHS_SPARSEOBJGRAPH_H_
#define INCLUDE_GRAPHS_SPARSEOBJGRAPH_H_

#include "Graphs/ICFG.h"

#include <deque>
#include <map>
#include <memory>

namespace SVF
{

class PointerAnalysis;
class SVFIR;
class SVFG;
class SparseObjNode;

typedef GenericEdge<SparseObjNode> GenericSparseObjEdgeTy;

/// An edge carries a set of packed base Objs from the source ICFG location to
/// the target ICFG location. The set (label) is interned: edges routing the
/// same bases share one NodeBS.
class SparseObjEdge : public GenericSparseObjEdgeTy
{
public:
    SparseObjEdge(SparseObjNode* source, SparseObjNode* target,
                  const NodeBS* bases)
        : GenericSparseObjEdgeTy(source, target, 0), bases(bases) {}

    /// Base Obj IDs routed over this edge; each base carries all its fields.
    const NodeBS& getBases() const
    {
        return *bases;
    }
    std::string toString() const;

private:
    friend class SparseObjGraph;
    const NodeBS* bases;
};

typedef GenericNode<SparseObjNode, SparseObjEdge> GenericSparseObjNodeTy;

/// A normal node is one ICFG location, shared by every packed Obj that stops
/// there. A non-null branch marks the node of one outgoing branch edge; its
/// location remains the branch's ICFG node, and AE refines the source state
/// once per branch edge before reading the bases of each outgoing edge.
class SparseObjNode : public GenericSparseObjNodeTy
{
public:
    SparseObjNode(NodeID id, const ICFGNode* location,
                  const IntraCFGEdge* branch)
        : GenericSparseObjNodeTy(id, OtherKd), location(location),
          branch(branch) {}

    const ICFGNode* location;
    const IntraCFGEdge* branch;

    const ICFGNode* getICFGNode() const
    {
        return location;
    }
    std::string toString() const;
};

typedef GenericGraph<SparseObjNode, SparseObjEdge> GenericSparseObjGraphTy;

/// Prebuild Obj routes from SVFG object information and ICFG order.
/// Fields and array elements of the same base are packed on one route:
///   D: a[0] = 1;  S: a[1] = 7;  N: use(a[0]);
/// Program order gives D -> S -> N, rather than two independent packed inputs
/// from D and S. Bases that share a route segment share one labeled edge:
///   S1: *p = 1 (p -> {a,b});  S2: *q = 2 (q -> {a,b})
/// gives one edge S1 -{a,b}-> S2. A branch edge gets one node, but the graph
/// neither computes nor stores intervals; clients check feasibility and
/// refine values.
class SparseObjGraph : public GenericSparseObjGraphTy
{
public:
    typedef SparseObjNode Node;
    typedef SparseObjEdge Edge;

    /// Construction uses static graphs and points-to results, not AE state.
    SparseObjGraph(const SVFG& svfg, ICFG& icfg,
                   PointerAnalysis& pta, const SVFIR& svfir);
    ~SparseObjGraph() override;
    SparseObjGraph(const SparseObjGraph&) = delete;
    SparseObjGraph& operator=(const SparseObjGraph&) = delete;

    /// Return incoming edges at this ICFG location for client-side value flow.
    const std::vector<const Edge*>& getInputs(const ICFGNode* node) const;
    /// Describe the static nodes and edges as text, without AE values.
    std::string toString() const;
    /// Write a DOT graph to <file>.dot through GraphPrinter.
    void dump(const std::string& file);

    const SVFIR& getSVFIR() const
    {
        return svfir;
    }

private:
    typedef std::pair<const ICFGNode*, const IntraCFGEdge*> NodeKey;
    typedef std::pair<NodeKey, NodeKey> EdgeKey;

    bool isBranch(const ICFGNode* node) const;
    bool isControlBoundary(const ICFGNode* node) const;
    /// Record that `bases` travel from source (via branch, if non-null) to
    /// target. Labels accumulate in routeLabels and become edges in finalize.
    void addRoute(const NodeBS& bases, const ICFGNode* source,
                  const ICFGNode* target, const IntraCFGEdge* branch);
    void addRoute(NodeID base, const ICFGNode* source,
                  const ICFGNode* target, const IntraCFGEdge* branch);
    /// Walk one function once, carrying base sets instead of one base.
    void connectFunction(const FunObjVar* fun, const NodeBS& funBases,
                         const Map<const ICFGNode*, NodeBS>& eventsAt);
    /// Queue `bases` to start a route segment at `node`, skipping bases
    /// already processed there.
    void addSource(const ICFGNode* node, const NodeBS& bases);
    /// Route the bases that no callee returns from the call to its return site.
    void connectCallBypass(const CallICFGNode* call, const NodeBS& bases);
    /// Follow ordinary ICFG nodes from `start` until each base reaches its
    /// next event or a control boundary.
    void connectSegment(const FunObjVar* fun, const ICFGNode* source,
                        const ICFGNode* start, const IntraCFGEdge* branch,
                        const NodeBS& bases,
                        const Map<const ICFGNode*, NodeBS>& eventsAt);
    static const NodeBS& getEvents(const Map<const ICFGNode*, NodeBS>& eventsAt,
                                   const ICFGNode* node);
    Node* getOrCreateNode(const ICFGNode* location, const IntraCFGEdge* branch);
    const NodeBS* intern(const NodeBS& bases);
    void finalize();

    ICFG& icfg;
    const SVFIR& svfir;
    Map<NodeKey, Node*> nodes;
    std::map<EdgeKey, NodeBS> routeLabels;
    Map<const ICFGNode*, std::vector<const Edge*>> inputs;
    /// Interned labels, bucketed by hash.
    Map<u64_t, std::vector<std::unique_ptr<NodeBS>>> labels;
    /// Bases with a callee-output to caller-return SVFG edge. The second map
    /// distinguishes callees when deciding whether a multi-target call bypasses.
    Map<const ICFGNode*, NodeBS> returnedGroups;
    Map<const ICFGNode*, Map<const FunObjVar*, NodeBS>> returnedGroupsByCallee;

    /// Worklist of connectFunction for the function being connected.
    Map<const ICFGNode*, NodeBS> doneSources;
    Map<const ICFGNode*, NodeBS> pendingSources;
    std::deque<const ICFGNode*> sourceWorklist;
};

} // namespace SVF

namespace SVF
{

template<>
struct GenericGraphTraits<SparseObjNode*>
    : public GenericGraphTraits<GenericNode<SparseObjNode, SparseObjEdge>*>
{
};

template<>
struct GenericGraphTraits<Inverse<SparseObjNode*>>
    : public GenericGraphTraits<Inverse<GenericNode<SparseObjNode, SparseObjEdge>*>>
{
};

template<>
struct GenericGraphTraits<SparseObjGraph*>
    : public GenericGraphTraits<GenericGraph<SparseObjNode, SparseObjEdge>*>
{
    typedef SparseObjNode* NodeRef;
};

} // namespace SVF

#endif
