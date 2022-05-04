//===- SVFGOPT.cpp -- SVFG optimizer------------------------------------------//
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
 * @file: SVFGOPT.cpp
 * @author: yesen
 * @date: 31/03/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */

#include "Util/Options.h"
#include "Graphs/SVFGOPT.h"
#include "Graphs/SVFGStat.h"

using namespace SVF;
using namespace SVFUtil;

static std::string KeepAllSelfCycle = "all";
static std::string KeepContextSelfCycle = "context";
static std::string KeepNoneSelfCycle = "none";


void SVFGOPT::buildSVFG()
{
    SVFG::buildSVFG();

    if(Options::DumpVFG)
        dump("SVFG_before_opt");

    DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("\tSVFG Optimisation\n"));

    keepActualOutFormalIn = Options::KeepAOFI;

    stat->sfvgOptStart();
    handleInterValueFlow();

    handleIntraValueFlow();
    stat->sfvgOptEnd();

}
/*!
 *
 */
SVFGEdge* SVFGOPT::addCallIndirectSVFGEdge(NodeID srcId, NodeID dstId, CallSiteID csid, const NodeBS& cpts)
{
    if (Options::ContextInsensitive)
        return addIntraIndirectVFEdge(srcId, dstId, cpts);
    else
        return addCallIndirectVFEdge(srcId, dstId, cpts, csid);
}

/*!
 *
 */
SVFGEdge* SVFGOPT::addRetIndirectSVFGEdge(NodeID srcId, NodeID dstId, CallSiteID csid, const NodeBS& cpts)
{
    if (Options::ContextInsensitive)
        return addIntraIndirectVFEdge(srcId, dstId, cpts);
    else
        return addRetIndirectVFEdge(srcId, dstId, cpts, csid);
}

/*!
 *
 */
void SVFGOPT::handleInterValueFlow()
{
    SVFGNodeSet candidates;
    for (SVFGNodeIDToNodeMapTy::iterator it = SVFG::begin(), eit = SVFG::end();
            it!=eit; ++it)
    {
        SVFGNode* node = it->second;
        if (SVFUtil::isa<ActualParmSVFGNode>(node) || SVFUtil::isa<ActualRetSVFGNode>(node)
                || SVFUtil::isa<FormalParmSVFGNode>(node) || SVFUtil::isa<FormalRetSVFGNode>(node)
                || SVFUtil::isa<ActualINSVFGNode>(node) || SVFUtil::isa<ActualOUTSVFGNode>(node)
                || SVFUtil::isa<FormalINSVFGNode>(node) || SVFUtil::isa<FormalOUTSVFGNode>(node))
            candidates.insert(node);
    }

    SVFGNodeSet nodesToBeDeleted;
    for (SVFGNodeSet::const_iterator it = candidates.begin(), eit = candidates.end();
            it!=eit; ++it)
    {
        SVFGNode* node = *it;
        if (FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node))
        {
            replaceFParamARetWithPHI(addInterPHIForFP(fp), fp);
            nodesToBeDeleted.insert(fp);
        }
        else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node))
        {
            replaceFParamARetWithPHI(addInterPHIForAR(ar), ar);
            nodesToBeDeleted.insert(ar);
        }
        else if (SVFUtil::isa<ActualParmSVFGNode>(node) || SVFUtil::isa<FormalRetSVFGNode>(node))
        {
            nodesToBeDeleted.insert(node);
        }
        else if (SVFUtil::isa<ActualINSVFGNode>(node) || SVFUtil::isa<FormalOUTSVFGNode>(node))
        {
            retargetEdgesOfAInFOut(node);
            nodesToBeDeleted.insert(node);
        }
        else if (SVFUtil::isa<ActualOUTSVFGNode>(node) || SVFUtil::isa<FormalINSVFGNode>(node))
        {
            if(keepActualOutFormalIn == false)
                nodesToBeDeleted.insert(node);
        }
    }

    for (SVFGNodeSet::iterator it = nodesToBeDeleted.begin(), eit = nodesToBeDeleted.end(); it != eit; ++it)
    {
        SVFGNode* node = *it;
        if (canBeRemoved(node))
        {
            if (SVFUtil::isa<ActualOUTSVFGNode>(node) || SVFUtil::isa<FormalINSVFGNode>(node))
                retargetEdgesOfAOutFIn(node);	/// reset def of address-taken variable

            removeAllEdges(node);
            removeSVFGNode(node);
        }
    }
}

