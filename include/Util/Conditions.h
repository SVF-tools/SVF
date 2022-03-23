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

namespace SVF {

class CondExpr {
public:

    enum CondExprKind {
        BDDK, Z3K
    };

    CondExpr(CondExprKind _k) : condExprK(_k) {
    }

    virtual ~CondExpr() {

    }

    /// get Condition kind
    inline CondExprKind getCondKind() const {
        return condExprK;
    }

private:
    CondExprKind condExprK;
};

class Z3Expr : public CondExpr {
public:
    typedef z3::expr Z3Cond;

    Z3Expr(const z3::expr &_e) : CondExpr(Z3K), e(_e) {
    }

    virtual ~Z3Expr() {

    }

    const Z3Cond &getExpr() const {
        return e;
    }

    /// get ctx
    inline z3::context &getContext() const {
        return getExpr().ctx();
    }

    /// get id
    inline NodeID getId() const {
        return getExpr().id();
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const Z3Expr *) {
        return true;
    }

    static inline bool classof(const CondExpr *cond) {
        return cond->getCondKind() == Z3K;
    }
    //@}

private:
    Z3Cond e;

};

class BDDExpr : public CondExpr {

public:
    typedef DdNode BDDCond;

    BDDExpr(BDDCond *_bddCond) : CondExpr(BDDK), bddCond(_bddCond) {
    }

    /// get BDD condition
    BDDCond *getBDDCond() const {
        return bddCond;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BDDExpr *) {
        return true;
    }

    static inline bool classof(const CondExpr *cond) {
        return cond->getCondKind() == BDDK;
    }
    //@}


private:
    BDDCond *bddCond;

};

class CondManager {

public:
    enum CondMgrKind {
        BDDMgrK, Z3MgrK
    };

private:
    static CondManager *condMgr;

protected:
    /// Constructor
    CondManager() {}

public:
    typedef Map<const CondExpr *, const Instruction *> CondToTermInstMap;    // map a condition to its branch instruction
    static u32_t totalCondNum; // a counter for fresh condition
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static CondManager *getCondMgr(CondMgrKind _condMgrKind);

    static void releaseCondMgr() {
        delete condMgr;
        condMgr = nullptr;
    }
    //@}

    /// Destructor
    virtual ~CondManager() {};

    /// Create a fresh condition to encode each program branch
    virtual CondExpr *createFreshBranchCond(const Instruction *inst) = 0;

    /// Return the number of condition expressions
    virtual u32_t getCondNumber() = 0;

    /// Return the unique true condition
    inline CondExpr *getTrueCond() const {
        return trueCond;
    }

    /// Return the unique false condition
    inline CondExpr *getFalseCond() const {
        return falseCond;
    }

    /// Operations on conditions.
    //@{
    virtual CondExpr *AND(CondExpr *lhs, CondExpr *rhs) = 0;

    virtual CondExpr *OR(CondExpr *lhs, CondExpr *rhs) = 0;

    virtual CondExpr *NEG(CondExpr *lhs) = 0;
    //@}

    virtual bool isNegCond(const CondExpr *cond) const = 0;

    /// Whether the condition is satisfiable
    virtual bool isSatisfiable(const CondExpr *cond) = 0;

    /// Whether lhs and rhs are equivalent branch conditions
    virtual bool isEquivalentBranchCond(const CondExpr *lhs, const CondExpr *rhs) = 0;

    /// Whether **All Paths** are reachable
    bool isAllPathReachable(const CondExpr *e);

    /// Get condition using condition id (z3 ast id)
    virtual CondExpr *getCond(u32_t id) const = 0;

