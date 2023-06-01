//===- ConsExeState.h ----Constant Execution State-------------------------//
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

#ifndef SVF_CONSEXESTATE_H
#define SVF_CONSEXESTATE_H

#include "AbstractExecution/SingleAbsValue.h"
#include "AbstractExecution/ExeState.h"

namespace SVF
{

/*!
 * Constant Expr Execution State
 *
 * Constant expr execution state support z3 symbolic value
 * and gives a top value when two different constants join
 *
 * lattice:          ⊤                      may be constant
 *         /    /   |   \  \    \
 *       true ...  c0  c1 ...  false        constant
 *         \    \   \   |  |    |
 *                   ⊥                      not contant
 */
class ConsExeState final : public ExeState
{
    friend class SVFIR2ConsExeState;

public:
    typedef Map<u32_t, SingleAbsValue> VarToValMap;
    typedef VarToValMap LocToValMap;

    static ConsExeState globalConsES;

protected:
    VarToValMap _varToVal;
    LocToValMap _locToVal;

public:
    ConsExeState(): ExeState(ExeState::SingleValueK) {}


    /// Constructor
    ConsExeState(VarToValMap varToValMap, LocToValMap locToValMap) : ExeState(ExeState::SingleValueK), _varToVal(
            SVFUtil::move(varToValMap)), _locToVal(SVFUtil::move(locToValMap))  {}

    /// Copy Constructor
    ConsExeState(const ConsExeState &rhs) : ExeState(rhs), _varToVal(rhs.getVarToVal()),
        _locToVal(rhs.getLocToVal())
    {

    }

    /// Destructor
    ~ConsExeState() override
    {
        _varToVal.clear();
        _locToVal.clear();
    }

    /// Copy operator
    ConsExeState &operator=(const ConsExeState &rhs);

    /// Move Constructor
    ConsExeState(ConsExeState &&rhs) noexcept: ExeState(std::move(rhs)),
        _varToVal(SVFUtil::move(rhs._varToVal)), _locToVal(SVFUtil::move(rhs._locToVal))
    {

    }

    /// Move operator
    ConsExeState &operator=(ConsExeState &&rhs) noexcept;

    /// Name
    static inline std::string name()
    {
        return "ConstantExpr";
    }

    /// Exposed APIs
    //{%
    bool operator==(const ConsExeState &rhs) const;

    bool operator!=(const ConsExeState &other) const
    {
        return !(*this == other);
    }

    bool operator<(const ConsExeState &rhs) const;

    u32_t hash() const override;

    /// Merge rhs into this
    bool joinWith(const ConsExeState &rhs);

    /// Build global execution state
    void buildGlobES(ConsExeState &globES, Set<u32_t> &vars);

    /// Update symbolic states based on the summary/side-effect of callee
    void applySummary(const ConsExeState &summary);

    /// Whether state is null state (uninitialized state)
    inline bool isNullState() const
    {
        return _varToVal.size() == 1 && eq((*_varToVal.begin()).second, -1) && _locToVal.empty();
    }

    /// Print values of all expressions
    void printExprValues() const;

    /// Print values of all expressions
    void printExprValues(std::ostream &oss) const override;

    std::string toString() const override;

    std::string pcToString() const;

    std::string varToString(u32_t varId) const;

    std::string locToString(u32_t objId) const;

    bool applySelect(u32_t res, u32_t cond, u32_t top, u32_t fop);

    bool applyPhi(u32_t res, std::vector<u32_t> &ops);

    inline bool inVarToVal(u32_t varId) const
    {
        return _varToVal.count(varId) || globalConsES._varToVal.count(varId);
    }

    inline bool inLocalLocToVal(const SingleAbsValue &loc) const
    {
        assert(loc.is_numeral() && "location must be numeral");
        s32_t virAddr = z3Expr2NumValue(loc);
        assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
        u32_t objId = getInternalID(virAddr);
        assert(getInternalID(objId) == objId && "SVFVar idx overflow > 0x7f000000?");
        return inLocalLocToVal(objId);
    }

    inline bool inLocalLocToVal(u32_t varId) const
    {
        return _locToVal.count(varId);
    }

    inline bool inLocToVal(u32_t varId) const
    {
        return inLocalLocToVal(varId) || globalConsES._locToVal.count(varId);
    }

    inline bool equalVar(u32_t lhs, u32_t rhs)
    {
        if (!inVarToVal(lhs) || !inVarToVal(rhs)) return false;
        return eq((*this)[lhs], (*this)[rhs]);
    }
    //%}

    virtual inline bool inVarToAddrsTable(u32_t id) const override
    {
        return _varToVAddrs.find(id) != _varToVAddrs.end() ||
               globalConsES._varToVAddrs.find(id) != globalConsES._varToVAddrs.end();
    }

    virtual inline bool inLocToAddrsTable(u32_t id) const override
    {
        return globalConsES._locToVAddrs.find(id) != globalConsES._locToVAddrs.end() || inLocalLocToAddrsTable(id);
    }

    virtual inline bool inLocalLocToAddrsTable(u32_t id) const
    {
        return _locToVAddrs.find(id) != _locToVAddrs.end();
    }