/*!
 *
 */
void SVFGOPT::replaceFParamARetWithPHI(PHISVFGNode* phi, SVFGNode* svfgNode)
{
    assert((SVFUtil::isa<FormalParmSVFGNode>(svfgNode) || SVFUtil::isa<ActualRetSVFGNode>(svfgNode))
           && "expecting a formal param or actual ret svfg node");

    /// create a new PHISVFGNode.
    NodeID phiId = phi->getId();
    /// migrate formal-param's edges to phi node.
    for (SVFGNode::const_iterator it = svfgNode->OutEdgeBegin(), eit = svfgNode->OutEdgeEnd();
            it != eit; ++it)
    {
        const SVFGEdge* outEdge = *it;
        SVFGNode* dstNode = outEdge->getDstNode();
        NodeID dstId = dstNode->getId();
        if (const CallDirSVFGEdge* callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(outEdge))
            addCallEdge(phiId, dstId, callEdge->getCallSiteId());
        else if (const RetDirSVFGEdge* retEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(outEdge))
            addRetEdge(phiId, dstId, retEdge->getCallSiteId());
        else
            addIntraDirectVFEdge(phiId, dstId);
    }

    /// add actual-param/formal-ret into phi's operand list
    if (FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(svfgNode))
    {
        for (SVFGNode::iterator it = svfgNode->InEdgeBegin(), eit = svfgNode->InEdgeEnd();
                it != eit; ++it)
        {
            ActualParmSVFGNode* ap = SVFUtil::cast<ActualParmSVFGNode>((*it)->getSrcNode());
            addInterPHIOperands(phi, ap->getParam());
            // connect actual param's def node to phi node
            addCallEdge(getDef(ap->getParam()), phiId, getCallSiteID(ap->getCallSite(), fp->getFun()));
        }
    }
    else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(svfgNode))
    {
        for (SVFGNode::iterator it = svfgNode->InEdgeBegin(), eit = svfgNode->InEdgeEnd();
                it != eit; ++it)
        {
            FormalRetSVFGNode* fr = SVFUtil::cast<FormalRetSVFGNode>((*it)->getSrcNode());
            addInterPHIOperands(phi, fr->getRet());
            // connect formal return's def node to phi node
            addRetEdge(getDef(fr->getRet()), phiId, getCallSiteID(ar->getCallSite(), fr->getFun()));
        }
    }

    removeAllEdges(svfgNode);
}

/*!
 * Record def sites of actual-in/formal-out and connect from those def-sites
 * to formal-in/actual-out directly if they exist.
 */
void SVFGOPT::retargetEdgesOfAInFOut(SVFGNode* node)
{
    assert(node->getInEdges().size() == 1 && "actual-in/formal-out can only have one incoming edge as its def size");

    SVFGNode* def = nullptr;
    NodeBS inPointsTo;

    SVFGNode::const_iterator it = node->InEdgeBegin();
    SVFGNode::const_iterator eit = node->InEdgeEnd();
    for (; it != eit; ++it)
    {
        const IndirectSVFGEdge* inEdge = SVFUtil::cast<IndirectSVFGEdge>(*it);
        inPointsTo = inEdge->getPointsTo();

        def = inEdge->getSrcNode();
        if (SVFUtil::isa<ActualINSVFGNode>(node))
            setActualINDef(node->getId(), def->getId());
        else if (SVFUtil::isa<FormalOUTSVFGNode>(node))
            setFormalOUTDef(node->getId(), def->getId());
    }

    it = node->OutEdgeBegin(), eit = node->OutEdgeEnd();
    for (; it != eit; ++it)
    {
        const IndirectSVFGEdge* outEdge = SVFUtil::cast<IndirectSVFGEdge>(*it);
        NodeBS intersection = inPointsTo;
        intersection &= outEdge->getPointsTo();

        if (intersection.empty())
            continue;

        SVFGNode* dstNode = outEdge->getDstNode();
        if (const CallIndSVFGEdge* callEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(outEdge))
            addCallIndirectSVFGEdge(def->getId(), dstNode->getId(), callEdge->getCallSiteId(), intersection);
        else if (const RetIndSVFGEdge* retEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(outEdge))
            addRetIndirectSVFGEdge(def->getId(), dstNode->getId(), retEdge->getCallSiteId(), intersection);
        else
            assert(false && "expecting an inter-procedural SVFG edge");
    }

    removeAllEdges(node);
}

