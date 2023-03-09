//===- SaberSVFGBuilder.cpp -- SVFG builder in Saber-------------------------//
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
 * SaberSVFGBuilder.cpp
 *
 *  Created on: May 1, 2014
 *      Author: rockysui
 */

#include "SABER/SaberSVFGBuilder.h"
#include "SABER/SaberCheckerAPI.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/SVFG.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

void SaberSVFGBuilder::buildSVFG()
{

    MemSSA* mssa = svfg->getMSSA();
    svfg->buildSVFG();
    BVDataPTAImpl* pta = mssa->getPTA();
    DBOUT(DGENERAL, outs() << pasMsg("\tCollect Global Variables\n"));

    collectGlobals(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tRemove Dereference Direct SVFG Edge\n"));

    rmDerefDirSVFGEdges(pta);

    rmIncomingEdgeForSUStore(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tAdd Sink SVFG Nodes\n"));

    AddExtActualParmSVFGNodes(pta->getPTACallGraph());

    if(pta->printStat())
        svfg->performStat();
}


/*!
 * Recursively collect global memory objects
 */
void SaberSVFGBuilder::collectGlobals(BVDataPTAImpl* pta)
{
    SVFIR* pag = svfg->getPAG();
    NodeVector worklist;
    for(SVFIR::iterator it = pag->begin(), eit = pag->end(); it!=eit; it++)
    {
        PAGNode* pagNode = it->second;
        if (SVFUtil::isa<DummyValVar, DummyObjVar>(pagNode))
            continue;

        if(GepObjVar* gepobj = SVFUtil::dyn_cast<GepObjVar>(pagNode))
        {
            if(SVFUtil::isa<DummyObjVar>(pag->getGNode(gepobj->getBaseNode())))
                continue;
        }
        if(const SVFValue* val = pagNode->getValue())
        {
            if(SVFUtil::isa<SVFGlobalValue>(val))
                worklist.push_back(it->first);
        }
    }

    NodeToPTSSMap cachedPtsMap;
    while(!worklist.empty())
    {
        NodeID id = worklist.back();
        worklist.pop_back();
        globs.set(id);
        const PointsTo& pts = pta->getPts(id);
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
        {
            globs |= CollectPtsChain(pta,*it,cachedPtsMap);
        }
    }
}


/*
 * https://github.com/SVF-tools/SVF/issues/991
 *
 * Originally, this function will collect all base pointers with all their fields
 * inside the points-to set of global variables. But if a global variable points
 * to the pointer returned by malloc() at some program points, then all pointers
 * returned by malloc() will be included in the global set because of the
 * context-insensitive pointer analysis results. This will make saber abandon
 * too many slicing thus miss potential bugs.
 *
 * We add an option "saber-collect-extret-globals" to control whether this function
 * will collect external functions' returned pointers. This option is true by default,
 * making it to be false will let saber analyze more slicing but cause performance downgrade.
 *
 */
PointsTo& SaberSVFGBuilder::CollectPtsChain(BVDataPTAImpl* pta, NodeID id, NodeToPTSSMap& cachedPtsMap)
{
    SVFIR* pag = svfg->getPAG();

    NodeID baseId = pag->getBaseObjVar(id);
    NodeToPTSSMap::iterator it = cachedPtsMap.find(baseId);
    if(it!=cachedPtsMap.end())
    {
        return it->second;
    }
    else
    {
        PointsTo& pts = cachedPtsMap[baseId];
        // base object
        if (!Options::CollectExtRetGlobals())
        {
            if(pta->isFIObjNode(baseId) && pag->getGNode(baseId)->hasValue())
            {
                const SVFCallInst* inst = SVFUtil::dyn_cast<SVFCallInst>(pag->getGNode(baseId)->getValue());
                if(inst && SVFUtil::isExtCall(inst))
                {
                    return pts;
                }
            }
        }

        pts |= pag->getFieldsAfterCollapse(baseId);

        WorkList worklist;
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
            worklist.push(*it);

        while(!worklist.empty())
        {
            NodeID nodeId = worklist.pop();
            const PointsTo& tmp = pta->getPts(nodeId);
            for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
            {
                pts |= CollectPtsChain(pta,*it,cachedPtsMap);
            }
        }
        return pts;
    }
}

/*!
 * Decide whether the node and its points-to contains a global objects
 */
bool SaberSVFGBuilder::accessGlobal(BVDataPTAImpl* pta,const PAGNode* pagNode)
{

    NodeID id = pagNode->getId();
    PointsTo pts = pta->getPts(id);
    pts.set(id);

    return pts.intersects(globs);
}

void SaberSVFGBuilder::rmDerefDirSVFGEdges(BVDataPTAImpl* pta)
{

    for(SVFG::iterator it = svfg->begin(), eit = svfg->end(); it!=eit; ++it)
    {
        const SVFGNode* node = it->second;

        if(const StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node))
        {
            /// for store, connect the RHS/LHS pointer to its def
            if(SVFUtil::isa<StoreSVFGNode>(stmtNode))
            {
                const SVFGNode* def = svfg->getDefSVFGNode(stmtNode->getPAGDstNode());
                if(SVFGEdge* edge = svfg->getIntraVFGEdge(def,stmtNode,SVFGEdge::IntraDirectVF))
                    svfg->removeSVFGEdge(edge);
                else
                    assert((svfg->getKind()==VFG::FULLSVFG_OPT || svfg->getKind()==VFG::PTRONLYSVFG_OPT)  && "Edge not found!");

                if(accessGlobal(pta,stmtNode->getPAGDstNode()))
                {
                    globSVFGNodes.insert(stmtNode);
                }
            }
            else if(SVFUtil::isa<LoadSVFGNode>(stmtNode))
            {
                const SVFGNode* def = svfg->getDefSVFGNode(stmtNode->getPAGSrcNode());
                if(SVFGEdge* edge = svfg->getIntraVFGEdge(def,stmtNode,SVFGEdge::IntraDirectVF))
                    svfg->removeSVFGEdge(edge);
                else
                    assert((svfg->getKind()==VFG::FULLSVFG_OPT || svfg->getKind()==VFG::PTRONLYSVFG_OPT)  && "Edge not found!");

                if(accessGlobal(pta,stmtNode->getPAGSrcNode()))
                {
                    globSVFGNodes.insert(stmtNode);
                }
            }

        }
    }
}

/*!
 * Return TRUE if this is a strong update STORE statement.
 */
bool SaberSVFGBuilder::isStrongUpdate(const SVFGNode* node, NodeID& singleton, BVDataPTAImpl* pta)
{
    bool isSU = false;
    if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        const PointsTo& dstCPSet = pta->getPts(store->getPAGDstNodeID());
        if (dstCPSet.count() == 1)
        {
            /// Find the unique element in cpts
            PointsTo::iterator it = dstCPSet.begin();
            singleton = *it;

            // Strong update can be made if this points-to target is not heap, array or field-insensitive.
            if (!pta->isHeapMemObj(singleton) && !pta->isArrayMemObj(singleton)
                    && SVFIR::getPAG()->getBaseObj(singleton)->isFieldInsensitive() == false
                    && !pta->isLocalVarInRecursiveFun(singleton))
            {
                isSU = true;
            }
        }
    }
    return isSU;
}

