//===- Z3Expr.h -- Z3 conditions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/*
 * Z3Expr.h
 *
 *  Created on: April 29, 2022
 *      Author: Xiao
 */

#include "Util/Z3Expr.h"
#include "Util/Options.h"

namespace SVF
{

z3::context *Z3Expr::ctx = nullptr;
z3::solver* Z3Expr::solver = nullptr;


/// release z3 context
void Z3Expr::releaseContext()
{
    if(solver)
        releaseSolver();
    delete ctx;
    ctx = nullptr;
}

/// release z3 solver
void Z3Expr::releaseSolver()
{
    delete solver;
    solver = nullptr;
}

/// Get z3 solver, singleton design here to make sure we only have one context
z3::solver &Z3Expr::getSolver()
{
    if (solver == nullptr)
    {
        solver = new z3::solver(getContext());
    }
    return *solver;
}

/// Get z3 context, singleton design here to make sure we only have one context
z3::context &Z3Expr::getContext()
{
    if (ctx == nullptr)
    {
        ctx = new z3::context();
    }
    return *ctx;
}

/// get the number of subexpression of a Z3 expression
u32_t Z3Expr::getExprSize(const Z3Expr &z3Expr)
{
    u32_t res = 1;
    if (z3Expr.getExpr().num_args() == 0)
    {
        return 1;
    }
    for (u32_t i = 0; i < z3Expr.getExpr().num_args(); ++i)
    {
        Z3Expr expr = z3Expr.getExpr().arg(i);
        res += getExprSize(expr);
    }
    return res;
}

/// compute AND, used for branch condition
Z3Expr Z3Expr::AND(const Z3Expr &lhs, const Z3Expr &rhs)
{
    if (eq(lhs, Z3Expr::getFalseCond()) || eq(rhs, Z3Expr::getFalseCond()))
    {
        return Z3Expr::getFalseCond();
    }
    else if (eq(lhs, Z3Expr::getTrueCond()))
    {
        return rhs;
    }
    else if (eq(rhs, Z3Expr::getTrueCond()))
    {
        return lhs;
    }
    else
    {
        Z3Expr expr = (lhs && rhs).simplify();
        // check subexpression size and option limit
        if (Z3Expr::getExprSize(expr) > Options::MaxZ3Size())
        {
            getSolver().push();
            getSolver().add(expr.getExpr());
            z3::check_result res = getSolver().check();
            getSolver().pop();
            if (res != z3::unsat)
            {
                return lhs;
            }
            else
            {
                return Z3Expr::getFalseCond();
            }
        }
        return expr;
    }
}

/// compute OR, used for branch condition
Z3Expr Z3Expr::OR(const Z3Expr &lhs, const Z3Expr &rhs)
{
    if (eq(lhs, Z3Expr::getTrueCond()) || eq(rhs, Z3Expr::getTrueCond()))
    {
        return Z3Expr::getTrueCond();
    }
    else if (eq(lhs, Z3Expr::getFalseCond()))
    {
        return rhs;
    }
    else if (eq(rhs, Z3Expr::getFalseCond()))
    {
        return lhs;
    }
    else
    {
        Z3Expr expr = (lhs || rhs).simplify();
        // check subexpression size and option limit
        if (Z3Expr::getExprSize(expr) > Options::MaxZ3Size())
        {
            getSolver().push();
            getSolver().add(expr.getExpr());
            z3::check_result res = getSolver().check();
            getSolver().pop();
            if (res != z3::unsat)
            {
                return Z3Expr::getTrueCond();
            }
            else
            {
                return Z3Expr::getFalseCond();
            }
        }
        return expr;
    }
}

/// output Z3 expression as a string
std::string Z3Expr::dumpStr(const Z3Expr &z3Expr)
{
    std::ostringstream out;
    out << z3Expr.getExpr();
    return out.str();
}
}

