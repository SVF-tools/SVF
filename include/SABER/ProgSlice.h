//===- ProgSlice.h -- Program slicing based on SVF----------------------------//
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
 * ProgSlice.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef PROGSLICE_H_
#define PROGSLICE_H_

#include "Util/PathCondAllocator.h"
#include "Util/WorkList.h"
#include "Graphs/SVFG.h"
#include "Util/DPItem.h"

namespace SVF
{

class ProgSlice
{

public:
    typedef Set<const SVFGNode*> SVFGNodeSet;
    typedef SVFGNodeSet::const_iterator SVFGNodeSetIter;
    typedef PathCondAllocator::Condition Condition;
    typedef Map<const SVFGNode*, Condition*> SVFGNodeToCondMap; 	///< map a SVFGNode to its condition during value-flow guard computation

    typedef FIFOWorkList<const SVFGNode*> VFWorkList;		    ///< worklist for value-flow guard computation
    typedef FIFOWorkList<const BasicBlock*> CFWorkList;	///< worklist for control-flow guard computation

    /// Constructor
    ProgSlice(const SVFGNode* src, PathCondAllocator* pa, const SVFG* graph):
        root(src), partialReachable(false), fullReachable(false), reachGlob(false),
        pathAllocator(pa), _curSVFGNode(nullptr), finalCond(pa->getFalseCond()), svfg(graph)
    {
    }

    /// Destructor
    virtual ~ProgSlice()
    {
        destroy();
    }

    inline u32_t getForwardSliceSize() const
    {
        return forwardslice.size();
    }
    inline u32_t getBackwardSliceSize() const
    {
        return backwardslice.size();
    }
    /// Forward and backward slice operations
    //@{
    inline void addToForwardSlice(const SVFGNode* node)
    {
        forwardslice.insert(node);
    }
    inline void addToBackwardSlice(const SVFGNode* node)
    {
        backwardslice.insert(node);
    }
    inline bool inForwardSlice(const SVFGNode* node)
    {
        return forwardslice.find(node)!=forwardslice.end();
    }
    inline bool inBackwardSlice(const SVFGNode* node)
    {
        return backwardslice.find(node)!=backwardslice.end();
    }
    inline SVFGNodeSetIter forwardSliceBegin() const
    {
        return forwardslice.begin();
    }
    inline SVFGNodeSetIter forwardSliceEnd() const
    {
        return forwardslice.end();
    }
    inline SVFGNodeSetIter backwardSliceBegin() const
    {
        return backwardslice.begin();
    }
    inline SVFGNodeSetIter backwardSliceEnd() const
    {
        return backwardslice.end();
    }
    //@}

    /// root and sink operations
    //@{
    inline const SVFGNode* getSource() const
    {
        return root;
    }
    inline void addToSinks(const SVFGNode* node)
    {
        sinks.insert(node);
    }
    inline const SVFGNodeSet& getSinks() const
    {
        return sinks;
    }
    inline SVFGNodeSetIter sinksBegin() const
    {
        return sinks.begin();
    }
    inline SVFGNodeSetIter sinksEnd() const
    {
        return sinks.end();
    }
    inline void setPartialReachable()
    {
        partialReachable = true;
    }
    inline void setAllReachable()
    {
        fullReachable = true;
    }
    inline bool setReachGlobal()
    {
        return reachGlob = true;
    }
    inline bool isPartialReachable() const
    {
        return partialReachable || reachGlob;
    }
    inline bool isAllReachable() const
    {
        return fullReachable || reachGlob;
    }
    inline bool isReachGlobal() const
    {
        return reachGlob;
    }
    //@}

    /// Guarded reachability solve
    bool AllPathReachableSolve();
    bool isSatisfiableForAll();
    bool isSatisfiableForPairs();

    /// Get llvm value from a SVFGNode
    const Value* getLLVMValue(const SVFGNode* node) const;

    /// Get callsite ID and get returnsiteID from SVFGEdge
    //@{
    const CallBlockNode* getCallSite(const SVFGEdge* edge) const;
    const CallBlockNode* getRetSite(const SVFGEdge* edge) const;
    //@}

