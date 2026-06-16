// MSli: LockAnalysis running on sliced ICFG view.

#include "MTA/SlicedLockAnalysis.h"

#include <Graphs/CallGraph.h>
#include <Graphs/ThreadCallGraph.h>
#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFVariables.h>
#include <WPA/Andersen.h>  // For PointerAnalysis complete definition
#include <Util/SVFUtil.h>
#include <Util/PTAStat.h>
#include <Util/WorkList.h>
#include "MTA/SlicedSVFIRView.h"
#include "MTA/SlicedICFGView.h"
#include "MTA/SlicedTCT.h"

using namespace SVF;
using namespace SVFUtil;

SlicedLockAnalysis::SlicedLockAnalysis(TCT* t, const SlicedSVFIRView* sv) 
    : LockAnalysis(t), slicedView(sv)
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
}

void SlicedLockAnalysis::analyze()
{
    stats = Stats{};
    
    // Note: collectLockUnlocksites, buildCandidateFuncSetforLock, analyzeIntraProcedualLock
    // are not virtual in base class, so we need to call our own versions explicitly
    collectLockUnlocksites();
    buildCandidateFuncSetforLock();

    DOTIMESTAT(double lockStart = PTAStat::getClk(true));

    DBOUT(DGENERAL, outs() << "\tIntra-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tIntra-procedural LockAnalysis\n");
    analyzeIntraProcedualLock();

    DBOUT(DGENERAL, outs() << "\tCollect context-sensitive locks\n");
    DBOUT(DMTA, outs() << "\tCollect context-sensitive locks\n");
    collectCxtLock();

    DBOUT(DGENERAL, outs() << "\tInter-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tInter-procedural LockAnalysis\n");
    analyzeLockSpanCxtStmt();

    DOTIMESTAT(double lockEnd = PTAStat::getClk(true));
    DOTIMESTAT(lockTime += (lockEnd - lockStart) / TIMEINTERVAL);
    
    // Update stats
    stats.locksites = locksites.size();
    stats.unlocksites = unlocksites.size();
    stats.candidateFuncs = lockcandidateFuncSet.size();
}

// Note: hasCxtStmtFromInst, getCxtStmtsFromInst, hasCxtLockfromCxtStmt, getCxtLockfromCxtStmt
// are inline methods in base class, so we don't need to redefine them here.
// They will use the base class implementation which accesses instToCxtStmtSet and cxtStmtToCxtLockSet
// which are inherited from the base class.

// ---- SVF logic (adapted, but traversal injected) ----

void SlicedLockAnalysis::collectLockUnlocksites()
{
    ThreadCallGraph* tcg = tct->getThreadCallGraph();

    // Use sliced CallGraph view if available, otherwise fall back to original
    // 优先使用 ThreadCallGraph，不存在则使用 CallGraph
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
        const FunObjVar* F = item.second->getFunction();
        for (auto it : *F)
        {
            const SVFBasicBlock* bb = it.second;
            for (const ICFGNode* icfgNode : bb->getICFGNodeList())
            {
                if (!acceptsNode(icfgNode))
                    continue;
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDRelease(cast<CallICFGNode>(icfgNode)))
                    unlocksites.insert(icfgNode);
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDAcquire(cast<CallICFGNode>(icfgNode)))
                    locksites.insert(icfgNode);
            }
        }
    }
}

