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

#include "SABER/CFLSolver.h"
#include "MSSA/SVFGOPT.h"
#include "SABER/ProgSlice.h"
#include "SABER/SaberSVFGBuilder.h"
#include "WPA/Andersen.h"

typedef CFLSolver<SVFG*,CxtDPItem> CFLSrcSnkSolver;

/*!
 * General source-sink analysis, which serves as a base analysis to be extended for various clients
 */
class SrcSnkDDA : public CFLSrcSnkSolver {

public:
    typedef ProgSlice::SVFGNodeSet SVFGNodeSet;
    typedef std::map<const SVFGNode*,ProgSlice*> SVFGNodeToSliceMap;
    typedef SVFGNodeSet::iterator SVFGNodeSetIter;
    typedef CxtDPItem DPIm;
    typedef std::set<DPIm> DPImSet;							///< dpitem set
    typedef std::map<const SVFGNode*, DPImSet> SVFGNodeToDPItemsMap; 	///< map a SVFGNode to its visited dpitems

private:
    ProgSlice* _curSlice;		/// current program slice
    SVFGNodeSet sources;		/// source nodes
    SVFGNodeSet sinks;		/// source nodes
    PathCondAllocator* pathCondAllocator;
    SVFGNodeToDPItemsMap nodeToDPItemsMap;	///<  record forward visited dpitems
    SVFGNodeSet visitedSet;	///<  record backward visited nodes
    SaberSVFGBuilder memSSA;
    SVFG* svfg;
    PTACallGraph* ptaCallGraph;
public:

    /// Constructor
    SrcSnkDDA() : _curSlice(NULL), svfg(NULL), ptaCallGraph(NULL) {
        pathCondAllocator = new PathCondAllocator();
    }
    /// Destructor
    virtual ~SrcSnkDDA() {
        if (svfg != NULL)
            delete svfg;
        svfg = NULL;

        if (ptaCallGraph != NULL)
            delete ptaCallGraph;
        ptaCallGraph = NULL;

        //if(pathCondAllocator)
        //    delete pathCondAllocator;
        //pathCondAllocator = NULL;
    }

    /// Start analysis here
    virtual void analyze(SVFModule module);

    /// Initialize analysis
    virtual void initialize(SVFModule module) {
        ptaCallGraph = new PTACallGraph(module);
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(module);
        svfg =  memSSA.buildSVFG(ander);
        setGraph(memSSA.getSVFG());
        //AndersenWaveDiff::releaseAndersenWaveDiff();
        /// allocate control-flow graph branch conditions
        getPathAllocator()->allocate(module);

        initSrcs();
        initSnks();
    }

    /// Finalize analysis
    virtual void finalize() {
        dumpSlices();
    }

    /// Get SVFG
    inline const SVFG* getSVFG() const {
        return graph();
    }

    /// Whether this svfg node may access global variable
    inline bool isGlobalSVFGNode(const SVFGNode* node) const {
        return memSSA.isGlobalSVFGNode(node);
    }
    /// Slice operations
    //@{
    void setCurSlice(const SVFGNode* src);

    inline ProgSlice* getCurSlice() const {
        return _curSlice;
    }
    inline void addSinkToCurSlice(const SVFGNode* node) {
        _curSlice->addToSinks(node);
        addToCurForwardSlice(node);
    }
    inline bool isInCurForwardSlice(const SVFGNode* node) {
        return _curSlice->inForwardSlice(node);
    }
    inline bool isInCurBackwardSlice(const SVFGNode* node) {
        return _curSlice->inBackwardSlice(node);
    }
    inline void addToCurForwardSlice(const SVFGNode* node) {
        _curSlice->addToForwardSlice(node);
    }
    inline void addToCurBackwardSlice(const SVFGNode* node) {
        _curSlice->addToBackwardSlice(node);
    }
    //@}

