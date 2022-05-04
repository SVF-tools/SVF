//===- BDDExpr.h -- Context/Path BDD conditions----------------------------//
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
 * Condition.h
 *
 *  Created on: Oct 19, 2021
 *      Author: Yulei and Xiao
 */

#ifndef BITVECTORCOND_H_
#define BITVECTORCOND_H_

#include "Util/BasicTypes.h"
#include <cstdio>

#include "CUDD/cuddInt.h"
#include "z3++.h"

namespace SVF
{

class BDDExprManager
{
public:
    typedef DdNode BDDExpr;
    typedef Map<u32_t, BDDExpr *> IndexToBDDExpr;
private:
    static BDDExprManager *bddExprMgr;
    IndexToBDDExpr indexToBddCondMap;
    DdManager *m_bdd_mgr;

    inline BDDExpr *BddOne() const
    {
        return Cudd_ReadOne(m_bdd_mgr);
    }

    inline BDDExpr *BddZero() const
    {
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    /// Constructor
    BDDExprManager();

public:
    typedef Map<const BDDExpr *, const Instruction *> CondToTermInstMap;    // map a condition to its branch instruction
    static u32_t totalCondNum; // a counter for fresh condition
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static BDDExprManager *getBDDExprMgr();

    static void releaseBDDExprMgr()
    {
        delete bddExprMgr;
        bddExprMgr = nullptr;
    }
    //@}

    /// Destructor
    virtual ~BDDExprManager();

    BDDExpr *createCond(u32_t i)
    {
        assert(indexToBddCondMap.find(i) == indexToBddCondMap.end() &&
               "This should be fresh index to create new BDD");
        BDDExpr *bddCond = Cudd_bddIthVar(m_bdd_mgr, i);
        return indexToBddCondMap.emplace(i, bddCond).first->second;
    }

    /// Create a fresh condition to encode each program branch
    virtual BDDExpr *createFreshBranchCond(const Instruction *inst);

    /// Return the number of condition expressions
    virtual inline u32_t getCondNumber()
    {
        return Cudd_ReadNodeCount(m_bdd_mgr);
    }


    /// Return the unique true condition
    inline BDDExpr *getTrueCond() const
    {
        return trueCond;
    }

    /// Return the unique false condition
    inline BDDExpr *getFalseCond() const
    {
        return falseCond;
    }

    /// Operations on conditions.
    //@{
    virtual BDDExpr *AND(BDDExpr *lhs, BDDExpr *rhs);

    virtual BDDExpr *OR(BDDExpr *lhs, BDDExpr *rhs);

    virtual BDDExpr *NEG(BDDExpr *lhs);
    //@}

    virtual bool isNegCond(const BDDExpr *cond)
    {
        return false;
    }

    /// Whether the condition is satisfiable
    virtual bool isSatisfiable(const BDDExpr *cond)
    {
        return cond != getFalseCond();
    }

    /// Whether lhs and rhs are equivalent branch conditions
    virtual bool isEquivalentBranchCond(const BDDExpr *lhs, const BDDExpr *rhs)
    {
        return lhs == rhs;
    }

    /// Whether **All Paths** are reachable
    bool isAllPathReachable(const BDDExpr *e);

    /// Get condition using condition id (z3 ast id)
    virtual BDDExpr *getCond(u32_t id)
    {
        auto it = indexToBddCondMap.find(id);
        assert(it != indexToBddCondMap.end() && "condition not found!");
        return it->second;
    }

    /// Get/Set llvm conditional expression
    //{@
    inline const Instruction *getCondInst(const BDDExpr *cond) const
    {
        CondToTermInstMap::const_iterator it = condToInstMap.find(cond);
        assert(it != condToInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    inline void setCondInst(const BDDExpr *cond, const Instruction *inst)
    {
        assert(condToInstMap.find(cond) == condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
    }
    //@}

    /**
     * Utilities for dumping conditions. These methods use global functions from CUDD
     * package and they can be removed outside this class scope to be used by others.
     */
    void ddClearFlag(BDDExpr *f) const;

    void BddSupportStep(BDDExpr *f, NodeBS &support) const;

    void extractSubConds(const BDDExpr *f, NodeBS &support) const;

    void dump(const BDDExpr *lhs, OutStream &O);

    std::string dumpStr(const BDDExpr *e) const;

    inline std::string getMemUsage()
    {
        return std::to_string(Cudd_ReadMemoryInUse(m_bdd_mgr));
    }

protected:
    static BDDExpr *trueCond;
    static BDDExpr *falseCond;

private:

    CondToTermInstMap condToInstMap; ///< map condition to llvm instruction

};



} // End namespace SVF

#endif /* BITVECTORCOND_H_ */
