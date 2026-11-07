//===- SVFIRAdapter.cpp -- SVFIR to abstract-domain symbols ------------===//
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

#include <cassert>
#include <cstdlib>
#include "AE/Svfexe/SVFIRAdapter.h"

#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFType.h"
#include "SVFIR/SVFVariables.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"

#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace SVF
{

using SVF::Location;
using SVF::MemoryLayout;
using SVF::Variable;

namespace
{

SVF::NumericType numericType(const SVFType* type, bool pointer)
{
    if (pointer)
        return SVF::NumericType::integer();

    if (!type || type->getKind() == SVFType::SVFIntegerTy)
        return SVF::NumericType::integer();
    if (type->getKind() != SVFType::SVFOtherTy)
        return SVF::NumericType::real();

    const std::string representation = type->toString();
    using SVF::FloatFormat;
    if (representation == "half")
        return SVF::NumericType::ieee(FloatFormat{5, 11});
    if (representation == "bfloat")
        return SVF::NumericType::ieee(FloatFormat{8, 8});
    if (representation == "float")
        return SVF::NumericType::ieee(FloatFormat::binary32());
    if (representation == "double")
        return SVF::NumericType::ieee(FloatFormat::binary64());
    if (representation == "x86_fp80")
        return SVF::NumericType::ieee(FloatFormat{15, 64});
    if (representation == "fp128")
        return SVF::NumericType::ieee(FloatFormat{15, 113});
    // Other scalar types have no integer semantics known to this adapter.
    // Real is conservative and, unlike Integer, cannot tighten a fractional
    // value to the empty set.
    return SVF::NumericType::real();
}

SVF::NumericType numericType(const SVFVar& variable)
{
    return numericType(variable.getType(), variable.isPointer());
}

SVF::NumericType contentNumericType(const ObjVar& object)
{
    const auto* gep = SVFUtil::dyn_cast<GepObjVar>(&object);
    if (!gep)
        return numericType(object);

    // GepObjVar::getType() asserts for overflow/summary fields and opaque
    // aggregates. Inspect the flattened layout with an explicit bounds check
    // so ordinary fields keep their precise type while such implementation
    // objects conservatively use Real.
    const SVFType* baseType = gep->getBaseObj()->getType();
    if (!baseType)
        return SVF::NumericType::real();
    const StInfo* typeInfo = baseType->getTypeInfo();
    const auto& elementTypes =
        Options::ModelArrays() ? typeInfo->getFlattenElementTypes()
        : typeInfo->getFlattenFieldTypes();
    const auto offset = static_cast<std::size_t>(gep->getConstantFieldIdx());
    if (offset >= elementTypes.size())
        return SVF::NumericType::real();
    const SVFType* elementType = elementTypes[offset];
    return numericType(elementType,
                       elementType && elementType->isPointerTy());
}

Variable nextVariable(std::uint64_t& next,
                      SVF::NumericType type)
{
    if (next > std::numeric_limits<std::uint32_t>::max())
    {
        assert(false && "too many abstract-domain variables");
        std::abort();
    }
    return Variable(static_cast<std::uint32_t>(next++), type);
}

Location nextLocation(std::uint64_t& next)
{
    if (next > std::numeric_limits<std::uint32_t>::max())
    {
        assert(false && "too many abstract-domain locations");
        std::abort();
    }
    return Location(static_cast<std::uint32_t>(next++));
}

} // namespace

SVF::Interval SVFIRAdapter::floatingConstant(double value)
{
    if (!std::isfinite(value))
        return SVF::Interval::top();
    return SVF::Interval::singleton(
               SVF::Rational(static_cast<s64_t>(value)));
}

SVFIRAdapter::SVFIRAdapter(const SVFIR& svfir)
{
    auto addScalars = [&](bool pointers)
    {
        for (auto iterator = svfir.begin(); iterator != svfir.end(); ++iterator)
        {
            const SVFVar* svfVariable = iterator->second;
            const auto* value = SVFUtil::dyn_cast<ValVar>(svfVariable);
            if (!value || value->isPointer() != pointers)
                continue;
            // Integer/floating aggregate constants are interpreted directly
            // and need no scalar slot.  Pointer constants (notably constant
            // expression GEPs in global initializers) are transfer results and
            // must retain their computed AddressSet.
            if (!pointers && value->isConstDataOrAggDataButNotNullPtr())
                continue;
            const Variable variable =
                nextVariable(nextVariableId_, numericType(*value));
            variables_.emplace(value, variable);
            valuesByVariableId_.resize(variable.id() + 1);
            valuesByVariableId_[variable.id()] = value;
        }
    };

    auto addObjectContents = [&](bool pointers)
    {
        for (auto iterator = svfir.begin(); iterator != svfir.end(); ++iterator)
        {
            const SVFVar* svfVariable = iterator->second;
            const auto* object = SVFUtil::dyn_cast<ObjVar>(svfVariable);
            if (!object || object->isPointer() != pointers)
                continue;

            registerObject(*object);
        }
    };

    // One stable analysis-wide ID space doubles as the sparse storage
    // coordinate. Pack it by Semi-Sparse carrier first and domain second:
    //
    //   numerical ValVars | pointer ValVars | numerical contents | pointer
    //   contents
    //
    // Every scalar/flow domain slice is a contiguous range. Dense states use
    // two compact ranges per domain; the sparse page directory does not
    // allocate their intervening range. Keeping all ValVars before contents
    // also keeps both O(1) reverse-index vectors compact.
    addScalars(false);
    addScalars(true);
    if (nextVariableId_ > std::numeric_limits<std::uint32_t>::max())
    {
        assert(false && "too many abstract-domain scalar variables");
        std::abort();
    }
    firstObjectContentVariableId_ =
        static_cast<std::uint32_t>(nextVariableId_);
    addObjectContents(false);
    addObjectContents(true);
}

void SVFIRAdapter::registerObject(const ObjVar& object) const
{
    if (locations_.count(&object) != 0)
        return;

    const Location location = nextLocation(nextLocationId_);
    const Variable content =
        nextVariable(nextVariableId_, contentNumericType(object));
    locations_.emplace(&object, location);
    objects_.emplace(location, &object);
    contentVariables_.emplace(&object, content);
    contentObjectsByVariableId_.resize(content.id() + 1);
    contentObjectsByVariableId_[content.id()] = &object;
    memoryLayout_.extend(location, content);
}

bool SVFIRAdapter::contains(const ValVar& value) const
{
    return variables_.count(&value) != 0;
}

bool SVFIRAdapter::contains(const ObjVar& object) const
{
    return locations_.count(&object) != 0;
}

Variable SVFIRAdapter::variable(const ValVar& value) const
{
    const auto iterator = variables_.find(&value);
    if (iterator == variables_.end())
    {
        assert(false && "ValVar is not tracked by this adapter");
        std::abort();
    }
    return iterator->second;
}

const ValVar* SVFIRAdapter::value(Variable variable) const
{
    return variable.id() < valuesByVariableId_.size()
           ? valuesByVariableId_[variable.id()]
           : nullptr;
}

Location SVFIRAdapter::location(const ObjVar& object) const
{
    const auto iterator = locations_.find(&object);
    if (iterator != locations_.end())
        return iterator->second;
    registerObject(object);
    return locations_.find(&object)->second;
}

Variable SVFIRAdapter::contentVariable(const ObjVar& object) const
{
    const auto iterator = contentVariables_.find(&object);
    if (iterator != contentVariables_.end())
        return iterator->second;
    registerObject(object);
    return contentVariables_.find(&object)->second;
}

const ObjVar* SVFIRAdapter::contentObject(Variable variable) const
{
    return variable.id() < contentObjectsByVariableId_.size()
           ? contentObjectsByVariableId_[variable.id()]
           : nullptr;
}

bool SVFIRAdapter::isPointer(Variable variable) const
{
    if (const ValVar* scalar = value(variable))
        return scalar->isPointer();
    if (const ObjVar* object = contentObject(variable))
        return object->isPointer();
    assert(false && "Variable is not tracked by this adapter");
    std::abort();
}

const ObjVar& SVFIRAdapter::object(Location location) const
{
    const auto iterator = objects_.find(location);
    if (iterator == objects_.end())
    {
        assert(false && "location is not tracked by this adapter");
        std::abort();
    }
    return *iterator->second;
}

} // namespace SVF
