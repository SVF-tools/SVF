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
#include "Graphs/ICFG.h"
#include "Graphs/GenericGraph.h"

namespace SVF
{
class CFBasicBlockNode;
class SVFIR;

typedef GenericEdge<CFBasicBlockNode> GenericCFBasicBlockEdgeTy;

class CFBasicBlockEdge : public GenericCFBasicBlockEdgeTy
{
public:
    typedef struct equalCFBBEdge
    {
        bool
        operator()(const CFBasicBlockEdge *lhs, const CFBasicBlockEdge *rhs) const
        {
            if (lhs->getSrcID() != rhs->getSrcID())
                return lhs->getSrcID() < rhs->getSrcID();
            else if (lhs->getDstID() != rhs->getDstID())
                return lhs->getDstID() < rhs->getDstID();
            else
                return lhs->getICFGEdge() < rhs->getICFGEdge();
        }
    } equalICFGEdgeWrapper;

    typedef OrderedSet<CFBasicBlockEdge *, equalICFGEdgeWrapper> CFBBEdgeSetTy;
    typedef CFBBEdgeSetTy::iterator iterator;
    typedef CFBBEdgeSetTy::const_iterator const_iterator;

private:
    const ICFGEdge *_icfgEdge;

public:
    CFBasicBlockEdge(CFBasicBlockNode* src, CFBasicBlockNode* dst,
                     const ICFGEdge* edge)
        : GenericCFBasicBlockEdgeTy(src, dst, 0), _icfgEdge(edge)
    {
    }

    CFBasicBlockEdge(CFBasicBlockNode* src, CFBasicBlockNode* dst)
        : GenericCFBasicBlockEdgeTy(src, dst, 0), _icfgEdge(nullptr)
    {
    }

    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockEdge &edge)
    {
        o << edge.toString();
        return o;
    }

    virtual const std::string toString() const
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "CFBBGEdge: [CFBBGNode" << getDstID() << " <-- CFBBGNode" << getSrcID() << "]\t";
        return rawstr.str();
    }

    inline const ICFGEdge *getICFGEdge() const
    {
        return _icfgEdge;
    }

    using SVF::GenericEdge<NodeType>::operator==;
    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{

    virtual inline bool operator==(const CFBasicBlockEdge *rhs) const
    {
        return (rhs->getSrcID() == this->getSrcID() && rhs->getDstID() == this->getDstID() &&
                rhs->getICFGEdge() == this->getICFGEdge());
    }
    //@}

};

typedef GenericNode<CFBasicBlockNode, CFBasicBlockEdge> GenericCFBasicBlockNodeTy;

class CFBasicBlockNode : public GenericCFBasicBlockNodeTy
{
public:
    typedef CFBasicBlockEdge::CFBBEdgeSetTy CFBBEdgeSetTy;
    typedef CFBasicBlockEdge::CFBBEdgeSetTy ::iterator iterator;
    typedef CFBasicBlockEdge::CFBBEdgeSetTy::const_iterator const_iterator;

private:
    std::vector<const ICFGNode *> _icfgNodes; /// Every CBFGNode holds a vector of ICFGNodes
    CFBBEdgeSetTy InEdges; ///< all incoming edge of this node
    CFBBEdgeSetTy OutEdges; ///< all outgoing edge of this node

public:
    CFBasicBlockNode(std::vector<const ICFGNode*> icfgNodes)
        : GenericCFBasicBlockNodeTy((*icfgNodes.begin())->getId(), 0),
          _icfgNodes(SVFUtil::move(icfgNodes))
    {
    }

    friend std::ostream &operator<<(std::ostream &o, const CFBasicBlockNode &node)
    {
        o << node.toString();
        return o;
    }

    virtual const std::string toString() const;

    inline std::string getName() const
    {
        assert(!_icfgNodes.empty() && "no ICFG nodes in CFBB");
        return (*_icfgNodes.begin())->getBB()->getName();
    }

    inline const std::vector<const ICFGNode*>& getICFGNodes() const
    {
        return _icfgNodes;
    }

    inline const SVFFunction *getFunction() const
    {
        assert(!_icfgNodes.empty() && "no ICFG nodes in CFBB");
        return (*_icfgNodes.begin())->getFun();
    }

    inline std::vector<const ICFGNode *>::const_iterator begin() const
    {
        return _icfgNodes.cbegin();
    }

