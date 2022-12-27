//===- SVFGOPT.h -- SVFG optimizer--------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * @file: SVFGOPT.h
 * @author: yesen
 * @date: 20/03/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#ifndef SVFGOPT_H_
#define SVFGOPT_H_


#include "Graphs/SVFG.h"
#include "Util/WorkList.h"

namespace SVF
{

/**
 * Optimised SVFG.
 * 1. FormalParam/ActualRet is converted into Phi. ActualParam/FormalRet becomes the
 *    operands of Phi nodes created at callee/caller's entry/callsite.
 * 2. ActualIns/ActualOuts resides at direct call sites id removed. Sources of its incoming
 *    edges are connected with the destinations of its outgoing edges directly.
 * 3. FormalIns/FormalOuts reside at the entry/exit of non-address-taken functions is
 *    removed as ActualIn/ActualOuts.
 * 4. MSSAPHI nodes are removed if it have no self cycle. Otherwise depends on user option.
 */
class SVFGOPT : public SVFG
{
    typedef Set<SVFGNode*> SVFGNodeSet;
    typedef Map<NodeID, NodeID> NodeIDToNodeIDMap;
    typedef FIFOWorkList<const MSSAPHISVFGNode*> WorkList;

public:
    /// Constructor
    SVFGOPT(std::unique_ptr<MemSSA> mssa, VFGK kind) : SVFG(std::move(mssa), kind)
    {
        keepAllSelfCycle = keepContextSelfCycle = keepActualOutFormalIn = false;
    }
    /// Destructor
    ~SVFGOPT() override = default;

    inline void setTokeepActualOutFormalIn()
    {
        keepActualOutFormalIn = true;
    }
    inline void setTokeepAllSelfCycle()
    {
        keepAllSelfCycle = true;
    }
    inline void setTokeepContextSelfCycle()
    {
        keepContextSelfCycle = true;
    }

protected:
    void buildSVFG() override;

    /// Connect SVFG nodes between caller and callee for indirect call sites
    //@{
    inline void connectAParamAndFParam(const PAGNode* cs_arg, const PAGNode* fun_arg, const CallICFGNode*, CallSiteID csId, SVFGEdgeSetTy& edges) override
    {
        NodeID phiId = getDef(fun_arg);
        SVFGEdge* edge = addCallEdge(getDef(cs_arg), phiId, csId);
        if (edge != nullptr)
        {
            PHISVFGNode* phi = SVFUtil::cast<PHISVFGNode>(getSVFGNode(phiId));
            addInterPHIOperands(phi, cs_arg);
            edges.insert(edge);
        }
    }
    /// Connect formal-ret and actual ret
    inline void connectFRetAndARet(const PAGNode* fun_ret, const PAGNode* cs_ret, CallSiteID csId, SVFGEdgeSetTy& edges) override
    {
        NodeID phiId = getDef(cs_ret);
        NodeID retdef = getDef(fun_ret);
        /// If a function does not have any return instruction. The def of a FormalRetVFGNode is itself (see VFG.h: addFormalRetVFGNode).
        /// Therefore, we do not connect return edge from a function without any return instruction (i.e., pag->isPhiNode(fun_ret)==false)
        /// because unique fun_ret PAGNode was not collected as a PhiNode in SVFIRBuilder::visitReturnInst
        if (pag->isPhiNode(fun_ret)==false)
            return;

        SVFGEdge* edge = addRetEdge(retdef, phiId, csId);
        if (edge != nullptr)
        {
            PHISVFGNode* phi = SVFUtil::cast<PHISVFGNode>(getSVFGNode(phiId));
            addInterPHIOperands(phi, fun_ret);
            edges.insert(edge);
        }
    }
    /// Connect actual-in and formal-in
    inline void connectAInAndFIn(const ActualINSVFGNode* actualIn, const FormalINSVFGNode* formalIn, CallSiteID csId, SVFGEdgeSetTy& edges) override
    {
        NodeBS intersection = actualIn->getPointsTo();
        intersection &= formalIn->getPointsTo();
        if (intersection.empty() == false)
        {
            NodeID aiDef = getActualINDef(actualIn->getId());
            SVFGEdge* edge = addCallIndirectSVFGEdge(aiDef,formalIn->getId(),csId,intersection);
            if (edge != nullptr)
                edges.insert(edge);
        }
    }
    /// Connect formal-out and actual-out
    inline void connectFOutAndAOut(const FormalOUTSVFGNode* formalOut, const ActualOUTSVFGNode* actualOut, CallSiteID csId, SVFGEdgeSetTy& edges) override
    {
        NodeBS intersection = formalOut->getPointsTo();
        intersection &= actualOut->getPointsTo();
        if (intersection.empty() == false)
        {
            NodeID foDef = getFormalOUTDef(formalOut->getId());
            SVFGEdge* edge = addRetIndirectSVFGEdge(foDef,actualOut->getId(),csId,intersection);
            if (edge != nullptr)
                edges.insert(edge);
        }
    }
    //@}

