//===- SrcSnkDDA.h -- Source-sink analyzer-----------------------------------//
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
 * SrcSnkDDA.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef SRCSNKANALYSIS_H_
#define SRCSNKANALYSIS_H_

#include "Util/CFLSolver.h"
#include "Graphs/SVFGOPT.h"
#include "SABER/ProgSlice.h"
#include "SABER/SaberSVFGBuilder.h"

namespace SVF
{

typedef CFLSolver<SVFG*,CxtDPItem> CFLSrcSnkSolver;

/*!
 * General source-sink analysis, which serves as a base analysis to be extended for various clients
 */
class SrcSnkDDA : public CFLSrcSnkSolver
{

public:
    typedef ProgSlice::SVFGNodeSet SVFGNodeSet;
    typedef Map<const SVFGNode*,ProgSlice*> SVFGNodeToSliceMap;
    typedef SVFGNodeSet::const_iterator SVFGNodeSetIter;
    typedef CxtDPItem DPIm;
    typedef Set<DPIm> DPImSet;							///< dpitem set
    typedef Map<const SVFGNode*, DPImSet> SVFGNodeToDPItemsMap; 	///< map a SVFGNode to its visited dpitems
    typedef Set<const CallBlockNode*> CallSiteSet;
    typedef NodeBS SVFGNodeBS;
    typedef ProgSlice::VFWorkList WorkList;

private:
    ProgSlice* _curSlice;		/// current program slice
    SVFGNodeSet sources;		/// source nodes
    SVFGNodeSet sinks;		/// source nodes
    PathCondAllocator* pathCondAllocator;
    SVFGNodeToDPItemsMap nodeToDPItemsMap;	///<  record forward visited dpitems
    SVFGNodeSet visitedSet;	///<  record backward visited nodes

protected:
    SaberSVFGBuilder memSSA;
    SVFG* svfg;
    PTACallGraph* ptaCallGraph;

public:

    /// Constructor
    SrcSnkDDA() : _curSlice(nullptr), svfg(nullptr), ptaCallGraph(nullptr)
    {
        pathCondAllocator = new PathCondAllocator();
    }
    /// Destructor
    virtual ~SrcSnkDDA()
    {
        if (svfg != nullptr)
            delete svfg;
        svfg = nullptr;

        if (_curSlice != nullptr)
            delete _curSlice;
        _curSlice = nullptr;

        /// the following shared by multiple checkers, thus can not be released.
        //if (ptaCallGraph != nullptr)
        //    delete ptaCallGraph;
        //ptaCallGraph = nullptr;

        //if(pathCondAllocator)
        //    delete pathCondAllocator;
        //pathCondAllocator = nullptr;
    }

    /// Start analysis here
    virtual void analyze(SVFModule* module);

    /// Initialize analysis
    virtual void initialize(SVFModule* module);

    /// Finalize analysis
    virtual void finalize()
    {
        dumpSlices();
    }

    /// Get PAG
    PAG* getPAG() const
    {
        return PAG::getPAG();
    }

    /// Get SVFG
    inline const SVFG* getSVFG() const
    {
        return graph();
    }

    /// Get Callgraph
    inline PTACallGraph* getCallgraph() const
    {
        return ptaCallGraph;
    }

    /// Whether this svfg node may access global variable
    inline bool isGlobalSVFGNode(const SVFGNode* node) const
    {
        return memSSA.isGlobalSVFGNode(node);
    }
    /// Slice operations
    //@{
    virtual void setCurSlice(const SVFGNode* src);

    inline ProgSlice* getCurSlice() const
    {
        return _curSlice;
    }
    inline void addSinkToCurSlice(const SVFGNode* node)
    {
        _curSlice->addToSinks(node);
        addToCurForwardSlice(node);
    }
    inline bool isInCurForwardSlice(const SVFGNode* node)
    {
        return _curSlice->inForwardSlice(node);
    }
    inline bool isInCurBackwardSlice(const SVFGNode* node)
    {
        return _curSlice->inBackwardSlice(node);
    }
    inline void addToCurForwardSlice(const SVFGNode* node)
    {
        _curSlice->addToForwardSlice(node);
    }
    inline void addToCurBackwardSlice(const SVFGNode* node)
    {
        _curSlice->addToBackwardSlice(node);
    }
    //@}