void SlicedLockAnalysis::buildCandidateFuncSetforLock()
{
    ThreadCallGraph* tcg = tct->getThreadCallGraph();
    TCT::PTACGNodeSet visited;
    FIFOWorkList<const CallGraphNode*> worklist;

    for (auto it = locksites.begin(), eit = locksites.end(); it != eit; ++it)
    {
        const FunObjVar* fun = (*it)->getFun();
        CallGraphNode* cgnode = tcg->getCallGraphNode(fun);
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    for (auto it = unlocksites.begin(), eit = unlocksites.end(); it != eit; ++it)
    {
        const FunObjVar* fun = (*it)->getFun();
        CallGraphNode* cgnode = tcg->getCallGraphNode(fun);
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }

    while (!worklist.empty())
    {
        const CallGraphNode* node = worklist.pop();
        lockcandidateFuncSet.insert(node->getFunction());
        for (auto nit = node->InEdgeBegin(), neit = node->InEdgeEnd(); nit != neit; ++nit)
        {
            const CallGraphNode* srcNode = (*nit)->getSrcNode();
            if (visited.find(srcNode) == visited.end())
            {
                visited.insert(srcNode);
                worklist.push(srcNode);
            }
        }
    }
}

void SlicedLockAnalysis::analyzeIntraProcedualLock()
{
    for (auto it = locksites.begin(), ie = locksites.end(); it != ie; ++it)
    {
        const ICFGNode* lockSite = *it;
        assert(isCallSite(lockSite) && "Lock acquire instruction must be a CallSite");

        InstSet forwardInsts;
        InstSet backwardInsts;
        InstSet unlockSet;

        bool forward = intraForwardTraverse(lockSite, unlockSet, forwardInsts);
        bool backward = intraBackwardTraverse(unlockSet, backwardInsts);

        if (forward && backward)
        {
            // addIntraLock(lockSite, forwardInsts)
            for (auto sit = forwardInsts.begin(), seit = forwardInsts.end(); sit != seit; ++sit)
            {
                instCILocksMap[*sit].insert(lockSite);
                ciLocktoSpan[lockSite].insert(*sit);
            }
        }
        else if (forward && !backward)
        {
            // addCondIntraLock(lockSite, forwardInsts)
            for (auto sit = forwardInsts.begin(), seit = forwardInsts.end(); sit != seit; ++sit)
                instTocondCILocksMap[*sit].insert(lockSite);
        }
    }
}

bool SlicedLockAnalysis::intraForwardTraverse(const ICFGNode* lockSite, InstSet& unlockSet, InstSet& forwardInsts)
{
    const FunObjVar* svfFun = lockSite->getFun();
    InstVec worklist;
    worklist.push_back(lockSite);

    while (!worklist.empty())
    {
        const ICFGNode* I = worklist.back();
        worklist.pop_back();

        const ICFGNode* exitInst = svfFun->getExitBB()->back();
        if (exitInst == I)
            return false;

        if (forwardInsts.find(I) != forwardInsts.end())
            continue;
        forwardInsts.insert(I);

        if (isTDRelease(I) && isAliasedLocks(lockSite, I))
        {
            unlockSet.insert(I);
            continue;
        }

        std::vector<const ICFGNode*> succ;
        getSuccNodes(I, succ);
        for (const ICFGNode* dst : succ)
        {
            if (dst->getFun() == I->getFun())
                worklist.push_back(dst);
        }
    }
    return true;
}

bool SlicedLockAnalysis::intraBackwardTraverse(const InstSet& unlockSet, InstSet& backwardInsts)
{
    InstVec worklist;
    for (auto it = unlockSet.begin(), eit = unlockSet.end(); it != eit; ++it)
    {
        const ICFGNode* unlockSite = *it;
        const ICFGNode* entryInst = unlockSite->getFun()->getEntryBlock()->back();
        worklist.push_back(unlockSite);

        while (!worklist.empty())
        {
            const ICFGNode* I = worklist.back();
            worklist.pop_back();

            if (entryInst == I)
                return false;

            if (backwardInsts.find(I) != backwardInsts.end())
                continue;
            backwardInsts.insert(I);

            if (isTDAcquire(I) && isAliasedLocks(unlockSite, I))
                continue;

            std::vector<const ICFGNode*> pred;
            // For backward traversal, we need to get predecessors
            if (icfgView != nullptr)
            {
                icfgView->getPredNodes(I, pred);
            }
            else
            {
                pred.clear();
                for (const ICFGEdge* e : I->getInEdges())
                    pred.push_back(e->getSrcNode());
            }
            for (const ICFGNode* src : pred)
            {
                if (src->getFun() == I->getFun())
                    worklist.push_back(src);
            }
        }
    }
    return true;
}

bool SlicedLockAnalysis::pushToCTPWorkList(const CxtLockProc& clp)
{
    if (!isVisitedCTPs(clp))
    {
        visitedCTPs.insert(clp);
        return clpList.push(clp);
    }
    return false;
}

SlicedLockAnalysis::CxtLockProc SlicedLockAnalysis::popFromCTPWorkList()
{
    return clpList.pop();
}

bool SlicedLockAnalysis::isVisitedCTPs(const CxtLockProc& clp) const
{
    return visitedCTPs.find(clp) != visitedCTPs.end();
}

bool SlicedLockAnalysis::pushToCTSWorkList(const CxtStmt& cs)
{
    return cxtStmtList.push(cs);
}

CxtStmt SlicedLockAnalysis::popFromCTSWorkList()
{
    return cxtStmtList.pop();
}

void SlicedLockAnalysis::collectCxtLock()
{
    FunSet entryFuncSet = tct->getEntryProcs();
    for (auto it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
    {
        if (lockcandidateFuncSet.find(*it) == lockcandidateFuncSet.end())
            continue;
        CallStrCxt cxt;
        CxtLockProc t(cxt, *it);
        pushToCTPWorkList(t);
    }

    while (!clpList.empty())
    {
        CxtLockProc clp = popFromCTPWorkList();
        CallGraphNode* cgNode = getTCG()->getCallGraphNode(clp.getProc());
        if (lockcandidateFuncSet.find(cgNode->getFunction()) == lockcandidateFuncSet.end())
            continue;

        for (auto nit = cgNode->OutEdgeBegin(), neit = cgNode->OutEdgeEnd(); nit != neit; ++nit)
        {
            const CallGraphEdge* cgEdge = (*nit);
            for (auto cit = cgEdge->directCallsBegin(), ecit = cgEdge->directCallsEnd(); cit != ecit; ++cit)
                handleCallRelation(clp, cgEdge, *cit);
            for (auto cit = cgEdge->indirectCallsBegin(), ecit = cgEdge->indirectCallsEnd(); cit != ecit; ++cit)
                handleCallRelation(clp, cgEdge, *cit);
        }
    }
}

void SlicedLockAnalysis::handleCallRelation(CxtLockProc& clp, const CallGraphEdge* cgEdge, const CallICFGNode* cs)
{
    CallStrCxt cxt(clp.getContext());
    const ICFGNode* curNode = cs;
    if (!acceptsNode(curNode))
        return;
    if (isTDAcquire(curNode))
    {
        addCxtLock(cxt, curNode);
        return;
    }
    const FunObjVar* svfcallee = cgEdge->getDstNode()->getFunction();
    pushCxt(cxt, SVFUtil::cast<CallICFGNode>(curNode), svfcallee);
    CxtLockProc newclp(cxt, svfcallee);
    pushToCTPWorkList(newclp);
}

void SlicedLockAnalysis::analyzeLockSpanCxtStmt()
{
    FunSet entryFuncSet = tct->getEntryProcs();
    for (auto it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
    {
        if (lockcandidateFuncSet.find(*it) == lockcandidateFuncSet.end())
            continue;
        CallStrCxt cxt;
        const ICFGNode* frontInst = getFunEntry(*it);
        if (!acceptsNode(frontInst))
            continue;
        CxtStmt cxtstmt(cxt, frontInst);
        pushToCTSWorkList(cxtstmt);
    }

    while (!cxtStmtList.empty())
    {
        CxtStmt cts = popFromCTSWorkList();
        stats.cxtStmtVisited++;

        touchCxtStmt(cts);
        const ICFGNode* curInst = cts.getStmt();
        if (!acceptsNode(curInst))
            continue;
        instToCxtStmtSet[curInst].insert(cts);

        if (isTDFork(curInst))
            handleFork(cts);
        else if (isTDAcquire(curInst))
        {
            assert(hasCxtLock(cts) && "context-sensitive lock not found!!");
            if (addCxtStmtToSpan(cts, cts))
                handleIntra(cts);
        }
        else if (isTDRelease(curInst))
        {
            CxtStmt tmp = cts;
            if (removeCxtStmtToSpan(tmp, tmp))
                handleIntra(tmp);
        }
        else if (isCallSite(curInst) && !isExtCall(curInst))
            handleCall(cts);
        else if (SVFUtil::dyn_cast<FunExitICFGNode>(curInst))
            handleRet(cts);
        else
            handleIntra(cts);
    }
}

void SlicedLockAnalysis::handleFork(const CxtStmt& cts)
{
    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    if (getTCG()->hasThreadForkEdge(call))
    {
        for (auto cgIt = getTCG()->getForkEdgeBegin(call), ecgIt = getTCG()->getForkEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfcallee = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfcallee);
            const ICFGNode* svfInst = getFunEntry(svfcallee);
            if (!acceptsNode(svfInst))
                continue;
            CxtStmt newCts(newCxt, svfInst);
            markCxtStmtFlag(newCts, cts);
        }
    }
    handleIntra(cts);
}

void SlicedLockAnalysis::handleCall(const CxtStmt& cts)
{
    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    if (getTCG()->hasCallGraphEdge(call))
    {
        for (auto cgIt = getTCG()->getCallEdgeBegin(call), ecgIt = getTCG()->getCallEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfcallee = (*cgIt)->getDstNode()->getFunction();
            if (SVFUtil::isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfcallee);
            const ICFGNode* svfInst = getFunEntry(svfcallee);
            if (!acceptsNode(svfInst))
                continue;
            CxtStmt newCts(newCxt, svfInst);
            markCxtStmtFlag(newCts, cts);
        }
    }
}

void SlicedLockAnalysis::handleRet(const CxtStmt& cts)
{
    const ICFGNode* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    CallGraphNode* curFunNode = getTCG()->getCallGraphNode(curInst->getFun());

    std::vector<const CallGraphEdge*> inEdges;
    getInEdgesOfCallGraphNode(curFunNode, inEdges);
    
    for (const CallGraphEdge* edgeConst : inEdges)
    {
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edgeConst))
            continue;
        
        // Need non-const for directCallsBegin/End and other operations
        CallGraphEdge* edge = const_cast<CallGraphEdge*>(edgeConst);

        auto processCallInst = [&](const ICFGNode* inst) {
            CallStrCxt newCxt = curCxt;
            if (matchCxt(newCxt, SVFUtil::cast<CallICFGNode>(inst), curFunNode->getFunction()))
            {
                std::vector<const ICFGNode*> succ;
                getSuccNodes(curInst, succ);
                for (const ICFGNode* dst : succ)
                {
                    if (dst->getFun() != inst->getFun())
                        continue;
                    if (!hasCxtStmtFromInst(inst))
                        continue;
                    for (const CxtStmt& cxtStmt : getCxtStmtsFromInst(inst))
                    {
                        CallStrCxt callSiteCxt = cxtStmt.getContext();
                        if (isContextSuffix(newCxt, callSiteCxt))
                        {
                            CxtStmt newCts(callSiteCxt, dst);
                            markCxtStmtFlag(newCts, cts);
                        }
                    }
                }
            }
        };

        for (auto cit = edge->directCallsBegin(), ecit = edge->directCallsEnd(); cit != ecit; ++cit)
            processCallInst(*cit);
        for (auto cit = edge->indirectCallsBegin(), ecit = edge->indirectCallsEnd(); cit != ecit; ++cit)
            processCallInst(*cit);
    }
}

