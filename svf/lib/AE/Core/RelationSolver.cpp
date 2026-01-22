//===- RelationSolver.cpp ----Relation Solver for Interval Domains-----------//
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
 * RelationSolver.cpp
 *
 *  Created on: Aug 4, 2022
 *      Author: Jiawei Ren
 *
 */
#include "AE/Core/RelationSolver.h"
#include <cmath>
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

AbstractState RelationSolver::bilateral(const AbstractState&domain, const Z3Expr& phi,
                                        u32_t descend_check)
{
    /// init variables
    AbstractState upper = domain.top();
    AbstractState lower = domain.bottom();
    u32_t meets_in_a_row = 0;
    z3::solver solver = Z3Expr::getSolver();
    z3::params p(Z3Expr::getContext());
    /// TODO: add option for timeout
    p.set(":timeout", static_cast<unsigned>(600)); // in milliseconds
    solver.set(p);
    AbstractState consequence;

    /// start processing
    while (lower != upper)
    {
        if (meets_in_a_row == descend_check)
        {
            consequence = lower;
        }
        else
        {
            consequence = abstract_consequence(lower, upper, domain);
        }
        /// compute domain.model_and(phi, domain.logic_not(domain.gamma_hat(consequence)))
        Z3Expr rhs = !(gamma_hat(consequence, domain));
        solver.push();
        solver.add(phi.getExpr() && rhs.getExpr());
        Map<u32_t, s32_t> solution;
        z3::check_result checkRes = solver.check();
        /// find any solution, which is sat
        if (checkRes == z3::sat)
        {
            z3::model m = solver.get_model();
            for (u32_t i = 0; i < m.size(); i++)
            {
                z3::func_decl v = m[i];
                // assert(v.arity() == 0);
                if (v.arity() != 0)
                    continue;
                solution.emplace(std::stoi(v.name().str()),
                                 m.get_const_interp(v).get_numeral_int());
            }
            for (const auto& item : domain.getVarToVal())
            {
                if (solution.find(item.first) == solution.end())
                {
                    solution.emplace(item.first, 0);
                }
            }
            solver.pop();
            AbstractState newLower = domain.bottom();
            newLower.joinWith(lower);
            AbstractState rhs = beta(solution, domain);
            newLower.joinWith(rhs);
            lower = newLower;
            meets_in_a_row = 0;
        }
        else /// unknown or unsat
        {
            solver.pop();
            if (checkRes == z3::unknown)
            {
                /// for timeout reason return upper
                if (solver.reason_unknown() == "timeout")
                    return upper;
            }
            AbstractState newUpper = domain.top();
            newUpper.meetWith(upper);
            newUpper.meetWith(consequence);
            upper = newUpper;
            meets_in_a_row += 1;
        }
    }
    return upper;
}

AbstractState RelationSolver::RSY(const AbstractState& domain, const Z3Expr& phi)
{
    AbstractState lower = domain.bottom();
    z3::solver& solver = Z3Expr::getSolver();
    z3::params p(Z3Expr::getContext());
    /// TODO: add option for timeout
    p.set(":timeout", static_cast<unsigned>(600)); // in milliseconds
    solver.set(p);
    while (1)
    {
        Z3Expr rhs = !(gamma_hat(lower, domain));
        solver.push();
        solver.add(phi.getExpr() && rhs.getExpr());
        Map<u32_t, s32_t> solution;
        z3::check_result checkRes = solver.check();
        /// find any solution, which is sat
        if (checkRes == z3::sat)
        {
            z3::model m = solver.get_model();
            for (u32_t i = 0; i < m.size(); i++)
            {
                z3::func_decl v = m[i];
                if (v.arity() != 0)
                    continue;

                solution.emplace(std::stoi(v.name().str()),
                                 m.get_const_interp(v).get_numeral_int());
            }
            for (const auto& item : domain.getVarToVal())
            {
                if (solution.find(item.first) == solution.end())
                {
                    solution.emplace(item.first, 0);
                }
            }
            solver.pop();
            AbstractState newLower = domain.bottom();
            newLower.joinWith(lower);
            newLower.joinWith(beta(solution, domain));
            lower = newLower;
        }
        else /// unknown or unsat
        {
            solver.pop();
            if (checkRes == z3::unknown)
            {
                /// for timeout reason return upper
                if (solver.reason_unknown() == "timeout")
                    return domain.top();
            }
            break;
        }
    }
    return lower;
}

