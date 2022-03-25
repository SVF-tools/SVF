//===- Conditions.cpp -- Context/path conditions in the form of BDDs----------//
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
 * Conditions.cpp
 *
 *  Created on: Oct 19, 2021
 *      Author: Yulei and Xiao
 */

#include "Util/Options.h"
#include "Util/Conditions.h"

using namespace SVF;
using namespace SVFUtil;


CondExpr *CondManager::trueCond = nullptr;
CondExpr *CondManager::falseCond = nullptr;
CondManager *CondManager::condMgr = nullptr;
u32_t CondManager::totalCondNum = 0;

CondManager *CondManager::getCondMgr(CondMgrKind _condMgrKind) {
    if (condMgr == nullptr) {
        if (_condMgrKind == BDDMgrK)
            condMgr = new BDDManager();
        else if (_condMgrKind == Z3MgrK)
            condMgr = new Z3Manager();
        else
            assert(false && "invalid condition manager kind!");
    }
    return condMgr;
}

/*!
 * Whether **All Paths** are reachable
 */
bool CondManager::isAllPathReachable(const CondExpr *e) {
    return isEquivalentBranchCond(e, getTrueCond());
}

Z3Expr *Z3Manager::getOrAddZ3Cond(const Z3Cond &z3Cond) {
    auto it = idToCondExprMap.find(z3Cond.id());
    if (it != idToCondExprMap.end()) {
        return it->second;
    } else {
        Z3Expr *z3CondExpr = new Z3Expr(z3Cond);
        return idToCondExprMap.emplace(z3Cond.id(), z3CondExpr).first->second;
    }
}

Z3Manager::Z3Manager() : sol(cxt) {
    const z3::expr &trueExpr = cxt.bool_val(true);
    trueCond = getOrAddZ3Cond(trueExpr);
    const z3::expr &falseExpr = cxt.bool_val(false);
    falseCond = getOrAddZ3Cond(falseExpr);
}

Z3Manager::~Z3Manager() {
    for (const auto &it: idToCondExprMap) {
        delete it.second;
    }
}

inline bool Z3Manager::isEquivalentBranchCond(const CondExpr *lhs, const CondExpr *rhs) {
    sol.push();
    const Z3Expr *z3lhs = dyn_cast<Z3Expr>(lhs);
    const Z3Expr *z3rhs = dyn_cast<Z3Expr>(rhs);
    assert(z3lhs && z3rhs && "not z3 condition?");
    sol.add(z3lhs->getExpr() != z3rhs->getExpr());
    z3::check_result res = sol.check();
    sol.pop();
    return res == z3::unsat;
}

/// Operations on conditions.
//@{
CondExpr *Z3Manager::AND(CondExpr *lhs, CondExpr *rhs) {
    auto *z3lhs = dyn_cast<Z3Expr>(lhs);
    auto *z3rhs = dyn_cast<Z3Expr>(rhs);
    assert(z3lhs && z3rhs && "not z3 condition?");
    if (z3lhs == getFalseCond() || z3rhs == getFalseCond())
        return getFalseCond();
    else if (z3lhs == getTrueCond())
        return z3rhs;
    else if (z3rhs == getTrueCond())
        return z3lhs;
    else {
        const z3::expr &expr = z3lhs->getExpr() && z3rhs->getExpr();
        return getOrAddZ3Cond(expr);
    }
}

CondExpr *Z3Manager::OR(CondExpr *lhs, CondExpr *rhs) {
    auto *z3lhs = dyn_cast<Z3Expr>(lhs);
    auto *z3rhs = dyn_cast<Z3Expr>(rhs);
    assert(z3lhs && z3rhs && "not z3 condition?");
    if (z3lhs == getTrueCond() || z3rhs == getTrueCond())
        return getTrueCond();
    else if (z3lhs == getFalseCond())
        return z3rhs;
    else if (z3rhs == getFalseCond())
        return z3lhs;
    else {
        const z3::expr &expr = z3lhs->getExpr() || z3rhs->getExpr();
        return getOrAddZ3Cond(expr);
    }
}

CondExpr *Z3Manager::NEG(CondExpr *lhs) {
    auto *z3lhs = dyn_cast<Z3Expr>(lhs);
    assert(z3lhs && "not z3 condition?");
    if (z3lhs == getTrueCond())
        return getFalseCond();
    else if (z3lhs == getFalseCond())
        return getTrueCond();
    else {
        const z3::expr &expr = !z3lhs->getExpr();
        return getOrAddZ3Cond(expr);
    }
}
//@}

CondExpr *Z3Manager::createFreshBranchCond(const Instruction *inst) {
    u32_t condCountIdx = totalCondNum++;
    const z3::expr &expr = cxt.bool_const(("c" + std::to_string(condCountIdx)).c_str());
    auto it = idToCondExprMap.find(expr.id());
    if (it != idToCondExprMap.end())
        return it->second;
    else {
        auto *cond = new Z3Expr(expr);
        auto *negCond = NEG(cond);
        setCondInst(cond, inst);
        setNegCondInst(negCond, inst);
        return idToCondExprMap.emplace(expr.id(), cond).first->second;
    }
}

