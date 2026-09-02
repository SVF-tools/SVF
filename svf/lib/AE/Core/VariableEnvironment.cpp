//===- VariableEnvironment.cpp -- Variables and dimensions --------------===//

#include "AE/Core/VariableEnvironment.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace SVF::AbstractDomain
{

struct VariableEnvironment::Data
{
    explicit Data(std::vector<VariableDeclaration> declarations)
        : variables(std::move(declarations))
    {
        std::sort(variables.begin(), variables.end(),
                  [](const VariableDeclaration& lhs,
                     const VariableDeclaration& rhs)
                  { return lhs.variable < rhs.variable; });
        for (Dimension dimension = 1; dimension < variables.size(); ++dimension)
        {
            if (variables[dimension - 1].variable ==
                variables[dimension].variable)
                throw std::invalid_argument(
                    "duplicate variable in relational environment");
        }
    }

    std::vector<VariableDeclaration> variables;
};

VariableEnvironment::VariableEnvironment() : data_(std::make_shared<Data>(
                                      std::vector<VariableDeclaration>{}))
{
}
VariableEnvironment::VariableEnvironment(std::vector<VariableDeclaration> variables)
    : data_(std::make_shared<Data>(std::move(variables)))
{
}

std::size_t VariableEnvironment::size() const
{
    return data_->variables.size();
}

bool VariableEnvironment::contains(Variable variable) const
{
    const auto iterator = std::lower_bound(
        data_->variables.begin(), data_->variables.end(), variable,
        [](const VariableDeclaration& declaration, Variable candidate)
        { return declaration.variable < candidate; });
    return iterator != data_->variables.end() &&
           iterator->variable == variable;
}

Dimension VariableEnvironment::dimensionOf(Variable variable) const
{
    const auto iterator = std::lower_bound(
        data_->variables.begin(), data_->variables.end(), variable,
        [](const VariableDeclaration& declaration, Variable candidate)
        { return declaration.variable < candidate; });
    if (iterator == data_->variables.end() || iterator->variable != variable)
        throw std::out_of_range("variable is not in relational environment");
    return static_cast<Dimension>(iterator - data_->variables.begin());
}

Variable VariableEnvironment::variableOf(Dimension dimension) const
{
    if (dimension >= size())
        throw std::out_of_range("invalid relational dimension");
    return data_->variables[dimension].variable;
}

const NumericType& VariableEnvironment::typeOf(Variable variable) const
{
    return data_->variables[dimensionOf(variable)].type;
}

const std::string& VariableEnvironment::nameOf(Variable variable) const
{
    return data_->variables[dimensionOf(variable)].name;
}

const std::vector<VariableDeclaration>& VariableEnvironment::variables() const
{
    return data_->variables;
}

VariableEnvironment VariableEnvironment::add(
    std::vector<VariableDeclaration> declarations) const
{
    std::vector<VariableDeclaration> combined = data_->variables;
    combined.insert(combined.end(),
                    std::make_move_iterator(declarations.begin()),
                    std::make_move_iterator(declarations.end()));
    return VariableEnvironment(std::move(combined));
}

VariableEnvironment VariableEnvironment::remove(const std::vector<Variable>& removed) const
{
    std::vector<VariableDeclaration> remaining;
    remaining.reserve(size());
    for (const VariableDeclaration& declaration : data_->variables)
    {
        if (std::find(removed.begin(), removed.end(), declaration.variable) ==
                removed.end())
            remaining.push_back(declaration);
    }
    return VariableEnvironment(std::move(remaining));
}

VariableEnvironment VariableEnvironment::merge(const VariableEnvironment& other) const
{
    std::map<Variable, VariableDeclaration> combined;
    for (const VariableDeclaration& declaration : data_->variables)
        combined.emplace(declaration.variable, declaration);
    for (const VariableDeclaration& declaration : other.data_->variables)
    {
        const auto [it, inserted] =
            combined.emplace(declaration.variable, declaration);
        if (!inserted && it->second.type != declaration.type)
            throw std::invalid_argument(
                "cannot merge relational environments with conflicting types");
    }

    std::vector<VariableDeclaration> declarations;
    declarations.reserve(combined.size());
    for (auto& entry : combined)
        declarations.push_back(std::move(entry.second));
    return VariableEnvironment(std::move(declarations));
}

bool operator==(const VariableEnvironment& lhs,
                                    const VariableEnvironment& rhs)
{
    if (lhs.data_ == rhs.data_)
        return true;
    if (lhs.data_->variables.size() != rhs.data_->variables.size())
        return false;
    for (std::size_t index = 0; index < lhs.data_->variables.size(); ++index)
    {
        const VariableDeclaration& left = lhs.data_->variables[index];
        const VariableDeclaration& right = rhs.data_->variables[index];
        if (left.variable != right.variable || left.type != right.type ||
                left.name != right.name)
            return false;
    }
    return true;
}

} // namespace SVF::AbstractDomain
