//===- OctagonDomain.cpp -- Native relational numerical domain -------===//
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
// Contributors: Jiawei Yang
//
//===----------------------------------------------------------------------===//

#include "AE/Core/OctagonDomain.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SVF
{

namespace
{
std::size_t positiveNode(Dimension dimension)
{
    return 2 * dimension;
}

std::size_t negativeNode(Dimension dimension)
{
    return 2 * dimension + 1;
}

std::size_t opposite(std::size_t node)
{
    return node ^ 1U;
}

class OctagonStorage final
{
public:
    struct ScratchMatrixTag
    {
    };

    explicit OctagonStorage(const DimensionLayout& layout,
                            OctagonStorageKind kind, bool bottom = false)
        : OctagonStorage(extractVariableKinds(layout), kind, bottom)
    {
    }

    explicit OctagonStorage(std::vector<NumericKind> variableKinds,
                            OctagonStorageKind kind, bool bottom = false)
        : dimensions(variableKinds.size()),
          variableKinds(std::move(variableKinds)),
          bottom(bottom),
          kind_(kind)
    {
        initializeCarrier();
        const std::size_t count = nodes();
        for (std::size_t node = 0; node < count; ++node)
            set(node, node, zero());
    }

    OctagonStorage(std::vector<NumericKind> variableKinds,
                   OctagonStorageKind kind, ScratchMatrixTag)
        : dimensions(variableKinds.size()),
          variableKinds(std::move(variableKinds)),
          kind_(kind)
    {
        initializeCarrier();
    }

    std::unique_ptr<OctagonStorage> clone(
        bool componentCopyOnWrite = true) const
    {
        auto result = std::make_unique<OctagonStorage>(*this);
        if (kind_ == OctagonStorageKind::ComponentDense &&
                !componentCopyOnWrite)
            for (Component& component : result->components_)
                component.matrix =
                    std::make_shared<std::vector<Bound>>(*component.matrix);
        return result;
    }

    std::size_t nodes() const
    {
        return 2 * dimensions;
    }

    std::vector<std::size_t> closureNodes(Dimension variable) const
    {
        std::vector<std::size_t> result;
        if (kind_ != OctagonStorageKind::ComponentDense)
        {
            result.resize(nodes());
            std::iota(result.begin(), result.end(), 0);
            return result;
        }
        const std::size_t owner = componentOwner_[variable];
        if (owner == noComponent())
            return {positiveNode(variable), negativeNode(variable)};
        result.reserve(2 * components_[owner].variables.size());
        for (const std::size_t member : components_[owner].variables)
        {
            result.push_back(positiveNode(member));
            result.push_back(negativeNode(member));
        }
        return result;
    }

    const Bound& at(std::size_t row, std::size_t column) const
    {
        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            return dense_[matrixIndex(row, column)];
        case OctagonStorageKind::SparseFinite:
        {
            const auto found = sparse_.find(matrixIndex(row, column));
            return found == sparse_.end() ? implicit(row, column)
                   : found->second;
        }
        case OctagonStorageKind::ComponentDense:
            return componentAt(row, column);
        }
        assert(false && "unknown Octagon storage kind");
        std::abort();
    }

    void set(std::size_t row, std::size_t column, const Bound& value)
    {
        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            dense_[matrixIndex(row, column)] = value;
            return;
        case OctagonStorageKind::SparseFinite:
            setSparse(row, column, value);
            return;
        case OctagonStorageKind::ComponentDense:
            setComponent(row, column, value);
            return;
        }
        assert(false && "unknown Octagon storage kind");
        std::abort();
    }

    bool tighten(std::size_t row, std::size_t column,
                 const Bound& candidate)
    {
        if (!(candidate < at(row, column)))
            return false;
        set(row, column, candidate);
        return true;
    }

    /// Return whether every finite bound represented by this storage (the
    /// right-hand side of an inclusion test) is satisfied by lhs.  Infinite
    /// right-hand-side cells impose no condition, and coherent logical pairs
    /// share one physical half-matrix slot, so visiting the physical carrier
    /// is sufficient.
    bool contains(const OctagonStorage& lhs) const
    {
        const auto accepts = [&](std::size_t row, std::size_t column,
                                 const Bound& rhsBound)
        {
            return rhsBound.isPlusInfinity() ||
                   lhs.at(row, column) <= rhsBound;
        };

        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            for (std::size_t row = 0; row < nodes(); ++row)
                for (std::size_t column = 0; column <= (row | 1U); ++column)
                {
                    const Bound& rhsBound = dense_[storedIndex(row, column)];
                    if (!accepts(row, column, rhsBound))
                        return false;
                }
            return true;
        case OctagonStorageKind::SparseFinite:
            for (const auto& [index, rhsBound] : sparse_)
            {
                const auto [row, column] = storedCoordinates(index, nodes());
                if (!accepts(row, column, rhsBound))
                    return false;
            }
            return true;
        case OctagonStorageKind::ComponentDense:
            for (const Component& component : components_)
            {
                const std::size_t localNodes = 2 * component.variables.size();
                for (std::size_t localRow = 0; localRow < localNodes;
                        ++localRow)
                    for (std::size_t localColumn = 0;
                            localColumn <= (localRow | 1U); ++localColumn)
                    {
                        const Bound& rhsBound =
                            (*component.matrix)[storedIndex(localRow,
                                                            localColumn)];
                        if (rhsBound.isPlusInfinity())
                            continue;
                        const std::size_t row =
                            2 * component.variables[localRow / 2] +
                            localRow % 2;
                        const std::size_t column =
                            2 * component.variables[localColumn / 2] +
                            localColumn % 2;
                        if (!accepts(row, column, rhsBound))
                            return false;
                    }
            }
            return true;
        }
        assert(false && "unknown Octagon storage kind");
        std::abort();
    }

    /// Forget all relations incident to one dimension without scanning the
    /// global logical matrix. Returns false when the active carrier requires
    /// the generic path.
    bool forgetComponentDimension(Dimension dimension)
    {
        if (kind_ != OctagonStorageKind::ComponentDense)
            return false;
        const std::size_t owner = componentOwner_[dimension];
        if (owner == noComponent())
            return true;

        const std::vector<std::size_t> variables =
            components_[owner].variables;
        const std::size_t first = positiveNode(dimension);
        const std::size_t second = negativeNode(dimension);
        for (const std::size_t variable : variables)
            for (std::size_t sign = 0; sign < 2; ++sign)
            {
                const std::size_t node = 2 * variable + sign;
                setComponent(first, node, infinity());
                setComponent(second, node, infinity());
                setComponent(node, first, infinity());
                setComponent(node, second, infinity());
            }
        setComponent(first, first, zero());
        setComponent(second, second, zero());
        compactComponents();
        return true;
    }

    /// Copy finite physical bounds through an old-to-new dimension mapping.
    /// Missing dimensions are projected away.  The destination is expected to
    /// be a freshly constructed top state with the requested carrier.
    void copyRemappedFiniteBoundsTo(
        OctagonStorage& destination,
        const std::vector<std::optional<Dimension>>& newDimensions) const
    {
        const auto copy = [&](std::size_t row, std::size_t column,
                              const Bound& value)
        {
            if (value.isPlusInfinity() ||
                    (row == column && value == zero()))
                return;
            const std::optional<Dimension> newRow = newDimensions[row / 2];
            const std::optional<Dimension> newColumn =
                newDimensions[column / 2];
            if (!newRow || !newColumn)
                return;
            destination.set(2 * *newRow + row % 2,
                            2 * *newColumn + column % 2, value);
        };

        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            for (std::size_t row = 0; row < nodes(); ++row)
                for (std::size_t column = 0; column <= (row | 1U); ++column)
                    copy(row, column, dense_[storedIndex(row, column)]);
            return;
        case OctagonStorageKind::SparseFinite:
            for (const auto& [index, value] : sparse_)
            {
                const auto [row, column] = storedCoordinates(index, nodes());
                copy(row, column, value);
            }
            return;
        case OctagonStorageKind::ComponentDense:
            for (const Component& component : components_)
            {
                const std::size_t localNodes = 2 * component.variables.size();
                for (std::size_t localRow = 0; localRow < localNodes;
                        ++localRow)
                    for (std::size_t localColumn = 0;
                            localColumn <= (localRow | 1U); ++localColumn)
                    {
                        const std::size_t row =
                            2 * component.variables[localRow / 2] +
                            localRow % 2;
                        const std::size_t column =
                            2 * component.variables[localColumn / 2] +
                            localColumn % 2;
                        copy(row, column,
                             (*component.matrix)[storedIndex(localRow,
                                                             localColumn)]);
                    }
            }
            return;
        }
        assert(false && "unknown Octagon storage kind");
        std::abort();
    }

    /// Materialize the point-wise maximum with rhs into a fresh sparse or
    /// component destination. A finite result exists only where both operands
    /// are finite, so the global logical matrix never needs to be scanned.
    void copyFiniteMaximumTo(const OctagonStorage& rhs,
                             OctagonStorage& destination) const
    {
        const auto copy = [&](std::size_t row, std::size_t column,
                              const Bound& lhsBound)
        {
            if (lhsBound.isPlusInfinity() ||
                    (row == column && lhsBound == zero()))
                return;
            const Bound& rhsBound = rhs.at(row, column);
            if (!rhsBound.isPlusInfinity())
                destination.set(row, column,
                                Bound::max(lhsBound, rhsBound));
        };
        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            assert(false && "finite join copy is intended for sparse carriers");
            std::abort();
        case OctagonStorageKind::SparseFinite:
            for (const auto& [index, value] : sparse_)
            {
                const auto [row, column] = storedCoordinates(index, nodes());
                copy(row, column, value);
            }
            return;
        case OctagonStorageKind::ComponentDense:
            for (const Component& component : components_)
            {
                const std::size_t localNodes = 2 * component.variables.size();
                for (std::size_t localRow = 0; localRow < localNodes;
                        ++localRow)
                    for (std::size_t localColumn = 0;
                            localColumn <= (localRow | 1U); ++localColumn)
                    {
                        const std::size_t row =
                            2 * component.variables[localRow / 2] +
                            localRow % 2;
                        const std::size_t column =
                            2 * component.variables[localColumn / 2] +
                            localColumn % 2;
                        copy(row, column,
                             (*component.matrix)[storedIndex(localRow,
                                                             localColumn)]);
                    }
            }
            return;
        }
        assert(false && "unknown Octagon storage kind");
        std::abort();
    }

    OctagonStorage converted(OctagonStorageKind kind,
                             bool componentCopyOnWrite = true) const
    {
        if (kind == kind_)
            return std::move(*clone(componentCopyOnWrite));
        OctagonStorage result(variableKinds, kind, ScratchMatrixTag{});
        for (std::size_t row = 0; row < nodes(); ++row)
            for (std::size_t column = 0; column < nodes(); ++column)
                result.set(row, column, at(row, column));
        result.bottom = bottom;
        result.stronglyClosed = stronglyClosed;
        return result;
    }

    OctagonStorageKind kind() const
    {
        return kind_;
    }

    std::size_t allocatedSlots() const
    {
        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            return dense_.size();
        case OctagonStorageKind::SparseFinite:
            return sparse_.size();
        case OctagonStorageKind::ComponentDense:
        {
            std::size_t result = 0;
            for (const Component& component : components_)
                result += component.matrix->size();
            return result;
        }
        }
        return 0;
    }

    std::size_t finiteStoredSlots() const
    {
        std::size_t result = 0;
        switch (kind_)
        {
        case OctagonStorageKind::DenseHalf:
            for (const Bound& value : dense_)
                result += static_cast<std::size_t>(value.isFinite());
            break;
        case OctagonStorageKind::SparseFinite:
            for (const auto& entry : sparse_)
                result += static_cast<std::size_t>(entry.second.isFinite());
            // Sparse diagonal zeroes are implicit.
            result += nodes();
            break;
        case OctagonStorageKind::ComponentDense:
            for (const Component& component : components_)
                for (const Bound& value : *component.matrix)
                    result += static_cast<std::size_t>(value.isFinite());
            // Diagonal zeroes outside components are implicit.
            for (std::size_t owner : componentOwner_)
                if (owner == noComponent())
                    result += 2;
            break;
        }
        return result;
    }

    /// Rebuilds component membership from the currently finite relation graph.
    /// It is semantically a no-op and recovers physical sparsity after forget.
    void compactComponents()
    {
        if (kind_ != OctagonStorageKind::ComponentDense)
            return;
        OctagonStorage rebuilt(variableKinds, kind_, ScratchMatrixTag{});
        for (const Component& component : components_)
        {
            const std::size_t localNodes = 2 * component.variables.size();
            for (std::size_t localRow = 0; localRow < localNodes; ++localRow)
                for (std::size_t localColumn = 0;
                        localColumn <= (localRow | 1U); ++localColumn)
                {
                    const Bound& value =
                        (*component.matrix)[storedIndex(localRow, localColumn)];
                    if (value.isPlusInfinity() ||
                            (localRow == localColumn && value == zero()))
                        continue;
                    const std::size_t row =
                        2 * component.variables[localRow / 2] + localRow % 2;
                    const std::size_t column =
                        2 * component.variables[localColumn / 2] +
                        localColumn % 2;
                    rebuilt.setComponent(row, column, value);
                }
        }
        rebuilt.bottom = bottom;
        rebuilt.stronglyClosed = stronglyClosed;
        *this = std::move(rebuilt);
    }

    /// Return the connected variable component without materializing the
    /// octagon as a LinearConstraintSet. ComponentDense maintains this index
    /// incrementally; the other carriers use their matrix as a compatibility
    /// path for tests and explicitly selected storage policies.
    std::vector<Dimension> relationComponent(Dimension seed) const
    {
        if (seed >= dimensions)
        {
            assert(false && "octagon relation seed is out of range");
            std::abort();
        }
        if (bottom)
            return {seed};
        if (kind_ == OctagonStorageKind::ComponentDense)
        {
            const std::size_t owner = componentOwner_[seed];
            if (owner == noComponent())
                return {seed};
            std::vector<Dimension> result(
                components_[owner].variables.begin(),
                components_[owner].variables.end());
            std::sort(result.begin(), result.end());
            return result;
        }

        std::vector<bool> reached(dimensions, false);
        std::vector<Dimension> worklist{seed};
        reached[seed] = true;
        while (!worklist.empty())
        {
            const Dimension current = worklist.back();
            worklist.pop_back();
            for (Dimension candidate = 0; candidate < dimensions;
                    ++candidate)
            {
                if (reached[candidate] || candidate == current)
                    continue;
                bool related = false;
                for (std::size_t currentSign = 0;
                        currentSign < 2 && !related; ++currentSign)
                    for (std::size_t candidateSign = 0;
                            candidateSign < 2; ++candidateSign)
                        if (at(2 * current + currentSign,
                                2 * candidate + candidateSign).isFinite())
                        {
                            related = true;
                            break;
                        }
                if (related)
                {
                    reached[candidate] = true;
                    worklist.push_back(candidate);
                }
            }
        }
        std::vector<Dimension> result;
        for (Dimension dimension = 0; dimension < dimensions; ++dimension)
            if (reached[dimension])
                result.push_back(dimension);
        return result;
    }

    std::size_t dimensions;
    std::vector<NumericKind> variableKinds;
    bool bottom = false;
    bool stronglyClosed = true;

