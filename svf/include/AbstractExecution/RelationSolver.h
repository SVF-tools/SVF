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

#include "AbstractExecution/IntervalExeState.h"
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
    Z3Expr gamma_hat(const IntervalESBase &exeState) const;

    /// Return Z3Expr according to another valToValMap
    Z3Expr gamma_hat(const IntervalESBase &alpha, const IntervalESBase &exeState) const;

    /// Return Z3Expr from a NodeID
    Z3Expr gamma_hat(u32_t id, const IntervalESBase &exeState) const;

    IntervalESBase abstract_consequence(const IntervalESBase &lower, const IntervalESBase &upper, const IntervalESBase &domain) const;

    IntervalESBase beta(const Map<u32_t, double> &sigma, const IntervalESBase &exeState) const;


    /// Return Z3 expression lazily based on SVFVar ID
    virtual inline Z3Expr toZ3Expr(u32_t varId) const
    {
        return Z3Expr::getContext().int_const(std::to_string(varId).c_str());
    }

    /* two optional solvers: RSY and bilateral */

    IntervalESBase bilateral(const IntervalESBase& domain, const Z3Expr &phi, u32_t descend_check = 0);

    IntervalESBase RSY(const IntervalESBase& domain, const Z3Expr &phi);

    Map<u32_t, NumericLiteral> BoxedOptSolver(const Z3Expr& phi, Map<u32_t, NumericLiteral>& ret, Map<u32_t, NumericLiteral>& low_values, Map<u32_t, NumericLiteral>& high_values);

    IntervalESBase BS(const IntervalESBase& domain, const Z3Expr &phi);

    void updateMap(Map<u32_t, NumericLiteral>& map, u32_t key, const NumericLiteral& value);

    void decide_cpa_ext(const Z3Expr &phi, Map<u32_t, Z3Expr>&, Map<u32_t, NumericLiteral>&, Map<u32_t, NumericLiteral>&, Map<u32_t, NumericLiteral>&, Map<u32_t, NumericLiteral>&);
};
}

#endif //Z3_EXAMPLE_RELATIONSOLVER_H
