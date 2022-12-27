//===- FSMPTA.cpp -- Flow-sensitive analysis of multithreaded programs-------------//
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
 * FSMPTA.cpp
 *
 *  Created on: Jul 29, 2015
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/FSMPTA.h"
#include "MTA/MHP.h"
#include "MTA/PCG.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

FSMPTA* FSMPTA::mfspta = nullptr;
u32_t MTASVFGBuilder::numOfNewSVFGEdges = 0;
u32_t MTASVFGBuilder::numOfRemovedSVFGEdges = 0;
u32_t MTASVFGBuilder::numOfRemovedPTS = 0;

/*!
 *
 */
void MTASVFGBuilder::buildSVFG()
{
    MemSSA* mssa = svfg->getMSSA();
    svfg->buildSVFG();
    if (ADDEDGE_NOEDGE != Options::AddModelFlag())
    {
        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("FSMPTA adding edge\n"));
        DBOUT(DMTA, outs() << SVFUtil::pasMsg("FSMPTA adding edge\n"));
        connectMHPEdges(mssa->getPTA());
    }
    if (mssa->getPTA()->printStat())
        svfg->performStat();
}

/*!
 *
 */
void MTASVFGBuilder::collectLoadStoreSVFGNodes()
{

    for (SVFG::const_iterator it = svfg->begin(), eit = svfg->end(); it != eit; ++it)
    {
        const SVFGNode* snode = it->second;
        if (SVFUtil::isa<LoadSVFGNode>(snode))
        {
            const StmtSVFGNode* node = SVFUtil::cast<StmtSVFGNode>(snode);
            if (node->getInst())
            {
                ldnodeSet.insert(node);
            }
        }
        if (SVFUtil::isa<StoreSVFGNode>(snode))
        {
            const StmtSVFGNode* node = SVFUtil::cast<StmtSVFGNode>(snode);
            if (node->getInst())
            {
                stnodeSet.insert(node);
            }
        }
    }
}
bool MTASVFGBuilder::recordEdge(NodeID id1, NodeID id2, PointsTo pts)
{
    NodeIDPair pair = std::make_pair(id1, id2);
    if (recordedges.find(pair) == recordedges.end())
    {
        recordedges.insert(pair);
        edge2pts[pair] = pts;
        return true;
    }
    else
    {
        edge2pts[pair] |= pts;
    }
    return false;
}
bool MTASVFGBuilder::recordAddingEdge(NodeID id1, NodeID id2, PointsTo pts)
{
    return recordEdge(id1, id2, pts);
}

bool MTASVFGBuilder::recordRemovingEdge(NodeID id1, NodeID id2, PointsTo pts)
{
    return recordEdge(id1, id2, pts);
}

void MTASVFGBuilder::performAddingMHPEdges()
{
    MTASVFGBuilder::numOfNewSVFGEdges = recordedges.size();
    while (!recordedges.empty())
    {
        std::pair<NodeID, NodeID> edgepair = *recordedges.begin();
        PointsTo pts = edge2pts[edgepair];
        recordedges.erase(recordedges.begin());
        addTDEdges(edgepair.first, edgepair.second, pts);
    }
}

SVFGEdge*  MTASVFGBuilder::addTDEdges(NodeID srcId, NodeID dstId, PointsTo& pts)
{

    SVFGNode* srcNode = svfg->getSVFGNode(srcId);
    SVFGNode* dstNode = svfg->getSVFGNode(dstId);

    if(SVFGEdge* edge = svfg->hasThreadVFGEdge(srcNode,dstNode,SVFGEdge::TheadMHPIndirectVF))
    {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(pts.toNodeBS()) ? edge : nullptr);
    }
    else
    {
        MTASVFGBuilder::numOfNewSVFGEdges++;
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(pts.toNodeBS());
        return (svfg->addSVFGEdge(indirectEdge) ? indirectEdge : nullptr);
    }
}

