//===- MemSSA.h -- Memory SSA-------------------------------------------------//
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
 * MemorySSAPass.h
 *
 *  Created on: Jul 14, 2013
 *      Author: Yulei Sui
 */

#ifndef MEMORYSSAPASS_H_
#define MEMORYSSAPASS_H_

#include "MSSA/MemRegion.h"
#include "MSSA/MSSAMuChi.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/DenseMap.h>			// for dense map
#include <llvm/Analysis/DominanceFrontier.h>

#include <vector>

class PointerAnalysis;
class MemSSAStat;
/*
 * Memory SSA implementation on top of partial SSA
 */
class MemSSA {

public:

    /// define condition here changes needed if we add new type
    typedef MemRegion::Condition Condition;
    typedef MSSAMU<Condition> MU;
    typedef RetMU<Condition> RETMU;
    typedef LoadMU<Condition> LOADMU;
    typedef CallMU<Condition> CALLMU;
    typedef MSSACHI<Condition> CHI;
    typedef EntryCHI<Condition> ENTRYCHI;
    typedef StoreCHI<Condition> STORECHI;
    typedef CallCHI<Condition> CALLCHI;
    typedef MSSAPHI<Condition> PHI;
    typedef MSSADEF MDEF;

    typedef std::set<MU*> MUSet;
    typedef std::set<CHI*> CHISet;
    typedef std::set<PHI*> PHISet;

    ///Define mem region set
    typedef MRGenerator::MRSet MRSet;
    typedef std::vector<const MemRegion*> MRVector;
    /// Map loads/stores to its mem regions,
    /// TODO:visitAtomicCmpXchgInst, visitAtomicRMWInst??
    //@{
    typedef llvm::DenseMap<const LoadPE*, MUSet> LoadToMUSetMap;
    typedef llvm::DenseMap<const StorePE*, CHISet> StoreToChiSetMap;
    typedef std::map<llvm::CallSite, MUSet> CallSiteToMUSetMap;
    typedef std::map<llvm::CallSite, CHISet> CallSiteToCHISetMap;
    typedef llvm::DenseMap<const llvm::BasicBlock*, PHISet> BBToPhiSetMap;
    //@}

    /// Map from fun to its entry chi set and return mu set
    typedef llvm::DenseMap<const llvm::Function*, CHISet> FunToEntryChiSetMap;
    typedef llvm::DenseMap<const llvm::Function*, MUSet> FunToReturnMuSetMap;

    /// For phi insertion
    //@{
    typedef std::vector<const llvm::BasicBlock*> BBList;
    typedef llvm::DenseMap<const llvm::BasicBlock*, MRSet> BBToMRSetMap;
    typedef llvm::DenseMap<const MemRegion*, BBList> MemRegToBBsMap;
    //@}

    /// For SSA renaming
    typedef llvm::DenseMap<const MemRegion*, std::vector<MRVer*> > MemRegToVerStackMap;
    typedef llvm::DenseMap<const MemRegion*, VERSION> MemRegToCounterMap;

    /// PAG edge list
    typedef PAG::PAGEdgeList PAGEdgeList;

    /// Statistics
    //@{
    static double timeOfGeneratingMemRegions;	///< Time for allocating regions
    static double timeOfCreateMUCHI;	///< Time for generating mu/chi for load/store/calls
    static double timeOfInsertingPHI;	///< Time for inserting phis
    static double timeOfSSARenaming;	///< Time for SSA rename
    //@}

protected:
    BVDataPTAImpl* pta;
    MRGenerator* mrGen;
    llvm::DominanceFrontier* df;
    llvm::DominatorTree* dt;
    MemSSAStat* stat;

    /// Create mu chi for candidate regions in a function
    virtual void createMUCHI(const llvm::Function& fun);
    /// Insert phi for candidate regions in a fucntion
    virtual void insertPHI(const llvm::Function& fun);
    /// SSA rename for a function
    virtual void SSARename(const llvm::Function& fun);
    /// SSA rename for a basic block
    virtual void SSARenameBB(const llvm::BasicBlock& bb);
private:
    LoadToMUSetMap load2MuSetMap;
    StoreToChiSetMap store2ChiSetMap;
    CallSiteToMUSetMap callsiteToMuSetMap;
    CallSiteToCHISetMap callsiteToChiSetMap;
    BBToPhiSetMap bb2PhiSetMap;

    FunToEntryChiSetMap funToEntryChiSetMap;
    FunToReturnMuSetMap funToReturnMuSetMap;

    MemRegToVerStackMap mr2VerStackMap;
    MemRegToCounterMap mr2CounterMap;

    /// The following three set are used for prune SSA phi insertion
    // (see algorithm in book Engineering A Compiler section 9.3)
    ///@{
    /// Collects used memory regions
    MRSet usedRegs;
    /// Maps memory region to its basic block
    MemRegToBBsMap reg2BBMap;
    /// Collect memory regions whose definition killed
    MRSet varKills;
    //@}

    /// Release the memory
    void destroy();

    /// Get previous BB index for phi
    u32_t getPreBBIndex(const llvm::BasicBlock* bb,
                        const llvm::BasicBlock* succb);

