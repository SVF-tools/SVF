//===- RelationSolver.h ----Relation Solver for Interval Domains-----------//
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
 * RelationSolver.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Jiawei Ren
 *
 */

#ifndef Z3_EXAMPLE_RELATIONSOLVER_H
#define Z3_EXAMPLE_RELATIONSOLVER_H

#include "AE/Core/AbstractState.h"
#include "Util/Z3Expr.h"

namespace SVF
{
class RelationSolver
{
public:
    RelationSolver() = default;

    /* gamma_hat, beta and abstract_consequence works on
    IntervalESBase (the last element of inputs) for RSY or bilateral solver */

    /// Return Z3Expr according to valToValMap
    Z3Expr gamma_hat(const AbstractState&exeState) const;

    /// Return Z3Expr according to another valToValMap
    Z3Expr gamma_hat(const AbstractState&alpha, const AbstractState&exeState) const;

    /// Return Z3Expr from a NodeID
    Z3Expr gamma_hat(u32_t id, const AbstractState&exeState) const;

    AbstractState abstract_consequence(const AbstractState&lower, const AbstractState&upper, const AbstractState&domain) const;

    AbstractState beta(const Map<u32_t, s32_t> &sigma, const AbstractState&exeState) const;


    /// Return Z3 expression lazily based on SVFVar ID
    virtual inline Z3Expr toIntZ3Expr(u32_t varId) const
    {
        return Z3Expr::getContext().int_const(std::to_string(varId).c_str());
    }

    inline Z3Expr toIntVal(s32_t f) const
    {
        return Z3Expr::getContext().int_val(f);
    }
    inline Z3Expr toRealVal(BoundedDouble f) const
    {
        return Z3Expr::getContext().real_val(std::to_string(f.getFVal()).c_str());
    }

    /* two optional solvers: RSY and bilateral */

    AbstractState bilateral(const AbstractState& domain, const Z3Expr &phi, u32_t descend_check = 0);

    AbstractState RSY(const AbstractState& domain, const Z3Expr &phi);

    Map<u32_t, s32_t> BoxedOptSolver(const Z3Expr& phi, Map<u32_t, s32_t>& ret, Map<u32_t, s32_t>& low_values, Map<u32_t, s32_t>& high_values);

    AbstractState BS(const AbstractState& domain, const Z3Expr &phi);

    void updateMap(Map<u32_t, s32_t>& map, u32_t key, const s32_t& value);

    void decide_cpa_ext(const Z3Expr &phi, Map<u32_t, Z3Expr>&, Map<u32_t, s32_t>&, Map<u32_t, s32_t>&, Map<u32_t, s32_t>&, Map<u32_t, s32_t>&);
};
}

#endif //Z3_EXAMPLE_RELATIONSOLVER_H