void MTASVFGBuilder::performRemovingMHPEdges()
{
    while (!recordedges.empty())
    {
        std::pair<NodeID, NodeID> edgepair = *recordedges.begin();
        recordedges.erase(recordedges.begin());

        PointsTo remove_pts = edge2pts[edgepair];
        const StmtSVFGNode*  n1 = SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(edgepair.first));
        const StmtSVFGNode*  n2 = SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(edgepair.second));

        assert (n1&&n2 && "one node of removed pair is null");
        assert (n1->hasOutgoingEdge() && "n1 doesn't have out edge");
        assert (n2->hasIncomingEdge() && "n2 doesn't have in edge");

        Set<SVFGEdge*> removededges;
        for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n1->OutEdgeBegin(); iter != n1->OutEdgeEnd(); ++iter)
        {
            SVFGEdge* edge = *iter;
            if (edge->isIndirectVFGEdge() && (edge->getDstNode()==n2))
            {
                IndirectSVFGEdge* e = SVFUtil::cast<IndirectSVFGEdge>(edge);
                const NodeBS& pts = e->getPointsTo();
                for (PointsTo::iterator o = remove_pts.begin(), eo = remove_pts.end(); o != eo; ++o)
                {
                    if (const_cast<NodeBS&>(pts).test(*o))
                    {
                        const_cast<NodeBS&>(pts).reset(*o);
                        MTASVFGBuilder::numOfRemovedPTS ++;
                    }
                }

                if (0 == e->getPointsTo().count())
                {
                    removededges.insert(edge);
                }
            }
        }

        while(!removededges.empty())
        {
            SVFGEdge* edge = *removededges.begin();
            removededges.erase(removededges.begin());
            svfg->removeSVFGEdge(edge);
            DBOUT(DMTA,outs()<<"Read Precision remove: "<<edge->getSrcID()<<" -> "<<edge->getDstID()<<"\n");
            MTASVFGBuilder::numOfRemovedSVFGEdges++;
        }
    }
}


/*!
 *
 */

/// whether is a first write in the lock span.
bool MTASVFGBuilder::isHeadofSpan(const StmtSVFGNode* n, LockAnalysis::LockSpan lspan)
{
    SVFGNodeLockSpan pair(n,lspan);
    if (pairheadmap.find(pair) != pairheadmap.end())
        return pairheadmap[pair];

    SVFGNodeIDSet prev = getPrevNodes(n);

    for (SVFGNodeIDSet::iterator it = prev.begin(), eit = prev.end(); it != eit; ++it)
    {
        assert (SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(*it)) && "prev is not a store node");
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* prevIns = prevNode->getInst();

        if (lockana->hasOneCxtInLockSpan(prevIns, lspan))
        {
            pairheadmap[pair]=false;
            return false;
        }
    }
    pairheadmap[pair]=true;
    return true;
}

bool MTASVFGBuilder::isHeadofSpan(const StmtSVFGNode* n, InstSet mergespan)
{

    SVFGNodeIDSet prev = getPrevNodes(n);

    for (SVFGNodeIDSet::iterator it = prev.begin(), eit = prev.end(); it != eit; ++it)
    {
        assert (SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(*it)) && "prev is not a store node");
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* prevIns = prevNode->getInst();
        if (mergespan.find(prevIns)!=mergespan.end())
            return false;
    }
    return true;
}

/// whether for all lockspans that n belongs to, n is the first write.
/// strong constraints but scalable
bool MTASVFGBuilder::isHeadofSpan(const StmtSVFGNode* n)
{
    if (headmap.find(n) != headmap.end())
        return headmap[n];

    SVFGNodeIDSet prev = getPrevNodes(n);

    for (SVFGNodeIDSet::iterator it = prev.begin(), eit = prev.end(); it != eit; ++it)
    {
        assert(SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(*it)) && "prev is not a store node");
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* prevIns = prevNode->getInst();

        if (lockana->isInSameSpan(prevIns, n->getInst()))
        {
            headmap[n]=false;
            return false;
        }
    }
    headmap[n]=true;
    return true;
}

bool MTASVFGBuilder::isTailofSpan(const StmtSVFGNode* n, InstSet mergespan)
{

    SVFGNodeIDSet succ = getSuccNodes(n);

    for (SVFGNodeIDSet::iterator it = succ.begin(), eit = succ.end(); it != eit; ++it)
    {
        assert((SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(svfg->getSVFGNode(*it))) &&
               "succ is not a store/load node");
        const StmtSVFGNode* succNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* succIns = succNode->getInst();

        if (mergespan.find(succIns)!=mergespan.end())
            return false;
    }
    return true;
}