private:
    struct Component
    {
        std::vector<std::size_t> variables;
        std::shared_ptr<std::vector<Bound>> matrix;
    };

    /// Store triangular 2x2 variable blocks. Coherent
    /// off-diagonal entries share an index. Each diagonal block is fully
    /// stored, so its two main-diagonal entries remain a distinct coherent
    /// pair that normalize() keeps equal. at(row,column) preserves the logical
    /// 2n x 2n matrix interface for all algorithms.
    static std::size_t matrixSize(std::size_t dimensions)
    {
        return 2 * dimensions * (dimensions + 1);
    }

    static std::size_t storedIndex(std::size_t row, std::size_t column)
    {
        return column + ((row + 1) * (row + 1)) / 2;
    }

    static std::size_t matrixIndex(std::size_t row, std::size_t column)
    {
        return column > row ? storedIndex(opposite(column), opposite(row))
               : storedIndex(row, column);
    }

    static std::pair<std::size_t, std::size_t> storedCoordinates(
        std::size_t index, std::size_t nodeCount)
    {
        std::size_t low = 0;
        std::size_t high = nodeCount;
        while (low + 1 < high)
        {
            const std::size_t middle = low + (high - low) / 2;
            if (storedIndex(middle, 0) <= index)
                low = middle;
            else
                high = middle;
        }
        return {low, index - storedIndex(low, 0)};
    }

    static const Bound& zero()
    {
        static const Bound value = Bound::finite(Rational());
        return value;
    }

    static const Bound& infinity()
    {
        static const Bound value = Bound::plusInfinity();
        return value;
    }

    static const Bound& implicit(std::size_t row, std::size_t column)
    {
        return row == column ? zero() : infinity();
    }

    static std::size_t noComponent()
    {
        return std::numeric_limits<std::size_t>::max();
    }

    void initializeCarrier()
    {
        if (kind_ == OctagonStorageKind::DenseHalf)
            dense_.resize(matrixSize(dimensions));
        else if (kind_ == OctagonStorageKind::ComponentDense)
        {
            componentOwner_.assign(dimensions, noComponent());
            componentLocal_.assign(dimensions, 0);
        }
    }

    void setSparse(std::size_t row, std::size_t column, const Bound& value)
    {
        const std::size_t index = matrixIndex(row, column);
        if (value.isPlusInfinity() || (row == column && value == zero()))
            sparse_.erase(index);
        else
            sparse_[index] = value;
    }

    const Bound& componentAt(std::size_t row, std::size_t column) const
    {
        const std::size_t rowVariable = row / 2;
        const std::size_t columnVariable = column / 2;
        const std::size_t owner = componentOwner_[rowVariable];
        if (owner == noComponent() ||
                owner != componentOwner_[columnVariable])
            return implicit(row, column);
        const std::size_t localRow = 2 * componentLocal_[rowVariable] + row % 2;
        const std::size_t localColumn =
            2 * componentLocal_[columnVariable] + column % 2;
        return (*components_[owner].matrix)[matrixIndex(localRow, localColumn)];
    }

    std::size_t createComponent(std::size_t variable)
    {
        const std::size_t owner = components_.size();
        Component component;
        component.variables.push_back(variable);
        component.matrix =
            std::make_shared<std::vector<Bound>>(matrixSize(1));
        (*component.matrix)[matrixIndex(0, 0)] = zero();
        (*component.matrix)[matrixIndex(1, 1)] = zero();
        components_.push_back(std::move(component));
        componentOwner_[variable] = owner;
        componentLocal_[variable] = 0;
        return owner;
    }

    std::size_t ensureComponent(std::size_t variable)
    {
        if (componentOwner_[variable] == noComponent())
            return createComponent(variable);
        return componentOwner_[variable];
    }

    std::size_t mergeComponents(std::size_t lhsOwner, std::size_t rhsOwner)
    {
        if (lhsOwner == rhsOwner)
            return lhsOwner;
        if (components_[lhsOwner].variables.size() <
                components_[rhsOwner].variables.size())
            std::swap(lhsOwner, rhsOwner);

        Component& lhs = components_[lhsOwner];
        const Component& rhs = components_[rhsOwner];
        const std::size_t oldSize = lhs.variables.size();
        std::vector<Bound> merged(matrixSize(oldSize + rhs.variables.size()));
        for (std::size_t node = 0; node < 2 * (oldSize + rhs.variables.size());
                ++node)
            merged[matrixIndex(node, node)] = zero();
        for (std::size_t row = 0; row < 2 * oldSize; ++row)
            for (std::size_t column = 0; column < 2 * oldSize; ++column)
                merged[matrixIndex(row, column)] =
                    (*lhs.matrix)[matrixIndex(row, column)];
        for (std::size_t row = 0; row < 2 * rhs.variables.size(); ++row)
            for (std::size_t column = 0; column < 2 * rhs.variables.size();
                    ++column)
                merged[matrixIndex(2 * oldSize + row, 2 * oldSize + column)] =
                    (*rhs.matrix)[matrixIndex(row, column)];

        for (std::size_t variable : rhs.variables)
        {
            componentOwner_[variable] = lhsOwner;
            componentLocal_[variable] = lhs.variables.size();
            lhs.variables.push_back(variable);
        }
        lhs.matrix =
            std::make_shared<std::vector<Bound>>(std::move(merged));

        components_.erase(components_.begin() + rhsOwner);
        for (std::size_t variable = 0; variable < dimensions; ++variable)
            if (componentOwner_[variable] != noComponent() &&
                    componentOwner_[variable] > rhsOwner)
                --componentOwner_[variable];
        return lhsOwner > rhsOwner ? lhsOwner - 1 : lhsOwner;
    }

    void setComponent(std::size_t row, std::size_t column, const Bound& value)
    {
        const std::size_t rowVariable = row / 2;
        const std::size_t columnVariable = column / 2;
        if (value.isPlusInfinity())
        {
            const std::size_t owner = componentOwner_[rowVariable];
            if (owner == noComponent() ||
                    owner != componentOwner_[columnVariable])
                return;
            const std::size_t localRow =
                2 * componentLocal_[rowVariable] + row % 2;
            const std::size_t localColumn =
                2 * componentLocal_[columnVariable] + column % 2;
            ensureUniqueMatrix(owner);
            (*components_[owner].matrix)[matrixIndex(localRow, localColumn)] =
                value;
            return;
        }
        if (row == column && value == zero() &&
                componentOwner_[rowVariable] == noComponent())
            return;
        std::size_t rowOwner = ensureComponent(rowVariable);
        std::size_t columnOwner = ensureComponent(columnVariable);
        const std::size_t owner = mergeComponents(rowOwner, columnOwner);
        const std::size_t localRow =
            2 * componentLocal_[rowVariable] + row % 2;
        const std::size_t localColumn =
            2 * componentLocal_[columnVariable] + column % 2;
        ensureUniqueMatrix(owner);
        (*components_[owner].matrix)[matrixIndex(localRow, localColumn)] = value;
    }

    void ensureUniqueMatrix(std::size_t owner)
    {
        Component& component = components_[owner];
        if (component.matrix.use_count() != 1)
            component.matrix =
                std::make_shared<std::vector<Bound>>(*component.matrix);
    }

    static std::vector<NumericKind> extractVariableKinds(
        const DimensionLayout& layout)
    {
        std::vector<NumericKind> result;
        result.reserve(layout.size());
        for (const DimensionEntry& declaration : layout.variables())
            result.push_back(declaration.variable.type().kind);
        return result;
    }

    OctagonStorageKind kind_ = OctagonStorageKind::DenseHalf;
    std::vector<Bound> dense_;
    std::unordered_map<std::size_t, Bound> sparse_;
    std::vector<Component> components_;
    std::vector<std::size_t> componentOwner_;
    std::vector<std::size_t> componentLocal_;
};

struct OctagonShape
{
    std::size_t dimensions = 0;
    std::size_t finiteStored = 0;
    std::size_t allocatedSlots = 0;
    std::size_t components = 0;
    std::size_t maximumComponent = 0;
    std::size_t unaryAnchors = 0;
    bool bottom = false;
};

OctagonShape measureShape(const OctagonStorage& state)
{
    OctagonShape result;
    result.dimensions = state.dimensions;
    result.allocatedSlots = state.allocatedSlots();
    result.bottom = state.bottom;
    result.finiteStored = state.finiteStoredSlots();

    std::vector<std::size_t> parent(state.dimensions);
    std::vector<std::size_t> componentSize(state.dimensions, 1);
    for (std::size_t dimension = 0; dimension < state.dimensions; ++dimension)
        parent[dimension] = dimension;
    const auto rootOf = [&](std::size_t start)
    {
        std::size_t root = start;
        while (parent[root] != root)
            root = parent[root];
        return root;
    };
    const auto unite = [&](std::size_t lhs, std::size_t rhs)
    {
        lhs = rootOf(lhs);
        rhs = rootOf(rhs);
        if (lhs == rhs)
            return;
        if (componentSize[lhs] < componentSize[rhs])
            std::swap(lhs, rhs);
        parent[rhs] = lhs;
        componentSize[lhs] += componentSize[rhs];
    };

    for (std::size_t lhs = 0; lhs < state.dimensions; ++lhs)
    {
        const std::size_t positive = positiveNode(lhs);
        const std::size_t negative = negativeNode(lhs);
        if (state.at(positive, negative).isFinite() ||
                state.at(negative, positive).isFinite())
            ++result.unaryAnchors;
        for (std::size_t rhs = lhs + 1; rhs < state.dimensions; ++rhs)
        {
            bool related = false;
            for (std::size_t lhsSign = 0; lhsSign < 2 && !related; ++lhsSign)
                for (std::size_t rhsSign = 0; rhsSign < 2; ++rhsSign)
                    if (state.at(2 * lhs + lhsSign,
                                 2 * rhs + rhsSign).isFinite())
                    {
                        related = true;
                        break;
                    }
            if (related)
                unite(lhs, rhs);
        }
    }
    for (std::size_t dimension = 0; dimension < state.dimensions; ++dimension)
    {
        if (rootOf(dimension) != dimension)
            continue;
        ++result.components;
        result.maximumComponent =
            std::max(result.maximumComponent, componentSize[dimension]);
    }
    return result;
}

const OctagonStorage& asOctagon(const OctagonStorage& state)
{
    return static_cast<const OctagonStorage&>(state);
}

OctagonStorage& asOctagon(OctagonStorage& state)
{
    return static_cast<OctagonStorage&>(state);
}

bool isNegativeDiagonal(const Bound& diagonal)
{
    if (diagonal.isMinusInfinity())
        return true;
    if (!diagonal.isFinite())
        return false;
    return diagonal.value().sign() < 0 ||
           (diagonal.value().isZero() && diagonal.isStrict());
}

Rational absolute(const Rational& value)
{
    return value.sign() < 0 ? -value : value;
}

/// Greatest even integer satisfying z <= bound (or z < bound for a strict
/// bound).  Unary integer octagon entries constrain 2*x and must be even.
Bound tightenIntegerUnary(const Bound& bound)
{
    if (!bound.isFinite())
        return bound;
    Rational greatest = bound.isStrict() ? bound.value().ceil() - Rational(1)
                        : bound.value().floor();
    Rational even = (greatest / Rational(2)).floor() * Rational(2);
    return Bound::finite(std::move(even));
}

} // namespace

const char* octagonStorageKindName(OctagonStorageKind kind)
{
    switch (kind)
    {
    case OctagonStorageKind::DenseHalf:
        return "dense-half";
    case OctagonStorageKind::SparseFinite:
        return "sparse-finite";
    case OctagonStorageKind::ComponentDense:
        return "component-dense";
    }
    return "unknown";
}

OctagonStorageKind octagonStorageKindFromName(const std::string& name)
{
    if (name == "dense-half")
        return OctagonStorageKind::DenseHalf;
    if (name == "sparse-finite")
        return OctagonStorageKind::SparseFinite;
    if (name == "component-dense")
        return OctagonStorageKind::ComponentDense;
    assert(false && "expected dense-half, sparse-finite, or component-dense Octagon storage");
    std::abort();
}

