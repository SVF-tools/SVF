//===- VFG.h ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * VFG.h
 *
 *  Created on: 18 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_UTIL_VFG_H_
#define INCLUDE_UTIL_VFG_H_


#include "Graphs/VFGNode.h"
#include "Graphs/VFGEdge.h"

namespace SVF
{

class PointerAnalysis;
class VFGStat;
class CallBlockNode;

/*!
 * Interprocedural Control-Flow Graph (VFG)
 */
using GenericVFGTy = GenericGraph<VFGNode, VFGEdge>;
class VFG : public GenericVFGTy
{

public:
    /// VFG kind
    enum VFGK
    {
        ORIGSVFGK, PTRONLYSVFGK
    };

    using VFGNodeIDToNodeMapTy = Map<NodeID, VFGNode *>;
    using VFGNodeSet = Set<VFGNode *>;
    using PAGNodeToDefMapTy = Map<const PAGNode *, NodeID>;
    using PAGNodeToActualParmMapTy = Map<std::pair<NodeID, const CallBlockNode *>, ActualParmVFGNode *>;
    using PAGNodeToActualRetMapTy = Map<const PAGNode *, ActualRetVFGNode *>;
    using PAGNodeToFormalParmMapTy = Map<const PAGNode *, FormalParmVFGNode *>;
    using PAGNodeToFormalRetMapTy = Map<const PAGNode *, FormalRetVFGNode *>;
    using PAGEdgeToStmtVFGNodeMapTy = Map<const PAGEdge *, StmtVFGNode *>;
    using PAGNodeToPHIVFGNodeMapTy = Map<const PAGNode *, IntraPHIVFGNode *>;
    using PAGNodeToBinaryOPVFGNodeMapTy = Map<const PAGNode *, BinaryOPVFGNode *>;
    using PAGNodeToUnaryOPVFGNodeMapTy = Map<const PAGNode *, UnaryOPVFGNode *>;
    using PAGNodeToCmpVFGNodeMapTy = Map<const PAGNode *, CmpVFGNode *>;
    using FunToVFGNodesMapTy = Map<const SVFFunction *, VFGNodeSet>;

    using CallPESet = FormalParmVFGNode::CallPESet;
    using RetPESet = FormalRetVFGNode::RetPESet;
    using VFGEdgeSetTy = VFGEdge::VFGEdgeSetTy;
    using SVFGEdgeSetTy = VFGEdge::SVFGEdgeSetTy;
    using VFGNodeIter = VFGEdge::VFGEdgeSetTy::iterator;
    using iterator = VFGNodeIDToNodeMapTy::iterator;
    using const_iterator = VFGNodeIDToNodeMapTy::const_iterator;
    using PAGEdgeSet = PAG::PAGEdgeSet;
    using GlobalVFGNodeSet = Set<const VFGNode *>;
    using PAGNodeSet = Set<const PAGNode *>;


protected:
    NodeID totalVFGNode;
    PAGNodeToDefMapTy PAGNodeToDefMap;	///< map a pag node to its definition SVG node
    PAGNodeToActualParmMapTy PAGNodeToActualParmMap; ///< map a PAGNode to an actual parameter
    PAGNodeToActualRetMapTy PAGNodeToActualRetMap; ///< map a PAGNode to an actual return
    PAGNodeToFormalParmMapTy PAGNodeToFormalParmMap; ///< map a PAGNode to a formal parameter
    PAGNodeToFormalRetMapTy PAGNodeToFormalRetMap; ///< map a PAGNode to a formal return
    PAGNodeToPHIVFGNodeMapTy PAGNodeToIntraPHIVFGNodeMap;	///< map a PAGNode to its PHIVFGNode
    PAGNodeToBinaryOPVFGNodeMapTy PAGNodeToBinaryOPVFGNodeMap;	///< map a PAGNode to its BinaryOPVFGNode
    PAGNodeToUnaryOPVFGNodeMapTy PAGNodeToUnaryOPVFGNodeMap;	///< map a PAGNode to its UnaryOPVFGNode
    PAGNodeToCmpVFGNodeMapTy PAGNodeToCmpVFGNodeMap;	///< map a PAGNode to its CmpVFGNode
    PAGEdgeToStmtVFGNodeMapTy PAGEdgeToStmtVFGNodeMap;	///< map a PAGEdge to its StmtVFGNode
    FunToVFGNodesMapTy funToVFGNodesMap; ///< map a function to its VFGNodes;

