//===- SaberSVFGBuilder.cpp -- SVFG builder in Saber-------------------------//
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
 * SaberSVFGBuilder.cpp
 *
 *  Created on: May 1, 2014
 *      Author: rockysui
 */

#include "SABER/SaberSVFGBuilder.h"
#include "SABER/SaberCheckerAPI.h"
#include "MSSA/SVFG.h"

using namespace llvm;
using namespace analysisUtil;

void SaberSVFGBuilder::createSVFG(MemSSA* mssa, SVFG* graph) {

    svfg = graph;
    svfg->buildSVFG(mssa);
    BVDataPTAImpl* pta = mssa->getPTA();
    DBOUT(DGENERAL, outs() << pasMsg("\tCollect Global Variables\n"));

    collectGlobals(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tRemove Dereference Direct SVFG Edge\n"));

    rmDerefDirSVFGEdges(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tAdd Sink SVFG Nodes\n"));

    AddExtActualParmSVFGNodes();

    if(pta->printStat())
        svfg->performStat();
    svfg->dump("Saber_SVFG",true);
}


/*!
 * Recursively collect global memory objects
 */
void SaberSVFGBuilder::collectGlobals(BVDataPTAImpl* pta) {
    PAG* pag = svfg->getPAG();
    NodeVector worklist;
    for(PAG::iterator it = pag->begin(), eit = pag->end(); it!=eit; it++) {
        PAGNode* pagNode = it->second;
        if(isa<DummyValPN>(pagNode) || isa<DummyObjPN>(pagNode))
            continue;
        if(const Value* val = pagNode->getValue()) {
            if(isa<GlobalVariable>(val))
                worklist.push_back(it->first);
        }
    }

    NodeToPTSSMap cachedPtsMap;
    while(!worklist.empty()) {
        NodeID id = worklist.back();
        worklist.pop_back();
        globs.set(id);
        PointsTo& pts = pta->getPts(id);
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it) {
            globs |= CollectPtsChain(pta,*it,cachedPtsMap);
        }
    }
}

NodeBS& SaberSVFGBuilder::CollectPtsChain(BVDataPTAImpl* pta,NodeID id, NodeToPTSSMap& cachedPtsMap) {
    PAG* pag = svfg->getPAG();

    NodeID baseId = pag->getBaseObjNode(id);
    NodeToPTSSMap::iterator it = cachedPtsMap.find(baseId);
    if(it!=cachedPtsMap.end())
        return it->second;
    else {
        PointsTo& pts = cachedPtsMap[baseId];
        pts |= pag->getFieldsAfterCollapse(baseId);

        WorkList worklist;
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
            worklist.push(*it);

        while(!worklist.empty()) {
            NodeID nodeId = worklist.pop();
            PointsTo& tmp = pta->getPts(nodeId);
            for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it) {
                pts |= CollectPtsChain(pta,*it,cachedPtsMap);
            }
        }
        return pts;
    }

}

/*!
 * Decide whether the node and its points-to contains a global objects
 */
bool SaberSVFGBuilder::accessGlobal(BVDataPTAImpl* pta,const PAGNode* pagNode) {

    NodeID id = pagNode->getId();
    PointsTo pts = pta->getPts(id);
    pts.set(id);

    return pts.intersects(globs);
}

void SaberSVFGBuilder::rmDerefDirSVFGEdges(BVDataPTAImpl* pta) {

    for(SVFG::iterator it = svfg->begin(), eit = svfg->end(); it!=eit; ++it) {
        const SVFGNode* node = it->second;

        if(const StmtSVFGNode* stmtNode = dyn_cast<StmtSVFGNode>(node)) {
            /// for store, connect the RHS/LHS pointer to its def
            if(isa<StoreSVFGNode>(stmtNode)) {
                const SVFGNode* def = svfg->getDefSVFGNode(stmtNode->getPAGDstNode());
                SVFGEdge* edge = svfg->getSVFGEdge(def,stmtNode,SVFGEdge::IntraDirect);
                assert(edge && "Edge not found!");
                svfg->removeSVFGEdge(edge);

                if(accessGlobal(pta,stmtNode->getPAGDstNode())) {
                    globSVFGNodes.insert(stmtNode);
                }
            }
            else if(isa<LoadSVFGNode>(stmtNode)) {
                const SVFGNode* def = svfg->getDefSVFGNode(stmtNode->getPAGSrcNode());
                SVFGEdge* edge = svfg->getSVFGEdge(def,stmtNode,SVFGEdge::IntraDirect);
                assert(edge && "Edge not found!");
                svfg->removeSVFGEdge(edge);

                if(accessGlobal(pta,stmtNode->getPAGSrcNode())) {
                    globSVFGNodes.insert(stmtNode);
                }
            }

        }
    }
}


/// Add actual parameter SVFGNode for 1st argument of a deallocation like external function
void SaberSVFGBuilder::AddExtActualParmSVFGNodes() {
    PAG* pag = PAG::getPAG();
    for(PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it) {
        const Function* fun = getCallee(it->first);
        if(SaberCheckerAPI::getCheckerAPI()->isMemDealloc(fun)
                || SaberCheckerAPI::getCheckerAPI()->isFClose(fun)) {
            PAG::PAGNodeList& arglist =	it->second;
            const PAGNode* pagNode = arglist.front();
            svfg->addActualParmSVFGNode(pagNode,it->first);
            svfg->addIntraDirectVFEdge(svfg->getDefSVFGNode(pagNode)->getId(),svfg->getActualParmSVFGNode(pagNode,it->first)->getId());
        }
    }
}
