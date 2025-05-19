//===- ICFG.h ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
#include "MemoryModel/SVFLoop.h"

namespace SVF
{

class CallGraph;

/*!
 * Interprocedural Control-Flow Graph (ICFG)
 */
typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
class ICFG : public GenericICFGTy
{
    friend class ICFGBuilder;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class ICFGSimplification;
    friend class GraphDBClient;

public:

    typedef OrderedMap<NodeID, ICFGNode *> ICFGNodeIDToNodeMapTy;
    typedef ICFGEdge::ICFGEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;

    typedef Map<const FunObjVar*, FunEntryICFGNode *> FunToFunEntryNodeMapTy;
    typedef Map<const FunObjVar*, FunExitICFGNode *> FunToFunExitNodeMapTy;
    typedef std::vector<const SVFLoop *> SVFLoopVec;
    typedef Map<const ICFGNode *, SVFLoopVec> ICFGNodeToSVFLoopVec;

    NodeID totalICFGNode;

private:
    FunToFunEntryNodeMapTy FunToFunEntryNodeMap; ///< map a function to its FunExitICFGNode
    FunToFunExitNodeMapTy FunToFunExitNodeMap; ///< map a function to its FunEntryICFGNode
    GlobalICFGNode* globalBlockNode; ///< unique basic block for all globals
    ICFGNodeToSVFLoopVec icfgNodeToSVFLoopVec; ///< map ICFG node to the SVF loops where it resides

public:
    /// Constructor
    ICFG();

    /// Destructor
    ~ICFG() override;

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
    void updateCallGraph(CallGraph* callgraph);

    /// Whether node is in a loop
    inline bool isInLoop(const ICFGNode *node)
    {
        auto it = icfgNodeToSVFLoopVec.find(node);
        return it != icfgNodeToSVFLoopVec.end();
    }

    /// Insert (node, loop) to icfgNodeToSVFLoopVec
    inline void addNodeToSVFLoop(const ICFGNode *node, const SVFLoop* loop)
    {
        icfgNodeToSVFLoopVec[node].push_back(loop);
    }

    /// Get loops where a node resides
    inline SVFLoopVec& getSVFLoops(const ICFGNode *node)
    {
        auto it = icfgNodeToSVFLoopVec.find(node);
        assert(it != icfgNodeToSVFLoopVec.end() && "node not in loop");
        return it->second;
    }

