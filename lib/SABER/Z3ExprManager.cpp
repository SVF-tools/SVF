//
// Created by Jiawei Ren on 2022/7/15.
//

#include "Util/Z3ExprManager.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

Z3ExprManager *Z3ExprManager::z3ExprMgr = nullptr;
u32_t Z3ExprManager::totalCondNum = 0;

Z3ExprManager::Z3ExprManager() {
}

Z3ExprManager::~Z3ExprManager() {
}

Z3ExprManager *Z3ExprManager::getZ3ExprMgr() {
    if (z3ExprMgr == nullptr) {
        z3ExprMgr = new Z3ExprManager();
    }
    return z3ExprMgr;
}

Z3Expr Z3ExprManager::createFreshBranchCond(const Instruction *inst) {
    u32_t condCountIdx = totalCondNum++;
    const Z3Expr &expr = getContext().bool_const(("c" + std::to_string(condCountIdx)).c_str());
    const Z3Expr &negCond = NEG(expr);
    setCondInst(expr, inst);
    setNegCondInst(negCond, inst);
    return expr;
}

Z3Expr Z3ExprManager::NEG(const Z3Expr &lhs) {
    return !lhs;
}

//Z3Expr *Z3ExprManager::getOrAddZ3Cond(const Z3Expr &z3Expr) {
//    auto it = idToExprMap.find(z3Expr.hash());
//    if (it != idToExprMap.end()) {
//        return it->second;
//    } else {
//        Z3Expr *newz3Expr = new Z3Expr(z3Expr);
//        return idToExprMap.emplace(newz3Expr->id(), newz3Expr).first->second;
//    }
//}

Z3Expr Z3ExprManager::AND(const Z3Expr &lhs, const Z3Expr &rhs) {
    Z3Expr expr = lhs.getExpr() && rhs.getExpr();
    if (expr.getExpr().num_args() > Options::MaxZ3Size) {
        z3::solver sol(Z3Expr::getContext());
        sol.push();
        sol.add(expr.getExpr());
        z3::check_result res = sol.check();
        sol.pop();
        if (res != z3::unsat) {
            return getTrueCond();
        } else {
            return getFalseCond();
        }
    }
    return expr.simplify();
}

Z3Expr Z3ExprManager::OR(const Z3Expr &lhs, const Z3Expr &rhs) {
    Z3Expr expr = lhs.getExpr() || rhs.getExpr();
    if (expr.getExpr().num_args() > Options::MaxZ3Size) {
        z3::solver sol(Z3Expr::getContext());
        sol.push();
        sol.add(expr.getExpr());
        z3::check_result res = sol.check();
        sol.pop();
        if (res != z3::unsat) {
            return getTrueCond();
        } else {
            return getFalseCond();
        }
    }
    return expr.simplify();
}

bool Z3ExprManager::isEquivalentBranchCond(const Z3Expr &lhs, const Z3Expr &rhs) {
    z3::solver sol(Z3Expr::getContext());
    sol.push();
    sol.add(lhs.getExpr() != rhs.getExpr());
    z3::check_result res = sol.check();
    sol.pop();
    return res == z3::unsat;
}

bool Z3ExprManager::isAllPathReachable(const Z3Expr &e) {
    return isEquivalentBranchCond(e, getTrueCond());
}

bool Z3ExprManager::isSatisfiable(const Z3Expr &z3Expr) {
    z3::solver sol(Z3Expr::getContext());
    sol.push();
    sol.add(z3Expr.getExpr());
    z3::check_result result = sol.check();
    sol.pop();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

void Z3ExprManager::extractSubConds(const Z3Expr &z3Expr, NodeBS &support) const {
    if (z3Expr.getExpr().num_args() == 1 && isNegCond(z3Expr.hash())) {
        support.set(z3Expr.getExpr().hash());
        return;
    }
    if (z3Expr.getExpr().num_args() == 0)
        if (!z3Expr.getExpr().is_true() && !z3Expr.getExpr().is_false())
            support.set(z3Expr.getExpr().hash());
    for (u32_t i = 0; i < z3Expr.getExpr().num_args(); ++i) {
        const Z3Expr &expr = Z3Expr(z3Expr.getExpr().arg(i));
        extractSubConds(expr, support);
    }
}

std::string Z3ExprManager::dumpStr(const Z3Expr &z3Expr) const {
    std::ostringstream out;
    out << z3Expr.getExpr();
    return out.str();
}