/// whether is a last write in the lock span.
bool MTASVFGBuilder::isTailofSpan(const StmtSVFGNode* n, LockAnalysis::LockSpan lspan)
{
    assert(SVFUtil::isa<StoreSVFGNode>(n) && "Node is not a store node");

    SVFGNodeLockSpan pair(n,lspan);
    if (pairtailmap.find(pair) != pairtailmap.end())
        return pairtailmap[pair];

    SVFGNodeIDSet succ = getSuccNodes(n);
    for (SVFGNodeIDSet::iterator it = succ.begin(), eit = succ.end(); it != eit; ++it)
    {
        assert((SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(svfg->getSVFGNode(*it))) &&
               "succ is not a store/load node");
        if (SVFUtil::isa<LoadSVFGNode>(svfg->getSVFGNode(*it)))
            continue;
        const StmtSVFGNode* succNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* succIns = succNode->getInst();

        if (lockana->hasOneCxtInLockSpan(succIns, lspan))
        {
            pairtailmap[pair]=false;
            return false;
        }
    }
    pairtailmap[pair]=true;
    return true;
}


/// whether for all lockspans that n belongs to, n is the last write.
/// strong constraints but scalable
bool MTASVFGBuilder::isTailofSpan(const StmtSVFGNode* n)
{
    assert(SVFUtil::isa<StoreSVFGNode>(n) && "Node is not a store node");

    if (tailmap.find(n) != tailmap.end())
        return tailmap[n];

    SVFGNodeIDSet succ = getSuccNodes(n);

    for (SVFGNodeIDSet::iterator it = succ.begin(), eit = succ.end(); it != eit; ++it)
    {
        assert((SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(svfg->getSVFGNode(*it))) && "succ is not a store/load node");
        if (SVFUtil::isa<LoadSVFGNode>(svfg->getSVFGNode(*it)))
            continue;

        const StmtSVFGNode* succNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(*it));
        const SVFInstruction* succIns = succNode->getInst();

        if (lockana->isInSameSpan(succIns, n->getInst()))
        {
            tailmap[n] = false;
            return false;
        }
    }
    tailmap[n]=true;
    return true;
}

MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getPrevNodes(const StmtSVFGNode* n)
{
    if (prevset.find(n)!=prevset.end())
        return prevset[n];

    SVFGNodeIDSet prev;
    SVFGNodeSet worklist;
    SVFGNodeSet visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n->InEdgeBegin(); iter != n->InEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge())
        {
            worklist.insert(edge->getSrcNode());
        }
    }

    while (!worklist.empty())
    {
        const SVFGNode* node =  *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode>(node))
            prev.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->InEdgeBegin(); iter != node->InEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() && visited.find(edge->getSrcNode())==visited.end())
                    worklist.insert(edge->getSrcNode());
            }
        }
    }
    prevset[n]=prev;
    return prev;
}
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getSuccNodes(const StmtSVFGNode* n)
{
    if (succset.find(n)!=succset.end())
        return succset[n];

    SVFGNodeIDSet succ;
    SVFGNodeSet worklist;
    SVFGNodeSet visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n->OutEdgeBegin(); iter != n->OutEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge())
        {
            worklist.insert(edge->getDstNode());
        }
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(node))
            succ.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->OutEdgeBegin(); iter != node->OutEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() && visited.find(edge->getDstNode()) == visited.end())
                    worklist.insert(edge->getDstNode());
            }
        }
    }
    succset[n]=succ;
    return succ;
}
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getSuccNodes(const StmtSVFGNode* n, NodeID o)
{

    SVFGNodeIDSet succ;
    SVFGNodeSet worklist;
    SVFGNodeSet visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n->OutEdgeBegin(); iter != n->OutEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge())
        {
            IndirectSVFGEdge* e = SVFUtil::cast<IndirectSVFGEdge>(edge);
            NodeBS pts = e->getPointsTo();
            if(pts.test(o))
                worklist.insert(edge->getDstNode());
        }
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(node))
            succ.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->OutEdgeBegin(); iter != node->OutEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() && visited.find(edge->getDstNode()) == visited.end())
                {
                    IndirectSVFGEdge* e = SVFUtil::cast<IndirectSVFGEdge>(edge);
                    NodeBS pts = e->getPointsTo();
                    if(pts.test(o))
                        worklist.insert(edge->getDstNode());
                }
            }
        }
    }

    return succ;
}

