//===- ThreadCallGraph.h -- Call graph considering thread fork/join-----------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * ThreadCallGraph.h
 *
 *  Created on: Jul 7, 2014
 *      Authors: Peng Di, Yulei Sui
 */

#ifndef RCG_H_
#define RCG_H_

#include <Util/PTACallGraph.h>
#include "WPA/Andersen.h"

/*!
 * PTA thread fork edge from fork site to the entry of a start routine function
 */
class ThreadForkEdge: public PTACallGraphEdge {

public:
    /// Constructor
    ThreadForkEdge(PTACallGraphNode* s, PTACallGraphNode* d) :
        PTACallGraphEdge(s, d, PTACallGraphEdge::TDForkEdge) {
    }
    /// Destructor
    virtual ~ThreadForkEdge() {
    }

    /// ClassOf
    //@{
    static inline bool classof(const ThreadForkEdge *edge) {
        return true;
    }
    static inline bool classof(const PTACallGraphEdge *edge) {
        return edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge;
    }
    //@}

    typedef GenericNode<PTACallGraphNode, ThreadForkEdge>::GEdgeSetTy ForkEdgeSet;
};

/*!
 * PTA thread join edge from the exit of a start routine function to a join point of the thread
 */
class ThreadJoinEdge: public PTACallGraphEdge {

public:
    /// Constructor
    ThreadJoinEdge(PTACallGraphNode* s, PTACallGraphNode* d) :
        PTACallGraphEdge(s, d, PTACallGraphEdge::TDJoinEdge) {
    }
    /// Destructor
    virtual ~ThreadJoinEdge() {
    }

    static inline bool classof(const ThreadJoinEdge *edge) {
        return true;
    }
    static inline bool classof(const PTACallGraphEdge *edge) {
        return edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
    }

    typedef GenericNode<PTACallGraphNode, ThreadJoinEdge>::GEdgeSetTy JoinEdgeSet;
};

/*!
 * Thread sensitive call graph
 */
class ThreadCallGraph: public PTACallGraph {

public:
    typedef std::set<const llvm::CallInst*> CallSiteSet;
    typedef std::set<const llvm::Instruction*> InstSet;
    typedef std::vector<const llvm::Instruction*> InstVector;
    typedef std::map<const llvm::CallInst*, InstSet> CallToInstMap;
    typedef std::set<const llvm::BasicBlock*> BBSet;
    typedef std::vector<const llvm::BasicBlock*> BBVector;
    typedef std::map<const llvm::BasicBlock*, const llvm::Instruction*> BBToInstMap;
    typedef std::set<CallSiteSet*> CtxSet;
    typedef ThreadForkEdge::ForkEdgeSet ForkEdgeSet;
    typedef std::map<const llvm::Instruction*, ForkEdgeSet> CallInstToForkEdgesMap;
    typedef ThreadJoinEdge::JoinEdgeSet JoinEdgeSet;
    typedef std::map<const llvm::Instruction*, JoinEdgeSet> CallInstToJoinEdgesMap;

    /// Constructor
    ThreadCallGraph(llvm::Module* module);
    /// Destructor
    virtual ~ThreadCallGraph() {
    }

    /// Update call graph using pointer results
    void updateCallGraph(PointerAnalysis* pta);
    /// Update join edge using pointer analysis results
    void updateJoinEdge(PointerAnalysis* pta);


    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasThreadForkEdge(const llvm::Instruction* inst) const {
        return callinstToThreadForkEdgesMap.find(inst) != callinstToThreadForkEdgesMap.end();
    }
    inline ForkEdgeSet::const_iterator getForkEdgeBegin(const llvm::Instruction* inst) const {
        CallInstToForkEdgesMap::const_iterator it = callinstToThreadForkEdgesMap.find(inst);
        assert(it != callinstToThreadForkEdgesMap.end() && "call instruction not found");
        return it->second.begin();
    }
    inline ForkEdgeSet::const_iterator getForkEdgeEnd(const llvm::Instruction* inst) const {
        CallInstToForkEdgesMap::const_iterator it = callinstToThreadForkEdgesMap.find(inst);
        assert(it != callinstToThreadForkEdgesMap.end() && "call instruction not found");
        return it->second.end();
    }

    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasThreadJoinEdge(const llvm::Instruction* inst) const {
        return callinstToThreadJoinEdgesMap.find(inst) != callinstToThreadJoinEdgesMap.end();
    }
    inline JoinEdgeSet::const_iterator getJoinEdgeBegin(const llvm::Instruction* inst) const {
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(inst);
        assert(it != callinstToThreadJoinEdgesMap.end() && "call instruction does not have a valid callee");
        return it->second.begin();
    }
    inline JoinEdgeSet::const_iterator getJoinEdgeEnd(const llvm::Instruction* inst) const {
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(inst);
        assert(it != callinstToThreadJoinEdgesMap.end() && "call instruction does not have a valid callee");
        return it->second.end();
    }
    inline void getJoinSites(const PTACallGraphNode* routine, InstSet& csSet) {
        for(CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.begin(), eit = callinstToThreadJoinEdgesMap.end(); it!=eit; ++it) {
            for(JoinEdgeSet::const_iterator jit = it->second.begin(), ejit = it->second.end(); jit!=ejit; ++jit) {
                if((*jit)->getDstNode() == routine) {
                    csSet.insert(it->first);
                }
            }
        }
    }
    //@}