void SlicedLockAnalysis::handleIntra(const CxtStmt& cts)
{
    const ICFGNode* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    std::vector<const ICFGNode*> succ;
    getSuccNodes(curInst, succ);
    for (const ICFGNode* dst : succ)
    {
        if (dst->getFun() == curInst->getFun())
        {
            CxtStmt newCts(curCxt, dst);
            markCxtStmtFlag(newCts, cts);
        }
    }
}

void SlicedLockAnalysis::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    // If TCT is SlicedTCT, use its pushCxt method which supports custom maxContextLen
    // Otherwise, use base class TCT::pushCxt
    // Use dynamic_cast instead of dyn_cast since SlicedTCT is a concrete class
    if (SlicedTCT* slicedTCT = dynamic_cast<SlicedTCT*>(tct))
    {
        slicedTCT->pushCxt(cxt, call, callee);
    }
    else
    {
        tct->pushCxt(cxt, call, callee);
    }
}

bool SlicedLockAnalysis::matchCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    const FunObjVar* svfcaller = call->getFun();
    CallSiteID csId = getTCG()->getCallSiteID(call, callee);
    if (cxt.empty())
        return true;
    if (tct->inSameCallGraphSCC(getTCG()->getCallGraphNode(svfcaller), getTCG()->getCallGraphNode(callee)) == false)
    {
        if (cxt.back() == csId)
            cxt.pop_back();
        else
            return false;
    }
    return true;
}