AbstractState RelationSolver::abstract_consequence(
    const AbstractState& lower, const AbstractState& upper, const AbstractState& domain) const
{
    /*Returns the "abstract consequence" of lower and upper.

    The abstract consequence must be a superset of lower and *NOT* a
    superset of upper.

            Note that this is a fairly "simple" abstract consequence, in that it
    sets only one variable to a non-top interval. This improves performance
    of the SMT solver in many cases. In certain cases, other choices for
    the abstract consequence will lead to better algorithm performance.*/

    for (auto it = domain.getVarToVal().begin();
            it != domain.getVarToVal().end(); ++it)
        /// for variable in self.variables:
    {
        AbstractState proposed = domain.top(); /// proposed = self.top.copy()
        proposed[it->first] = lower[it->first].getInterval();
        /// proposed.set_interval(variable, lower.interval_of(variable))
        /// proposed._locToItvVal
        if (!(proposed >= upper)) /// if not proposed >= upper:
        {
            return proposed; /// return proposed
        }
    }
    return lower; /// return lower.copy()
}

Z3Expr RelationSolver::gamma_hat(const AbstractState& exeState) const
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        IntervalValue interval = item.second.getInterval();
        if (interval.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (interval.isTop())
            continue;
        Z3Expr v = toIntZ3Expr(item.first);
        res = (res && v >= (int)interval.lb().getNumeral() &&
               v <= (int)interval.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(const AbstractState& alpha,
                                 const AbstractState& exeState) const
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        IntervalValue interval = alpha[item.first].getInterval();
        if (interval.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (interval.isTop())
            continue;
        Z3Expr v = toIntZ3Expr(item.first);
        res = (res && v >= (int)interval.lb().getNumeral() &&
               v <= (int)interval.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(u32_t id, const AbstractState& exeState) const
{
    auto it = exeState.getVarToVal().find(id);
    assert(it != exeState.getVarToVal().end() && "id not in varToVal?");
    Z3Expr v = toIntZ3Expr(id);
    // Z3Expr v = Z3Expr::getContext().int_const(std::to_string(id).c_str());
    Z3Expr res = (v >= (int)it->second.getInterval().lb().getNumeral() &&
                  v <= (int)it->second.getInterval().ub().getNumeral());
    return res;
}

AbstractState RelationSolver::beta(const Map<u32_t, s32_t>& sigma,
                                   const AbstractState& exeState) const
{
    AbstractState res;
    for (const auto& item : exeState.getVarToVal())
    {
        res[item.first] = IntervalValue(
                              sigma.at(item.first), sigma.at(item.first));
    }
    return res;
}

void RelationSolver::updateMap(Map<u32_t, s32_t>& map, u32_t key, const s32_t& value)
{
    auto it = map.find(key);
    if (it == map.end())
    {
        map.emplace(key, value);
    }
    else
    {
        it->second = value;
    }
}

AbstractState RelationSolver::BS(const AbstractState& domain, const Z3Expr &phi)
{
    /// because key of _varToItvVal is u32_t, -key may out of range for int
    /// so we do key + bias for -key
    u32_t bias = 0;
    s32_t infinity = INT32_MAX/2 - 1;

    // int infinity = (INT32_MAX) - 1;
    // int infinity = 20;
    Map<u32_t, s32_t> ret;
    Map<u32_t, s32_t> low_values, high_values;
    Z3Expr new_phi = phi;
    /// init low, ret, high
    for (const auto& item: domain.getVarToVal())
    {
        IntervalValue interval = item.second.getInterval();
        updateMap(ret, item.first, interval.ub().getIntNumeral());
        if (interval.lb().is_minus_infinity())
            updateMap(low_values, item.first, -infinity);
        else
            updateMap(low_values, item.first, interval.lb().getIntNumeral());
        if (interval.ub().is_plus_infinity())
            updateMap(high_values, item.first, infinity);
        else
            updateMap(high_values, item.first, interval.ub().getIntNumeral());
        if (item.first > bias)
            bias = item.first + 1;
    }
    for (const auto& item: domain.getVarToVal())
    {
        /// init objects -x
        IntervalValue interval = item.second.getInterval();
        u32_t reverse_key = item.first + bias;
        updateMap(ret, reverse_key, -interval.lb().getIntNumeral());
        if (interval.ub().is_plus_infinity())
            updateMap(low_values, reverse_key, -infinity);
        else
            updateMap(low_values, reverse_key, -interval.ub().getIntNumeral());
        if (interval.lb().is_minus_infinity())
            updateMap(high_values, reverse_key, infinity);
        else
            updateMap(high_values, reverse_key, -interval.lb().getIntNumeral());
        /// add a relation that x == -(x+bias)
        new_phi = (new_phi && (toIntZ3Expr(reverse_key) == -1 * toIntZ3Expr(item.first)));
    }
    /// optimize each object
    BoxedOptSolver(new_phi.simplify(), ret, low_values, high_values);
    /// fill in the return values
    AbstractState retInv;
    for (const auto& item: ret)
    {
        if (item.first >= bias)
        {
            if (!retInv.inVarToValTable(item.first-bias))
                retInv[item.first-bias] = IntervalValue::top();

            if (item.second == (infinity))
                retInv[item.first - bias] = IntervalValue(BoundedInt::minus_infinity(),
                                            retInv[item.first - bias].getInterval().ub());
            else
                retInv[item.first - bias] = IntervalValue(float(-item.second), retInv[item.first - bias].getInterval().ub());

        }
        else
        {
            if (item.second == (infinity))
                retInv[item.first] = IntervalValue(retInv[item.first].getInterval().lb(),
                                                   BoundedInt::plus_infinity());
            else
                retInv[item.first] = IntervalValue(retInv[item.first].getInterval().lb(), float(item.second));
        }
    }
    return retInv;
}

Map<u32_t, s32_t> RelationSolver::BoxedOptSolver(const Z3Expr& phi, Map<u32_t, s32_t>& ret, Map<u32_t, s32_t>& low_values, Map<u32_t, s32_t>& high_values)
{
    /// this is the S in the original paper
    Map<u32_t, Z3Expr> L_phi;
    Map<u32_t, s32_t> mid_values;
    while (1)
    {
        L_phi.clear();
        for (const auto& item : ret)
        {
            Z3Expr v = toIntZ3Expr(item.first);
            if (low_values.at(item.first) <= (high_values.at(item.first)))
            {
                s32_t mid = (low_values.at(item.first) + (high_values.at(item.first) - low_values.at(item.first)) / 2);
                updateMap(mid_values, item.first, mid);
                Z3Expr expr = (toIntVal(mid) <= v && v <= toIntVal(high_values.at(item.first)));
                L_phi[item.first] = expr;
            }
        }
        if (L_phi.empty())
            break;
        else
            decide_cpa_ext(phi, L_phi, mid_values, ret, low_values, high_values);
    }
    return ret;
}


void RelationSolver::decide_cpa_ext(const Z3Expr& phi,
                                    Map<u32_t, Z3Expr>& L_phi,
                                    Map<u32_t, s32_t>& mid_values,
                                    Map<u32_t, s32_t>& ret,
                                    Map<u32_t, s32_t>& low_values,
                                    Map<u32_t, s32_t>& high_values)
{
    while (1)
    {
        Z3Expr join_expr(Z3Expr::getContext().bool_val(false));
        for (const auto& item : L_phi)
            join_expr = (join_expr || item.second);
        join_expr = (join_expr && phi).simplify();
        z3::solver& solver = Z3Expr::getSolver();
        solver.push();
        solver.add(join_expr.getExpr());
        Map<u32_t, double> solution;
        z3::check_result checkRes = solver.check();
        /// find any solution, which is sat
        if (checkRes == z3::sat)
        {
            z3::model m = solver.get_model();
            solver.pop();
            for(const auto & item : L_phi)
            {
                u32_t id = item.first;
                int value = m.eval(toIntZ3Expr(id).getExpr()).get_numeral_int();
                // int value = m.eval(Z3Expr::getContext().int_const(std::to_string(id).c_str())).get_numeral_int();
                /// id is the var id, value is the solution found for var_id
                /// add a relation to check if the solution meets phi_id
                Z3Expr expr = (item.second && toIntZ3Expr(id) == value);
                solver.push();
                solver.add(expr.getExpr());
                // solution meets phi_id
                if (solver.check() == z3::sat)
                {
                    updateMap(ret, id, (value));
                    updateMap(low_values, id, ret.at(id) + 1);

                    s32_t mid = (low_values.at(id) + high_values.at(id) + 1) / 2;
                    updateMap(mid_values, id, mid);
                    Z3Expr v = toIntZ3Expr(id);
                    // Z3Expr v = Z3Expr::getContext().int_const(std::to_string(id).c_str());
                    Z3Expr expr = (toIntVal(mid_values.at(id)) <= v && v <= toIntVal(high_values.at(id)));
                    L_phi[id] = expr;
                }
                solver.pop();
            }
        }
        else /// unknown or unsat, we consider unknown as unsat
        {
            solver.pop();
            for (const auto& item : L_phi)
                high_values.at(item.first) = mid_values.at(item.first) - 1;
            return;
        }
    }

}