/*!
 * Extract sub conditions of this expression
 */
void Z3Manager::extractSubConds(const CondExpr *cond, NodeBS &support) const {
    const auto *z3CondExpr = dyn_cast<Z3Expr>(cond);
    assert(z3CondExpr && "not z3 condition?");
    if (z3CondExpr->getExpr().num_args() == 1 && isNegCond(z3CondExpr)) {
        support.set(z3CondExpr->getExpr().id());
        return;
    }
    if (z3CondExpr->getExpr().num_args() == 0)
        if (!z3CondExpr->getExpr().is_true() && !z3CondExpr->getExpr().is_false())
            support.set(z3CondExpr->getExpr().id());
    for (u32_t i = 0; i < z3CondExpr->getExpr().num_args(); ++i) {
        const z3::expr &expr = z3CondExpr->getExpr().arg(i);
        extractSubConds(getCond(expr.id()), support);
    }
}

/*!
 * Whether the condition is satisfiable
 */
bool Z3Manager::isSatisfiable(const CondExpr *cond) {
    sol.push();
    const Z3Expr *z3CondExpr = dyn_cast<Z3Expr>(cond);
    assert(z3CondExpr && "not z3 condition?");
    sol.add(z3CondExpr->getExpr());
    z3::check_result result = sol.check();
    sol.pop();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

/*!
 *  Preprocess the condition,
 *  e.g., Compressing using And-Inverter-Graph, Gaussian Elimination
 */
z3::expr Z3Manager::simplify(const z3::expr &expr) const {
    z3::goal g(expr.ctx());
    z3::tactic qe =
            z3::tactic(expr.ctx(), "aig");
    g.add(expr);
    z3::apply_result r = qe(g);
    z3::expr res(expr.ctx().bool_val(false));
    for (u32_t i = 0; i < r.size(); ++i) {
        if (res.is_false()) {
            res = r[i].as_expr();
        } else {
            res = res || r[i].as_expr();
        }
    }
    return res;
}


/*!
 * Print the expressions in this model
 */
void Z3Manager::printModel() {
    SVFUtil::outs() << sol.check() << "\n";
    z3::model m = sol.get_model();
    for (u32_t i = 0; i < m.size(); i++) {
        z3::func_decl v = m[i];
        SVFUtil::outs() << v.name() << " = " << m.get_const_interp(v) << "\n";
    }
}

/*!
 * Print out one particular expression
 */
inline void Z3Manager::printDbg(const CondExpr *e) {
    const Z3Expr *z3CondExpr = dyn_cast<Z3Expr>(e);
    assert(z3CondExpr && "not z3 condition?");
    SVFUtil::outs() << z3CondExpr->getExpr() << "\n";
}

/*!
 * Return string format of this expression
 */
std::string Z3Manager::dumpStr(const CondExpr *e) const {
    std::ostringstream out;
    const Z3Expr *z3CondExpr = dyn_cast<Z3Expr>(e);
    assert(z3CondExpr && "not z3 condition?");
    out << z3CondExpr->getExpr();
    return out.str();
}


/*!
 * Get or add a single branch condition,
 * e.g., when doing condition conjunction
 */
BDDExpr *BDDManager::getOrAddBranchCond(BDDCond *bddCond) {
    auto it = bddToBddCondExprMap.find(bddCond);
    if (it != bddToBddCondExprMap.end())
        return it->second;
    else {
        auto *cond = new BDDExpr(bddCond);
        return bddToBddCondExprMap.emplace(bddCond, cond).first->second;
    }
}

BDDManager::BDDManager() {
    m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    trueCond = getOrAddBranchCond(BddOne());
    falseCond = getOrAddBranchCond(BddZero());
}

BDDManager::~BDDManager() {
    for (const auto &it: bddToBddCondExprMap) {
        delete it.second;
    }
    Cudd_Quit(m_bdd_mgr);
}

CondExpr *BDDManager::createFreshBranchCond(const Instruction *inst) {
    u32_t condCountIdx = totalCondNum++;
    BDDCond *bddCond = createCond(condCountIdx);
    auto it = bddToBddCondExprMap.find(bddCond);
    if (it != bddToBddCondExprMap.end())
        return it->second;
    else {
        auto *cond = new BDDExpr(bddCond);
        setCondInst(cond, inst);
        auto *negCond = NEG(cond);
        setCondInst(negCond, inst);
        return bddToBddCondExprMap.emplace(bddCond, cond).first->second;
    }
}

/// Operations on conditions.
//@{
/// use Cudd_bddAndLimit interface to avoid bdds blow up
CondExpr *BDDManager::AND(CondExpr *lhs, CondExpr *rhs) {
    auto *bddlhs = dyn_cast<BDDExpr>(lhs);
    auto *bddrhs = dyn_cast<BDDExpr>(rhs);
    assert(bddlhs && bddrhs && "not bdd condition?");
    if (bddlhs == getFalseCond() || bddrhs == getFalseCond())
        return getFalseCond();
    else if (bddlhs == getTrueCond())
        return bddrhs;
    else if (bddrhs == getTrueCond())
        return bddlhs;
    else {
        BDDCond *tmp = Cudd_bddAndLimit(m_bdd_mgr, bddlhs->getBDDCond(), bddrhs->getBDDCond(), Options::MaxBddSize);
        if (tmp == nullptr) {
            SVFUtil::writeWrnMsg("exceeds max bdd size \n");
            ///drop the rhs condition
            return bddlhs;
        } else {
            Cudd_Ref(tmp);
            return getOrAddBranchCond(tmp);
        }
    }
}

/*!
 * Use Cudd_bddOrLimit interface to avoid bdds blow up
 */
CondExpr *BDDManager::OR(CondExpr *lhs, CondExpr *rhs) {
    auto *bddlhs = dyn_cast<BDDExpr>(lhs);
    auto *bddrhs = dyn_cast<BDDExpr>(rhs);
    assert(bddlhs && bddrhs && "not bdd condition?");

    if (bddlhs == getTrueCond() || bddrhs == getTrueCond())
        return getTrueCond();
    else if (bddlhs == getFalseCond())
        return bddrhs;
    else if (bddrhs == getFalseCond())
        return bddlhs;
    else {
        BDDCond *tmp = Cudd_bddOrLimit(m_bdd_mgr, bddlhs->getBDDCond(), bddrhs->getBDDCond(), Options::MaxBddSize);
        if (tmp == nullptr) {
            SVFUtil::writeWrnMsg("exceeds max bdd size \n");
            /// drop the two conditions here
            return getTrueCond();
        } else {
            Cudd_Ref(tmp);
            return getOrAddBranchCond(tmp);
        }
    }
}

CondExpr *BDDManager::NEG(CondExpr *lhs) {
    auto *bddlhs = dyn_cast<BDDExpr>(lhs);
    assert(bddlhs && "not bdd condition?");

    if (bddlhs == getTrueCond())
        return getFalseCond();
    else if (bddlhs == getFalseCond())
        return getTrueCond();
    else
        return getOrAddBranchCond(Cudd_Not(bddlhs->getBDDCond()));
}
//@}

/*!
 * Whether the condition is satisfiable
 */
bool BDDManager::isSatisfiable(const CondExpr *cond) {
    return cond != getFalseCond();
}

/*!
 * Utilities for dumping conditions. These methods use global functions from CUDD
 * package and they can be removed outside this class scope to be used by others.
 */
void BDDManager::ddClearFlag(BDDCond *f) const {
    if (!Cudd_IsComplement(f->next))
        return;
    /* Clear visited flag. */
    f->next = Cudd_Regular(f->next);
    if (cuddIsConstant(f))
        return;
    ddClearFlag(cuddT(f));
    ddClearFlag(Cudd_Regular(cuddE(f)));
    return;
}

void BDDManager::BddSupportStep(BDDCond *f, NodeBS &support) const {
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
        return;

    support.set(f->index);

    BddSupportStep(cuddT(f), support);
    BddSupportStep(Cudd_Regular(cuddE(f)), support);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);
}

