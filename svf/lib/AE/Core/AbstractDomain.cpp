//===- AbstractDomain.cpp -- Common abstract-property lattice API ------===//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//
// Contributors: Xiao Cheng, Jiawei Wang, Jiawei Yang
//
//===----------------------------------------------------------------------===//

#include "AE/Core/AbstractDomain.h"

#include <cstdlib>
#include <cassert>

namespace SVF
{

const char* toString(CheckResult result)
{
    switch (result)
    {
    case CheckResult::False:
        return "false";
    case CheckResult::True:
        return "true";
    case CheckResult::Unknown:
        return "unknown";
    }
    return "unknown";
}

AbstractDomain::~AbstractDomain() = default;

void AbstractDomain::requireCompatible(const AbstractDomain& other) const
{
    if (!hasCompatibleDomain(other))
    {
        assert(false && "abstract properties use incompatible domains or configurations");
        std::abort();
    }
}

void AbstractDomain::joinWith(const AbstractDomain& other)
{
    requireCompatible(other);
    joinDomain(other);
}

void AbstractDomain::meetWith(const AbstractDomain& other)
{
    requireCompatible(other);
    meetDomain(other);
}

void AbstractDomain::widenWith(const AbstractDomain& next)
{
    requireCompatible(next);
    widenDomain(next);
}

void AbstractDomain::narrowWith(const AbstractDomain& next)
{
    requireCompatible(next);
    if (!next.leqDomain(*this))
    {
        assert(false && "narrowing requires next to be included in current");
        std::abort();
    }
    narrowDomain(next);
}

bool AbstractDomain::isBottom() const
{
    return isBottomDomain();
}

bool AbstractDomain::isTop() const
{
    return isTopDomain();
}

CheckResult AbstractDomain::isSubsetOf(const AbstractDomain& other) const
{
    requireCompatible(other);
    return leqDomain(other) ? CheckResult::True : CheckResult::False;
}

CheckResult AbstractDomain::isEquivalentTo(const AbstractDomain& other) const
{
    requireCompatible(other);
    return leqDomain(other) && other.leqDomain(*this) ? CheckResult::True
           : CheckResult::False;
}

std::string AbstractDomain::toString() const
{
    return domainToString();
}

} // namespace SVF
