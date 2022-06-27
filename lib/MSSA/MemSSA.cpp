//===- MemSSA.cpp -- Base class of pointer analyses------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * MemSSA.cpp
 *
 *  Created on: Dec 14, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "MSSA/MemPartition.h"
#include "MSSA/MemSSA.h"
#include "Graphs/SVFGStat.h"

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;


double MemSSA::timeOfGeneratingMemRegions = 0;	///< Time for allocating regions
double MemSSA::timeOfCreateMUCHI  = 0;	///< Time for generating mu/chi for load/store/calls
double MemSSA::timeOfInsertingPHI  = 0;	///< Time for inserting phis
double MemSSA::timeOfSSARenaming  = 0;	///< Time for SSA rename

/*!
 * Constructor
 */
MemSSA::MemSSA(BVDataPTAImpl* p, bool ptrOnlyMSSA) : df(nullptr),dt(nullptr)
{
    pta = p;
    assert((pta->getAnalysisTy()!=PointerAnalysis::Default_PTA)
           && "please specify a pointer analysis");

    if (Options::MemPar == MemPartition::Distinct)
        mrGen = new DistinctMRG(pta, ptrOnlyMSSA);
    else if (Options::MemPar == MemPartition::IntraDisjoint)
        mrGen = new IntraDisjointMRG(pta, ptrOnlyMSSA);
    else if (Options::MemPar == MemPartition::InterDisjoint)
        mrGen = new InterDisjointMRG(pta, ptrOnlyMSSA);
    else
        assert(false && "unrecognised memory partition strategy");


    stat = new MemSSAStat(this);

    /// Generate whole program memory regions
    double mrStart = stat->getClk(true);
    mrGen->generateMRs();
    double mrEnd = stat->getClk(true);
    timeOfGeneratingMemRegions = (mrEnd - mrStart)/TIMEINTERVAL;
}

SVFIR* MemSSA::getPAG()
{
    return pta->getPAG();
}

/*!
 * Set DF/DT
 */
void MemSSA::setCurrentDFDT(DominanceFrontier* f, DominatorTree* t)
{
    df = f;
    dt = t;
    usedRegs.clear();
    reg2BBMap.clear();
}

/*!
 * Start building memory SSA
 */
void MemSSA::buildMemSSA(const SVFFunction& fun, DominanceFrontier* f, DominatorTree* t)
{

    assert(!isExtCall(&fun) && "we do not build memory ssa for external functions");

    DBOUT(DMSSA, outs() << "Building Memory SSA for function " << fun.getName()
          << " \n");

    setCurrentDFDT(f,t);

    /// Create mus/chis for loads/stores/calls for memory regions
    double muchiStart = stat->getClk(true);
    createMUCHI(fun);
    double muchiEnd = stat->getClk(true);
    timeOfCreateMUCHI += (muchiEnd - muchiStart)/TIMEINTERVAL;

    /// Insert PHI for memory regions
    double phiStart = stat->getClk(true);
    insertPHI(fun);
    double phiEnd = stat->getClk(true);
    timeOfInsertingPHI += (phiEnd - phiStart)/TIMEINTERVAL;

    /// SSA rename for memory regions
    double renameStart = stat->getClk(true);
    SSARename(fun);
    double renameEnd = stat->getClk(true);
    timeOfSSARenaming += (renameEnd - renameStart)/TIMEINTERVAL;

}

/*!
 * Create mu/chi according to memory regions
 * collect used mrs in usedRegs and construction map from region to BB for prune SSA phi insertion
 */
