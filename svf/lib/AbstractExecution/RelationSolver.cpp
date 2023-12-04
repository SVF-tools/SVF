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

IntervalExeState RelationSolver::bilateral(IntervalExeState domain, Z3Expr phi,
                                           u32_t descend_check)
{
    /// init variables
    IntervalExeState upper = domain.top();
    IntervalExeState lower = domain.bottom();
    u32_t meets_in_a_row = 0;
    z3::solver solver = Z3Expr::getSolver();
    z3::params p(Z3Expr::getContext());
    /// TODO: add option for timeout
    p.set(":timeout", static_cast<unsigned>(1)); // in milliseconds
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
                assert(v.arity() == 0);
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

IntervalExeState RelationSolver::RSY(IntervalExeState domain, const Z3Expr& phi)
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
            // IntervalExeState rhs = beta(solution, domain);
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
    IntervalExeState& lower, IntervalExeState& upper, IntervalExeState& domain)
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
        proposed._varToItvVal[it->first] = lower._varToItvVal[it->first];
        /// proposed.set_interval(variable, lower.interval_of(variable))
               /// proposed._locToItvVal
        if (!(proposed >= upper)) /// if not proposed >= upper:
        {
            return proposed; /// return proposed
        }
    }
    return lower; /// return lower.copy()
}

Z3Expr RelationSolver::gamma_hat(IntervalExeState& exeState)
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        if (item.second.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (item.second.isTop())
            continue;
        Z3Expr v = Z3Expr::getContext().int_const(
            std::to_string(item.first).c_str());
        res = (res && v >= (int)item.second.lb().getNumeral() &&
               v <= (int)item.second.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(IntervalExeState& alpha,
                                 IntervalExeState& exeState)
{
    Z3Expr res(Z3Expr::getContext().bool_val(true));
    for (auto& item : exeState.getVarToVal())
    {
        IntervalValue interval = alpha._varToItvVal[item.first];
        if (interval.isBottom())
            return Z3Expr::getContext().bool_val(false);
        if (interval.isTop())
            continue;
        Z3Expr v = Z3Expr::getContext().int_const(
            std::to_string(item.first).c_str());
        res = (res && v >= (int)interval.lb().getNumeral() &&
               v <= (int)interval.ub().getNumeral()).simplify();
    }
    return res;
}

Z3Expr RelationSolver::gamma_hat(u32_t id, IntervalExeState& exeState)
{
    auto it = exeState.getVarToVal().find(id);
    assert(it != exeState.getVarToVal().end() && "id not in varToVal?");
    Z3Expr v = Z3Expr::getContext().int_const(std::to_string(id).c_str());
    Z3Expr res = (v >= (int)it->second.lb().getNumeral() &&
                  v <= (int)it->second.ub().getNumeral());
    return res;
}

IntervalExeState RelationSolver::beta(Map<u32_t, double>& sigma,
                                      IntervalExeState& exeState)
{
    IntervalExeState res;
    for (const auto& item : exeState.getVarToVal())
    {
        res._varToItvVal[item.first] = IntervalValue(
            sigma[item.first], sigma[item.first]);
    }
    return res;
}


Map<u32_t, NumericLiteral> RelationSolver::BS(IntervalExeState domain, const Z3Expr& phi)
{
    Map<u32_t, Z3Expr> L_phi;
    IntervalExeState copy = domain;
    Map<u32_t, NumericLiteral> mid_values, ret;
    for (auto& item: domain.getVarToVal())
    {
        ret[item.first] = item.second.ub();
    }
    // const u32_t infinity_value = 4000000;
    while (1)
    {
        L_phi.clear();
        for (auto& item : copy.getVarToVal())
        {
            Z3Expr v = Z3Expr::getContext().int_const(std::to_string(item.first).c_str());
            if (item.second.isBottom())
            {
                outs() << item.second.toString() << "is bottom !\n";
                L_phi[item.first] = (1 <= v && v <= -1);
                continue;
            }
            if (item.second.lb().leq(item.second.ub()))
            {
                outs() << item.second.toString() << "is normal !\n";
                NumericLiteral mid = (item.second.lb().getNumeral() + item.second.ub().getNumeral()) / 2;
                Z3Expr expr = ((int)mid.getNumeral() <= v && v <= (int)item.second.ub().getNumeral());
                mid_values[item.first] = mid;
                L_phi[item.first] = expr;
            }

        }
        if (L_phi.empty())
            break;
        else
            decide_cpa_ext(copy, phi, L_phi, mid_values, ret);
        for (auto item : copy.getVarToVal())
        {
            outs() << "L280: "<< item.first << " " << copy._varToItvVal[item.first].toString() << "\n";
        }
        // return copy;
    }
    return ret;
}


void RelationSolver::decide_cpa_ext(IntervalExeState& domain, const Z3Expr& phi,
                                    Map<u32_t, Z3Expr>& L_phi,
                                    Map<u32_t, NumericLiteral>& mid_values,
                                    Map<u32_t, NumericLiteral>& ret)
{
    // Map<u32_t, Z3Expr> copied_L_phi(L_phi.begin(), L_phi.end());
    // L_phi.clear();
    while (1)
    {
        Z3Expr join_expr(Z3Expr::getContext().bool_val(false));
        for (auto& item : L_phi)
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
            for (u32_t i = 0; i < m.size(); i++)
            {
                z3::func_decl v = m[i];
                if (v.arity() != 0)
                    continue;
                u32_t id = std::stoi(v.name().str());
                int value = m.get_const_interp(v).get_numeral_int();
                outs() << "L329: " << L_phi[id].to_string() << "\n";
                Z3Expr expr = (L_phi[id] && Z3Expr::getContext().int_const(
                                   std::to_string(id).c_str()) == value);
                solver.push();
                solver.add(expr.getExpr());
                if (solver.check() == z3::sat)
                {
                    ret[id] = NumericLiteral(value);
                    domain._varToItvVal[id].setLb(
                        (NumericLiteral(value) + NumericLiteral(1)));
                    // if (domain._varToItvVal[id].lb().geq(domain._varToItvVal[id].ub()))
                    //     domain._varToItvVal.erase(id);
                    Z3Expr v = Z3Expr::getContext().int_const(
                        std::to_string(id).c_str());
                    if (domain._varToItvVal[id].isBottom())
                    {
                        L_phi[id] = (1 <= v && v <= -1);;
                        continue;
                    }
                    NumericLiteral mid = NumericLiteral(
                    (domain._varToItvVal[id].lb().getNumeral() + domain.
                     _varToItvVal[id].ub().getNumeral()) / 2);
                    Z3Expr expr = (
                        (int)mid.getNumeral() <= v && v <= (int)domain.
                        _varToItvVal[id].ub().getNumeral());
                    mid_values[id] = mid;
                    L_phi[id] = expr;
                }
                solver.pop();
            }
        }
        else /// unknown or unsat
        {
            outs() << "unsat\n";
            solver.pop();
            for (auto& item : domain.getVarToVal())
            {
                domain._varToItvVal[item.first].setUb(
                    (mid_values[item.first] - NumericLiteral(1)));
                outs() << "L365: " << domain._varToItvVal[item.first].toString() << "\n";
                // if (domain._varToItvVal[item.first].lb().geq(domain._varToItvVal[item.first].ub()))
                //     domain._varToItvVal.erase(item.first);
                // outs() << domain[item.first].toString() <<std::endl;
            }
            return;
        }
    }

}