class OctagonDomain::Impl final
{
public:
    Impl(const DimensionLayout& layout, OctagonConfig options, bool bottom)
        : options_(std::move(options)),
          state_(OctagonStorage(layout, options_.storage, bottom))
    {
    }

    Impl(OctagonConfig options, OctagonStorage state)
        : options_(std::move(options)), state_(std::move(state))
    {
    }

    Impl(const Impl& other)
        : options_(other.options_),
          state_(std::move(*other.state_.clone(
                               other.options_.componentCopyOnWrite)))
    {
    }

    const char* name() const
    {
        return "gmp-octagon";
    }

    ApproximationKind assign(OctagonStorage& genericState,
                             const DimensionLayout& layout, Variable target,
                             const LinearExpression& expression) const
    {
        OctagonStorage& state = asOctagon(genericState);
        requireVariables(layout, expression);
        if (!layout.contains(target))
        {
            assert(false && "assignment target is not in relational layout");
            std::abort();
        }
        if (layout.typeOf(target).kind == NumericKind::IEEEFloat)
        {
            forget(state, layout.dimensionOf(target));
            return ApproximationKind::UnsupportedFallback;
        }
        if (state.bottom)
            return ApproximationKind::Exact;

        const Dimension targetDimension = layout.dimensionOf(target);
        if (expression.terms().empty())
        {
            forget(state, targetDimension);
            LinearExpression equality(target);
            equality -= expression;
            addConstraint(
                state, layout,
                LinearConstraint(std::move(equality), ConstraintKind::Equal));
            closeAfterSingleVariableUpdate(
                state, layout, targetDimension, true);
            return ApproximationKind::Exact;
        }

        if (expression.terms().size() == 1)
        {
            const auto& [source, coefficient] = *expression.terms().begin();
            if ((coefficient == Rational(1) || coefficient == Rational(-1)) &&
                    layout.typeOf(source).kind != NumericKind::IEEEFloat)
            {
                const Dimension sourceDimension =
                    layout.dimensionOf(source);
                if (sourceDimension == targetDimension)
                {
                    selfAssign(state, layout, targetDimension,
                               coefficient.sign(), expression.constant());
                }
                else
                {
                    forget(state, targetDimension);
                    LinearExpression equality(target);
                    equality -= expression;
                    addConstraint(state, layout,
                                  LinearConstraint(std::move(equality),
                                                   ConstraintKind::Equal));
                    closeAfterSingleVariableUpdate(
                        state, layout, targetDimension, false);
                }
                return ApproximationKind::Exact;
            }
        }

        // Strong-update soundness requires killing every old relation of the
        // target first. What follows is the recovery, which used to be absent:
        // the target was simply forgotten.
        //
        // The right-hand side has to be measured before the update, because
        // the update destroys what it is measured against. That matters even
        // when the recovery is the equality below: if the expression reads the
        // target, as in `x := 2*x + y`, then asserting `target = expression`
        // after the strong update is not a weaker fact, it is a false one --
        // the `target` in the expression is the old value and is already gone.
        // The best interval-only image is not enough for an octagon: every
        // objective E +/- y and -E +/- y yields a representable relation
        // between the new target x'=E and an unchanged variable y.  Measure
        // all of them on the incoming state before the strong update.  This
        // is the general octagonal affine-image construction; it does not
        // depend on the number or values of the coefficients in E.
        normalize(state, layout);
        const Interval assigned =
            evaluateInterval(state, layout, expression);
        struct RelationalImage
        {
            Variable variable;
            int sign;
            Bound targetUpper;
            Bound targetLower;
        };
        std::vector<RelationalImage> relationalImages;
        relationalImages.reserve(2 * (layout.size() - 1));
        for (const DimensionEntry& declaration : layout.variables())
        {
            if (declaration.variable == target)
                continue;
            for (const int sign :
                    {
                        -1, 1
                    })
            {
                LinearExpression positiveObjective = expression;
                positiveObjective.setCoefficient(
                    declaration.variable,
                    positiveObjective.coefficient(declaration.variable) +
                    Rational(sign));
                LinearExpression negativeObjective = -expression;
                negativeObjective.setCoefficient(
                    declaration.variable,
                    negativeObjective.coefficient(declaration.variable) +
                    Rational(sign));
                relationalImages.push_back(
                {
                    declaration.variable, sign,
                    boundExpression(state, layout, positiveObjective)
                    .upper(),
                    boundExpression(state, layout, negativeObjective)
                    .upper()});
            }
        }

        forget(state, targetDimension);
        if (assigned.upper().isFinite())
        {
            LinearExpression upper(target);
            upper.setConstant(-assigned.upper().value());
            addLessEqual(state, layout, upper, assigned.upper().isStrict(),
                         false);
        }
        if (assigned.lower().isFinite())
        {
            LinearExpression lower;
            lower.setCoefficient(target, Rational(-1));
            lower.setConstant(assigned.lower().value());
            addLessEqual(state, layout, lower, assigned.lower().isStrict(),
                         false);
        }
        for (const RelationalImage& image : relationalImages)
        {
            if (image.targetUpper.isFinite())
            {
                LinearExpression upper(target);
                upper.setCoefficient(image.variable, Rational(image.sign));
                upper.setConstant(-image.targetUpper.value());
                addLessEqual(state, layout, upper,
                             image.targetUpper.isStrict(), false);
            }
            if (image.targetLower.isFinite())
            {
                LinearExpression lower;
                lower.setCoefficient(target, Rational(-1));
                lower.setCoefficient(image.variable, Rational(image.sign));
                lower.setConstant(-image.targetLower.value());
                addLessEqual(state, layout, lower,
                             image.targetLower.isStrict(), false);
            }
        }
        closeAfterSingleVariableUpdate(
            state, layout, targetDimension, false);
        return ApproximationKind::SoundOverApproximation;
    }

    ApproximationKind assume(OctagonStorage& genericState,
                             const DimensionLayout& layout,
                             const LinearConstraint& constraint) const
    {
        OctagonStorage& state = asOctagon(genericState);
        const bool wasClosed = state.stronglyClosed;
        std::optional<Dimension> touchedVariable;
        const ApproximationKind approximation =
            addAssumption(state, layout, constraint, &touchedVariable);
        if (wasClosed && approximation == ApproximationKind::Exact &&
                touchedVariable)
            closeAfterSingleVariableUpdate(
                state, layout, *touchedVariable, false);
        else
            normalize(state, layout);
        return approximation;
    }

    ApproximationKind addAssumption(
        OctagonStorage& state, const DimensionLayout& layout,
        const LinearConstraint& constraint,
        std::optional<Dimension>* touchedVariable = nullptr) const
    {
        requireVariables(layout, constraint.expression());
        for (const auto& [variable, coefficient] :
                constraint.expression().terms())
        {
            (void)coefficient;
            // Unary rows describe finite value bounds, not floating-point
            // arithmetic. Retain them when importing rounded tree results,
            // Box projections, or a serialized DBM. Multi-variable IEEE
            // arithmetic still uses the explicit TreeExpression fallback.
            if (layout.typeOf(variable).kind == NumericKind::IEEEFloat &&
                    constraint.expression().terms().size() != 1)
                return ApproximationKind::UnsupportedFallback;
        }
        const bool exact = addConstraint(state, layout, constraint);
        if (exact && touchedVariable)
        {
            for (const auto& [variable, coefficient] :
                    constraint.expression().terms())
            {
                if (!coefficient.isZero())
                {
                    *touchedVariable = layout.dimensionOf(variable);
                    break;
                }
            }
        }
        return exact ? ApproximationKind::Exact
               : ApproximationKind::SoundOverApproximation;
    }

    void forget(OctagonStorage& genericState, const DimensionLayout& layout,
                Variable variable) const
    {
        if (!layout.contains(variable))
        {
            assert(false && "forgotten variable is not in relational layout");
            std::abort();
        }
        forget(asOctagon(genericState), layout.dimensionOf(variable));
    }

    std::unique_ptr<OctagonStorage> join(const OctagonStorage& genericLhs,
                                         const OctagonStorage& genericRhs) const
    {
        std::optional<OctagonStorage> lhsStorage;
        std::optional<OctagonStorage> rhsStorage;
        const OctagonStorage& lhs =
            normalized(asOctagon(genericLhs), lhsStorage);
        const OctagonStorage& rhs =
            normalized(asOctagon(genericRhs), rhsStorage);
        requireSameSize(lhs, rhs);
        if (lhs.bottom)
            return std::make_unique<OctagonStorage>(rhs.converted(
                    lhs.kind(), options_.componentCopyOnWrite));
        if (rhs.bottom)
            return lhs.clone(options_.componentCopyOnWrite);

        if (lhs.kind() != OctagonStorageKind::DenseHalf)
        {
            auto result = std::make_unique<OctagonStorage>(
                              lhs.variableKinds, lhs.kind(),
                              OctagonStorage::ScratchMatrixTag{});
            lhs.copyFiniteMaximumTo(rhs, *result);
            result->stronglyClosed = true;
            return result;
        }

        // Dense half matrices retain copy-on-write behavior by starting from
        // lhs and replacing only cells for which rhs is looser.
        auto result = lhs.clone(options_.componentCopyOnWrite);
        bool removedFiniteRelation = false;
        for (std::size_t row = 0; row < result->nodes(); ++row)
            for (std::size_t column = 0; column <= (row | 1); ++column)
            {
                const Bound& left = lhs.at(row, column);
                const Bound& right = rhs.at(row, column);
                if (left < right)
                {
                    removedFiniteRelation =
                        removedFiniteRelation ||
                        (left.isFinite() && right.isPlusInfinity());
                    result->set(row, column, right);
                }
            }
        if (removedFiniteRelation)
            result->compactComponents();

        // Point-wise maximum preserves coherence and every closure inequality.
        // If R[i,j] comes from operand K, then
        // K[i,j] <= K[i,h] + K[h,j] <= R[i,h] + R[h,j]; the same monotonicity
        // argument applies to the strong-closure inequality. Integer-tight
        // unary entries remain even because the maximum of two even bounds is
        // even, so the result needs no further closure.
        result->stronglyClosed = true;
        return result;
    }

    std::unique_ptr<OctagonStorage> meet(const OctagonStorage& genericLhs,
                                         const OctagonStorage& genericRhs) const
    {
        std::optional<OctagonStorage> lhsStorage;
        std::optional<OctagonStorage> rhsStorage;
        const OctagonStorage& lhs =
            normalized(asOctagon(genericLhs), lhsStorage);
        const OctagonStorage& rhs =
            normalized(asOctagon(genericRhs), rhsStorage);
        requireSameSize(lhs, rhs);
        if (lhs.bottom)
            return lhs.clone(options_.componentCopyOnWrite);
        if (rhs.bottom)
            return rhs.clone(options_.componentCopyOnWrite);

        auto result = std::make_unique<OctagonStorage>(
                          lhs.variableKinds, lhs.kind(), OctagonStorage::ScratchMatrixTag{});
        for (std::size_t row = 0; row < result->nodes(); ++row)
            for (std::size_t column = 0; column < result->nodes(); ++column)
            {
                const Bound& left = lhs.at(row, column);
                const Bound& right = rhs.at(row, column);
                result->set(row, column, left <= right ? left : right);
            }
        result->stronglyClosed = false;
        normalize(*result);
        return result;
    }

    std::unique_ptr<OctagonStorage> widen(const OctagonStorage& genericCurrent,
                                          const OctagonStorage& genericNext,
                                          const WideningPolicy& policy) const
    {
        const OctagonStorage& current = asOctagon(genericCurrent);
        std::optional<OctagonStorage> nextStorage;
        const OctagonStorage& next =
            normalized(asOctagon(genericNext), nextStorage);
        requireSameSize(current, next);
        if (current.bottom)
            return std::make_unique<OctagonStorage>(
                       next.converted(current.kind(),
                                      options_.componentCopyOnWrite));
        if (next.bottom)
            return current.clone(options_.componentCopyOnWrite);

        std::vector<Rational> thresholds = policy.thresholds;
        std::sort(thresholds.begin(), thresholds.end());

        auto result = std::make_unique<OctagonStorage>(
                          current.variableKinds, current.kind(),
                          OctagonStorage::ScratchMatrixTag{});
        for (std::size_t row = 0; row < result->nodes(); ++row)
        {
            for (std::size_t column = 0; column < result->nodes(); ++column)
            {
                const Bound& oldBound = current.at(row, column);
                const Bound& nextBound = next.at(row, column);
                if (nextBound <= oldBound)
                {
                    result->set(row, column, oldBound);
                    continue;
                }

                result->set(row, column, Bound::plusInfinity());
                if (nextBound.isFinite())
                {
                    // Public thresholds use normalized octagonal constants.
                    // A unary DBM entry encodes +/-2*x, so its matrix bound
                    // needs twice the user-visible threshold.
                    const bool unary = row != column && row / 2 == column / 2;
                    for (const Rational& threshold : thresholds)
                    {
                        Bound candidate = Bound::finite(
                                              unary ? threshold * Rational(2) : threshold);
                        if (nextBound <= candidate)
                        {
                            result->set(row, column, candidate);
                            break;
                        }
                    }
                }
            }
        }
        for (std::size_t node = 0; node < result->nodes(); ++node)
            result->set(node, node, Bound::finite(Rational()));
        result->stronglyClosed = false;
        return result;
    }

