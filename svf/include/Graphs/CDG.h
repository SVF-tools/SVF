//===- CDG.h -- Control Dependence Graph --------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * CDG.h
 *
 *  Created on: Sep 27, 2023
 *      Author: Xiao Cheng
 */

#ifndef SVF_CONTROLDG_H
#define SVF_CONTROLDG_H

#include "SVFIR/SVFIR.h"

namespace SVF
{

class CDGNode;

typedef GenericEdge<CDGNode> GenericCDGEdgeTy;

class CDGEdge : public GenericCDGEdgeTy
{
public:
    typedef std::pair<const SVFVar *, s32_t> BranchCondition;

    /// Constructor
    CDGEdge(CDGNode *s, CDGNode *d) : GenericCDGEdgeTy(s, d, 0)
    {
    }

    /// Destructor
    ~CDGEdge()
    {
    }

    typedef GenericNode<CDGNode, CDGEdge>::GEdgeSetTy CDGEdgeSetTy;
    typedef CDGEdgeSetTy SVFGEdgeSetTy;

    virtual const std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "CDGEdge " << " [";
        rawstr << getDstID() << "<--" << getSrcID() << "\t";
        return rawstr.str();
    }

    /// get/set branch condition
    //{@
    const Set<BranchCondition> &getBranchConditions() const
    {
        return brConditions;
    }

    void insertBranchCondition(const SVFVar *pNode, s32_t branchID)
    {
        brConditions.insert(std::make_pair(pNode, branchID));
    }
    //@}


private:
    Set<BranchCondition> brConditions;
};

typedef GenericNode<CDGNode, CDGEdge> GenericCDGNodeTy;

class CDGNode : public GenericCDGNodeTy
{

public:

    typedef CDGEdge::CDGEdgeSetTy::iterator iterator;
    typedef CDGEdge::CDGEdgeSetTy::const_iterator const_iterator;

public:
    /// Constructor
    CDGNode(const ICFGNode *icfgNode) : GenericCDGNodeTy(icfgNode->getId(), CDNodeKd), _icfgNode(icfgNode)
    {

    }

    virtual const std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << getId();
        return rawstr.str();
    }

    const ICFGNode *getICFGNode() const
    {
        return _icfgNode;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CDGNode *)
    {
        return true;
    }

    static inline bool classof(const GenericICFGNodeTy* node)
    {
        return node->getNodeKind() == CDNodeKd;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == CDNodeKd;
    }
    //@}

private:
    const ICFGNode *_icfgNode;
};

typedef std::vector<std::pair<NodeID, NodeID>> NodePairVector;
typedef GenericGraph<CDGNode, CDGEdge> GenericCDGTy;

class CDG : public GenericCDGTy
{

public:

    typedef Map<NodeID, CDGNode *> CDGNodeIDToNodeMapTy;
    typedef CDGEdge::CDGEdgeSetTy CDGEdgeSetTy;
    typedef CDGNodeIDToNodeMapTy::iterator iterator;
    typedef CDGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef std::vector<const ICFGNode *> ICFGNodeVector;
    typedef std::vector<std::pair<const ICFGNode *, const ICFGNode *>> ICFGNodePairVector;

private:
    static CDG *controlDg; ///< Singleton pattern here
    /// Constructor
    CDG()
    {

    }


public:
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline CDG * getCDG()
    {
        if (controlDg == nullptr)
        {
            controlDg = new CDG();
        }
        return controlDg;
    }

    static void releaseCDG()
    {
        if (controlDg)
            delete controlDg;
        controlDg = nullptr;
    }
    //@}

    /// Destructor
    virtual ~CDG() {}

    /// Get a CDG node
    inline CDGNode *getCDGNode(NodeID id) const
    {
        if (!hasCDGNode(id))
            return nullptr;
        return getGNode(id);
    }

    /// Whether has the CDGNode
    inline bool hasCDGNode(NodeID id) const
    {
        return hasGNode(id);
    }

    /// Whether we has a CDG edge
    bool hasCDGEdge(CDGNode *src, CDGNode *dst)
    {
        CDGEdge edge(src, dst);
        CDGEdge *outEdge = src->hasOutgoingEdge(&edge);
        CDGEdge *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge)
        {
            assert(outEdge == inEdge && "edges not match");
            return true;
        }
        else
            return false;
    }

    /// Get a control dependence edge according to src and dst
    CDGEdge *getCDGEdge(const CDGNode *src, const CDGNode *dst)
    {
        CDGEdge *edge = nullptr;
        size_t counter = 0;
        for (CDGEdge::CDGEdgeSetTy::iterator iter = src->OutEdgeBegin();
                iter != src->OutEdgeEnd(); ++iter)
        {
            if ((*iter)->getDstID() == dst->getId())
            {
                counter++;
                edge = (*iter);
            }
        }
        assert(counter <= 1 && "there's more than one edge between two CDG nodes");
        return edge;
    }

    /// View graph from the debugger
    void view()
    {
        SVF::ViewGraph(this, "Control Dependence Graph");
    }

    /// Dump graph into dot file
    void dump(const std::string &filename)
    {
        GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
    }

