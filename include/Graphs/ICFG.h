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

#include "Graphs/ICFGNode.h"
#include "Graphs/ICFGEdge.h"
#include "Util/WorkList.h"

namespace SVF
{

class PTACallGraph;

/*!
 * Interprocedural Control-Flow Graph (ICFG)
 */
typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
class ICFG : public GenericICFGTy
{

public:

    typedef Map<NodeID, ICFGNode *> ICFGNodeIDToNodeMapTy;
    typedef ICFGEdge::ICFGEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;

    typedef Map<const SVFFunction*, FunEntryBlockNode *> FunToFunEntryNodeMapTy;
    typedef Map<const SVFFunction*, FunExitBlockNode *> FunToFunExitNodeMapTy;
    typedef Map<const Instruction*, CallBlockNode *> CSToCallNodeMapTy;
    typedef Map<const Instruction*, RetBlockNode *> CSToRetNodeMapTy;
    typedef Map<const Instruction*, IntraBlockNode *> InstToBlockNodeMapTy;

    NodeID totalICFGNode;

private:
    FunToFunEntryNodeMapTy FunToFunEntryNodeMap; ///< map a function to its FunExitBlockNode
    FunToFunExitNodeMapTy FunToFunExitNodeMap; ///< map a function to its FunEntryBlockNode
    CSToCallNodeMapTy CSToCallNodeMap; ///< map a callsite to its CallBlockNode
    CSToRetNodeMapTy CSToRetNodeMap; ///< map a callsite to its RetBlockNode
    InstToBlockNodeMapTy InstToBlockNodeMap; ///< map a basic block to its ICFGNode
    GlobalBlockNode* globalBlockNode; ///< unique basic block for all globals

public:
    /// Constructor
    ICFG();

    /// Destructor
    virtual ~ICFG()
    {
    }

    /// Get a ICFG node
    inline ICFGNode* getICFGNode(NodeID id) const
    {
        return getGNode(id);
    }

    /// Whether has the ICFGNode
    inline bool hasICFGNode(NodeID id) const
    {
        return hasGNode(id);
    }

    /// Whether we has a SVFG edge
    //@{
    ICFGEdge* hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    ICFGEdge* hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    ICFGEdge* hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    //@}

    /// Get a SVFG edge according to src and dst
    ICFGEdge* getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// View graph from the debugger
    void view();