    std::unique_ptr<OctagonStorage> narrow(const OctagonStorage& genericCurrent,
                                           const OctagonStorage& genericNext) const
    {
        std::optional<OctagonStorage> currentStorage;
        std::optional<OctagonStorage> nextStorage;
        const OctagonStorage& current =
            normalized(asOctagon(genericCurrent), currentStorage);
        const OctagonStorage& next =
            normalized(asOctagon(genericNext), nextStorage);
        requireSameSize(current, next);
        if (current.bottom || next.bottom)
            return next.clone(options_.componentCopyOnWrite);

        auto result = current.clone(options_.componentCopyOnWrite);
        for (std::size_t row = 0; row < result->nodes(); ++row)
            for (std::size_t column = 0; column < result->nodes(); ++column)
                if (current.at(row, column).isPlusInfinity() &&
                        next.at(row, column).isFinite())
                    result->set(row, column, next.at(row, column));
        result->stronglyClosed = false;
        return result;
    }

    std::unique_ptr<OctagonStorage> projectLowerBounds(
        const OctagonStorage& genericState) const
    {
        std::optional<OctagonStorage> sourceStorage;
        const OctagonStorage& source =
            normalized(asOctagon(genericState), sourceStorage);
        if (source.bottom)
            return source.clone(options_.componentCopyOnWrite);

        auto result = std::make_unique<OctagonStorage>(
                          source.variableKinds, source.kind());
        for (Dimension dimension = 0; dimension < source.dimensions;
                ++dimension)
        {
            setCoherent(
                *result, negativeNode(dimension), positiveNode(dimension),
                source.at(negativeNode(dimension), positiveNode(dimension)));
        }
        for (Dimension lhs = 0; lhs < source.dimensions; ++lhs)
        {
            for (Dimension rhs = lhs + 1; rhs < source.dimensions; ++rhs)
            {
                // -(x+y) and -(x-y) are the selected lower orientations.
                setCoherent(*result, negativeNode(lhs), positiveNode(rhs),
                            source.at(negativeNode(lhs), positiveNode(rhs)));
                setCoherent(*result, negativeNode(lhs), negativeNode(rhs),
                            source.at(negativeNode(lhs), negativeNode(rhs)));
            }
        }
        result->stronglyClosed = false;
        normalize(*result);
        return result;
    }

    std::unique_ptr<OctagonStorage> changeLayout(
        const OctagonStorage& genericState, const DimensionLayout& oldLayout,
        const DimensionLayout& newLayout) const
    {
        std::optional<OctagonStorage> sourceStorage;
        const OctagonStorage& source =
            normalized(asOctagon(genericState), sourceStorage);
        if (source.dimensions != oldLayout.size())
        {
            assert(false && "old layout does not match octagon dimensions");
            std::abort();
        }
        if (source.bottom)
            return std::make_unique<OctagonStorage>(newLayout,
                                                    source.kind(), true);

        auto result = std::make_unique<OctagonStorage>(newLayout,
            source.kind());
        std::vector<std::optional<Dimension>> newDimensions(
            oldLayout.size());
        for (Dimension oldDimension = 0;
                oldDimension < oldLayout.size(); ++oldDimension)
        {
            const Variable variable = oldLayout.variableOf(oldDimension);
            if (!newLayout.contains(variable))
                continue;
            if (oldLayout.typeOf(variable) !=
                    newLayout.typeOf(variable))
            {
                assert(false && "layout change cannot alter a variable's type");
                std::abort();
            }
            newDimensions[oldDimension] =
                newLayout.dimensionOf(variable);
        }
        source.copyRemappedFiniteBoundsTo(*result, newDimensions);

        // The source is strongly closed. Restricting it to a principal
        // submatrix, permuting dimensions, and adding independent top
        // dimensions all preserve shortest-path, strong, and integer closure.
        result->stronglyClosed = true;
        return result;
    }

    bool isBottom(const OctagonStorage& genericState) const
    {
        std::optional<OctagonStorage> storage;
        return normalized(asOctagon(genericState), storage).bottom;
    }

    bool isTop(const OctagonStorage& genericState) const
    {
        std::optional<OctagonStorage> storage;
        const OctagonStorage& state =
            normalized(asOctagon(genericState), storage);
        if (state.bottom)
            return false;
        for (std::size_t row = 0; row < state.nodes(); ++row)
        {
            for (std::size_t column = 0; column < state.nodes(); ++column)
            {
                if (row == column)
                    continue;
                if (!state.at(row, column).isPlusInfinity())
                    return false;
            }
        }
        return true;
    }

    bool leq(const OctagonStorage& genericLhs, const OctagonStorage& genericRhs) const
    {
        std::optional<OctagonStorage> lhsStorage;
        std::optional<OctagonStorage> rhsStorage;
        const OctagonStorage& lhs =
            normalized(asOctagon(genericLhs), lhsStorage);
        const OctagonStorage& rhs =
            normalized(asOctagon(genericRhs), rhsStorage);
        requireSameSize(lhs, rhs);
        if (lhs.bottom)
            return true;
        if (rhs.bottom)
            return false;
        return rhs.contains(lhs);
    }

    Interval bound(const OctagonStorage& genericState,
                   const DimensionLayout& layout, Variable variable) const
    {
        if (!layout.contains(variable))
        {
            assert(false && "bounded variable is not in relational layout");
            std::abort();
        }
        std::optional<OctagonStorage> storage;
        const OctagonStorage& state =
            normalized(asOctagon(genericState), storage);
        if (state.bottom)
            return Interval(Bound::plusInfinity(), Bound::minusInfinity());

        const Dimension dimension = layout.dimensionOf(variable);
        const Bound& doubledUpper =
            state.at(positiveNode(dimension), negativeNode(dimension));
        const Bound& doubledNegativeLower =
            state.at(negativeNode(dimension), positiveNode(dimension));

        Bound lower = Bound::minusInfinity();
        Bound upper = Bound::plusInfinity();
        if (doubledUpper.isFinite())
            upper = Bound::divideByTwo(doubledUpper);
        if (doubledNegativeLower.isFinite())
        {
            lower = Bound::finite(
                        -doubledNegativeLower.value().dividedByPowerOfTwo(1),
                        doubledNegativeLower.isStrict());
        }
        return Interval(std::move(lower), std::move(upper));
    }

    LinearConstraintSet constraints(const OctagonStorage& genericState,
                                    const DimensionLayout& layout) const
    {
        std::optional<OctagonStorage> storage;
        const OctagonStorage& state =
            normalized(asOctagon(genericState), storage);
        LinearConstraintSet result;
        if (state.bottom)
            return result;

        for (std::size_t row = 0; row < state.nodes(); ++row)
        {
            for (std::size_t column = 0; column < state.nodes(); ++column)
            {
                if (row == column || !state.at(row, column).isFinite())
                    continue;

                // Coherent entries describe the same octagonal constraint.
                const std::size_t coherentRow = opposite(column);
                const std::size_t coherentColumn = opposite(row);
                if (std::pair<std::size_t, std::size_t>(coherentRow,
                                                        coherentColumn) <
                        std::pair<std::size_t, std::size_t>(row, column))
                    continue;

                LinearExpression expression;
                addNodeTerm(expression, layout, row, Rational(1));
                addNodeTerm(expression, layout, column, Rational(-1));
                expression.setConstant(-state.at(row, column).value());
                result.emplace_back(std::move(expression),
                                    state.at(row, column).isStrict()
                                    ? ConstraintKind::LessThan
                                    : ConstraintKind::LessEqual);
            }
        }
        return result;
    }

    std::string toString(const OctagonStorage& genericState,
                         const DimensionLayout& layout) const
    {
        if (isBottom(genericState))
            return "bottom";
        const LinearConstraintSet exported =
            constraints(genericState, layout);
        if (exported.empty())
            return "top";
        std::ostringstream output;
        for (std::size_t index = 0; index < exported.size(); ++index)
        {
            if (index != 0)
                output << " && ";
            output << exported[index].toString();
        }
        return output.str();
    }

    const OctagonConfig& config() const
    {
        return options_;
    }

    OctagonStorageStats storageStats() const
    {
        const OctagonShape shape = measureShape(state_);
        return {state_.kind(), shape.dimensions, shape.finiteStored,
                shape.allocatedSlots, shape.components,
                shape.maximumComponent};
    }

    void reconfigure(OctagonConfig options)
    {
        if (state_.kind() != options.storage)
            state_ = state_.converted(options.storage,
                                      options.componentCopyOnWrite);
        options_ = std::move(options);
        // A matrix marked closed under a weaker policy must be normalized
        // again when stronger integer/strong closure is enabled.
        if (!state_.bottom)
        {
            state_.stronglyClosed = false;
            normalize(state_);
        }
    }

    ApproximationKind assignCurrent(const DimensionLayout& layout,
                                    Variable target,
                                    const LinearExpression& expression)
    {
        return assign(state_, layout, target, expression);
    }

    ApproximationKind assumeCurrent(const DimensionLayout& layout,
                                    const LinearConstraint& constraint)
    {
        return assume(state_, layout, constraint);
    }

    ApproximationKind assumeAllCurrent(
        const DimensionLayout& layout,
        const LinearConstraintSet& constraints)
    {
        const bool wasClosed = state_.stronglyClosed;
        ApproximationKind result = ApproximationKind::Exact;
        std::optional<Dimension> touchedVariable;
        bool commonTouchedVariable = true;
        for (const LinearConstraint& constraint : constraints)
        {
            std::optional<Dimension> currentTouchedVariable;
            const ApproximationKind current =
                addAssumption(state_, layout, constraint,
                              &currentTouchedVariable);
            if (!currentTouchedVariable)
                commonTouchedVariable = false;
            else if (!touchedVariable)
                touchedVariable = currentTouchedVariable;
            else if (*touchedVariable != *currentTouchedVariable)
                commonTouchedVariable = false;
            if (current == ApproximationKind::UnsupportedFallback)
                result = current;
            else if (current != ApproximationKind::Exact &&
                     result == ApproximationKind::Exact)
                result = current;
        }
        if (wasClosed && result == ApproximationKind::Exact &&
                commonTouchedVariable && touchedVariable)
            closeAfterSingleVariableUpdate(
                state_, layout, *touchedVariable, false);
        else
            normalize(state_, layout);
        return result;
    }

    void forgetCurrent(const DimensionLayout& layout, Variable variable)
    {
        forget(state_, layout, variable);
    }

    void assignIntervalCurrent(const DimensionLayout& layout,
                               Variable target, const Interval& value)
    {
        if (!layout.contains(target))
        {
            assert(false && "assignment target is not in relational layout");
            std::abort();
        }
        if (state_.bottom)
            return;
        const Dimension dimension = layout.dimensionOf(target);
        forget(state_, dimension);
        if (value.isBottom())
        {
            state_.bottom = true;
            return;
        }
        if (value.upper().isFinite())
        {
            LinearExpression upper(target);
            upper.setConstant(-value.upper().value());
            addLessEqual(state_, layout, upper,
                         value.upper().isStrict(), false);
        }
        if (value.lower().isFinite())
        {
            LinearExpression lower;
            lower.setCoefficient(target, Rational(-1));
            lower.setConstant(value.lower().value());
            addLessEqual(state_, layout, lower,
                         value.lower().isStrict(), false);
        }
        closeAfterSingleVariableUpdate(state_, layout, dimension, true);
    }

    void canonicalizeCurrent()
    {
        normalize(state_);
    }

    void joinCurrent(const Impl& other)
    {
        state_ = std::move(*join(state_, other.state_));
    }

    std::unique_ptr<Impl> joined(const Impl& other) const
    {
        return std::make_unique<Impl>(
                   options_, std::move(*join(state_, other.state_)));
    }

    void meetCurrent(const Impl& other)
    {
        state_ = std::move(*meet(state_, other.state_));
    }

    std::unique_ptr<Impl> met(const Impl& other) const
    {
        return std::make_unique<Impl>(
                   options_, std::move(*meet(state_, other.state_)));
    }

    void widenCurrent(const Impl& other, const WideningPolicy& policy)
    {
        state_ = std::move(*widen(state_, other.state_, policy));
    }

    std::unique_ptr<Impl> widened(
        const Impl& other, const WideningPolicy& policy) const
    {
        return std::make_unique<Impl>(
                   options_, std::move(*widen(state_, other.state_, policy)));
    }

    void narrowCurrent(const Impl& other)
    {
        state_ = std::move(*narrow(state_, other.state_));
    }

    std::unique_ptr<Impl> narrowed(const Impl& other) const
    {
        return std::make_unique<Impl>(
                   options_, std::move(*narrow(state_, other.state_)));
    }

    void projectLowerBoundsCurrent()
    {
        state_ = std::move(*projectLowerBounds(state_));
    }

    std::unique_ptr<Impl> projectedLowerBounds() const
    {
        return std::make_unique<Impl>(
                   options_, std::move(*projectLowerBounds(state_)));
    }

    void changeLayoutCurrent(const DimensionLayout& oldLayout,
                             const DimensionLayout& newLayout)
    {
        state_ = std::move(*changeLayout(
                               state_, oldLayout, newLayout));
    }

    bool isBottomCurrent() const
    {
        return isBottom(state_);
    }

    bool isTopCurrent() const
    {
        return isTop(state_);
    }

    bool leqCurrent(const Impl& other) const
    {
        return leq(state_, other.state_);
    }

    Interval boundCurrent(const DimensionLayout& layout,
                          Variable variable) const
    {
        return bound(state_, layout, variable);
    }

    Interval boundExpressionCurrent(
        const DimensionLayout& layout,
        const LinearExpression& expression) const
    {
        return boundExpression(state_, layout, expression);
    }