void MTASVFGBuilder::handleStoreLoadNonSparse(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
    pts &= pta->getPts(n2->getPAGSrcNodeID());

    addTDEdges(n1->getId(), n2->getId(), pts);
}



void MTASVFGBuilder::handleStoreStoreNonSparse(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
    pts &= pta->getPts(n2->getPAGDstNodeID());

    addTDEdges(n1->getId(), n2->getId(), pts);
    addTDEdges(n2->getId(), n1->getId(), pts);
}

void MTASVFGBuilder::handleStoreLoad(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    const SVFInstruction* i1 = n1->getInst();
    const SVFInstruction* i2 = n2->getInst();
    /// MHP
    if (ADDEDGE_NOMHP!=Options::AddModelFlag() && !mhp->mayHappenInParallel(i1, i2))
        return;
    /// Alias
    if (ADDEDGE_NOALIAS!=Options::AddModelFlag() && !pta->alias(n1->getPAGDstNodeID(), n2->getPAGSrcNodeID()))
        return;


    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
    pts &= pta->getPts(n2->getPAGSrcNodeID());

    /// Lock
    /// todo: we only consider all cxtstmt of one instruction in one lock span,
    /// otherwise we think this instruction is not locked
    /// This constrait is too strong. All cxt lock under different cxt cannot be identified.


    if (ADDEDGE_NOLOCK!=Options::AddModelFlag() && lockana->isProtectedByCommonLock(i1, i2))
    {
        if (isTailofSpan(n1) && isHeadofSpan(n2))
            addTDEdges(n1->getId(), n2->getId(), pts);
    }
    else
    {
        addTDEdges(n1->getId(), n2->getId(), pts);
    }
}



void MTASVFGBuilder::handleStoreStore(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    const SVFInstruction* i1 = n1->getInst();
    const SVFInstruction* i2 = n2->getInst();
    /// MHP
    if (ADDEDGE_NOMHP!=Options::AddModelFlag() && !mhp->mayHappenInParallel(i1, i2))
        return;
    /// Alias
    if (ADDEDGE_NOALIAS!=Options::AddModelFlag() && !pta->alias(n1->getPAGDstNodeID(), n2->getPAGDstNodeID()))
        return;

    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
    pts &= pta->getPts(n2->getPAGDstNodeID());

    /// Lock
    if (ADDEDGE_NOLOCK!=Options::AddModelFlag() && lockana->isProtectedByCommonLock(i1, i2))
    {
        if (isTailofSpan(n1) && isHeadofSpan(n2))
            addTDEdges(n1->getId(), n2->getId(), pts);
        if (isTailofSpan(n2) && isHeadofSpan(n1))
            addTDEdges(n2->getId(), n1->getId(), pts);
    }
    else
    {
        addTDEdges(n1->getId(), n2->getId(), pts);
        addTDEdges(n2->getId(), n1->getId(), pts);
    }
}

