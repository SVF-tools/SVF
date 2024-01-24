//===- CPPUtil.h -- Base class of pointer analyses ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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

/*
 * CPPUtil.h
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#ifndef CPPUtil_H_
#define CPPUtil_H_

#include "SVFIR/SVFValue.h"

namespace SVF
{

class CHGraph;
/*
 * Util class to assist pointer analysis for cpp programs
 */

namespace cppUtil
{

struct DemangledName
{
    std::string className;
    std::string funcName;
    bool isThunkFunc;
};

struct DemangledName demangle(const std::string& name);

std::string getBeforeBrackets(const std::string& name);
std::string getClassNameFromVtblObj(const std::string& vtblName);

/// Constants pertaining to CTir, for C and C++.
/// TODO: move helper functions here too?
namespace ctir
{
/// On loads, stores, GEPs representing dereferences, and calls
/// representing virtual calls.
/// (The static type.)
const std::string derefMDName = "ctir";
/// On the (global) virtual table itself.
/// (The class it corresponds to.)
const std::string vtMDName = "ctir.vt";
/// On the bitcast of `this` to i8*.
/// (The class the constructor it corresponds to.)
const std::string vtInitMDName = "ctir.vt.init";

/// Value we expect a ctir-annotated module to have.
const uint32_t moduleFlagValue = 1;
} // namespace ctir

} // End namespace cppUtil

} // End namespace SVF

#endif /* CPPUtil_H_ */
