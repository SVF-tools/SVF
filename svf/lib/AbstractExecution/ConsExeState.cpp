//===- ConsExeState.cpp ----Constant Execution State-------------------------//
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

//
// Created by jiawei and xiao on 6/1/23.
//



#include "AbstractExecution/ConsExeState.h"
#include <iomanip>
#include "Util/Options.h"


using namespace SVF;
using namespace SVFUtil;


ConsExeState ConsExeState::globalConsES(initExeState());

/*!
 * Copy operator
 * @param rhs
 * @return
 */
ConsExeState &ConsExeState::operator=(const ConsExeState &rhs)
{
    if (*this != rhs)
    {
        _varToVal = rhs.getVarToVal();
        _locToVal = rhs.getLocToVal();
        ExeState::operator=(rhs);
    }
    return *this;
}

/*!
 * Move operator
 * @param rhs
 * @return
 */
ConsExeState &ConsExeState::operator=(ConsExeState &&rhs) noexcept
{
    if (this != &rhs)
    {
        _varToVal = SVFUtil::move(rhs._varToVal);
        _locToVal = SVFUtil::move(rhs._locToVal);
        ExeState::operator=(std::move(rhs));
    }
    return *this;
}

/*!
 * Overloading Operator==
 * @param rhs
 * @return
 */
bool ConsExeState::operator==(const ConsExeState &rhs) const
{
    // if values of variables are not changed, fix-point is reached
    return ExeState::operator==(rhs) && eqVarToValMap(_varToVal, rhs.getVarToVal()) &&
           eqVarToValMap(_locToVal, rhs.getLocToVal());
}

/*!
 * Overloading Operator<
 * @param rhs
 * @return
 */
bool ConsExeState::operator<(const ConsExeState &rhs) const
{
    // judge from path constraint
    if (lessThanVarToValMap(_varToVal, rhs.getVarToVal()) || lessThanVarToValMap(_locToVal, rhs.getLocToVal()))
        return true;
    return false;
}

u32_t ConsExeState::hash() const
{
    size_t bas = ExeState::hash();
    size_t h = getVarToVal().size() * 2;
    SVF::Hash<SVF::u32_t> hf;
    for (const auto &t: getVarToVal())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hf(t.second.id()) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }

    size_t h2 = getVarToVal().size() * 2;

    for (const auto &t: getLocToVal())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
        h2 ^= hf(t.second.id()) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    SVF::Hash<std::pair<SVF::u32_t, SVF::u32_t>> pairH;

    return pairH(std::make_pair(bas, pairH(std::make_pair(h, h2))));
}

/*!
 * Build global execution state
 * @param globES
 * @param vars
 */
void ConsExeState::buildGlobES(ConsExeState &globES, Set<u32_t> &vars)
{
    for (const auto &varId: vars)
    {
        SingleAbsValue &expr = globES[varId];
        if (expr.is_numeral() && isVirtualMemAddress(expr.get_numeral_int()))
        {
            if (globES.inLocalLocToVal(expr))
            {
                store(expr, globES.load(expr));
            }
        }
    }
}

/*!
 * Merge rhs into this
 * @param rhs
 * @return
 */
bool ConsExeState::joinWith(const SVF::ConsExeState &rhs)
{
    bool changed = ExeState::joinWith(rhs);
    for (const auto &rhsItem: rhs._varToVal)
    {
        auto it = _varToVal.find(rhsItem.first);
        // Intersection - lhs and rhs have the same var id
        if (it != getVarToVal().end())
        {
            if (it->second.isTop() || rhsItem.second.isTop())
            {
                if (assign(it->second, SingleAbsValue::topConstant()))
                    changed = true;
            }
            else if (it->second.isBottom())
            {
                if (assign(it->second, rhsItem.second))
                    changed = true;
            }
            else if (rhsItem.second.isBottom())
            {

            }
            else
            {
                if (!eq(it->second, rhsItem.second))
                {
                    if (assign(it->second, SingleAbsValue::topConstant()))
                        changed = true;
                }
            }
        }
        else
        {
            _varToVal.emplace(rhsItem.first, rhsItem.second);
            changed = true;
        }
    }

    for (const auto &rhsItem: rhs._locToVal)
    {
        auto it = _locToVal.find(rhsItem.first);
        // Intersection - lhs and rhs have the same var id
        if (it != getLocToVal().end())
        {
            if (it->second.isTop() || rhsItem.second.isTop())
            {
                if (assign(it->second, SingleAbsValue::topConstant()))
                    changed = true;
            }
            else if (it->second.isBottom())
            {
                if (assign(it->second, rhsItem.second))
                    changed = true;
            }
            else if (rhsItem.second.isBottom())
            {

            }
            else
            {
                if (!eq(it->second, rhsItem.second))
                {
                    if (assign(it->second, SingleAbsValue::topConstant()))
                        changed = true;
                }
            }
        }
        else
        {
            _locToVal.emplace(rhsItem.first, rhsItem.second);
            changed = true;
        }
    }
    return changed;
}