    std::vector<Variable> relationalClosureCurrent(
        const DimensionLayout& layout,
        const std::vector<Variable>& seeds) const
    {
        std::set<Variable> closure(seeds.begin(), seeds.end());
        std::optional<OctagonStorage> normalizedStorage;
        const OctagonStorage& state = normalized(state_, normalizedStorage);
        if (!state.bottom)
            for (Variable seed : seeds)
                if (layout.contains(seed))
                    for (Dimension dimension :
                            state.relationComponent(layout.dimensionOf(seed)))
                        closure.insert(layout.variableOf(dimension));
        return std::vector<Variable>(closure.begin(), closure.end());
    }

    LinearConstraintSet constraintsCurrent(
        const DimensionLayout& layout) const
    {
        return constraints(state_, layout);
    }

    std::string toStringCurrent(const DimensionLayout& layout) const
    {
        return toString(state_, layout);
    }

private:
    void requireSameSize(const OctagonStorage& lhs, const OctagonStorage& rhs) const
    {
        if (lhs.dimensions != rhs.dimensions ||
                lhs.variableKinds != rhs.variableKinds)
        {
            assert(false && "octagon state shapes do not match");
            std::abort();
        }
    }

    void requireVariables(const DimensionLayout& layout,
                          const LinearExpression& expression) const
    {
        for (const auto& [variable, coefficient] : expression.terms())
        {
            (void)coefficient;
            if (!layout.contains(variable))
            {
                assert(false && "expression variable is not in relational layout");
                std::abort();
            }
        }
    }

    void setCoherent(OctagonStorage& state, std::size_t row, std::size_t column,
                     const Bound& bound) const
    {
        state.tighten(row, column, bound);
        const std::size_t coherentRow = opposite(column);
        const std::size_t coherentColumn = opposite(row);
        state.tighten(coherentRow, coherentColumn, bound);
        state.stronglyClosed = false;
    }

    void forget(OctagonStorage& state, Dimension dimension) const
    {
        if (dimension >= state.dimensions)
        {
            assert(false && "invalid forgotten octagon dimension");
            std::abort();
        }
        if (state.bottom)
            return;
        normalize(state);
        if (state.forgetComponentDimension(dimension))
        {
            state.stronglyClosed = true;
            return;
        }
        const std::size_t first = positiveNode(dimension);
        const std::size_t second = negativeNode(dimension);
        const Bound infinity = Bound::plusInfinity();
        for (std::size_t node = 0; node < state.nodes(); ++node)
        {
            state.set(first, node, infinity);
            state.set(second, node, infinity);
            state.set(node, first, infinity);
            state.set(node, second, infinity);
        }
        state.set(first, first, Bound::finite(Rational()));
        state.set(second, second, Bound::finite(Rational()));
        state.compactComponents();
        state.stronglyClosed = true;
    }

    bool addConstraint(OctagonStorage& state, const DimensionLayout& layout,
                       const LinearConstraint& constraint) const
    {
        if (state.bottom)
            return true;

        switch (constraint.kind())
        {
        case ConstraintKind::Equal:
        {
            const bool forward = addLessEqual(state, layout,
                                              constraint.expression(), false);
            const bool backward = addLessEqual(state, layout,
                                               -constraint.expression(), false);
            return forward && backward;
        }
        case ConstraintKind::NotEqual:
            return handleConstantNotEqual(state, constraint.expression());
        case ConstraintKind::LessThan:
            return addLessEqual(state, layout, constraint.expression(),
                                true);
        case ConstraintKind::LessEqual:
            return addLessEqual(state, layout, constraint.expression(),
                                false);
        case ConstraintKind::GreaterThan:
            return addLessEqual(state, layout, -constraint.expression(),
                                true);
        case ConstraintKind::GreaterEqual:
            return addLessEqual(state, layout, -constraint.expression(),
                                false);
        }
        return false;
    }

    bool handleConstantNotEqual(OctagonStorage& state,
                                const LinearExpression& expression) const
    {
        if (!expression.terms().empty())
            return false;
        if (expression.constant().isZero())
            state.bottom = true;
        return true;
    }

    bool addLessEqual(OctagonStorage& state, const DimensionLayout& layout,
                      const LinearExpression& expression, bool strict,
                      bool allowLinearization = true) const
    {
        const auto& terms = expression.terms();
        if (terms.empty())
        {
            const int sign = expression.constant().sign();
            if (sign > 0 || (sign == 0 && strict))
                state.bottom = true;
            return true;
        }

        if (terms.size() == 1)
        {
            const auto& [variable, coefficient] = *terms.begin();
            if (coefficient.isZero())
                return false;
            const Dimension dimension = layout.dimensionOf(variable);
            const Rational magnitude = absolute(coefficient);
            Rational value = -expression.constant() / magnitude;
            bool storedStrict = strict;
            if (storedStrict && options_.integerTightening &&
                    layout.typeOf(variable).kind == NumericKind::Integer)
            {
                value = value.ceil() - Rational(1);
                storedStrict = false;
            }
            const std::size_t row = coefficient.sign() > 0
                                    ? positiveNode(dimension)
                                    : negativeNode(dimension);
            const std::size_t column = opposite(row);
            setCoherent(state, row, column,
                        Bound::finite(value * Rational(2), storedStrict));
            return true;
        }

        if (terms.size() == 2)
        {
            auto it = terms.begin();
            const Variable lhsVariable = it->first;
            const Rational lhsCoefficient = it->second;
            ++it;
            const Variable rhsVariable = it->first;
            const Rational rhsCoefficient = it->second;
            const Rational lhsMagnitude = absolute(lhsCoefficient);
            if (lhsMagnitude != absolute(rhsCoefficient) ||
                    lhsMagnitude.isZero())
            {
                if (allowLinearization)
                    addLinearized(state, layout, expression, strict);
                return false;
            }

            const Dimension lhsDimension = layout.dimensionOf(lhsVariable);
            const Dimension rhsDimension = layout.dimensionOf(rhsVariable);
            const std::size_t lhsNode = lhsCoefficient.sign() > 0
                                        ? positiveNode(lhsDimension)
                                        : negativeNode(lhsDimension);
            const std::size_t negativeRhsNode =
                rhsCoefficient.sign() > 0 ? negativeNode(rhsDimension)
                : positiveNode(rhsDimension);
            Rational value = -expression.constant() / lhsMagnitude;
            bool storedStrict = strict;
            if (storedStrict && options_.integerTightening &&
                    layout.typeOf(lhsVariable).kind == NumericKind::Integer &&
                    layout.typeOf(rhsVariable).kind == NumericKind::Integer)
            {
                value = value.ceil() - Rational(1);
                storedStrict = false;
            }
            setCoherent(state, lhsNode, negativeRhsNode,
                        Bound::finite(value, storedStrict));
            return true;
        }

        if (allowLinearization)
            addLinearized(state, layout, expression, strict);
        return false;
    }

    /// Interval of a linear expression under the current per-variable bounds.
    Interval evaluateInterval(const OctagonStorage& state,
                              const DimensionLayout& layout,
                              const LinearExpression& expression) const
    {
        Rational low = expression.constant();
        Rational high = expression.constant();
        bool lowFinite = true;
        bool highFinite = true;
        bool lowStrict = false;
        bool highStrict = false;
        for (const auto& [variable, coefficient] : expression.terms())
        {
            if (coefficient.isZero())
                continue;
            const Interval interval = bound(state, layout, variable);
            const Bound& least =
                coefficient.sign() > 0 ? interval.lower() : interval.upper();
            const Bound& greatest =
                coefficient.sign() > 0 ? interval.upper() : interval.lower();
            if (lowFinite && least.isFinite())
            {
                low += least.value() * coefficient;
                lowStrict = lowStrict || least.isStrict();
            }
            else
            {
                lowFinite = false;
            }
            if (highFinite && greatest.isFinite())
            {
                high += greatest.value() * coefficient;
                highStrict = highStrict || greatest.isStrict();
            }
            else
            {
                highFinite = false;
            }
        }
        return Interval(lowFinite ? Bound::finite(low, lowStrict)
                        : Bound::minusInfinity(),
                        highFinite ? Bound::finite(high, highStrict)
                        : Bound::plusInfinity());
    }

    Interval boundExpression(const OctagonStorage& state,
                             const DimensionLayout& layout,
                             const LinearExpression& expression) const
    {
        requireVariables(layout, expression);
        if (state.bottom)
            return Interval(Bound::plusInfinity(), Bound::minusInfinity());
        if (expression.terms().size() != 2)
            return evaluateInterval(state, layout, expression);

        auto term = expression.terms().begin();
        const Variable firstVariable = term->first;
        const Rational firstCoefficient = term->second;
        ++term;
        const Variable secondVariable = term->first;
        const Rational secondCoefficient = term->second;
        const Rational magnitude = absolute(firstCoefficient);
        if (magnitude.isZero() ||
                magnitude != absolute(secondCoefficient))
            return evaluateInterval(state, layout, expression);

        std::optional<OctagonStorage> normalizedStorage;
        const OctagonStorage& source = normalized(state, normalizedStorage);
        const auto signedSumUpper = [&](bool negate) -> Bound
        {
            const int firstSign =
            (negate ? -firstCoefficient : firstCoefficient).sign();
            const int secondSign =
            (negate ? -secondCoefficient : secondCoefficient).sign();
            const std::size_t row =
            firstSign > 0
            ? positiveNode(layout.dimensionOf(firstVariable))
            : negativeNode(layout.dimensionOf(firstVariable));
            const std::size_t column =
            secondSign > 0
            ? negativeNode(layout.dimensionOf(secondVariable))
            : positiveNode(layout.dimensionOf(secondVariable));
            return source.at(row, column);
        };

        const Bound positive = signedSumUpper(false);
        const Bound negative = signedSumUpper(true);
        const Bound lower = negative.isFinite()
                            ? Bound::finite(
                                expression.constant() -
                                magnitude * negative.value(),
                                negative.isStrict())
                            : Bound::minusInfinity();
        const Bound upper = positive.isFinite()
                            ? Bound::finite(
                                expression.constant() +
                                magnitude * positive.value(),
                                positive.isStrict())
                            : Bound::plusInfinity();
        return Interval(lower, upper);
    }

    /// Strongest octagonal consequences of a constraint that is not itself
    /// octagonal.
    ///
    /// A DBM cannot store `sum a_i x_i + k <= 0` once it has three terms, or
    /// two terms of different magnitude. Dropping it is sound and needlessly
    /// weak: replacing the terms that cannot be stored by their current bounds
    /// leaves a constraint over one or two variables that can be. That is
    /// Mine's interval linearization, restricted to the sub-expressions this
    /// domain represents exactly, and it is what makes a guard such as
    /// `2*i + 3*j <= 10` narrow anything at all here.
    void addLinearized(OctagonStorage& state,
                       const DimensionLayout& layout,
                       const LinearExpression& expression, bool strict) const
    {
        normalize(state, layout);
        if (state.bottom)
            return;

        std::vector<Variable> variables;
        for (const auto& [variable, coefficient] : expression.terms())
        {
            if (!coefficient.isZero())
                variables.push_back(variable);
        }
        if (variables.size() < 2)
            return;

        // Derive every bound from the same pre-constraint snapshot, so the
        // result does not depend on the order the sub-constraints are applied.
        std::map<Variable, Interval> intervals;
        for (Variable variable : variables)
            intervals.emplace(variable, bound(state, layout, variable));

        // Least value the terms outside `kept` can take.
        const auto restMinimum = [&](const std::vector<Variable>& kept,
                                     Rational& total, bool& totalStrict)
        {
            total = Rational();
            totalStrict = false;
            for (const auto& [variable, coefficient] : expression.terms())
            {
                if (coefficient.isZero())
                    continue;
                if (std::find(kept.begin(), kept.end(), variable) != kept.end())
                    continue;
                const Interval& interval = intervals.at(variable);
                const Bound& endpoint = coefficient.sign() > 0
                                        ? interval.lower()
                                        : interval.upper();
                if (!endpoint.isFinite())
                    return false;
                total += endpoint.value() * coefficient;
                totalStrict = totalStrict || endpoint.isStrict();
            }
            return true;
        };

        const auto applyUnary = [&](Variable kept)
        {
            Rational rest;
            bool restStrict = false;
            if (!restMinimum({kept}, rest, restStrict))
                return;
            LinearExpression reduced(expression.constant() + rest);
            reduced.setCoefficient(kept, expression.coefficient(kept));
            addLessEqual(state, layout, reduced, strict || restStrict,
                         false);
        };

        // For a pair a*x+b*y, retain the largest common octagonal part
        // m*(sign(a)*x+sign(b)*y), where m=min(|a|,|b|), and intervalize only
        // the coefficient remainders.  When |a|=|b| this is the exact pair
        // handled previously.  Unequal magnitudes now produce the strongest
        // consequence available from this interval decomposition, and the
        // construction is invariant under positive scaling of the input row.
        const auto applyPair = [&](Variable first, Variable second)
        {
            const Rational firstCoefficient = expression.coefficient(first);
            const Rational secondCoefficient = expression.coefficient(second);
            const Rational common =
                std::min(absolute(firstCoefficient),
                         absolute(secondCoefficient));
            if (common.isZero())
                return;

            Rational rest = expression.constant();
            bool restStrict = false;
            for (const auto& [variable, coefficient] : expression.terms())
            {
                Rational remainder = coefficient;
                if (variable == first)
                    remainder -= Rational(firstCoefficient.sign()) * common;
                else if (variable == second)
                    remainder -= Rational(secondCoefficient.sign()) * common;
                if (remainder.isZero())
                    continue;
                const Interval& interval = intervals.at(variable);
                const Bound& endpoint = remainder.sign() > 0
                                        ? interval.lower()
                                        : interval.upper();
                if (!endpoint.isFinite())
                    return;
                rest += remainder * endpoint.value();
                restStrict = restStrict || endpoint.isStrict();
            }

            LinearExpression reduced(rest);
            reduced.setCoefficient(
                first, Rational(firstCoefficient.sign()) * common);
            reduced.setCoefficient(
                second, Rational(secondCoefficient.sign()) * common);
            addLessEqual(state, layout, reduced, strict || restStrict,
                         false);
        };

        for (std::size_t first = 0; first < variables.size(); ++first)
        {
            applyUnary(variables[first]);
            for (std::size_t second = first + 1; second < variables.size();
                    ++second)
                applyPair(variables[first], variables[second]);
        }
    }

