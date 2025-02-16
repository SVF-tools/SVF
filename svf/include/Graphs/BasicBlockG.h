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
class SVFBasicBlock;
class BasicBlockEdge;
class ICFGNode;
class FunObjVar;

typedef GenericEdge<SVFBasicBlock> GenericBasicBlockEdgeTy;
class BasicBlockEdge: public GenericBasicBlockEdgeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
public:
    /// Constructor
    BasicBlockEdge(SVFBasicBlock* s, SVFBasicBlock* d) : GenericBasicBlockEdgeTy(s, d, 0)
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

    virtual const std::string toString() const;
};


typedef GenericNode<SVFBasicBlock, BasicBlockEdge> GenericBasicBlockNodeTy;
class SVFBasicBlock : public GenericBasicBlockNodeTy
{
    friend class LLVMModuleSet;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
    friend class FunObjVar;
    friend class ICFGBuilder;
    friend class ICFG;

public:
    typedef std::vector<const ICFGNode*>::const_iterator const_iterator;
    std::vector<const SVFBasicBlock*> succBBs;
    std::vector<const SVFBasicBlock*> predBBs;

private:
    std::vector<const ICFGNode*> allICFGNodes;    ///< all ICFGNodes in this BasicBlock
    const FunObjVar* fun;                 /// Function where this BasicBlock is



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
    SVFBasicBlock(NodeID id, const FunObjVar* f): GenericBasicBlockNodeTy(id, BasicBlockKd), fun(f)
    {

    }
    SVFBasicBlock() = delete;
    ~SVFBasicBlock()
    {

    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFValue::BasicBlockKd;
    }

    static inline bool classof(const SVFBasicBlock* node)
    {
        return true;
    }

    //@{
    friend OutStream &operator<<(OutStream &o, const SVFBasicBlock&node)
    {
        o << node.toString();
        return o;
    }
    //@}


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


    inline void setFun(const FunObjVar* f)
    {
        fun = f;
    }

    inline void addSuccBasicBlock(const SVFBasicBlock* succ2)
    {
        // check if the edge already exists
        for (auto edge: this->getOutEdges())
        {
            if (edge->getDstNode() == succ2)
                return;
        }

        SVFBasicBlock* succ = const_cast<SVFBasicBlock*>(succ2);
        BasicBlockEdge* edge = new BasicBlockEdge(this, succ);
        this->addOutgoingEdge(edge);
        succ->addIncomingEdge(edge);
        this->succBBs.push_back(succ);
        succ->predBBs.push_back(this);
    }

    inline void addPredBasicBlock(const SVFBasicBlock* pred2)
    {
        // check if the edge already exists
        for (auto edge: this->getInEdges())
        {
            if (edge->getSrcNode() == pred2)
                return;
        }
        SVFBasicBlock* pred = const_cast<SVFBasicBlock*>(pred2);
        BasicBlockEdge* edge = new BasicBlockEdge(pred, this);
        this->addIncomingEdge(edge);
        pred->addOutgoingEdge(edge);
        this->predBBs.push_back(pred);
        pred->succBBs.push_back(this);
    }

    inline const FunObjVar* getParent() const
    {
        assert(fun && "Function is null?");
        return fun;
    }

    inline const FunObjVar* getFunction() const
    {
        assert(fun && "Function is null?");
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

    inline std::vector<const SVFBasicBlock*> getSuccessors() const
    {
        std::vector<const SVFBasicBlock*> res;
        for (auto edge : this->getOutEdges())
        {
            res.push_back(edge->getDstNode());
        }
        return res;
    }

    inline std::vector<const SVFBasicBlock*> getPredecessors() const
    {
        std::vector<const SVFBasicBlock*> res;
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
    u32_t getBBSuccessorPos(const SVFBasicBlock* Succ)
    {
        u32_t i = 0;
        for (const SVFBasicBlock* SuccBB: succBBs)
        {
            if (SuccBB == Succ)
                return i;
            i++;
        }
        assert(false && "Didn't find successor edge?");
        return 0;
    }
    u32_t getBBSuccessorPos(const SVFBasicBlock* Succ) const
    {
        u32_t i = 0;
        for (const SVFBasicBlock* SuccBB: succBBs)
        {
            if (SuccBB == Succ)
                return i;
            i++;
        }
        assert(false && "Didn't find successor edge?");
        return 0;

    }
    u32_t getBBPredecessorPos(const SVFBasicBlock* succbb)
    {
        u32_t pos = 0;
        for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
        {
            if(PredBB == this)
                return pos;
            ++pos;
        }
        assert(false && "Didn't find predecessor edge?");
        return pos;
    }
    u32_t getBBPredecessorPos(const SVFBasicBlock* succbb) const
    {
        u32_t pos = 0;
        for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
        {
            if(PredBB == this)
                return pos;
            ++pos;
        }
        assert(false && "Didn't find predecessor edge?");
        return pos;
    }

    const std::string toString() const;

    const std::vector<const SVFBasicBlock*> getSuccBBs() const
    {
        return succBBs;
    }

    const std::vector<const SVFBasicBlock*> getPredBBs() const
    {
        return predBBs;
    }

};



typedef GenericGraph<SVFBasicBlock, BasicBlockEdge> GenericBasicBlockGraphTy;
class BasicBlockGraph: public GenericBasicBlockGraphTy
{
private:
    NodeID id{0};
public:
    /// Constructor
    BasicBlockGraph(): GenericBasicBlockGraphTy()
    {

    }


    SVFBasicBlock* addBasicBlock(const std::string& bbname)
    {
        id++;
        SVFBasicBlock* bb = new SVFBasicBlock(id, nullptr);
        addGNode(id, bb);
        bb->setName(bbname);
        return bb;
    }

};
}

#endif