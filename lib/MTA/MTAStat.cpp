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
#include "MTA/MTAAnnotator.h"
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
        const Function* spawnee = SVFUtil::dyn_cast<Function>(tcg->getThreadAPI()->getForkedFun((*it)->getCallSite()));
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

    std::cout << "\n****Thread Call Graph Statistics****\n";
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
    std::cout << "\n****Thread Creation Tree Statistics****\n";
    PTAStat::printStat();
}

/*!
 * Iterate every memory access pairs
 * write vs read
 * write vs write
 */
void MTAStat::performMHPPairStat(MHP* mhp, LockAnalysis* lsa)
{

    if(Options::AllPairMHP)
    {
        InstSet instSet1;
        InstSet instSet2;
        SVFModule* mod = mhp->getTCT()->getSVFModule();
        for (SVFModule::iterator F = mod->begin(), E = mod->end(); F != E; ++F)
        {
            const SVFFunction* fun = (*F);
            const Function* llvmfun = fun->getLLVMFun();
            if(SVFUtil::isExtCall(fun))
                continue;
            if(!mhp->isConnectedfromMain(llvmfun))
                continue;
            for (const_inst_iterator II = inst_begin(llvmfun), E = inst_end(llvmfun); II != E; ++II)
            {
                const Instruction *inst = &*II;
                if(SVFUtil::isa<LoadInst>(inst))
                {
                    instSet1.insert(inst);
                }
                else if(SVFUtil::isa<StoreInst>(inst))
                {
                    instSet1.insert(inst);
                    instSet2.insert(inst);
                }
            }
        }


        for(InstSet::const_iterator it1 = instSet1.begin(), eit1 = instSet1.end(); it1!=eit1; ++it1)
        {
            for(InstSet::const_iterator it2 = instSet2.begin(), eit2 = instSet2.end(); it2!=eit2; ++it2)
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

    std::cout << "\n****MHP Stmt Pairs Statistics****\n";
    PTAStat::printStat();
}

void MTAStat::performAnnotationStat(MTAAnnotator* anno)
{

    PTNumStatMap.clear();
    timeStatMap.clear();
    PTNumStatMap["TotalNumOfStore"] = anno->numOfAllSt;
    PTNumStatMap["TotalNumOfLoad"] = anno->numOfAllLd;
    PTNumStatMap["NumOfNonLocalStore"] = anno->numOfNonLocalSt;
    PTNumStatMap["NumOfNonLocalLoad"] = anno->numOfNonLocalLd;
    PTNumStatMap["NumOfAliasStore"] = anno->numOfAliasSt;
    PTNumStatMap["NumOfAliasLoad"] = anno->numOfAliasLd;
    PTNumStatMap["NumOfMHPStore"] = anno->numOfMHPSt;
    PTNumStatMap["NumOfMHPLoad"] = anno->numOfMHPLd;
    PTNumStatMap["NumOfAnnotatedStore"] = anno->numOfAnnotatedSt;
    PTNumStatMap["NumOfAnnotatedLoad"] = anno->numOfAnnotatedLd;
    timeStatMap["AnnotationTime"] = AnnotationTime;

    std::cout << "\n****Annotation Statistics****\n";
    PTAStat::printStat();
}

