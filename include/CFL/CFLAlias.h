//===----- CFLAlias.h -- CFL Alias Analysis Client--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFLAlias.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */


#include "CFL/CFLSolver.h"
#include "CFL/CFGNormalizer.h"
#include "CFL/GrammarBuilder.h"
#include "CFL/CFLGraphBuilder.h"
#include "CFL/CFLGramGraphChecker.h"
#include "MemoryModel/PointerAnalysis.h"
#include "Graphs/ConsG.h"
#include "Util/Options.h"

namespace SVF
{

class CFLAlias : public PointerAnalysis
{

public:
    typedef OrderedMap<CallSite, NodeID> CallSite2DummyValPN;

    CFLAlias(SVFIR* ir) : PointerAnalysis(ir, PointerAnalysis::CFLFICI_WPA), svfir(ir), graph(nullptr), grammar(nullptr), solver(nullptr)
    {
    }

    /// Destructor
    virtual ~CFLAlias()
    {
        delete solver;
    }

    /// Start Analysis here (main part of pointer analysis).
    virtual void analyze();

    /// Interface exposed to users of our pointer analysis, given Value infos
    virtual AliasResult alias(const Value* v1, const Value* v2)
    {
        NodeID n1 = svfir->getValueNode(v1);
        NodeID n2 = svfir->getValueNode(v2);
        return alias(n1,n2);
    }

    /// Interface exposed to users of our pointer analysis, given PAGNodeID
    virtual AliasResult alias(NodeID node1, NodeID node2)
    {
        if(graph->hasEdge(graph->getGNode(node1), graph->getGNode(node2), graph->startKind))
            return AliasResult::MayAlias;
        else
            return AliasResult::NoAlias;
    }

    /// Get points-to targets of a pointer.  V In this context
    virtual const PointsTo& getPts(NodeID ptr)
    {
        /// Check V Dst of ptr.
        PointsTo *ps = new PointsTo();
        CFLNode *funNode = graph->getGNode(ptr);
        for(auto outedge = funNode->getOutEdges().begin(); outedge!=funNode->getOutEdges().end(); outedge++)
        {
            if((*outedge)->getEdgeKind() == graph->getStartKind())
            {
                // Need to Find dst addr src
                CFLNode *vNode = graph->getGNode((*outedge)->getDstID());

                for(auto inEdge = vNode->getInEdges().begin(); inEdge!=vNode->getInEdges().end(); inEdge++)
                {
                    if((*inEdge)->getEdgeKind() == 0)
                    {
                        ps->set((*inEdge)->getSrcID());
                    }
                }
            }
        }
        return *ps;
    }

    /// Add copy edge on constraint graph
    virtual inline bool addCopyEdge(NodeID src, NodeID dst)
    {
        const CFLEdge *edge = graph->hasEdge(graph->getGNode(src),graph->getGNode(dst), 1);
        if (edge != nullptr )
        {
            return false;

        }
        CFLGrammar::Kind copyKind = grammar->str2Kind("Copy");
        CFLGrammar::Kind copybarKind = grammar->str2Kind("Copybar");
        solver->pushIntoWorklist(graph->addCFLEdge(graph->getGNode(src),graph->getGNode(dst), copyKind));
        solver->pushIntoWorklist(graph->addCFLEdge(graph->getGNode(dst),graph->getGNode(src), copybarKind));
        return true;
    }

    /// Given an object, get all the nodes having whose pointsto contains the object
    virtual const NodeSet& getRevPts(NodeID nodeId)
    {
        /// Check Outgoing flowtobar edge dst of ptr
        abort(); // to be implemented
    }

    /// Update call graph for the input indirect callsites
    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);

    /// On the fly call graph construction
    virtual void onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges);

    /// Connect formal and actual parameters for indirect callsites
    void connectCaller2CalleeParams(CallSite cs, const SVFFunction* F);

    void heapAllocatorViaIndCall(CallSite cs);
private:
    CallSite2DummyValPN callsite2DummyValPN;        ///< Map an instruction to a dummy obj which created at an indirect callsite, which invokes a heap allocator
    SVFIR* svfir;
    CFLGraph* graph;
    CFLGrammar* grammar;
    CFLSolver *solver;
};

}
