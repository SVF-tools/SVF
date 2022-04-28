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
#include "Util/BasicTypes.h"    // for NodeBS
#include "Graphs/ConsG.h"
#include "Util/WorkList.h"
#include <limits.h>
#include <stack>
#include <map>

namespace SVF
{

typedef SCCDetection<ConstraintGraph *> CGSCC;

/*!
 * class CSC: cycle stride calculation
 */
class CSC
{
public:
    typedef Map<NodeID, NodeID> IdToIdMap;
    typedef FILOWorkList<NodeID> WorkStack;
    typedef typename IdToIdMap::iterator iterator;

private:
    ConstraintGraph* _consG;
    CGSCC* _scc;

    NodeID _I;
    IdToIdMap _D;       // the sum of weight of a path relevant to a certain node, while accessing the node via DFS
    NodeStack _S;       // a stack holding a DFS branch
    NodeSet _visited;   // a set holding visited nodes
//    NodeStrides nodeStrides;
//    IdToIdMap pwcReps;

public:
    CSC(ConstraintGraph* g, CGSCC* c)
        : _consG(g), _scc(c), _I(0) {}

    void find(NodeStack& candidates);
    void visit(NodeID nodeId, s32_t _w);
    void clear();

    bool isVisited(NodeID nId)
    {
        return _visited.find(nId) != _visited.end();
    }

    void setVisited(NodeID nId)
    {
        _visited.insert(nId);
    }
//    inline iterator begin() { return pwcReps.begin(); }
//
//    inline iterator end() { return pwcReps.end(); }

//    NodeStrides &getNodeStrides() { return nodeStrides; }

//    const NodeSet& getPWCReps() const { return  _pwcReps; }
};

} // End namespace SVF

#endif //PROJECT_CSC_H