    /// Condition operations
    //@{
    inline Condition* condAnd(Condition* lhs, Condition* rhs)
    {
        return pathAllocator->condAnd(lhs,rhs);
    }
    inline Condition* condOr(Condition* lhs, Condition* rhs)
    {
        return pathAllocator->condOr(lhs,rhs);
    }
    inline Condition* condNeg(Condition* cond)
    {
        return pathAllocator->condNeg(cond);
    }
    inline Condition* getTrueCond() const
    {
        return pathAllocator->getTrueCond();
    }
    inline Condition* getFalseCond() const
    {
        return pathAllocator->getFalseCond();
    }
    inline std::string dumpCond(Condition* cond) const
    {
        return pathAllocator->dumpCond(cond);
    }
    /// Evaluate final condition
    std::string evalFinalCond() const;
    //@}

    /// Annotate program according to final condition
    void annotatePaths();

protected:
    inline const SVFG* getSVFG() const
    {
        return svfg;
    }

    /// Release memory
    void destroy();
    /// Clear Control flow conditions before each VF computation
    inline void clearCFCond()
    {
        /// TODO: how to clean bdd memory
        pathAllocator->clearCFCond();
    }

    /// Get/set VF (value-flow) and CF (control-flow) conditions
    //@{
    inline Condition* getVFCond(const SVFGNode* node) const
    {
        SVFGNodeToCondMap::const_iterator it = svfgNodeToCondMap.find(node);
        if(it==svfgNodeToCondMap.end())
        {
            return getFalseCond();
        }
        return it->second;
    }
    inline bool setVFCond(const SVFGNode* node, Condition* cond)
    {
        SVFGNodeToCondMap::iterator it = svfgNodeToCondMap.find(node);
        if(it!=svfgNodeToCondMap.end() && it->second == cond)
            return false;

        svfgNodeToCondMap[node] = cond;
        return true;
    }
    //@}

    /// Compute guards for value-flows
    //@{
    inline Condition* ComputeIntraVFGGuard(const BasicBlock* src, const BasicBlock* dst)
    {
        return pathAllocator->ComputeIntraVFGGuard(src,dst);
    }
    inline Condition* ComputeInterCallVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* callBB)
    {
        return pathAllocator->ComputeInterCallVFGGuard(src,dst,callBB);
    }
    inline Condition* ComputeInterRetVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* retBB)
    {
        return pathAllocator->ComputeInterRetVFGGuard(src,dst,retBB);
    }
    //@}

    /// Return the basic block where a SVFGNode resides in
    /// a SVFGNode may not in a basic block if it is not a program statement
    /// (e.g. PAGEdge is an global assignment or NullPtrSVFGNode)
    inline const BasicBlock* getSVFGNodeBB(const SVFGNode* node) const
    {
        const ICFGNode* icfgNode = node->getICFGNode();
        if(SVFUtil::isa<NullPtrSVFGNode>(node) == false)
        {
            return icfgNode->getBB();
        }
        return nullptr;
    }

    /// Get/set current SVFG node
    //@{
    inline const SVFGNode* getCurSVFGNode() const
    {
        return _curSVFGNode;
    }
    inline void setCurSVFGNode(const SVFGNode* node)
    {
        _curSVFGNode = node;
        pathAllocator->setCurEvalVal(getLLVMValue(_curSVFGNode));
    }
    //@}
    /// Set final condition after all path reachability analysis
    inline void setFinalCond(Condition* cond)
    {
        finalCond = cond;
    }

private:
    SVFGNodeSet forwardslice;				///<  the forward slice
    SVFGNodeSet backwardslice;				///<  the backward slice
    SVFGNodeSet sinks;						///<  a set of sink nodes
    const SVFGNode* root;					///<  root node on the slice
    SVFGNodeToCondMap svfgNodeToCondMap;	///<  map a SVFGNode to its path condition starting from root
    bool partialReachable;					///<  reachable from some paths
    bool fullReachable;						///<  reachable from all paths
    bool reachGlob;							///<  Whether slice reach a global
    PathCondAllocator* pathAllocator;		///<  path condition allocator
    const SVFGNode* _curSVFGNode;			///<  current svfg node during guard computation
    Condition* finalCond;					///<  final condition
    const SVFG* svfg;						///<  SVFG
};

} // End namespace SVF

#endif /* PROGSLICE_H_ */
