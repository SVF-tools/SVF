//===- CFBasicBlockG.h ----------------------------------------------------------------//
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
 * CFBasicBlockG.h
 *
 *  Created on: 24 Dec. 2022
 *      Author: Xiao, Jiawei
 */

#ifndef SVF_CFBASICBLOCKG_H
#define SVF_CFBASICBLOCKG_H
#include "Util/SVFUtil.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/GenericGraph.h"

namespace SVF
{
class CFBasicBlockNode;
class SVFIR;

typedef GenericEdge<CFBasicBlockNode> GenericCFBasicBlockEdgeTy;

class CFBasicBlockEdge : public GenericCFBasicBlockEdgeTy
{
public:
    CFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst) : GenericCFBasicBlockEdgeTy(src, dst, 0) {}


    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockEdge &edge)
    {
        o << edge.toString();
        return o;
    }

    virtual const std::string toString() const
    {
        return std::to_string(getSrcID()) + " --> " + std::to_string(getDstID());
    }
};

typedef GenericNode<CFBasicBlockNode, CFBasicBlockEdge> GenericCFBasicBlockNodeTy;

class CFBasicBlockNode : public GenericCFBasicBlockNodeTy
{
private:
    const SVFBasicBlock *_svfBasicBlock; /// Every CFBasicBlockNode holds a SVFBasicBlock
    std::vector<const ICFGNode *> _icfgNodes; /// Every CBFGNode holds a vector of ICFGNodes

public:
    CFBasicBlockNode(u32_t id, const SVFBasicBlock *svfBasicBlock);

    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockNode &node)
    {
        o << node.toString();
        return o;
    }

    virtual const std::string toString() const;

    inline std::string getName() const
    {
        return _svfBasicBlock->getName();
    }

    inline const SVFBasicBlock *getSVFBasicBlock() const
    {
        return _svfBasicBlock;
    }

    inline const SVFFunction *getFunction() const
    {
        return _svfBasicBlock->getFunction();
    }

    inline std::vector<const ICFGNode *>::const_iterator begin() const
    {
        return _icfgNodes.cbegin();
    }

    inline std::vector<const ICFGNode *>::const_iterator end() const
    {
        return _icfgNodes.cend();
    }
};

typedef GenericGraph<CFBasicBlockNode, CFBasicBlockEdge> GenericCFBasicBlockGTy;

class CFBasicBlockGraph : public GenericCFBasicBlockGTy
{
    friend class CFBasicBlockGBuilder;

public:
    typedef Map<const SVFBasicBlock *, CFBasicBlockNode *> SVFBasicBlockToCFBasicBlockNodeMap;

private:
    SVFBasicBlockToCFBasicBlockNodeMap _bbToNode;
    const SVFFunction *_svfFunction;
    u32_t _totalCFBasicBlockNode{0};
    u32_t _totalCFBasicBlockEdge{0};

public:

    CFBasicBlockGraph(const SVFFunction *svfFunction) : _svfFunction(svfFunction)
    {

    }

    ~CFBasicBlockGraph() override = default;

    /// Dump graph into dot file
    void dump(const std::string &filename)
    {
        GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
    }

    inline CFBasicBlockNode *getCFBasicBlockNode(u32_t id) const
    {
        if (!hasGNode(id)) return nullptr;
        return getGNode(id);
    }

    inline CFBasicBlockNode *getCFBasicBlockNode(const SVFBasicBlock *bb) const
    {
        auto it = _bbToNode.find(bb);
        if (it == _bbToNode.end()) return nullptr;
        return it->second;
    }

    inline bool hasCFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst) const
    {
        CFBasicBlockEdge edge(src, dst);
        CFBasicBlockEdge *outEdge = src->hasOutgoingEdge(&edge);
        CFBasicBlockEdge *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge)
        {
            assert(outEdge == inEdge && "edges not match");
            return true;
        }
        else
            return false;
    }

    CFBasicBlockEdge* getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst);

    CFBasicBlockEdge* getCFBasicBlockEdge(const SVFBasicBlock *src, const SVFBasicBlock *dst);

private:

    /// Add a CFBasicBlockNode
    inline const CFBasicBlockNode* getOrAddCFBasicBlockNode(const SVFBasicBlock *bb);

    inline const CFBasicBlockEdge *getOrAddCFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst);

};

class CFBasicBlockGBuilder
{

private:
    CFBasicBlockGraph* _CFBasicBlockG;

public:
    CFBasicBlockGBuilder(const SVFFunction *func) : _CFBasicBlockG(new CFBasicBlockGraph(func)) {}

    void build();

    inline CFBasicBlockGraph* getCFBasicBlockGraph()
    {
        return _CFBasicBlockG;
    }
};
}


namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::CFBasicBlockNode *>
    : public GenericGraphTraits<SVF::GenericNode<SVF::CFBasicBlockNode, SVF::CFBasicBlockEdge> *>
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse< SVF::CFBasicBlockNode *> > : public GenericGraphTraits<
    Inverse<SVF::GenericNode<SVF::CFBasicBlockNode, SVF::CFBasicBlockEdge> *> >
{
};

template<>
struct GenericGraphTraits<SVF::CFBasicBlockGraph *>
    : public GenericGraphTraits<SVF::GenericGraph<SVF::CFBasicBlockNode, SVF::CFBasicBlockEdge> *>
{
    typedef SVF::CFBasicBlockNode *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::CFBasicBlockGraph *> : public DOTGraphTraits<SVF::SVFIR *>
{

    typedef SVF::CFBasicBlockNode NodeType;

    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVF::SVFIR *>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::CFBasicBlockGraph *)
    {
        return "CFBasicBlockGraph";
    }

    std::string getNodeLabel(NodeType *node, SVF::CFBasicBlockGraph *graph)
    {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::CFBasicBlockGraph *)
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        rawstr << node->toString();

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::CFBasicBlockGraph *)
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "color=black";
        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::CFBasicBlockGraph *)
    {
        return "style=solid";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI)
    {
        return "";
    }
};

} // End namespace SVF
#endif //SVF_CFBASICBLOCKG_H