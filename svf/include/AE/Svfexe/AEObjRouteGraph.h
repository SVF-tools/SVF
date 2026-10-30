//===- AEObjRouteGraph.h - Static base/sub value-flow routes ---------------===//

#ifndef INCLUDE_AE_SVFEXE_AEOBJROUTEGRAPH_H_
#define INCLUDE_AE_SVFEXE_AEOBJROUTEGRAPH_H_

#include <iosfwd>
#include <memory>
#include <vector>

#include "Graphs/ICFG.h"
#include "Graphs/SVFG.h"

namespace SVF
{

class PointerAnalysis;
class SVFIR;

/// Prebuilt, value-independent flow for each base and its sub-Objs.
/// Original SVFG nodes at a site share one Obj-group node. A branch exit is
/// a synthetic node between its defining group and the next group use.
class AEObjRouteGraph
{
public:
    struct Node
    {
        NodeID id;
        NodeID base;
        const ICFGNode* location;
        const IntraCFGEdge* branch;
        std::vector<const VFGNode*> originals;

        const ICFGNode* getICFGNode() const { return location; }
    };

    struct Edge
    {
        const Node* source;
        const Node* target;
    };

    AEObjRouteGraph(const SVFG& svfg, ICFG& icfg,
                    PointerAnalysis& pta, const SVFIR& svfir);

    const std::vector<Edge>& getInputs(const ICFGNode* node) const;
    void dump(std::ostream& out) const;
    void dumpDot(std::ostream& out) const;

private:
    bool isBranch(const ICFGNode* node) const;
    Node* getGroupNode(NodeID base, const ICFGNode* location);
    Node* getBranchNode(NodeID base, const IntraCFGEdge* branch);
    void recordOriginal(NodeID base, const ICFGNode* location,
                        const VFGNode* original);
    void addEdge(NodeID base, const ICFGNode* source,
                 const ICFGNode* target, const IntraCFGEdge* branch);
    void connectFrom(NodeID base, const ICFGNode* source,
                     const Set<const ICFGNode*>& events,
                     Set<const ICFGNode*>& newSources);

    ICFG& icfg;
    std::vector<std::unique_ptr<Node>> nodes;
    Map<NodeID, Map<const ICFGNode*, Node*>> groupNodes;
    Map<NodeID, Map<const IntraCFGEdge*, Node*>> branchNodes;
    Map<const ICFGNode*, std::vector<Edge>> inputs;
    Map<const ICFGNode*, Set<NodeID>> returnedGroups;
    Map<const ICFGNode*, Map<const FunObjVar*, Set<NodeID>>> returnedGroupsByCallee;
};

} // namespace SVF

#endif