    /// update ICFG for indirect calls
    void updateCallGraph(PTACallGraph* callgraph);

public:
    /// Remove a SVFG edge
    inline void removeICFGEdge(ICFGEdge* edge)
    {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a ICFGNode
    inline void removeICFGNode(ICFGNode* node)
    {
        removeGNode(node);
    }

    /// Add control-flow edges for top level pointers
    //@{
    ICFGEdge* addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode);
    ICFGEdge* addConditionalIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Value* condition, NodeID branchID);
    ICFGEdge* addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction* cs);
    ICFGEdge* addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction* cs);
    //@}

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(const ICFGNode *srcNode, const ICFGNode *dstNode)
    {
        const SVFFunction* srcfun = srcNode->getFun();
        const SVFFunction* dstfun = dstNode->getFun();
        if(srcfun != nullptr && dstfun != nullptr)
        {
            assert((srcfun == dstfun) && "src and dst nodes of an intra edge should in the same function!" );
        }
    }

    /// Add ICFG edge
    inline bool addICFGEdge(ICFGEdge* edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Add a ICFG node
    virtual inline void addICFGNode(ICFGNode* node)
    {
        addGNode(node->getId(),node);
    }

    /// Get a basic block ICFGNode
    /// TODO:: need to fix the assertions
    //@{
    ICFGNode* getBlockICFGNode(const Instruction* inst);

    CallBlockNode* getCallBlockNode(const Instruction* inst);

    RetBlockNode* getRetBlockNode(const Instruction* inst);

    IntraBlockNode* getIntraBlockNode(const Instruction* inst);

    FunEntryBlockNode* getFunEntryBlockNode(const SVFFunction*  fun);

    FunExitBlockNode* getFunExitBlockNode(const SVFFunction*  fun);

    GlobalBlockNode* getGlobalBlockNode() const
    {
        return globalBlockNode;
    }
    
    //@}

private:
	
    /// Get/Add IntraBlock ICFGNode
    inline IntraBlockNode* getIntraBlockICFGNode(const Instruction* inst)
    {
        InstToBlockNodeMapTy::const_iterator it = InstToBlockNodeMap.find(inst);
        if (it == InstToBlockNodeMap.end())
            return nullptr;
        return it->second;
    }
    inline IntraBlockNode* addIntraBlockICFGNode(const Instruction* inst)
    {
        IntraBlockNode* sNode = new IntraBlockNode(totalICFGNode++,inst);
        addICFGNode(sNode);
        InstToBlockNodeMap[inst] = sNode;
        return sNode;
    }

    /// Get/Add a function entry node
    inline FunEntryBlockNode* getFunEntryICFGNode(const SVFFunction* fun)
    {
        FunToFunEntryNodeMapTy::const_iterator it = FunToFunEntryNodeMap.find(fun);
        if (it == FunToFunEntryNodeMap.end())
            return nullptr;
        return it->second;
    }
    inline FunEntryBlockNode* addFunEntryICFGNode(const SVFFunction* fun)
    {
        FunEntryBlockNode* sNode = new FunEntryBlockNode(totalICFGNode++,fun);
        addICFGNode(sNode);
        FunToFunEntryNodeMap[fun] = sNode;
        return sNode;
    }

    /// Get/Add a function exit node
    inline FunExitBlockNode* getFunExitICFGNode(const SVFFunction* fun)
    {
        FunToFunExitNodeMapTy::const_iterator it = FunToFunExitNodeMap.find(fun);
        if (it == FunToFunExitNodeMap.end())
            return nullptr;
        return it->second;
    }
    inline FunExitBlockNode* addFunExitICFGNode(const SVFFunction* fun)
    {
        FunExitBlockNode* sNode = new FunExitBlockNode(totalICFGNode++, fun);
        addICFGNode(sNode);
        FunToFunExitNodeMap[fun] = sNode;
        return sNode;
    }

    /// Get/Add a call node
    inline CallBlockNode* addCallICFGNode(const Instruction* cs)
    {
        CallBlockNode* sNode = new CallBlockNode(totalICFGNode++, cs);
        addICFGNode(sNode);
        CSToCallNodeMap[cs] = sNode;
        return sNode;
    }
    inline CallBlockNode* getCallICFGNode(const Instruction* cs)
    {
        CSToCallNodeMapTy::const_iterator it = CSToCallNodeMap.find(cs);
        if (it == CSToCallNodeMap.end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a return node
    inline RetBlockNode* getRetICFGNode(const Instruction* cs)
    {
        CSToRetNodeMapTy::const_iterator it = CSToRetNodeMap.find(cs);
        if (it == CSToRetNodeMap.end())
            return nullptr;
        return it->second;
    }
    inline RetBlockNode* addRetICFGNode(const Instruction* cs)
    {
        CallBlockNode* callBlockNode = getCallBlockNode(cs);
        RetBlockNode* sNode = new RetBlockNode(totalICFGNode++, cs, callBlockNode);
        callBlockNode->setRetBlockNode(sNode);
        addICFGNode(sNode);
        CSToRetNodeMap[cs] = sNode;
        return sNode;
    }

};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::ICFGNode*> : public GraphTraits<SVF::GenericNode<SVF::ICFGNode,SVF::ICFGEdge>*  >
{
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::ICFGNode *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::ICFGNode,SVF::ICFGEdge>* > >
{
};

template<> struct GraphTraits<SVF::ICFG*> : public GraphTraits<SVF::GenericGraph<SVF::ICFGNode,SVF::ICFGEdge>* >
{
    typedef SVF::ICFGNode *NodeRef;
};

} // End namespace llvm

#endif /* INCLUDE_UTIL_ICFG_H_ */
