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
#include "AbstractExecution/RelationSolver.h"
using namespace SVF;
using namespace SVFUtil;

IntervalExeState RelationSolver::bilateral(const IntervalExeState &domain, const Z3Expr& phi,
        u32_t descend_check)
{
    /// init variables
    IntervalExeState upper = domain.top();
    IntervalExeState lower = domain.bottom();
    u32_t meets_in_a_row = 0;
    z3::solver solver = Z3Expr::getSolver();
    z3::params p(Z3Expr::getContext());
    /// TODO: add option for timeout
    p.set(":timeout", static_cast<unsigned>(600)); // in milliseconds
    solver.set(p);
    IntervalExeState consequence;

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
        Map<u32_t, double> solution;
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
            IntervalExeState newLower = domain.bottom();
            newLower.joinWith(lower);
            IntervalExeState rhs = beta(solution, domain);
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
            IntervalExeState newUpper = domain.top();
            newUpper.meetWith(upper);
            newUpper.meetWith(consequence);
            upper = newUpper;
            meets_in_a_row += 1;
        }
    }
    return upper;
}

IntervalExeState RelationSolver::RSY(const IntervalExeState& domain, const Z3Expr& phi)
{
    IntervalExeState lower = domain.bottom();
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
        Map<u32_t, double> solution;
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
            IntervalExeState newLower = domain.bottom();
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

IntervalExeState RelationSolver::abstract_consequence(
    const IntervalExeState& lower, const IntervalExeState& upper, const IntervalExeState& domain) const
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
        IntervalExeState proposed = domain.top(); /// proposed = self.top.copy()
        proposed._varToItvVal[it->first] = lower._varToItvVal.at(it->first);
        /// proposed.set_interval(variable, lower.interval_of(variable))
        /// proposed._locToItvVal
        if (!(proposed >= upper)) /// if not proposed >= upper:
        {
            return proposed; /// return proposed
        }
    }
    return lower; /// return lower.copy()
}

