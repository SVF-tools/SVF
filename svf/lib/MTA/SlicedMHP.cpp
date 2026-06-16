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
#include <chrono>
#include <iomanip>
#include <iostream>

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

// Most methods are inherited from base class, only override ICFG traversal methods
const ICFGNode* SlicedForkJoinAnalysis::getFunEntry(const FunObjVar* fun) const
{
    // Use sliced view if available, otherwise fall back to original entry
    if (icfgView != nullptr)
    {
        // For sliced view, return the first kept node in the function's entry block
        // or the original entry if not kept
        const ICFGNode* entry = fun->getEntryBlock()->front();
        if (icfgView->isKeptNode(entry))
            return entry;
        // If entry is not kept, find first kept node in entry block
        for (const ICFGNode* node : fun->getEntryBlock()->getICFGNodeList())
        {
            if (icfgView->isKeptNode(node))
                return node;
        }
        // If no kept node found, return original entry anyway
        return entry;
    }
    else
    {
        return fun->getEntryBlock()->front();
    }
}

void SlicedForkJoinAnalysis::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    if (icfgView != nullptr)
    {
        icfgView->getSuccNodes(node, out);
    }
    else
    {
        // Fall back to full ICFG
        out.clear();
        for (const ICFGEdge* e : node->getOutEdges())
            out.push_back(e->getDstNode());
    }
}

