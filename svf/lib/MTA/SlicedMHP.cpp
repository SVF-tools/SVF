// MSli: MHP running on sliced ICFG view.

#include "MTA/SlicedMHP.h"

#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFVariables.h>  // For ValVar complete definition
#include <WPA/Andersen.h>  // For PointerAnalysis complete definition
#include <Util/SVFUtil.h>
#include <Util/WorkList.h>
#include <Util/Options.h>
#include "MTA/SlicedSVFIRView.h"
#include "MTA/SlicedICFGView.h"
#include "MTA/SlicedTCT.h"
#include "MTA/SlicedViewAdapter.h"

using namespace SVF;
using namespace SVFUtil;

//===----------------------------------------------------------------------===//
// SlicedForkJoinAnalysis (inherits from SVF::MHP::ForkJoinAnalysis)
//===----------------------------------------------------------------------===//

SlicedForkJoinAnalysis::SlicedForkJoinAnalysis(TCT* t, const SlicedICFGView* view, const SlicedMHP* p)
    : ForkJoinAnalysis(t), icfgView(view), parent(p)
{
    // SVF version calls collectSCEVInfo(); currently it is effectively a no-op.
}

// Most methods are inherited from base class, only override ICFG traversal
// methods (shared with SlicedMHP/SlicedLockAnalysis via SlicedViewAdapter).
const ICFGNode* SlicedForkJoinAnalysis::getFunEntry(const FunObjVar* fun) const
{
    return SlicedViewAdapter::getFunEntry(icfgView, fun);
}

void SlicedForkJoinAnalysis::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getSuccNodes(icfgView, node, out);
}

bool SlicedForkJoinAnalysis::acceptsNode(const ICFGNode* node) const
{
    return SlicedViewAdapter::acceptsNode(icfgView, node);
}

// Override handlers to use sliced ICFG traversal methods
// Note: These methods are virtual in base class, but they access private members (getTCG, pushCxt, getMarkedFlag, etc.)
// Since we can't access private members, we need to reimplement the logic.
// However, the base class implementation uses getOutEdges() which we can't override.
// The best approach is to override analyzeForkJoinPair() instead, which calls these handle methods.
// For now, we'll provide simplified implementations that call base class where possible.
void SlicedForkJoinAnalysis::handleFork(const CxtStmt& cts, NodeID rootTid)
{
    // Since we can't access private methods, we'll use a workaround:
    // Call base class implementation, but it won't use our sliced view for ICFG traversal
    // This is a limitation until SVF makes these methods protected
    ForkJoinAnalysis::handleFork(cts, rootTid);
}

void SlicedForkJoinAnalysis::handleJoin(const CxtStmt& cts, NodeID rootTid)
{
    // Call base class implementation
    ForkJoinAnalysis::handleJoin(cts, rootTid);
}

void SlicedForkJoinAnalysis::handleCall(const CxtStmt& cts, NodeID rootTid)
{
    // Call base class implementation
    ForkJoinAnalysis::handleCall(cts, rootTid);
}

void SlicedForkJoinAnalysis::handleRet(const CxtStmt& cts)
{
    // Call base class implementation
    ForkJoinAnalysis::handleRet(cts);
}

void SlicedForkJoinAnalysis::handleIntra(const CxtStmt& cts)
{
    // Call base class implementation
    // Note: It will use getOutEdges() which we can't override
    // We need to override analyzeForkJoinPair() instead to use sliced view
    ForkJoinAnalysis::handleIntra(cts);
}

// Note: analyzeForkJoinPair is not virtual in base class, so we can't override it
// We'll need to make it virtual in SVF, or work around by overriding handle methods
// For now, handle methods will call base class implementation

//===----------------------------------------------------------------------===//
// SlicedMHP (inherits from SVF::MHP)
//===----------------------------------------------------------------------===//

SlicedMHP::SlicedMHP(TCT* t, const SlicedSVFIRView* sv)
    : MHP(t), slicedView(sv)
{
    // Get ICFG view from slicedView if available, otherwise nullptr (will use full ICFG)
    if (slicedView != nullptr)
    {
        icfgView = slicedView->getICFG();
    }
    else
    {
        icfgView = nullptr;
    }

    fja = std::make_unique<SlicedForkJoinAnalysis>(t, icfgView, this);
    fja->analyzeForkJoinPair();
}

SlicedMHP::~SlicedMHP() = default;

// Note: analyze() is not virtual in base class, so we don't override it.
// Instead, we call MHP::analyze() directly when needed.

// These methods are inherited from base class, no need to override unless signature differs

// These methods are inherited from base class or can use base class implementation
// Only override if we need sliced view semantics

// handleJoin stays overridden because it queries the sliced ForkJoinAnalysis (fja);
// the rest of the MHP handlers are inherited and reach the slice via the hooks.
void SlicedMHP::handleJoin(const CxtThreadStmt& cts, NodeID rootTid)
{
    const CallStrCxt& curCxt = cts.getContext();
    assert(isTDJoin(cts.getStmt()));
    const CallICFGNode* call = SVFUtil::cast<CallICFGNode>(cts.getStmt());

    NodeBS joinedTids = fja->getDirAndIndJoinedTid(CxtStmt(curCxt, call));
    if (!joinedTids.empty())
    {
        if (fja->hasJoinLoop(call))
        {
            std::vector<const SVFBasicBlock*> exitbbs;
            call->getFun()->getExitBlocksOfLoop(call->getBB(), exitbbs);
            while (!exitbbs.empty())
            {
                const SVFBasicBlock* eb = exitbbs.back();
                exitbbs.pop_back();
                const ICFGNode* svfEntryInst = eb->front();
                CxtThreadStmt newCts(cts.getTid(), curCxt, svfEntryInst);
                addInterleavingThread(newCts, cts);
                if (fja->hasJoinInSymmetricLoop(CxtStmt(curCxt, call)))
                    rmInterleavingThread(newCts, joinedTids, call);
            }
        }
        else
        {
            rmInterleavingThread(cts, joinedTids, call);
        }
    }
    else
    {
        if (fja->hasJoinLoop(call))
        {
            std::vector<const SVFBasicBlock*> exitbbs;
            call->getFun()->getExitBlocksOfLoop(call->getBB(), exitbbs);
            while (!exitbbs.empty())
            {
                const SVFBasicBlock* eb = exitbbs.back();
                exitbbs.pop_back();
                const ICFGNode* svfEntryInst = eb->front();
                CxtThreadStmt newCts(cts.getTid(), cts.getContext(), svfEntryInst);
                addInterleavingThread(newCts, cts);
            }
        }
    }
    handleIntra(cts);
}

void SlicedMHP::updateNonCandidateFunInterleaving()
{
    // Use sliced CallGraph view if available, otherwise fall back to original.
    // Prefer ThreadCallGraph, fall back to CallGraph if not available.
    const CallGraph* cg = nullptr;
    if (slicedView != nullptr) {
        if (slicedView->getThreadCallGraph() != nullptr) {
            cg = slicedView->getThreadCallGraph()->getCallGraph();
        } else if (slicedView->getCallGraph() != nullptr) {
            cg = slicedView->getCallGraph()->getCallGraph();
        }
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

