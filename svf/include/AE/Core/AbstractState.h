//===- AbstractState.h -- Common abstract-state lattice API -----*- C++ -*-===//

#ifndef SVF_AE_ABSTRACT_STATE_H
#define SVF_AE_ABSTRACT_STATE_H

#include <memory>
#include <string>

namespace SVF::AbstractDomain
{

enum class CheckResult
{
    False,
    True,
    Unknown
};

const char* toString(CheckResult result);

/// Common value-level interface implemented by every complete abstract state.
///
/// It deliberately contains only lattice operations. Transfer functions live
/// on more specific state interfaces such as NumericalState.
class AbstractState
{
public:
    virtual ~AbstractState();

    virtual std::unique_ptr<AbstractState> clone() const = 0;
    virtual const char* name() const = 0;

    void joinWith(const AbstractState& other);
    void meetWith(const AbstractState& other);
    void widenWith(const AbstractState& next);
    void narrowWith(const AbstractState& next);

    bool isBottom() const;
    bool isTop() const;
    /// Return whether every concrete state represented by this state is also
    /// represented by `other`.
    CheckResult isSubsetOf(const AbstractState& other) const;
    CheckResult isEquivalentTo(const AbstractState& other) const;
    std::string toString() const;

    /// RTTI-free concrete-state query. SVF is commonly built with -fno-rtti,
    /// so abstract domains use stable per-C++-type tokens for checked dispatch.
    template <typename StateT> bool isState() const noexcept
    {
        return dynamicTypeToken() == staticTypeToken<StateT>();
    }

protected:
    AbstractState() = default;
    AbstractState(const AbstractState&) = default;
    AbstractState(AbstractState&&) noexcept = default;
    AbstractState& operator=(const AbstractState&) = default;
    AbstractState& operator=(AbstractState&&) noexcept = default;

    void requireCompatible(const AbstractState& other) const;

    template <typename StateT> static const void* staticTypeToken() noexcept
    {
        static const char token = 0;
        return &token;
    }

private:
    virtual const void* dynamicTypeToken() const noexcept = 0;
    virtual bool hasCompatibleDomain(const AbstractState& other) const = 0;
    virtual void joinState(const AbstractState& other) = 0;
    virtual void meetState(const AbstractState& other) = 0;
    virtual void widenState(const AbstractState& next) = 0;
    virtual void narrowState(const AbstractState& next) = 0;
    virtual bool isBottomState() const = 0;
    virtual bool isTopState() const = 0;
    virtual bool leqState(const AbstractState& other) const = 0;
    virtual std::string stateToString() const = 0;
};

} // namespace SVF::AbstractDomain

#endif // SVF_AE_ABSTRACT_STATE_H
