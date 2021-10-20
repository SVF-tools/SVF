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

class CondExpr{
public:

    enum CondExprKind
    {
        BranchK
    };

    CondExpr(const z3::expr &_e, CondExprKind _k) : e(_e), condExprK(_k) {
    }
    virtual ~CondExpr(){

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

    /// get Condition kind
    inline CondExprKind getCondKind() const{
        return condExprK;
    }

private:
    z3::expr e;
    CondExprKind condExprK;
};

class BranchCondExpr: public CondExpr{

public:
    typedef DdNode BranchCond;
    BranchCondExpr(const z3::expr& _e, BranchCond* _branchCond): CondExpr(_e, BranchK), branchCond(_branchCond){
    }

    /// get branch condition
    BranchCond* getBranchCond() const{
        return branchCond;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BranchCondExpr *)
    {
        return true;
    }

    static inline bool classof(const CondExpr *cond)
    {
        return cond->getCondKind() == BranchK;
    }
    //@}


private:
    BranchCond* branchCond;

};


/**
 * Branch Condition Manager
 */
class BranchCondManager
{
public:
    typedef BranchCondExpr::BranchCond BranchCond;
    /// Constructor
    BranchCondManager()
    {
        m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    }

    /// Destructor
    ~BranchCondManager()
    {
        Cudd_Quit(m_bdd_mgr);
    }

    BranchCond* createCond(u32_t i)
    {
        return Cudd_bddIthVar(m_bdd_mgr, i);
    }
    inline BranchCond* getTrueCond() const
    {
        return BddOne();
    }
    inline BranchCond* getFalseCond() const
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
    BranchCond* AND(BranchCond* lhs, BranchCond* rhs);
    BranchCond* OR(BranchCond* lhs, BranchCond* rhs);
    BranchCond* NEG(BranchCond* lhs);
    //@}

    /**
     * Utilities for dumping conditions. These methods use global functions from CUDD
     * package and they can be removed outside this class scope to be used by others.
     */
    void ddClearFlag(BranchCond * f) const;
    void BddSupportStep( BranchCond * f,  NodeBS &support) const;
    void extractSubConds( BranchCond * f,  NodeBS &support) const;
    void dump(BranchCond* lhs, raw_ostream & O);
    std::string dumpStr(BranchCond* lhs) const;

private:
    inline BranchCond* BddOne() const
    {
        return Cudd_ReadOne(m_bdd_mgr);
    }
    inline BranchCond* BddZero() const
    {
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    DdManager *m_bdd_mgr;
};

class CondManager{

private:
    static CondManager* condMgr;

    /// Constructor
    CondManager();

public:
    typedef BranchCondExpr::BranchCond BranchCond;
    typedef Map<const CondExpr*, const Instruction* > CondToTermInstMap;	// map a condition to its branch instruction
    static u32_t totalCondNum; // a counter for fresh condition
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
    typedef Map<BranchCond*, CondExpr*> BranchCondToCondExprMap;

    /// Destructor
    ~CondManager();

    /// Create a fresh condition to encode each program branch
    CondExpr* createFreshBranchCond(const Instruction* inst);

    /// Get or add a single branch condition, e.g., when doing condition conjunction
    CondExpr* getOrAddBranchCond(const z3::expr& e, BranchCond* branchCond);

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

    /// Preprocess the condition, e.g., Compressing using And-Inverter-Graph
    z3::expr simplify(const z3::expr& expr) const;
    /// Operations on conditions.
    //@{
    CondExpr* AND(CondExpr* lhs, CondExpr* rhs);
    CondExpr* OR(CondExpr* lhs, CondExpr* rhs);
    CondExpr* NEG(CondExpr* lhs);
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
    void extractSubConds(const CondExpr* cond,  NodeBS &support) const;

    /// Whether the condition is satisfiable
    bool isSatisfiable(const CondExpr* cond);

    /// Whether lhs and rhs are equivalent branch conditions
    inline bool isEquivalentBranchCond(const CondExpr* lhs, const CondExpr* rhs) const{
        return SVFUtil::dyn_cast<BranchCondExpr>(lhs)->getBranchCond() ==
               SVFUtil::dyn_cast<BranchCondExpr>(rhs)->getBranchCond();
    }
    /// Whether **All Paths** are reachable
    bool isAllPathReachable(const CondExpr* e);

    /// Get condition using condition id (z3 ast id)
    inline CondExpr* getCond(u32_t id) const {
        const IDToCondExprMap::const_iterator it = allocatedConds.find(id);
        assert(it!=allocatedConds.end() && "condition not found!");
        return it->second;
    }

    /// Get/Set llvm conditional expression
    //{@
    inline const Instruction* getCondInst(const CondExpr* cond) const
    {
        CondToTermInstMap::const_iterator it = condToInstMap.find(cond);
        assert(it!=condToInstMap.end() && "this should be a fresh condition");
        return it->second;
    }
    inline void setCondInst(const CondExpr* cond, const Instruction* inst){
        assert(condToInstMap.find(cond)==condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
    }
    //@}

private:
    z3::context cxt;
    z3::solver sol;
    static CondExpr* trueCond;
    static CondExpr* falseCond;
    IDToCondExprMap allocatedConds; ///< map condition id (z3 ast id) to its Condition wrapper
    BranchCondToCondExprMap branchCondToCondExpr; ///< map branch condition to its Condition wrapper
    CondToTermInstMap condToInstMap; ///< map condition to llvm instruction
    BranchCondManager branchCondManager; ///< branch condition manager
};

} // End namespace SVF

#endif /* BITVECTORCOND_H_ */
