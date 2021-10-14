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
#include <cstdio>

#include "CUDD/cuddInt.h"
#include "z3++.h"

namespace SVF
{

class CondExpr{

public:

    explicit CondExpr(const z3::expr& _e): e(_e){
    }
    const z3::expr& getExpr() const{
        return e;
    }
    
    /// get ctx
    inline z3::context& getContext() const {
        return getExpr().ctx();
    }

    /// get id
    inline u32_t getId() const {
        return getExpr().id();
    }

    // branch condition is the same
    bool operator==(const CondExpr &condExpr) const {
        return getId() == condExpr.getId();
    }

    /// overload logical operators
    //{@
    friend CondExpr* operator||(const CondExpr &lhs, const CondExpr &rhs);
    friend CondExpr* operator&&(const CondExpr &lhs, const CondExpr &rhs);
    friend CondExpr* operator!(const CondExpr &lhs);
    //@}

private:
    z3::expr e;

};

class CondManager{

private:
    static CondManager* condMgr;

    /// Constructor
    CondManager();

public:
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline CondManager* getCondMgr()
    {
        if (condMgr == nullptr)
        {
            condMgr = new CondManager();
        }
        return condMgr;
    }
    static void releaseCondMgr()
    {
        delete condMgr;
        condMgr = nullptr;
    }
    //@}

    typedef Map<u32_t, CondExpr*> IDToCondExprMap;

    /// Destructor
    ~CondManager();

    /// Create a single condition
    CondExpr* createCondForBranch(u32_t i);

    /// new a single condition
    CondExpr* getExistingCond(const z3::expr& e) const;

    /// new a single condition
    CondExpr* createNewCond(const z3::expr& e);

    /// Return the number of condition expressions
    u32_t getCondNumber();

    /// Return the unique true condition
    inline CondExpr* getTrueCond() const
    {
        return trueCond;
    }
    /// Return the unique false condition
    inline CondExpr* getFalseCond() const
    {
        return falseCond;
    }

    /// simplify
    z3::expr simplify(const z3::expr& expr) const;
    /// Operations on conditions.
    //@{
    inline CondExpr* AND(const CondExpr* lhs, const CondExpr* rhs){
        return (*lhs) && (*rhs);
    }
    inline CondExpr* OR(const CondExpr* lhs, const CondExpr* rhs){
        return (*lhs) || (*rhs);
    }
    inline CondExpr* NEG(const CondExpr* lhs){
        return !(*lhs);
    }
    //@}

    /// Return memory usage for this condition manager
    std::string getMemUsage();

    /// Dump out all expressions
    void printModel();

    /// Print out one particular expression
    void printDbg(const CondExpr* e);

    /// Return string format of this expression
    std::string dumpStr(const CondExpr* e) const;

    /// Extract sub conditions of this expression
    void extractSubConds(const z3::expr& e,  NodeBS &support) const;

    /// whether z3 condition e is satisfiable
    bool isSatisfiable(const CondExpr* cond);

    /// whether the conditions of **All Paths** are satisfiable
    bool isAllSatisfiable(const CondExpr* e);

    inline CondExpr* getCond(u32_t h) const {
        const IDToCondExprMap::const_iterator it = allocatedConds.find(h);
        assert(it!=allocatedConds.end() && "condition not found!");
        return it->second;
    }

private:
    z3::context cxt;
    z3::solver sol;
    static CondExpr* trueCond;
    static CondExpr* falseCond;
//    Set<CondExpr*> allocatedConds;
    IDToCondExprMap allocatedConds;
};

inline CondExpr* operator||(const CondExpr &lhs, const CondExpr &rhs) {
    CondManager *condMgr = CondManager::getCondMgr();
    if (lhs == *condMgr->getTrueCond() || rhs == *condMgr->getTrueCond())
        return condMgr->getTrueCond();
    else if (lhs == *condMgr->getFalseCond())
        return condMgr->getExistingCond(rhs.getExpr());
    else if (rhs == *condMgr->getFalseCond())
        return condMgr->getExistingCond(lhs.getExpr());
    else{
        const z3::expr &expr = condMgr->simplify(lhs.getExpr() || rhs.getExpr());
        if (CondExpr *cond = condMgr->getExistingCond(expr))
            return cond;
        else
            return condMgr->createNewCond(expr);
    }
}

inline CondExpr* operator&&(const CondExpr &lhs, const CondExpr &rhs) {
    CondManager *condMgr = CondManager::getCondMgr();
    if (lhs == *condMgr->getFalseCond() || rhs == *condMgr->getFalseCond())
        return condMgr->getFalseCond();
    else if (lhs == *condMgr->getTrueCond())
        return condMgr->getExistingCond(rhs.getExpr());
    else if (rhs == *condMgr->getTrueCond())
        return condMgr->getExistingCond(lhs.getExpr());
    else {
        const z3::expr &expr = condMgr->simplify(lhs.getExpr() && rhs.getExpr());
        if (CondExpr *cond = condMgr->getExistingCond(expr))
            return cond;
        else
            return condMgr->createNewCond(expr);
    }
}

inline CondExpr* operator!(const CondExpr &lhs) {
    CondManager *condMgr = CondManager::getCondMgr();
    if (lhs == *condMgr->getTrueCond())
        return condMgr->getFalseCond();
    else if (lhs == *condMgr->getFalseCond())
        return condMgr->getTrueCond();
    else{
        const z3::expr &expr = !lhs.getExpr();
        if (CondExpr *cond = condMgr->getExistingCond(expr))
            return cond;
        else
            return condMgr->createNewCond(expr);
    }
}

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

    DdNode* createCond(u32_t i)
    {
        return Cudd_bddIthVar(m_bdd_mgr, i);
    }
    inline DdNode* getTrueCond() const
    {
        return BddOne();
    }
    inline DdNode* getFalseCond() const
    {
        return BddZero();
    }

    inline std::string getMemUsage()
    {
        return std::to_string(Cudd_ReadMemoryInUse(m_bdd_mgr));
    }
    inline u32_t getCondNumber()
    {
        return Cudd_ReadNodeCount(m_bdd_mgr);
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
    void extractSubConds( DdNode * f,  NodeBS &support) const;
    void dump(DdNode* lhs, raw_ostream & O);
    std::string dumpStr(DdNode* lhs) const;

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
