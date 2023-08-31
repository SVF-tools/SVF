//===- ExtAPI.h -- External functions -----------------------------------------//
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
 * ExtAPI.h
 *
 *  Created on: July 1, 2022
 *      Author: Shuangxiang Kan
 */

#ifndef __ExtAPI_H
#define __ExtAPI_H

#include "SVFIR/SVFValue.h"
#include <Util/config.h>
#include <string>
#include <vector>
#include <map>

/// For a more detailed explanation of how External APIs are handled in SVF, please refer to the SVF Wiki: https://github.com/SVF-tools/SVF/wiki/Handling-External-APIs-with-extapi.c

#define EXTAPI_BC_PATH "Release-build/svf-llvm/extapi.bc"

namespace SVF
{

class ExtAPI
{
private:

    static ExtAPI *extOp;

    ExtAPI() = default;

public:

    static ExtAPI *getExtAPI(const std::string& = "");

    static void destory();

    // Get extapi.bc file path
    std::string getExtBcPath();

    // Get the annotation of (F)
    std::string getExtFuncAnnotation(const SVFFunction* fun, const std::string& funcAnnotation);

    // Does (F) have some annotation?
    bool hasExtFuncAnnotation(const SVFFunction* fun, const std::string& funcAnnotation);

    // Does (F) have a static var X (unavailable to us) that its return points to?
    bool has_static(const SVFFunction *F);

    // Does (F) have a memcpy_like operation?
    bool is_memcpy(const SVFFunction *F);

    // Does (F) have a memset_like operation?
    bool is_memset(const SVFFunction *F);

    // Does (F) allocate a new object and return it?
    bool is_alloc(const SVFFunction *F);

    // Does (F) allocate a new object and assign it to one of its arguments?
    bool is_arg_alloc(const SVFFunction *F);

    // Get the position of argument which holds the new object
    s32_t get_alloc_arg_pos(const SVFFunction *F);

    // Does (F) reallocate a new object?
    bool is_realloc(const SVFFunction *F);

    // Should (F) be considered "external" (either not defined in the program
    //   or a user-defined version of a known alloc or no-op)?
    bool is_ext(const SVFFunction *F);
};
} // End namespace SVF

#endif