void BDDManager::extractSubConds(const CondExpr *f, NodeBS &support) const {
    const auto *bddCondExpr = dyn_cast<BDDExpr>(f);
    assert(bddCondExpr && "not bdd condition?");
    BddSupportStep(Cudd_Regular(bddCondExpr->getBDDCond()), support);
    ddClearFlag(Cudd_Regular(bddCondExpr->getBDDCond()));
}

/*!
 * Dump BDD
 */
void BDDManager::dump(const CondExpr *lhs, OutStream &O) {
    if (lhs == getTrueCond())
        O << "T";
    else {
        NodeBS support;
        extractSubConds(lhs, support);
        for (NodeBS::iterator iter = support.begin(); iter != support.end();
             ++iter) {
            unsigned rid = *iter;
            O << rid << " ";
        }
    }
}


/*!
 * Dump BDD
 */
std::string BDDManager::dumpStr(const CondExpr *e) const {
    const auto *bddCondExpr = dyn_cast<BDDExpr>(e);
    assert(bddCondExpr && "not bdd condition?");
    std::string str;
    if (bddCondExpr == getTrueCond())
        str += "T";
    else {
        NodeBS support;
        extractSubConds(bddCondExpr, support);
        for (NodeBS::iterator iter = support.begin(); iter != support.end();
             ++iter) {
            unsigned rid = *iter;
            char int2str[16];
            sprintf(int2str, "%d", rid);
            str += int2str;
            str += " ";
        }
    }
    return str;
}