void MemSSA::createMUCHI(const SVFFunction& fun)
{

    SVFIR* pag = pta->getPAG();

    DBOUT(DMSSA,
          outs() << "\t creating mu chi for function " << fun.getName()
          << "\n");
    // 1. create mu/chi
    //	insert a set of mus for memory regions at each load
    //  inset a set of chis for memory regions at each store

    // 2. find global names (region name before renaming) of each memory region,
    // collect used mrs in usedRegs, and collect its def basic block in reg2BBMap
    // in the form of mu(r) and r = chi (r)
    // a) mu(r):
    // 		if(r \not\in varKills) global = global \cup r
    // b) r = chi(r):
    // 		if(r \not\in varKills) global = global \cup r
    //		varKills = varKills \cup r
    //		block(r) = block(r) \cup bb_{chi}

    /// get all reachable basic blocks from function entry
    /// ignore dead basic blocks
    BBList reachableBBs;
    getFunReachableBBs(fun.getLLVMFun(),getDT(fun),reachableBBs);

    for (BBList::const_iterator iter = reachableBBs.begin(), eiter = reachableBBs.end();
            iter != eiter; ++iter)
    {
        const BasicBlock* bb = *iter;
        varKills.clear();
        for (BasicBlock::const_iterator it = bb->begin(), eit = bb->end();
                it != eit; ++it)
        {
            const Instruction* inst = &*it;
            if(mrGen->hasSVFStmtList(inst))
            {
                SVFStmtList& pagEdgeList = mrGen->getPAGEdgesFromInst(inst);
                for (SVFStmtList::const_iterator bit = pagEdgeList.begin(),
                        ebit = pagEdgeList.end(); bit != ebit; ++bit)
                {
                    const PAGEdge* inst = *bit;
                    if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(inst))
                        AddLoadMU(bb, load, mrGen->getLoadMRSet(load));
                    else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(inst))
                        AddStoreCHI(bb, store, mrGen->getStoreMRSet(store));
                }
            }
            if (isNonInstricCallSite(inst))
            {
                const CallICFGNode* cs = pag->getICFG()->getCallICFGNode(inst);
                if(mrGen->hasRefMRSet(cs))
                    AddCallSiteMU(cs,mrGen->getCallSiteRefMRSet(cs));

                if(mrGen->hasModMRSet(cs))
                    AddCallSiteCHI(cs,mrGen->getCallSiteModMRSet(cs));
            }
        }
    }

    // create entry chi for this function including all memory regions
    // initialize them with version 0 and 1 r_1 = chi (r_0)
    for (MRSet::iterator iter = usedRegs.begin(), eiter = usedRegs.end();
            iter != eiter; ++iter)
    {
        const MemRegion* mr = *iter;
        // initialize mem region version and stack for renaming phase
        mr2CounterMap[mr] = 0;
        mr2VerStackMap[mr].clear();
        ENTRYCHI* chi = new ENTRYCHI(&fun, mr);
        chi->setOpVer(newSSAName(mr,chi));
        chi->setResVer(newSSAName(mr,chi));
        funToEntryChiSetMap[&fun].insert(chi);

        /// if the function does not have a reachable return instruction from function entry
        /// then we won't create return mu for it
        if(functionDoesNotRet(fun.getLLVMFun()) == false)
        {
            RETMU* mu = new RETMU(&fun, mr);
            funToReturnMuSetMap[&fun].insert(mu);
        }

    }

}

/*
 * Insert phi node
 */
void MemSSA::insertPHI(const SVFFunction& fun)
{

    DBOUT(DMSSA,
          outs() << "\t insert phi for function " << fun.getName() << "\n");

    const DominanceFrontier* df = getDF(fun);
    // record whether a phi of mr has already been inserted into the bb.
    BBToMRSetMap bb2MRSetMap;

    // start inserting phi node
    for (MRSet::iterator iter = usedRegs.begin(), eiter = usedRegs.end();
            iter != eiter; ++iter)
    {
        const MemRegion* mr = *iter;

        BBList bbs = reg2BBMap[mr];
        while (!bbs.empty())
        {
            const BasicBlock* bb = bbs.back();
            bbs.pop_back();
            DominanceFrontierBase::const_iterator it = df->find(const_cast<BasicBlock*>(bb));
            if(it == df->end())
            {
                writeWrnMsg("bb not in the dominance frontier map??");
                continue;
            }
            const DominanceFrontierBase::DomSetType& domSet = it->second;
            for (DominanceFrontierBase::DomSetType::const_iterator bit =
                        domSet.begin(); bit != domSet.end(); ++bit)
            {
                const BasicBlock* pbb = *bit;
                // if we never insert this phi node before
                if (0 == bb2MRSetMap[pbb].count(mr))
                {
                    bb2MRSetMap[pbb].insert(mr);
                    // insert phi node
                    AddMSSAPHI(pbb,mr);
                    // continue to insert phi in its iterative dominate frontiers
                    bbs.push_back(pbb);
                }
            }
        }
    }

}

/*!
 * SSA construction algorithm
 */
void MemSSA::SSARename(const SVFFunction& fun)
{

    DBOUT(DMSSA,
          outs() << "\t ssa rename for function " << fun.getName() << "\n");

    SSARenameBB(fun.getLLVMFun()->getEntryBlock());
}

