//===- SaberSVFGBuilder.h -- Building SVFG for Saber--------------------------//
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
 * SaberSVFGBuilder.h
 *
 *  Created on: May 1, 2014
 *      Author: Yulei Sui
 */

#ifndef SABERSVFGBUILDER_H_
#define SABERSVFGBUILDER_H_

#include "MSSA/SVFGBuilder.h"
#include "Util/BasicTypes.h"
#include "Util/WorkList.h"

namespace SVF
{

class SaberSVFGBuilder : public SVFGBuilder
{

public:
    typedef Set<const SVFGNode*> SVFGNodeSet;
    typedef Map<NodeID, PointsTo> NodeToPTSSMap;
    typedef FIFOWorkList<NodeID> WorkList;

    /// Constructor
    SaberSVFGBuilder(): SVFGBuilder(true) {}

    /// Destructor
    virtual ~SaberSVFGBuilder() {}

    inline bool isGlobalSVFGNode(const SVFGNode* node) const
    {
        return globSVFGNodes.find(node)!=globSVFGNodes.end();
    }

    /// Add ActualParmVFGNode
    inline void addActualParmVFGNode(const PAGNode* pagNode, const CallICFGNode* cs)
    {
        svfg->addActualParmVFGNode(pagNode, cs);
    }

protected:
    /// Re-write create SVFG method
    virtual void buildSVFG();

    /// Return TRUE if this is a strong update STORE statement.
    bool isStrongUpdate(const SVFGNode* node, NodeID& singleton, BVDataPTAImpl* pta);

private:
    /// Remove direct value-flow edge to a dereference point for Saber source-sink memory error detection
    /// for example, given two statements: p = alloc; q = *p, the direct SVFG edge between them is deleted
    /// Because those edges only stand for values used at the dereference points but they can not pass the value to other definitions
    void rmDerefDirSVFGEdges(BVDataPTAImpl* pta);

    /// Remove Incoming Edge for strong-update (SU) store instruction
    /// Because the SU node does not receive indirect value
    void rmIncomingEdgeForSUStore(BVDataPTAImpl* pta);

    /// Add actual parameter SVFGNode for 1st argument of a deallocation like external function
    /// In order to path sensitive leak detection
    virtual void AddExtActualParmSVFGNodes(PTACallGraph* callgraph);

    /// Collect memory pointed global pointers,
    /// note that this collection is recursively performed, for example gp-->obj-->obj'
    /// obj and obj' are both considered global memory
    void collectGlobals(BVDataPTAImpl* pta);

    /// Whether points-to of a PAGNode points-to global variable
    bool accessGlobal(BVDataPTAImpl* pta,const PAGNode* pagNode);

    /// Collect objects along points-to chains
    PointsTo& CollectPtsChain(BVDataPTAImpl* pta,NodeID id, NodeToPTSSMap& cachedPtsMap);

    PointsTo globs;
    /// Store all global SVFG nodes
    SVFGNodeSet globSVFGNodes;
};

} // End namespace SVF

#endif /* SABERSVFGBUILDER_H_ */
