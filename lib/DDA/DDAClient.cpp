/*
 * @file: DDAClient.cpp
 * @author: yesen
 * @date: 16 Feb 2015
 *
 * LICENSE
 *
 */


#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "MemoryModel/PointsTo.h"
#include "SVF-FE/CPPUtil.h"

#include "DDA/DDAClient.h"
#include "DDA/FlowDDA.h"
#include <iostream>
#include <iomanip>	// for std::setw

using namespace SVF;
using namespace SVFUtil;


void DDAClient::answerQueries(PointerAnalysis* pta)
{

    DDAStat* stat = static_cast<DDAStat*>(pta->getStat());
    u32_t vmrss = 0;
    u32_t vmsize = 0;
    SVFUtil::getMemoryUsageKB(&vmrss, &vmsize);
    stat->setMemUsageBefore(vmrss, vmsize);

    collectCandidateQueries(pta->getPAG());

    u32_t count = 0;
    for (OrderedNodeSet::iterator nIter = candidateQueries.begin();
            nIter != candidateQueries.end(); ++nIter,++count)
    {
        PAGNode* node = pta->getPAG()->getGNode(*nIter);
        if(pta->getPAG()->isValidTopLevelPtr(node))
        {
            DBOUT(DGENERAL,outs() << "\n@@Computing PointsTo for :" << node->getId() <<
                  " [" << count + 1<< "/" << candidateQueries.size() << "]" << " \n");
            DBOUT(DDDA,outs() << "\n@@Computing PointsTo for :" << node->getId() <<
                  " [" << count + 1<< "/" << candidateQueries.size() << "]" << " \n");
            setCurrentQueryPtr(node->getId());
            pta->computeDDAPts(node->getId());
        }
    }

    vmrss = vmsize = 0;
    SVFUtil::getMemoryUsageKB(&vmrss, &vmsize);
    stat->setMemUsageAfter(vmrss, vmsize);
}

OrderedNodeSet& FunptrDDAClient::collectCandidateQueries(SVFIR* p)
{
    setPAG(p);
    for(SVFIR::CallSiteToFunPtrMap::const_iterator it = pag->getIndirectCallsites().begin(),
            eit = pag->getIndirectCallsites().end(); it!=eit; ++it)
    {
        if (cppUtil::isVirtualCallSite(SVFUtil::getLLVMCallSite(it->first->getCallSite())))
        {
            const Value *vtblPtr = cppUtil::getVCallVtblPtr(SVFUtil::getLLVMCallSite(it->first->getCallSite()));
            assert(pag->hasValueNode(vtblPtr) && "not a vtable pointer?");
            NodeID vtblId = pag->getValueNode(vtblPtr);
            addCandidate(vtblId);
            vtableToCallSiteMap[vtblId] = it->first;
        }
        else
        {
            addCandidate(it->second);
        }
    }
    return candidateQueries;
}