/*!
 * Remove Incoming Edge for strong-update (SU) store instruction
 * Because the SU node does not receive indirect value
 *
 * e.g.,
 *      L1: *p = O; (singleton)
 *      L2: *p = _; (SU here)
 *      We should remove the indirect value flow L1 -> L2
 *      Because the points-to set of O from L1 does not pass to that after L2
 */
void SaberSVFGBuilder::rmIncomingEdgeForSUStore(BVDataPTAImpl* pta)
{

    for(SVFG::iterator it = svfg->begin(), eit = svfg->end(); it!=eit; ++it)
    {
        const SVFGNode* node = it->second;

        if(const StoreSVFGNode* stmtNode = SVFUtil::dyn_cast<StoreSVFGNode>(node))
        {
            if(SVFUtil::isa<StoreStmt>(stmtNode->getPAGEdge()))
            {
                NodeID singleton;
                if(isStrongUpdate(node, singleton, pta))
                {
                    Set<SVFGEdge*> toRemove;
                    for (SVFGNode::const_iterator it2 = node->InEdgeBegin(), eit2 = node->InEdgeEnd(); it2 != eit2; ++it2)
                    {
                        if ((*it2)->isIndirectVFGEdge())
                        {
                            toRemove.insert(*it2);
                        }
                    }
                    for (SVFGEdge* edge: toRemove)
                    {
                        svfg->removeSVFGEdge(edge);
                    }
                }
            }
        }
    }
}


/// Add actual parameter SVFGNode for 1st argument of a deallocation like external function
void SaberSVFGBuilder::AddExtActualParmSVFGNodes(PTACallGraph* callgraph)
{
    SVFIR* pag = SVFIR::getPAG();
    for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it)
    {
        PTACallGraph::FunctionSet callees;
        callgraph->getCallees(it->first, callees);
        for (PTACallGraph::FunctionSet::const_iterator cit = callees.begin(),
                ecit = callees.end(); cit != ecit; cit++)
        {

            const SVFFunction* fun = *cit;
            if (SaberCheckerAPI::getCheckerAPI()->isMemDealloc(fun)
                    || SaberCheckerAPI::getCheckerAPI()->isFClose(fun))
            {
                SVFIR::SVFVarList& arglist = it->second;
                for(SVFIR::SVFVarList::const_iterator ait = arglist.begin(), aeit = arglist.end(); ait!=aeit; ++ait)
                {
                    const PAGNode *pagNode = *ait;
                    if (pagNode->isPointer())
                    {
                        addActualParmVFGNode(pagNode, it->first);
                        svfg->addIntraDirectVFEdge(svfg->getDefSVFGNode(pagNode)->getId(), svfg->getActualParmVFGNode(pagNode, it->first)->getId());
                    }
                }
            }
        }
    }
}
