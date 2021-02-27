//===- ThreadCallGraph.h -- Call graph considering thread fork/join-----------//
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
 * ThreadCallGraph.h
 *
 *  Created on: Jul 7, 2014
 *      Authors: Peng Di, Yulei Sui, Ding Ye
 */

#ifndef RCG_H_
#define RCG_H_

#include "Graphs/PTACallGraph.h"
#include "MemoryModel/PointerAnalysisImpl.h"

namespace SVF
{

class SVFModule;
class ThreadAPI;
/*!
 * PTA thread fork edge from fork site to the entry of a start routine function
 */
class ThreadForkEdge: public PTACallGraphEdge
{

public:
    /// Constructor
    ThreadForkEdge(PTACallGraphNode* s, PTACallGraphNode* d, CallSiteID csId) :
        PTACallGraphEdge(s, d, PTACallGraphEdge::TDForkEdge, csId)
    {
    }
    /// Destructor
    virtual ~ThreadForkEdge()
    {
    }

    /// ClassOf
    //@{
    static inline bool classof(const ThreadForkEdge*)
    {
        return true;
    }
    static inline bool classof(const PTACallGraphEdge *edge)
    {
        return edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge;
    }
    //@}

    virtual const std::string toString() const {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "ThreadForkEdge ";
        rawstr << "CallSite ID: " << getCallSiteID();
        rawstr << " srcNode ID " << getSrcID() << " (fun: " << getSrcNode()->getFunction()->getName() << ")";
        rawstr << " dstNode ID " << getDstID() << " (fun: " << getDstNode()->getFunction()->getName() << ")";
        return rawstr.str();
    }

    typedef GenericNode<PTACallGraphNode, ThreadForkEdge>::GEdgeSetTy ForkEdgeSet;
};

/*!
 * PTA thread join edge from the exit of a start routine function to a join point of the thread
 */
class ThreadJoinEdge: public PTACallGraphEdge
{

public:
    /// Constructor
    ThreadJoinEdge(PTACallGraphNode* s, PTACallGraphNode* d, CallSiteID csId) :
        PTACallGraphEdge(s, d, PTACallGraphEdge::TDJoinEdge, csId)
    {
    }
    /// Destructor
    virtual ~ThreadJoinEdge()
    {
    }

    static inline bool classof(const ThreadJoinEdge*)
    {
        return true;
    }
    static inline bool classof(const PTACallGraphEdge *edge)
    {
        return edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
    }

    virtual const std::string toString() const {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "ThreadJoinEdge ";
        rawstr << "CallSite ID: " << getCallSiteID();
        rawstr << " srcNode ID " << getSrcID() << " (fun: " << getSrcNode()->getFunction()->getName() << ")";
        rawstr << " dstNode ID " << getDstID() << " (fun: " << getDstNode()->getFunction()->getName() << ")";
        return rawstr.str();
    }

    typedef GenericNode<PTACallGraphNode, ThreadJoinEdge>::GEdgeSetTy JoinEdgeSet;
};

/*!
 * hare_parallel_for edge from fork site to the entry of a start routine function
 */
class HareParForEdge: public PTACallGraphEdge
{

public:
    /// Constructor
    HareParForEdge(PTACallGraphNode* s, PTACallGraphNode* d, CallSiteID csId) :
        PTACallGraphEdge(s, d, PTACallGraphEdge::HareParForEdge, csId)
    {
    }
    /// Destructor
    virtual ~HareParForEdge()
    {
    }

    /// ClassOf
    //@{
    static inline bool classof(const HareParForEdge*)
    {
        return true;
    }
    static inline bool classof(const PTACallGraphEdge *edge)
    {
        return edge->getEdgeKind() == PTACallGraphEdge::HareParForEdge;
    }
    //@}

    typedef GenericNode<PTACallGraphNode, HareParForEdge>::GEdgeSetTy ParForEdgeSet;
};


/*!
 * Thread sensitive call graph
 */
class ThreadCallGraph: public PTACallGraph
{

public:
    typedef Set<const CallBlockNode*> InstSet;
    typedef InstSet CallSiteSet;
    typedef std::vector<const Instruction*> InstVector;
    typedef Map<const Instruction*, InstSet> CallToInstMap;
    typedef Set<CallSiteSet*> CtxSet;
    typedef ThreadForkEdge::ForkEdgeSet ForkEdgeSet;
    typedef Map<const CallBlockNode*, ForkEdgeSet> CallInstToForkEdgesMap;
    typedef ThreadJoinEdge::JoinEdgeSet JoinEdgeSet;
    typedef Map<const CallBlockNode*, JoinEdgeSet> CallInstToJoinEdgesMap;
    typedef HareParForEdge::ParForEdgeSet ParForEdgeSet;
    typedef Map<const CallBlockNode*, ParForEdgeSet> CallInstToParForEdgesMap;