    inline const ICFGNodeToSVFLoopVec& getIcfgNodeToSVFLoopVec() const
    {
        return icfgNodeToSVFLoopVec;
    }

protected:
    /// Add intraprocedural and interprocedural control-flow edges.
    //@{
    ICFGEdge* addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode);
    ICFGEdge* addConditionalIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode, s64_t branchCondVal);
    ICFGEdge* addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode);
    ICFGEdge* addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode);
    //@}
    /// Remove a ICFG edge
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

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(const ICFGNode *srcNode, const ICFGNode *dstNode)
    {
        const FunObjVar* srcfun = srcNode->getFun();
        const FunObjVar* dstfun = dstNode->getFun();
        if(srcfun != nullptr && dstfun != nullptr)
        {
            assert((srcfun == dstfun) && "src and dst nodes of an intra edge should in the same function!" );
        }
    }

    virtual inline IntraICFGNode* addIntraICFGNode(const SVFBasicBlock* bb, bool isRet)
    {
        IntraICFGNode* intraIcfgNode =
            new IntraICFGNode(totalICFGNode++, bb, isRet);
        addICFGNode(intraIcfgNode);
        return intraIcfgNode;
    }

    virtual inline void addIntraICFGNodeFromDB(IntraICFGNode* intraICFGNode)
    {
        totalICFGNode++;
        addICFGNode(intraICFGNode);
    }

    virtual inline CallICFGNode* addCallICFGNode(
        const SVFBasicBlock* bb, const SVFType* ty,
        const FunObjVar* calledFunc, bool isVararg, bool isvcall,
        s32_t vcallIdx, const std::string& funNameOfVcall)
    {

        CallICFGNode* callICFGNode =
            new CallICFGNode(totalICFGNode++, bb, ty, calledFunc, isVararg,
                             isvcall, vcallIdx, funNameOfVcall);
        addICFGNode(callICFGNode);
        return callICFGNode;
    }

    virtual inline void addCallICFGNodeFromDB(CallICFGNode* callICFGNode)
    {
        totalICFGNode++;
        addICFGNode(callICFGNode);
    }
    
    virtual inline RetICFGNode* addRetICFGNode(CallICFGNode* call)
    {
        RetICFGNode* retICFGNode = new RetICFGNode(totalICFGNode++, call);
        call->setRetICFGNode(retICFGNode);
        addICFGNode(retICFGNode);
        return retICFGNode;
    }
    virtual inline void addRetICFGNodeFromDB(RetICFGNode* retICFGNode)
    {
        totalICFGNode++;
        addICFGNode(retICFGNode);
    }

    virtual inline FunEntryICFGNode* addFunEntryICFGNode(const FunObjVar* svfFunc)
    {
        FunEntryICFGNode* sNode = new FunEntryICFGNode(totalICFGNode++,svfFunc);
        addICFGNode(sNode);
        return FunToFunEntryNodeMap[svfFunc] = sNode;
    }

    virtual inline void addFunEntryICFGNodeFromDB(FunEntryICFGNode* funEntryICFGNode)
    {
        totalICFGNode++;
        addICFGNode(funEntryICFGNode);
        FunToFunEntryNodeMap[funEntryICFGNode->getFun()] = funEntryICFGNode;
    }

    virtual inline void addGlobalICFGNodeFromDB(GlobalICFGNode* globalICFGNode)
    {
        totalICFGNode++;
        this->globalBlockNode = globalICFGNode;
        addICFGNode(globalICFGNode);
    }

    virtual inline FunExitICFGNode* addFunExitICFGNode(const FunObjVar* svfFunc)
    {
        FunExitICFGNode* sNode = new FunExitICFGNode(totalICFGNode++, svfFunc);
        addICFGNode(sNode);
        return FunToFunExitNodeMap[svfFunc] = sNode;
    }

    virtual inline void addFunExitICFGNodeFromDB(FunExitICFGNode* funExitICFGNode)
    {
        totalICFGNode++;
        addICFGNode(funExitICFGNode);
        FunToFunExitNodeMap[funExitICFGNode->getFun()] = funExitICFGNode;
    }

    /// Add a ICFG node
    virtual inline void addICFGNode(ICFGNode* node)
    {
        addGNode(node->getId(),node);
    }

public:
    /// Get a basic block ICFGNode
    /// TODO:: need to fix the assertions
    //@{


    FunEntryICFGNode* getFunEntryICFGNode(const FunObjVar*  fun);

    FunExitICFGNode* getFunExitICFGNode(const FunObjVar*  fun);

    inline GlobalICFGNode* getGlobalICFGNode() const
    {
        return globalBlockNode;
    }
    //@}

private:
    /// Add ICFG edge, only used by addIntraEdge, addCallEdge, addRetEdge etc.
    inline bool addICFGEdge(ICFGEdge* edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        bool all_added = added1 && added2;
        assert(all_added && "ICFGEdge not added?");
        return all_added;
    }

    /// Get/Add a function entry node
    inline FunEntryICFGNode* getFunEntryBlock(const FunObjVar* fun)
    {
        FunToFunEntryNodeMapTy::const_iterator it = FunToFunEntryNodeMap.find(fun);
        if (it == FunToFunEntryNodeMap.end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a function exit node
    inline FunExitICFGNode* getFunExitBlock(const FunObjVar* fun)
    {
        FunToFunExitNodeMapTy::const_iterator it = FunToFunExitNodeMap.find(fun);
        if (it == FunToFunExitNodeMap.end())
            return nullptr;
        return it->second;
    }

};

} // End namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::ICFGNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::ICFGNode,SVF::ICFGEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::ICFGNode *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::ICFGNode,SVF::ICFGEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::ICFG*> : public GenericGraphTraits<SVF::GenericGraph<SVF::ICFGNode,SVF::ICFGEdge>* >
{
    typedef SVF::ICFGNode *NodeRef;
};

} // End namespace llvm

#endif /* INCLUDE_UTIL_ICFG_H_ */