    void selfAssign(OctagonStorage& state, const DimensionLayout& layout,
                    Dimension target, int sign, const Rational& constant) const
    {
        normalize(state, layout);
        if (state.bottom)
            return;
        OctagonStorage old = std::move(state);
        OctagonStorage result(old.variableKinds, old.kind(),
                              OctagonStorage::ScratchMatrixTag{});

        auto oldNode = [target, sign](std::size_t newNode)
        {
            if (newNode == positiveNode(target))
                return sign > 0 ? positiveNode(target) : negativeNode(target);
            if (newNode == negativeNode(target))
                return sign > 0 ? negativeNode(target) : positiveNode(target);
            return newNode;
        };
        auto delta = [target, &constant](std::size_t newNode)
        {
            if (newNode == positiveNode(target))
                return constant;
            if (newNode == negativeNode(target))
                return -constant;
            return Rational();
        };

        for (std::size_t row = 0; row < result.nodes(); ++row)
        {
            for (std::size_t column = 0; column < result.nodes(); ++column)
            {
                const Bound& oldBound = old.at(oldNode(row), oldNode(column));
                if (!oldBound.isFinite())
                    result.set(row, column, oldBound);
                else
                    result.set(row, column,
                               Bound::finite(oldBound.value() + delta(row) -
                                             delta(column),
                                             oldBound.isStrict()));
            }
        }
        result.stronglyClosed = old.stronglyClosed;
        state = std::move(result);
    }

    void enforceCoherence(OctagonStorage& state) const
    {
        for (std::size_t row = 0; row < state.nodes(); ++row)
        {
            for (std::size_t column = 0; column < state.nodes(); ++column)
            {
                const Bound coherent =
                    state.at(opposite(column), opposite(row));
                const Bound minimum =
                    Bound::min(state.at(row, column), coherent);
                state.set(row, column, minimum);
                state.set(opposite(column), opposite(row), minimum);
            }
        }
    }

    void shortestPathClosure(OctagonStorage& state) const
    {
        Bound candidate;
        for (std::size_t middle = 0; middle < state.nodes(); ++middle)
        {
            for (std::size_t row = 0; row < state.nodes(); ++row)
            {
                if (state.at(row, middle).isPlusInfinity())
                    continue;
                for (std::size_t column = 0; column < state.nodes(); ++column)
                {
                    if (state.at(middle, column).isPlusInfinity())
                        continue;
                    candidate.assignSum(state.at(row, middle),
                                        state.at(middle, column));
                    state.tighten(row, column, candidate);
                }
            }
        }
    }

    void integerTighten(OctagonStorage& state) const
    {
        if (!options_.integerTightening)
            return;
        for (Dimension dimension = 0; dimension < state.dimensions; ++dimension)
        {
            if (state.variableKinds[dimension] != NumericKind::Integer)
                continue;
            const std::size_t positive = positiveNode(dimension);
            const std::size_t negative = negativeNode(dimension);
            state.set(positive, negative,
                      tightenIntegerUnary(state.at(positive, negative)));
            state.set(negative, positive,
                      tightenIntegerUnary(state.at(negative, positive)));
        }
    }

    void strongClosure(OctagonStorage& state) const
    {
        if (!options_.strongClosure)
            return;
        std::vector<std::size_t> anchoredRows;
        std::vector<std::size_t> anchoredColumns;
        anchoredRows.reserve(state.nodes());
        anchoredColumns.reserve(state.nodes());
        for (std::size_t node = 0; node < state.nodes(); ++node)
            if (state.at(node, opposite(node)).isFinite())
            {
                anchoredRows.push_back(node);
                anchoredColumns.push_back(opposite(node));
            }
        Bound candidate;
        for (const std::size_t row : anchoredRows)
        {
            for (const std::size_t column : anchoredColumns)
            {
                const Bound& lhs = state.at(row, opposite(row));
                const Bound& rhs = state.at(opposite(column), column);
                if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
                    continue;
                candidate.assignSum(lhs, rhs).divideByTwoInPlace();
                state.tighten(row, column, candidate);
            }
        }
    }

    /// Restore canonical strong closure after a forgotten (therefore
    /// independent) variable receives only unary bounds.  The retained
    /// submatrix is already strongly closed, and its unary bounds did not
    /// change.  Consequently the full strong-closure formula can improve only
    /// rows or columns belonging to the new variable.
    void strongCloseIndependentVariable(OctagonStorage& state,
                                        Dimension dimension) const
    {
        if (state.bottom)
            return;
        const std::size_t positive = positiveNode(dimension);
        const std::size_t negative = negativeNode(dimension);
        if (options_.integerTightening &&
                state.variableKinds[dimension] == NumericKind::Integer)
        {
            state.set(positive, negative,
                      tightenIntegerUnary(state.at(positive, negative)));
            state.set(negative, positive,
                      tightenIntegerUnary(state.at(negative, positive)));
        }
        if (detectBottom(state))
            return;
        if (options_.strongClosure)
        {
            Bound candidate;
            const auto tightenStrong = [&](std::size_t row,
                                           std::size_t column)
            {
                const Bound& lhs = state.at(row, opposite(row));
                const Bound& rhs = state.at(opposite(column), column);
                if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
                    return;
                candidate.assignSum(lhs, rhs).divideByTwoInPlace();
                state.tighten(row, column, candidate);
            };
            for (std::size_t column = 0; column < state.nodes(); ++column)
            {
                tightenStrong(positive, column);
                tightenStrong(negative, column);
            }
            for (std::size_t row = 0; row < state.nodes(); ++row)
            {
                tightenStrong(row, positive);
                tightenStrong(row, negative);
            }
        }
        if (!detectBottom(state))
            state.stronglyClosed = true;
    }

    void closeAfterSingleVariableUpdate(
        OctagonStorage& state, const DimensionLayout& layout,
        Dimension dimension, bool independent) const
    {
        if (!options_.incrementalClosure)
        {
            normalize(state, layout);
            return;
        }
        if (independent)
            strongCloseIndependentVariable(state, dimension);
        else
            incrementalCloseWithIntegerTightening(state, dimension);
    }

    bool detectBottom(OctagonStorage& state) const
    {
        for (std::size_t node = 0; node < state.nodes(); ++node)
        {
            if (isNegativeDiagonal(state.at(node, node)))
            {
                state.bottom = true;
                return true;
            }
        }
        return state.bottom;
    }

    /// Restore closure after adding exact constraints that all involve one
    /// variable to an already strongly closed DBM. First repair rows and
    /// columns incident to the changed variable, then use its two signed nodes
    /// as the only new pivots. The unaffected submatrix was already closed, so
    /// the work is quadratic rather than cubic.
    void incrementalClose(OctagonStorage& state, Dimension variable) const
    {
        if (state.bottom)
            return;
        const std::vector<std::size_t> nodes = state.closureNodes(variable);
        const std::size_t first = positiveNode(variable);
        const std::size_t last = negativeNode(variable);
        Bound candidate;
        const auto relax = [&](std::size_t row, std::size_t column,
                               const Bound& lhs, const Bound& rhs)
        {
            if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
                return;
            candidate.assignSum(lhs, rhs);
            state.tighten(row, column, candidate);
        };

        for (const std::size_t pivot : nodes)
        {
            const std::size_t pairedPivot = opposite(pivot);
            for (std::size_t endpoint = first; endpoint <= last; ++endpoint)
            {
                const Bound endpointPivot = state.at(endpoint, pivot);
                const Bound endpointPaired =
                    state.at(endpoint, pairedPivot);
                for (const std::size_t column : nodes)
                {
                    relax(endpoint, column, endpointPivot,
                          state.at(pivot, column));
                    relax(endpoint, column, endpointPaired,
                          state.at(pairedPivot, column));
                }
            }
        }

        for (std::size_t pivot = first; pivot <= last; ++pivot)
        {
            const std::size_t pairedPivot = opposite(pivot);
            for (std::size_t rowIndex = 0; rowIndex < nodes.size(); ++rowIndex)
            {
                const std::size_t row = nodes[rowIndex];
                const Bound rowPivot = state.at(row, pivot);
                const Bound rowPaired = state.at(row, pairedPivot);
                // The carrier stores one coherent half-matrix cell for both
                // (row,column) and (!column,!row). Visiting only the physical
                // half avoids computing every relaxation twice.
                for (std::size_t columnIndex = 0;
                        columnIndex <= (rowIndex | 1); ++columnIndex)
                {
                    const std::size_t column = nodes[columnIndex];
                    relax(row, column, rowPivot,
                          state.at(pivot, column));
                    relax(row, column, rowPaired,
                          state.at(pairedPivot, column));
                }
            }
        }

        if (detectBottom(state))
            return;
        {
            if (options_.strongClosure)
                for (std::size_t rowIndex = 0;
                        rowIndex < nodes.size(); ++rowIndex)
                    for (std::size_t columnIndex = 0;
                            columnIndex <= (rowIndex | 1); ++columnIndex)
                    {
                        const std::size_t row = nodes[rowIndex];
                        const std::size_t column = nodes[columnIndex];
                        const Bound& lhs = state.at(row, opposite(row));
                        const Bound& rhs =
                            state.at(opposite(column), column);
                        if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
                            continue;
                        candidate.assignSum(lhs, rhs).divideByTwoInPlace();
                        state.tighten(row, column, candidate);
                    }
        }
        // Off-diagonal coherent pairs are one physical half-matrix slot. Main
        // diagonals remain zero unless detectBottom above found a negative
        // cycle, so no separate full-matrix coherence pass is needed here.
        state.stronglyClosed = true;
    }

    void incrementalCloseWithIntegerTightening(OctagonStorage& state,
            Dimension variable) const
    {
        incrementalClose(state, variable);
        if (state.bottom)
            return;
        bool stabilized = true;
        if (options_.integerTightening)
        {
            const std::vector<std::size_t> affectedNodes =
                state.closureNodes(variable);
            std::vector<Dimension> affectedDimensions;
            affectedDimensions.reserve(affectedNodes.size() / 2);
            for (const std::size_t node : affectedNodes)
                affectedDimensions.push_back(node / 2);
            std::sort(affectedDimensions.begin(), affectedDimensions.end());
            affectedDimensions.erase(
                std::unique(affectedDimensions.begin(),
                            affectedDimensions.end()),
                affectedDimensions.end());

            // Tightening a unary integer bound is another exact update
            // incident to that dimension. Repair it incrementally before
            // examining the remaining dimensions; repeat because a later
            // repair can expose a tighter odd bound on an earlier dimension.
            stabilized = false;
            const std::size_t limit = affectedDimensions.size() + 1;
            for (std::size_t pass = 0; pass < limit; ++pass)
            {
                bool changed = false;
                for (const Dimension dimension : affectedDimensions)
                {
                    if (state.variableKinds[dimension] != NumericKind::Integer)
                        continue;
                    const std::size_t positive = positiveNode(dimension);
                    const std::size_t negative = negativeNode(dimension);
                    const Bound upper =
                        tightenIntegerUnary(state.at(positive, negative));
                    const Bound lower =
                        tightenIntegerUnary(state.at(negative, positive));
                    if (!(upper < state.at(positive, negative)) &&
                            !(lower < state.at(negative, positive)))
                        continue;
                    state.set(positive, negative, upper);
                    state.set(negative, positive, lower);
                    state.stronglyClosed = false;
                    incrementalClose(state, dimension);
                    if (state.bottom)
                        return;
                    changed = true;
                }
                if (!changed)
                {
                    stabilized = true;
                    break;
                }
            }
        }
        if (!stabilized)
        {
            // Defensive fallback for an unusual strict-bound case.
            state.stronglyClosed = false;
            normalize(state);
            return;
        }

        // A previously unanchored component can receive its first unary bound
        // in the update above. The local shortest-path repair is complete, but
        // canonical strong closure must also materialize implications between
        // unary-anchored components. This is quadratic, not Floyd-Warshall.
        strongClosure(state);
        if (!detectBottom(state))
            state.stronglyClosed = true;
    }

    void normalize(OctagonStorage& state) const
    {
        if (state.bottom || state.stronglyClosed)
        {
            return;
        }

        // Every carrier maps coherent off-diagonal entries to one physical
        // slot. Main diagonals start at zero and any negative diagonal makes
        // the state bottom below. A full logical coherence sweep is therefore
        // redundant and particularly costly for a partitioned carrier.
        shortestPathClosure(state);
        if (detectBottom(state))
            return;
        integerTighten(state);
        shortestPathClosure(state);
        if (detectBottom(state))
            return;
        strongClosure(state);
        if (detectBottom(state))
            return;
        state.stronglyClosed = true;
    }

    void normalize(OctagonStorage& state, const DimensionLayout& layout) const
    {
        (void)layout;
        normalize(state);
    }

