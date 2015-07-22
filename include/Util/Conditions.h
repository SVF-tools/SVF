//===- Conditions.h -- Context/Path BDD conditions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * BitVectorCond.h
 *
 *  Created on: 31/10/2013
 *      Author: yesen
 */

#ifndef BITVECTORCOND_H_
#define BITVECTORCOND_H_

#include <stdio.h>
#include "Util/BasicTypes.h"
#include "CUDD/cuddInt.h"

/**
 * Using Cudd as conditions.
 */
class BddCondManager {
public:
    typedef std::map<unsigned,DdNode*> IndexToDDNodeMap;

    /// Constructor
    BddCondManager() {
        m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    }

    /// Destructor
    ~BddCondManager() {
        delete m_bdd_mgr;
    }
    /// Create new BDD condition
    inline DdNode* createNewCond(unsigned i) {
        assert(indexToDDNodeMap.find(i)==indexToDDNodeMap.end() && "This should be fresh index to create new BDD");
        DdNode* d = Cudd_bddIthVar(m_bdd_mgr, i);
        indexToDDNodeMap[i] = d;
        return  d;
    }
    /// Get existing BDD condition
    inline DdNode* getCond(unsigned i) const {
        IndexToDDNodeMap::const_iterator it = indexToDDNodeMap.find(i);
        assert(it!=indexToDDNodeMap.end() && "condition not found!");
        return it->second;
    }
    inline unsigned BddVarNum() {
        return Cudd_ReadSize(m_bdd_mgr);
    }

    inline DdNode* getTrueCond() const {
        return BddOne();
    }
    inline DdNode* getFalseCond() const {
        return BddZero();
    }

    inline u32_t getBDDMemUsage() {
        return Cudd_ReadMemoryInUse(m_bdd_mgr);
    }
    inline u32_t getCondNumber() {
        return Cudd_ReadNodeCount(m_bdd_mgr);
    }
    inline u32_t getMaxLiveCondNumber() {
        return Cudd_ReadPeakLiveNodeCount(m_bdd_mgr);
    }
    inline void markForRelease(DdNode* cond) {
        Cudd_RecursiveDeref(m_bdd_mgr,cond);
    }
    /// Operations on conditions.
    //@{
    DdNode* AND(DdNode* lhs, DdNode* rhs);
    DdNode* OR(DdNode* lhs, DdNode* rhs);
    DdNode* NEG(DdNode* lhs);
    //@}

    /**
     * Utilities for dumping conditions. These methods use global functions from CUDD
     * package and they can be removed outside this class scope to be used by others.
     */
    void ddClearFlag(DdNode * f) const;
    void BddSupportStep( DdNode * f,  NodeBS &support) const;
    void BddSupport( DdNode * f,  NodeBS &support) const;
    void dump(DdNode* lhs, llvm::raw_ostream & O = llvm::outs());
    std::string dumpStr(DdNode* lhs) const;
    /// print minterms and debug information for the Ddnode
    inline void printMinterms(DdNode* d) {
        Cudd_PrintMinterm(m_bdd_mgr,d);
    }
    inline void printDbg(DdNode* d) {
        Cudd_PrintDebug(m_bdd_mgr,d,0,3);
    }
private:
    inline DdNode* BddOne() const	{
        return Cudd_ReadOne(m_bdd_mgr);
    }
    inline DdNode* BddZero() const	{
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    DdManager *m_bdd_mgr;
    IndexToDDNodeMap indexToDDNodeMap;
};

#endif /* BITVECTORCOND_H_ */
