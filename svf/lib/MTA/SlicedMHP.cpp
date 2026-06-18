// MSli: MHP running on sliced ICFG view.
// Author: Jiawei Yang

#include "MTA/SlicedMHP.h"

#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFVariables.h>  // For ValVar complete definition
#include <WPA/Andersen.h>  // For PointerAnalysis complete definition
#include <Util/SVFUtil.h>
#include <Util/WorkList.h>
#include <Util/Options.h>
#include "MTA/SlicedView.h"

using namespace SVF;
using namespace SVFUtil;

namespace SVF
{

SlicedMHP::SlicedMHP(TCT* t, const SlicedSVFIRView* sv)
    : MHP(t), slicedView(sv)
{
    // ICFG view from slicedView if available, otherwise nullptr (full ICFG).
    icfgView = (slicedView != nullptr) ? slicedView->getICFG() : nullptr;
}

SlicedMHP::~SlicedMHP() = default;

// All MHP handlers (including handleJoin, which reads the base ForkJoinAnalysis)
// are inherited; only the traversal hooks below and updateNonCandidateFunInterleaving
// are overridden to walk the slice. Fork/join relationships are program-global, so
// the base full-graph ForkJoinAnalysis is exactly what handleJoin needs.

void SlicedMHP::updateNonCandidateFunInterleaving()
{
    // Use the sliced ThreadCallGraph view if available, else the full CallGraph.
    const CallGraph* cg = nullptr;
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr) {
        cg = slicedView->getThreadCallGraph()->getCallGraph();
    }
    if (cg == nullptr) {
        cg = PAG::getPAG()->getCallGraph();
    }

    for (const auto& item : *cg)
    {
        const FunObjVar* fun = item.second->getFunction();
        if (getTCT()->isCandidateFun(fun) || isExtCall(fun))
            continue;

        const ICFGNode* entryNode = getFunEntry(fun);
        if (!hasThreadStmtSet(entryNode))
            continue;

        const MHP::CxtThreadStmtSet& tsSet = getThreadStmtSet(entryNode);

        // Iterate over kept ICFG nodes only, instead of all nodes.
        std::vector<const ICFGNode*> funICFGNodes;
        getFunICFGNodes(fun, funICFGNodes);

        for (const CxtThreadStmt& cts : tsSet)
        {
            const CallStrCxt& curCxt = cts.getContext();
            for (const ICFGNode* curNode : funICFGNodes)
            {
                if (curNode == entryNode)
                    continue;
                CxtThreadStmt newCts(cts.getTid(), curCxt, curNode);
                addInterleavingThread(newCts, cts);
            }
        }
    }
}

void SlicedMHP::getInEdgesOfCallGraphNode(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    SlicedViewAdapter::getInEdgesOfCallGraphNode(slicedView, node, out);
}

const ICFGNode* SlicedMHP::getFunEntry(const FunObjVar* fun) const
{
    return SlicedViewAdapter::getFunEntry(icfgView, fun);
}

void SlicedMHP::getFunICFGNodes(const FunObjVar* fun, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getFunICFGNodes(icfgView, fun, out);
}

void SlicedMHP::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getSuccNodes(icfgView, node, out);
}

} // namespace SVF