/*!
 *
 */
void SVFGOPT::retargetEdgesOfAOutFIn(SVFGNode* node)
{
    SVFGNode::const_iterator inIt = node->InEdgeBegin();
    SVFGNode::const_iterator inEit = node->InEdgeEnd();
    for (; inIt != inEit; ++inIt)
    {
        const IndirectSVFGEdge* inEdge = SVFUtil::cast<IndirectSVFGEdge>(*inIt);
        NodeID srcId = inEdge->getSrcID();

        SVFGNode::const_iterator outIt = node->OutEdgeBegin();
        SVFGNode::const_iterator outEit = node->OutEdgeEnd();
        for (; outIt != outEit; ++outIt)
        {
            const IndirectSVFGEdge* outEdge = SVFUtil::cast<IndirectSVFGEdge>(*outIt);

            NodeBS intersection = inEdge->getPointsTo();
            intersection &= outEdge->getPointsTo();
            if (intersection.empty())
                continue;

            NodeID dstId = outEdge->getDstID();
            if (const RetIndSVFGEdge* retEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(inEdge))
            {
                addRetIndirectSVFGEdge(srcId, dstId, retEdge->getCallSiteId(), intersection);
            }
            else if (const CallIndSVFGEdge* callEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(inEdge))
            {
                addCallIndirectSVFGEdge(srcId, dstId, callEdge->getCallSiteId(), intersection);
            }
            else
            {
                addIntraIndirectVFEdge(srcId, dstId, intersection);
            }
        }
    }

    removeAllEdges(node);
}

/*!
 *
 */
bool SVFGOPT::isConnectingTwoCallSites(const SVFGNode* node) const
{
    bool hasInCallRet = false;
    bool hasOutCallRet = false;

    SVFGNode::const_iterator edgeIt = node->InEdgeBegin();
    SVFGNode::const_iterator edgeEit = node->InEdgeEnd();
    for (; edgeIt != edgeEit; ++edgeIt)
    {
        if (SVFUtil::isa<CallIndSVFGEdge>(*edgeIt) || SVFUtil::isa<RetIndSVFGEdge>(*edgeIt))
        {
            hasInCallRet = true;
            break;
        }
    }

    edgeIt = node->OutEdgeBegin();
    edgeEit = node->OutEdgeEnd();
    for (; edgeIt != edgeEit; ++edgeIt)
    {
        if (SVFUtil::isa<CallIndSVFGEdge>(*edgeIt) || SVFUtil::isa<RetIndSVFGEdge>(*edgeIt))
        {
            hasOutCallRet = true;
            break;
        }
    }

    if (hasInCallRet && hasOutCallRet)
        return true;
    else
        return false;
}

