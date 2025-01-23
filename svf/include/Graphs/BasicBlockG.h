//===- BasicBlockG.h -- BasicBlock node------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2025>  <Yulei Sui>
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
 * ICFGNode.h
 *
 *  Created on: 23 Jan, 2025
 *      Author: Jiawei, Xiao
 */

#ifndef BASICBLOCKGRAPH_H_
#define BASICBLOCKGRAPH_H_
#include "GenericGraph.h"
#include <sstream>
#include <algorithm>


namespace SVF
{
class BasicBlockNode;
class BasicBlockEdge;
class ICFGNode;
class SVFFunction;
typedef GenericEdge<BasicBlockNode> GenericBasicBlockEdgeTy;
class BasicBlockEdge: public GenericBasicBlockEdgeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
public:
    /// Constructor
    BasicBlockEdge(BasicBlockNode* s, BasicBlockNode* d) : GenericBasicBlockEdgeTy(s, d, 0)
    {
    }
    /// Destructor
    ~BasicBlockEdge() {}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<<(OutStream& o, const BasicBlockEdge& edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const
    {
        //TODO: BBG
        std::string str;
        std::stringstream rawstr(str);
        return rawstr.str();
    }
};


typedef GenericNode<BasicBlockNode, BasicBlockEdge> GenericBasicBlockNodeTy;
class BasicBlockNode : public GenericBasicBlockNodeTy
{
    friend class LLVMModuleSet;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
    friend class SVFFunction;
    friend class ICFGBuilder;
    friend class ICFG;

public:
    typedef std::vector<const ICFGNode*>::const_iterator const_iterator;
    std::vector<const BasicBlockNode*> succBBs;
    std::vector<const BasicBlockNode*> predBBs;

private:
    std::vector<const ICFGNode*> allICFGNodes;    ///< all ICFGNodes in this BasicBlock
    const SVFFunction* fun;                 /// Function where this BasicBlock is



protected:
    ///@{ attributes to be set only through Module builders e.g., LLVMModule

    inline void addICFGNode(const ICFGNode* icfgNode)
    {
        assert(std::find(getICFGNodeList().begin(), getICFGNodeList().end(),
                         icfgNode) == getICFGNodeList().end() && "duplicated icfgnode");
        allICFGNodes.push_back(icfgNode);
    }

    /// @}

public:
    /// Constructor without name
    ///TODO: rewrite ID and GNodeK

    BasicBlockNode(NodeID id, const SVFFunction* f): GenericBasicBlockNodeTy(id, OtherKd), fun(f){

    }
    BasicBlockNode() = delete;
    ~BasicBlockNode() {

    }

    //@{
    friend OutStream &operator<<(OutStream &o, const BasicBlockNode&node)
    {
        o << node.toString();
        return o;
    }
    //@}


    inline const std::string toString() const {
        return "";
    }

    inline const std::vector<const ICFGNode*>& getICFGNodeList() const
    {
        return allICFGNodes;
    }

    inline const_iterator begin() const
    {
        return allICFGNodes.begin();
    }

    inline const_iterator end() const
    {
        return allICFGNodes.end();
    }

    inline void addSuccBasicBlock(const BasicBlockNode* succ2)
    {
        // TODO: discuss shall we check duplicated edges
        BasicBlockNode* succ = const_cast<BasicBlockNode*>(succ2);
        BasicBlockEdge* edge = new BasicBlockEdge(this, succ);
        this->addOutgoingEdge(edge);
        succ->addIncomingEdge(edge);
        this->succBBs.push_back(succ);
        succ->predBBs.push_back(this);

    }

    inline void addPredBasicBlock(const BasicBlockNode* pred2)
    {
        BasicBlockNode* pred = const_cast<BasicBlockNode*>(pred2);
        BasicBlockEdge* edge = new BasicBlockEdge(pred, this);
        this->addIncomingEdge(edge);
        pred->addOutgoingEdge(edge);
        this->predBBs.push_back(pred);
        pred->succBBs.push_back(this);
    }

    inline const SVFFunction* getParent() const
    {
        return fun;
    }

    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    inline const ICFGNode* front() const
    {
        assert(!allICFGNodes.empty() && "bb empty?");
        return allICFGNodes.front();
    }

    inline const ICFGNode* back() const
    {
        assert(!allICFGNodes.empty() && "bb empty?");
        return allICFGNodes.back();
    }

    inline std::vector<const BasicBlockNode*> getSuccessors() const
    {
        std::vector<const BasicBlockNode*> res;
        for (auto edge : this->getOutEdges())
        {
            res.push_back(edge->getDstNode());
        }
        return res;
    }

    inline std::vector<const BasicBlockNode*> getPredecessors() const
    {
        std::vector<const BasicBlockNode*> res;
        for (auto edge : this->getInEdges())
        {
            res.push_back(edge->getSrcNode());
        }
        return res;
    }
    u32_t getNumSuccessors() const
    {
        return this->getOutEdges().size();
    }
    u32_t getBBSuccessorPos(const BasicBlockNode* Succ) {
        u32_t i = 0;
        for (const BasicBlockNode* SuccBB: succBBs)
        {
            if (SuccBB == Succ)
                return i;
            i++;
        }
        assert(false && "Didn't find successor edge?");
        return 0;
    }
    u32_t getBBSuccessorPos(const BasicBlockNode* Succ) const {
        u32_t i = 0;
        for (const BasicBlockNode* SuccBB: succBBs)
        {
            if (SuccBB == Succ)
                return i;
            i++;
        }
        assert(false && "Didn't find successor edge?");
        return 0;

    }
    u32_t getBBPredecessorPos(const BasicBlockNode* succbb) {
        u32_t pos = 0;
        for (const BasicBlockNode* PredBB : succbb->getPredecessors())
        {
            if(PredBB == this)
                return pos;
            ++pos;
        }
        assert(false && "Didn't find predecessor edge?");
        return pos;
    }
    u32_t getBBPredecessorPos(const BasicBlockNode* succbb) const {
        u32_t pos = 0;
        for (const BasicBlockNode* PredBB : succbb->getPredecessors())
        {
            if(PredBB == this)
                return pos;
            ++pos;
        }
        assert(false && "Didn't find predecessor edge?");
        return pos;
    }
};



typedef GenericGraph<BasicBlockNode, BasicBlockEdge> GenericBasicBlockGraphTy;
class BasicBlockGraph: public GenericBasicBlockGraphTy
{
private:
    NodeID id{0};
public:
    /// Constructor
    BasicBlockGraph() {

    }

    BasicBlockNode* addBasicBlock(const SVFFunction* f)
    {
        id++;
        BasicBlockNode* bb = new BasicBlockNode(id, f);
        addGNode(id, bb);
        return bb;
    }


};
}

#endif