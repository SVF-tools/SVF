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

#ifndef Z3_EXAMPLE_Z3EXPR_H
#define Z3_EXAMPLE_Z3EXPR_H

#include "z3++.h"
#include "SVFIR/SVFType.h"

namespace SVF
{

class Z3Expr
{
public:
    static z3::context *ctx;
    static z3::solver* solver;

private:
    z3::expr e;

    Z3Expr(float f); // placeholder don't support floating point expression
    Z3Expr(double f); // placeholder don't support floating point expression

public:

    Z3Expr() : e(nullExpr())
    {
    }

    Z3Expr(const z3::expr &_e) : e(_e)
    {
    }

    Z3Expr(int i) : e(getContext().int_val(i))
    {
    }

    Z3Expr(const Z3Expr &z3Expr) : e(z3Expr.getExpr())
    {
    }

    virtual ~Z3Expr() = default;

    inline Z3Expr &operator=(const Z3Expr &rhs)
    {
        if (this->id() != rhs.id())
        {
            e = rhs.getExpr();
        }
        return *this;
    }

    const z3::expr &getExpr() const
    {
        return e;
    }

    /// Get z3 solver, singleton design here to make sure we only have one context
    static z3::solver &getSolver();

    /// Get z3 context, singleton design here to make sure we only have one context
    static z3::context &getContext();

    /// release z3 context
    static void releaseContext();

    /// release z3 solver
    static void releaseSolver();


    /// null expression
    static z3::expr nullExpr()
    {
        return getContext().int_const("null");
    }

    /// get id
    inline u32_t id() const
    {
        return e.id();
    }

    /// get hash id
    inline u32_t hash() const
    {
        return e.hash();
    }

    inline const std::string to_string() const
    {
        return e.to_string();
    }

    inline bool is_numeral() const
    {
        return e.is_numeral();
    }

    inline bool is_bool() const
    {
        return e.is_bool();
    }

    inline Z3Expr simplify() const
    {
        return e.simplify();
    }

    inline int64_t get_numeral_int64() const
    {
        return e.get_numeral_int64();
    }

    inline int get_numeral_int() const
    {
        return e.get_numeral_int();
    }

    //{% reload operator
    friend Z3Expr operator==(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() == rhs.getExpr();
    }

    friend Z3Expr operator!=(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() != rhs.getExpr();
    }

    friend Z3Expr operator>(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() > rhs.getExpr();
    }

    friend Z3Expr operator<(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() < rhs.getExpr();
    }

    friend Z3Expr operator<=(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() <= rhs.getExpr();
    }

    friend Z3Expr operator>=(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() >= rhs.getExpr();
    }

    friend Z3Expr operator+(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() + rhs.getExpr();
    }

    friend Z3Expr operator-(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() - rhs.getExpr();
    }

    friend Z3Expr operator*(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() * rhs.getExpr();
    }

    friend Z3Expr operator/(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() / rhs.getExpr();
    }

    friend Z3Expr operator%(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() % rhs.getExpr();
    }

    friend Z3Expr operator^(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() ^ rhs.getExpr();
    }

    friend Z3Expr operator&(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() & rhs.getExpr();
    }

    friend Z3Expr operator|(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() | rhs.getExpr();
    }

    friend Z3Expr ashr(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return ashr(lhs.getExpr(), rhs.getExpr());
    }

    friend Z3Expr shl(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return shl(lhs.getExpr(), rhs.getExpr());
    }

    friend Z3Expr int2bv(u32_t n, const Z3Expr &e)
    {
        return int2bv(n, e.getExpr());
    }

    friend Z3Expr bv2int(const Z3Expr &e, bool isSigned)
    {
        return z3::bv2int(e.getExpr(), isSigned);
    }

    friend Z3Expr operator&&(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() && rhs.getExpr();
    }

    friend Z3Expr operator||(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return lhs.getExpr() || rhs.getExpr();
    }

    friend Z3Expr operator!(const Z3Expr &lhs)
    {
        return !lhs.getExpr();
    }

    friend Z3Expr ite(const Z3Expr &cond, const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return ite(cond.getExpr(), lhs.getExpr(), rhs.getExpr());
    }

    friend std::ostream &operator<<(std::ostream &out, const Z3Expr &expr)
    {
        out << expr.getExpr();
        return out;
    }

    friend bool eq(const Z3Expr &lhs, const Z3Expr &rhs)
    {
        return eq(lhs.getExpr().simplify(), rhs.getExpr().simplify());
    }

    z3::sort get_sort() const
    {
        return getExpr().get_sort();
    }
    //%}

    /// output Z3 expression as a string
    static std::string dumpStr(const Z3Expr &z3Expr);


    /// get the number of subexpression of a Z3 expression
    static u32_t getExprSize(const Z3Expr &z3Expr);

    /// Return the unique true condition
    static inline Z3Expr getTrueCond()
    {
        return getContext().bool_val(true);
    }


    /// Return the unique false condition
    static inline Z3Expr getFalseCond()
    {
        return getContext().bool_val(false);
    }

    /// compute NEG
    static inline Z3Expr NEG(const Z3Expr &z3Expr)
    {
        return (!z3Expr).simplify();
    }

    /// compute AND, used for branch condition
    static Z3Expr AND(const Z3Expr &lhs, const Z3Expr &rhs);

    /// compute OR, used for branch condition
    static Z3Expr OR(const Z3Expr &lhs, const Z3Expr &rhs);
};
} // End namespace SVF

/// Specialise hash for AbsCxtDPItem.
template<>
struct std::hash<SVF::Z3Expr>
{
    size_t operator()(const SVF::Z3Expr &z3Expr) const
    {
        return z3Expr.id();
    }
};

#endif //Z3_EXAMPLE_Z3EXPR_H