/// Return TRUE if this SVFGNode can be removed.
/// Nodes can be removed if it is:
/// 1. ActualParam/FormalParam/ActualRet/FormalRet
/// 2. ActualIN if it doesn't reside at indirect call site
/// 3. FormalIN if it doesn't reside at the entry of address-taken function and it's not
///    definition site of ActualIN
/// 4. ActualOUT if it doesn't reside at indirect call site and it's not definition site
///    of FormalOUT
/// 5. FormalOUT if it doesn't reside at the exit of address-taken function
bool SVFGOPT::canBeRemoved(const SVFGNode * node)
{
    if (SVFUtil::isa<ActualParmSVFGNode>(node) || SVFUtil::isa<FormalParmSVFGNode>(node)
            || SVFUtil::isa<ActualRetSVFGNode>(node) || SVFUtil::isa<FormalRetSVFGNode>(node))
        return true;
    else if (SVFUtil::isa<ActualINSVFGNode>(node) || SVFUtil::isa<ActualOUTSVFGNode>(node)
             || SVFUtil::isa<FormalINSVFGNode>(node) || SVFUtil::isa<FormalOUTSVFGNode>(node)
             || SVFUtil::isa<MSSAPHISVFGNode>(node))
    {
        /// Now each SVFG edge can only be associated with one call site id,
        /// so if this node has both incoming call/ret and outgoting call/ret
        /// edges, we don't remove this node.
        if (isConnectingTwoCallSites(node))
            return false;

        if (const ActualINSVFGNode* ai = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            return (actualInOfIndCS(ai) == false);
        }
        else if (const ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
        {
            return (actualOutOfIndCS(ao) == false && isDefOfAInFOut(node) == false);
        }
        else if (const FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            return (formalInOfAddressTakenFunc(fi) == false && isDefOfAInFOut(node) == false);
        }
        else if (const FormalOUTSVFGNode* fo = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            return (formalOutOfAddressTakenFunc(fo) == false);
        }
    }

    return false;
}

/*!
 *
 */
void SVFGOPT::parseSelfCycleHandleOption()
{
    std::string choice = (Options::SelfCycle.getValue().empty()) ? "" : Options::SelfCycle.getValue();
    if (choice.empty() || choice == KeepAllSelfCycle)
        keepAllSelfCycle = true;
    else if (choice == KeepContextSelfCycle)
        keepContextSelfCycle = true;
    else if (choice == KeepNoneSelfCycle)
        keepAllSelfCycle = keepContextSelfCycle = false;
    else
    {
        SVFUtil::writeWrnMsg("Unrecognised option. All self cycle edges will be kept.");
        keepAllSelfCycle = true;
    }
}

/*!
 *  Remove MSSAPHI SVFG nodes.
 */
void SVFGOPT::handleIntraValueFlow()
{
    parseSelfCycleHandleOption();

    initialWorkList();

    while (!worklist.empty())
    {
        const MSSAPHISVFGNode* node = worklist.pop();

        /// Skip nodes which have self cycle
        if (checkSelfCycleEdges(node))
            continue;

        if (node->hasOutgoingEdge() && node->hasIncomingEdge())
            bypassMSSAPHINode(node);

        /// remove node's edges if it only has incoming or outgoing edges.
        if (node->hasIncomingEdge() && node->hasOutgoingEdge() == false)
        {
            /// remove all the incoming edges;
            SVFGNode::const_iterator edgeIt = node->InEdgeBegin();
            SVFGNode::const_iterator edgeEit = node->InEdgeEnd();
            for (; edgeIt != edgeEit; ++edgeIt)
                addIntoWorklist((*edgeIt)->getSrcNode());

            removeInEdges(node);
        }
        else if (node->hasOutgoingEdge() && node->hasIncomingEdge() == false)
        {
            /// remove all the outgoing edges;
            SVFGNode::const_iterator edgeIt = node->OutEdgeBegin();
            SVFGNode::const_iterator edgeEit = node->OutEdgeEnd();
            for (; edgeIt != edgeEit; ++edgeIt)
                addIntoWorklist((*edgeIt)->getDstNode());

            removeOutEdges(node);
        }

        /// remove this node if it has no edges
        if (node->hasIncomingEdge() == false && node->hasOutgoingEdge() == false)
            removeSVFGNode(const_cast<MSSAPHISVFGNode*>(node));
    }
}


/// Remove self cycle edges according to specified options:
/// 1. keepAllSelfCycle = TRUE: all self cycle edges are kept;
/// 2. keepContextSelfCycle = TRUE: all self cycle edges related-to context are kept;
/// 3. Otherwise, all self cycle edges are NOT kept.
/// Return TRUE if some self cycle edges remaine in this node.
bool SVFGOPT::checkSelfCycleEdges(const MSSAPHISVFGNode* node)
{
    bool hasSelfCycle = false;

    SVFGEdge::SVFGEdgeSetTy inEdges = node->getInEdges();
    SVFGNode::const_iterator inEdgeIt = inEdges.begin();
    SVFGNode::const_iterator inEdgeEit = inEdges.end();
    for (; inEdgeIt != inEdgeEit; ++inEdgeIt)
    {
        SVFGEdge* preEdge = *inEdgeIt;

        if (preEdge->getSrcID() == preEdge->getDstID())
        {
            if (keepAllSelfCycle)
            {
                hasSelfCycle = true;
                break;	/// There's no need to check other edge if we do not remove self cycle
            }
            else if (keepContextSelfCycle &&
                     (SVFUtil::isa<CallIndSVFGEdge>(preEdge) || SVFUtil::isa<RetIndSVFGEdge>(preEdge)))
            {
                hasSelfCycle = true;
                continue;	/// Continue checking and remove other self cycle which are NOT context-related
            }
            else
            {
                assert(SVFUtil::isa<IndirectSVFGEdge>(preEdge) && "can only remove indirect SVFG edge");
                removeSVFGEdge(preEdge);
            }
        }
    }

    return hasSelfCycle;
}

/*!
 * Remove MSSAPHI node if possible
 */
void SVFGOPT::bypassMSSAPHINode(const MSSAPHISVFGNode* node)
{
    SVFGNode::const_iterator inEdgeIt = node->InEdgeBegin();
    SVFGNode::const_iterator inEdgeEit = node->InEdgeEnd();
    for (; inEdgeIt != inEdgeEit; ++inEdgeIt)
    {
        const SVFGEdge* preEdge = *inEdgeIt;
        const SVFGNode* srcNode = preEdge->getSrcNode();

        bool added = false;
        /// add new edges from predecessor to all successors.
        SVFGNode::const_iterator outEdgeIt = node->OutEdgeBegin();
        SVFGNode::const_iterator outEdgeEit = node->OutEdgeEnd();
        for (; outEdgeIt != outEdgeEit; ++outEdgeIt)
        {
            const SVFGEdge* succEdge = *outEdgeIt;
            const SVFGNode* dstNode = (*outEdgeIt)->getDstNode();
            if (srcNode->getId() != dstNode->getId()
                    && addNewSVFGEdge(srcNode->getId(), dstNode->getId(), preEdge, succEdge))
                added = true;
            else
            {
                /// if no new edge is added, the number of dst node's incoming edges may be decreased.
                /// try to analyze it again.
                addIntoWorklist(dstNode);
            }
        }

        if (added == false)
        {
            /// if no new edge is added, the number of src node's outgoing edges may be decreased.
            /// try to analyze it again.
            addIntoWorklist(srcNode);
        }
    }

    removeAllEdges(node);
}

/*!
 * Add new SVFG edge from src to dst.
 * The edge's kind depends on preEdge and succEdge. Self-cycle edges may be added here.
 */
bool SVFGOPT::addNewSVFGEdge(NodeID srcId, NodeID dstId, const SVFGEdge* preEdge, const SVFGEdge* succEdge)
{
    assert(SVFUtil::isa<IndirectSVFGEdge>(preEdge) && SVFUtil::isa<IndirectSVFGEdge>(succEdge)
           && "either pre or succ edge is not indirect SVFG edge");

    const IndirectSVFGEdge* preIndEdge = SVFUtil::cast<IndirectSVFGEdge>(preEdge);
    const IndirectSVFGEdge* succIndEdge = SVFUtil::cast<IndirectSVFGEdge>(succEdge);

    NodeBS intersection = preIndEdge->getPointsTo();
    intersection &= succIndEdge->getPointsTo();

    if (intersection.empty())
        return false;

    assert(bothInterEdges(preEdge, succEdge) == false && "both edges are inter edges");

    if (const CallIndSVFGEdge* preCallEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(preEdge))
    {
        return addCallIndirectSVFGEdge(srcId, dstId, preCallEdge->getCallSiteId(), intersection);
    }
    else if (const CallIndSVFGEdge* succCallEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(succEdge))
    {
        return addCallIndirectSVFGEdge(srcId, dstId, succCallEdge->getCallSiteId(), intersection);
    }
    else if (const RetIndSVFGEdge* preRetEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(preEdge))
    {
        return addRetIndirectSVFGEdge(srcId, dstId, preRetEdge->getCallSiteId(), intersection);
    }
    else if (const RetIndSVFGEdge* succRetEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(succEdge))
    {
        return addRetIndirectSVFGEdge(srcId, dstId, succRetEdge->getCallSiteId(), intersection);
    }
    else
    {
        return addIntraIndirectVFEdge(srcId, dstId, intersection);
    }

    return false;
}