    /// Get a new SSA name of a memory region
    MRVer* newSSAName(const MemRegion* mr, MSSADEF* def);

    /// Get the last version of the SSA ver of memory region
    inline MRVer* getTopStackVer(const MemRegion* mr) {
        std::vector<MRVer*> &stack = mr2VerStackMap[mr];
        assert(!stack.empty() && "stack is empty!!");
        return stack.back();
    }

    /// Collect region uses and region defs according to mus/chis, in order to insert phis
    //@{
    inline void collectRegUses(const MemRegion* mr) {
        if (0 == varKills.count(mr))
            usedRegs.insert(mr);
    }
    inline void collectRegDefs(const llvm::BasicBlock* bb, const MemRegion* mr) {
        varKills.insert(mr);
        reg2BBMap[mr].push_back(bb);
    }
    //@}

    /// Add methods for mus/chis/phis
    //@{
    inline void AddLoadMU(const llvm::BasicBlock* bb, const LoadPE* load, const MRSet& mrSet) {
        for (MRSet::iterator iter = mrSet.begin(), eiter = mrSet.end(); iter != eiter; ++iter)
            AddLoadMU(bb,load,*iter);
    }
    inline void AddStoreCHI(const llvm::BasicBlock* bb, const StorePE* store, const MRSet& mrSet) {
        for (MRSet::iterator iter = mrSet.begin(), eiter = mrSet.end(); iter != eiter; ++iter)
            AddStoreCHI(bb,store,*iter);
    }
    inline void AddCallSiteMU(llvm::CallSite cs,  const MRSet& mrSet) {
        for (MRSet::iterator iter = mrSet.begin(), eiter = mrSet.end(); iter != eiter; ++iter)
            AddCallSiteMU(cs,*iter);
    }
    inline void AddCallSiteCHI(llvm::CallSite cs,  const MRSet& mrSet) {
        for (MRSet::iterator iter = mrSet.begin(), eiter = mrSet.end(); iter != eiter; ++iter)
            AddCallSiteCHI(cs,*iter);
    }
    inline void AddMSSAPHI(const llvm::BasicBlock* bb, const MRSet& mrSet) {
        for (MRSet::iterator iter = mrSet.begin(), eiter = mrSet.end(); iter != eiter; ++iter)
            AddMSSAPHI(bb,*iter);
    }
    inline void AddLoadMU(const llvm::BasicBlock* bb, const LoadPE* load, const MemRegion* mr) {
        LOADMU* mu = new LOADMU(bb,load, mr);
        load2MuSetMap[load].insert(mu);
        collectRegUses(mr);
    }
    inline void AddStoreCHI(const llvm::BasicBlock* bb, const StorePE* store, const MemRegion* mr) {
        STORECHI* chi = new STORECHI(bb,store, mr);
        store2ChiSetMap[store].insert(chi);
        collectRegUses(mr);
        collectRegDefs(bb,mr);
    }
    inline void AddCallSiteMU(llvm::CallSite cs, const MemRegion* mr) {
        CALLMU* mu = new CALLMU(cs, mr);
        callsiteToMuSetMap[cs].insert(mu);
        collectRegUses(mr);
    }
    inline void AddCallSiteCHI(llvm::CallSite cs, const MemRegion* mr) {
        CALLCHI* chi = new CALLCHI(cs, mr);
        callsiteToChiSetMap[cs].insert(chi);
        collectRegUses(mr);
        collectRegDefs(chi->getBasicBlock(),mr);
    }
    inline void AddMSSAPHI(const llvm::BasicBlock* bb, const MemRegion* mr) {
        bb2PhiSetMap[bb].insert(new PHI(bb, mr));
    }
    //@}

    /// Rename mus, chis and phis
    //@{
    /// Rename mu set
    inline void RenameMuSet(const MUSet& muSet) {
        for (MUSet::iterator mit = muSet.begin(), emit = muSet.end();
                mit != emit; ++mit) {
            MU* mu = (*mit);
            mu->setVer(getTopStackVer(mu->getMR()));
        }
    }

    /// Rename chi set
    inline void RenameChiSet(const CHISet& chiSet, MRVector& memRegs) {
        for (CHISet::iterator cit = chiSet.begin(), ecit = chiSet.end();
                cit != ecit; ++cit) {
            CHI* chi = (*cit);
            chi->setOpVer(getTopStackVer(chi->getMR()));
            chi->setResVer(newSSAName(chi->getMR(),chi));
            memRegs.push_back(chi->getMR());
        }
    }

    /// Rename result (LHS) of phis
    inline void RenamePhiRes(const PHISet& phiSet, MRVector& memRegs) {
        for (PHISet::iterator iter = phiSet.begin(), eiter = phiSet.end();
                iter != eiter; ++iter) {
            PHI* phi = *iter;
            phi->setResVer(newSSAName(phi->getMR(),phi));
            memRegs.push_back(phi->getMR());
        }
    }

