//===----- CFLDataBuilder.h -- CFL Data Builder --------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFLGraphBuilder.h
 *
 *  Created on: Nov 6, 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFLDATABUILDER_H_
#define INCLUDE_CFL_CFLDATABUILDER_H_

#include "CFL/CFLData.h"
#include "Graphs/CFLGraph.h"

namespace SVF
{

/*!
 * Build CFLData from CFL graph

 */

class CFLDataBuilder
{
protected:
    CFLData* _cflData;
    CFLGraph* _graph;

public:
    /// Constructor
    CFLDataBuilder(CFLGraph* cflgraph) : _cflData(NULL), _graph(cflgraph)
    {
        if (!_cflData)
            _cflData = new CFLData();
    }

    /// Destructor
    virtual ~CFLDataBuilder()
    {
    }

    CFLData* cflData()
    {
        return _cflData;
    }

    //CFL data operations
    //@{
    virtual bool addEdge(const NodeID srcId, const NodeID dstId, const Label ty)
    {
        return cflData()->addEdge(srcId, dstId, ty);
    }

    virtual NodeBS addEdges(const NodeID srcId, const NodeBS& dstData, const Label ty)
    {
        return cflData()->addEdges(srcId, dstData, ty);
    }
    //@}
    CFLData* build()
    {
        for (CFLEdge* edge: _graph->getCFLEdges())
            addEdge(edge->getSrcID(), edge->getDstID(), edge->getEdgeKind());
        return  cflData();
    }


};
}// SVF

#endif /* INCLUDE_CFL_CFLGRAPHBUILDER_H_*/
