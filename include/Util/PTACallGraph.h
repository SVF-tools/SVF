//===- PTACallGraph.h -- Call graph representation----------------------------//
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
 * PTACallGraph.h
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#ifndef PTACALLGRAPH_H_
#define PTACALLGRAPH_H_

#include "MemoryModel/GenericGraph.h"
#include "Util/AnalysisUtil.h"
#include "Util/BasicTypes.h"
#include <llvm/IR/Module.h>			// llvm module
#include <llvm/ADT/STLExtras.h>		/// map iter
#include <llvm/ADT/GraphTraits.h>	// graph traits
#include <llvm/ADT/DenseMap.h>	// llvm dense map

#include <set>



class PTACallGraphNode;

/*
 * Call Graph edge representing a calling relation between two functions
 * Multiple calls from function A to B are merged into one call edge
 * Each call edge has a set of direct callsites and a set of indirect callsites
 */
typedef GenericEdge<PTACallGraphNode> GenericCallGraphEdgeTy;
class PTACallGraphEdge : public GenericCallGraphEdgeTy {

public:
    typedef std::set<const llvm::Instruction*> CallInstSet;
    enum CEDGEK {
        CallRetEdge,TDForkEdge,TDJoinEdge,HareParForEdge
    };


private:
    CallInstSet directCalls;
    CallInstSet indirectCalls;

public:
    /// Constructor
    PTACallGraphEdge(PTACallGraphNode* s, PTACallGraphNode* d, CEDGEK kind): GenericCallGraphEdgeTy(s,d,kind) {
    }
    /// Destructor
    virtual ~PTACallGraphEdge() {
    }

    /// Get direct and indirect calls
    //@{
    inline CallInstSet& getDirectCalls() {
        return directCalls;
    }
    inline CallInstSet& getIndirectCalls() {
        return indirectCalls;
    }
    inline const CallInstSet& getDirectCalls() const {
        return directCalls;
    }
    inline const CallInstSet& getIndirectCalls() const {
        return indirectCalls;
    }
    //@}

    /// Add direct and indirect callsite
    //@{
    void addDirectCallSite(const llvm::Instruction* call) {
        assert((llvm::isa<llvm::CallInst>(call) || llvm::isa<llvm::InvokeInst>(call)) && "not a call or inovke??");
        assert(analysisUtil::getCallee(call) && "not a direct callsite??");
        directCalls.insert(call);
    }

    void addInDirectCallSite(const llvm::Instruction* call) {
        assert((llvm::isa<llvm::CallInst>(call) || llvm::isa<llvm::InvokeInst>(call)) && "not a call or inovke??");
        assert((NULL == analysisUtil::getCallee(call) || NULL == llvm::dyn_cast<llvm::Function> (analysisUtil::getForkedFun(call))) && "not an indirect callsite??");
        indirectCalls.insert(call);
    }
    //@}

    /// Iterators for direct and indirect callsites
    //@{
    inline CallInstSet::iterator directCallsBegin() const {
        return directCalls.begin();
    }
    inline CallInstSet::iterator directCallsEnd() const {
        return directCalls.end();
    }

    inline CallInstSet::iterator indirectCallsBegin() const {
        return indirectCalls.begin();
    }
    inline CallInstSet::iterator indirectCallsEnd() const {
        return indirectCalls.end();
    }
    //@}

    /// ClassOf
    //@{
    static inline bool classof(const PTACallGraphEdge *edge) {
        return true;
    }
    static inline bool classof(const GenericCallGraphEdgeTy *edge) {
        return edge->getEdgeKind() == PTACallGraphEdge::CallRetEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
    }
    //@}

    typedef GenericNode<PTACallGraphNode,PTACallGraphEdge>::GEdgeSetTy CallGraphEdgeSet;

};

/*
 * Call Graph node representing a function
 */
typedef GenericNode<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphNodeTy;
class PTACallGraphNode : public GenericCallGraphNodeTy {

public:
    typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef PTACallGraphEdge::CallGraphEdgeSet::iterator iterator;
    typedef PTACallGraphEdge::CallGraphEdgeSet::const_iterator const_iterator;

private:
    const llvm::Function* fun;

public:
    /// Constructor
    PTACallGraphNode(NodeID i, const llvm::Function* f) : GenericCallGraphNodeTy(i,0), fun(f) {

    }