    /// Rename operands (RHS) of phis
    inline void RenamePhiOps(const PHISet& phiSet, u32_t pos, MRVector& memRegs) {
        for (PHISet::iterator iter = phiSet.begin(), eiter = phiSet.end();
                iter != eiter; ++iter) {
            PHI* phi = *iter;
            phi->setOpVer(getTopStackVer(phi->getMR()), pos);
        }
    }

    //@}
    /// Get/set methods for dominace frontier/tree
    //@{
    llvm::DominanceFrontier* getDF(const llvm::Function& fn) {
        return df;
    }
    llvm::DominatorTree* getDT(const llvm::Function& fn) {
        return dt;
    }
    void setCurrentDFDT(llvm::DominanceFrontier* f, llvm::DominatorTree* t);
    //@}

public:
    /// Constructor
    MemSSA(BVDataPTAImpl* p);

    /// Destructor
    virtual ~MemSSA() {
        destroy();
    }
    /// Return PAG
    inline PAG* getPAG() {
        return pta->getPAG();
    }
    /// Return PTA
    inline BVDataPTAImpl* getPTA() const {
        return pta;
    }
    /// Return MRGenerator
    inline MRGenerator* getMRGenerator() {
        return mrGen;
    }
    /// We start from here
    virtual void buildMemSSA(const llvm::Function& fun,llvm::DominanceFrontier*, llvm::DominatorTree*);

    /// Perform statistics
    void performStat();

    /// Has mu/chi methods
    //@{
    inline bool hasMU(const PAGEdge* inst) const {
        if (const LoadPE* load = llvm::dyn_cast<LoadPE>(inst)) {
            assert(0 != load2MuSetMap.count(load)
                   && "not associated with mem region!");
            return true;
        } else
            return false;
    }
    inline bool hasCHI(const PAGEdge* inst) const {
        if (const StorePE* store = llvm::dyn_cast<StorePE>(
                                       inst)) {
            assert(0 != store2ChiSetMap.count(store)
                   && "not associated with mem region!");
            return true;
        } else
            return false;
    }
    inline bool hasMU(llvm::CallSite cs) const {
        return callsiteToMuSetMap.find(cs)!=callsiteToMuSetMap.end();
    }
    inline bool hasCHI(llvm::CallSite cs) const {
        return callsiteToChiSetMap.find(cs)!=callsiteToChiSetMap.end();
    }
    //@}

    /// Has function entry chi or return mu
    //@{
    inline bool hasFuncEntryChi(llvm::Function * fun) const {
        return (funToEntryChiSetMap.find(fun) != funToEntryChiSetMap.end());
    }
    inline bool hasReturnMu(llvm::Function * fun) const {
        return (funToReturnMuSetMap.find(fun) != funToReturnMuSetMap.end());
    }

    inline CHISet& getFuncEntryChiSet(const llvm::Function * fun) {
        return funToEntryChiSetMap[fun];
    }
    inline MUSet& getReturnMuSet(const llvm::Function * fun) {
        return funToReturnMuSetMap[fun];
    }
    //@}

    /// Get methods of mu/chi/phi
    //@{
    inline MUSet& getMUSet(const LoadPE* ld) {
        return load2MuSetMap[ld];
    }
    inline CHISet& getCHISet(const StorePE* st) {
        return store2ChiSetMap[st];
    }
    inline MUSet& getMUSet(llvm::CallSite cs) {
        return callsiteToMuSetMap[cs];
    }
    inline CHISet& getCHISet(llvm::CallSite cs) {
        return callsiteToChiSetMap[cs];
    }
    inline PHISet& getPHISet(const llvm::BasicBlock* bb) {
        return bb2PhiSetMap[bb];
    }
    inline bool hasPHISet(const llvm::BasicBlock* bb) const {
        return bb2PhiSetMap.find(bb)!=bb2PhiSetMap.end();
    }
    inline LoadToMUSetMap& getLoadToMUSetMap() {
        return load2MuSetMap;
    }
    inline StoreToChiSetMap& getStoreToChiSetMap() {
        return store2ChiSetMap;
    }
    inline FunToReturnMuSetMap& getFunToRetMuSetMap() {
        return funToReturnMuSetMap;
    }
    inline FunToEntryChiSetMap& getFunToEntryChiSetMap() {
        return funToEntryChiSetMap;
    }
    inline CallSiteToMUSetMap& getCallSiteToMuSetMap() {
        return callsiteToMuSetMap;
    }
    inline CallSiteToCHISetMap& getCallSiteToChiSetMap() {
        return callsiteToChiSetMap;
    }
    inline BBToPhiSetMap& getBBToPhiSetMap() {
        return bb2PhiSetMap;
    }
    //@}

    /// Stat methods
    //@{
    u32_t getLoadMuNum() const;
    u32_t getStoreChiNum() const;
    u32_t getFunEntryChiNum() const;
    u32_t getFunRetMuNum() const;
    u32_t getCallSiteMuNum() const;
    u32_t getCallSiteChiNum() const;
    u32_t getBBPhiNum() const;
    //@}

    /// Print Memory SSA
    void dumpMSSA(llvm::raw_ostream & Out = llvm::outs());
};

#endif /* MEMORYSSAPASS_H_ */
