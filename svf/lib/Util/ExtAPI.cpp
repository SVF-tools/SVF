//===- ExtAPI.cpp -- External functions -----------------------------------------//
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
 * ExtAPI.cpp
 *
 *  Created on: July 1, 2022
 *      Author: Shuangxiang Kan
 */

#include "Util/ExtAPI.h"

using namespace SVF;

ExtAPI* ExtAPI::extOp = nullptr;

ExtAPI* ExtAPI::getExtAPI(const std::string& path)
{
    if (extOp == nullptr)
    {
        extOp = new ExtAPI;
    }
    return extOp;
}

void ExtAPI::destory()
{
    if (extOp != nullptr)
    {
        delete extOp;
        extOp = nullptr;
    }
}

std::string ExtAPI::getExtFuncAnnotation(const SVFFunction* fun, const std::string& funcAnnotation)
{
    assert(fun && "Null SVFFunction* pointer");
    const std::vector<std::string>& annotations = fun->getAnnotations();
    for (const std::string& annotation : annotations)
        if (annotation.find(funcAnnotation) != std::string::npos)
            return annotation;
    return "";
}

bool ExtAPI::hasExtFuncAnnotation(const SVFFunction* fun, const std::string& funcAnnotation)
{
    assert(fun && "Null SVFFunction* pointer");
    const std::vector<std::string>& annotations = fun->getAnnotations();
    for (const std::string& annotation : annotations)
        if (annotation.find(funcAnnotation) != std::string::npos)
            return true;
    return false;
}

// Does (F) have a static var X (unavailable to us) that its return points to?
bool ExtAPI::has_static(const SVFFunction* F)
{
    return F && hasExtFuncAnnotation(F, "STATIC");
}

bool ExtAPI::is_memcpy(const SVFFunction *F)
{
    return F && hasExtFuncAnnotation(F, "MEMCPY");
}

bool ExtAPI::is_memset(const SVFFunction *F)
{
    return F && hasExtFuncAnnotation(F, "MEMSET");
}

bool ExtAPI::is_alloc(const SVFFunction* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_RET");
}

// Does (F) allocate a new object and assign it to one of its arguments?
bool ExtAPI::is_arg_alloc(const SVFFunction* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_ARG");
}

// Get the position of argument which holds the new object
s32_t ExtAPI::get_alloc_arg_pos(const SVFFunction* F)
{
    std::string s = "ALLOC_ARG";
    std::string allocArg = getExtFuncAnnotation(F, s);
    assert(!allocArg.empty() && "Not an alloc call via argument or incorrect extern function annotation!");

    std::string number;
    for (char c : allocArg)
    {
        if (isdigit(c))
            number.push_back(c);
    }
    assert(!number.empty() && "Incorrect naming convention for svf external functions(ALLOC_ARG + number)?");
    return std::stoi(number);
}

// Does (F) reallocate a new object?
bool ExtAPI::is_realloc(const SVFFunction* F)
{
    return F && hasExtFuncAnnotation(F, "REALLOC_RET");
}


// Should (F) be considered "external" (either not defined in the program
//   or a user-defined version of a known alloc or no-op)?
bool ExtAPI::is_ext(const SVFFunction* F)
{
    assert(F && "Null SVFFunction* pointer");
    if (F->isDeclaration() || F->isIntrinsic())
        return true;
    else if (hasExtFuncAnnotation(F, "OVERWRITE") && F->getAnnotations().size() == 1)
        return false;
    else
        return !F->getAnnotations().empty();
}