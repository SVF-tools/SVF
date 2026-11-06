//===- SparseObjGraph.h - Static packed Obj value-flow routes -------------===//

#ifndef INCLUDE_GRAPHS_SPARSEOBJGRAPH_H_
#define INCLUDE_GRAPHS_SPARSEOBJGRAPH_H_

#include "Graphs/ICFG.h"

namespace SVF
{

class PointerAnalysis;
class SVFIR;
class SVFG;
class SparseObjNode;

typedef GenericEdge<SparseObjNode> GenericSparseObjEdgeTy;

/// An edge carries the source packed Obj to the target ICFG location.
class SparseObjEdge : public GenericSparseObjEdgeTy
{
public:
    SparseObjEdge(SparseObjNode* source, SparseObjNode* target)
        : GenericSparseObjEdgeTy(source, target, 0) {}

    std::string toString() const;
};

typedef GenericNode<SparseObjNode, SparseObjEdge> GenericSparseObjNodeTy;

/// A normal node represents a packed Obj at one ICFG location.
/// A non-null branch marks a synthetic node for an outgoing branch edge;
/// its location remains the branch's ICFG node.
class SparseObjNode : public GenericSparseObjNodeTy
{
public:
    SparseObjNode(NodeID id, NodeID base, const ICFGNode* location,
                  const IntraCFGEdge* branch)
        : GenericSparseObjNodeTy(id, OtherKd), base(base), location(location),
          branch(branch) {}

    NodeID base;
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
/// from D and S. A branch edge gets a synthetic node, but the graph neither
/// computes nor stores intervals; clients check feasibility and refine values.
class SparseObjGraph : public GenericSparseObjGraphTy
{
public:
    typedef SparseObjNode Node;
    typedef SparseObjEdge Edge;

    /// Construction uses static graphs and points-to results, not AE state.
    SparseObjGraph(const SVFG& svfg, ICFG& icfg,
                   PointerAnalysis& pta, const SVFIR& svfir);
    SparseObjGraph(const SparseObjGraph&) = delete;
    SparseObjGraph& operator=(const SparseObjGraph&) = delete;

    /// Return incoming edges at this ICFG location for client-side value flow.
    const std::vector<const Edge*>& getInputs(const ICFGNode* node) const;
    /// Describe the static nodes and edges as text, without AE values.
    std::string toString() const;
    /// Write a DOT graph to <file>.dot through GraphPrinter.
    void dump(const std::string& file);

private:
    bool isBranch(const ICFGNode* node) const;
    Node* getOrCreateBaseObjNode(NodeID baseObjId, const ICFGNode* location);
    Node* getBranchNode(NodeID base, const IntraCFGEdge* branch);
    void addRouteEdge(Node* source, Node* target);
    void addEdge(NodeID base, const ICFGNode* source,
                 const ICFGNode* target, const IntraCFGEdge* branch);
    void connectFrom(NodeID base, const ICFGNode* source,
                     const Set<const ICFGNode*>& events,
                     Set<const ICFGNode*>& newSources);

    ICFG& icfg;
    /// Normal nodes are unique by (base, ICFG location); branch nodes by edge.
    Map<NodeID, Map<const ICFGNode*, Node*>> groupNodes;
    Map<NodeID, Map<const IntraCFGEdge*, Node*>> branchNodes;
    Map<const ICFGNode*, std::vector<const Edge*>> inputs;
    /// Bases with a callee-output to caller-return SVFG edge. The second map
    /// distinguishes callees when deciding whether a multi-target call bypasses.
    Map<const ICFGNode*, Set<NodeID>> returnedGroups;
    Map<const ICFGNode*, Map<const FunObjVar*, Set<NodeID>>> returnedGroupsByCallee;
};

} // namespace SVF

namespace SVF
{

template<>
struct GenericGraphTraits<SparseObjNode*>
    :
    public GenericGraphTraits<GenericNode<SparseObjNode, SparseObjEdge>*>
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
