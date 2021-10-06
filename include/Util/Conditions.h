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

    /// get/insert/set condition id
    //{@
    Set<u32_t> getBranchCondIDs() const{
        return branchCondIDs;
    }
    void insertBranchCondIDs(u32_t id) {
        branchCondIDs.insert(id);
    }
    void setBranchCondIDs(const Set<u32_t>& _ids){
        branchCondIDs = _ids;
    }
    //@}
    
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
    Set<u32_t> branchCondIDs; ///< record unique z3 expr id of the truth table

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
    CondExpr* createCond(u32_t i);

    /// new a single condition
    bool newCond(const z3::expr& i);

    /// Return the number of condition expressions
    u32_t getCondNumber();

    /// Return the unique true condition
    inline static CondExpr* getTrueCond()
    {
        return trueCond;
    }
    /// Return the unique false condition
    inline static CondExpr* getFalseCond()
    {
        return falseCond;
    }

    /// simplify
    static z3::expr simplify(const z3::expr& expr);
    /// Operations on conditions.
    //@{
    static inline CondExpr* AND(const CondExpr* lhs, const CondExpr* rhs){
        return (*lhs) && (*rhs);
    }
    static inline CondExpr* OR(const CondExpr* lhs, const CondExpr* rhs){
        return (*lhs) || (*rhs);
    }
    static inline CondExpr* NEG(const CondExpr* lhs){
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
    bool isSatisfiable(const z3::expr& e);

    /// whether the conditions of **All Paths** are satisfiable
    bool isAllSatisfiable(const CondExpr* e);

    /// build truth table (stored in truthTable)
    void buildTruthTable(Set<u32_t>::const_iterator curit,
                         Set<u32_t>::const_iterator eit,
                         std::vector<z3::expr> &tmpExpr,
                         std::vector<std::vector<z3::expr>> &truthTable);

    /// Enumerate all path conditions by assigning each row of the truth table
    /// to the boolean identifiers of the original condition
    z3::expr_vector enumerateAllPathConditions(const CondExpr *condition);


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
    const z3::expr &expr = CondManager::simplify(lhs.getExpr() || rhs.getExpr());
    CondManager *condMgr = CondManager::getCondMgr();
    if (!condMgr->newCond(expr)) {
        return condMgr->getCond(expr.id());;
    } else {
        // new condition
        CondExpr *cond = condMgr->getCond(expr.id());
        Set<u32_t> lhsCondIDs = lhs.getBranchCondIDs();
        for (u32_t id: rhs.getBranchCondIDs())
            lhsCondIDs.insert(id);
        cond->setBranchCondIDs(lhsCondIDs);
        return cond;
    }
}

inline CondExpr* operator&&(const CondExpr &lhs, const CondExpr &rhs) {
    const z3::expr &expr = CondManager::simplify(lhs.getExpr() && rhs.getExpr());
    CondManager *condMgr = CondManager::getCondMgr();
    if (!condMgr->newCond(expr)) {
        return condMgr->getCond(expr.id());
    } else {
        // new condition
        CondExpr *cond = condMgr->getCond(expr.id());
        Set<u32_t> lhsCondIDs = lhs.getBranchCondIDs();
        for (u32_t id: rhs.getBranchCondIDs())
            lhsCondIDs.insert(id);
        cond->setBranchCondIDs(lhsCondIDs);
        return cond;
    }
}

inline CondExpr* operator!(const CondExpr &lhs) {
    const z3::expr &expr = CondManager::simplify(!lhs.getExpr());
    CondManager *condMgr = CondManager::getCondMgr();
    if (!condMgr->newCond(expr)) {
        return condMgr->getCond(expr.id());
    } else {
        // new condition
        CondExpr *cond = condMgr->getCond(expr.id());
        Set<u32_t> lhsCondIDs = lhs.getBranchCondIDs();
        cond->setBranchCondIDs(lhsCondIDs);
        cond->setBranchCondIDs(lhsCondIDs);
        return cond;
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
