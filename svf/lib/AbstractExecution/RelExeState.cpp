//===- RelExeState.cpp ----Relation Execution States for Interval Domains-------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
 * RelExeState.cpp
 *
 *  Created on: Aug 15, 2022
 *      Author: Jiawei Ren, Xiao Cheng
 *
 */

#include "AbstractExecution/RelExeState.h"
#include "SVFIR/SVFIR.h"
#include <iomanip>

using namespace SVF;
using namespace SVFUtil;

/*!
 * Extract sub SVFVar IDs of a Z3Expr
 *
 * e.g., Given an expr "a+b", return {a.id, b.id}
 * @param expr
 * @param res
 */
void RelExeState::extractSubVars(const Z3Expr &expr, Set<u32_t> &res)
{
    if (expr.getExpr().num_args() == 0)
        if (!expr.getExpr().is_true() && !expr.getExpr().is_false() && !expr.is_numeral())
        {
            const std::string &exprStr = expr.to_string();
            res.insert(std::stoi(exprStr.substr(1, exprStr.size() - 1)));
        }
    for (u32_t i = 0; i < expr.getExpr().num_args(); ++i)
    {
        const z3::expr &e = expr.getExpr().arg(i);
        extractSubVars(e, res);
    }
}

/*!
 * Extract all related SVFVar IDs based on compare expr
 *
 * @param expr
 * @param res
 */
void RelExeState::extractCmpVars(const Z3Expr &expr, Set<u32_t> &res)
{
    Set<u32_t> r;
    extractSubVars(expr, r);
    res.insert(r.begin(), r.end());
    assert(!r.empty() && "symbol not init?");
    if (r.size() == 1 && eq(expr, toZ3Expr(*r.begin())))
    {
        return;
    }
    for (const auto &id: r)
    {
        extractCmpVars((*this)[id], res);
    }
}

/*!
 * Build relational Z3Expr
 * @param cmp
 * @param succ
 * @param vars return all the relational vars of a given variable
 * @param initVars the vars on the right hand side of cmp statement, e.g., {a} for "cmp = a > 1"
 * @return
 */
Z3Expr RelExeState::buildRelZ3Expr(u32_t cmp, s32_t succ, Set<u32_t> &vars, Set<u32_t> &initVars)
{
    Z3Expr res = (getZ3Expr(cmp) == succ).simplify();
    extractSubVars(res, initVars);
    extractCmpVars(res, vars);
    for (const auto &id: vars)
    {
        res = (res && toZ3Expr(id) == getZ3Expr(id)).simplify();
    }
    res = (res && (toZ3Expr(cmp) == getZ3Expr(cmp))).simplify();
    vars.insert(cmp);
    return res;
}

RelExeState &RelExeState::operator=(const RelExeState &rhs)
{
    if (*this != rhs)
    {
        _varToVal = rhs.getVarToVal();
        _locToVal = rhs.getLocToVal();
    }
    return *this;
}

/*!
 * Overloading Operator==
 * @param rhs
 * @return
 */
bool RelExeState::operator==(const RelExeState &rhs) const
{
    return eqVarToValMap(_varToVal, rhs.getVarToVal()) &&
           eqVarToValMap(_locToVal, rhs.getLocToVal());
}

/*!
 * Overloading Operator<
 * @param rhs
 * @return
 */
bool RelExeState::operator<(const RelExeState &rhs) const
{
    return lessThanVarToValMap(_varToVal, rhs.getVarToVal()) ||
           lessThanVarToValMap(_locToVal, rhs.getLocToVal());
}

bool RelExeState::eqVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const
{
    if (lhs.size() != rhs.size()) return false;
    for (const auto &item: lhs)
    {
        auto it = rhs.find(item.first);
        // return false if SVFVar not exists in rhs or z3Expr not equal
        if (it == rhs.end() || !eq(item.second, it->second))
            return false;
    }
    return true;
}

bool RelExeState::lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const
{
    if (lhs.size() != rhs.size()) return lhs.size() < rhs.size();
    for (const auto &item: lhs)
    {
        auto it = rhs.find(item.first);
        // lhs > rhs if SVFVar not exists in rhs
        if (it == rhs.end())
            return false;
        // judge from expr id
        if (!eq(item.second, it->second))
            return item.second.id() < it->second.id();
    }
    return false;
}

/*!
 * Store value to location
 * @param loc location, e.g., int_val(0x7f..01)
 * @param value
 */
void RelExeState::store(const Z3Expr &loc, const Z3Expr &value)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    store(getInternalID(virAddr), value);
}

/*!
 * Load value at location
 * @param loc location, e.g., int_val(0x7f..01)
 * @return
 */
Z3Expr &RelExeState::load(const Z3Expr &loc)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    u32_t objId = getInternalID(virAddr);
    assert(getInternalID(objId) == objId && "SVFVar idx overflow > 0x7f000000?");
    return load(objId);
}

/*!
 * Print values of all expressions
 */
void RelExeState::printExprValues()
{
    std::cout.flags(std::ios::left);
    std::cout << "-----------Var and Value-----------\n";
    for (const auto &item: getVarToVal())
    {
        std::stringstream exprName;
        exprName << "Var" << item.first;
        std::cout << std::setw(25) << exprName.str();
        const Z3Expr &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            std::cout << "\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            std::cout << "\t Value: " << std::dec << sim << "\n";
        }
    }
    std::cout << "-----------------------------------------\n";
}