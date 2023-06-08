//===- ContextDDA.cpp -- Context-sensitive demand-driven analysis-------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

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
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Constructor
 */
ContextDDA::ContextDDA(SVFIR* _pag,  DDAClient* client)
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
    LocDPItem::setMaxBudget(Options::CxtBudget());

    NodeID id = var.get_id();
    PAGNode* node = getPAG()->getGNode(id);
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
            const GepStmt* gepStmt = SVFUtil::cast<GepStmt>(gep->getPAGEdge());
            if (gepStmt->isVariantFieldGep())
            {
                setObjFieldInsensitive(ptd.get_id());
                CxtVar var(ptd.get_cond(),getFIObjVar(ptd.get_id()));
                tmpDstPts.set(var);
            }
            else
            {
                CxtVar var(ptd.get_cond(),getGepObjVar(ptd.get_id(),gepStmt->getAccessPath().getConstantFieldIdx()));
                tmpDstPts.set(var);
            }
        }
    }

    DBOUT(DDDA, outs() << "\t return created gep objs ");
    DBOUT(DDDA, outs() << srcPts.toString());
    DBOUT(DDDA, outs() << " --> ");
    DBOUT(DDDA, outs() << tmpDstPts.toString());
    DBOUT(DDDA, outs() << "\n");
    return tmpDstPts;
}

bool ContextDDA::testIndCallReachability(CxtLocDPItem& dpm, const SVFFunction* callee, const CallICFGNode* cs)
{
    if(getPAG()->isIndirectCallSites(cs))
    {
        NodeID id = getPAG()->getFunPtr(cs);
        PAGNode* node = getPAG()->getGNode(id);
        CxtVar funptrVar(dpm.getCondVar().get_cond(), id);
        CxtLocDPItem funptrDpm = getDPIm(funptrVar,getDefSVFGNode(node));
        PointsTo pts = getBVPointsTo(findPT(funptrDpm));
        if(pts.test(getPAG()->getObjectNode(callee)))
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

    const CallICFGNode* cbn = getSVFG()->getCallSite(svfg_csId);
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

    const CallICFGNode* cbn = getSVFG()->getCallSite(svfg_csId);
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
    if (mem->isHeap())
    {
        if (!mem->getValue())
        {
            PAGNode *pnode = _pag->getGNode(getPtrNodeID(var));
            GepObjVar* gepobj = SVFUtil::dyn_cast<GepObjVar>(pnode);
            if (gepobj != nullptr)
            {
                assert(SVFUtil::isa<DummyObjVar>(_pag->getGNode(gepobj->getBaseNode()))
                       && "emtpy refVal in a gep object whose base is a non-dummy object");
            }
            else
            {
                assert((SVFUtil::isa<DummyObjVar, DummyValVar>(pnode))
                       && "empty refVal in non-dummy object");
            }
            return true;
        }
        else if(const SVFInstruction* mallocSite = SVFUtil::dyn_cast<SVFInstruction>(mem->getValue()))
        {
            const SVFFunction* svfFun = mallocSite->getFunction();
            if(_ander->isInRecursion(svfFun))
                return true;
            if(var.get_cond().isConcreteCxt() == false)
                return true;
            if(_pag->getICFG()->isInLoop(mallocSite))
                return true;
        }
    }
    return false;
}