    /// Get def-site of actual-in/formal-out.
    //@{
    inline NodeID getActualINDef(NodeID ai) const
    {
        NodeIDToNodeIDMap::const_iterator it = actualInToDefMap.find(ai);
        assert(it != actualInToDefMap.end() && "can not find actual-in's def");
        return it->second;
    }
    inline NodeID getFormalOUTDef(NodeID fo) const
    {
        NodeIDToNodeIDMap::const_iterator it = formalOutToDefMap.find(fo);
        assert(it != formalOutToDefMap.end() && "can not find formal-out's def");
        return it->second;
    }
    //@}

private:
    void parseSelfCycleHandleOption();

    /// Add inter-procedural value flow edge
    //@{
    /// Add indirect call edge from src to dst with one call site ID.
    SVFGEdge* addCallIndirectSVFGEdge(NodeID srcId, NodeID dstId, CallSiteID csid, const NodeBS& cpts);
    /// Add indirect ret edge from src to dst with one call site ID.
    SVFGEdge* addRetIndirectSVFGEdge(NodeID srcId, NodeID dstId, CallSiteID csid, const NodeBS& cpts);
    //@}

    /// 1. Convert FormalParmSVFGNode into PHISVFGNode and add all ActualParmSVFGNoe which may
    /// propagate pts to it as phi's operands.
    /// 2. Do the same thing for ActualRetSVFGNode and FormalRetSVFGNode.
    /// 3. Record def site of ActualINSVFGNode. Remove all its edges and connect its predecessors
    ///    and successors.
    /// 4. Do the same thing for FormalOUTSVFGNode as 3.
    /// 5. Remove ActualINSVFGNode/FormalINSVFGNode/ActualOUTSVFGNode/FormalOUTSVFGNode if they
    ///    will not be used when updating call graph.
    void handleInterValueFlow();

    /// Replace FormalParam/ActualRet node with PHI node.
    //@{
    void replaceFParamARetWithPHI(PHISVFGNode* phi, SVFGNode* svfgNode);
    //@}

    /// Retarget edges related to actual-in/-out and formal-in/-out.
    //@{
    /// Record def sites of actual-in/formal-out and connect from those def-sites
    /// to formal-in/actual-out directly if they exist.
    void retargetEdgesOfAInFOut(SVFGNode* node);
    /// Connect actual-out/formal-in's predecessors to their successors directly.
    void retargetEdgesOfAOutFIn(SVFGNode* node);
    //@}

    /// Remove MSSAPHI SVFG nodes.
    void handleIntraValueFlow();

    /// Initial work list with MSSAPHI nodes which may be removed.
    inline void initialWorkList()
    {
        for (SVFG::const_iterator it = begin(), eit = end(); it != eit; ++it)
            addIntoWorklist(it->second);
    }

    /// Only MSSAPHI node which satisfy following conditions will be removed:
    /// 1. it's not def-site of actual-in/formal-out;
    /// 2. it doesn't have incoming and outgoing call/ret at the same time.
    inline bool addIntoWorklist(const SVFGNode* node)
    {
        if (const MSSAPHISVFGNode* phi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            if (isConnectingTwoCallSites(phi) == false && isDefOfAInFOut(phi) == false)
                return worklist.push(phi);
        }
        return false;
    }

    /// Remove MSSAPHI node if possible
    void bypassMSSAPHINode(const MSSAPHISVFGNode* node);

    /// Remove self cycle edges if needed. Return TRUE if some self cycle edges remained.
    bool checkSelfCycleEdges(const MSSAPHISVFGNode* node);

    /// Add new SVFG edge from src to dst.
    bool addNewSVFGEdge(NodeID srcId, NodeID dstId, const SVFGEdge* preEdge, const SVFGEdge* succEdge);

    /// Return TRUE if both edges are indirect call/ret edges.
    inline bool bothInterEdges(const SVFGEdge* edge1, const SVFGEdge* edge2) const
    {
        bool inter1 = SVFUtil::isa<CallIndSVFGEdge, RetIndSVFGEdge>(edge1);
        bool inter2 = SVFUtil::isa<CallIndSVFGEdge, RetIndSVFGEdge>(edge2);
        return (inter1 && inter2);
    }

    inline void addInterPHIOperands(PHISVFGNode* phi, const PAGNode* operand)
    {
        phi->setOpVer(phi->getOpVerNum(), operand);
    }

