//===- ICFG.h ----------------------------------------------------------------//
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
 * ICFG.h
 *
 *  Created on: 11 Sep. 2018
 *      Author: Yulei
 */

#ifndef INCLUDE_UTIL_ICFG_H_
#define INCLUDE_UTIL_ICFG_H_

#include "Util/ICFGNode.h"
#include "Util/ICFGEdge.h"

class PointerAnalysis;
class ICFGStat;
class StmtVFGNode;
class VFG;

/*!
 * Interprocedural Control-Flow Graph (ICFG)
 */
typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
class ICFG : public GenericICFGTy {

public:

    typedef llvm::DenseMap<NodeID, ICFGNode *> ICFGNodeIDToNodeMapTy;
    typedef std::map<const PAGEdge*, StmtVFGNode*> PAGEdgeToStmtVFGNodeMapTy;

    typedef std::map<const Function*, FunEntryBlockNode *> FunToFunEntryNodeMapTy;
    typedef std::map<const Function*, FunExitBlockNode *> FunToFunExitNodeMapTy;
    typedef std::map<const Instruction*, IntraBlockNode *> InstToBlockNodeMapTy;
    typedef std::map<CallSite, CallBlockNode *> CSToCallNodeMapTy;
    typedef std::map<CallSite, RetBlockNode *> CSToRetNodeMapTy;

    typedef ICFGEdge::ICFGEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef PAG::PAGEdgeSet PAGEdgeSet;
    typedef std::vector<const Instruction*> InstVec;
    typedef std::set<const Instruction*> BBSet;
    typedef FIFOWorkList<const Instruction*> WorkList;


protected:
    NodeID totalICFGNode;
    FunToFunEntryNodeMapTy FunToFunEntryNodeMap; ///< map a function to its FunExitBlockNode
    FunToFunExitNodeMapTy FunToFunExitNodeMap; ///< map a function to its FunEntryBlockNode
    CSToCallNodeMapTy CSToCallNodeMap; ///< map a callsite to its CallBlockNode
    CSToRetNodeMapTy CSToRetNodeMap; ///< map a callsite to its RetBlockNode
    InstToBlockNodeMapTy InstToBlockNodeMap; ///< map a basic block to its ICFGNode
    ICFGStat * stat;
    PTACallGraph* callgraph;
    PAG* pag;
	VFG* vfg;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    ICFG(PTACallGraph* callgraph);

    /// Destructor
    virtual ~ICFG() {
        destroy();
    }

    /// Return statistics
    inline ICFGStat* getStat() const {
        return stat;
    }

    /// Return PAG
    inline PAG* getPAG() {
        return PAG::getPAG();
    }

    /// Return VFG
	inline VFG* getVFG() const{
		return vfg;
	}

    /// Get a ICFG node
    inline ICFGNode* getICFGNode(NodeID id) const {
        return getGNode(id);
    }

    /// Whether has the ICFGNode
    inline bool hasICFGNode(NodeID id) const {
        return hasGNode(id);
    }

    /// Whether we has a SVFG edge
    //@{
    ICFGEdge* hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    ICFGEdge* hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind, CallSiteID csId);
    ICFGEdge* hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    //@}

    /// Get a SVFG edge according to src and dst
    ICFGEdge* getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);

	/// Connect interprocedural CFG edges based on pointer analysis results
	void updateCallgraph(PointerAnalysis* pta);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(CallSite cs, const Function* func) const {
        return callgraph->getCallSiteID(cs, func);
    }
    inline CallSite getCallSite(CallSiteID id) const {
        return callgraph->getCallSite(id);
    }
    //@}

    /// Whether a node is function entry ICFGNode
    const Function* isFunEntryICFGNode(const ICFGNode* node) const;

