//
// Created by Jiawei Ren on 2022/7/15.
//

#include "Util/Z3ExprManager.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

Z3ExprManager *Z3ExprManager::z3ExprMgr = nullptr;
u32_t Z3ExprManager::totalCondNum = 0;

Z3ExprManager::Z3ExprManager() : sol(getContext()) {
}

Z3ExprManager::~Z3ExprManager() {
}

Z3ExprManager *Z3ExprManager::getZ3ExprMgr() {
    if (z3ExprMgr == nullptr) {
        z3ExprMgr = new Z3ExprManager();
    }
    return z3ExprMgr;
}

/// Create a fresh condition to encode each program branch
Z3Expr Z3ExprManager::createFreshBranchCond(const Instruction *inst) {
    u32_t condCountIdx = totalCondNum++;
    Z3Expr expr = getContext().bool_const(("c" + std::to_string(condCountIdx)).c_str());
    Z3Expr negCond = NEG(expr);
    setCondInst(expr, inst);
    setNegCondInst(negCond, inst);
    z3ExprVec.push_back(expr);
    z3ExprVec.push_back(negCond);
    return expr;
}

// compute NEG
Z3Expr Z3ExprManager::NEG(const Z3Expr &z3Expr) {
    return !z3Expr;
}

// compute AND
Z3Expr Z3ExprManager::AND(const Z3Expr &lhs, const Z3Expr &rhs) {
    if (eq(lhs, getFalseCond()) || eq(rhs, getFalseCond())) {
        return getFalseCond();
    } else if (eq(lhs, getTrueCond())) {
        return rhs;
    } else if (eq(rhs, getTrueCond())) {
        return lhs;
    } else {
        Z3Expr expr = lhs.getExpr() && rhs.getExpr();
        // check subexpression size and option limit
        if (getExprSize(expr) > Options::MaxZ3Size) {
            sol.push();
            sol.add(expr.getExpr());
            z3::check_result res = sol.check();
            sol.pop();
            if (res != z3::unsat) {
                return lhs;
            } else {
                return getFalseCond();
            }
        }
        return expr;
    }
}

// compute OR
Z3Expr Z3ExprManager::OR(const Z3Expr &lhs, const Z3Expr &rhs) {
    if (eq(lhs, getTrueCond()) || eq(rhs, getTrueCond())) {
        return getTrueCond();
    } else if (eq(lhs, getFalseCond())) {
        return rhs;
    } else if (eq(rhs, getFalseCond())) {
        return lhs;
    } else {
        Z3Expr expr = lhs.getExpr() || rhs.getExpr();
        // check subexpression size and option limit
        if (getExprSize(expr) > Options::MaxZ3Size) {
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
        return expr;
    }
}

/// Whether lhs and rhs are equivalent branch conditions
bool Z3ExprManager::isEquivalentBranchCond(const Z3Expr &lhs, const Z3Expr &rhs) {
    sol.push();
    sol.add(lhs.getExpr() != rhs.getExpr()); // check equal using z3 solver
    z3::check_result res = sol.check();
    sol.pop();
    return res == z3::unsat;
}

/// Whether **All Paths** are reachable
bool Z3ExprManager::isAllPathReachable(const Z3Expr &z3Expr) {
    return isEquivalentBranchCond(z3Expr, getTrueCond());
}

/// Whether the condition is satisfiable
bool Z3ExprManager::isSatisfiable(const Z3Expr &z3Expr) {
    sol.push();
    sol.add(z3Expr.getExpr());
    z3::check_result result = sol.check();
    sol.pop();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

// extract subexpression from a Z3 expression
void Z3ExprManager::extractSubConds(const Z3Expr &z3Expr, NodeBS &support) const {
    if (z3Expr.getExpr().num_args() == 1 && isNegCond(z3Expr.id())) {
        support.set(z3Expr.getExpr().id());
        return;
    }
    if (z3Expr.getExpr().num_args() == 0)
        if (!z3Expr.getExpr().is_true() && !z3Expr.getExpr().is_false())
            support.set(z3Expr.getExpr().id());
    for (u32_t i = 0; i < z3Expr.getExpr().num_args(); ++i) {
        Z3Expr expr = z3Expr.getExpr().arg(i);
        extractSubConds(expr, support);
    }
}

// output Z3 expression as a string
std::string Z3ExprManager::dumpStr(const Z3Expr &z3Expr) const {
    std::ostringstream out;
    out << z3Expr.getExpr();
    return out.str();
}

// get the number of subexpression of a Z3 expression
u32_t Z3ExprManager::getExprSize(const Z3Expr &z3Expr) {
    u32_t res = 1;
    if (z3Expr.getExpr().num_args() == 0) {
        return 1;
    }
    for (u32_t i = 0; i < z3Expr.getExpr().num_args(); ++i) {
        Z3Expr expr = z3Expr.getExpr().arg(i);
        res += getExprSize(expr);
    }
    return res;
}