bool SlicedForkJoinAnalysis::acceptsNode(const ICFGNode* node) const
{
    if (icfgView != nullptr)
    {
        return icfgView->isKeptNode(node);
    }
    else
    {
        // Accept all nodes in full ICFG
        return true;
    }
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

void SlicedMHP::handleIntra(const CxtThreadStmt& cts)
{
    stats.handleIntraCalls++;
    // Track visited ICFG nodes
    const ICFGNode* node = cts.getStmt();
    if (visitedNodes.insert(node).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
    std::vector<const ICFGNode*> succ;
    if (icfgView != nullptr)
    {
        icfgView->getSuccNodes(cts.getStmt(), succ);
    }
    else
    {
        for (const ICFGEdge* e : cts.getStmt()->getOutEdges())
            succ.push_back(e->getDstNode());
    }
    for (const ICFGNode* dst : succ)
    {
        if (dst->getFun() == cts.getStmt()->getFun())
        {
            CxtThreadStmt newCts(cts.getTid(), cts.getContext(), dst);
            // Use base class method if available, otherwise keep current implementation
            addInterleavingThread(newCts, cts);
        }
    }
}

void SlicedMHP::handleNonCandidateFun(const CxtThreadStmt& cts)
{
    stats.handleNonCandidateCalls++;
    // Track visited ICFG nodes
    const ICFGNode* curInst = cts.getStmt();
    if (visitedNodes.insert(curInst).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
    const FunObjVar* curfun = curInst->getFun();

    const CallStrCxt& curCxt = cts.getContext();
    CallGraphNode* cgNode = getThreadCallGraph()->getCallGraphNode(curfun);
    for (auto nit = cgNode->OutEdgeBegin(), neit = cgNode->OutEdgeEnd(); nit != neit; ++nit)
    {
        const FunObjVar* callee = (*nit)->getDstNode()->getFunction();
        if (!isExtCall(callee))
        {
            const ICFGNode* calleeInst = getFunEntry(callee);
            CxtThreadStmt newCts(cts.getTid(), curCxt, calleeInst);
            addInterleavingThread(newCts, cts);
        }
    }
}

void SlicedMHP::handleFork(const CxtThreadStmt& cts, NodeID rootTid)
{
    stats.handleForkCalls++;
    // Track visited ICFG nodes
    const ICFGNode* node = cts.getStmt();
    if (visitedNodes.insert(node).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
    const ICFGNode* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    assert(isTDFork(call));
    const CallICFGNode* cbn = cast<CallICFGNode>(call);
    if (getTCT()->getThreadCallGraph()->hasCallGraphEdge(cbn))
    {
        for (auto cgIt = getThreadCallGraph()->getForkEdgeBegin(cbn), ecgIt = getThreadCallGraph()->getForkEdgeEnd(cbn); cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfroutine = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, cbn, svfroutine);
            const ICFGNode* stmt = getFunEntry(svfroutine);
            CxtThread ct(newCxt, call);
            CxtThreadStmt newcts(getTCT()->getTCTNode(ct)->getId(), ct.getContext(), stmt);
            addInterleavingThread(newcts, cts);
        }
    }
    handleIntra(cts);
}

void SlicedMHP::handleJoin(const CxtThreadStmt& cts, NodeID rootTid)
{
    stats.handleJoinCalls++;
    // Track visited ICFG nodes
    const ICFGNode* node = cts.getStmt();
    if (visitedNodes.insert(node).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
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

void SlicedMHP::handleCall(const CxtThreadStmt& cts, NodeID)
{
    stats.handleCallCalls++;
    // Track visited ICFG nodes
    const ICFGNode* node = cts.getStmt();
    if (visitedNodes.insert(node).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
    const ICFGNode* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* cbn = cast<CallICFGNode>(call);
    if (getTCT()->getThreadCallGraph()->hasCallGraphEdge(cbn))
    {
        for (auto cgIt = getThreadCallGraph()->getCallEdgeBegin(cbn), ecgIt = getThreadCallGraph()->getCallEdgeEnd(cbn); cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfcallee = (*cgIt)->getDstNode()->getFunction();
            if (isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, SVFUtil::cast<CallICFGNode>(call), svfcallee);
            const ICFGNode* svfEntryInst = getFunEntry(svfcallee);
            CxtThreadStmt newCts(cts.getTid(), newCxt, svfEntryInst);
            addInterleavingThread(newCts, cts);
        }
    }

    // propagate to return site only if callee is non-candidate
    if (const CallICFGNode* callSite = SVFUtil::cast<CallICFGNode>(call))
    {
        CallGraph::FunctionSet callees;
        if (!getTCT()->isCandidateFun(getCallee(callSite, callees)))
        {
            CxtThreadStmt newCts(cts.getTid(), cts.getContext(), callSite->getRetICFGNode());
            addInterleavingThread(newCts, cts);
        }
    }
    else
        assert(false && "cts.getStmt() is not a CallICFGNode!");
}

void SlicedMHP::handleRet(const CxtThreadStmt& cts)
{
    stats.handleRetCalls++;
    // Track visited ICFG nodes
    const ICFGNode* node = cts.getStmt();
    if (visitedNodes.insert(node).second)
    {
        stats.uniqueVisitedICFGNodes++;
    }
    
    CallGraphNode* curFunNode = getThreadCallGraph()->getCallGraphNode(cts.getStmt()->getFun());
    std::vector<const CallGraphEdge*> inEdges;
    getInEdgesOfCallGraphNode(curFunNode, inEdges);
    for (const CallGraphEdge* edgeConst : inEdges)
    {
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edgeConst))
            continue;
        
        // Need non-const for directCallsBegin/End
        CallGraphEdge* edge = const_cast<CallGraphEdge*>(edgeConst);
        for (auto cit = edge->directCallsBegin(), ecit = edge->directCallsEnd(); cit != ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if (matchAndPopCxt(newCxt, *cit, curFunNode->getFunction()))
            {
                std::vector<const ICFGNode*> succ;
                getSuccNodes(cts.getStmt(), succ);
                for (const ICFGNode* dst : succ)
                {
                    if (dst->getFun() != (*cit)->getFun())
                        continue;
                    if (!hasThreadStmtSet(*cit))
                        continue;
                    for (const CxtThreadStmt& cxtThreadStmt : getThreadStmtSet(*cit))
                    {
                        CallStrCxt callSiteCxt = cxtThreadStmt.getContext();
                        if (isContextSuffix(newCxt, callSiteCxt))
                        {
                            CxtThreadStmt newCts(cts.getTid(), callSiteCxt, dst);
                            addInterleavingThread(newCts, cts);
                        }
                    }
                }
            }
        }
        for (auto cit = edge->indirectCallsBegin(), ecit = edge->indirectCallsEnd(); cit != ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if (matchAndPopCxt(newCxt, *cit, curFunNode->getFunction()))
            {
                std::vector<const ICFGNode*> succ;
                getSuccNodes(cts.getStmt(), succ);
                for (const ICFGNode* dst : succ)
                {
                    if (dst->getFun() != (*cit)->getFun())
                        continue;
                    if (!hasThreadStmtSet(*cit))
                        continue;
                    for (const CxtThreadStmt& cxtThreadStmt : getThreadStmtSet(*cit))
                    {
                        CallStrCxt callSiteCxt = cxtThreadStmt.getContext();
                        if (isContextSuffix(newCxt, callSiteCxt))
                        {
                            CxtThreadStmt newCts(cts.getTid(), callSiteCxt, dst);
                            addInterleavingThread(newCts, cts);
                        }
                    }
                }
            }
        }
    }
}

// These methods can use base class implementation if they call virtual methods
// The virtual methods (getSuccNodes, getFunEntry) will be called correctly

void SlicedMHP::updateNonCandidateFunInterleaving()
{
    // 开始时间统计
    auto startTime = std::chrono::steady_clock::now();
    
    // 循环计数器统计
    size_t totalFunctions = 0;
    size_t processedFunctions = 0;
    size_t skippedFunctions = 0;
    size_t totalCxtThreadStmts = 0;
    size_t totalICFGNodes = 0;
    size_t totalKeptICFGNodes = 0;  // Total kept ICFG nodes across all processed functions
    size_t totalAddInterleavingThreadCalls = 0;
    
    // Use sliced CallGraph view if available, otherwise fall back to original
    const CallGraph* cg = nullptr;
    if (slicedView != nullptr) {
        // Prefer ThreadCallGraph, fall back to CallGraph if not available
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
        totalFunctions++;
        const FunObjVar* fun = item.second->getFunction();
        if (!getTCT()->isCandidateFun(fun) && !isExtCall(fun))
        {
            const ICFGNode* entryNode = getFunEntry(fun);
            if (!hasThreadStmtSet(entryNode))
            {
                skippedFunctions++;
                continue;
            }

            processedFunctions++;
            const MHP::CxtThreadStmtSet& tsSet = getThreadStmtSet(entryNode);
            
            // Get all kept ICFG nodes for this function
            std::vector<const ICFGNode*> funICFGNodes;
            getFunICFGNodes(fun, funICFGNodes);
            totalKeptICFGNodes += funICFGNodes.size();
            
            for (const CxtThreadStmt& cts : tsSet)
            {
                totalCxtThreadStmts++;
                const CallStrCxt& curCxt = cts.getContext();
                
                // Iterate over kept ICFG nodes instead of all nodes
                for (const ICFGNode* curNode : funICFGNodes)
                {
                    totalICFGNodes++;
                    if (curNode == entryNode)
                        continue;
                    CxtThreadStmt newCts(cts.getTid(), curCxt, curNode);
                    // Note: These operations may need to access base class protected members
                    // If base class doesn't expose these, we may need to keep our own implementation
                    // For now, we'll assume base class has these methods or protected members
                    addInterleavingThread(newCts, cts);
                    totalAddInterleavingThreadCalls++;
                }
            }
        }
        else
        {
            skippedFunctions++;
        }
    }
    
    // 结束时间统计
    auto endTime = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(endTime - startTime).count();
    
    // 输出性能统计信息
    std::cout << "[SlicedMHP::updateNonCandidateFunInterleaving] Performance Statistics:\n";
    std::cout << "  - Total time: " << std::fixed << std::setprecision(2) << elapsedMs << " ms";
    if (elapsedMs >= 1000.0) {
        std::cout << " (" << std::fixed << std::setprecision(2) << (elapsedMs / 1000.0) << " s)";
    }
    std::cout << "\n";
    std::cout << "  - Total functions in CallGraph: " << totalFunctions << "\n";
    std::cout << "  - Processed functions (non-candidate, non-ext, with thread stmt set): " << processedFunctions << "\n";
    std::cout << "  - Skipped functions: " << skippedFunctions << "\n";
    std::cout << "  - Total CxtThreadStmt iterations: " << totalCxtThreadStmts << "\n";
    std::cout << "  - Total kept ICFG nodes (across all processed functions): " << totalKeptICFGNodes << "\n";
    std::cout << "  - Total ICFGNode iterations (CxtThreadStmt × kept nodes): " << totalICFGNodes << "\n";
    std::cout << "  - Total addInterleavingThread() calls: " << totalAddInterleavingThreadCalls << "\n";
    if (processedFunctions > 0 && totalCxtThreadStmts > 0) {
        std::cout << "  - Average CxtThreadStmt per processed function: " 
                  << std::fixed << std::setprecision(2) 
                  << (static_cast<double>(totalCxtThreadStmts) / processedFunctions) << "\n";
    }
    if (processedFunctions > 0 && totalKeptICFGNodes > 0) {
        std::cout << "  - Average kept ICFG nodes per processed function: " 
                  << std::fixed << std::setprecision(2) 
                  << (static_cast<double>(totalKeptICFGNodes) / processedFunctions) << "\n";
    }
    if (totalCxtThreadStmts > 0 && totalICFGNodes > 0) {
        std::cout << "  - Average kept ICFG nodes per CxtThreadStmt: " 
                  << std::fixed << std::setprecision(2) 
                  << (static_cast<double>(totalICFGNodes) / totalCxtThreadStmts) << "\n";
    }
    std::cout << std::endl;
}

// Override analyzeInterleaving to track statistics
void SlicedMHP::analyzeInterleaving()
{
    // Clear visited nodes set for this analysis run
    visitedNodes.clear();
    stats.worklistPops = 0;
    stats.uniqueVisitedICFGNodes = 0;
    stats.seedEntryNotKept = 0;

    for (const std::pair<const NodeID, TCTNode*>& tpair : *getTCT())
    {
        const CxtThread& ct = tpair.second->getCxtThread();
        NodeID rootTid = tpair.first;
        const FunObjVar* routine = getTCT()->getStartRoutineOfCxtThread(ct);
        const ICFGNode* svfInst = getFunEntry(routine);
        
        // Check if seed entry is not kept in sliced view
        if (icfgView != nullptr && !icfgView->isKeptNode(routine->getEntryBlock()->front()))
        {
            stats.seedEntryNotKept++;
        }
        
        CxtThreadStmt rootcts(rootTid, ct.getContext(), svfInst);

        addInterleavingThread(rootcts, rootTid);
        updateAncestorThreads(rootTid);
        updateSiblingThreads(rootTid);

        while (!cxtStmtList.empty())
        {
            CxtThreadStmt cts = popFromCTSWorkList();
            stats.worklistPops++;
            const ICFGNode* curInst = cts.getStmt();

            DBOUT(DMTA, outs() << "-----\nMHP analysis root thread: " << rootTid << " ");
            DBOUT(DMTA, cts.dump());
            DBOUT(DMTA, outs() << "current thread interleaving: < ");
            DBOUT(DMTA, dumpSet(getInterleavingThreads(cts)));
            DBOUT(DMTA, outs() << " >\n-----\n");

            /// handle non-candidate function
            if (!getTCT()->isCandidateFun(curInst->getFun()))
            {
                handleNonCandidateFun(cts);
            }
            /// handle candidate function
            else
            {
                if (isTDFork(curInst))
                {
                    handleFork(cts, rootTid);
                }
                else if (isTDJoin(curInst))
                {
                    handleJoin(cts, rootTid);
                }
                else if (getTCT()->isCallSite(curInst) && !getTCT()->isExtCall(curInst))
                {
                    handleCall(cts, rootTid);
                }
                else if (SVFUtil::dyn_cast<FunExitICFGNode>(curInst))
                {
                    handleRet(cts);
                }
                else
                {
                    handleIntra(cts);
                }
            }
        }
    }

    /// update non-candidate functions' interleaving
    updateNonCandidateFunInterleaving();

    if (Options::PrintInterLev())
        printInterleaving();
}

// These methods can use base class implementation

void SlicedMHP::getInEdgesOfCallGraphNode(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    
    // Use sliced view if available (prefer TCG)
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr)
    {
        // Use SlicedThreadCallGraphView
        slicedView->getThreadCallGraph()->getInEdgesOf(node, out);
    }
    else if (slicedView != nullptr && slicedView->getCallGraph() != nullptr)
    {
        // Use SlicedCallGraphView
        slicedView->getCallGraph()->getInEdgesOf(node, out);
    }
    else
    {
        // Fall back to original CallGraphNode
        for (CallGraphEdge* edge : node->getInEdges())
        {
            out.push_back(edge);
        }
    }
}

const ICFGNode* SlicedMHP::getFunEntry(const FunObjVar* fun) const
{
    // Use sliced view if available, otherwise fall back to original entry
    if (icfgView != nullptr)
    {
        // For sliced view, return the first kept node in the function's entry block
        // or the original entry if not kept
        const ICFGNode* entry = fun->getEntryBlock()->front();
        if (icfgView->isKeptNode(entry))
            return entry;
        // If entry is not kept, find first kept node in entry block
        for (const ICFGNode* node : fun->getEntryBlock()->getICFGNodeList())
        {
            if (icfgView->isKeptNode(node))
                return node;
        }
        // If no kept node found, return original entry anyway
        return entry;
    }
    else
    {
        return fun->getEntryBlock()->front();
    }
}

void SlicedMHP::getFunICFGNodes(const FunObjVar* fun, std::vector<const ICFGNode*>& out) const
{
    out.clear();
    
    if (icfgView != nullptr)
    {
        // Use sliced view: only return kept nodes
        for (auto it : *fun)
        {
            const SVFBasicBlock* svfbb = it.second;
            for (const ICFGNode* node : svfbb->getICFGNodeList())
            {
                if (icfgView->isKeptNode(node))
                {
                    out.push_back(node);
                }
            }
        }
    }
    else
    {
        // No sliced view: return all nodes
        for (auto it : *fun)
        {
            const SVFBasicBlock* svfbb = it.second;
            for (const ICFGNode* node : svfbb->getICFGNodeList())
            {
                out.push_back(node);
            }
        }
    }
}

void SlicedMHP::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    if (icfgView != nullptr)
    {
        icfgView->getSuccNodes(node, out);
    }
    else
    {
        // Fall back to full ICFG
        out.clear();
        for (const ICFGEdge* e : node->getOutEdges())
            out.push_back(e->getDstNode());
    }
}

bool SlicedMHP::acceptsNode(const ICFGNode* node) const
{
    if (icfgView != nullptr)
    {
        return icfgView->isKeptNode(node);
    }
    else
    {
        return true; // Accept all nodes in full ICFG
    }
}

void SlicedMHP::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    // Handle calling context for candidate functions only
    if (getTCT()->isCandidateFun(call->getFun()) == false)
        return;
    
    // If TCT is SlicedTCT, use its pushCxt method which supports custom maxContextLen
    // Otherwise, use base class TCT::pushCxt
    // Use dynamic_cast instead of dyn_cast since SlicedTCT is a concrete class
    if (SlicedTCT* slicedTCT = dynamic_cast<SlicedTCT*>(getTCT()))
    {
        slicedTCT->pushCxt(cxt, call, callee);
    }
    else
    {
        getTCT()->pushCxt(cxt, call, callee);
    }
}