Z3Expr RelationSolver::gamma_hat(const IntervalExeState& exeState) const
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        if (item.second.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (item.second.isTop())
            continue;
        Z3Expr v = toZ3Expr(item.first);
        res = (res && v >= (int)item.second.lb().getNumeral() &&
               v <= (int)item.second.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(const IntervalExeState& alpha,
                                 const IntervalExeState& exeState) const
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        IntervalValue interval = alpha._varToItvVal.at(item.first);
        if (interval.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (interval.isTop())
            continue;
        Z3Expr v = toZ3Expr(item.first);
        res = (res && v >= (int)interval.lb().getNumeral() &&
               v <= (int)interval.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(u32_t id, const IntervalExeState& exeState) const
{
    auto it = exeState.getVarToVal().find(id);
    assert(it != exeState.getVarToVal().end() && "id not in varToVal?");
    Z3Expr v = toZ3Expr(id);
    // Z3Expr v = Z3Expr::getContext().int_const(std::to_string(id).c_str());
    Z3Expr res = (v >= (int)it->second.lb().getNumeral() &&
                  v <= (int)it->second.ub().getNumeral());
    return res;
}

IntervalExeState RelationSolver::beta(const Map<u32_t, double>& sigma,
                                      const IntervalExeState& exeState) const
{
    IntervalExeState res;
    for (const auto& item : exeState.getVarToVal())
    {
        res._varToItvVal[item.first] = IntervalValue(
                                           sigma.at(item.first), sigma.at(item.first));
    }
    return res;
}

void RelationSolver::updateMap(Map<u32_t, NumericLiteral>& map, u32_t key, const NumericLiteral& value)
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

IntervalExeState RelationSolver::BS(const IntervalExeState& domain, const Z3Expr &phi)
{
    /// because key of _varToItvVal is u32_t, -key may out of range for int
    /// so we do key + bias for -key
    u32_t bias = 0;
    int infinity = (INT32_MAX) - 1;
    // int infinity = 20;
    Map<u32_t, NumericLiteral> ret;
    Map<u32_t, NumericLiteral> low_values, high_values;
    Z3Expr new_phi = phi;
    /// init low, ret, high
    for (const auto& item: domain.getVarToVal())
    {
        updateMap(ret, item.first, item.second.ub());
        if (item.second.lb().is_minus_infinity())
            updateMap(low_values, item.first, -infinity);
        else
            updateMap(low_values, item.first, item.second.lb());
        if (item.second.ub().is_plus_infinity())
            updateMap(high_values, item.first, infinity);
        else
            updateMap(high_values, item.first, item.second.ub());
        if (item.first > bias)
            bias = item.first + 1;
    }
    for (const auto& item: domain.getVarToVal())
    {
        /// init objects -x
        u32_t reverse_key = item.first + bias;
        updateMap(ret, reverse_key, -item.second.lb());
        if (item.second.ub().is_plus_infinity())
            updateMap(low_values, reverse_key, -infinity);
        else
            updateMap(low_values, reverse_key, -item.second.ub());
        if (item.second.lb().is_minus_infinity())
            updateMap(high_values, reverse_key, infinity);
        else
            updateMap(high_values, reverse_key, -item.second.lb());
        /// add a relation that x == -(x+bias)
        new_phi = (new_phi && (toZ3Expr(reverse_key) == -1 * toZ3Expr(item.first)));
    }
    /// optimize each object
    BoxedOptSolver(new_phi.simplify(), ret, low_values, high_values);
    /// fill in the return values
    IntervalExeState retInv;
    for (const auto& item: ret)
    {
        if (item.first >= bias)
        {
            if (item.second.equal(infinity))
                retInv._varToItvVal[item.first - bias].setLb(Z3Expr::getContext().int_const("-oo"));
            else
                retInv._varToItvVal[item.first - bias].setLb(-item.second);
        }
        else
        {
            if (item.second.equal(infinity))
                retInv._varToItvVal[item.first].setUb(Z3Expr::getContext().int_const("+oo"));
            else
                retInv._varToItvVal[item.first].setUb(item.second);
        }
    }
    return retInv;
}

Map<u32_t, NumericLiteral> RelationSolver::BoxedOptSolver(const Z3Expr& phi, Map<u32_t, NumericLiteral>& ret, Map<u32_t, NumericLiteral>& low_values, Map<u32_t, NumericLiteral>& high_values)
{
    /// this is the S in the original paper
    Map<u32_t, Z3Expr> L_phi;
    Map<u32_t, NumericLiteral> mid_values;
    while (1)
    {
        L_phi.clear();
        for (const auto& item : ret)
        {
            Z3Expr v = toZ3Expr(item.first);
            if (low_values.at(item.first).leq(high_values.at(item.first)))
            {
                NumericLiteral mid = (low_values.at(item.first) + (high_values.at(item.first) - low_values.at(item.first)) / 2);
                updateMap(mid_values, item.first, mid);
                Z3Expr expr = ((int)mid.getNumeral() <= v && v <= (int)high_values.at(item.first).getNumeral());
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
                                    Map<u32_t, NumericLiteral>& mid_values,
                                    Map<u32_t, NumericLiteral>& ret,
                                    Map<u32_t, NumericLiteral>& low_values,
                                    Map<u32_t, NumericLiteral>& high_values)
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
                int value = m.eval(toZ3Expr(id).getExpr()).get_numeral_int();
                // int value = m.eval(Z3Expr::getContext().int_const(std::to_string(id).c_str())).get_numeral_int();
                /// id is the var id, value is the solution found for var_id
                /// add a relation to check if the solution meets phi_id
                Z3Expr expr = (item.second && toZ3Expr(id) == value);
                solver.push();
                solver.add(expr.getExpr());
                // solution meets phi_id
                if (solver.check() == z3::sat)
                {
                    updateMap(ret, id, NumericLiteral(value));
                    updateMap(low_values, id, ret.at(id) + 1);

                    NumericLiteral mid = (low_values.at(id) + high_values.at(id) + 1) / 2;
                    updateMap(mid_values, id, mid);
                    Z3Expr v = toZ3Expr(id);
                    // Z3Expr v = Z3Expr::getContext().int_const(std::to_string(id).c_str());
                    Z3Expr expr = ((int)mid_values.at(id).getNumeral() <= v && v <= (int)high_values.at(id).getNumeral());
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
