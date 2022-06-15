//===- SVFLoop.h -- SVFLoop of SVF ------------------------------//
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
 * SVFLoop.h
 *
 *  Created on: 14, 06, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 */

#ifndef SVF_SVFLOOP_H
#define SVF_SVFLOOP_H

#include "Util/SVFBasicTypes.h"

namespace SVF
{
class SVFLoop
{
    typedef Set<const ICFGEdge *> ICFGEdgeSet;
    typedef Set<const ICFGNode *> ICFGNodeSet;
private:
    ICFGEdgeSet entryICFGEdges, backICFGEdges, inICFGEdges, outICFGEdges;
    ICFGNodeSet icfgNodes;
    u32_t loopBound;

public:
    SVFLoop(const ICFGNodeSet &_nodes, u32_t _bound) :
        icfgNodes(_nodes), loopBound(_bound)
    {

    }

    virtual ~SVFLoop() = default;

    inline ICFGNodeSet::iterator ICFGNodesBegin()
    {
        return icfgNodes.begin();
    }

    inline ICFGNodeSet::iterator ICFGNodesEnd()
    {
        return icfgNodes.end();
    }

    inline bool isInLoop(const ICFGNode *icfgNode) const
    {
        return icfgNodes.find(icfgNode) != icfgNodes.end();
    }

    inline bool isEntryICFGEdge(const ICFGEdge *edge) const
    {
        return entryICFGEdges.find(edge) != entryICFGEdges.end();
    }

    inline bool isBackICFGEdge(const ICFGEdge *edge) const
    {
        return backICFGEdges.find(edge) != entryICFGEdges.end();
    }

    inline bool isInICFGEdge(const ICFGEdge *edge) const
    {
        return inICFGEdges.find(edge) != entryICFGEdges.end();
    }

    inline bool isOutICFGEdge(const ICFGEdge *edge) const
    {
        return outICFGEdges.find(edge) != entryICFGEdges.end();
    }

    inline void addEntryICFGEdge(const ICFGEdge *edge)
    {
        entryICFGEdges.insert(edge);
    }

    inline ICFGEdgeSet::iterator entryICFGEdgesBegin()
    {
        return entryICFGEdges.begin();
    }

    inline ICFGEdgeSet::iterator entryICFGEdgesEnd()
    {
        return entryICFGEdges.end();
    }

    inline void addOutICFGEdge(const ICFGEdge *edge)
    {
        outICFGEdges.insert(edge);
    }

    inline ICFGEdgeSet::iterator outICFGEdgesBegin()
    {
        return outICFGEdges.begin();
    }

    inline ICFGEdgeSet::iterator outICFGEdgesEnd()
    {
        return outICFGEdges.end();
    }

    inline void addBackICFGEdge(const ICFGEdge *edge)
    {
        backICFGEdges.insert(edge);
    }

    inline ICFGEdgeSet::iterator backICFGEdgesBegin()
    {
        return backICFGEdges.begin();
    }

    inline ICFGEdgeSet::iterator backICFGEdgesEnd()
    {
        return backICFGEdges.end();
    }

    inline void addInICFGEdge(const ICFGEdge *edge)
    {
        inICFGEdges.insert(edge);
    }

    inline ICFGEdgeSet::iterator inEdgesBegin()
    {
        return inICFGEdges.begin();
    }

    inline ICFGEdgeSet::iterator inEdgesEnd()
    {
        return inICFGEdges.end();
    }

    inline void setLoopBound(u32_t _bound)
    {
        loopBound = _bound;
    }

    inline u32_t getLoopBound() const
    {
        return loopBound;
    }
};

}

#endif //SVF_SVFLOOP_H
