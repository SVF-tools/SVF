//===- SymState.h -- Symbolic State----------------------------//
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
 * SymState.h
 *
 *  Created on: 2022/6/21
 *      Author: Xiao Cheng
 */

#ifndef SVF_SYMSTATE_H
#define SVF_SYMSTATE_H

#include "ExeState.h"

namespace SVF
{
class SymState
{
public:
    typedef std::string AbstractState;

    SymState(const ExeState &_es, const AbstractState &_as) : exeState(_es), absState(_as)
    {

    }

    virtual ~SymState()
    {

    }

    SymState(const SymState &symState) : absState(symState.getAbstractState()),
        exeState(symState.getExecutionState())
    {
    }


    const AbstractState& getAbstractState() const
    {
        return absState;
    }

    const ExeState& getExecutionState() const
    {
        return exeState;
    }

    SymState &operator=(const SymState &rhs)
    {
        if (*this != rhs)
        {
            exeState = rhs.getExecutionState();
            absState = rhs.getAbstractState();
        }
        return *this;
    }

    /// Overloading Operator==
    inline bool operator==(const SymState &rhs) const
    {
        return absState == rhs.getAbstractState() && exeState = rhs.getExecutionState();
    }

    /// Overloading Operator!=
    inline bool operator!=(const SymState &rhs) const
    {
        return !(*this == rhs);
    }

    /// Overloading Operator==
    inline bool operator<(const SymState &rhs) const
    {
        if (absState != rhs.getAbstractState())
            return absState < rhs.getAbstractState();
        if (exeState != rhs.getExecutionState())
            return exeState < rhs.getExecutionState();
        return false;
    }

private:
    ExeState exeState;
    AbstractState absState;
};
}

template<>
struct std::hash<SVF::SymState>
{
    size_t operator()(const SVF::SymState &symState) const
    {

        SVF::Hash<std::pair<SVF::AbstractState, SVF::ExeState>> pairH;

        return pairH(make_pair(symState.getAbstractState(), symState.getExecutionState()));
    }
};

#endif //SVF_SYMSTATE_H