void FunptrDDAClient::performStat(PointerAnalysis* pta)
{

    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pta->getPAG());
    u32_t totalCallsites = 0;
    u32_t morePreciseCallsites = 0;
    u32_t zeroTargetCallsites = 0;
    u32_t oneTargetCallsites = 0;
    u32_t twoTargetCallsites = 0;
    u32_t moreThanTwoCallsites = 0;

    for (VTablePtrToCallSiteMap::iterator nIter = vtableToCallSiteMap.begin();
            nIter != vtableToCallSiteMap.end(); ++nIter)
    {
        NodeID vtptr = nIter->first;
        const PointsTo& ddaPts = pta->getPts(vtptr);
        const PointsTo& anderPts = ander->getPts(vtptr);

        PTACallGraph* callgraph = ander->getPTACallGraph();
        const CallICFGNode* cbn = nIter->second;

        if(!callgraph->hasIndCSCallees(cbn))
        {
            //outs() << "virtual callsite has no callee" << *(nIter->second.getInstruction()) << "\n";
            continue;
        }

        const PTACallGraph::FunctionSet& callees = callgraph->getIndCSCallees(cbn);
        totalCallsites++;
        if(callees.size() == 0)
            zeroTargetCallsites++;
        else if(callees.size() == 1)
            oneTargetCallsites++;
        else if(callees.size() == 2)
            twoTargetCallsites++;
        else
            moreThanTwoCallsites++;

        if(ddaPts.count() >= anderPts.count() || ddaPts.empty())
            continue;

        Set<const SVFFunction*> ander_vfns;
        Set<const SVFFunction*> dda_vfns;
        ander->getVFnsFromPts(cbn,anderPts, ander_vfns);
        pta->getVFnsFromPts(cbn,ddaPts, dda_vfns);

        ++morePreciseCallsites;
        outs() << "============more precise callsite =================\n";
        outs() << SVFUtil::value2String((nIter->second)->getCallSite()) << "\n";
        outs() << getSourceLoc((nIter->second)->getCallSite()) << "\n";
        outs() << "\n";
        outs() << "------ander pts or vtable num---(" << anderPts.count()  << ")--\n";
        outs() << "------DDA vfn num---(" << ander_vfns.size() << ")--\n";
        //ander->dumpPts(vtptr, anderPts);
        outs() << "------DDA pts or vtable num---(" << ddaPts.count() << ")--\n";
        outs() << "------DDA vfn num---(" << dda_vfns.size() << ")--\n";
        //pta->dumpPts(vtptr, ddaPts);
        outs() << "-------------------------\n";
        outs() << "\n";
        outs() << "=================================================\n";
    }

    outs() << "=================================================\n";
    outs() << "Total virtual callsites: " << vtableToCallSiteMap.size() << "\n";
    outs() << "Total analyzed virtual callsites: " << totalCallsites << "\n";
    outs() << "Indirect call map size: " << ander->getPTACallGraph()->getIndCallMap().size() << "\n";
    outs() << "Precise callsites: " << morePreciseCallsites << "\n";
    outs() << "Zero target callsites: " << zeroTargetCallsites << "\n";
    outs() << "One target callsites: " << oneTargetCallsites << "\n";
    outs() << "Two target callsites: " << twoTargetCallsites << "\n";
    outs() << "More than two target callsites: " << moreThanTwoCallsites << "\n";
    outs() << "=================================================\n";
}


/// Only collect function pointers as query candidates.
OrderedNodeSet& AliasDDAClient::collectCandidateQueries(SVFIR* pag)
{
    setPAG(pag);
    SVFStmt::SVFStmtSetTy& loads = pag->getSVFStmtSet(SVFStmt::Load);
    for (SVFStmt::SVFStmtSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        PAGNode* loadsrc = (*iter)->getSrcNode();
        loadSrcNodes.insert(loadsrc);
        addCandidate(loadsrc->getId());
    }

    SVFStmt::SVFStmtSetTy& stores = pag->getSVFStmtSet(SVFStmt::Store);
    for (SVFStmt::SVFStmtSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        PAGNode* storedst = (*iter)->getDstNode();
        storeDstNodes.insert(storedst);
        addCandidate(storedst->getId());
    }
    SVFStmt::SVFStmtSetTy& geps = pag->getSVFStmtSet(SVFStmt::Gep);
    for (SVFStmt::SVFStmtSetTy::iterator iter = geps.begin(), eiter =
                geps.end(); iter != eiter; ++iter)
    {
        PAGNode* gepsrc = (*iter)->getSrcNode();
        gepSrcNodes.insert(gepsrc);
        addCandidate(gepsrc->getId());
    }
    return candidateQueries;
}

void AliasDDAClient::performStat(PointerAnalysis* pta)
{

    for(PAGNodeSet::const_iterator lit = loadSrcNodes.begin(); lit!=loadSrcNodes.end(); lit++)
    {
        for(PAGNodeSet::const_iterator sit = storeDstNodes.begin(); sit!=storeDstNodes.end(); sit++)
        {
            const PAGNode* node1 = *lit;
            const PAGNode* node2 = *sit;
            if(node1->hasValue() && node2->hasValue())
            {
                AliasResult result = pta->alias(node1->getId(),node2->getId());

                outs() << "\n=================================================\n";
                outs() << "Alias Query for (" << SVFUtil::value2String(node1->getValue()) << ",";
                outs() << SVFUtil::value2String(node2->getValue()) << ") \n";
                outs() << "[NodeID:" << node1->getId() <<  ", NodeID:" << node2->getId() << " " << result << "]\n";
                outs() << "=================================================\n";

            }
        }
    }
}