bool SlicedLockAnalysis::isContextSuffix(const CallStrCxt& lhs, const CallStrCxt& call) const
{
    return tct->isContextSuffix(lhs, call);
}

bool SlicedLockAnalysis::isTDFork(const ICFGNode* call) const
{
    if (!SVFUtil::isa<CallICFGNode>(call))
        return false;
    return getTCG()->getThreadAPI()->isTDFork(SVFUtil::cast<CallICFGNode>(call));
}

bool SlicedLockAnalysis::isTDAcquire(const ICFGNode* call) const
{
    if (!SVFUtil::isa<CallICFGNode>(call))
        return false;
    return getTCG()->getThreadAPI()->isTDAcquire(SVFUtil::cast<CallICFGNode>(call));
}

bool SlicedLockAnalysis::isTDRelease(const ICFGNode* call) const
{
    if (!SVFUtil::isa<CallICFGNode>(call))
        return false;
    return getTCG()->getThreadAPI()->isTDRelease(SVFUtil::cast<CallICFGNode>(call));
}

bool SlicedLockAnalysis::isCallSite(const ICFGNode* inst) const
{
    return tct->isCallSite(inst);
}

bool SlicedLockAnalysis::isExtCall(const ICFGNode* inst) const
{
    return tct->isExtCall(inst);
}