void MTASVFGBuilder::handleStoreLoadWithLockPrecisely(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    if (!pta->alias(n1->getPAGDstNodeID(), n2->getPAGSrcNodeID()))
        return;

//    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
//    pts &= pta->getPts(n2->getPAGSrcNodeID());
//
//    const SVFInstruction* i1 = n1->getInst();
//    const SVFInstruction* i2 = n2->getInst();
//
//    NodeBS comlocks1;
//    NodeBS comlocks2;
//    lockana->getCommonLocks(i1, i2, comlocks1, comlocks2);
//
//    outs()<<comlocks1.count() << "  "<< comlocks2.count()<<"\n";


//    if(comlocks1.count() && comlocks2.count()) {
//        bool n1istail = false;
//        for (NodeBS::iterator it1 = comlocks1.begin(), ie1 = comlocks1.end(); it1 != ie1; ++it1) {
//            LockAnalysis::LockSpan lspan1 = lockana->getSpanfromCxtLock(*it1);
//            /// exist lock span, n1 is tail;
//            if (isTailofSpan(n1, lspan1))
//                n1istail = true;
//        }
//
//        bool n2ishead = false;
//        for (NodeBS::iterator it2 = comlocks2.begin(), ie2 = comlocks2.end(); it2 != ie2; ++it2) {
//            LockAnalysis::LockSpan lspan2 = lockana->getSpanfromCxtLock(*it2);
//            /// exist lock span, n2 is head;
//            if (isHeadofSpan(n2, lspan2))
//                n2ishead = true;
//        }
//
//        if (n1istail && n2ishead)
//            addTDEdges(n1->getId(), n2->getId(), pts);
//    } else {
//        addTDEdges(n1->getId(), n2->getId(), pts);
//    }
}
void MTASVFGBuilder::handleStoreStoreWithLockPrecisely(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    if (!pta->alias(n1->getPAGDstNodeID(), n2->getPAGDstNodeID()))
        return;

//    PointsTo pts = pta->getPts(n1->getPAGDstNodeID());
//    pts &= pta->getPts(n2->getPAGDstNodeID());
//
//    const SVFInstruction* i1 = n1->getInst();
//    const SVFInstruction* i2 = n2->getInst();


//    NodeBS comlocks1;
//    NodeBS comlocks2;
//    lockana->getCommonLocks(i1, i2, comlocks1, comlocks2);
//    if(comlocks1.count() && comlocks2.count()) {
//        bool n1istail = false;
//        for (NodeBS::iterator it1 = comlocks1.begin(), ie1 = comlocks1.end(); it1 != ie1; ++it1) {
//            LockAnalysis::LockSpan lspan1 = lockana->getSpanfromCxtLock(*it1);
//            /// exist lock span, n1 is tail;
//            if (isTailofSpan(n1, lspan1))
//                n1istail = true;
//        }
//        bool n2ishead = false;
//        for (NodeBS::iterator it2 = comlocks2.begin(), ie2 = comlocks2.end(); it2 != ie2; ++it2) {
//            LockAnalysis::LockSpan lspan2 = lockana->getSpanfromCxtLock(*it2);
//            /// exist lock span, n2 is head;
//            if (isHeadofSpan(n2, lspan2))
//                n2ishead = true;
//        }
//        if (n1istail && n2ishead)
//            addTDEdges(n1->getId(), n2->getId(), pts);
//
//
//
//        bool n2istail = false;
//        for (NodeBS::iterator it2 = comlocks2.begin(), ie2 = comlocks2.end(); it2 != ie2; ++it2) {
//            LockAnalysis::LockSpan lspan2 = lockana->getSpanfromCxtLock(*it2);
//            /// exist lock span, n2 is tail;
//            if (isTailofSpan(n2, lspan2))
//                n2istail = true;
//        }
//        bool n1ishead = false;
//        for (NodeBS::iterator it1 = comlocks1.begin(), ie1 = comlocks1.end(); it1 != ie1; ++it1) {
//            LockAnalysis::LockSpan lspan1 = lockana->getSpanfromCxtLock(*it1);
//            /// exist lock span, n1 is head;
//            if (isHeadofSpan(n1, lspan1))
//                n1ishead = true;
//        }
//        if (n2istail && n1ishead)
//            addTDEdges(n2->getId(), n1->getId(), pts);
//
//    } else {
//        addTDEdges(n1->getId(), n2->getId(), pts);
//        addTDEdges(n2->getId(), n1->getId(), pts);
//    }
}


void MTASVFGBuilder::mergeSpan(NodeBS comlocks, InstSet& res)
{
//    for (NodeBS::iterator it = comlocks.begin(), ie = comlocks.end(); it != ie; ++it) {
//        LockAnalysis::LockSpan lspan = lockana->getSpanfromCxtLock(*it);
//        for (LockAnalysis::LockSpan::const_iterator cts = lspan.begin(), ects = lspan.end(); cts!=ects; ++cts) {
//            res.insert((*cts).getStmt());
//        }
//    }
}