    /// Constructor
    ThreadCallGraph();
    /// Destructor
    virtual ~ThreadCallGraph()
    {
    }

    /// ClassOf
    //@{
    static inline bool classof(const ThreadCallGraph *)
    {
        return true;
    }
    static inline bool classof(const PTACallGraph *g)
    {
        return g->getKind() == PTACallGraph::ThdCallGraph;
    }
    //@}

    /// Update call graph using pointer results
    void updateCallGraph(PointerAnalysis* pta);
    /// Update join edge using pointer analysis results
    void updateJoinEdge(PointerAnalysis* pta);


    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasThreadForkEdge(const CallBlockNode* cs) const
    {
        return callinstToThreadForkEdgesMap.find(cs) != callinstToThreadForkEdgesMap.end();
    }
    inline ForkEdgeSet::const_iterator getForkEdgeBegin(const CallBlockNode* cs) const
    {
        CallInstToForkEdgesMap::const_iterator it = callinstToThreadForkEdgesMap.find(cs);
        assert(it != callinstToThreadForkEdgesMap.end() && "call instruction not found");
        return it->second.begin();
    }
    inline ForkEdgeSet::const_iterator getForkEdgeEnd(const CallBlockNode* cs) const
    {
        CallInstToForkEdgesMap::const_iterator it = callinstToThreadForkEdgesMap.find(cs);
        assert(it != callinstToThreadForkEdgesMap.end() && "call instruction not found");
        return it->second.end();
    }

    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasThreadJoinEdge(const CallBlockNode* cs) const
    {
        return callinstToThreadJoinEdgesMap.find(cs) != callinstToThreadJoinEdgesMap.end();
    }
    inline JoinEdgeSet::const_iterator getJoinEdgeBegin(const CallBlockNode* cs) const
    {
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(cs);
        assert(it != callinstToThreadJoinEdgesMap.end() && "call instruction does not have a valid callee");
        return it->second.begin();
    }
    inline JoinEdgeSet::const_iterator getJoinEdgeEnd(const CallBlockNode* cs) const
    {
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(cs);
        assert(it != callinstToThreadJoinEdgesMap.end() && "call instruction does not have a valid callee");
        return it->second.end();
    }
    inline void getJoinSites(const PTACallGraphNode* routine, InstSet& csSet)
    {
        for(CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.begin(), eit = callinstToThreadJoinEdgesMap.end(); it!=eit; ++it)
        {
            for(JoinEdgeSet::const_iterator jit = it->second.begin(), ejit = it->second.end(); jit!=ejit; ++jit)
            {
                if((*jit)->getDstNode() == routine)
                {
                    csSet.insert(it->first);
                }
            }
        }
    }
    //@}

    /// Whether a callsite is a fork or join or hare_parallel_for
    ///@{
    inline bool isForksite(const CallBlockNode* csInst)
    {
        return forksites.find(csInst) != forksites.end();
    }
    inline bool isJoinsite(const CallBlockNode* csInst)
    {
        return joinsites.find(csInst) != joinsites.end();
    }
    inline bool isParForSite(const CallBlockNode* csInst)
    {
        return parForSites.find(csInst) != parForSites.end();
    }
    ///

    /// Fork sites iterators
    //@{
    inline CallSiteSet::const_iterator forksitesBegin() const
    {
        return forksites.begin();
    }
    inline CallSiteSet::const_iterator forksitesEnd() const
    {
        return forksites.end();
    }
    //@}

    /// Join sites iterators
    //@{
    inline CallSiteSet::const_iterator joinsitesBegin() const
    {
        return joinsites.begin();
    }
    inline CallSiteSet::const_iterator joinsitesEnd() const
    {
        return joinsites.end();
    }
    //@}

