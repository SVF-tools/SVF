/*
 * ContextDDA.cpp
 *
 *  Created on: Aug 17, 2014
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "DDA/ContextDDA.h"
#include "DDA/FlowDDA.h"
#include "DDA/DDAClient.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Constructor
 */
ContextDDA::ContextDDA(PAG* _pag,  DDAClient* client)
    : CondPTAImpl<ContextCond>(_pag, PointerAnalysis::Cxt_DDA),DDAVFSolver<CxtVar,CxtPtSet,CxtLocDPItem>(),
      _client(client)
{
    flowDDA = new FlowDDA(_pag, client);
}

/*!
 * Destructor
 */
ContextDDA::~ContextDDA()
{
    if(flowDDA)
        delete flowDDA;
    flowDDA = nullptr;
}

/*!
 * Analysis initialization
 */
void ContextDDA::initialize()
{
    CondPTAImpl<ContextCond>::initialize();
    buildSVFG(pag);
    setCallGraph(getPTACallGraph());
    setCallGraphSCC(getCallGraphSCC());
    stat = setDDAStat(new DDAStat(this));
    flowDDA->initialize();
}

/*!
 * Compute points-to set for a context-sensitive pointer
 */
const CxtPtSet& ContextDDA::computeDDAPts(const CxtVar& var)
{

    resetQuery();
    LocDPItem::setMaxBudget(Options::CxtBudget);

    NodeID id = var.get_id();
    PAGNode* node = getPAG()->getPAGNode(id);
    CxtLocDPItem dpm = getDPIm(var, getDefSVFGNode(node));

    // start DDA analysis
    DOTIMESTAT(double start = DDAStat::getClk(true));
    const CxtPtSet& cpts = findPT(dpm);
    DOTIMESTAT(ddaStat->_AnaTimePerQuery = DDAStat::getClk(true) - start);
    DOTIMESTAT(ddaStat->_TotalTimeOfQueries += ddaStat->_AnaTimePerQuery);

    if(isOutOfBudgetQuery() == false)
        unionPts(var,cpts);
    else
        handleOutOfBudgetDpm(dpm);

    if (this->printStat())
        DOSTAT(stat->performStatPerQuery(id));
    DBOUT(DGENERAL, stat->printStatPerQuery(id,getBVPointsTo(getPts(var))));
    return this->getPts(var);
}

/*!
 *  Compute points-to set for an unconditional pointer
 */
void ContextDDA::computeDDAPts(NodeID id)
{
    ContextCond cxt;
    CxtVar var(cxt, id);
    computeDDAPts(var);
}

/*!
 * Handle out-of-budget dpm
 */
void ContextDDA::handleOutOfBudgetDpm(const CxtLocDPItem& dpm)
{

    DBOUT(DGENERAL,outs() << "~~~Out of budget query, downgrade to flow sensitive analysis \n");
    flowDDA->computeDDAPts(dpm.getCurNodeID());
    const PointsTo& flowPts = flowDDA->getPts(dpm.getCurNodeID());
    CxtPtSet cxtPts;
    for(PointsTo::iterator it = flowPts.begin(), eit = flowPts.end(); it!=eit; ++it)
    {
        ContextCond cxt;
        CxtVar var(cxt, *it);
        cxtPts.set(var);
    }
    updateCachedPointsTo(dpm,cxtPts);
    unionPts(dpm.getCondVar(),cxtPts);
    addOutOfBudgetDpm(dpm);
}

/*!
 * context conditions of local(not in recursion)  and global variables are compatible
 */
bool ContextDDA::isCondCompatible(const ContextCond& cxt1, const ContextCond& cxt2, bool singleton) const
{
    if(singleton)
        return true;

    int i = cxt1.cxtSize() - 1;
    int j = cxt2.cxtSize() - 1;
    for(; i >= 0 && j>=0; i--, j--)
    {
        if(cxt1[i] != cxt2[j])
            return false;
    }
    return true;
}

/*!
 * Generate field objects for structs
 */
CxtPtSet ContextDDA::processGepPts(const GepSVFGNode* gep, const CxtPtSet& srcPts)
{
    CxtPtSet tmpDstPts;
    for (CxtPtSet::iterator piter = srcPts.begin(); piter != srcPts.end(); ++piter)
    {

        CxtVar ptd = *piter;
        if (isBlkObjOrConstantObj(ptd.get_id()))
            tmpDstPts.set(ptd);
        else
        {
            if (SVFUtil::isa<VariantGepPE>(gep->getPAGEdge()))
            {
                setObjFieldInsensitive(ptd.get_id());
                CxtVar var(ptd.get_cond(),getFIObjNode(ptd.get_id()));
                tmpDstPts.set(var);
            }
            else if (const NormalGepPE* normalGep = SVFUtil::dyn_cast<NormalGepPE>(gep->getPAGEdge()))
            {
                CxtVar var(ptd.get_cond(),getGepObjNode(ptd.get_id(),normalGep->getLocationSet()));
                tmpDstPts.set(var);
            }
            else
                assert(false && "new gep edge?");
        }
    }

    DBOUT(DDDA, outs() << "\t return created gep objs ");
    DBOUT(DDDA, outs() << srcPts.toString());
    DBOUT(DDDA, outs() << " --> ");
    DBOUT(DDDA, outs() << tmpDstPts.toString());
    DBOUT(DDDA, outs() << "\n");
    return tmpDstPts;
}

bool ContextDDA::testIndCallReachability(CxtLocDPItem& dpm, const SVFFunction* callee, const CallBlockNode* cs)
{
    if(getPAG()->isIndirectCallSites(cs))
    {
        NodeID id = getPAG()->getFunPtr(cs);
        PAGNode* node = getPAG()->getPAGNode(id);
        CxtVar funptrVar(dpm.getCondVar().get_cond(), id);
        CxtLocDPItem funptrDpm = getDPIm(funptrVar,getDefSVFGNode(node));
        PointsTo pts = getBVPointsTo(findPT(funptrDpm));
        if(pts.test(getPAG()->getObjectNode(callee->getLLVMFun())))
            return true;
        else
            return false;
    }
    return true;
}