    const OctagonStorage& normalized(
        const OctagonStorage& state,
        std::optional<OctagonStorage>& normalizedStorage) const
    {
        if (state.bottom || state.stronglyClosed)
            return state;
        normalizedStorage.emplace(
            std::move(*state.clone(options_.componentCopyOnWrite)));
        normalize(*normalizedStorage);
        return *normalizedStorage;
    }

    void addNodeTerm(LinearExpression& expression,
                     const DimensionLayout& layout, std::size_t node,
                     const Rational& multiplier) const
    {
        const Dimension dimension = node / 2;
        const Rational sign = node % 2 == 0 ? Rational(1) : Rational(-1);
        const Variable variable = layout.variableOf(dimension);
        expression.setCoefficient(variable, expression.coefficient(variable) +
                                  sign * multiplier);
    }

    OctagonConfig options_;
    OctagonStorage state_;
};

OctagonDomain::OctagonDomain(DimensionLayout layout, OctagonConfig config,
                             bool bottom)
    : layout_(std::move(layout)),
      impl_(std::make_unique<Impl>(layout_, std::move(config), bottom))
{
}

OctagonDomain::OctagonDomain(DimensionLayout layout, std::unique_ptr<Impl> impl)
    : layout_(std::move(layout)), impl_(std::move(impl))
{
}

OctagonDomain OctagonDomain::top(const DimensionLayout& layout,
                                 const OctagonConfig& config)
{
    return OctagonDomain(layout, config, false);
}

OctagonDomain OctagonDomain::bottom(const DimensionLayout& layout,
                                    const OctagonConfig& config)
{
    return OctagonDomain(layout, config, true);
}

OctagonDomain OctagonDomain::fromConstraints(
    const DimensionLayout& layout,
    const LinearConstraintSet& constraints,
    const OctagonConfig& config)
{
    OctagonDomain state = top(layout, config);
    state.assumeAll(constraints);
    return state;
}

OctagonDomain::OctagonDomain(const OctagonDomain& other)
    : NumericalDomain(other), layout_(other.layout_),
      impl_(std::make_unique<Impl>(*other.impl_))
{
}

OctagonDomain::OctagonDomain(OctagonDomain&& other) noexcept = default;

OctagonDomain& OctagonDomain::operator=(const OctagonDomain& other)
{
    if (this == &other)
        return *this;
    NumericalDomain::operator=(other);
    layout_ = other.layout_;
    impl_ = std::make_unique<Impl>(*other.impl_);
    return *this;
}

OctagonDomain& OctagonDomain::operator=(OctagonDomain&& other) noexcept = default;

OctagonDomain::~OctagonDomain() = default;

std::unique_ptr<AbstractDomain> OctagonDomain::clone() const
{
    return std::make_unique<OctagonDomain>(*this);
}

const char* OctagonDomain::name() const
{
    return impl_->name();
}

void OctagonDomain::report(OperationKind operation,
                           ApproximationKind approximation,
                           std::string reason, bool best) const
{
    recordOperation(operation, approximation, best, reason);
    DiagnosticSink* sink = diagnosticSink();
    if (sink && approximation != ApproximationKind::Exact)
        sink->report({operation, approximation, std::move(reason)});
}

void OctagonDomain::assign(Variable target,
                           const LinearExpression& expression)
{
    ensureVariables({target});
    ensureExpression(expression);

    const ApproximationKind approximation = assignState(target, expression);
    report(OperationKind::Assignment, approximation,
           approximation == ApproximationKind::UnsupportedFallback
           ? std::string(name()) +
           " forgot a target assigned an unsupported linear expression"
           : std::string(name()) +
           " approximated a linear assignment",
           approximation == ApproximationKind::Exact);
}

void OctagonDomain::assign(Variable target, const TreeExpression& expression)
{
    if (const auto linear = expression.asLinear())
    {
        assign(target, *linear);
        return;
    }
    const Interval value = evaluateTreeExpression(expression);
    assignInterval(target, value);
    report(OperationKind::Assignment,
           ApproximationKind::SoundOverApproximation,
           std::string(name()) +
           " interval-linearized a nonlinear or finite IEEE assignment",
           false);
}

void OctagonDomain::substitute(Variable target,
                               const LinearExpression& expression)
{
    substituteParallel({{target, expression}});
}

void OctagonDomain::substituteParallel(
    const LinearAssignmentList& assignments)
{
    std::vector<Variable> variables;
    std::set<Variable> targets;
    for (const auto& assignment : assignments)
    {
        if (!targets.insert(assignment.target).second)
        {
            assert(false && "duplicate parallel target");
            std::abort();
        }
        variables.push_back(assignment.target);
        for (const auto& term : assignment.expression.terms())
            variables.push_back(term.first);
    }
    ensureVariables(variables);

    std::map<Variable, LinearExpression> replacements;
    for (const LinearAssignment& assignment : assignments)
    {
        if (!layout_.contains(assignment.target))
        {
            assert(false && "substitution target is not in layout");
            std::abort();
        }
        if (!replacements.emplace(assignment.target, assignment.expression)
                .second)
        {
            assert(false && "parallel substitution contains a duplicate target");
            std::abort();
        }
        for (const auto& [variable, coefficient] :
                assignment.expression.terms())
        {
            (void)coefficient;
            if (!layout_.contains(variable))
            {
                assert(false && "substitution expression uses an unknown variable");
                std::abort();
            }
        }
    }
    recordOperation(OperationKind::Substitution, ApproximationKind::Exact,
                    true);
    if (assignments.empty() || isBottom())
        return;

    LinearConstraintSet preimage;
    for (const LinearConstraint& constraint : toConstraints())
        preimage.emplace_back(
            constraint.expression().substituted(replacements),
            constraint.kind());
    *this = fromConstraints(layout_, preimage, config());
    recordOperation(OperationKind::Substitution, ApproximationKind::SoundOverApproximation, false);
}

void OctagonDomain::assume(const LinearConstraint& constraint)
{
    ensureExpression(constraint.expression());

    const ApproximationKind approximation = assumeState(constraint);
    report(OperationKind::Assumption, approximation,
           std::string(name()) +
           " ignored or approximated an unsupported constraint",
           approximation == ApproximationKind::Exact);
}

void OctagonDomain::assumeAll(const LinearConstraintSet& constraints)
{
    std::vector<Variable> variables;
    for (const auto& constraint : constraints)
        for (const auto& term : constraint.expression().terms())
            variables.push_back(term.first);
    ensureVariables(variables);

    if (constraints.empty())
    {
        recordOperation(OperationKind::Assumption, ApproximationKind::Exact,
                        true);
        return;
    }
    if (constraints.size() == 1)
    {
        assume(constraints.front());
        return;
    }

    // Exact octagonal rows can be inserted into one dirty DBM and closed once.
    // Closing after every row is semantically redundant and turns bulk
    // construction into constraints-times-cubic work. Constraints requiring
    // interval linearization still iterate to the same fixed point as the
    // generic implementation, while exact rows within a pass share closure.
    const std::size_t limit =
        constraints.size() * (layout_.size() + 1) + 1;
    for (std::size_t pass = 0; pass < limit; ++pass)
    {
        const std::unique_ptr<AbstractDomain> before = clone();
        const ApproximationKind approximation =
            impl_->assumeAllCurrent(layout_, constraints);
        report(OperationKind::Assumption, approximation,
               std::string(name()) +
               " ignored or approximated an unsupported constraint",
               approximation == ApproximationKind::Exact);
        if (approximation == ApproximationKind::Exact || isBottom() ||
                isEquivalentTo(*before) == CheckResult::True)
            return;
    }
}

void OctagonDomain::assume(const TreeConstraint& constraint)
{
    if (const auto linear = constraint.expression().asLinear())
    {
        assume(LinearConstraint(*linear, constraint.kind()));
        return;
    }
    const LinearConstraintSet consequences =
        treeConstraintConsequences(constraint);
    assumeAll(consequences);
    report(OperationKind::Assumption,
           ApproximationKind::SoundOverApproximation,
           consequences.empty()
           ? std::string(name()) +
           " found no affine consequence for a nonlinear or finite IEEE guard"
           : std::string(name()) +
           " reduced a nonlinear guard to sound affine consequences",
           false);
}

void OctagonDomain::forget(Variable variable)
{
    if (!layout_.contains(variable))
        return;

    forgetState(variable);
    recordOperation(OperationKind::Forget, ApproximationKind::Exact, true);
}

void OctagonDomain::assignInterval(Variable target, const Interval& value)
{
    ensureVariables({target});

    impl_->assignIntervalCurrent(layout_, target, value);
}

void OctagonDomain::projectLowerBounds()
{
    projectLowerBoundsState();
}

void OctagonDomain::changeLayout(const DimensionLayout& layout)
{
    if (layout_ == layout)
    {
        recordOperation(OperationKind::Canonicalization,
                        ApproximationKind::Exact, true);
        return;
    }
    const DimensionLayout oldLayout = layout_;
    changeLayoutState(oldLayout, layout);
    layout_ = layout;
    recordOperation(OperationKind::Canonicalization,
                    ApproximationKind::Exact, true);
}

void OctagonDomain::expandDimensions(
    Variable source, const std::vector<DimensionEntry>& copies)
{
    if (!layout_.contains(source))
    {
        assert(false && "expanded variable is not in layout");
        std::abort();
    }
    std::set<Variable> seen;
    for (const DimensionEntry& copy : copies)
    {
        if (layout_.contains(copy.variable) ||
                !seen.insert(copy.variable).second)
        {
            assert(false && "expanded variables must be new and unique");
            std::abort();
        }
        if (copy.variable.type() != layout_.typeOf(source))
        {
            assert(false && "expanded variables must have the source numeric type");
            std::abort();
        }
    }
    if (copies.empty())
    {
        recordOperation(OperationKind::Expand, ApproximationKind::Exact, true);
        return;
    }
    const Interval sourceValue = bound(source);
    const LinearConstraintSet original = toConstraints();
    const bool relational =
        layout_.typeOf(source).kind != NumericKind::IEEEFloat;
    changeLayout(layout_.add(copies));
    for (const DimensionEntry& copy : copies)
    {
        if (relational)
        {
            std::map<Variable, LinearExpression> replacement
            {
                {source, LinearExpression(copy.variable)}};
            LinearConstraintSet duplicated;
            duplicated.reserve(original.size());
            for (const LinearConstraint& constraint : original)
            {
                duplicated.emplace_back(
                    constraint.expression().substituted(replacement),
                    constraint.kind());
            }
            assumeAll(duplicated);
        }
        else
            assignInterval(copy.variable, sourceValue);
    }
    recordOperation(OperationKind::Expand,
                    relational ? ApproximationKind::Exact
                    : ApproximationKind::SoundOverApproximation,
                    relational,
                    relational ? "" : "IEEE expand retained finite bounds");
}

void OctagonDomain::fold(Variable target,
                         const std::vector<Variable>& folded)
{
    std::vector<Variable> variables = folded;
    variables.push_back(target);
    ensureVariables(variables);

    if (!layout_.contains(target))
    {
        assert(false && "fold target is not in layout");
        std::abort();
    }
    std::set<Variable> seen;
    std::vector<Variable> sources{target};
    for (Variable variable : folded)
    {
        if (variable == target || !layout_.contains(variable) ||
                !seen.insert(variable).second)
        {
            assert(false && "folded variables must be distinct non-target dimensions");
            std::abort();
        }
        if (layout_.typeOf(variable) != layout_.typeOf(target))
        {
            assert(false && "folded variables must have the target numeric type");
            std::abort();
        }
        sources.push_back(variable);
    }
    if (folded.empty())
    {
        recordOperation(OperationKind::Fold, ApproximationKind::Exact, true);
        return;
    }
    const bool relational =
        layout_.typeOf(target).kind != NumericKind::IEEEFloat;
    OctagonDomain result = bottom(layout_, config());
    for (Variable source : sources)
    {
        OctagonDomain branch = *this;
        if (source != target)
        {
            if (relational)
                branch.assign(target, LinearExpression(source));
            else
                branch.assignInterval(target, bound(source));
        }
        result = result.join(branch);
    }
    result.changeLayout(layout_.remove(folded));
    *this = std::move(result);
    recordOperation(OperationKind::Fold,
                    relational ? ApproximationKind::Exact
                    : ApproximationKind::SoundOverApproximation,
                    relational,
                    relational ? "" : "IEEE fold retained finite hull bounds");
}

CheckResult OctagonDomain::entails(const LinearConstraint& constraint) const
{
    if (constraint.kind() == ConstraintKind::Equal)
    {
        const CheckResult le = entails(LinearConstraint(
                                           constraint.expression(), ConstraintKind::LessEqual));
        const CheckResult ge = entails(LinearConstraint(
                                           constraint.expression(), ConstraintKind::GreaterEqual));
        if (le == CheckResult::True && ge == CheckResult::True)
            return CheckResult::True;
        if (le == CheckResult::False || ge == CheckResult::False)
            return CheckResult::False;
        return CheckResult::Unknown;
    }

    if (constraint.kind() == ConstraintKind::NotEqual)
    {
        OctagonDomain witness(*this);
        witness.assume(LinearConstraint(constraint.expression(),
                                        ConstraintKind::Equal));
        return witness.isBottom() ? CheckResult::True : CheckResult::False;
    }

    ConstraintKind negated;
    switch (constraint.kind())
    {
    case ConstraintKind::LessThan:
        negated = ConstraintKind::GreaterEqual;
        break;
    case ConstraintKind::LessEqual:
        negated = ConstraintKind::GreaterThan;
        break;
    case ConstraintKind::GreaterThan:
        negated = ConstraintKind::LessEqual;
        break;
    case ConstraintKind::GreaterEqual:
        negated = ConstraintKind::LessThan;
        break;
    case ConstraintKind::Equal:
    case ConstraintKind::NotEqual:
        assert(false && "equality entailment was not normalized");
        std::abort();
    }
    OctagonDomain counterexample(*this);
    counterexample.assume(
        LinearConstraint(constraint.expression(), negated));
    return counterexample.isBottom() ? CheckResult::True : CheckResult::False;
}