    /// Whether a callsite is a fork or join
    ///@{
    inline bool isForksite(const llvm::CallInst* callInst) {
        return forksites.find(callInst) != forksites.end();
    }
    inline bool isJoinsite(const llvm::CallInst* callInst) {
        return joinsites.find(callInst) != joinsites.end();
    }
    ///

    /// Fork sites iterators
    //@{
    inline CallSiteSet::iterator forksitesBegin() const {
        return forksites.begin();
    }
    inline CallSiteSet::iterator forksitesEnd() const {
        return forksites.end();
    }
    //@}

    /// Join sites iterators
    //@{
    inline CallSiteSet::iterator joinsitesBegin() const {
        return joinsites.begin();
    }
    inline CallSiteSet::iterator joinsitesEnd() const {
        return joinsites.end();
    }
    //@}

    /// Num of fork/join sites
    //@{
    inline u32_t getNumOfForksite() const {
        return forksites.size();
    }
    inline u32_t getNumOfJoinsite() const {
        return joinsites.size();
    }
    //@}

    /// Thread API
    inline ThreadAPI* getThreadAPI() const {
        return tdAPI;
    }

private:
    /// map call instruction to its CallGraphEdge map
    inline void addThreadForkEdgeSetMap(const llvm::Instruction* inst, ThreadForkEdge* edge) {
        if(edge!=NULL) {
            callinstToThreadForkEdgesMap[inst].insert(edge);
            addCallGraphEdgeSetMap(inst,edge);
        }
    }

    /// map call instruction to its CallGraphEdge map
    inline void addThreadJoinEdgeSetMap(const llvm::Instruction* inst, ThreadJoinEdge* edge) {
        if(edge!=NULL) {
            callinstToThreadJoinEdgesMap[inst].insert(edge);
            addCallGraphEdgeSetMap(inst,edge);
        }
    }
    /// has thread join edge
    inline ThreadJoinEdge* hasThreadJoinEdge(const llvm::Instruction* call, PTACallGraphNode* joinFunNode, PTACallGraphNode* threadRoutineFunNode) const {
        ThreadJoinEdge joinEdge(joinFunNode,threadRoutineFunNode);
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(call);
        if(it != callinstToThreadJoinEdgesMap.end()) {
            JoinEdgeSet::const_iterator jit = it->second.find(&joinEdge);
            if(jit!=it->second.end())
                return *jit;
        }
        return NULL;
    }

    /// Add direct/indirect thread fork edges
    //@{
    void addDirectForkEdge(const llvm::Instruction* call);
    void addIndirectForkEdge(const llvm::Instruction* call, const llvm::Function* callee);
    //@}

    /// Add thread join edges
    //@{
    void addDirectJoinEdge(const llvm::Instruction* call,const CallSiteSet& forksite);
    //@}

    /// Start building Thread Call graph
    virtual void build(llvm::Module* m);

    /// Add fork sites which directly or indirectly create a thread
    //@{
    inline bool addForksite(const llvm::CallInst* callInst) {
        callinstToThreadForkEdgesMap[callInst];
        return forksites.insert(callInst).second;
    }
    inline bool addJoinsite(const llvm::CallInst* callInst) {
        callinstToThreadJoinEdgesMap[callInst];
        return joinsites.insert(callInst).second;
    }
    //@}

private:
    ThreadAPI* tdAPI;		///< Thread API
    CallSiteSet forksites; ///< all thread fork sites
    CallSiteSet joinsites; ///< all thread fork sites
    CallInstToForkEdgesMap callinstToThreadForkEdgesMap; ///< Map a call instruction to its corresponding fork edges
    CallInstToJoinEdgesMap callinstToThreadJoinEdgesMap; ///< Map a call instruction to its corresponding join edges
};


#endif /* RCG_H_ */
