//===----- CFLGraph.h -- Graph for context-free language reachability analysis --//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFLGraph.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#ifndef CFLG_H_
#define CFLG_H_

#include <fstream>
#include <iostream>
#include <string>
#include <regex>
#include "CFL/CFGrammar.h"
#include "Graphs/GenericGraph.h"
#include "Graphs/ConsG.h"

namespace SVF
{
class CFLNode;

typedef GenericEdge<CFLNode> GenericCFLEdgeTy;

class CFLEdge: public GenericCFLEdgeTy
{
public:
    typedef GenericNode<CFLNode, CFLEdge>::GEdgeSetTy CFLEdgeSetTy;

    CFLEdge(CFLNode *s, CFLNode *d, GEdgeFlag k = 0):
        GenericCFLEdgeTy(s,d,k)
    {
    }
    ~CFLEdge() override = default;

    inline GEdgeKind getEdgeKind() const
    {
        return this->getEdgeKindWithoutMask();
    }

    inline GEdgeKind getEdgeKindWithMask() const
    {
        return (EdgeKindMask & this->getEdgeKindWithoutMask());
    }

    inline GEdgeKind getEdgeAttri() const
    {
        return (getEdgeKind() >> this->EdgeKindMaskBits);
    }
};


typedef GenericNode<CFLNode,CFLEdge> GenericCFLNodeTy;
class CFLNode: public GenericCFLNodeTy
{
public:
    CFLNode (NodeID i = 0, GNodeK k = 0):
        GenericCFLNodeTy(i, k)
    {
    }

    ~CFLNode() override = default;

    /// Different Kind(label) associated edges set
    typedef std::map <GrammarBase::Symbol, CFLEdge::CFLEdgeSetTy> CFLEdgeDataTy;

private:
    CFLEdgeDataTy inCFLEdges;
    CFLEdgeDataTy outCFLEdges;

public:
    inline const CFLEdge::CFLEdgeSetTy& getInEdgeWithTy(GrammarBase::Symbol s)
    {
        return inCFLEdges[s];
    }

    inline const CFLEdge::CFLEdgeSetTy& getOutEdgeWithTy(GrammarBase::Symbol s)
    {
        return outCFLEdges[s];
    }

    inline bool addInEdgeWithKind(CFLEdge* inEdge, GrammarBase::Symbol s)
    {
        assert(inEdge->getDstID() == this->getId());
        bool added1 = GenericNode::addIncomingEdge(inEdge);
        bool added2 = inCFLEdges[s].insert(inEdge).second;

        return added1 && added2;
    }

    inline bool addIngoingEdge(CFLEdge* inEdge)
    {
        return addInEdgeWithKind(inEdge, inEdge->getEdgeKind());
    }

    inline bool addOutEdgeWithKind(CFLEdge* outEdge, GrammarBase::Symbol s)
    {
        assert(outEdge->getSrcID() == this->getId());
        bool added1 = GenericNode::addOutgoingEdge(outEdge);
        bool added2 = outCFLEdges[s].insert(outEdge).second;

        return added1 && added2;
    }

    inline bool addOutgoingEdge(CFLEdge* OutEdge)
    {
        return addOutEdgeWithKind(OutEdge, OutEdge->getEdgeKind());
    }

    inline bool removeCFLInEdge(CFLEdge* inEdge)
    {
        u32_t num1 = removeIncomingEdge(inEdge);

        GrammarBase::Symbol s = inEdge->getEdgeKind();
        u32_t num2 = inCFLEdges[s].erase(inEdge);

        return num1 && num2;
    }

    inline bool removeCFLOutEdge(CFLEdge* outEdge)
    {
        u32_t num1 = removeOutgoingEdge(outEdge);

        GrammarBase::Symbol s = outEdge->getEdgeKind();
        u32_t num2 = outCFLEdges[s].erase(outEdge);

        return num1 && num2;
    }
};

/// Edge-labeled graph for CFL Reachability analysis
typedef GenericGraph<CFLNode,CFLEdge> GenericCFLGraphTy;
class CFLGraph: public GenericCFLGraphTy
{
public:
    typedef CFGrammar::Symbol Symbol;
    typedef CFGrammar::Kind Kind;
    typedef GenericNode<CFLNode,CFLEdge>::GEdgeSetTy CFLEdgeSet;
    Kind startKind;

    CFLGraph(Kind kind)
    {
        startKind = kind;
    }
    ~CFLGraph() override = default;

    Kind getStartKind() const;

    virtual void addCFLNode(NodeID id, CFLNode* node);

    virtual const CFLEdge* addCFLEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label);

    virtual const CFLEdge* hasEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label);

    void dump(const std::string& filename);

    void view();

    inline const CFLEdgeSet& getCFLEdges() const
    {
        return cflEdgeSet;
    }

private:
    CFLEdgeSet cflEdgeSet;
};

}

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::CFLNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::CFLNode,SVF::CFLEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::CFLNode *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::CFLNode,SVF::CFLEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::CFLGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::CFLNode,SVF::CFLEdge>* >
{
    typedef SVF::CFLNode *NodeRef;
};

} // End namespace llvm

#endif /* CFLG_H_ */