public:
    /// Remove a control dependence edge
    inline void removeCDGEdge(CDGEdge *edge)
    {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }

    /// Remove a CDGNode
    inline void removeCDGNode(CDGNode *node)
    {
        std::set<CDGEdge *> temp;
        for (CDGEdge *e: node->getInEdges())
            temp.insert(e);
        for (CDGEdge *e: node->getOutEdges())
            temp.insert(e);
        for (CDGEdge *e: temp)
        {
            removeCDGEdge(e);
        }
        removeGNode(node);
    }

    /// Remove node from nodeID
    inline bool removeCDGNode(NodeID id)
    {
        if (hasCDGNode(id))
        {
            removeCDGNode(getCDGNode(id));
            return true;
        }
        return false;
    }

    /// Add CDG edge
    inline bool addCDGEdge(CDGEdge *edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return added1 && added2;
    }

    /// Add a CDG node
    virtual inline void addCDGNode(CDGNode *node)
    {
        addGNode(node->getId(), node);
    }

    /// Add CDG nodes from nodeid vector
    inline void addCDGNodesFromVector(ICFGNodeVector nodes)
    {
        for (const ICFGNode *icfgNode: nodes)
        {
            if (!IDToNodeMap.count(icfgNode->getId()))
            {
                addGNode(icfgNode->getId(), new CDGNode(icfgNode));
            }
        }
    }

    /// Add CDG edges from nodeid pair
    void addCDGEdgeFromSrcDst(const ICFGNode *src, const ICFGNode *dst, const SVFVar *pNode, s32_t branchID);

};
} // end namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::CDGNode *>
    : public GenericGraphTraits<SVF::GenericNode<SVF::CDGNode, SVF::CDGEdge> *>
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse<SVF::CDGNode *> > : public GenericGraphTraits<
    Inverse<SVF::GenericNode<SVF::CDGNode, SVF::CDGEdge> *> >
{
};

template<>
struct GenericGraphTraits<SVF::CDG *>
    : public GenericGraphTraits<SVF::GenericGraph<SVF::CDGNode, SVF::CDGEdge> *>
{
    typedef SVF::CDGNode *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::CDG *> : public DOTGraphTraits<SVF::PAG *>
{

    typedef SVF::CDGNode NodeType;

    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVF::PAG *>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::CDG *)
    {
        return "Control Dependence Graph";
    }

    std::string getNodeLabel(NodeType *node, SVF::CDG *graph)
    {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::CDG *)
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        const SVF::ICFGNode *icfgNode = node->getICFGNode();
        if (const SVF::IntraICFGNode *bNode = SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(icfgNode))
        {
            rawstr << "IntraBlockNode ID: " << bNode->getId() << " \t";
            SVF::PAG::SVFStmtList &edges = SVF::PAG::getPAG()->getPTASVFStmtList(bNode);
            if (edges.empty())
            {
                rawstr << (bNode)->toString() << " \t";
            }
            else
            {
                for (SVF::PAG::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
                {
                    const SVF::PAGEdge *edge = *it;
                    rawstr << edge->toString();
                }
            }
            rawstr << " {fun: " << bNode->getFun()->getName() << "}";
        }
        else if (const SVF::FunEntryICFGNode *entry = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(icfgNode))
        {
            rawstr << entry->toString();
        }
        else if (const SVF::FunExitICFGNode *exit = SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(icfgNode))
        {
            rawstr << exit->toString();
        }
        else if (const SVF::CallICFGNode *call = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(icfgNode))
        {
            rawstr << call->toString();
        }
        else if (const SVF::RetICFGNode *ret = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(icfgNode))
        {
            rawstr << ret->toString();
        }
        else if (const SVF::GlobalICFGNode *glob = SVF::SVFUtil::dyn_cast<SVF::GlobalICFGNode>(icfgNode))
        {
            SVF::PAG::SVFStmtList &edges = SVF::PAG::getPAG()->getPTASVFStmtList(glob);
            for (SVF::PAG::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
            {
                const SVF::PAGEdge *edge = *it;
                rawstr << edge->toString();
            }
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::CDG *)
    {
        std::string str;
        std::stringstream rawstr(str);
        const SVF::ICFGNode *icfgNode = node->getICFGNode();

        if (SVF::SVFUtil::isa<SVF::IntraICFGNode>(icfgNode))
        {
            rawstr << "color=black";
        }
        else if (SVF::SVFUtil::isa<SVF::FunEntryICFGNode>(icfgNode))
        {
            rawstr << "color=yellow";
        }
        else if (SVF::SVFUtil::isa<SVF::FunExitICFGNode>(icfgNode))
        {
            rawstr << "color=green";
        }
        else if (SVF::SVFUtil::isa<SVF::CallICFGNode>(icfgNode))
        {
            rawstr << "color=red";
        }
        else if (SVF::SVFUtil::isa<SVF::RetICFGNode>(icfgNode))
        {
            rawstr << "color=blue";
        }
        else if (SVF::SVFUtil::isa<SVF::GlobalICFGNode>(icfgNode))
        {
            rawstr << "color=purple";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr << "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::CDG *)
    {
        assert(*(EI.getCurrent()) && "No edge found!!");
        return "style=solid";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI)
    {
        SVF::CDGEdge *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        for (const auto &cond: edge->getBranchConditions())
        {
            rawstr << std::to_string(cond.second) << "|";
        }
        std::string lb = rawstr.str();
        lb.pop_back();

        return lb;
    }
};

} // End namespace SVF
#endif //SVF_CONTROLDG_H