    /// Get/Set llvm conditional expression
    //{@
    inline const Instruction *getCondInst(const CondExpr *cond) const {
        CondToTermInstMap::const_iterator it = condToInstMap.find(cond);
        assert(it != condToInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    inline void setCondInst(const CondExpr *cond, const Instruction *inst) {
        assert(condToInstMap.find(cond) == condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
    }
    //@}

    /// Return memory usage for this condition manager
    virtual std::string getMemUsage() = 0;

    /// Return string format of this expression
    virtual std::string dumpStr(const CondExpr *e) const = 0;

    /// Extract sub conditions of this expression
    virtual void extractSubConds(const CondExpr *cond, NodeBS &support) const = 0;

protected:
    static CondExpr *trueCond;
    static CondExpr *falseCond;

private:

    CondToTermInstMap condToInstMap; ///< map condition to llvm instruction

};

/**
* Z3 Condition Manager
*/
class Z3Manager : public CondManager {
    friend class CondManager;

public:
    typedef Z3Expr::Z3Cond Z3Cond;
    typedef Map<u32_t, Z3Expr *> IDToCondExprMap;

private:
    /// Constructor
    Z3Manager();

    IDToCondExprMap idToCondExprMap;
    z3::context cxt;
    z3::solver sol;
    NodeBS negConds;

public:

    /// Destructor
    virtual ~Z3Manager();


    inline u32_t getCondNumber() override {
        return sol.get_model().size();
    }

    /// Preprocess the condition, e.g., Compressing using And-Inverter-Graph
    z3::expr simplify(const z3::expr &expr) const;

    /// Given an id, get its condition
    CondExpr *getCond(u32_t i) const override {
        auto it = idToCondExprMap.find(i);
        assert(it != idToCondExprMap.end() && "condition not found!");
        return it->second;
    }

    CondExpr *createFreshBranchCond(const Instruction *inst) override;

    Z3Expr *getOrAddZ3Cond(const Z3Cond &z3Cond);

    /// Operations on conditions.
    //@{
    CondExpr *AND(CondExpr *lhs, CondExpr *rhs) override;

    CondExpr *OR(CondExpr *lhs, CondExpr *rhs) override;

    CondExpr *NEG(CondExpr *lhs) override;
    //@}

    bool isSatisfiable(const CondExpr *cond) override;

    bool isEquivalentBranchCond(const CondExpr *lhs, const CondExpr *rhs) override;

    inline void setNegCondInst(const CondExpr *cond, const Instruction *inst) {
        setCondInst(cond, inst);
        const Z3Expr *z3CondExpr = SVFUtil::dyn_cast<Z3Expr>(cond);
        assert(z3CondExpr && "not z3 condition.");
        negConds.set(z3CondExpr->getId());
    }

    bool isNegCond(const CondExpr *cond) const override {
        const Z3Expr *z3CondExpr = SVFUtil::dyn_cast<Z3Expr>(cond);
        assert(z3CondExpr && "not z3 condition.");
        return negConds.test(z3CondExpr->getId());
    }

    void extractSubConds(const CondExpr *f, NodeBS &support) const override;

    std::string dumpStr(const CondExpr *e) const override;

    inline std::string getMemUsage() override {
        return "";
    }

    /// Print out one particular expression
    void printDbg(const CondExpr *e);

    /// Dump out all expressions
    void printModel();
};


/**
* BDD Condition Manager
*/
class BDDManager : public CondManager {
    friend class CondManager;

public:
    typedef BDDExpr::BDDCond BDDCond;
    typedef Map<BDDCond *, BDDExpr *> BDDToBDDCondExpr;
    typedef Map<u32_t, BDDCond *> IndexToBDDCond;

private:

    /// Constructor
    BDDManager();

    inline BDDCond *BddOne() const {
        return Cudd_ReadOne(m_bdd_mgr);
    }

    inline BDDCond *BddZero() const {
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    DdManager *m_bdd_mgr;
    BDDToBDDCondExpr bddToBddCondExprMap;
    IndexToBDDCond indexToBddCondMap;

public:

    /// Destructor
    virtual ~BDDManager();

    BDDCond *createCond(u32_t i) {
        assert(indexToBddCondMap.find(i) == indexToBddCondMap.end() &&
               "This should be fresh index to create new BDD");
        BDDCond *bddCond = Cudd_bddIthVar(m_bdd_mgr, i);
        return indexToBddCondMap.emplace(i, bddCond).first->second;
    }

    /// Given an index, get its condition
    CondExpr *getCond(u32_t i) const override {
        auto it = indexToBddCondMap.find(i);
        assert(it != indexToBddCondMap.end() && "condition not found!");
        auto it2 = bddToBddCondExprMap.find(it->second);
        assert(it2 != bddToBddCondExprMap.end() && "condition not found!");
        return it2->second;
    }

    CondExpr *createFreshBranchCond(const Instruction *inst) override;

    inline u32_t getCondNumber() override {
        return Cudd_ReadNodeCount(m_bdd_mgr);
    }

    BDDExpr *getOrAddBranchCond(BDDCond *bddCond);

    /// Operations on conditions.
    //@{
    CondExpr *AND(CondExpr *lhs, CondExpr *rhs) override;

    CondExpr *OR(CondExpr *lhs, CondExpr *rhs) override;

    CondExpr *NEG(CondExpr *lhs) override;
    //@}

    bool isNegCond(const CondExpr *cond) const override {
        return false;
    }

    bool isEquivalentBranchCond(const CondExpr *lhs, const CondExpr *rhs) override {
        return lhs == rhs;
    }

    bool isSatisfiable(const CondExpr *cond) override;

    /**
     * Utilities for dumping conditions. These methods use global functions from CUDD
     * package and they can be removed outside this class scope to be used by others.
     */
    void ddClearFlag(BDDCond *f) const;

    void BddSupportStep(BDDCond *f, NodeBS &support) const;

    void extractSubConds(const CondExpr *f, NodeBS &support) const override;

    void dump(const CondExpr *lhs, OutStream &O);

    std::string dumpStr(const CondExpr *e) const override;

    inline std::string getMemUsage() override {
        return std::to_string(Cudd_ReadMemoryInUse(m_bdd_mgr));
    }

};


} // End namespace SVF

#endif /* BITVECTORCOND_H_ */