    GlobalVFGNodeSet globalVFGNodes;	///< set of global store VFG nodes
    PTACallGraph* callgraph;
    PAG* pag;
    VFGK kind;
    bool dumpVFG;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    VFG(PTACallGraph* callgraph, VFGK k = ORIGSVFGK);

    /// Destructor
    virtual ~VFG()
    {
        destroy();
    }

    /// Get VFG kind
    inline VFGK getKind() const
    {
        return kind;
    }

    /// Return true if this VFG only contains pointer related SVFGNodes for pointer analysis
    inline bool isPtrOnlySVFG() const
    {
        return kind == PTRONLYSVFGK;
    }

	/// Whether to dump VFG;
	inline void setDumpVFG(bool flag)
	{
		dumpVFG = flag;
	}

	/// Whether to dump VFG;
	inline bool getDumpVFG() const
	{
		return dumpVFG;
	}

    /// Return PAG
    inline PAG* getPAG() const
    {
        return pag;
    }

    /// Return CallGraph
    inline PTACallGraph* getCallGraph() const
    {
        return callgraph;
    }

    /// Get a VFG node
    inline VFGNode* getVFGNode(NodeID id) const
    {
        return getGNode(id);
    }

    /// Whether has the VFGNode
    inline bool hasVFGNode(NodeID id) const
    {
        return hasGNode(id);
    }
    /// Return global stores
    inline GlobalVFGNodeSet& getGlobalVFGNodes()
    {
        return globalVFGNodes;
    }

    /// Get a SVFG edge according to src and dst
    VFGEdge* getIntraVFGEdge(const VFGNode* src, const VFGNode* dst, VFGEdge::VFGEdgeK kind);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Update VFG based on pointer analysis results
    void updateCallGraph(PointerAnalysis* pta);

    /// Connect VFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(const CallBlockNode* cs, const SVFFunction* callee, VFGEdgeSetTy& edges);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(const CallBlockNode* cs, const SVFFunction* func) const
    {
        return callgraph->getCallSiteID(cs, func);
    }
    inline const CallBlockNode* getCallSite(CallSiteID id) const
    {
        return callgraph->getCallSite(id);
    }
    //@}

    /// Given a pagNode, return its definition site
    inline const VFGNode* getDefVFGNode(const PAGNode* pagNode) const
    {
        return getVFGNode(getDef(pagNode));
    }

    // Given an VFG node, return its left hand side top level pointer (PAGnode)
    const PAGNode* getLHSTopLevPtr(const VFGNode* node) const;

