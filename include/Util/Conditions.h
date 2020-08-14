//===- Conditions.h -- Context/Path BDD conditions----------------------------//
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
 * BitVectorCond.h
 *
 *  Created on: 31/10/2013
 *      Author: yesen
 */

#ifndef BITVECTORCOND_H_
#define BITVECTORCOND_H_

#include "Util/BasicTypes.h"
#include <stdio.h>
#include "CUDD/cuddInt.h"

namespace SVF
{

/**
 * Using Cudd as conditions.
 */
class BddCondManager
{
public:

    /// Constructor
    BddCondManager()
    {
        m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    }

    /// Destructor
    ~BddCondManager()
    {
        Cudd_Quit(m_bdd_mgr);
    }

    DdNode* Cudd_bdd(u32_t i)
    {
        return Cudd_bddIthVar(m_bdd_mgr, i);
    }
    inline unsigned BddVarNum()
    {
        return Cudd_ReadSize(m_bdd_mgr);
    }

    inline DdNode* getTrueCond() const
    {
        return BddOne();
    }
    inline DdNode* getFalseCond() const
    {
        return BddZero();
    }

    inline u32_t getBDDMemUsage()
    {
        return Cudd_ReadMemoryInUse(m_bdd_mgr);
    }
    inline u32_t getCondNumber()
    {
        return Cudd_ReadNodeCount(m_bdd_mgr);
    }
    inline u32_t getMaxLiveCondNumber()
    {
        return Cudd_ReadPeakLiveNodeCount(m_bdd_mgr);
    }
    inline void markForRelease(DdNode* cond)
    {
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
    void dump(DdNode* lhs, raw_ostream & O);
    std::string dumpStr(DdNode* lhs) const;
    /// print minterms and debug information for the Ddnode
    inline void printMinterms(DdNode* d)
    {
        Cudd_PrintMinterm(m_bdd_mgr,d);
    }
    inline void printDbg(DdNode* d)
    {
        Cudd_PrintDebug(m_bdd_mgr,d,0,3);
    }
private:
    inline DdNode* BddOne() const
    {
        return Cudd_ReadOne(m_bdd_mgr);
    }
    inline DdNode* BddZero() const
    {
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    DdManager *m_bdd_mgr;
};

} // End namespace SVF

#endif /* BITVECTORCOND_H_ */
