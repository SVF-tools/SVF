//===- CGSCC.h -- Constraint graph based SCC detection algorithm---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * CGSCC.h
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#ifndef PROJECT_CGSCC_H
#define PROJECT_CGSCC_H

#include "Util/SCC.h"
#include "MemoryModel/ConsG.h"


class CGSCC :  public SCCDetection<ConstraintGraph*>{
private:
    void visit(NodeID v, const std::string& edgeFlag="default");

public:
    CGSCC(ConstraintGraph* g) : SCCDetection(g) {}
    void find(const std::string& edgeFlag="default");
    void find(NodeSet& candidates, const std::string& edgeFlag="default");

};

#endif //PROJECT_CGSCC_H
