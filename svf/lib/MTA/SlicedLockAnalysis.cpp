// MSli: LockAnalysis running on a sliced ICFG view.
//
// Inherits SVF::LockAnalysis unchanged; only the ICFG/CallGraph traversal hooks
// are overridden so the inherited analysis routines walk the sliced view. The
// sliced ForkJoin/lock-span algorithms are exactly the base ones reached
// through these hooks (see LockAnalysis.cpp), so nothing else is reimplemented.
// Author: Jiawei Yang

#include "MTA/SlicedLockAnalysis.h"

#include <Graphs/CallGraph.h>
#include <Graphs/ThreadCallGraph.h>
#include <SVFIR/SVFIR.h>
#include "MTA/SlicedView.h"

using namespace SVF;

namespace SVF
{

SlicedLockAnalysis::SlicedLockAnalysis(TCT* t, const SlicedSVFIRView* sv)
    : LockAnalysis(t), slicedView(sv)
{
    // ICFG view from slicedView if available, otherwise nullptr (full ICFG).
    icfgView = (slicedView != nullptr) ? slicedView->getICFG() : nullptr;
}

const ICFGNode* SlicedLockAnalysis::getFunEntry(const FunObjVar* fun) const
{
    return SlicedViewAdapter::getFunEntry(icfgView, fun);
}

void SlicedLockAnalysis::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getSuccNodes(icfgView, node, out);
}

void SlicedLockAnalysis::getPredNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getPredNodes(icfgView, node, out);
}

bool SlicedLockAnalysis::acceptsNode(const ICFGNode* node) const
{
    return SlicedViewAdapter::acceptsNode(icfgView, node);
}

void SlicedLockAnalysis::getInEdgesOfCallGraphNode(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    SlicedViewAdapter::getInEdgesOfCallGraphNode(slicedView, node, out);
}

const CallGraph* SlicedLockAnalysis::getAnalysisCallGraph() const
{
    // Prefer the sliced ThreadCallGraph view; fall back to the full CallGraph.
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr)
        return slicedView->getThreadCallGraph()->getCallGraph();
    return PAG::getPAG()->getCallGraph();
}

} // namespace SVF
