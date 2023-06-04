//===- BoundedZ3Expr.cpp ----Address Value Sets-------------------------//
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
 * BoundedZ3Expr.cpp
 *
 *  Created on: Mar 20, 2023
 *      Author: Xiao Cheng
 *
 */
#include "AbstractExecution/BoundedZ3Expr.h"
#include "Util/Options.h"

using namespace SVF;

int64_t BoundedZ3Expr::bvLen() const
{
    if(is_infinite()) return Options::MaxBVLen();
    // No overflow
    if(getNumeral() != INT64_MIN && getNumeral() != INT64_MAX) return Options::MaxBVLen();
    // Create a symbolic variable
    Z3Expr x = getContext().real_const("x");
    Z3Expr y = getContext().real_const("y");

    // Add constraints and assertions
    Z3Expr constraint1 = x > 0;                          // x > 0
    Z3Expr constraint2 = x == z3::pw(2, y.getExpr()); // x = 2^y, where y is a real variable
    Z3Expr assertions = constraint1 && constraint2;
    Z3Expr::getSolver().push();
    // Add assertions to the solver
    Z3Expr::getSolver().add(assertions.getExpr());

    // Check for a solution
    if (solver->check() == z3::sat)
    {
        z3::model model = solver->get_model();
        Z3Expr log2_x = model.eval(y.getExpr(), true);
        Z3Expr::getSolver().pop();
        return BoundedZ3Expr(log2_x + 1).simplify().getNumeral();
    }
    else
    {
        Z3Expr::getSolver().pop();
        return Options::MaxBVLen();
    }
}