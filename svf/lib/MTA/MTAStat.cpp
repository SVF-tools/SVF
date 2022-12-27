//===- MTAStat.cpp -- Statistics for MTA-------------//
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
 * MTAStat.cpp
 *
 *  Created on: Jun 23, 2015
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/MTAStat.h"
#include "MTA/TCT.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/FSMPTA.h"
#include "Graphs/ThreadCallGraph.h"

using namespace SVF;


/*!
 * Statistics for thread call graph
 */
void MTAStat::performThreadCallGraphStat(ThreadCallGraph* tcg)
{
    u32_t numOfForkEdge = 0;
    u32_t numOfJoinEdge = 0;
    u32_t numOfIndForksite = 0;
    u32_t numOfIndForkEdge = 0;
    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
    {
        bool indirectfork = false;
        const SVFFunction* spawnee = SVFUtil::dyn_cast<SVFFunction>(tcg->getThreadAPI()->getForkedFun((*it)->getCallSite()));
        if(spawnee==nullptr)
        {
            numOfIndForksite++;
            indirectfork = true;
        }
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = tcg->getForkEdgeBegin(*it), ecgIt =
                    tcg->getForkEdgeEnd(*it); cgIt != ecgIt; ++cgIt)
        {
            numOfForkEdge++;
            if(indirectfork)
                numOfIndForkEdge++;
        }
    }

    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->joinsitesBegin(), eit = tcg->joinsitesEnd(); it != eit; ++it)
    {
        for (ThreadCallGraph::JoinEdgeSet::const_iterator cgIt = tcg->getJoinEdgeBegin(*it), ecgIt =
                    tcg->getJoinEdgeEnd(*it); cgIt != ecgIt; ++cgIt)
        {
            numOfJoinEdge++;
        }
    }


    PTNumStatMap.clear();
    PTNumStatMap["NumOfForkSite"] = tcg->getNumOfForksite();
    PTNumStatMap["NumOfForkEdge"] = numOfForkEdge;
    PTNumStatMap["NumOfJoinEdge"] = numOfJoinEdge;
    PTNumStatMap["NumOfJoinSite"] = tcg->getNumOfJoinsite();
    PTNumStatMap["NumOfIndForkSite"] = numOfIndForksite;
    PTNumStatMap["NumOfIndForkEdge"] = numOfIndForkEdge;
    PTNumStatMap["NumOfIndCallEdge"] = tcg->getNumOfResolvedIndCallEdge();

    SVFUtil::outs() << "\n****Thread Call Graph Statistics****\n";
    PTAStat::printStat();
}


void MTAStat::performTCTStat(TCT* tct)
{

    PTNumStatMap.clear();
    timeStatMap.clear();
    PTNumStatMap["NumOfCandidateFun"] = tct->getMakredProcs().size();
    PTNumStatMap["NumOfTotalFun"] = tct->getThreadCallGraph()->getTotalNodeNum();
    PTNumStatMap["NumOfTCTNode"] = tct->getTCTNodeNum();
    PTNumStatMap["NumOfTCTEdge"] = tct->getTCTEdgeNum();
    PTNumStatMap["MaxCxtSize"] = tct->getMaxCxtSize();
    timeStatMap["BuildingTCTTime"] = TCTTime;
    SVFUtil::outs() << "\n****Thread Creation Tree Statistics****\n";
    PTAStat::printStat();
}

/*!
 * Iterate every memory access pairs
 * write vs read
 * write vs write
 */
