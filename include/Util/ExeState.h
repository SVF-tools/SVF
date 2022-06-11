//===- ExeState.h -- Execution State ----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/*
 * ExeState.h
 *
 *  Created on: April 29, 2022
 *      Author: Xiao
 */

#ifndef SVF_EXESTATE_H
#define SVF_EXESTATE_H
#include "Util/Z3Expr.h"
#include "MemoryModel/SVFVariables.h"

namespace SVF
{

#define AddressMask 0x7f000000
#define FlippedAddressMask (AddressMask^0xffffffff)

class ExeState
{
public:
    typedef Map<u32_t, Z3Expr> VarToValMap;
    typedef VarToValMap LocToValMap;

protected:
    VarToValMap varToVal;
    LocToValMap locToVal;
    Z3Expr pathConstraint;

public:
    ExeState() : pathConstraint(getContext().bool_val(true)) {}

    ExeState(const Z3Expr &_pc, VarToValMap &_varToValMap, LocToValMap &_locToValMap) : varToVal(_varToValMap),
        locToVal(_locToValMap),
        pathConstraint(_pc) {}

    ExeState(const ExeState &rhs) : varToVal(rhs.getVarToVal()), locToVal(rhs.getLocToVal()),
        pathConstraint(rhs.getPathConstraint())
    {

    }

    virtual ~ExeState() = default;

    ExeState &operator=(const ExeState &rhs);

    /// Overloading Operator==
    bool operator==(const ExeState &rhs) const;

    /// Overloading Operator!=
    inline bool operator!=(const ExeState &rhs) const
    {
        return !(*this == rhs);
    }

    /// Overloading Operator==
    bool operator<(const ExeState &rhs) const;



    z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    const VarToValMap &getVarToVal() const
    {
        return varToVal;
    }

    const LocToValMap &getLocToVal() const
    {
        return locToVal;
    }

    const Z3Expr &getPathConstraint() const
    {
        return pathConstraint;
    }

    void setPathConstraint(const Z3Expr &pc)
    {
        pathConstraint = pc.simplify();
    }

    inline Z3Expr &operator[](u32_t varId)
    {
        return getZ3Expr(varId);
    }

    /// Init Z3Expr for ValVar
    void initValVar(const ValVar *valVar, Z3Expr &e);

    /// Init Z3Expr for ObjVar
    void initObjVar(const ObjVar *objVar, Z3Expr &e);

    /// Return Z3 expression based on SVFVar ID
    Z3Expr &getZ3Expr(u32_t varId);

    /// Store value to location
    void store(const Z3Expr &loc, const Z3Expr &value);

    /// Load value at location
    Z3Expr &load(const Z3Expr &loc);

    /// The physical address starts with 0x7f...... + idx
    inline u32_t getVirtualMemAddress(u32_t idx) const
    {
        return AddressMask + idx;
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    inline bool isVirtualMemAddress(u32_t val)
    {
        return (val & 0xff000000) == AddressMask;
    }

    /// Return the internal index if idx is an address otherwise return the value of idx
    inline u32_t getInternalID(u32_t idx) const
    {
        return (idx & FlippedAddressMask);
    }

    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    inline s32_t z3Expr2NumValue(const Z3Expr &e)
    {
        assert(e.is_numeral() && "not numeral?");
        return e.get_numeral_int64();
    }

    /// Print values of all expressions
    void printExprValues();

private:
    bool eqVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const;
    bool lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const;
protected:
    inline void store(u32_t objId, const Z3Expr &z3Expr)
    {
        locToVal[objId] = z3Expr.simplify();
    }

    inline Z3Expr &load(u32_t objId)
    {
        return locToVal[objId];
    }
};
}

template<>
struct std::hash<SVF::ExeState>
{
    size_t operator()(const SVF::ExeState &exeState) const
    {

        size_t h = exeState.getVarToVal().size() * 2;
        SVF::Hash<SVF::u32_t> hf;
        for (const auto &t: exeState.getVarToVal())
        {
            h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hf(t.second.id()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        size_t h2 = exeState.getVarToVal().size() * 2;

        for (const auto &t: exeState.getLocToVal())
        {
            h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
            h2 ^= hf(t.second.id()) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
        }
        SVF::Hash<std::pair<SVF::u32_t, SVF::u32_t>> pairH;

        return pairH(make_pair(pairH(make_pair(h, h2)), exeState.getPathConstraint().id()));
    }
};
#endif //SVF_EXESTATE_H
