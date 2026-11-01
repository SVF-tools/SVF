//===- AEObjRouteGraph.h - Static base/sub value-flow routes ---------------===//

#ifndef INCLUDE_AE_SVFEXE_AEOBJROUTEGRAPH_H_
#define INCLUDE_AE_SVFEXE_AEOBJROUTEGRAPH_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "Graphs/GenericGraph.h"
#include "Graphs/ICFG.h"
#include "Graphs/SVFG.h"

namespace SVF
{

class PointerAnalysis;
class SVFIR;
class AEObjRouteNode;

typedef GenericEdge<AEObjRouteNode> GenericAEObjRouteEdgeTy;

/// An edge carries the source Obj group to the target ICFG location.
class AEObjRouteEdge : public GenericAEObjRouteEdgeTy
{
public:
    AEObjRouteEdge(AEObjRouteNode* source, AEObjRouteNode* target)
        : GenericAEObjRouteEdgeTy(source, target, 0) {}

    std::string toString() const;
};

typedef GenericNode<AEObjRouteNode, AEObjRouteEdge> GenericAEObjRouteNodeTy;

/// A normal node groups a base Obj and its sub-Objs at one ICFG location.
/// A non-null branch marks a synthetic node for an outgoing branch edge;
/// its location remains the branch's ICFG node. Originals identify the
/// contributing SVFG nodes for debugging, not another abstract value.
class AEObjRouteNode : public GenericAEObjRouteNodeTy
{
public:
    AEObjRouteNode(NodeID id, NodeID base, const ICFGNode* location,
                   const IntraCFGEdge* branch)
        : GenericAEObjRouteNodeTy(id, OtherKd), base(base), location(location),
          branch(branch) {}

    NodeID base;
    const ICFGNode* location;
    const IntraCFGEdge* branch;
    std::vector<const VFGNode*> originals;

    const ICFGNode* getICFGNode() const { return location; }
    std::string toString() const;
};

typedef GenericGraph<AEObjRouteNode, AEObjRouteEdge> GenericAEObjRouteGraphTy;

/// Prebuild Obj routes from SVFG object information and ICFG order before AE.
/// Fields and array elements of the same base share a route:
///   D: a[0] = 1;  S: a[1] = 7;  N: use(a[0]);
/// Program order gives D -> S -> N, rather than two independent group inputs
/// from D and S. A branch edge gets a synthetic node, but the graph neither
/// computes nor stores intervals; AE checks feasibility and refines values.
class AEObjRouteGraph : public GenericAEObjRouteGraphTy
{
public:
    typedef AEObjRouteNode Node;
    typedef AEObjRouteEdge Edge;

    /// Construction uses static graphs and points-to results, not AE state.
    AEObjRouteGraph(const SVFG& svfg, ICFG& icfg,
                    PointerAnalysis& pta, const SVFIR& svfir);
    AEObjRouteGraph(const AEObjRouteGraph&) = delete;
    AEObjRouteGraph& operator=(const AEObjRouteGraph&) = delete;

    /// Return the incoming edges at this ICFG location. AE reads the source
    /// base+sub values and checks its branch edge when present.
    const std::vector<const Edge*>& getInputs(const ICFGNode* node) const;
    /// Describe the static nodes and edges as text, without AE values.
    std::string toString() const;
    /// Write a DOT graph to <file>.dot, like SVFG::dump.
    void dump(const std::string& file) const;

private:
    bool isBranch(const ICFGNode* node) const;
    Node* getOrCreateBaseObjNode(NodeID baseObjId, const ICFGNode* location);
    Node* getBranchNode(NodeID base, const IntraCFGEdge* branch);
    void addRouteEdge(Node* source, Node* target);
    void recordOriginal(NodeID base, const ICFGNode* location,
                        const VFGNode* original);
    void addEdge(NodeID base, const ICFGNode* source,
                 const ICFGNode* target, const IntraCFGEdge* branch);
    void connectFrom(NodeID base, const ICFGNode* source,
                     const Set<const ICFGNode*>& events,
                     Set<const ICFGNode*>& newSources);
    void dumpDot(std::ostream& out) const;

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

#endif