/*!
 * Renaming for each memory regions
 * See the renaming algorithm in book Engineering A Compiler (Figure 9.12)
 */
void MemSSA::SSARenameBB(const BasicBlock& bb)
{

    SVFIR* pag = pta->getPAG();
    // record which mem region needs to pop stack
    MRVector memRegs;

    // rename phi result op
    // for each r = phi (...)
    // 		rewrite r as new name
    if (hasPHISet(&bb))
        RenamePhiRes(getPHISet(&bb),memRegs);


    // process mu and chi
    // for each mu(r)
    // 		rewrite r with top mrver of stack(r)
    // for each r = chi(r')
    // 		rewrite r' with top mrver of stack(r)
    // 		rewrite r with new name

    for (BasicBlock::const_iterator it = bb.begin(), eit = bb.end();
            it != eit; ++it)
    {
        const Instruction* inst = &*it;
        if(mrGen->hasSVFStmtList(inst))
        {
            SVFStmtList& pagEdgeList = mrGen->getPAGEdgesFromInst(inst);
            for(SVFStmtList::const_iterator bit = pagEdgeList.begin(), ebit= pagEdgeList.end();
                    bit!=ebit; ++bit)
            {
                const PAGEdge* inst = *bit;
                if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(inst))
                    RenameMuSet(getMUSet(load));

                else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(inst))
                    RenameChiSet(getCHISet(store),memRegs);

            }
        }
        if (isNonInstricCallSite(inst))
        {
            const CallICFGNode* cs = pag->getICFG()->getCallICFGNode(inst);
            if(mrGen->hasRefMRSet(cs))
                RenameMuSet(getMUSet(cs));

            if(mrGen->hasModMRSet(cs))
                RenameChiSet(getCHISet(cs),memRegs);
        }
        else if(isReturn(inst))
        {
            const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(bb.getParent());
            RenameMuSet(getReturnMuSet(fun));
        }
    }


    // fill phi operands of succ basic blocks
    for (succ_const_iterator sit = succ_begin(&bb), esit = succ_end(&bb);
            sit != esit; ++sit)
    {
        const BasicBlock* succ = *sit;
        u32_t pos = getBBPredecessorPos(&bb, succ);
        if (hasPHISet(succ))
            RenamePhiOps(getPHISet(succ),pos,memRegs);
    }

    // for succ basic block in dominator tree
    const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(bb.getParent());
    DominatorTree* dt = getDT(*fun);
    if(DomTreeNode *dtNode = dt->getNode(const_cast<BasicBlock*>(&bb)))
    {
        for (DomTreeNode::iterator DI = dtNode->begin(), DE = dtNode->end();
                DI != DE; ++DI)
        {
            SSARenameBB(*((*DI)->getBlock()));
        }
    }
    // for each r = chi(..), and r = phi(..)
    // 		pop ver stack(r)
    while (!memRegs.empty())
    {
        const MemRegion* mr = memRegs.back();
        memRegs.pop_back();
        mr2VerStackMap[mr].pop_back();
    }

}

MRVer* MemSSA::newSSAName(const MemRegion* mr, MSSADEF* def)
{
    assert(0 != mr2CounterMap.count(mr)
           && "did not find initial version in map? ");
    assert(0 != mr2VerStackMap.count(mr)
           && "did not find initial stack in map? ");

    MRVERSION version = mr2CounterMap[mr];
    mr2CounterMap[mr] = version + 1;
    MRVer* mrVer = new MRVer(mr, version, def);
    mr2VerStackMap[mr].push_back(mrVer);
    return mrVer;
}

/*!
 * Clean up memory
 */