const SVFVar* SlicedLockAnalysis::getLockVal(const ICFGNode* call) const
{
    return getTCG()->getThreadAPI()->getLockVal(call);
}

ThreadCallGraph* SlicedLockAnalysis::getTCG() const
{
    return tct->getThreadCallGraph();
}

bool SlicedLockAnalysis::isAliasedLocks(const ICFGNode* i1, const ICFGNode* i2) const
{
    return tct->getPTA()->alias(getLockVal(i1)->getId(), getLockVal(i2)->getId());
}

void SlicedLockAnalysis::addCxtLock(const CallStrCxt& cxt, const ICFGNode* inst)
{
    CxtLock cxtlock(cxt, inst);
    cxtLockset.insert(cxtlock);
}

bool SlicedLockAnalysis::hasCxtLock(const CxtLock& cxtLock) const
{
    return cxtLockset.find(cxtLock) != cxtLockset.end();
}

bool SlicedLockAnalysis::addCxtStmtToSpan(const CxtStmt& cts, const CxtLock& cl)
{
    cxtLocktoSpan[cl].insert(cts);
    return cxtStmtToCxtLockSet[cts].insert(cl).second;
}

bool SlicedLockAnalysis::removeCxtStmtToSpan(CxtStmt& cts, const CxtLock& cl)
{
    bool find = cxtStmtToCxtLockSet[cts].find(cl) != cxtStmtToCxtLockSet[cts].end();
    if (find)
    {
        cxtStmtToCxtLockSet[cts].erase(cl);
        cxtLocktoSpan[cl].erase(cts);
    }
    return find;
}

void SlicedLockAnalysis::touchCxtStmt(CxtStmt& cts)
{
    cxtStmtToCxtLockSet[cts];
}

void SlicedLockAnalysis::markCxtStmtFlag(const CxtStmt& tgr, const CxtStmt& src)
{
    const CxtLockSet& srclockset = getCxtLockfromCxtStmt(src);
    if (!hasCxtLockfromCxtStmt(tgr))
    {
        for (auto it = srclockset.begin(), eit = srclockset.end(); it != eit; ++it)
            addCxtStmtToSpan(tgr, *it);
        pushToCTSWorkList(tgr);
    }
    else
    {
        if (intersect(cxtStmtToCxtLockSet[tgr], srclockset))
            pushToCTSWorkList(tgr);
    }
}

bool SlicedLockAnalysis::intersect(CxtLockSet& tgrlockset, const CxtLockSet& srclockset)
{
    CxtLockSet toBeDeleted;
    for (auto it = tgrlockset.begin(), eit = tgrlockset.end(); it != eit; ++it)
    {
        if (srclockset.find(*it) == srclockset.end())
            toBeDeleted.insert(*it);
    }
    for (auto it = toBeDeleted.begin(), eit = toBeDeleted.end(); it != eit; ++it)
        tgrlockset.erase(*it);
    return !toBeDeleted.empty();
}

const ICFGNode* SlicedLockAnalysis::getFunEntry(const FunObjVar* fun) const
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

void SlicedLockAnalysis::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
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

bool SlicedLockAnalysis::acceptsNode(const ICFGNode* node) const
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

void SlicedLockAnalysis::getInEdgesOfCallGraphNode(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    
    // Use sliced view if available (优先使用 TCG)
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

