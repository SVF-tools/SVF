//===- BoxProgramState.cpp -- Complete Box AE state -----------------===//

#include "AE/Core/BoxProgramState.h"

#include <algorithm>
#include <sstream>

namespace SVF::AbstractDomain
{

namespace
{

template <typename Key, typename Value>
std::set<Key> combinedKeys(const std::map<Key, Value>& lhs,
                           const std::map<Key, Value>& rhs)
{
    std::set<Key> keys;
    for (const auto& entry : lhs)
        keys.insert(entry.first);
    for (const auto& entry : rhs)
        keys.insert(entry.first);
    return keys;
}

} // namespace

PointeeSet PointeeSet::bottom()
{
    return PointeeSet(false);
}

PointeeSet PointeeSet::top()
{
    return PointeeSet(true);
}

PointeeSet PointeeSet::singleton(Location location)
{
    PointeeSet result = bottom();
    result.insert(location);
    return result;
}

bool PointeeSet::isBottom() const
{
    return !top_ && locations_.empty();
}

bool PointeeSet::isTop() const
{
    return top_;
}

bool PointeeSet::contains(Location location) const
{
    return top_ || locations_.count(location) != 0;
}

bool PointeeSet::isSingleton() const
{
    return !top_ && locations_.size() == 1;
}

const std::set<Location>& PointeeSet::locations() const
{
    if (top_)
        throw std::logic_error("top address set has no finite enumeration");
    return locations_;
}

void PointeeSet::insert(Location location)
{
    if (!top_)
        locations_.insert(location);
}

void PointeeSet::joinWith(const PointeeSet& other)
{
    if (top_ || other.isBottom())
        return;
    if (other.top_)
    {
        *this = top();
        return;
    }
    locations_.insert(other.locations_.begin(), other.locations_.end());
}

void PointeeSet::meetWith(const PointeeSet& other)
{
    if (other.top_ || isBottom())
        return;
    if (top_)
    {
        *this = other;
        return;
    }
    std::set<Location> intersection;
    std::set_intersection(locations_.begin(), locations_.end(),
                          other.locations_.begin(), other.locations_.end(),
                          std::inserter(intersection, intersection.begin()));
    locations_ = std::move(intersection);
}

bool PointeeSet::isSubsetOf(const PointeeSet& other) const
{
    if (other.top_ || isBottom())
        return true;
    if (top_)
        return false;
    return std::includes(other.locations_.begin(), other.locations_.end(),
                         locations_.begin(), locations_.end());
}

std::string PointeeSet::toString() const
{
    if (top_)
        return "top";
    if (locations_.empty())
        return "bottom";
    std::ostringstream output;
    output << "{";
    bool first = true;
    for (Location location : locations_)
    {
        if (!first)
            output << ",";
        first = false;
        output << location.id();
    }
    output << "}";
    return output.str();
}

PointerMap PointerMap::top()
{
    return PointerMap(true);
}

PointerMap PointerMap::bottom()
{
    return PointerMap(false);
}

PointeeSet PointerMap::pointeesOf(Variable variable) const
{
    const auto it = values_->find(variable);
    return it == values_->end() ? defaultValue() : it->second;
}

void PointerMap::assign(Variable variable, PointeeSet addresses)
{
    writableValues()[variable] = std::move(addresses);
    normalize(variable);
}

void PointerMap::forget(Variable variable)
{
    assign(variable, PointeeSet::top());
}

void PointerMap::changeEnvironment(const VariableEnvironment& environment)
{
    const bool hasOutOfScope =
        std::any_of(values_->begin(), values_->end(), [&](const auto& entry) {
            return !environment.contains(entry.first);
        });
    if (!hasOutOfScope)
        return;
    Values& values = writableValues();
    for (auto iterator = values.begin(); iterator != values.end();)
    {
        if (!environment.contains(iterator->first))
            iterator = values.erase(iterator);
        else
            ++iterator;
    }
}

void PointerMap::joinWith(const PointerMap& other)
{
    if (other.isBottom())
        return;
    if (isBottom())
    {
        *this = other;
        return;
    }
    const std::set<Variable> variables = combinedKeys(*values_, *other.values_);
    const bool nextDefaultTop = defaultTop_ || other.defaultTop_;
    std::map<Variable, PointeeSet> next;
    for (Variable variable : variables)
    {
        PointeeSet value = pointeesOf(variable);
        value.joinWith(other.pointeesOf(variable));
        if (value !=
            (nextDefaultTop ? PointeeSet::top() : PointeeSet::bottom()))
            next.emplace(variable, std::move(value));
    }
    defaultTop_ = nextDefaultTop;
    values_ = std::make_shared<Values>(std::move(next));
}

void PointerMap::meetWith(const PointerMap& other)
{
    if (other.isTop())
        return;
    if (isTop())
    {
        *this = other;
        return;
    }
    const std::set<Variable> variables = combinedKeys(*values_, *other.values_);
    const bool nextDefaultTop = defaultTop_ && other.defaultTop_;
    std::map<Variable, PointeeSet> next;
    for (Variable variable : variables)
    {
        PointeeSet value = pointeesOf(variable);
        value.meetWith(other.pointeesOf(variable));
        if (value !=
            (nextDefaultTop ? PointeeSet::top() : PointeeSet::bottom()))
            next.emplace(variable, std::move(value));
    }
    defaultTop_ = nextDefaultTop;
    values_ = std::make_shared<Values>(std::move(next));
}

void PointerMap::widenWith(const PointerMap& next)
{
    joinWith(next);
}

void PointerMap::narrowWith(const PointerMap& next)
{
    meetWith(next);
}

bool PointerMap::isBottom() const
{
    return !defaultTop_ && values_->empty();
}

bool PointerMap::isTop() const
{
    return defaultTop_ && values_->empty();
}

bool PointerMap::isSubsetOf(const PointerMap& other) const
{
    if (defaultTop_ == other.defaultTop_ &&
        (values_ == other.values_ || *values_ == *other.values_))
        return true;
    if (defaultTop_ && !other.defaultTop_)
        return false;
    const std::set<Variable> variables = combinedKeys(*values_, *other.values_);
    return std::all_of(
        variables.begin(), variables.end(), [&](Variable variable) {
            return pointeesOf(variable).isSubsetOf(other.pointeesOf(variable));
        });
}

std::string PointerMap::toString() const
{
    std::ostringstream output;
    output << "default=" << defaultValue().toString() << " {";
    bool first = true;
    for (const auto& [variable, value] : *values_)
    {
        if (!first)
            output << ", ";
        first = false;
        output << variable.id() << "=" << value.toString();
    }
    output << "}";
    return output.str();
}

void PointerMap::normalize(Variable variable)
{
    const auto it = values_->find(variable);
    if (it != values_->end() && it->second == defaultValue())
        writableValues().erase(variable);
}

PointerMap::Values& PointerMap::writableValues()
{
    if (values_.use_count() != 1)
        values_ = std::make_shared<Values>(*values_);
    return *values_;
}

PointeeSet PointerMap::defaultValue() const
{
    return defaultTop_ ? PointeeSet::top() : PointeeSet::bottom();
}

Lifetime join(Lifetime lhs, Lifetime rhs)
{
    if (lhs == Lifetime::Bottom)
        return rhs;
    if (rhs == Lifetime::Bottom || lhs == rhs)
        return lhs;
    return Lifetime::MaybeFreed;
}

Lifetime meet(Lifetime lhs, Lifetime rhs)
{
    if (lhs == Lifetime::MaybeFreed)
        return rhs;
    if (rhs == Lifetime::MaybeFreed || lhs == rhs)
        return lhs;
    return Lifetime::Bottom;
}

bool isSubsetOf(Lifetime lhs, Lifetime rhs)
{
    return lhs == Lifetime::Bottom || rhs == Lifetime::MaybeFreed || lhs == rhs;
}

const char* toString(Lifetime lifetime)
{
    switch (lifetime)
    {
    case Lifetime::Bottom:
        return "bottom";
    case Lifetime::Alive:
        return "alive";
    case Lifetime::Freed:
        return "freed";
    case Lifetime::MaybeFreed:
        return "maybe-freed";
    }
    return "invalid";
}

LifetimeState LifetimeState::top()
{
    return LifetimeState(Lifetime::MaybeFreed);
}

LifetimeState LifetimeState::bottom()
{
    return LifetimeState(Lifetime::Bottom);
}

ValueShapeState ValueShapeState::top()
{
    return ValueShapeState(true, true);
}

ValueShapeState ValueShapeState::bottom()
{
    return ValueShapeState(false, false);
}

std::unique_ptr<AbstractState> ValueShapeState::clone() const
{
    return std::make_unique<ValueShapeState>(*this);
}

const char* ValueShapeState::name() const
{
    return "ValueShapeState";
}

ValueShapeState::Shape ValueShapeState::shapeOf(Variable variable) const
{
    return decode(encodedShapeOf(variable));
}

bool ValueShapeState::isDefined(Variable variable) const
{
    return shapeOf(variable).defined;
}

bool ValueShapeState::hasNumeric(Variable variable) const
{
    return shapeOf(variable).numeric;
}

std::vector<Variable> ValueShapeState::definedVariables(
    const VariableEnvironment& environment) const
{
    std::vector<Variable> result;
    if (decode(default_).defined)
    {
        result.reserve(environment.size());
        for (const VariableDeclaration& declaration : environment.variables())
        {
            if (isDefined(declaration.variable))
                result.push_back(declaration.variable);
        }
        return result;
    }

    for (const ShapePageEntry& entry : pages_)
    {
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
        {
            if (!decode(entry.page->shapes[offset]).defined)
                continue;
            const Variable variable(static_cast<std::uint32_t>(
                entry.index * ShapesPerPage + offset));
            if (environment.contains(variable))
                result.push_back(variable);
        }
    }
    return result;
}

void ValueShapeState::assign(Variable variable, bool numeric)
{
    setEncodedShape(variable, encode({true, numeric}));
}

void ValueShapeState::forget(Variable variable)
{
    setEncodedShape(variable, encode({false, false}));
}

void ValueShapeState::changeEnvironment(const VariableEnvironment& environment)
{
    std::vector<ShapePageEntry> next;
    next.reserve(pages_.size());
    for (const ShapePageEntry& entry : pages_)
    {
        std::shared_ptr<ShapePage> page = entry.page;
        bool changed = false;
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
        {
            if (page->shapes[offset] == default_)
                continue;
            const Variable variable(static_cast<std::uint32_t>(
                entry.index * ShapesPerPage + offset));
            if (!environment.contains(variable))
            {
                if (!changed)
                    page = std::make_shared<ShapePage>(*page);
                page->shapes[offset] = default_;
                changed = true;
            }
        }
        if (!pageIsDefault(*page, default_))
            next.push_back({entry.index, std::move(page)});
    }
    pages_ = std::move(next);
}

bool ValueShapeState::hasCompatibleDomain(const AbstractState& other) const
{
    return other.isState<ValueShapeState>();
}

void ValueShapeState::joinState(const AbstractState& other)
{
    const auto& state = static_cast<const ValueShapeState&>(other);
    if (state.isBottomState())
        return;
    if (isBottomState())
    {
        *this = state;
        return;
    }
    const std::uint8_t nextDefault = default_ | state.default_;
    std::vector<ShapePageEntry> next;
    next.reserve(pages_.size() + state.pages_.size());
    std::size_t lhs = 0;
    std::size_t rhs = 0;
    while (lhs < pages_.size() || rhs < state.pages_.size())
    {
        const std::size_t pageIndex =
            rhs == state.pages_.size() ||
                    (lhs < pages_.size() &&
                     pages_[lhs].index < state.pages_[rhs].index)
                ? pages_[lhs].index
                : state.pages_[rhs].index;
        const ShapePage* lhsPage =
            lhs < pages_.size() && pages_[lhs].index == pageIndex
                ? pages_[lhs++].page.get()
                : nullptr;
        const ShapePage* rhsPage =
            rhs < state.pages_.size() && state.pages_[rhs].index == pageIndex
                ? state.pages_[rhs++].page.get()
                : nullptr;
        auto page = std::make_shared<ShapePage>();
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
            page->shapes[offset] =
                (lhsPage ? lhsPage->shapes[offset] : default_) |
                (rhsPage ? rhsPage->shapes[offset] : state.default_);
        if (!pageIsDefault(*page, nextDefault))
            next.push_back({pageIndex, std::move(page)});
    }
    default_ = nextDefault;
    pages_ = std::move(next);
}

void ValueShapeState::meetState(const AbstractState& other)
{
    const auto& state = static_cast<const ValueShapeState&>(other);
    if (state.isTopState())
        return;
    if (isTopState())
    {
        *this = state;
        return;
    }
    const std::uint8_t nextDefault = default_ & state.default_;
    std::vector<ShapePageEntry> next;
    next.reserve(pages_.size() + state.pages_.size());
    std::size_t lhs = 0;
    std::size_t rhs = 0;
    while (lhs < pages_.size() || rhs < state.pages_.size())
    {
        const std::size_t pageIndex =
            rhs == state.pages_.size() ||
                    (lhs < pages_.size() &&
                     pages_[lhs].index < state.pages_[rhs].index)
                ? pages_[lhs].index
                : state.pages_[rhs].index;
        const ShapePage* lhsPage =
            lhs < pages_.size() && pages_[lhs].index == pageIndex
                ? pages_[lhs++].page.get()
                : nullptr;
        const ShapePage* rhsPage =
            rhs < state.pages_.size() && state.pages_[rhs].index == pageIndex
                ? state.pages_[rhs++].page.get()
                : nullptr;
        auto page = std::make_shared<ShapePage>();
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
            page->shapes[offset] =
                (lhsPage ? lhsPage->shapes[offset] : default_) &
                (rhsPage ? rhsPage->shapes[offset] : state.default_);
        if (!pageIsDefault(*page, nextDefault))
            next.push_back({pageIndex, std::move(page)});
    }
    default_ = nextDefault;
    pages_ = std::move(next);
}

void ValueShapeState::widenState(const AbstractState& next)
{
    joinState(next);
}

void ValueShapeState::narrowState(const AbstractState& next)
{
    meetState(next);
}

bool ValueShapeState::isBottomState() const
{
    return default_ == encode({false, false}) && pages_.empty();
}

bool ValueShapeState::isTopState() const
{
    return default_ == encode({true, true}) && pages_.empty();
}

bool ValueShapeState::leqState(const AbstractState& other) const
{
    const auto& state = static_cast<const ValueShapeState&>(other);
    if (default_ == state.default_ && pages_.size() == state.pages_.size())
    {
        bool equal = true;
        for (std::size_t index = 0; index < pages_.size(); ++index)
        {
            if (pages_[index].index != state.pages_[index].index ||
                (pages_[index].page != state.pages_[index].page &&
                 pages_[index].page->shapes !=
                     state.pages_[index].page->shapes))
            {
                equal = false;
                break;
            }
        }
        if (equal)
            return true;
    }
    if ((default_ & ~state.default_) != 0)
        return false;
    std::size_t lhs = 0;
    std::size_t rhs = 0;
    while (lhs < pages_.size() || rhs < state.pages_.size())
    {
        const std::size_t pageIndex =
            rhs == state.pages_.size() ||
                    (lhs < pages_.size() &&
                     pages_[lhs].index < state.pages_[rhs].index)
                ? pages_[lhs].index
                : state.pages_[rhs].index;
        const ShapePage* lhsPage =
            lhs < pages_.size() && pages_[lhs].index == pageIndex
                ? pages_[lhs++].page.get()
                : nullptr;
        const ShapePage* rhsPage =
            rhs < state.pages_.size() && state.pages_[rhs].index == pageIndex
                ? state.pages_[rhs++].page.get()
                : nullptr;
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
        {
            const std::uint8_t lhsShape =
                lhsPage ? lhsPage->shapes[offset] : default_;
            const std::uint8_t rhsShape =
                rhsPage ? rhsPage->shapes[offset] : state.default_;
            if ((lhsShape & ~rhsShape) != 0)
                return false;
        }
    }
    return true;
}

std::string ValueShapeState::stateToString() const
{
    std::ostringstream output;
    const Shape defaultShape = decode(default_);
    output << "default=(defined=" << defaultShape.defined
           << ",numeric=" << defaultShape.numeric << ") {";
    bool first = true;
    for (const ShapePageEntry& entry : pages_)
    {
        for (std::size_t offset = 0; offset < ShapesPerPage; ++offset)
        {
            if (entry.page->shapes[offset] == default_)
                continue;
            if (!first)
                output << ", ";
            first = false;
            const Shape shape = decode(entry.page->shapes[offset]);
            output << entry.index * ShapesPerPage + offset
                   << "=(defined=" << shape.defined
                   << ",numeric=" << shape.numeric << ")";
        }
    }
    output << "}";
    return output.str();
}

std::uint8_t ValueShapeState::encode(Shape shape)
{
    return static_cast<std::uint8_t>((shape.defined ? 1U : 0U) |
                                     (shape.numeric ? 2U : 0U));
}

ValueShapeState::Shape ValueShapeState::decode(std::uint8_t shape)
{
    return {(shape & 1U) != 0, (shape & 2U) != 0};
}

std::uint8_t ValueShapeState::encodedShapeOf(Variable variable) const
{
    const std::size_t pageIndex = variable.id() / ShapesPerPage;
    const auto iterator =
        std::lower_bound(pages_.begin(), pages_.end(), pageIndex,
                         [](const ShapePageEntry& entry, std::size_t index) {
                             return entry.index < index;
                         });
    if (iterator == pages_.end() || iterator->index != pageIndex)
        return default_;
    return iterator->page->shapes[variable.id() % ShapesPerPage];
}

void ValueShapeState::setEncodedShape(Variable variable, std::uint8_t shape)
{
    const std::size_t pageIndex = variable.id() / ShapesPerPage;
    auto iterator =
        std::lower_bound(pages_.begin(), pages_.end(), pageIndex,
                         [](const ShapePageEntry& entry, std::size_t index) {
                             return entry.index < index;
                         });
    if (iterator == pages_.end() || iterator->index != pageIndex)
    {
        if (shape == default_)
            return;
        auto page = std::make_shared<ShapePage>();
        page->shapes.fill(default_);
        iterator = pages_.insert(iterator, {pageIndex, std::move(page)});
    }
    else if (iterator->page.use_count() != 1)
    {
        iterator->page = std::make_shared<ShapePage>(*iterator->page);
    }
    iterator->page->shapes[variable.id() % ShapesPerPage] = shape;
    if (pageIsDefault(*iterator->page, default_))
        pages_.erase(iterator);
}

bool ValueShapeState::pageIsDefault(const ShapePage& page,
                                    std::uint8_t defaultShape)
{
    return std::all_of(
        page.shapes.begin(), page.shapes.end(),
        [defaultShape](std::uint8_t shape) { return shape == defaultShape; });
}

std::unique_ptr<AbstractState> LifetimeState::clone() const
{
    return std::make_unique<LifetimeState>(*this);
}

const char* LifetimeState::name() const
{
    return "LifetimeState";
}

Lifetime LifetimeState::statusOf(Location location) const
{
    const auto it = values_->find(location);
    return it == values_->end() ? defaultValue_ : it->second;
}

void LifetimeState::allocate(Location location)
{
    set(location, Lifetime::Alive);
}

void LifetimeState::release(Location location)
{
    const Lifetime current = statusOf(location);
    set(location, current == Lifetime::Alive || current == Lifetime::Freed
                      ? Lifetime::Freed
                      : Lifetime::MaybeFreed);
}

bool LifetimeState::mayBeFreed(Location location) const
{
    const Lifetime lifetime = statusOf(location);
    return lifetime == Lifetime::Freed || lifetime == Lifetime::MaybeFreed;
}

bool LifetimeState::mustBeFreed(Location location) const
{
    return statusOf(location) == Lifetime::Freed;
}

bool LifetimeState::hasCompatibleDomain(const AbstractState& other) const
{
    return other.isState<LifetimeState>();
}

void LifetimeState::joinState(const AbstractState& other)
{
    const auto& state = static_cast<const LifetimeState&>(other);
    if (state.isBottomState())
        return;
    if (isBottomState())
    {
        *this = state;
        return;
    }
    const std::set<Location> locations = combinedKeys(*values_, *state.values_);
    const Lifetime nextDefault = join(defaultValue_, state.defaultValue_);
    std::map<Location, Lifetime> next;
    for (Location location : locations)
    {
        const Lifetime value =
            join(statusOf(location), state.statusOf(location));
        if (value != nextDefault)
            next.emplace(location, value);
    }
    defaultValue_ = nextDefault;
    values_ = std::make_shared<Values>(std::move(next));
}

void LifetimeState::meetState(const AbstractState& other)
{
    const auto& state = static_cast<const LifetimeState&>(other);
    if (state.isTopState())
        return;
    if (isTopState())
    {
        *this = state;
        return;
    }
    const std::set<Location> locations = combinedKeys(*values_, *state.values_);
    const Lifetime nextDefault = meet(defaultValue_, state.defaultValue_);
    std::map<Location, Lifetime> next;
    for (Location location : locations)
    {
        const Lifetime value =
            meet(statusOf(location), state.statusOf(location));
        if (value != nextDefault)
            next.emplace(location, value);
    }
    defaultValue_ = nextDefault;
    values_ = std::make_shared<Values>(std::move(next));
}

void LifetimeState::widenState(const AbstractState& next)
{
    joinState(next);
}

void LifetimeState::narrowState(const AbstractState& next)
{
    meetState(next);
}

bool LifetimeState::isBottomState() const
{
    return defaultValue_ == Lifetime::Bottom && values_->empty();
}

bool LifetimeState::isTopState() const
{
    return defaultValue_ == Lifetime::MaybeFreed && values_->empty();
}

bool LifetimeState::leqState(const AbstractState& other) const
{
    const auto& state = static_cast<const LifetimeState&>(other);
    if (defaultValue_ == state.defaultValue_ &&
        (values_ == state.values_ || *values_ == *state.values_))
        return true;
    if (!SVF::AbstractDomain::isSubsetOf(defaultValue_, state.defaultValue_))
        return false;
    const std::set<Location> locations = combinedKeys(*values_, *state.values_);
    return std::all_of(locations.begin(), locations.end(),
                       [&](Location location) {
                           return SVF::AbstractDomain::isSubsetOf(
                               statusOf(location), state.statusOf(location));
                       });
}

std::string LifetimeState::stateToString() const
{
    std::ostringstream output;
    output << "default=" << SVF::AbstractDomain::toString(defaultValue_)
           << " {";
    bool first = true;
    for (const auto& [location, value] : *values_)
    {
        if (!first)
            output << ", ";
        first = false;
        output << location.id() << "=" << SVF::AbstractDomain::toString(value);
    }
    output << "}";
    return output.str();
}

void LifetimeState::set(Location location, Lifetime lifetime)
{
    if (lifetime == defaultValue_)
        writableValues().erase(location);
    else
        writableValues()[location] = lifetime;
}

LifetimeState::Values& LifetimeState::writableValues()
{
    if (values_.use_count() != 1)
        values_ = std::make_shared<Values>(*values_);
    return *values_;
}

Variable MemoryLayout::contentOf(Location location) const
{
    const auto it = cells_->find(location);
    if (it == cells_->end())
        throw std::out_of_range("location has no content symbol");
    return it->second;
}

} // namespace SVF::AbstractDomain