/*!
 *  We should build a summary for each actual params to boost precision.
 *  The summary of callee only contains the side-effects of callee
 *  (input obj value, return value, global value) without irrelevant caller information.
 *  We apply the summary to the exestate at each callsite.
 * @param summary
 */
void ConsExeState::applySummary(const ConsExeState &summary)
{
    for (const auto &item: summary._varToVal)
    {
        _varToVal[item.first] = item.second;
    }
    for (const auto &item: summary._locToVal)
    {
        _locToVal[item.first] = item.second;
    }
    for (const auto &item: summary._varToVAddrs)
    {
        _varToVAddrs[item.first] = item.second;
    }
    for (const auto &item: summary._locToVAddrs)
    {
        _locToVAddrs[item.first] = item.second;
    }
}

/*!
 * Print values of all expressions
 */
void ConsExeState::printExprValues() const
{
    std::cout << "\n";
    std::cout.flags(std::ios::left);
    std::cout << "\t-----------------Var and Value-----------------\n";
    for (const auto &item: getVarToVal())
    {
        std::stringstream exprName;
        exprName << "\tVar" << item.first;
        std::cout << std::setw(20) << exprName.str();
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            std::cout << "\t\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            std::cout << "\t\t Value: " << std::dec << sim << "\n";
        }
    }
    std::cout << "\t-----------------------------------------------\n";
    std::cout << "\t-----------------Loc and Value-----------------\n";
    for (const auto &item: getLocToVal())
    {
        std::stringstream exprName;
        exprName << "\tVar" << item.first;
        std::cout << std::setw(20) << exprName.str();
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            std::cout << "\t\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            std::cout << "\t\t Value: " << std::dec << sim << "\n";
        }
    }
    std::cout << "\t-----------------------------------------------\n";
}

void ConsExeState::printExprValues(std::ostream &oss) const
{
    oss << "\n";
    oss.flags(std::ios::left);
    oss << "\t-----------------Var and Value-----------------\n";
    for (const auto &item: getVarToVal())
    {
        std::stringstream exprName;
        exprName << "\tVar" << item.first;
        oss << std::setw(20) << exprName.str();
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            oss << "\t\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            oss << "\t\t Value: " << std::dec << sim << "\n";
        }
    }
    oss << "\t-----------------------------------------------\n";
    oss << "\t-----------------Loc and Value-----------------\n";
    for (const auto &item: getLocToVal())
    {
        std::stringstream exprName;
        exprName << "\tVar" << item.first;
        oss << std::setw(20) << exprName.str();
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            oss << "\t\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            oss << "\t\t Value: " << std::dec << sim << "\n";
        }
    }
    oss << "\t-----------------------------------------------\n";
}

std::string ConsExeState::pcToString() const
{
    std::stringstream exprName;
    exprName << "Path Constraint:\n";
    return SVFUtil::move(exprName.str());
}

std::string ConsExeState::varToString(u32_t valId) const
{
    std::stringstream exprName;
    auto it = getVarToVal().find(valId);
    if (it == getVarToVal().end())
    {
        auto it2 = globalConsES._varToVal.find(valId);
        if (it2 == globalConsES._varToVal.end())
        {
            exprName << "Var not in varToVal!\n";
        }
        else
        {
            const SingleAbsValue &sim = it2->second.simplify();
            if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
            {
                exprName << "addr: " << std::dec << getInternalID(z3Expr2NumValue(sim)) << "\n";
            }
            else
            {
                if (sim.is_numeral())
                    exprName << std::dec << z3Expr2NumValue(sim) << "\n";
                else
                    exprName << sim.to_string() << "\n";
            }
        }
    }
    else
    {
        const SingleAbsValue &sim = it->second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            exprName << "addr: " << std::dec << getInternalID(z3Expr2NumValue(sim)) << "\n";
        }
        else
        {
            if (sim.is_numeral())
                exprName << std::dec << z3Expr2NumValue(sim) << "\n";
            else
                exprName << sim.to_string() << "\n";
        }
    }
    return SVFUtil::move(exprName.str());
}

std::string ConsExeState::locToString(u32_t objId) const
{
    std::stringstream exprName;
    auto it = getLocToVal().find(objId);
    if (it == getLocToVal().end())
    {
        exprName << "Obj not in locToVal!\n";
    }
    else
    {
        const SingleAbsValue &sim = it->second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            exprName << "addr: " << std::dec << getInternalID(z3Expr2NumValue(sim)) << "\n";
        }
        else
        {
            if (sim.is_numeral())
                exprName << std::dec << z3Expr2NumValue(sim) << "\n";
            else
                exprName << sim.to_string() << "\n";
        }
    }
    return SVFUtil::move(exprName.str());
}