    /// Add inter PHI SVFG node for formal parameter
    inline InterPHISVFGNode* addInterPHIForFP(const FormalParmSVFGNode* fp)
    {
        InterPHISVFGNode* sNode = new InterPHISVFGNode(totalVFGNode++,fp);
        addSVFGNode(sNode, pag->getICFG()->getFunEntryICFGNode(fp->getFun()));
        resetDef(fp->getParam(),sNode);
        return sNode;
    }
    /// Add inter PHI SVFG node for actual return
    inline InterPHISVFGNode* addInterPHIForAR(const ActualRetSVFGNode* ar)
    {
        InterPHISVFGNode* sNode = new InterPHISVFGNode(totalVFGNode++,ar);
        addSVFGNode(sNode, pag->getICFG()->getRetICFGNode(ar->getCallSite()->getCallSite()));
        resetDef(ar->getRev(),sNode);
        return sNode;
    }

    inline void resetDef(const PAGNode* pagNode, const SVFGNode* node)
    {
        PAGNodeToDefMapTy::iterator it = PAGNodeToDefMap.find(pagNode);
        assert(it != PAGNodeToDefMap.end() && "a SVFIR node doesn't have definition before");
        it->second = node->getId();
    }

    /// Set def-site of actual-in/formal-out.
    ///@{
    inline void setActualINDef(NodeID ai, NodeID def)
    {
        bool inserted = actualInToDefMap.emplace(ai, def).second;
        (void)inserted; // Suppress warning of unused variable under release build
        assert(inserted && "can not set actual-in's def twice");
        defNodes.set(def);
    }
    inline void setFormalOUTDef(NodeID fo, NodeID def)
    {
        bool inserted = formalOutToDefMap.emplace(fo, def).second;
        (void) inserted;
        assert(inserted && "can not set formal-out's def twice");
        defNodes.set(def);
    }
    ///@}

    inline bool isDefOfAInFOut(const SVFGNode* node)
    {
        return defNodes.test(node->getId());
    }

    /// Check if actual-in/actual-out exist at indirect call site.
    //@{
    inline bool actualInOfIndCS(const ActualINSVFGNode* ai) const
    {
        return (SVFIR::getPAG()->isIndirectCallSites(ai->getCallSite()));
    }
    inline bool actualOutOfIndCS(const ActualOUTSVFGNode* ao) const
    {
        return (SVFIR::getPAG()->isIndirectCallSites(ao->getCallSite()));
    }
    //@}

    /// Check if formal-in/formal-out reside in address-taken function.
    //@{
    inline bool formalInOfAddressTakenFunc(const FormalINSVFGNode* fi) const
    {
        return (fi->getFun()->hasAddressTaken());
    }
    inline bool formalOutOfAddressTakenFunc(const FormalOUTSVFGNode* fo) const
    {
        return (fo->getFun()->hasAddressTaken());
    }
    //@}

    /// Return TRUE if this node has both incoming call/ret and outgoing call/ret edges.
    bool isConnectingTwoCallSites(const SVFGNode* node) const;

    /// Return TRUE if this SVFGNode can be removed.
    /// Nodes can be removed if it is:
    /// 1. ActualParam/FormalParam/ActualRet/FormalRet
    /// 2. ActualIN if it doesn't reside at indirect call site
    /// 3. FormalIN if it doesn't reside at the entry of address-taken function and it's not
    ///    definition site of ActualIN
    /// 4. ActualOUT if it doesn't reside at indirect call site and it's not definition site
    ///    of FormalOUT
    /// 5. FormalOUT if it doesn't reside at the exit of address-taken function
    bool canBeRemoved(const SVFGNode * node);

    /// Remove edges of a SVFG node
    //@{
    inline void removeAllEdges(const SVFGNode* node)
    {
        removeInEdges(node);
        removeOutEdges(node);
    }
    inline void removeInEdges(const SVFGNode* node)
    {
        /// remove incoming edges
        while (node->hasIncomingEdge())
            removeSVFGEdge(*(node->InEdgeBegin()));
    }
    inline void removeOutEdges(const SVFGNode* node)
    {
        while (node->hasOutgoingEdge())
            removeSVFGEdge(*(node->OutEdgeBegin()));
    }
    //@}


    NodeIDToNodeIDMap actualInToDefMap;	///< map actual-in to its def-site node
    NodeIDToNodeIDMap formalOutToDefMap;	///< map formal-out to its def-site node
    NodeBS defNodes;	///< preserved def nodes of formal-in/actual-out

    WorkList worklist;	///< storing MSSAPHI nodes which may be removed.

    bool keepActualOutFormalIn;
    bool keepAllSelfCycle;
    bool keepContextSelfCycle;
};

} // End namespace SVF

#endif /* SVFGOPT_H_ */