    virtual VAddrs &getVAddrs(u32_t id) override
    {
        auto it = globalConsES._varToVAddrs.find(id);
        if (it != globalConsES._varToVAddrs.end()) return it->second;
        return _varToVAddrs[id];
    }

    virtual VAddrs &loadVAddrs(u32_t addr) override
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToVAddrs.find(objId);
        if (it != _locToVAddrs.end())
        {
            return it->second;
        }
        else
        {
            auto globIt = globalConsES._locToVAddrs.find(objId);
            if (globIt != globalConsES._locToVAddrs.end())
            {
                return globIt->second;
            }
            else
            {
                return getVAddrs(0);
            }
        }
    }

    virtual std::string varToAddrs(u32_t varId) const override
    {
        std::stringstream exprName;
        auto it = _varToVAddrs.find(varId);
        if (it == _varToVAddrs.end())
        {
            auto git = globalConsES._varToVAddrs.find(varId);
            if (git == globalConsES._varToVAddrs.end())
                exprName << "Var not in varToAddrs!\n";
            else
            {
                const VAddrs &vaddrs = git->second;
                if (vaddrs.size() == 1)
                {
                    exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
                }
                else
                {
                    exprName << "addr: {";
                    for (const auto &addr: vaddrs)
                    {
                        exprName << std::dec << getInternalID(addr) << ", ";
                    }
                    exprName << "}\n";
                }
            }
        }
        else
        {
            const VAddrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
    }

    virtual std::string locToAddrs(u32_t objId) const override
    {
        std::stringstream exprName;
        auto it = _locToVAddrs.find(objId);
        if (it == _locToVAddrs.end())
        {
            auto git = globalConsES._locToVAddrs.find(objId);
            if (git == globalConsES._locToVAddrs.end())
                exprName << "Obj not in locToVal!\n";
            else
            {
                const VAddrs &vaddrs = git->second;
                if (vaddrs.size() == 1)
                {
                    exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
                }
                else
                {
                    exprName << "addr: {";
                    for (const auto &addr: vaddrs)
                    {
                        exprName << std::dec << getInternalID(addr) << ", ";
                    }
                    exprName << "}\n";
                }
            }
        }
        else
        {
            const VAddrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
    }

    /// Empty execution state with a true path constraint
    static inline ConsExeState initExeState()
    {
        VarToValMap mp;
        ConsExeState exeState(mp, SVFUtil::move(mp));
        return SVFUtil::move(exeState);
    }

    /// Empty execution state with a null expr
    static inline ConsExeState nullExeState()
    {
        VarToValMap mp;
        ConsExeState exeState(mp, SVFUtil::move(mp));
        exeState._varToVal[PAG::getPAG()->getNullPtr()] = -1;
        return SVFUtil::move(exeState);
    }

public:

    inline const VarToValMap &getVarToVal() const
    {
        return _varToVal;
    }

    inline const LocToValMap &getLocToVal() const
    {
        return _locToVal;
    }


    s64_t getNumber(u32_t lhs);

public:


    static inline SingleAbsValue getIntOneZ3Expr()
    {
        return getContext().int_val(1);
    }

    static inline SingleAbsValue getIntZeroZ3Expr()
    {
        return getContext().int_val(0);
    }

    static inline SingleAbsValue getTrueZ3Expr()
    {
        return getContext().bool_val(true);
    }

    static inline SingleAbsValue getFalseZ3Expr()
    {
        return getContext().bool_val(false);
    }

public:
    inline SingleAbsValue &operator[](u32_t varId)
    {
        auto it = globalConsES._varToVal.find(varId);
        if (it != globalConsES._varToVal.end())
            return it->second;
        else
            return _varToVal[varId];
    }

    /// Store value to location
    bool store(const SingleAbsValue &loc, const SingleAbsValue &value);

    /// Load value at location
    SingleAbsValue load(const SingleAbsValue &loc);

    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    static inline s32_t z3Expr2NumValue(const SingleAbsValue &e)
    {
        assert(e.is_numeral() && "not numeral?");
        int64_t i;
        if(e.getExpr().is_numeral_i64(i))
            return e.get_numeral_int64();
        else
        {
            return e.leq(0) ? INT32_MIN : INT32_MAX;
        }
    }

    /// Whether two var to value map is equivalent
    static bool eqVarToValMap(const VarToValMap &pre, const VarToValMap &nxt);


    /// Whether lhs is less than rhs
    static bool lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs);


private:


    static bool assign(SingleAbsValue &lhs, const SingleAbsValue &rhs);

    inline bool store(u32_t objId, const SingleAbsValue &z3Expr)
    {
        SingleAbsValue &lhs = _locToVal[objId];
        if (!eq(lhs, z3Expr.simplify()))
        {
            lhs = z3Expr.simplify();
            return true;
        }
        else
            return false;
    }

    inline SingleAbsValue load(u32_t objId);
}; // end class ConExeState

} // end namespace SVF

/// Specialized hash for ConExeState
template<>
struct std::hash<SVF::ConsExeState>
{
    size_t operator()(const SVF::ConsExeState &exeState) const
    {
        return exeState.hash();
    }
};



#endif // SVF_CONSEXESTATE_H