Interval OctagonDomain::bound(Variable variable) const
{
    if (isBottom())
        return Interval::bottom();
    if (!layout_.contains(variable))
        return Interval::top();

    return boundState(variable);
}

Interval OctagonDomain::bound(const LinearExpression& expression) const
{
    if (isBottom())
        return Interval::bottom();
    for (const auto& term : expression.terms())
        if (!layout_.contains(term.first))
            return Interval::top();

    return impl_->boundExpressionCurrent(layout_, expression);
}

LinearConstraintSet OctagonDomain::toConstraints() const
{
    return constraintsState();
}

void OctagonDomain::close()
{
    recordOperation(OperationKind::TopologicalClosure,
                    ApproximationKind::Exact, true);
    if (isBottom())
        return;
    LinearConstraintSet closed;
    for (const LinearConstraint& constraint : toConstraints())
    {
        ConstraintKind kind = constraint.kind();
        if (kind == ConstraintKind::LessThan)
            kind = ConstraintKind::LessEqual;
        else if (kind == ConstraintKind::GreaterThan)
            kind = ConstraintKind::GreaterEqual;
        closed.emplace_back(constraint.expression(), kind);
    }
    *this = fromConstraints(layout_, closed, config());
    recordOperation(OperationKind::TopologicalClosure, ApproximationKind::Exact, true);
}

void OctagonDomain::canonicalize()
{
    impl_->canonicalizeCurrent();
    recordOperation(OperationKind::Canonicalization,
                    ApproximationKind::Exact, true);
}

const OctagonConfig& OctagonDomain::config() const
{
    return impl_->config();
}

OctagonStorageStats OctagonDomain::storageStats() const
{
    return impl_->storageStats();
}

OctagonDomain OctagonDomain::reconfigured(const OctagonConfig& config) const
{
    OctagonDomain result(*this);
    result.impl_->reconfigure(config);
    return result;
}

OctagonDomain OctagonDomain::join(const OctagonDomain& other) const
{
    requireCompatible(other);
    if (layout_ != other.layout_)
    {
        const auto merged = layout_.merge(other.layout_);
        auto left = *this;
        auto right = other;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.join(right);
    }

    OctagonDomain result(layout(), impl_->joined(*other.impl_));
    result.recordOperation(OperationKind::Join, ApproximationKind::Exact,
                           true);
    return result;
}

OctagonDomain OctagonDomain::meet(const OctagonDomain& other) const
{
    requireCompatible(other);
    if (layout_ != other.layout_)
    {
        const auto merged = layout_.merge(other.layout_);
        auto left = *this;
        auto right = other;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.meet(right);
    }

    OctagonDomain result(layout(), impl_->met(*other.impl_));
    result.recordOperation(OperationKind::Meet, ApproximationKind::Exact,
                           true);
    return result;
}

OctagonDomain OctagonDomain::widen(
    const OctagonDomain& next, const WideningPolicy& policy) const
{
    requireCompatible(next);
    if (layout_ != next.layout_)
    {
        const auto merged = layout_.merge(next.layout_);
        auto left = *this;
        auto right = next;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.widen(right, policy);
    }

    OctagonDomain result(layout(), impl_->widened(*next.impl_, policy));
    for (const LinearConstraint& threshold : policy.linearThresholds)
    {
        if (entails(threshold) == CheckResult::True &&
                next.entails(threshold) == CheckResult::True)
            result.assume(threshold);
    }
    result.recordOperation(OperationKind::Widening,
                           ApproximationKind::SoundOverApproximation, true);
    return result;
}

OctagonDomain OctagonDomain::narrow(const OctagonDomain& next) const
{
    requireCompatible(next);
    if (layout_ != next.layout_)
    {
        const auto merged = layout_.merge(next.layout_);
        auto left = *this;
        auto right = next;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.narrow(right);
    }

    if (!next.leqDomain(*this))
    {
        assert(false && "narrowing requires next to be included in current");
        std::abort();
    }
    OctagonDomain result(layout(), impl_->narrowed(*next.impl_));
    result.recordOperation(OperationKind::Narrowing,
                           ApproximationKind::Exact, true);
    return result;
}

OctagonDomain OctagonDomain::projectedLowerBounds() const
{
    return OctagonDomain(layout(), impl_->projectedLowerBounds());
}

DiagnosticSink* OctagonDomain::diagnosticSink() const
{
    return impl_->config().diagnostics.get();
}

const OctagonDomain& OctagonDomain::requireOctagon(
    const AbstractDomain& other) const
{
    const auto* octagon = other.isDomain<OctagonDomain>()
                          ? &static_cast<const OctagonDomain&>(other)
                          : nullptr;
    if (!octagon)
    {
        assert(false && "relational state is not an OctagonDomain");
        std::abort();
    }
    return *octagon;
}

bool OctagonDomain::hasCompatibleDomain(const AbstractDomain& other) const
{
    const auto* octagon = other.isDomain<OctagonDomain>()
                          ? &static_cast<const OctagonDomain&>(other)
                          : nullptr;
    return octagon &&
           config().operationCompatible(octagon->config());
}

ApproximationKind OctagonDomain::assignState(
    Variable target, const LinearExpression& expression)
{
    return impl_->assignCurrent(layout(), target, expression);
}

ApproximationKind OctagonDomain::assumeState(
    const LinearConstraint& constraint)
{
    return impl_->assumeCurrent(layout(), constraint);
}

void OctagonDomain::forgetState(Variable variable)
{
    impl_->forgetCurrent(layout(), variable);
}

void OctagonDomain::joinDomain(const AbstractDomain& other)
{
    const auto& alignedOperand = requireOctagon(other);
    if (layout_ != alignedOperand.layout_)
    {
        *this = join(alignedOperand);
        return;
    }

    impl_->joinCurrent(*requireOctagon(other).impl_);
}

void OctagonDomain::meetDomain(const AbstractDomain& other)
{
    const auto& alignedOperand = requireOctagon(other);
    if (layout_ != alignedOperand.layout_)
    {
        *this = meet(alignedOperand);
        return;
    }

    impl_->meetCurrent(*requireOctagon(other).impl_);
}

void OctagonDomain::widenDomain(const AbstractDomain& next)
{
    const auto& alignedOperand = requireOctagon(next);
    if (layout_ != alignedOperand.layout_)
    {
        *this = widen(alignedOperand);
        return;
    }

    impl_->widenCurrent(*requireOctagon(next).impl_, {});
}

void OctagonDomain::narrowDomain(const AbstractDomain& next)
{
    const auto& alignedOperand = requireOctagon(next);
    if (layout_ != alignedOperand.layout_)
    {
        *this = narrow(alignedOperand);
        return;
    }

    impl_->narrowCurrent(*requireOctagon(next).impl_);
}

void OctagonDomain::projectLowerBoundsState()
{
    impl_->projectLowerBoundsCurrent();
}

void OctagonDomain::changeLayoutState(
    const DimensionLayout& oldLayout, const DimensionLayout& newLayout)
{
    impl_->changeLayoutCurrent(oldLayout, newLayout);
}

bool OctagonDomain::isBottomDomain() const
{
    return impl_->isBottomCurrent();
}

bool OctagonDomain::isTopDomain() const
{
    return impl_->isTopCurrent();
}

bool OctagonDomain::leqDomain(const AbstractDomain& other) const
{
    const auto& rhs = requireOctagon(other);
    requireCompatible(rhs);
    if (layout_ != rhs.layout_)
    {
        const auto merged = layout_.merge(rhs.layout_);
        auto left = *this;
        auto right = rhs;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.leqDomain(right);
    }

    return impl_->leqCurrent(*requireOctagon(other).impl_);
}

Interval OctagonDomain::boundState(Variable variable) const
{
    return impl_->boundCurrent(layout(), variable);
}

LinearConstraintSet OctagonDomain::constraintsState() const
{
    return impl_->constraintsCurrent(layout());
}

std::string OctagonDomain::domainToString() const
{
    return impl_->toStringCurrent(layout());
}

OctagonDomain OctagonDomain::top(const OctagonConfig& config)
{
    return top(DimensionLayout(), config);
}

OctagonDomain OctagonDomain::bottom(const OctagonConfig& config)
{
    return bottom(DimensionLayout(), config);
}

OctagonDomain OctagonDomain::fromConstraints(const LinearConstraintSet& constraints, const OctagonConfig& config)
{
    auto result = top(config);
    result.assumeAll(constraints);
    return result;
}

OctagonDomain OctagonDomain::fromBox(const BoxDomain& box, const OctagonConfig& config)
{
    return box.isBottom() ? bottom(config) : fromConstraints(box.toConstraints(), config);
}

BoxDomain OctagonDomain::toBox() const
{
    BoxSemanticConfig options;
    options.integerTightening = config().integerTightening;
    auto result = isBottom() ? BoxDomain::bottom(options) : BoxDomain::top(options);
    if (!isBottom())
        for (const auto& entry : layout_.variables())
        {
            const auto value = bound(entry.variable);
            const LinearExpression variable(entry.variable);
            if (value.lower().isFinite())
                result.assume(LinearConstraint(
                                  variable - LinearExpression(value.lower().value()),
                                  value.lower().isStrict() ? ConstraintKind::GreaterThan : ConstraintKind::GreaterEqual));
            if (value.upper().isFinite())
                result.assume(LinearConstraint(
                                  variable - LinearExpression(value.upper().value()),
                                  value.upper().isStrict() ? ConstraintKind::LessThan : ConstraintKind::LessEqual));
        }
    return result;
}

std::vector<Variable> OctagonDomain::supportVariables() const
{
    std::vector<Variable> result;
    result.reserve(layout_.size());
    for (const auto& entry : layout_.variables())
        result.push_back(entry.variable);
    return result;
}

std::vector<Variable> OctagonDomain::relationalClosureState(
    const std::vector<Variable>& seeds) const
{
    recordRelationalClosureIndexedQuery();
    return impl_->relationalClosureCurrent(layout(), seeds);
}

void OctagonDomain::ensureVariables(const std::vector<Variable>& variables)
{
    std::set<Variable> missing;
    for (Variable variable : variables)
        if (!layout_.contains(variable))
            missing.insert(variable);
    if (missing.empty())
        return;
    std::vector<DimensionEntry> added;
    for (Variable variable : missing)
        added.emplace_back(variable);
    changeLayout(layout_.add(added));
}

void OctagonDomain::ensureExpression(const LinearExpression& expression)
{
    std::vector<Variable> variables;
    for (const auto& term : expression.terms())
        variables.push_back(term.first);
    ensureVariables(variables);
}

void OctagonDomain::project(const std::vector<Variable>& retained)
{
    std::set<Variable> keep(retained.begin(), retained.end());
    std::vector<Variable> removed;
    for (const auto& entry : layout_.variables())
        if (!keep.count(entry.variable))
            removed.push_back(entry.variable);
    changeLayout(layout_.remove(removed));
    recordOperation(OperationKind::Forget, ApproximationKind::Exact, true);
}

void OctagonDomain::expand(Variable source, const std::vector<Variable>& copies)
{
    std::set<Variable> seen;
    for (Variable variable : copies)
        if (variable == source || variable.type() != source.type() || !seen.insert(variable).second)
        {
            assert(false && "expand requires distinct, same-type copies");
            std::abort();
        }
    // Copies are overwritten, just as in Box. A coordinate retained after a
    // previous forget must not change the meaning of a fresh expansion.
    changeLayout(layout_.remove(copies));
    ensureVariables({source});
    std::vector<DimensionEntry> added;
    for (Variable variable : copies)
        added.emplace_back(variable);
    expandDimensions(source, added);
}

void OctagonDomain::assignParallel(const LinearAssignmentList& assignments)
{
    std::vector<Variable> variables;
    std::set<Variable> targets;
    for (const auto& assignment : assignments)
    {
        if (!targets.insert(assignment.target).second)
        {
            assert(false && "duplicate parallel target");
            std::abort();
        }
        variables.push_back(assignment.target);
        for (const auto& term : assignment.expression.terms())
            variables.push_back(term.first);
    }
    ensureVariables(variables);

    if (assignments.empty() || isBottom())
        return;
    const auto original = layout_;
    std::set<Variable> occupied;
    for (const auto& entry : layout_.variables())
        occupied.insert(entry.variable);
    std::vector<Variable> temporaries;
    for (const auto& assignment : assignments)
    {
        std::uint64_t id = 0;
        while (id <= std::numeric_limits<std::uint32_t>::max() &&
                occupied.count(Variable(static_cast<std::uint32_t>(id), assignment.target.type())))
            ++id;
        if (id > std::numeric_limits<std::uint32_t>::max())
        {
            assert(false && "no temporary variable available");
            std::abort();
        }
        const Variable temporary(static_cast<std::uint32_t>(id), assignment.target.type());
        temporaries.push_back(temporary);
        occupied.insert(temporary);
    }
    ensureVariables(temporaries);
    for (std::size_t i = 0; i < assignments.size(); ++i)
        assign(temporaries[i], assignments[i].expression);
    for (std::size_t i = 0; i < assignments.size(); ++i)
        assign(assignments[i].target, LinearExpression(temporaries[i]));
    changeLayout(original);
    recordOperation(OperationKind::Assignment, ApproximationKind::SoundOverApproximation, false,
                    "simultaneous octagonal abstraction through fresh coordinates");
}

} // namespace SVF