/*!
 * get callsite id from call, return 0 if it is a spurious call edge
 * translate the callsite id from pre-computed callgraph on SVFG to the one on current callgraph
 */
CallSiteID ContextDDA::getCSIDAtCall(CxtLocDPItem&, const SVFGEdge* edge)
{

    CallSiteID svfg_csId = 0;
    if (const CallDirSVFGEdge* callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
        svfg_csId = callEdge->getCallSiteId();
    else
        svfg_csId = SVFUtil::cast<CallIndSVFGEdge>(edge)->getCallSiteId();

    const CallBlockNode* cbn = getSVFG()->getCallSite(svfg_csId);
    const SVFFunction* callee = edge->getDstNode()->getFun();

    if(getPTACallGraph()->hasCallSiteID(cbn,callee))
    {
        return getPTACallGraph()->getCallSiteID(cbn,callee);
    }

    return 0;
}

/*!
 * get callsite id from return, return 0 if it is a spurious return edge
 * translate the callsite id from pre-computed callgraph on SVFG to the one on current callgraph
 */
CallSiteID ContextDDA::getCSIDAtRet(CxtLocDPItem&, const SVFGEdge* edge)
{

    CallSiteID svfg_csId = 0;
    if (const RetDirSVFGEdge* retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
        svfg_csId = retEdge->getCallSiteId();
    else
        svfg_csId = SVFUtil::cast<RetIndSVFGEdge>(edge)->getCallSiteId();

    const CallBlockNode* cbn = getSVFG()->getCallSite(svfg_csId);
    const SVFFunction* callee = edge->getSrcNode()->getFun();

    if(getPTACallGraph()->hasCallSiteID(cbn,callee))
    {
        return getPTACallGraph()->getCallSiteID(cbn,callee);
    }

    return 0;
}


/// Handle conditions during backward traversing
bool ContextDDA::handleBKCondition(CxtLocDPItem& dpm, const SVFGEdge* edge)
{
    _client->handleStatement(edge->getSrcNode(), dpm.getCurNodeID());

    if (edge->isCallVFGEdge())
    {
        /// we don't handle context in recursions, they treated as assignments
        if(CallSiteID csId = getCSIDAtCall(dpm,edge))
        {

            if(isEdgeInRecursion(csId))
            {
                DBOUT(DDDA,outs() << "\t\t call edge " << getPTACallGraph()->getCallerOfCallSite(csId)->getName() <<
                      "=>" << getPTACallGraph()->getCalleeOfCallSite(csId)->getName() << "in recursion \n");
                popRecursiveCallSites(dpm);
            }
            else
            {
                if (dpm.matchContext(csId) == false)
                {
                    DBOUT(DDDA,	outs() << "\t\t context not match, edge "
                          << edge->getDstID() << " --| " << edge->getSrcID() << " \t");
                    DBOUT(DDDA, dumpContexts(dpm.getCond()));
                    return false;
                }

                DBOUT(DDDA, outs() << "\t\t match contexts ");
                DBOUT(DDDA, dumpContexts(dpm.getCond()));
            }
        }
    }

    else if (edge->isRetVFGEdge())
    {
        /// we don't handle context in recursions, they treated as assignments
        if(CallSiteID csId = getCSIDAtRet(dpm,edge))
        {

            if(isEdgeInRecursion(csId))
            {
                DBOUT(DDDA,outs() << "\t\t return edge " << getPTACallGraph()->getCalleeOfCallSite(csId)->getName() <<
                      "=>" << getPTACallGraph()->getCallerOfCallSite(csId)->getName() << "in recursion \n");
                popRecursiveCallSites(dpm);
            }
            else
            {
                /// TODO: When this call site id is contained in current call string, we may find a recursion. Try
                ///       to solve this later.
                if (dpm.getCond().containCallStr(csId))
                {
                    outOfBudgetQuery = true;
                    SVFUtil::writeWrnMsg("Call site ID is contained in call string. Is this a recursion?");
                    return false;
                }
                else
                {
                    assert(dpm.getCond().containCallStr(csId) ==false && "contain visited call string ??");
                    if(dpm.pushContext(csId))
                    {
                        DBOUT(DDDA, outs() << "\t\t push context ");
                        DBOUT(DDDA, dumpContexts(dpm.getCond()));
                    }
                    else
                    {
                        DBOUT(DDDA, outs() << "\t\t context is full ");
                        DBOUT(DDDA, dumpContexts(dpm.getCond()));
                    }
                }
            }
        }
    }

    return true;
}


/// we exclude concrete heap given the following conditions:
/// (1) concrete calling context (not involved in recursion and not exceed the maximum context limit)
/// (2) not inside loop
bool ContextDDA::isHeapCondMemObj(const CxtVar& var, const StoreSVFGNode*)
{
    const MemObj* mem = _pag->getObject(getPtrNodeID(var));
    assert(mem && "memory object is null??");
    if(mem->isHeap())
    {
        if(const Instruction* mallocSite = SVFUtil::dyn_cast<Instruction>(mem->getRefVal()))
        {
            const Function* fun = mallocSite->getFunction();
            const SVFFunction* svfFun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
            if(_ander->isInRecursion(svfFun))
                return true;
            if(var.get_cond().isConcreteCxt() == false)
                return true;
            if(loopInfoBuilder.getLoopInfo(fun)->getLoopFor(mallocSite->getParent()))
                return true;
        }
    }
    return false;
}
