//===- PTAStat.h -- Base class for statistics---------------------------------//
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
 * PTAStat.h
 *
 *  Created on: Oct 12, 2013
 *      Author: Yulei Sui
 */

#ifndef ANDERSENSTAT_H_
#define ANDERSENSTAT_H_

#include "SVFIR/SVFValue.h"
#include "MemoryModel/PointsTo.h"
#include "Util/SVFStat.h"
#include <iostream>
#include <map>
#include <string>

namespace SVF
{

class PointerAnalysis;

/*!
 * Pointer Analysis Statistics
 */
class PTAStat: public SVFStat
{
public:
    PTAStat(PointerAnalysis* p);
    virtual ~PTAStat() {}

    NodeBS localVarInRecursion;

    void performStat() override;

    void callgraphStat() override;
private:
    PointerAnalysis* pta;
};

} // End namespace SVF

#endif /* ANDERSENSTAT_H_ */