/// For o,  n2-o->n1, n1 and n2 are write. Foreach n3:n1->n3, n2->n3; then remove n2->n1.
void MTASVFGBuilder::readPrecision()
{

    recordedges.clear();
    edge2pts.clear();

    for (SVFGNodeSet::iterator it1 = stnodeSet.begin(), eit1 = stnodeSet.end(); it1 != eit1; ++it1)
    {
        const StmtSVFGNode* n1 = SVFUtil::cast<StmtSVFGNode>(*it1);

        for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n1->InEdgeBegin(); iter != n1->InEdgeEnd(); ++iter)
        {
            SVFGEdge* edge = *iter;
            if (edge->isIndirectVFGEdge() && SVFUtil::isa<StoreSVFGNode>(edge->getSrcNode()))
            {
                const StmtSVFGNode* n2 = SVFUtil::cast<StmtSVFGNode>(edge->getSrcNode());

                IndirectSVFGEdge* e = SVFUtil::cast<IndirectSVFGEdge>(edge);
                NodeBS pts = e->getPointsTo();
                PointsTo remove_pts;

                for (NodeBS::iterator o = pts.begin(), eo = pts.end(); o != eo; ++o)
                {
                    SVFGNodeIDSet succ1 = getSuccNodes(n1, *o);
                    SVFGNodeIDSet succ2 = getSuccNodes(n2, *o);

                    bool remove = true;
                    for (SVFGNodeIDSet::iterator sn1 = succ1.begin(), esn1 = succ1.end(); sn1 != esn1; sn1++)
                    {
                        if (!succ2.test(*sn1))
                        {
                            remove = false;
                            break;
                        }
                    }
                    if (remove)
                        remove_pts.set(*o);
                }

                if (remove_pts.count())
                    recordRemovingEdge(n2->getId(), n1->getId(), remove_pts);
            }
        }
    }

    performRemovingMHPEdges();
}

void MTASVFGBuilder::connectMHPEdges(PointerAnalysis* pta)
{
    PCG* pcg = new PCG(pta);
    if ((ADDEDGE_NONSPARSE==Options::AddModelFlag()) && Options::UsePCG())
    {
        pcg->analyze();
    }
    collectLoadStoreSVFGNodes();
    recordedges.clear();
    edge2pts.clear();

    /// todo: we ignore rule 2 and 3. but so far I haven't added intra-thread value flow affected by fork
    /// and inter-thread value flow affected by join
    for (SVFGNodeSet::const_iterator it1 = stnodeSet.begin(), eit1 =  stnodeSet.end(); it1!=eit1; ++it1)
    {
        const StmtSVFGNode* n1 = SVFUtil::cast<StmtSVFGNode>(*it1);
        const SVFInstruction* i1 = n1->getInst();

        for (SVFGNodeSet::const_iterator it2 = ldnodeSet.begin(), eit2 = ldnodeSet.end(); it2 != eit2; ++it2)
        {
            const StmtSVFGNode* n2 = SVFUtil::cast<StmtSVFGNode>(*it2);
            const SVFInstruction* i2 = n2->getInst();
            if (ADDEDGE_NONSPARSE==Options::AddModelFlag())
            {
                if (Options::UsePCG())
                {
                    if (pcg->mayHappenInParallel(i1, i2) || mhp->mayHappenInParallel(i1, i2))
                        handleStoreLoadNonSparse(n1, n2, pta);
                }
                else
                {
                    handleStoreLoadNonSparse(n1, n2, pta);
                }
            }
            else
            {
                handleStoreLoad(n1, n2, pta);
            }
        }

        for (SVFGNodeSet::const_iterator it2 = std::next(it1), eit2 =  stnodeSet.end(); it2!=eit2; ++it2)
        {
            const StmtSVFGNode* n2 = SVFUtil::cast<StmtSVFGNode>(*it2);
            const SVFInstruction* i2 = n2->getInst();
            if (ADDEDGE_NONSPARSE == Options::AddModelFlag())
            {
                if (Options::UsePCG())
                {
                    if(pcg->mayHappenInParallel(i1, i2) || mhp->mayHappenInParallel(i1, i2))
                        handleStoreStoreNonSparse(n1, n2, pta);
                }
                else
                {
                    handleStoreStoreNonSparse(n1, n2, pta);
                }
            }
            else
            {
                handleStoreStore(n1, n2, pta);
            }
        }
    }

    if(Options::ReadPrecisionTDEdge() && ADDEDGE_NORP!=Options::AddModelFlag())
    {
        DBOUT(DGENERAL,outs()<<"Read precision edge removing \n");
        DBOUT(DMTA,outs()<<"Read precision edge removing \n");
        readPrecision();
    }
}

/*!
 * Initialize analysis
 */
void FSMPTA::initialize(SVFModule* module)
{
    PointerAnalysis::initialize();

    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());
    MTASVFGBuilder mtaSVFGBuilder(mhp,lockana);
    svfg = mtaSVFGBuilder.buildPTROnlySVFG(ander);
    setGraph(svfg);
    //AndersenWaveDiff::releaseAndersenWaveDiff();

    stat = new FlowSensitiveStat(this);
}