std::string ConsExeState::toString() const
{
    std::stringstream exprName;
    exprName << pcToString();
    exprName << "VarToVal:\n";
    for (const auto &item: getVarToVal())
    {
        exprName << "Var" << std::to_string(item.first) << ":\n";
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            exprName << " \tValue" << std::dec << getInternalID(z3Expr2NumValue(sim)) << "\n";
        }
        else
        {
            exprName << " \tValue" << std::dec << z3Expr2NumValue(sim) << "\n";
        }
    }

    exprName << "LocToVal:\n";
    for (const auto &item: getLocToVal())
    {
        exprName << "Var" << std::to_string(item.first) << ":\n";
        const SingleAbsValue &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            exprName << " \tValue" << std::dec << getInternalID(z3Expr2NumValue(sim)) << "\n";
        }
        else
        {
            exprName << " \tValue" << std::dec << z3Expr2NumValue(sim) << "\n";
        }
    }
    return SVFUtil::move(exprName.str());
}

bool ConsExeState::applySelect(u32_t res, u32_t cond, u32_t top, u32_t fop)
{
    if (inVarToVal(top) && inVarToVal(fop) && inVarToVal(cond))
    {
        SingleAbsValue &tExpr = (*this)[top], &fExpr = (*this)[fop], &condExpr = (*this)[cond];

        return assign((*this)[res], ite(condExpr == 1, tExpr, fExpr));
    }
    else if (inVarToAddrsTable(top) && inVarToAddrsTable(fop) && inVarToVal(cond))
    {
        SingleAbsValue &condExpr = (*this)[cond];
        if (condExpr.is_numeral())
        {
            getVAddrs(res) = condExpr.is_zero() ? getVAddrs(fop) : getVAddrs(top);
        }
    }
    return false;
}

bool ConsExeState::applyPhi(u32_t res, std::vector<u32_t> &ops)
{
    for (u32_t i = 0; i < ops.size(); i++)
    {
        NodeID curId = ops[i];
        if (inVarToVal(curId))
        {
            const SingleAbsValue &cur = (*this)[curId];
            if (!inVarToVal(res))
            {
                (*this)[res] = cur;
            }
            else
            {
                (*this)[res].join_with(cur);
            }
        }
        else if (inVarToAddrsTable(curId))
        {
            const VAddrs &cur = getVAddrs(curId);
            if (!inVarToAddrsTable(res))
            {
                getVAddrs(res) = cur;
            }
            else
            {
                getVAddrs(res).join_with(cur);
            }
        }
    }
    return true;
}

s64_t ConsExeState::getNumber(u32_t lhs)
{
    return z3Expr2NumValue((*this)[lhs]);
}

/*!
 * Store value to location
 * @param loc location, e.g., int_val(0x7f..01)
 * @param value
 */
bool ConsExeState::store(const SingleAbsValue &loc, const SingleAbsValue &value)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    return store(getInternalID(virAddr), value);
}

/*!
 * Load value at location
 * @param loc location, e.g., int_val(0x7f..01)
 * @return
 */
SingleAbsValue ConsExeState::load(const SingleAbsValue &loc)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    u32_t objId = getInternalID(virAddr);
    assert(getInternalID(objId) == objId && "SVFVar idx overflow > 0x7f000000?");
    return load(objId);
}


/*!
 *  Whether two var to value map is equivalent
 * @param pre
 * @param nxt
 * @return
 */
bool ConsExeState::eqVarToValMap(const VarToValMap &pre, const VarToValMap &nxt)
{
    if (pre.size() != nxt.size()) return false;
    for (const auto &item: nxt)
    {
        auto it = pre.find(item.first);
        // return false if SVFVar not exists in rhs
        if (it == pre.end())
            return false;
        if (!eq(it->second, item.second))
            return false;
    }
    return true;
}

/*!
 *  Whether lhs is less than rhs
 * @param lhs
 * @param rhs
 * @return
 */
bool ConsExeState::lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs)
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

SingleAbsValue ConsExeState::load(u32_t objId)
{
    auto it = _locToVal.find(objId);
    if (it != _locToVal.end())
    {
        return it->second;
    }
    else
    {
        auto globIt = globalConsES._locToVal.find(objId);
        if (globIt != globalConsES._locToVal.end())
            return globIt->second;
        else
        {
            SVFUtil::writeWrnMsg("Null dereference");
            return SingleAbsValue::topConstant();
        }
    }
}

bool ConsExeState::assign(SingleAbsValue &lhs, const SingleAbsValue &rhs)
{
    if (!eq(lhs, rhs))
    {
        lhs = rhs;
        return true;
    }
    else
    {
        return false;
    }
}
