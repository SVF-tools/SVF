//===- BoxProgramState.h -- Complete Box AE state --------*- C++ -*-===//

#ifndef SVF_AE_BOX_PROGRAM_STATE_H
#define SVF_AE_BOX_PROGRAM_STATE_H

#include "AE/Core/AbstractState.h"
#include "AE/Core/BoxDomain.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SVF::AbstractDomain
{

class Location
{
public:
    explicit Location(std::uint32_t id = 0) : id_(id) {}

    std::uint32_t id() const
    {
        return id_;
    }

    friend bool operator==(Location lhs, Location rhs)
    {
        return lhs.id_ == rhs.id_;
    }
    friend bool operator!=(Location lhs, Location rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(Location lhs, Location rhs)
    {
        return lhs.id_ < rhs.id_;
    }

private:
    std::uint32_t id_;
};

class PointeeSet
{
public:
    PointeeSet() = default;
    static PointeeSet bottom();
    static PointeeSet top();
    static PointeeSet singleton(Location location);

    bool isBottom() const;
    bool isTop() const;
    bool isSingleton() const;
    bool contains(Location location) const;
    const std::set<Location>& locations() const;

    void insert(Location location);
    void joinWith(const PointeeSet& other);
    void meetWith(const PointeeSet& other);
    bool isSubsetOf(const PointeeSet& other) const;
    std::string toString() const;

    friend bool operator==(const PointeeSet& lhs, const PointeeSet& rhs)
    {
        return lhs.top_ == rhs.top_ && lhs.locations_ == rhs.locations_;
    }
    friend bool operator!=(const PointeeSet& lhs, const PointeeSet& rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit PointeeSet(bool top) : top_(top) {}

    bool top_ = false;
    std::set<Location> locations_;
};

/// Internal Box-state pointer metadata. This is not an independently
/// selectable abstract domain; it exists only to preserve AE load/store and
/// object-lifetime semantics alongside the numerical Box carrier.
class PointerMap final
{
public:
    static PointerMap top();
    static PointerMap bottom();

    PointeeSet pointeesOf(Variable variable) const;
    void assign(Variable variable, PointeeSet addresses);
    void forget(Variable variable);
    void changeEnvironment(const VariableEnvironment& environment);
    void joinWith(const PointerMap& other);
    void meetWith(const PointerMap& other);
    void widenWith(const PointerMap& next);
    void narrowWith(const PointerMap& next);
    bool isBottom() const;
    bool isTop() const;
    bool isSubsetOf(const PointerMap& other) const;
    std::string toString() const;

private:
    using Values = std::map<Variable, PointeeSet>;
    explicit PointerMap(bool defaultTop)
        : defaultTop_(defaultTop), values_(std::make_shared<Values>())
    {
    }

    void normalize(Variable variable);
    Values& writableValues();
    PointeeSet defaultValue() const;

    bool defaultTop_ = false;
    std::shared_ptr<Values> values_;
};

enum class Lifetime
{
    Bottom,
    Alive,
    Freed,
    MaybeFreed
};

Lifetime join(Lifetime lhs, Lifetime rhs);
Lifetime meet(Lifetime lhs, Lifetime rhs);
bool isSubsetOf(Lifetime lhs, Lifetime rhs);
const char* toString(Lifetime lifetime);

class LifetimeState final : public AbstractState
{
public:
    static LifetimeState top();
    static LifetimeState bottom();

    std::unique_ptr<AbstractState> clone() const override;
    const char* name() const override;

    Lifetime statusOf(Location location) const;
    void allocate(Location location);
    void release(Location location);
    bool mayBeFreed(Location location) const;
    bool mustBeFreed(Location location) const;

private:
    using Values = std::map<Location, Lifetime>;
    explicit LifetimeState(Lifetime defaultValue)
        : defaultValue_(defaultValue), values_(std::make_shared<Values>())
    {
    }

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<LifetimeState>();
    }
    bool hasCompatibleDomain(const AbstractState& other) const override;
    void joinState(const AbstractState& other) override;
    void meetState(const AbstractState& other) override;
    void widenState(const AbstractState& next) override;
    void narrowState(const AbstractState& next) override;
    bool isBottomState() const override;
    bool isTopState() const override;
    bool leqState(const AbstractState& other) const override;
    std::string stateToString() const override;

    void set(Location location, Lifetime lifetime);
    Values& writableValues();

    Lifetime defaultValue_ = Lifetime::Bottom;
    std::shared_ptr<Values> values_;
};

/// Tracks which facets of an AbstractValue are present for every domain
/// variable.  The numerical and address domains deliberately use top/bottom
/// defaults, so they cannot by themselves distinguish an absent value from
/// an explicitly stored numeric top or address bottom.  AE needs that
/// distinction for uninitialised-value checks and for sparse materialisation.
class ValueShapeState final : public AbstractState
{
public:
    struct Shape
    {
        bool defined = false;
        bool numeric = false;

        friend bool operator==(Shape lhs, Shape rhs)
        {
            return lhs.defined == rhs.defined && lhs.numeric == rhs.numeric;
        }
        friend bool operator!=(Shape lhs, Shape rhs)
        {
            return !(lhs == rhs);
        }
    };

    static ValueShapeState top();
    static ValueShapeState bottom();

    std::unique_ptr<AbstractState> clone() const override;
    const char* name() const override;

    Shape shapeOf(Variable variable) const;
    bool isDefined(Variable variable) const;
    bool hasNumeric(Variable variable) const;
    std::vector<Variable> definedVariables(
        const VariableEnvironment& environment) const;
    void assign(Variable variable, bool numeric);
    void forget(Variable variable);
    void changeEnvironment(const VariableEnvironment& environment);

private:
    static constexpr std::size_t ShapesPerPage = 64;

    struct ShapePage
    {
        std::array<std::uint8_t, ShapesPerPage> shapes;
    };

    struct ShapePageEntry
    {
        std::size_t index;
        std::shared_ptr<ShapePage> page;
    };

    ValueShapeState(bool defaultDefined, bool defaultNumeric)
        : default_(encode({defaultDefined, defaultNumeric}))
    {
    }

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<ValueShapeState>();
    }
    bool hasCompatibleDomain(const AbstractState& other) const override;
    void joinState(const AbstractState& other) override;
    void meetState(const AbstractState& other) override;
    void widenState(const AbstractState& next) override;
    void narrowState(const AbstractState& next) override;
    bool isBottomState() const override;
    bool isTopState() const override;
    bool leqState(const AbstractState& other) const override;
    std::string stateToString() const override;

    static std::uint8_t encode(Shape shape);
    static Shape decode(std::uint8_t shape);
    std::uint8_t encodedShapeOf(Variable variable) const;
    void setEncodedShape(Variable variable, std::uint8_t shape);
    static bool pageIsDefault(const ShapePage& page, std::uint8_t defaultShape);

    std::uint8_t default_ = 0;
    /// As with BoxState, the page directory is sorted and cheap to copy while
    /// the payload pages are detached only when a shape in that page changes.
    std::vector<ShapePageEntry> pages_;
};

/// Immutable mapping from abstract locations to the scalar symbol denoting the
/// location's stored content. It is layout metadata, not mutable memory state.
class MemoryLayout
{
public:
    MemoryLayout() : cells_(std::make_shared<const Cells>()) {}
    explicit MemoryLayout(std::map<Location, Variable> cells)
        : cells_(std::make_shared<const Cells>(std::move(cells)))
    {
    }

    bool contains(Location location) const
    {
        return cells_->count(location) != 0;
    }
    Variable contentOf(Location location) const;
    const std::map<Location, Variable>& cells() const
    {
        return *cells_;
    }

    friend bool operator==(const MemoryLayout& lhs, const MemoryLayout& rhs)
    {
        return lhs.cells_ == rhs.cells_ || *lhs.cells_ == *rhs.cells_;
    }

private:
    using Cells = std::map<Location, Variable>;
    std::shared_ptr<const Cells> cells_;
};

/// Complete Box-backed program state. Memory contents are ordinary symbols in
/// the numerical state; pointer and lifetime facts are kept in small companion
/// facets because they are not numerical intervals.
class BoxProgramState final : public AbstractState
{
public:
    BoxProgramState(BoxState numerical, MemoryLayout memoryLayout,
                    PointerMap pointers = PointerMap::bottom(),
                    LifetimeState lifetimes = LifetimeState::bottom(),
                    ValueShapeState shapes = ValueShapeState::bottom())
        : numerical_(std::move(numerical)),
          memoryLayout_(std::move(memoryLayout)),
          pointers_(std::move(pointers)), lifetimes_(std::move(lifetimes)),
          shapes_(std::move(shapes))
    {
    }

    std::unique_ptr<AbstractState> clone() const override
    {
        return std::make_unique<BoxProgramState>(*this);
    }
    const char* name() const override
    {
        return "BoxProgramState";
    }

    BoxState& numerical()
    {
        return numerical_;
    }
    const BoxState& numerical() const
    {
        return numerical_;
    }
    PointerMap& pointers()
    {
        return pointers_;
    }
    const PointerMap& pointers() const
    {
        return pointers_;
    }
    LifetimeState& lifetimes()
    {
        return lifetimes_;
    }
    const LifetimeState& lifetimes() const
    {
        return lifetimes_;
    }
    ValueShapeState& shapes()
    {
        return shapes_;
    }
    const ValueShapeState& shapes() const
    {
        return shapes_;
    }
    const MemoryLayout& memoryLayout() const
    {
        return memoryLayout_;
    }

    void assignPointer(Variable target, const PointeeSet& value)
    {
        pointers_.assign(target, value);
        numerical_.forget(target);
        shapes_.assign(target, false);
    }

    void assignNumeric(Variable target, const LinearExpression& expression)
    {
        numerical_.assign(target, expression);
        pointers_.assign(target, PointeeSet::bottom());
        shapes_.assign(target, true);
    }

    void assignNumericParallel(const LinearAssignmentList& assignments)
    {
        numerical_.assignParallel(assignments);
        for (const LinearAssignment& assignment : assignments)
        {
            pointers_.assign(assignment.target, PointeeSet::bottom());
            shapes_.assign(assignment.target, true);
        }
    }

    void assignNumericParallel(const TreeAssignmentList& assignments)
    {
        numerical_.assignParallel(assignments);
        for (const TreeAssignment& assignment : assignments)
        {
            pointers_.assign(assignment.target, PointeeSet::bottom());
            shapes_.assign(assignment.target, true);
        }
    }

    void assume(const LinearConstraint& constraint)
    {
        numerical_.assume(constraint);
    }

    void changeEnvironment(const VariableEnvironment& environment,
                           bool initializeNewVariablesToZero = false)
    {
        numerical_.changeEnvironment(environment, initializeNewVariablesToZero);
        pointers_.changeEnvironment(environment);
        shapes_.changeEnvironment(environment);
    }

    void load(Variable target, Variable pointer)
    {
        const PointeeSet pointees = pointers_.pointeesOf(pointer);
        if (pointees.isTop() || pointees.isBottom())
        {
            numerical_.forget(target);
            if (pointees.isTop())
            {
                pointers_.forget(target);
                shapes_.assign(target, true);
            }
            else
            {
                pointers_.assign(target, PointeeSet::bottom());
                shapes_.assign(target, false);
            }
            return;
        }

        bool first = true;
        BoxProgramState result(*this);
        for (Location location : pointees.locations())
        {
            if (!memoryLayout_.contains(location))
                continue;
            BoxProgramState alternative(*this);
            const Variable content = memoryLayout_.contentOf(location);
            alternative.numerical_.assign(target, LinearExpression(content));
            alternative.pointers_.assign(
                target, alternative.pointers_.pointeesOf(content));
            alternative.shapes_.assign(target,
                                       alternative.shapes_.hasNumeric(content));
            if (first)
            {
                result = std::move(alternative);
                first = false;
            }
            else
            {
                result.joinState(alternative);
            }
        }
        if (first)
        {
            numerical_.forget(target);
            pointers_.forget(target);
            shapes_.assign(target, false);
        }
        else
        {
            *this = std::move(result);
        }
    }

    void store(Variable pointer, Variable source)
    {
        const PointeeSet pointees = pointers_.pointeesOf(pointer);
        if (pointees.isTop())
        {
            for (const auto& [location, content] : memoryLayout_.cells())
            {
                (void)location;
                weakStore(content, source);
            }
            return;
        }
        if (pointees.isBottom())
            return;
        if (pointees.isSingleton())
        {
            const Location location = *pointees.locations().begin();
            if (memoryLayout_.contains(location))
                strongStore(memoryLayout_.contentOf(location), source);
            return;
        }
        for (Location location : pointees.locations())
        {
            if (memoryLayout_.contains(location))
                weakStore(memoryLayout_.contentOf(location), source);
        }
    }

    void allocate(Location location)
    {
        lifetimes_.allocate(location);
    }

    void release(Variable pointer)
    {
        const PointeeSet pointees = pointers_.pointeesOf(pointer);
        if (pointees.isTop())
        {
            for (const auto& [location, content] : memoryLayout_.cells())
            {
                (void)content;
                lifetimes_.release(location);
            }
            return;
        }
        for (Location location : pointees.locations())
            lifetimes_.release(location);
    }

private:
    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<BoxProgramState>();
    }
    bool hasCompatibleDomain(const AbstractState& other) const override
    {
        const auto* product = other.isState<BoxProgramState>()
                                  ? &static_cast<const BoxProgramState&>(other)
                                  : nullptr;
        return product && memoryLayout_ == product->memoryLayout_ &&
               numerical_.environment() == product->numerical_.environment() &&
               numerical_.config().operationCompatible(
                   product->numerical_.config());
    }

    void joinState(const AbstractState& other) override
    {
        const BoxProgramState& product = requireProduct(other);
        numerical_.joinWith(product.numerical_);
        pointers_.joinWith(product.pointers_);
        lifetimes_.joinWith(product.lifetimes_);
        shapes_.joinWith(product.shapes_);
    }

    void meetState(const AbstractState& other) override
    {
        const BoxProgramState& product = requireProduct(other);
        numerical_.meetWith(product.numerical_);
        pointers_.meetWith(product.pointers_);
        lifetimes_.meetWith(product.lifetimes_);
        shapes_.meetWith(product.shapes_);
    }

    void widenState(const AbstractState& next) override
    {
        const BoxProgramState& product = requireProduct(next);
        numerical_.widenWith(product.numerical_);
        pointers_.widenWith(product.pointers_);
        lifetimes_.widenWith(product.lifetimes_);
        shapes_.widenWith(product.shapes_);
    }

    void narrowState(const AbstractState& next) override
    {
        const BoxProgramState& product = requireProduct(next);
        numerical_.narrowWith(product.numerical_);
        pointers_.narrowWith(product.pointers_);
        lifetimes_.narrowWith(product.lifetimes_);
        shapes_.narrowWith(product.shapes_);
    }

    bool isBottomState() const override
    {
        return numerical_.isBottom();
    }

    bool isTopState() const override
    {
        return numerical_.isTop() && pointers_.isTop() && lifetimes_.isTop() &&
               shapes_.isTop();
    }

    bool leqState(const AbstractState& other) const override
    {
        const BoxProgramState& product = requireProduct(other);
        return numerical_.isSubsetOf(product.numerical_) == CheckResult::True &&
               pointers_.isSubsetOf(product.pointers_) &&
               lifetimes_.isSubsetOf(product.lifetimes_) == CheckResult::True &&
               shapes_.isSubsetOf(product.shapes_) == CheckResult::True;
    }

    std::string stateToString() const override
    {
        return "numeric=" + numerical_.toString() +
               ", pointers=" + pointers_.toString() +
               ", lifetimes=" + lifetimes_.toString() +
               ", shapes=" + shapes_.toString();
    }

    const BoxProgramState& requireProduct(const AbstractState& other) const
    {
        requireCompatible(other);
        return static_cast<const BoxProgramState&>(other);
    }

    void strongStore(Variable content, Variable source)
    {
        numerical_.assign(content, LinearExpression(source));
        pointers_.assign(content, pointers_.pointeesOf(source));
        shapes_.assign(content, shapes_.hasNumeric(source));
    }

    void weakStore(Variable content, Variable source)
    {
        BoxProgramState alternative(*this);
        alternative.strongStore(content, source);
        joinState(alternative);
    }

    BoxState numerical_;
    MemoryLayout memoryLayout_;
    PointerMap pointers_;
    LifetimeState lifetimes_;
    ValueShapeState shapes_;
};

} // namespace SVF::AbstractDomain

#endif // SVF_AE_BOX_PROGRAM_STATE_H