    /// Get function of this call node
    inline const llvm::Function* getFunction() const {
        return fun;
    }

    /// Return TRUE if this function can be reached from main.
    bool isReachableFromProgEntry() const;
};

/*!
 * Pointer Analysis Call Graph used internally for various pointer analysis
 */
typedef GenericGraph<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphTy;
class PTACallGraph : public GenericCallGraphTy {

public:
    typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef llvm::DenseMap<const llvm::Function*, PTACallGraphNode *> FunToCallGraphNodeMap;
    typedef llvm::DenseMap<const llvm::Instruction*, CallGraphEdgeSet> CallInstToCallGraphEdgesMap;
    typedef std::pair<llvm::CallSite, const llvm::Function*> CallSitePair;
    typedef std::map<CallSitePair, CallSiteID> CallSiteToIdMap;
    typedef std::map<CallSiteID, CallSitePair> IdToCallSiteMap;
    typedef	std::set<const llvm::Function*> FunctionSet;
    typedef std::map<llvm::CallSite, FunctionSet> CallEdgeMap;
    typedef CallGraphEdgeSet::iterator CallGraphNodeIter;

private:
    llvm::Module* mod;

    /// Indirect call map
    CallEdgeMap indirectCallMap;

    /// Call site information
    static CallSiteToIdMap csToIdMap;	///< Map a pair of call instruction and callee to a callsite ID
    static IdToCallSiteMap idToCSMap;	///< Map a callsite ID to a pair of call instruction and callee
    static CallSiteID totalCallSiteNum;	///< CallSiteIDs, start from 1;

    FunToCallGraphNodeMap funToCallGraphNodeMap; ///< Call Graph node map
    CallInstToCallGraphEdgesMap callinstToCallGraphEdgesMap; ///< Map a call instruction to its corresponding call edges

    NodeID callGraphNodeNum;
    Size_t numOfResolvedIndCallEdge;

    /// Build Call Graph
    void buildCallGraph(llvm::Module* module);

    /// Add callgraph Node
    void addCallGraphNode(const llvm::Function* fun);

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    PTACallGraph(llvm::Module* module)
        : mod(module), callGraphNodeNum(0), numOfResolvedIndCallEdge(0) {
        buildCallGraph(module);
    }
    /// Destructor
    virtual ~PTACallGraph() {
        destroy();
    }