    inline std::vector<const ICFGNode *>::const_iterator end() const
    {
        return _icfgNodes.cend();
    }

    inline void removeNode(const ICFGNode* node)
    {
        const auto it = std::find(_icfgNodes.begin(), _icfgNodes.end(), node);
        assert(it != _icfgNodes.end() && "icfg node not in BB?");
        _icfgNodes.erase(it);
    }

    inline void addNode(const ICFGNode* node)
    {
        _icfgNodes.push_back(node);
    }

    inline u32_t getICFGNodeNum() const
    {
        return _icfgNodes.size();
    }

public:
    /// Get incoming/outgoing edge set
    ///@{
    inline const CFBBEdgeSetTy &getOutEdges() const
    {
        return OutEdges;
    }

    inline const CFBBEdgeSetTy &getInEdges() const
    {
        return InEdges;
    }
    ///@}

    /// Has incoming/outgoing edge set
    //@{
    inline bool hasIncomingEdge() const
    {
        return (InEdges.empty() == false);
    }

    inline bool hasOutgoingEdge() const
    {
        return (OutEdges.empty() == false);
    }
    //@}

    ///  iterators
    //@{
    inline iterator OutEdgeBegin()
    {
        return OutEdges.begin();
    }

    inline iterator OutEdgeEnd()
    {
        return OutEdges.end();
    }

    inline iterator InEdgeBegin()
    {
        return InEdges.begin();
    }

    inline iterator InEdgeEnd()
    {
        return InEdges.end();
    }

    inline const_iterator OutEdgeBegin() const
    {
        return OutEdges.begin();
    }

    inline const_iterator OutEdgeEnd() const
    {
        return OutEdges.end();
    }

    inline const_iterator InEdgeBegin() const
    {
        return InEdges.begin();
    }

    inline const_iterator InEdgeEnd() const
    {
        return InEdges.end();
    }
    //@}

    /// Iterators used for SCC detection, overwrite it in child class if necessory
    //@{
    virtual inline iterator directOutEdgeBegin()
    {
        return OutEdges.begin();
    }

    virtual inline iterator directOutEdgeEnd()
    {
        return OutEdges.end();
    }

    virtual inline iterator directInEdgeBegin()
    {
        return InEdges.begin();
    }

    virtual inline iterator directInEdgeEnd()
    {
        return InEdges.end();
    }

    virtual inline const_iterator directOutEdgeBegin() const
    {
        return OutEdges.begin();
    }

    virtual inline const_iterator directOutEdgeEnd() const
    {
        return OutEdges.end();
    }

    virtual inline const_iterator directInEdgeBegin() const
    {
        return InEdges.begin();
    }

    virtual inline const_iterator directInEdgeEnd() const
    {
        return InEdges.end();
    }
    //@}

    /// Add incoming and outgoing edges
    //@{
    inline bool addIncomingEdge(CFBasicBlockEdge *inEdge)
    {
        return InEdges.insert(inEdge).second;
    }

    inline bool addOutgoingEdge(CFBasicBlockEdge *outEdge)
    {
        return OutEdges.insert(outEdge).second;
    }
    //@}

    /// Remove incoming and outgoing edges
    ///@{
    inline u32_t removeIncomingEdge(CFBasicBlockEdge *edge)
    {
        assert(InEdges.find(edge) != InEdges.end() && "can not find in edge in SVFG node");
        return InEdges.erase(edge);
    }

    inline u32_t removeOutgoingEdge(CFBasicBlockEdge *edge)
    {
        assert(OutEdges.find(edge) != OutEdges.end() && "can not find out edge in SVFG node");
        return OutEdges.erase(edge);
    }
    ///@}

    /// Find incoming and outgoing edges
    //@{
    inline CFBasicBlockEdge *hasIncomingEdge(CFBasicBlockEdge *edge) const
    {
        const_iterator it = InEdges.find(edge);
        if (it != InEdges.end())
            return *it;
        else
            return nullptr;
    }

    inline CFBasicBlockEdge *hasOutgoingEdge(CFBasicBlockEdge *edge) const
    {
        const_iterator it = OutEdges.find(edge);
        if (it != OutEdges.end())
            return *it;
        else
            return nullptr;
    }
    //@}
};

typedef GenericGraph<CFBasicBlockNode, CFBasicBlockEdge> GenericCFBasicBlockGTy;

