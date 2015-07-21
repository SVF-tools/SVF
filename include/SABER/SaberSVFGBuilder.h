/* SVF - Static Value-Flow Analysis Framework 
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

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

class SVFGNode;
class PAGNode;

class SaberSVFGBuilder : public SVFGBuilder {

public:
    typedef std::set<const SVFGNode*> SVFGNodeSet;
    typedef std::map<NodeID, NodeBS> NodeToPTSSMap;
    typedef FIFOWorkList<NodeID> WorkList;

    /// Constructor
    SaberSVFGBuilder() {}

    /// Destructor
    virtual ~SaberSVFGBuilder() {}

    inline bool isGlobalSVFGNode(const SVFGNode* node) const {
        return globSVFGNodes.find(node)!=globSVFGNodes.end();
    }
protected:
    /// Re-write create SVFG method
    virtual void createSVFG(MemSSA* mssa, SVFG* graph);

private:
    /// Remove direct value-flow edge to a dereference point for Saber source-sink memory error detection
    /// for example, given two statements: p = alloc; q = *p, the direct SVFG edge between them is deleted
    /// Because those edges only stand for values used at the dereference points but they can not pass the value to other definitions
    void rmDerefDirSVFGEdges(BVDataPTAImpl* pta);

    /// Add actual parameter SVFGNode for 1st argument of a deallocation like external function
    /// In order to path sensitive leak detection
    void AddExtActualParmSVFGNodes();

    /// Collect memory pointed global pointers,
    /// note that this collection is recursively performed, for example gp-->obj-->obj'
    /// obj and obj' are both considered global memory
    void collectGlobals(BVDataPTAImpl* pta);

    /// Whether points-to of a PAGNode points-to global variable
    bool accessGlobal(BVDataPTAImpl* pta,const PAGNode* pagNode);

    /// Collect objects along points-to chains
    NodeBS& CollectPtsChain(BVDataPTAImpl* pta,NodeID id, NodeToPTSSMap& cachedPtsMap);

    NodeBS globs;
    /// Store all global SVFG nodes
    SVFGNodeSet globSVFGNodes;
};


#endif /* SABERSVFGBUILDER_H_ */
