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


#include "MemoryModel/SVFIR.h"
#include "Graphs/PTACallGraph.h"
#include "Graphs/VFGNode.h"
#include "Graphs/VFGEdge.h"

namespace SVF
{

class PointerAnalysis;
class VFGStat;
class CallICFGNode;

/*!
 *  Value Flow Graph (VFG)
 */
typedef GenericGraph<VFGNode,VFGEdge> GenericVFGTy;
class VFG : public GenericVFGTy
{

public:
    /// VFG kind
    enum VFGK
    {
        FULLSVFG, PTRONLYSVFG, FULLSVFG_OPT, PTRONLYSVFG_OPT
    };

    typedef Map<NodeID, VFGNode *> VFGNodeIDToNodeMapTy;
    typedef Set<VFGNode*> VFGNodeSet;
    typedef Map<const PAGNode*, NodeID> PAGNodeToDefMapTy;
    typedef Map<std::pair<NodeID,const CallICFGNode*>, ActualParmVFGNode *> PAGNodeToActualParmMapTy;
    typedef Map<const PAGNode*, ActualRetVFGNode *> PAGNodeToActualRetMapTy;
    typedef Map<const PAGNode*, FormalParmVFGNode *> PAGNodeToFormalParmMapTy;
    typedef Map<const PAGNode*, FormalRetVFGNode *> PAGNodeToFormalRetMapTy;
    typedef Map<const PAGEdge*, StmtVFGNode*> PAGEdgeToStmtVFGNodeMapTy;
    typedef Map<const PAGNode*, IntraPHIVFGNode*> PAGNodeToPHIVFGNodeMapTy;
    typedef Map<const PAGNode*, BinaryOPVFGNode*> PAGNodeToBinaryOPVFGNodeMapTy;
    typedef Map<const PAGNode*, UnaryOPVFGNode*> PAGNodeToUnaryOPVFGNodeMapTy;
    typedef Map<const PAGNode*, BranchVFGNode*> PAGNodeToBranchVFGNodeMapTy;
    typedef Map<const PAGNode*, CmpVFGNode*> PAGNodeToCmpVFGNodeMapTy;
    typedef Map<const SVFFunction*, VFGNodeSet > FunToVFGNodesMapTy;

    typedef FormalParmVFGNode::CallPESet CallPESet;
    typedef FormalRetVFGNode::RetPESet RetPESet;
    typedef VFGEdge::VFGEdgeSetTy VFGEdgeSetTy;
    typedef VFGEdge::SVFGEdgeSetTy SVFGEdgeSetTy;
    typedef VFGEdge::VFGEdgeSetTy::iterator VFGNodeIter;
    typedef VFGNodeIDToNodeMapTy::iterator iterator;
    typedef VFGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef SVFIR::SVFStmtSet SVFStmtSet;
    typedef Set<const VFGNode*> GlobalVFGNodeSet;
    typedef Set<const PAGNode*> PAGNodeSet;


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
    PAGNodeToBranchVFGNodeMapTy PAGNodeToBranchVFGNodeMap;	///< map a PAGNode to its BranchVFGNode
    PAGNodeToCmpVFGNodeMapTy PAGNodeToCmpVFGNodeMap;	///< map a PAGNode to its CmpVFGNode
    PAGEdgeToStmtVFGNodeMapTy PAGEdgeToStmtVFGNodeMap;	///< map a PAGEdge to its StmtVFGNode
    FunToVFGNodesMapTy funToVFGNodesMap; ///< map a function to its VFGNodes;

    GlobalVFGNodeSet globalVFGNodes;	///< set of global store VFG nodes
    PTACallGraph* callgraph;
    SVFIR* pag;
    VFGK kind;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    VFG(PTACallGraph* callgraph, VFGK k = FULLSVFG);

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
        return (kind == PTRONLYSVFG) || (kind == PTRONLYSVFG_OPT);
    }

    /// Return SVFIR
    inline SVFIR* getPAG() const
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

    /// Dump graph into dot file
    void view();

    /// Update VFG based on pointer analysis results
    void updateCallGraph(PointerAnalysis* pta);