class CFBasicBlockGraph : public GenericCFBasicBlockGTy
{
    friend class CFBasicBlockGBuilder;
private:
    u32_t _totalCFBasicBlockNode{0};
    u32_t _totalCFBasicBlockEdge{0};
public:

    CFBasicBlockGraph() = default;

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

    inline bool hasCFBasicBlockNode(NodeID id) const
    {
        return hasGNode(id);
    }


    bool hasCFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst, ICFGEdge *icfgEdge)
    {
        CFBasicBlockEdge edge(src, dst, icfgEdge);
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

    inline bool hasCFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst) const
    {
        for (const auto &e: src->getOutEdges())
        {
            if (e->getDstNode() == dst)
                return true;
        }
        return false;
    }

    CFBasicBlockEdge* getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst, const ICFGEdge *icfgEdge);

    std::vector<CFBasicBlockEdge*> getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst);

    /// Remove a ICFGEdgeWrapper
    inline void removeCFBBEdge(CFBasicBlockEdge *edge)
    {
        if (edge->getDstNode()->hasIncomingEdge(edge))
        {
            edge->getDstNode()->removeIncomingEdge(edge);
        }
        if (edge->getSrcNode()->hasOutgoingEdge(edge))
        {
            edge->getSrcNode()->removeOutgoingEdge(edge);
        }
        delete edge;
        _totalCFBasicBlockEdge--;
    }

    /// Remove a ICFGNodeWrapper
    inline void removeCFBBNode(CFBasicBlockNode *node)
    {
        std::set<CFBasicBlockEdge *> temp;
        for (CFBasicBlockEdge *e: node->getInEdges())
            temp.insert(e);
        for (CFBasicBlockEdge *e: node->getOutEdges())
            temp.insert(e);
        for (CFBasicBlockEdge *e: temp)
        {
            removeCFBBEdge(e);
        }
        removeGNode(node);
        _totalCFBasicBlockNode--;
    }


    /// Remove node from nodeID
    inline bool removeCFBBNode(NodeID id)
    {
        if (hasGNode(id))
        {
            removeCFBBNode(getGNode(id));
            return true;
        }
        return false;
    }

    /// Add ICFGEdgeWrapper
    inline bool addCFBBEdge(CFBasicBlockEdge *edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        _totalCFBasicBlockEdge++;
        return added1 && added2;
    }

    /// Add a ICFGNodeWrapper
    virtual inline void addCFBBNode(CFBasicBlockNode *node)
    {
        addGNode(node->getId(), node);
        _totalCFBasicBlockNode++;
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

} // End namespace SVF

namespace SVF
{
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
        if(node->getICFGNodes().size() == 1)
        {
            const ICFGNode* n = node->getICFGNodes()[0];
            if(SVFUtil::isa<IntraICFGNode>(n))
            {
                rawstr <<  "color=black";
            }
            else if(SVFUtil::isa<FunEntryICFGNode>(n))
            {
                rawstr <<  "color=yellow";
            }
            else if(SVFUtil::isa<FunExitICFGNode>(n))
            {
                rawstr <<  "color=green";
            }
            else if(SVFUtil::isa<CallICFGNode>(n))
            {
                rawstr <<  "color=red";
            }
            else if(SVFUtil::isa<RetICFGNode>(n))
            {
                rawstr <<  "color=blue";
            }
            else if(SVFUtil::isa<GlobalICFGNode>(n))
            {
                rawstr <<  "color=purple";
            }
            else
                assert(false && "no such kind of node!!");
        }
        else
        {
            rawstr << "color=black";
        }
        rawstr <<  "";
        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::CFBasicBlockGraph *)
    {
        CFBasicBlockEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (edge->getICFGEdge())
        {
            if (SVFUtil::isa<CallCFGEdge>(edge->getICFGEdge()))
            {
                return "style=solid,color=red";
            }
            else if (SVFUtil::isa<RetCFGEdge>(edge->getICFGEdge()))
                return "style=solid,color=blue";
            else
                return "style=solid";
        }
        else
        {
            return "style=solid";
        }
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI)
    {
        CFBasicBlockEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        if (edge->getICFGEdge())
        {
            if (const CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge->getICFGEdge()))
                rawstr << dirCall->getCallSite();
            else if (const RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge->getICFGEdge()))
                rawstr << dirRet->getCallSite();
        }
        return rawstr.str();
    }
};
}
#endif //SVF_CFBASICBLOCKG_H