    /// Initialize sources and sinks
    ///@{
    virtual void initSrcs() = 0;
    virtual void initSnks() = 0;
    virtual bool isSourceLikeFun(const llvm::Function* fun) = 0;
    virtual bool isSinkLikeFun(const llvm::Function* fun) = 0;
    virtual bool isSource(const SVFGNode* node) = 0;
    virtual bool isSink(const SVFGNode* node) = 0;
    ///@}

    /// report bug on the current analyzed slice
    virtual void reportBug(ProgSlice* slice) = 0;

    /// Get sources/sinks
    //@{
    inline const SVFGNodeSet& getSources() const {
        return sources;
    }
    inline SVFGNodeSetIter sourcesBegin() const {
        return sources.begin();
    }
    inline SVFGNodeSetIter sourcesEnd() const {
        return sources.end();
    }
    inline void addToSources(const SVFGNode* node) {
        sources.insert(node);
    }
    inline const SVFGNodeSet& getSinks() const {
        return sinks;
    }
    inline SVFGNodeSetIter sinksBegin() const {
        return sinks.begin();
    }
    inline SVFGNodeSetIter sinksEnd() const {
        return sinks.end();
    }
    inline void addToSinks(const SVFGNode* node) {
        sinks.insert(node);
    }
    //@}

    /// Get path condition allocator
    PathCondAllocator* getPathAllocator() const {
        return pathCondAllocator;
    }

protected:
    /// Forward traverse
    virtual inline void forwardProcess(const DPIm& item) {
        const SVFGNode* node = getNode(item.getCurNodeID());
        if(isSink(node)) {
            addSinkToCurSlice(node);
            _curSlice->setPartialReachable();
        }
        else
            addToCurForwardSlice(node);
    }
    /// Backward traverse
    virtual inline void backwardProcess(const DPIm& item) {
        const SVFGNode* node = getNode(item.getCurNodeID());
        if(isInCurForwardSlice(node)) {
            addToCurBackwardSlice(node);
        }
    }
    /// Propagate information forward by matching context
    virtual void forwardpropagate(const DPIm& item, SVFGEdge* edge);
    /// Propagate information backward without matching context, as forward analysis already did it
    virtual void backwardpropagate(const DPIm& item, SVFGEdge* edge);
    /// Whether has been visited or not, in order to avoid recursion on SVFG
    //@{
    inline bool forwardVisited(const SVFGNode* node, const DPIm& item) {
        SVFGNodeToDPItemsMap::iterator it = nodeToDPItemsMap.find(node);
        if(it!=nodeToDPItemsMap.end())
            return it->second.find(item)!=it->second.end();
        else
            return false;
    }
    inline void addForwardVisited(const SVFGNode* node, const DPIm& item) {
        nodeToDPItemsMap[node].insert(item);
    }
    inline bool backwardVisited(const SVFGNode* node) {
        return visitedSet.find(node)!=visitedSet.end();
    }
    inline void addBackwardVisited(const SVFGNode* node) {
        visitedSet.insert(node);
    }
    inline void clearVisitedMap() {
        nodeToDPItemsMap.clear();
        visitedSet.clear();
    }
    //@}

    /// Guarded reachability search
    //@{
    virtual void AllPathReachability();
    inline bool isSatisfiableForAll(ProgSlice* slice) {
        return slice->isSatisfiableForAll();
    }
    inline bool isSatisfiableForPairs(ProgSlice* slice) {
        return slice->isSatisfiableForPairs();
    }
    //@}
    /// Whether it is all path reachable from a source
    virtual bool isAllPathReachable() {
        return _curSlice->isAllReachable();
    }
    /// Whether it is some path reachable from a source
    virtual bool isSomePathReachable() {
        return _curSlice->isPartialReachable();
    }
    /// Dump SVFG with annotated slice informaiton
    //@{
    void dumpSlices();
    void annotateSlice(ProgSlice* slice);
    void printBDDStat();
    //@}

};


#endif /* SRCSNKDDA_H_ */