    /// Connect VFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(const CallICFGNode* cs, const SVFFunction* callee, VFGEdgeSetTy& edges);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(const CallICFGNode* cs, const SVFFunction* func) const
    {
        return callgraph->getCallSiteID(cs, func);
    }
    inline const CallICFGNode* getCallSite(CallSiteID id) const
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
        PAGEdgeToStmtVFGNodeMapTy::const_iterator it = PAGEdgeToStmtVFGNodeMap.find(pagEdge);
        assert(it != PAGEdgeToStmtVFGNodeMap.end() && "StmtVFGNode can not be found??");
        return it->second;
    }
    inline IntraPHIVFGNode* getIntraPHIVFGNode(const PAGNode* pagNode) const
    {
        PAGNodeToPHIVFGNodeMapTy::const_iterator it = PAGNodeToIntraPHIVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToIntraPHIVFGNodeMap.end() && "PHIVFGNode can not be found??");
        return it->second;
    }
    inline BinaryOPVFGNode* getBinaryOPVFGNode(const PAGNode* pagNode) const
    {
        PAGNodeToBinaryOPVFGNodeMapTy::const_iterator it = PAGNodeToBinaryOPVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToBinaryOPVFGNodeMap.end() && "BinaryOPVFGNode can not be found??");
        return it->second;
    }
    inline UnaryOPVFGNode* getUnaryOPVFGNode(const PAGNode* pagNode) const
    {
        PAGNodeToUnaryOPVFGNodeMapTy::const_iterator it = PAGNodeToUnaryOPVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToUnaryOPVFGNodeMap.end() && "UnaryOPVFGNode can not be found??");
        return it->second;
    }
    inline BranchVFGNode* getBranchVFGNode(const PAGNode* pagNode) const
    {
        PAGNodeToBranchVFGNodeMapTy::const_iterator it = PAGNodeToBranchVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToBranchVFGNodeMap.end() && "BranchVFGNode can not be found??");
        return it->second;
    }
    inline CmpVFGNode* getCmpVFGNode(const PAGNode* pagNode) const
    {
        PAGNodeToCmpVFGNodeMapTy::const_iterator it = PAGNodeToCmpVFGNodeMap.find(pagNode);
        assert(it != PAGNodeToCmpVFGNodeMap.end() && "CmpVFGNode can not be found??");
        return it->second;
    }
    inline ActualParmVFGNode* getActualParmVFGNode(const PAGNode* aparm,const CallICFGNode* cs) const
    {
        PAGNodeToActualParmMapTy::const_iterator it = PAGNodeToActualParmMap.find(std::make_pair(aparm->getId(),cs));
        assert(it!=PAGNodeToActualParmMap.end() && "actual parameter VFG node can not be found??");
        return it->second;
    }
    inline ActualRetVFGNode* getActualRetVFGNode(const PAGNode* aret) const
    {
        PAGNodeToActualRetMapTy::const_iterator it = PAGNodeToActualRetMap.find(aret);
        assert(it!=PAGNodeToActualRetMap.end() && "actual return VFG node can not be found??");
        return it->second;
    }
    inline FormalParmVFGNode* getFormalParmVFGNode(const PAGNode* fparm) const
    {
        PAGNodeToFormalParmMapTy::const_iterator it = PAGNodeToFormalParmMap.find(fparm);
        assert(it!=PAGNodeToFormalParmMap.end() && "formal parameter VFG node can not be found??");
        return it->second;
    }
    inline FormalRetVFGNode* getFormalRetVFGNode(const PAGNode* fret) const
    {
        PAGNodeToFormalRetMapTy::const_iterator it = PAGNodeToFormalRetMap.find(fret);
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
            if (const AddrVFGNode* addr = SVFUtil::dyn_cast<AddrVFGNode>(defNode))
            {
                if (SVFIR::getPAG()->isBlkObjOrConstantObj(addr->getPAGEdge()->getSrcID()))
                    return true;
            }
            else if(const CopyVFGNode* copy = SVFUtil::dyn_cast<CopyVFGNode>(defNode))
            {
                if (SVFIR::getPAG()->isNullPtr(copy->getPAGEdge()->getSrcID()))
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
		FunToVFGNodesMapTy::const_iterator it = funToVFGNodesMap.find(fun);
		assert(it != funToVFGNodesMap.end() && "this function does not have any VFGNode");
		return it->second.begin();
	}
	inline VFGNodeSet::const_iterator getVFGNodeEnd(const SVFFunction *fun) const {
		FunToVFGNodesMapTy::const_iterator it = funToVFGNodesMap.find(fun);
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
    virtual inline void connectAParamAndFParam(const PAGNode* csArg, const PAGNode* funArg, const CallICFGNode* cbn, CallSiteID csId, VFGEdgeSetTy& edges)
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
        PAGNodeToDefMapTy::iterator it = PAGNodeToDefMap.find(pagNode);
        if(it == PAGNodeToDefMap.end())
        {
            PAGNodeToDefMap[pagNode] = node->getId();
            assert(hasVFGNode(node->getId()) && "not in the map!!");
        }
        else
        {
            assert((it->second == node->getId()) && "a SVFVar can only have unique definition ");
        }
    }
    inline NodeID getDef(const PAGNode* pagNode) const
    {
        PAGNodeToDefMapTy::const_iterator it = PAGNodeToDefMap.find(pagNode);
        assert(it!=PAGNodeToDefMap.end() && "SVFVar does not have a definition??");
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
    virtual inline SVFStmt::SVFStmtSetTy& getPAGEdgeSet(SVFStmt::PEDGEK kind)
    {
        if (isPtrOnlySVFG())
            return pag->getPTASVFStmtSet(kind);
        else
            return pag->getSVFStmtSet(kind);
    }

    virtual inline bool isInterestedPAGNode(const SVFVar* node) const
    {
        if (isPtrOnlySVFG())
            return node->isPointer();
        else
            return true;
    }

    /// Create edges between VFG nodes within a function
    void connectDirectVFGEdges();

    /// Create edges between VFG nodes across functions
    void addVFGInterEdges(const CallICFGNode* cs, const SVFFunction* callee);

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
        NullPtrVFGNode* sNode = new NullPtrVFGNode(totalVFGNode++,pagNode);
        addVFGNode(sNode, pag->getICFG()->getGlobalICFGNode());
        setDef(pagNode,sNode);
    }
    /// Add an Address VFG node
    inline void addAddrVFGNode(const AddrStmt* addr)
    {
        AddrVFGNode* sNode = new AddrVFGNode(totalVFGNode++,addr);
        addStmtVFGNode(sNode, addr);
        setDef(addr->getLHSVar(),sNode);
    }
    /// Add a Copy VFG node
    inline void addCopyVFGNode(const CopyStmt* copy)
    {
        CopyVFGNode* sNode = new CopyVFGNode(totalVFGNode++,copy);
        addStmtVFGNode(sNode, copy);
        setDef(copy->getLHSVar(),sNode);
    }
    /// Add a Gep VFG node
    inline void addGepVFGNode(const GepStmt* gep)
    {
        GepVFGNode* sNode = new GepVFGNode(totalVFGNode++,gep);
        addStmtVFGNode(sNode, gep);
        setDef(gep->getLHSVar(),sNode);
    }
    /// Add a Load VFG node
    void addLoadVFGNode(const LoadStmt* load)
    {
        LoadVFGNode* sNode = new LoadVFGNode(totalVFGNode++,load);
        addStmtVFGNode(sNode, load);
        setDef(load->getLHSVar(),sNode);
    }
    /// Add a Store VFG node,
    /// To be noted store does not create a new pointer, we do not set def for any SVFIR node
    void addStoreVFGNode(const StoreStmt* store)
    {
        StoreVFGNode* sNode = new StoreVFGNode(totalVFGNode++,store);
        addStmtVFGNode(sNode, store);
    }

    /// Add an actual parameter VFG node
    /// To be noted that multiple actual parameters may have same value (PAGNode)
    /// So we need to make a pair <PAGNodeID,CallSiteID> to find the right VFGParmNode
    inline void addActualParmVFGNode(const PAGNode* aparm, const CallICFGNode* cs)
    {
        ActualParmVFGNode* sNode = new ActualParmVFGNode(totalVFGNode++,aparm,cs);
        addVFGNode(sNode, pag->getICFG()->getCallICFGNode(cs->getCallSite()));
        PAGNodeToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add a formal parameter VFG node
    inline void addFormalParmVFGNode(const PAGNode* fparm, const SVFFunction* fun, CallPESet& callPEs)
    {
        FormalParmVFGNode* sNode = new FormalParmVFGNode(totalVFGNode++,fparm,fun);
        addVFGNode(sNode, pag->getICFG()->getFunEntryICFGNode(fun));
        for(CallPESet::const_iterator it = callPEs.begin(), eit=callPEs.end();
                it!=eit; ++it)
            sNode->addCallPE(*it);

        setDef(fparm,sNode);
        PAGNodeToFormalParmMap[fparm] = sNode;
    }
    /// Add a callee Return VFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <PAGNodeID,CallSiteID> pair to find FormalRetVFG node same as handling actual parameters
    inline void addFormalRetVFGNode(const PAGNode* uniqueFunRet, const SVFFunction* fun, RetPESet& retPEs)
    {
		FormalRetVFGNode *sNode = new FormalRetVFGNode(totalVFGNode++, uniqueFunRet, fun);
		addVFGNode(sNode, pag->getICFG()->getFunExitICFGNode(fun));
		for (RetPESet::const_iterator it = retPEs.begin(), eit = retPEs.end(); it != eit; ++it)
			sNode->addRetPE(*it);

		PAGNodeToFormalRetMap[uniqueFunRet] = sNode;
		/// if this uniqueFunRet is a phi node, which means it will receive values from multiple return instructions of fun
		/// we will set this phi node's def later
		/// Ideally, every function uniqueFunRet should be a PhiNode (SVFIRBuilder.cpp), unless it does not have ret instruction
		if (!pag->isPhiNode(uniqueFunRet)){
			std::string warn = fun->getName();
			SVFUtil::writeWrnMsg(warn + " does not have any ret instruction!");
			setDef(uniqueFunRet, sNode);
		}
    }
    /// Add a callsite Receive VFG node
    inline void addActualRetVFGNode(const PAGNode* ret,const CallICFGNode* cs)
    {
        ActualRetVFGNode* sNode = new ActualRetVFGNode(totalVFGNode++,ret,cs);
        addVFGNode(sNode, pag->getICFG()->getRetICFGNode(cs->getCallSite()));
        setDef(ret,sNode);
        PAGNodeToActualRetMap[ret] = sNode;
    }
    /// Add an llvm PHI VFG node
    inline void addIntraPHIVFGNode(const MultiOpndStmt* edge)
    {
        IntraPHIVFGNode* sNode = new IntraPHIVFGNode(totalVFGNode++,edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVerAndBB(pos, var, edge->getICFGNode());
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        PAGNodeToIntraPHIVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a Compare VFG node
    inline void addCmpVFGNode(const CmpStmt* edge)
    {
        CmpVFGNode* sNode = new CmpVFGNode(totalVFGNode++, edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVer(pos, var);
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        PAGNodeToCmpVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a BinaryOperator VFG node
    inline void addBinaryOPVFGNode(const BinaryOPStmt* edge)
    {
        BinaryOPVFGNode* sNode = new BinaryOPVFGNode(totalVFGNode++, edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVer(pos, var);
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        PAGNodeToBinaryOPVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a UnaryOperator VFG node
    inline void addUnaryOPVFGNode(const UnaryOPStmt* edge)
    {
        UnaryOPVFGNode* sNode = new UnaryOPVFGNode(totalVFGNode++, edge->getRes());
        sNode->setOpVer(0, edge->getOpVar());
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        PAGNodeToUnaryOPVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a BranchVFGNode 
    inline void addBranchVFGNode(const BranchStmt* edge)
    {
        BranchVFGNode* sNode = new BranchVFGNode(totalVFGNode++, edge);
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getBranchInst(),sNode);
        PAGNodeToBranchVFGNodeMap[edge->getBranchInst()] = sNode;
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
    typedef SVF::VFGNode *NodeRef;
};

} // End namespace llvm

#endif /* INCLUDE_UTIL_VFG_H_ */