    /// Get an VFGNode
    //@{
    inline StmtVFGNode* getStmtVFGNode(const PAGEdge* pagEdge) const
    {
        auto it = PAGEdgeToStmtVFGNodeMap.find(pagEdge);
        assert(it != PAGEdgeToStmtVFGNodeMap.end() && "StmtVFGNode can not be found??");
        return it->second;
    }
    inline IntraPHIVFGNode* getIntraPHIVFGNode(const PAGNode* pagNode) const
    {
        auto it = PAGNodeToIntraPHIVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToIntraPHIVFGNodeMap.end() && "PHIVFGNode can not be found??");
        return it->second;
    }
    inline BinaryOPVFGNode* getBinaryOPVFGNode(const PAGNode* pagNode) const
    {
        auto it = PAGNodeToBinaryOPVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToBinaryOPVFGNodeMap.end() && "BinaryOPVFGNode can not be found??");
        return it->second;
    }
    inline UnaryOPVFGNode* getUnaryOPVFGNode(const PAGNode* pagNode) const
    {
        auto it = PAGNodeToUnaryOPVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToUnaryOPVFGNodeMap.end() && "UnaryOPVFGNode can not be found??");
        return it->second;
    }
    inline CmpVFGNode* getCmpVFGNode(const PAGNode* pagNode) const
    {
        auto it = PAGNodeToCmpVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToCmpVFGNodeMap.end() && "CmpVFGNode can not be found??");
        return it->second;
    }
    inline ActualParmVFGNode* getActualParmVFGNode(const PAGNode* aparm,const CallBlockNode* cs) const
    {
        auto it = PAGNodeToActualParmMap.find(std::make_pair(aparm->getId(),cs));
        assert(it!=PAGNodeToActualParmMap.end() && "acutal parameter VFG node can not be found??");
        return it->second;
    }
    inline ActualRetVFGNode* getActualRetVFGNode(const PAGNode* aret) const
    {
        auto it = PAGNodeToActualRetMap.find(aret);
        assert(it!=PAGNodeToActualRetMap.end() && "actual return VFG node can not be found??");
        return it->second;
    }
    inline FormalParmVFGNode* getFormalParmVFGNode(const PAGNode* fparm) const
    {
        auto it = PAGNodeToFormalParmMap.find(fparm);
        assert(it!=PAGNodeToFormalParmMap.end() && "formal parameter VFG node can not be found??");
        return it->second;
    }
    inline FormalRetVFGNode* getFormalRetVFGNode(const PAGNode* fret) const
    {
        auto it = PAGNodeToFormalRetMap.find(fret);
        assert(it!=PAGNodeToFormalRetMap.end() && "formal return VFG node can not be found??");
        return it->second;
    }
    //@}

    /// Whether a node is function entry VFGNode
    const SVFFunction* isFunEntryVFGNode(const VFGNode* node) const;

    /// Whether a PAGNode has a blackhole or const object as its definition
    inline bool hasBlackHoleConstObjAddrAsDef(const PAGNode* pagNode) const
    {
        if (hasDef(pagNode))
        {
            const VFGNode* defNode = getVFGNode(getDef(pagNode));
            if (const auto* addr = SVFUtil::dyn_cast<AddrVFGNode>(defNode))
            {
                if (PAG::getPAG()->isBlkObjOrConstantObj(addr->getPAGEdge()->getSrcID()))
                    return true;
            }
            else if(const auto* copy = SVFUtil::dyn_cast<CopyVFGNode>(defNode))
            {
                if (PAG::getPAG()->isNullPtr(copy->getPAGEdge()->getSrcID()))
                    return true;
            }
        }
        return false;
    }

	/// Return all the VFGNodes of a function
	///@{
	inline VFGNodeSet& getVFGNodes(const SVFFunction *fun) {
		return funToVFGNodesMap[fun];
	}
	inline bool hasVFGNodes(const SVFFunction *fun) const {
		return funToVFGNodesMap.find(fun) != funToVFGNodesMap.end();
	}
	inline bool VFGNodes(const SVFFunction *fun) const {
		return funToVFGNodesMap.find(fun) != funToVFGNodesMap.end();
	}
	inline VFGNodeSet::const_iterator getVFGNodeBegin(const SVFFunction *fun) const {
		auto it = funToVFGNodesMap.find(fun);
		assert(it != funToVFGNodesMap.end() && "this function does not have any VFGNode");
		return it->second.begin();
	}
	inline VFGNodeSet::const_iterator getVFGNodeEnd(const SVFFunction *fun) const {
		auto it = funToVFGNodesMap.find(fun);
		assert(it != funToVFGNodesMap.end() && "this function does not have any VFGNode");
		return it->second.end();
	}
	///@}
    /// Add control-flow edges for top level pointers
    //@{
    VFGEdge* addIntraDirectVFEdge(NodeID srcId, NodeID dstId);
    VFGEdge* addCallEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    VFGEdge* addRetEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    //@}

    /// Remove a SVFG edge
    inline void removeVFGEdge(VFGEdge* edge)
    {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a VFGNode
    inline void removeVFGNode(VFGNode* node)
    {
        removeGNode(node);
    }

    /// Whether we has a SVFG edge
    //@{
    VFGEdge* hasIntraVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind);
    VFGEdge* hasInterVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind, CallSiteID csId);
    VFGEdge* hasThreadVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind);
    //@}

    /// Add VFG edge
    inline bool addVFGEdge(VFGEdge* edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

protected:

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(const VFGNode *srcNode, const VFGNode *dstNode)
    {
        const SVFFunction *srcfun = srcNode->getFun();
        const SVFFunction *dstfun = dstNode->getFun();
        if(srcfun != nullptr && dstfun != nullptr)
        {
            assert((srcfun == dstfun) && "src and dst nodes of an intra VFG edge are not in the same function?");
        }
    }

    /// Add inter VF edge from actual to formal parameters
    inline VFGEdge* addInterEdgeFromAPToFP(ActualParmVFGNode* src, FormalParmVFGNode* dst, CallSiteID csId)
    {
        return addCallEdge(src->getId(),dst->getId(),csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline VFGEdge* addInterEdgeFromFRToAR(FormalRetVFGNode* src, ActualRetVFGNode* dst, CallSiteID csId)
    {
        return addRetEdge(src->getId(),dst->getId(),csId);
    }

    /// Add inter VF edge from actual to formal parameters
    inline VFGEdge* addInterEdgeFromAPToFP(NodeID src, NodeID dst, CallSiteID csId)
    {
        return addCallEdge(src,dst,csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline VFGEdge* addInterEdgeFromFRToAR(NodeID src, NodeID dst, CallSiteID csId)
    {
        return addRetEdge(src,dst,csId);
    }

    /// Connect VFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-param and formal param
    virtual inline void connectAParamAndFParam(const PAGNode* csArg, const PAGNode* funArg, const CallBlockNode* cbn, CallSiteID csId, VFGEdgeSetTy& edges)
    {
        NodeID actualParam = getActualParmVFGNode(csArg, cbn)->getId();
        NodeID formalParam = getFormalParmVFGNode(funArg)->getId();
        VFGEdge* edge = addInterEdgeFromAPToFP(actualParam, formalParam,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    /// Connect formal-ret and actual ret
    virtual inline void connectFRetAndARet(const PAGNode* funReturn, const PAGNode* csReturn, CallSiteID csId, VFGEdgeSetTy& edges)
    {
        NodeID formalRet = getFormalRetVFGNode(funReturn)->getId();
        NodeID actualRet = getActualRetVFGNode(csReturn)->getId();
        VFGEdge* edge = addInterEdgeFromFRToAR(formalRet, actualRet,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    //@}

    /// Given a PAGNode, set/get its def VFG node (definition of top level pointers)
    //@{
    inline void setDef(const PAGNode* pagNode, const VFGNode* node)
    {
        auto it = PAGNodeToDefMap.find(pagNode);
        if(it == PAGNodeToDefMap.end())
        {
            PAGNodeToDefMap[pagNode] = node->getId();
            assert(hasVFGNode(node->getId()) && "not in the map!!");
        }
        else
        {
            assert((it->second == node->getId()) && "a PAG node can only have unique definition ");
        }
    }
    inline NodeID getDef(const PAGNode* pagNode) const
    {
        auto it = PAGNodeToDefMap.find(pagNode);
        assert(it!=PAGNodeToDefMap.end() && "PAG node does not have a definition??");
        return it->second;
    }
    inline bool hasDef(const PAGNode* pagNode) const
    {
        return (PAGNodeToDefMap.find(pagNode) != PAGNodeToDefMap.end());
    }
    //@}

    /// Create VFG nodes
    void addVFGNodes();

    /// Get PAGEdge set
    virtual inline PAGEdge::PAGEdgeSetTy& getPAGEdgeSet(PAGEdge::PEDGEK kind)
    {
        if (isPtrOnlySVFG())
            return pag->getPTAEdgeSet(kind);
        else
            return pag->getEdgeSet(kind);
    }

    virtual inline bool isInterestedPAGNode(const PAGNode* node) const
    {
        if (isPtrOnlySVFG())
            return node->isPointer();
        else
            return true;
    }

    /// Create edges between VFG nodes within a function
    void connectDirectVFGEdges();

    /// Create edges between VFG nodes across functions
    void addVFGInterEdges(const CallBlockNode* cs, const SVFFunction* callee);

    inline bool isPhiCopyEdge(const PAGEdge* copy) const
    {
        return pag->isPhiNode(copy->getDstNode());
    }

    /// Add a VFG node
    virtual inline void addVFGNode(VFGNode* vfgNode, ICFGNode* icfgNode)
    {
        addGNode(vfgNode->getId(), vfgNode);
        vfgNode->setICFGNode(icfgNode);
        icfgNode->addVFGNode(vfgNode);

        if(const SVFFunction* fun = icfgNode->getFun())
        	funToVFGNodesMap[fun].insert(vfgNode);
        else
        	globalVFGNodes.insert(vfgNode);
    }

    /// Add a VFG node for program statement
    inline void addStmtVFGNode(StmtVFGNode* node, const PAGEdge* pagEdge)
    {
        assert(PAGEdgeToStmtVFGNodeMap.find(pagEdge)==PAGEdgeToStmtVFGNodeMap.end() && "should not insert twice!");
        PAGEdgeToStmtVFGNodeMap[pagEdge] = node;
        addVFGNode(node, pagEdge->getICFGNode());
    }
    /// Add a Dummy VFG node for null pointer definition
    /// To be noted for black hole pointer it has already has address edge connected
    inline void addNullPtrVFGNode(const PAGNode* pagNode)
    {
        auto* sNode = new NullPtrVFGNode(totalVFGNode++,pagNode);
        addVFGNode(sNode, pag->getICFG()->getGlobalBlockNode());
        setDef(pagNode,sNode);
    }
    /// Add an Address VFG node
    inline void addAddrVFGNode(const AddrPE* addr)
    {
        auto* sNode = new AddrVFGNode(totalVFGNode++,addr);
        addStmtVFGNode(sNode, addr);
        setDef(addr->getDstNode(),sNode);
    }
    /// Add a Copy VFG node
    inline void addCopyVFGNode(const CopyPE* copy)
    {
        auto* sNode = new CopyVFGNode(totalVFGNode++,copy);
        addStmtVFGNode(sNode, copy);
        setDef(copy->getDstNode(),sNode);
    }
    /// Add a Gep VFG node
    inline void addGepVFGNode(const GepPE* gep)
    {
        auto* sNode = new GepVFGNode(totalVFGNode++,gep);
        addStmtVFGNode(sNode, gep);
        setDef(gep->getDstNode(),sNode);
    }
    /// Add a Load VFG node
    void addLoadVFGNode(const LoadPE* load)
    {
        auto* sNode = new LoadVFGNode(totalVFGNode++,load);
        addStmtVFGNode(sNode, load);
        setDef(load->getDstNode(),sNode);
    }
    /// Add a Store VFG node,
    /// To be noted store does not create a new pointer, we do not set def for any PAG node
    void addStoreVFGNode(const StorePE* store)
    {
        auto* sNode = new StoreVFGNode(totalVFGNode++,store);
        addStmtVFGNode(sNode, store);
    }

    /// Add an actual parameter VFG node
    /// To be noted that multiple actual parameters may have same value (PAGNode)
    /// So we need to make a pair <PAGNodeID,CallSiteID> to find the right VFGParmNode
    inline void addActualParmVFGNode(const PAGNode* aparm, const CallBlockNode* cs)
    {
        auto* sNode = new ActualParmVFGNode(totalVFGNode++,aparm,cs);
        addVFGNode(sNode, pag->getICFG()->getCallBlockNode(cs->getCallSite()));
        PAGNodeToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add a formal parameter VFG node
    inline void addFormalParmVFGNode(const PAGNode* fparm, const SVFFunction* fun, CallPESet& callPEs)
    {
        auto* sNode = new FormalParmVFGNode(totalVFGNode++,fparm,fun);
        addVFGNode(sNode, pag->getICFG()->getFunEntryBlockNode(fun));
        for(const auto *callPE : callPEs)
            sNode->addCallPE(callPE);

        setDef(fparm,sNode);
        PAGNodeToFormalParmMap[fparm] = sNode;
    }
    /// Add a callee Return VFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <PAGNodeID,CallSiteID> pair to find FormalRetVFG node same as handling actual parameters
    inline void addFormalRetVFGNode(const PAGNode* uniqueFunRet, const SVFFunction* fun, RetPESet& retPEs)
    {
		auto *sNode = new FormalRetVFGNode(totalVFGNode++, uniqueFunRet, fun);
		addVFGNode(sNode, pag->getICFG()->getFunExitBlockNode(fun));
		for (const auto *retPE : retPEs)
			sNode->addRetPE(retPE);

		PAGNodeToFormalRetMap[uniqueFunRet] = sNode;
		/// if this uniqueFunRet is a phi node, which means it will receive values from multiple return instructions of fun
		/// we will set this phi node's def later
		/// Ideally, every function uniqueFunRet should be a PhiNode (PAGBuilder.cpp), unless it does not have ret instruction
		if (!pag->isPhiNode(uniqueFunRet)){
			std::string warn = fun->getName();
			SVFUtil::writeWrnMsg(warn + " does not have any ret instruction!");
			setDef(uniqueFunRet, sNode);
		}
    }
    /// Add a callsite Receive VFG node
    inline void addActualRetVFGNode(const PAGNode* ret,const CallBlockNode* cs)
    {
        auto* sNode = new ActualRetVFGNode(totalVFGNode++,ret,cs);
        addVFGNode(sNode, pag->getICFG()->getRetBlockNode(cs->getCallSite()));
        setDef(ret,sNode);
        PAGNodeToActualRetMap[ret] = sNode;
    }
    /// Add an llvm PHI VFG node
    inline void addIntraPHIVFGNode(const PAGNode* phiResNode, PAG::CopyPEList& oplist)
    {
        auto* sNode = new IntraPHIVFGNode(totalVFGNode++,phiResNode);
        u32_t pos = 0;
        const PAGEdge* edge = nullptr;
        for(auto it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
        {
            edge = *it;
            sNode->setOpVerAndBB(pos, edge->getSrcNode(), edge->getICFGNode());
        }
        assert(edge && "edge not found?");
        addVFGNode(sNode,edge->getICFGNode());
        setDef(phiResNode,sNode);
        PAGNodeToIntraPHIVFGNodeMap[phiResNode] = sNode;
    }
    /// Add a Compare VFG node
    inline void addCmpVFGNode(const PAGNode* resNode, PAG::CmpPEList& oplist)
    {
        auto* sNode = new CmpVFGNode(totalVFGNode++, resNode);
        u32_t pos = 0;
        const PAGEdge* edge = nullptr;
        for(auto it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
        {
            edge = *it;
            sNode->setOpVer(pos, edge->getSrcNode());
        }
        assert(edge && "edge not found?");
        addVFGNode(sNode,edge->getICFGNode());
        setDef(resNode,sNode);
        PAGNodeToCmpVFGNodeMap[resNode] = sNode;
    }
    /// Add a BinaryOperator VFG node
    inline void addBinaryOPVFGNode(const PAGNode* resNode, PAG::BinaryOPList& oplist)
    {
        auto* sNode = new BinaryOPVFGNode(totalVFGNode++, resNode);
        u32_t pos = 0;
        const PAGEdge* edge = nullptr;
        for(auto it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
        {
            edge = *it;
            sNode->setOpVer(pos, (*it)->getSrcNode());
        }

        assert(edge && "edge not found?");
        addVFGNode(sNode,edge->getICFGNode());
        setDef(resNode,sNode);
        PAGNodeToBinaryOPVFGNodeMap[resNode] = sNode;
    }
    /// Add a UnaryOperator VFG node
    inline void addUnaryOPVFGNode(const PAGNode* resNode, PAG::UnaryOPList& oplist)
    {
        auto* sNode = new UnaryOPVFGNode(totalVFGNode++, resNode);
        u32_t pos = 0;
        const PAGEdge* edge = nullptr;
        for(auto it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
        {
            edge = *it;
            sNode->setOpVer(pos, (*it)->getSrcNode());
        }

        assert(edge && "edge not found?");
        addVFGNode(sNode,edge->getICFGNode());
        setDef(resNode,sNode);
        PAGNodeToUnaryOPVFGNodeMap[resNode] = sNode;
    }
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::VFGNode*> : public GraphTraits<SVF::GenericNode<SVF::VFGNode,SVF::VFGEdge>*  >
{
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::VFGNode *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::VFGNode,SVF::VFGEdge>* > >
{
};

template<> struct GraphTraits<SVF::VFG*> : public GraphTraits<SVF::GenericGraph<SVF::VFGNode,SVF::VFGEdge>* >
{
    using NodeRef = SVF::VFGNode *;
};

} // End namespace llvm

#endif /* INCLUDE_UTIL_VFG_H_ */