    /// Initialize sources and sinks
    ///@{
    virtual void initSrcs() = 0;
    virtual void initSnks() = 0;
    virtual bool isSourceLikeFun(const SVFFunction* fun) {
        return false;
    }

    virtual bool isSinkLikeFun(const SVFFunction* fun) {
        return false;
    }

    bool isSource(const SVFGNode* node) const {
        return getSources().find(node)!=getSources().end();
    }

    bool isSink(const SVFGNode* node) const {
        return getSinks().find(node)!=getSinks().end();
    }
    ///@}

    /// Identify allocation wrappers
    bool isInAWrapper(const SVFGNode* src, CallSiteSet& csIdSet);

    /// report bug on the current analyzed slice
    virtual void reportBug(ProgSlice* slice) = 0;

    /// Get sources/sinks
    //@{
    inline const SVFGNodeSet& getSources() const
    {
        return sources;
    }
    inline SVFGNodeSetIter sourcesBegin() const
    {
        return sources.begin();
    }
    inline SVFGNodeSetIter sourcesEnd() const
    {
        return sources.end();
    }
    inline void addToSources(const SVFGNode* node)
    {
        sources.insert(node);
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
    inline void addToSinks(const SVFGNode* node)
    {
        sinks.insert(node);
    }
    //@}

    /// Get path condition allocator
    PathCondAllocator* getPathAllocator() const
    {
        return pathCondAllocator;
    }

protected:
    /// Forward traverse
    virtual inline void FWProcessCurNode(const DPIm& item)
    {
        const SVFGNode* node = getNode(item.getCurNodeID());
        if(isSink(node))
        {
            addSinkToCurSlice(node);
            _curSlice->setPartialReachable();
        }
        else
            addToCurForwardSlice(node);
    }
    /// Backward traverse
    virtual inline void BWProcessCurNode(const DPIm& item)
    {
        const SVFGNode* node = getNode(item.getCurNodeID());
        if(isInCurForwardSlice(node))
        {
            addToCurBackwardSlice(node);
        }
    }
    /// Propagate information forward by matching context
    virtual void FWProcessOutgoingEdge(const DPIm& item, SVFGEdge* edge);
    /// Propagate information backward without matching context, as forward analysis already did it
    virtual void BWProcessIncomingEdge(const DPIm& item, SVFGEdge* edge);
    /// Whether has been visited or not, in order to avoid recursion on SVFG
    //@{
    inline bool forwardVisited(const SVFGNode* node, const DPIm& item)
    {
        SVFGNodeToDPItemsMap::const_iterator it = nodeToDPItemsMap.find(node);
        if(it!=nodeToDPItemsMap.end())
            return it->second.find(item)!=it->second.end();
        else
            return false;
    }
    inline void addForwardVisited(const SVFGNode* node, const DPIm& item)
    {
        nodeToDPItemsMap[node].insert(item);
    }
    inline bool backwardVisited(const SVFGNode* node)
    {
        return visitedSet.find(node)!=visitedSet.end();
    }
    inline void addBackwardVisited(const SVFGNode* node)
    {
        visitedSet.insert(node);
    }
    inline void clearVisitedMap()
    {
        nodeToDPItemsMap.clear();
        visitedSet.clear();
    }
    //@}

    /// Whether it is all path reachable from a source
    virtual bool isAllPathReachable()
    {
        return _curSlice->isAllReachable();
    }
    /// Whether it is some path reachable from a source
    virtual bool isSomePathReachable()
    {
        return _curSlice->isPartialReachable();
    }
    /// Dump SVFG with annotated slice informaiton
    //@{
    void dumpSlices();
    void annotateSlice(ProgSlice* slice);
    void printBDDStat();
    //@}

};

} // End namespace SVF

#endif /* SRCSNKDDA_H_ */