protected:
    /// Remove a SVFG edge
    inline void removeICFGEdge(ICFGEdge* edge) {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a ICFGNode
    inline void removeICFGNode(ICFGNode* node) {
        removeGNode(node);
    }

    /// Add control-flow edges for top level pointers
    //@{
    ICFGEdge* addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode);
    ICFGEdge* addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSiteID csId);
    ICFGEdge* addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSiteID csId);
    //@}

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(const ICFGNode *srcNode, const ICFGNode *dstNode) {
        const BasicBlock *srcBB = srcNode->getBB();
        const BasicBlock *dstBB = dstNode->getBB();
        if(srcBB != nullptr && dstBB != nullptr) {
            assert(srcBB->getParent() == dstBB->getParent());
        }
    }

    /// Add ICFG edge
    inline bool addICFGEdge(ICFGEdge* edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Create edges between ICFG nodes within a function
    ///@{
    void build();

    void processFunEntry(const Function* fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function* fun);
    //@}

    /// Add VFG nodes to ICFG
    //@{
    /// Add VFGNodes to ICFG
    void addVFGToICFG();
    /// Add global stores to the function entry ICFGNode of main
    void connectGlobalToProgEntry();
    /// Add VFGStmtNode into IntraBlockNode
    void handleIntraBlock(IntraBlockNode* intraICFGNode);
    /// Add ArgumentVFGNode into InterBlockNode
    void handleInterBlock(InterBlockNode* interICFGNode);
    //@}
    /// Within handleInterBlock: handle 4 kinds of ArgumentNodes
    void handleFormalParm(FunEntryBlockNode* funEntryBlockNode);
    void handleFormalRet(FunExitBlockNode* funExitBlockNode);
    void handleActualParm(CallBlockNode* callBlockNode);
    void handleActualRet(RetBlockNode* retBlockNode);

    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(CallSite cs, const Function* callee);

    inline bool isPhiCopyEdge(const PAGEdge* copy) const {
        return pag->isPhiNode(copy->getDstNode());
    }
    inline bool isBinaryEdge(const PAGEdge* copy) const {
        return pag->isBinaryNode(copy->getDstNode());
    }
    inline bool isCmpEdge(const PAGEdge* copy) const {
        return pag->isCmpNode(copy->getDstNode());
    }

    /// Add a ICFG node
    virtual inline void addICFGNode(ICFGNode* node) {
        addGNode(node->getId(),node);
    }

    /// Create InterBlockNode at direct callsites
    void handleCall(IntraBlockNode* instICFGNode, const Instruction* inst);

    /// Add/Get an intra block ICFGNode
    IntraBlockNode* getIntraBlockICFGNode(const Instruction* inst);

    /// Add/Get an inter block ICFGNode
    InterBlockNode* getInterBlockICFGNode(const Instruction* inst);

	/// Add/Get a basic block ICFGNode
	inline ICFGNode* getBlockICFGNode(const Instruction* inst) {
		if(SVFUtil::isNonInstricCallSite(inst))
			return getInterBlockICFGNode(inst);
		else
			return getIntraBlockICFGNode(inst);
	}

public:
    /// Add a function entry node
    inline FunEntryBlockNode* getFunEntryICFGNode(const Function* fun) {
		FunToFunEntryNodeMapTy::const_iterator it = FunToFunEntryNodeMap.find(fun);
		if (it == FunToFunEntryNodeMap.end()) {
			FunEntryBlockNode* sNode = new FunEntryBlockNode(totalICFGNode++,fun);
			addICFGNode(sNode);
			FunToFunEntryNodeMap[fun] = sNode;
			return sNode;
		}
		return it->second;
	}
	/// Add a function exit node
	inline FunExitBlockNode* getFunExitICFGNode(const Function* fun) {
		FunToFunExitNodeMapTy::const_iterator it = FunToFunExitNodeMap.find(fun);
		if (it == FunToFunExitNodeMap.end()) {
			FunExitBlockNode* sNode = new FunExitBlockNode(totalICFGNode++, fun);
			addICFGNode(sNode);
			FunToFunExitNodeMap[fun] = sNode;
			return sNode;
		}
		return it->second;
	}
    /// Add a call node
    inline CallBlockNode* getCallICFGNode(CallSite cs) {
		CSToCallNodeMapTy::const_iterator it = CSToCallNodeMap.find(cs);
		if (it == CSToCallNodeMap.end()) {
			CallBlockNode* sNode = new CallBlockNode(totalICFGNode++, cs);
			addICFGNode(sNode);
			CSToCallNodeMap[cs] = sNode;
			return sNode;
		}
		return it->second;
    }
    /// Add a return node
    inline RetBlockNode* getRetICFGNode(CallSite cs) {
		CSToRetNodeMapTy::const_iterator it = CSToRetNodeMap.find(cs);
		if (it == CSToRetNodeMap.end()) {
			RetBlockNode* sNode = new RetBlockNode(totalICFGNode++, cs);
			addICFGNode(sNode);
			CSToRetNodeMap[cs] = sNode;
			return sNode;
		}
		return it->second;
    }
};


namespace llvm {
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<ICFGNode*> : public GraphTraits<GenericNode<ICFGNode,ICFGEdge>*  > {
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<ICFGNode *> > : public GraphTraits<Inverse<GenericNode<ICFGNode,ICFGEdge>* > > {
};

template<> struct GraphTraits<ICFG*> : public GraphTraits<GenericGraph<ICFGNode,ICFGEdge>* > {
    typedef ICFGNode *NodeRef;
};


}

#endif /* INCLUDE_UTIL_ICFG_H_ */
