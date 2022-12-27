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
    IntervalExeState (the last element of inputs) for RSY or bilateral solver */

    /// Return Z3Expr according to valToValMap
    Z3Expr gamma_hat(IntervalExeState &exeState);

    /// Return Z3Expr according to another valToValMap
    Z3Expr gamma_hat(IntervalExeState &alpha, IntervalExeState &exeState);

    /// Return Z3Expr from a NodeID
    Z3Expr gamma_hat(u32_t id, IntervalExeState &exeState);

    IntervalExeState abstract_consequence(IntervalExeState &lower, IntervalExeState &upper, IntervalExeState &domain);

    IntervalExeState beta(Map<u32_t, double> &sigma, IntervalExeState &exeState);

    /* two optional solvers: RSY and bilateral */

    IntervalExeState bilateral(IntervalExeState domain, Z3Expr phi, u32_t descend_check = 0);

    IntervalExeState RSY(IntervalExeState domain, const Z3Expr &phi);

};
}

#endif //Z3_EXAMPLE_RELATIONSOLVER_H
