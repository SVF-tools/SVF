//===- AbstractState.cpp -- Common abstract-state lattice API -----------===//

#include "AE/Core/AbstractState.h"

#include <stdexcept>

namespace SVF::AbstractDomain
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

AbstractState::~AbstractState() = default;

void AbstractState::requireCompatible(const AbstractState& other) const
{
    if (!hasCompatibleDomain(other))
        throw std::invalid_argument(
            "abstract states use different domains or configurations");
}

void AbstractState::joinWith(const AbstractState& other)
{
    requireCompatible(other);
    joinState(other);
}

void AbstractState::meetWith(const AbstractState& other)
{
    requireCompatible(other);
    meetState(other);
}

void AbstractState::widenWith(const AbstractState& next)
{
    requireCompatible(next);
    widenState(next);
}

void AbstractState::narrowWith(const AbstractState& next)
{
    requireCompatible(next);
    if (!next.leqState(*this))
        throw std::invalid_argument(
            "narrowing requires next to be included in current");
    narrowState(next);
}

bool AbstractState::isBottom() const
{
    return isBottomState();
}

bool AbstractState::isTop() const
{
    return isTopState();
}

CheckResult AbstractState::isSubsetOf(const AbstractState& other) const
{
    requireCompatible(other);
    return leqState(other) ? CheckResult::True : CheckResult::False;
}

CheckResult AbstractState::isEquivalentTo(const AbstractState& other) const
{
    requireCompatible(other);
    return leqState(other) && other.leqState(*this) ? CheckResult::True
                                                    : CheckResult::False;
}

std::string AbstractState::toString() const
{
    return stateToString();
}

} // namespace SVF::AbstractDomain