void MemSSA::destroy()
{

    for (LoadToMUSetMap::iterator iter = load2MuSetMap.begin(), eiter =
                load2MuSetMap.end(); iter != eiter; ++iter)
    {
        for (MUSet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (StoreToChiSetMap::iterator iter = store2ChiSetMap.begin(), eiter =
                store2ChiSetMap.end(); iter != eiter; ++iter)
    {
        for (CHISet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (CallSiteToMUSetMap::iterator iter = callsiteToMuSetMap.begin(),
            eiter = callsiteToMuSetMap.end(); iter != eiter; ++iter)
    {
        for (MUSet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (CallSiteToCHISetMap::iterator iter = callsiteToChiSetMap.begin(),
            eiter = callsiteToChiSetMap.end(); iter != eiter; ++iter)
    {
        for (CHISet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (FunToEntryChiSetMap::iterator iter = funToEntryChiSetMap.begin(),
            eiter = funToEntryChiSetMap.end(); iter != eiter; ++iter)
    {
        for (CHISet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (FunToReturnMuSetMap::iterator iter = funToReturnMuSetMap.begin(),
            eiter = funToReturnMuSetMap.end(); iter != eiter; ++iter)
    {
        for (MUSet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    for (BBToPhiSetMap::iterator iter = bb2PhiSetMap.begin(), eiter =
                bb2PhiSetMap.end(); iter != eiter; ++iter)
    {
        for (PHISet::iterator it = iter->second.begin(), eit =
                    iter->second.end(); it != eit; ++it)
        {
            delete *it;
        }
    }

    delete mrGen;
    mrGen = nullptr;
    delete stat;
    stat = nullptr;
    pta = nullptr;
}

/*!
 * Perform statistics
 */
void MemSSA::performStat()
{
    if(pta->printStat())
        stat->performStat();
}

/*!
 * Get loadMU numbers
 */
u32_t MemSSA::getLoadMuNum() const
{
    u32_t num = 0;
    LoadToMUSetMap::const_iterator it = load2MuSetMap.begin();
    LoadToMUSetMap::const_iterator eit = load2MuSetMap.end();
    for (; it != eit; it++)
    {
        const MUSet & muSet = it->second;
        num+= muSet.size();
    }
    return num;
}


/*!
 * Get StoreCHI numbers
 */
u32_t MemSSA::getStoreChiNum() const
{
    u32_t num = 0;
    StoreToChiSetMap::const_iterator it = store2ChiSetMap.begin();
    StoreToChiSetMap::const_iterator eit = store2ChiSetMap.end();
    for (; it != eit; it++)
    {
        const CHISet& chiSet = it->second;
        num += chiSet.size();
    }
    return num;
}


/*!
 * Get EntryCHI numbers
 */
u32_t MemSSA::getFunEntryChiNum() const
{
    u32_t num = 0;
    FunToEntryChiSetMap::const_iterator it = funToEntryChiSetMap.begin();
    FunToEntryChiSetMap::const_iterator eit = funToEntryChiSetMap.end();
    for (; it != eit; it++)
    {
        const CHISet& chiSet = it->second;
        num += chiSet.size();
    }
    return num;
}


/*!
 * Get RetMU numbers
 */
u32_t MemSSA::getFunRetMuNum() const
{
    u32_t num = 0;
    FunToReturnMuSetMap::const_iterator it = funToReturnMuSetMap.begin();
    FunToReturnMuSetMap::const_iterator eit = funToReturnMuSetMap.end();
    for (; it != eit; it++)
    {
        const MUSet & muSet = it->second;
        num+= muSet.size();
    }
    return num;
}


/*!
 * Get CallMU numbers
 */
u32_t MemSSA::getCallSiteMuNum() const
{
    u32_t num = 0;
    CallSiteToMUSetMap::const_iterator it = callsiteToMuSetMap.begin();
    CallSiteToMUSetMap::const_iterator eit = callsiteToMuSetMap.end();
    for (; it != eit; it++)
    {
        const MUSet & muSet = it->second;
        num+= muSet.size();
    }
    return num;
}


/*!
 * Get CallCHI numbers
 */
u32_t MemSSA::getCallSiteChiNum() const
{
    u32_t num = 0;
    CallSiteToCHISetMap::const_iterator it = callsiteToChiSetMap.begin();
    CallSiteToCHISetMap::const_iterator eit = callsiteToChiSetMap.end();
    for (; it != eit; it++)
    {
        const CHISet & chiSet = it->second;
        num+= chiSet.size();
    }
    return num;
}


/*!
 * Get PHI numbers
 */
u32_t MemSSA::getBBPhiNum() const
{
    u32_t num = 0;
    BBToPhiSetMap::const_iterator it = bb2PhiSetMap.begin();
    BBToPhiSetMap::const_iterator eit = bb2PhiSetMap.end();
    for (; it != eit; it++)
    {
        const PHISet & phiSet = it->second;
        num+= phiSet.size();
    }
    return num;
}

/*!
 * Print SSA
 */
void MemSSA::dumpMSSA(OutStream& Out)
{
    SVFIR* pag = pta->getPAG();

    for (SVFModule::iterator fit = pta->getModule()->begin(), efit = pta->getModule()->end();
            fit != efit; ++fit)
    {
        const SVFFunction* fun = *fit;
        if(Options::MSSAFun!="" && Options::MSSAFun!=fun->getName())
            continue;

        Out << "==========FUNCTION: " << fun->getName() << "==========\n";
        // dump function entry chi nodes
        if (hasFuncEntryChi(fun))
        {
            CHISet & entry_chis = getFuncEntryChiSet(fun);
            for (CHISet::iterator chi_it = entry_chis.begin(); chi_it != entry_chis.end(); chi_it++)
            {
                (*chi_it)->dump();
            }
        }

        for (Function::iterator bit = fun->getLLVMFun()->begin(), ebit = fun->getLLVMFun()->end();
                bit != ebit; ++bit)
        {
            BasicBlock& bb = *bit;
            if (bb.hasName())
                Out << bb.getName().str() << "\n";
            PHISet& phiSet = getPHISet(&bb);
            for(PHISet::iterator pi = phiSet.begin(), epi = phiSet.end(); pi !=epi; ++pi)
            {
                (*pi)->dump();
            }

            bool last_is_chi = false;
            for (BasicBlock::iterator it = bb.begin(), eit = bb.end();
                    it != eit; ++it)
            {
                Instruction& inst = *it;
                bool isAppCall = isNonInstricCallSite(&inst) && !isExtCall(&inst);
                if (isAppCall || isHeapAllocExtCall(&inst))
                {
                    const CallICFGNode* cs = pag->getICFG()->getCallICFGNode(&inst);
                    if(hasMU(cs))
                    {
                        if (!last_is_chi)
                        {
                            Out << "\n";
                        }
                        for (MUSet::iterator mit = getMUSet(cs).begin(), emit = getMUSet(cs).end();
                                mit != emit; ++mit)
                        {
                            (*mit)->dump();
                        }
                    }

                    Out << SVFUtil::value2String(&inst) << "\n";

                    if(hasCHI(cs))
                    {
                        for (CHISet::iterator cit = getCHISet(cs).begin(), ecit = getCHISet(cs).end();
                                cit != ecit; ++cit)
                        {
                            (*cit)->dump();
                        }
                        Out << "\n";
                        last_is_chi = true;
                    }
                    else
                        last_is_chi = false;
                }
                else
                {
                    bool dump_preamble = false;
                    SVFStmtList& pagEdgeList = mrGen->getPAGEdgesFromInst(&inst);
                    for(SVFStmtList::const_iterator bit = pagEdgeList.begin(), ebit= pagEdgeList.end();
                            bit!=ebit; ++bit)
                    {
                        const PAGEdge* edge = *bit;
                        if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(edge))
                        {
                            MUSet& muSet = getMUSet(load);
                            for(MUSet::iterator it = muSet.begin(), eit = muSet.end(); it!=eit; ++it)
                            {
                                if (!dump_preamble && !last_is_chi)
                                {
                                    Out << "\n";
                                    dump_preamble = true;
                                }
                                (*it)->dump();
                            }
                        }
                    }

                    Out << SVFUtil::value2String(&inst) << "\n";

                    bool has_chi = false;
                    for(SVFStmtList::const_iterator bit = pagEdgeList.begin(), ebit= pagEdgeList.end();
                            bit!=ebit; ++bit)
                    {
                        const PAGEdge* edge = *bit;
                        if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(edge))
                        {
                            CHISet& chiSet = getCHISet(store);
                            for(CHISet::iterator it = chiSet.begin(), eit = chiSet.end(); it!=eit; ++it)
                            {
                                has_chi = true;
                                (*it)->dump();
                            }
                        }
                    }
                    if (has_chi)
                    {
                        Out << "\n";
                        last_is_chi = true;
                    }
                    else
                        last_is_chi = false;
                }
            }
        }

        // dump return mu nodes
        if (hasReturnMu(fun))
        {
            MUSet & return_mus = getReturnMuSet(fun);
            for (MUSet::iterator mu_it = return_mus.begin(); mu_it != return_mus.end(); mu_it++)
            {
                (*mu_it)->dump();
            }
        }
    }
}
