//
// Created by Jiawei Ren on 2022/7/15.
//

#include "Util/Z3ExprManager.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

Z3Expr *Z3ExprManager::trueCond = nullptr;
Z3Expr *Z3ExprManager::falseCond = nullptr;
Z3ExprManager *Z3ExprManager::z3ExprMgr = nullptr;
u32_t Z3ExprManager::totalCondNum = 0;

Z3ExprManager::Z3ExprManager() {
    const Z3Expr &trueExpr = Z3Expr::getContext().bool_val(true);
    trueCond = getOrAddZ3Cond(trueExpr);
    const Z3Expr &falseExpr = Z3Expr::getContext().bool_val(false);
    falseCond = getOrAddZ3Cond(falseExpr);
}

Z3ExprManager::~Z3ExprManager() {
    for (const auto &it: idToExprMap) {
        delete it.second;
    }
}

Z3ExprManager *Z3ExprManager::getZ3ExprMgr() {
    if (z3ExprMgr == nullptr) {
        z3ExprMgr = new Z3ExprManager();
    }
    return z3ExprMgr;
}

Z3Expr *Z3ExprManager::createFreshBranchCond(const Instruction *inst) {
    u32_t condCountIdx = totalCondNum++;
    Z3Expr *expr = new Z3Expr(Z3Expr::getContext().bool_const(("c" + std::to_string(condCountIdx)).c_str()));
    auto it = idToExprMap.find(expr->id());
    if (it == idToExprMap.end()) {
        // jod of adding neg to map is done in NEG
        auto *negCond = NEG(expr);
        setCondInst(expr, inst);
        setNegCondInst(negCond, inst);
        return idToExprMap.emplace(expr->id(), expr).first->second;
    } else {
        return it->second;
    }
}

Z3Expr *Z3ExprManager::NEG(Z3Expr *lhs) {
    if (lhs == getTrueCond())
        return getFalseCond();
    else if (lhs == getFalseCond())
        return getTrueCond();
    else {
        const Z3Expr &expr = Z3Expr(!lhs->getExpr());
        return getOrAddZ3Cond(expr);
    }
}

Z3Expr *Z3ExprManager::getOrAddZ3Cond(const Z3Expr &z3Expr) {
    auto it = idToExprMap.find(z3Expr.id());
    if (it != idToExprMap.end()) {
        return it->second;
    } else {
        Z3Expr *newz3Expr = new Z3Expr(z3Expr);
        return idToExprMap.emplace(newz3Expr->id(), newz3Expr).first->second;
    }
}

Z3Expr *Z3ExprManager::AND(Z3Expr *lhs, Z3Expr *rhs) {
    assert(lhs && rhs && "not z3 condition?");
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return rhs;
    else if (rhs == getTrueCond())
        return lhs;
    else {
        const Z3Expr &expr = Z3Expr(lhs->getExpr() && rhs->getExpr());
        if (expr.getExpr().num_args() > Options::MaxZ3Size) {
            z3::solver sol(Z3Expr::getContext());
            sol.push();
            sol.add(expr.getExpr());
            z3::check_result res = sol.check();
            sol.pop();
            if (res != z3::unsat) {
                return trueCond;
            } else {
                return falseCond;
            }
        }
        return getOrAddZ3Cond(expr);
    }
}

Z3Expr *Z3ExprManager::OR(Z3Expr *lhs, Z3Expr *rhs) {
    assert(lhs && rhs && "not z3 condition?");
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return rhs;
    else if (rhs == getFalseCond())
        return lhs;
    else {
        const Z3Expr &expr = Z3Expr(lhs->getExpr() || rhs->getExpr());
        if (expr.getExpr().num_args() > Options::MaxZ3Size) {
            z3::solver sol(Z3Expr::getContext());
            sol.push();
            sol.add(expr.getExpr());
            z3::check_result res = sol.check();
            sol.pop();
            if (res != z3::unsat) {
                return trueCond;
            } else {
                return falseCond;
            }
        }
        return getOrAddZ3Cond(expr);
    }
}

bool Z3ExprManager::isEquivalentBranchCond(const Z3Expr *lhs, const Z3Expr *rhs) {
    z3::solver sol(Z3Expr::getContext());
    sol.push();
    assert(lhs && rhs && "not z3 condition?");
    sol.add(lhs->getExpr() != rhs->getExpr());
    z3::check_result res = sol.check();
    sol.pop();
    return res == z3::unsat;
}

bool Z3ExprManager::isAllPathReachable(const Z3Expr *e) {
    return isEquivalentBranchCond(e, getTrueCond());
}

bool Z3ExprManager::isSatisfiable(const Z3Expr *z3Expr) {
    z3::solver sol(Z3Expr::getContext());
    sol.push();
    assert(z3Expr && "not z3 condition?");
    sol.add(z3Expr->getExpr());
    z3::check_result result = sol.check();
    sol.pop();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

void Z3ExprManager::extractSubConds(const Z3Expr *z3Expr, NodeBS &support) const {
    assert(z3Expr && "not z3 condition?");
    if (z3Expr->getExpr().num_args() == 1 && isNegCond(z3Expr)) {
        support.set(z3Expr->getExpr().id());
        return;
    }
    if (z3Expr->getExpr().num_args() == 0)
        if (!z3Expr->getExpr().is_true() && !z3Expr->getExpr().is_false())
            support.set(z3Expr->getExpr().id());
    for (u32_t i = 0; i < z3Expr->getExpr().num_args(); ++i) {
        const Z3Expr &expr = Z3Expr(z3Expr->getExpr().arg(i));
        extractSubConds(getCond(expr.id()), support);
    }
}

std::string Z3ExprManager::dumpStr(const Z3Expr *z3Expr) const {
    std::ostringstream out;
    assert(z3Expr && "expression cannot be null.");
    out << z3Expr->getExpr();
    return out.str();
}