void MTAStat::performMHPPairStat(MHP* mhp, LockAnalysis* lsa)
{

    SVFIR* pag = SVFIR::getPAG();
    if(Options::AllPairMHP())
    {
        Set<const SVFInstruction*> instSet1;
        Set<const SVFInstruction*> instSet2;
        SVFModule* mod = mhp->getTCT()->getSVFModule();
        for (const SVFFunction* fun : mod->getFunctionSet())
        {
            if(SVFUtil::isExtCall(fun))
                continue;
            if(!mhp->isConnectedfromMain(fun))
                continue;
            for (SVFFunction::const_iterator bit =  fun->begin(), ebit = fun->end(); bit != ebit; ++bit)
            {
                const SVFBasicBlock* bb = *bit;
                for (SVFBasicBlock::const_iterator ii = bb->begin(), eii = bb->end(); ii != eii; ++ii)
                {
                    const SVFInstruction* inst = *ii;
                    for(const SVFStmt* stmt : pag->getSVFStmtList(pag->getICFG()->getICFGNode(inst)))
                    {
                        if(SVFUtil::isa<LoadStmt>(stmt))
                        {
                            instSet1.insert(inst);
                        }
                        else if(SVFUtil::isa<StoreStmt>(stmt))
                        {
                            instSet1.insert(inst);
                            instSet2.insert(inst);
                        }
                    }

                }
            }
        }


        for(Set<const SVFInstruction*>::const_iterator it1 = instSet1.begin(), eit1 = instSet1.end(); it1!=eit1; ++it1)
        {
            for(Set<const SVFInstruction*>::const_iterator it2 = instSet2.begin(), eit2 = instSet2.end(); it2!=eit2; ++it2)
            {
                mhp->mayHappenInParallel(*it1,*it2);
            }
        }
    }


    generalNumMap.clear();
    PTNumStatMap.clear();
    timeStatMap.clear();
    PTNumStatMap["TotalMHPQueries"] = mhp->numOfTotalQueries;
    PTNumStatMap["NumOfMHPPairs"] = mhp->numOfMHPQueries;
    PTNumStatMap["TotalLockQueries"] = lsa->numOfTotalQueries;
    PTNumStatMap["NumOfLockedPairs"] = lsa->numOfLockedQueries;
    PTNumStatMap["NumOfCxtLocks"] = lsa->getNumOfCxtLocks();
    PTNumStatMap["NumOfNewSVFGEdges"] = MTASVFGBuilder::numOfNewSVFGEdges;
    PTNumStatMap["NumOfRemovedEdges"] = MTASVFGBuilder::numOfRemovedSVFGEdges;
    PTNumStatMap["NumOfRemovedPTS"] = MTASVFGBuilder::numOfRemovedPTS;
    timeStatMap["InterlevAnaTime"] = mhp->interleavingTime;
    timeStatMap["LockAnaTime"] = lsa->lockTime;
    timeStatMap["InterlevQueryTime"] = mhp->interleavingQueriesTime;
    timeStatMap["LockQueryTime"] = lsa->lockQueriesTime;
    timeStatMap["MHPAnalysisTime"] = MHPTime;
    timeStatMap["MFSPTATime"] = FSMPTATime;

    SVFUtil::outs() << "\n****MHP Stmt Pairs Statistics****\n";
    PTAStat::printStat();
}

// void MTAStat::performAnnotationStat(MTAAnnotator* anno)
// {

//     PTNumStatMap.clear();
//     timeStatMap.clear();
//     PTNumStatMap["TotalNumOfStore"] = anno->numOfAllSt;
//     PTNumStatMap["TotalNumOfLoad"] = anno->numOfAllLd;
//     PTNumStatMap["NumOfNonLocalStore"] = anno->numOfNonLocalSt;
//     PTNumStatMap["NumOfNonLocalLoad"] = anno->numOfNonLocalLd;
//     PTNumStatMap["NumOfAliasStore"] = anno->numOfAliasSt;
//     PTNumStatMap["NumOfAliasLoad"] = anno->numOfAliasLd;
//     PTNumStatMap["NumOfMHPStore"] = anno->numOfMHPSt;
//     PTNumStatMap["NumOfMHPLoad"] = anno->numOfMHPLd;
//     PTNumStatMap["NumOfAnnotatedStore"] = anno->numOfAnnotatedSt;
//     PTNumStatMap["NumOfAnnotatedLoad"] = anno->numOfAnnotatedLd;
//     timeStatMap["AnnotationTime"] = AnnotationTime;

//     SVFUtil::outs() << "\n****Annotation Statistics****\n";
//     PTAStat::printStat();
// }

