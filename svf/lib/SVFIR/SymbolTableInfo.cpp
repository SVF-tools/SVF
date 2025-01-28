//===- SymbolTableInfo.cpp -- Symbol information from IR------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SymbolTableInfo.cpp
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#include <memory>

#include "SVFIR/SymbolTableInfo.h"
#include "Util/Options.h"
#include "SVFIR/SVFModule.h"


using namespace std;
using namespace SVF;
using namespace SVFUtil;



ObjTypeInfo::ObjTypeInfo(const SVFType* t, u32_t max) : type(t), flags(0), maxOffsetLimit(max), elemNum(max)
{
    assert(t && "no type information for this object?");
}


void ObjTypeInfo::resetTypeForHeapStaticObj(const SVFType* t)
{
    assert((isStaticObj() || isHeap()) && "can only reset the inferred type for heap and static objects!");
    type = t;
}