    /// Get callees from an indirect callsite
    //@{
    inline CallEdgeMap& getIndCallMap() {
        return indirectCallMap;
    }
    inline bool hasIndCSCallees(llvm::CallSite cs) const {
        return (indirectCallMap.find(cs) != indirectCallMap.end());
    }
    inline const FunctionSet& getIndCSCallees(llvm::CallSite cs) const {
        CallEdgeMap::const_iterator it = indirectCallMap.find(cs);
        assert(it!=indirectCallMap.end() && "not an indirect callsite!");
        return it->second;
    }
    inline const FunctionSet& getIndCSCallees(llvm::CallInst* csInst) const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(csInst);
        return getIndCSCallees(cs);
    }
    //@}
    inline u32_t getTotalCallSiteNumber() const {
        return totalCallSiteNum;
    }

    inline Size_t getNumOfResolvedIndCallEdge() const {
        return numOfResolvedIndCallEdge;
    }

    /// Issue a warning if the function which has indirect call sites can not be reached from program entry.
    void vefityCallGraph();

    /// Get call graph node
    //@{
    inline PTACallGraphNode* getCallGraphNode(NodeID id) const {
        return getGNode(id);
    }
    inline PTACallGraphNode* getCallGraphNode(const llvm::Function* fun) const {
        FunToCallGraphNodeMap::const_iterator it = funToCallGraphNodeMap.find(fun);
        assert(it!=funToCallGraphNodeMap.end() && "call graph node not found!!");
        return it->second;
    }
    //@}

    /// Add/Get CallSiteID
    //@{
    inline void addCallSite(llvm::CallSite cs, const llvm::Function* callee) {
        std::pair<llvm::CallSite, const llvm::Function*> newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        //assert(it == csToIdMap.end() && "cannot add a callsite twice");
        if(it == csToIdMap.end()) {
            CallSiteID id = totalCallSiteNum++;
            csToIdMap.insert(std::make_pair(newCS, id));
            idToCSMap.insert(std::make_pair(id, newCS));
        }
    }
    inline CallSiteID getCallSiteID(llvm::CallSite cs, const llvm::Function* callee) const {
        CallSitePair newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        assert(it != csToIdMap.end() && "callsite id not found! This maybe a partially resolved callgraph, please check the indCallEdge limit");
        return it->second;
    }
    inline bool hasCallSiteID(llvm::CallSite cs, const llvm::Function* callee) const {
        CallSitePair newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        return it != csToIdMap.end();
    }
    inline const CallSitePair& getCallSitePair(CallSiteID id) const {
        IdToCallSiteMap::const_iterator it = idToCSMap.find(id);
        assert(it != idToCSMap.end() && "cannot find call site for this CallSiteID");
        return (it->second);
    }
    inline llvm::CallSite getCallSite(CallSiteID id) const {
        return getCallSitePair(id).first;
    }
    inline const llvm::Function* getCallerOfCallSite(CallSiteID id) const {
        return getCallSite(id).getCaller();
    }
    inline const llvm::Function* getCalleeOfCallSite(CallSiteID id) const {
        return getCallSitePair(id).second;
    }
    //@}
    /// Get Module
    inline llvm::Module* getModule() {
        return mod;
    }
    /// Whether we have aleady created this call graph edge
    PTACallGraphEdge* hasGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind) const;
    /// Get call graph edge via nodes
    PTACallGraphEdge* getGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind);
    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasCallGraphEdge(const llvm::Instruction* inst) const {
        return callinstToCallGraphEdgesMap.find(inst)!=callinstToCallGraphEdgesMap.end();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeBegin(const llvm::Instruction* inst) const {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.begin();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeEnd(const llvm::Instruction* inst) const {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.end();
    }
    //@}
    /// map call instruction to its CallGraphEdge map
    inline void addCallGraphEdgeSetMap(const llvm::Instruction* inst, PTACallGraphEdge* edge) {
        if (callinstToCallGraphEdgesMap[inst].insert(edge).second) {
            /// Record <CallSite,Callee> pair
            llvm::CallSite cs = analysisUtil::getLLVMCallSite(inst);
            addCallSite(cs, edge->getDstNode()->getFunction());
        }
    }
    /// Add call graph edge
    inline void addEdge(PTACallGraphEdge* edge) {
        edge->getDstNode()->addIncomingEdge(edge);
        edge->getSrcNode()->addOutgoingEdge(edge);
    }

    /// Add direct/indirect call edges
    //@{
    void addDirectCallGraphEdge(const llvm::Instruction* call);
    void addIndirectCallGraphEdge(const llvm::Instruction* call, const llvm::Function* callee);
    //@}

    /// Get callsites invoking the callee
    //@{
    void getAllCallSitesInvokingCallee(const llvm::Function* callee, PTACallGraphEdge::CallInstSet& csSet);
    void getDirCallSitesInvokingCallee(const llvm::Function* callee, PTACallGraphEdge::CallInstSet& csSet);
    void getIndCallSitesInvokingCallee(const llvm::Function* callee, PTACallGraphEdge::CallInstSet& csSet);
    //@}

    /// Dump the graph
    void dump(const std::string& filename);
};


namespace llvm {
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<PTACallGraphNode*> : public GraphTraits<GenericNode<PTACallGraphNode,PTACallGraphEdge>*  > {
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<PTACallGraphNode *> > : public GraphTraits<Inverse<GenericNode<PTACallGraphNode,PTACallGraphEdge>* > > {
};

template<> struct GraphTraits<PTACallGraph*> : public GraphTraits<GenericGraph<PTACallGraphNode,PTACallGraphEdge>* > {
  typedef PTACallGraphNode *NodeRef;
};


}


#endif /* PTACALLGRAPH_H_ */
