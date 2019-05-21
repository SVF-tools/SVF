//===- CSC.h -- Cycle Stride Calculation algorithm---------------------------------------//
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
 * CSC.h
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#ifndef PROJECT_CSC_H
#define PROJECT_CSC_H

#include "Util/SCC.h"
#include "Util/BasicTypes.h"	// for NodeBS
#include "MemoryModel/ConsG.h"
#include "Util/WorkList.h"
#include <limits.h>
#include <stack>
#include <map>


/*!
 * class CGSCC: constraint graph specific SCC detection
 */
class CGSCC :  public SCCDetection<ConstraintGraph*>{
private:
    void visit(NodeID v, const std::string& edgeFlag="default");

public:
    CGSCC(ConstraintGraph* g) : SCCDetection(g) {}
    void find(const std::string& edgeFlag="default");
    void find(NodeSet& candidates, const std::string& edgeFlag="default");

};


/*!
 * class CSC: cycle stride calculation
 */
class CSC {
protected:
    typedef llvm::DenseMap<NodeID, NodeID> IdToIdMap;
    typedef llvm::DenseMap<NodeID, NodeBS> NodeStrides;
    typedef FILOWorkList<NodeID> WorkStack;

private:
    ConstraintGraph* _consG;
    CGSCC* _scc;

    NodeID _I;
    IdToIdMap _D;
    WorkStack _S;
    NodeStrides _nodeStrides;
//    NodeSet _pwcReps;

public:
    CSC(ConstraintGraph* g, CGSCC* c) : _consG(g), _scc(c) {}
    NodeStrides& getNodeStrides() { return _nodeStrides; }
//    const NodeSet& getPWCReps() const { return  _pwcReps; }
    void find(NodeStack& candidates);
    void visit(NodeID nodeId, const ConstraintEdge* edge);
    void clear();
};

#endif //PROJECT_CSC_H