    /// hare_parallel_for sites iterators
    //@{
    inline CallSiteSet::const_iterator parForSitesBegin() const
    {
        return parForSites.begin();
    }
    inline CallSiteSet::const_iterator parForSitesEnd() const
    {
        return parForSites.end();
    }
    //@}

    /// Num of fork/join sites
    //@{
    inline u32_t getNumOfForksite() const
    {
        return forksites.size();
    }
    inline u32_t getNumOfJoinsite() const
    {
        return joinsites.size();
    }
    inline u32_t getNumOfParForSite() const
    {
        return parForSites.size();
    }
    //@}

    /// Thread API
    inline ThreadAPI* getThreadAPI() const
    {
        return tdAPI;
    }

    /// Add fork sites which directly or indirectly create a thread
    //@{
    inline bool addForksite(const CallBlockNode* cs)
    {
        callinstToThreadForkEdgesMap[cs];
        return forksites.insert(cs).second;
    }
    inline bool addJoinsite(const CallBlockNode* cs)
    {
        callinstToThreadJoinEdgesMap[cs];
        return joinsites.insert(cs).second;
    }
    inline bool addParForSite(const CallBlockNode* cs)
    {
        callinstToHareParForEdgesMap[cs];
        return parForSites.insert(cs).second;
    }
    //@}

    /// Add direct/indirect thread fork edges
    //@{
    void addDirectForkEdge(const CallBlockNode* cs);
    void addIndirectForkEdge(const CallBlockNode* cs, const SVFFunction* callee);
    //@}

    /// Add thread join edges
    //@{
    void addDirectJoinEdge(const CallBlockNode* cs,const CallSiteSet& forksite);
    //@}

    /// Add direct/indirect parallel for edges
    //@{
    void addDirectParForEdge(const CallBlockNode* cs);
    void addIndirectParForEdge(const CallBlockNode* cs, const SVFFunction* callee);
    //@}


    /// map call instruction to its CallGraphEdge map
    inline void addThreadForkEdgeSetMap(const CallBlockNode* cs, ThreadForkEdge* edge)
    {
        if(edge!=nullptr)
        {
            callinstToThreadForkEdgesMap[cs].insert(edge);
            callinstToCallGraphEdgesMap[cs].insert(edge);
        }
    }

    /// map call instruction to its CallGraphEdge map
    inline void addThreadJoinEdgeSetMap(const CallBlockNode* cs, ThreadJoinEdge* edge)
    {
        if(edge!=nullptr)
        {
            callinstToThreadJoinEdgesMap[cs].insert(edge);
            callinstToCallGraphEdgesMap[cs].insert(edge);
        }
    }

    /// map call instruction to its CallGraphEdge map
    inline void addHareParForEdgeSetMap(const CallBlockNode* cs, HareParForEdge* edge)
    {
        if(edge!=nullptr)
        {
            callinstToHareParForEdgesMap[cs].insert(edge);
            callinstToCallGraphEdgesMap[cs].insert(edge);
        }
    }

    /// has thread join edge
    inline ThreadJoinEdge* hasThreadJoinEdge(const CallBlockNode* call, PTACallGraphNode* joinFunNode, PTACallGraphNode* threadRoutineFunNode, CallSiteID csId) const
    {
        ThreadJoinEdge joinEdge(joinFunNode,threadRoutineFunNode, csId);
        CallInstToJoinEdgesMap::const_iterator it = callinstToThreadJoinEdgesMap.find(call);
        if(it != callinstToThreadJoinEdgesMap.end())
        {
            JoinEdgeSet::const_iterator jit = it->second.find(&joinEdge);
            if(jit!=it->second.end())
                return *jit;
        }
        return nullptr;
    }

private:
    ThreadAPI* tdAPI;		///< Thread API
    CallSiteSet forksites; ///< all thread fork sites
    CallSiteSet joinsites; ///< all thread fork sites
    CallSiteSet parForSites; ///< all parallel for sites
    CallInstToForkEdgesMap callinstToThreadForkEdgesMap; ///< Map a call instruction to its corresponding fork edges
    CallInstToJoinEdgesMap callinstToThreadJoinEdgesMap; ///< Map a call instruction to its corresponding join edges
    CallInstToParForEdgesMap callinstToHareParForEdgesMap; ///< Map a call instruction to its corresponding hare_parallel_for edges
};

} // End namespace SVF

#endif /* RCG_H_ */
