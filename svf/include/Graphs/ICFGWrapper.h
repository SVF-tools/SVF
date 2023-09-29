//===- ICFGWrapper.h -- ICFG Wrapper-----------------------------------------//
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
 * ICFGWrapper.h
 *
 *  Created on: Sep 26, 2023
 *      Author: Xiao Cheng
 */

#ifndef SVF_ICFGWRAPPER_H
#define SVF_ICFGWRAPPER_H

#include "Graphs/ICFG.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{
class ICFGNodeWrapper;

typedef GenericEdge<ICFGNodeWrapper> GenericICFGWrapperEdgeTy;


class ICFGEdgeWrapper : public GenericICFGWrapperEdgeTy
{
public:
    typedef struct equalICFGEdgeWrapper
    {
        bool
        operator()(const ICFGEdgeWrapper *lhs, const ICFGEdgeWrapper *rhs) const
        {
            if (lhs->getSrcID() != rhs->getSrcID())
                return lhs->getSrcID() < rhs->getSrcID();
            else if (lhs->getDstID() != rhs->getDstID())
                return lhs->getDstID() < rhs->getDstID();
            else
                return lhs->getICFGEdge() < rhs->getICFGEdge();
        }
    } equalICFGEdgeWrapper;

    typedef OrderedSet<ICFGEdgeWrapper *, equalICFGEdgeWrapper> ICFGEdgeWrapperSetTy;
    typedef ICFGEdgeWrapperSetTy::iterator iterator;
    typedef ICFGEdgeWrapperSetTy::const_iterator const_iterator;

private:
    ICFGEdge *_icfgEdge;

public:
    ICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst, ICFGEdge *edge) :
        GenericICFGWrapperEdgeTy(src, dst, 0), _icfgEdge(edge)
    {

    }

    ~ICFGEdgeWrapper() {}

    virtual const std::string toString() const
    {
        return _icfgEdge->toString();
    }

    inline ICFGEdge *getICFGEdge() const
    {
        return _icfgEdge;
    }

    inline void setICFGEdge(ICFGEdge *edge)
    {
        _icfgEdge = edge;
    }

    using SVF::GenericEdge<NodeType>::operator==;
    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{

    virtual inline bool operator==(const ICFGEdgeWrapper *rhs) const
    {
        return (rhs->getSrcID() == this->getSrcID() && rhs->getDstID() == this->getDstID() &&
                rhs->getICFGEdge() == this->getICFGEdge());
    }
    //@}

};

typedef GenericNode<ICFGNodeWrapper, ICFGEdgeWrapper> GenericICFGNodeWrapperTy;

class ICFGNodeWrapper : public GenericICFGNodeWrapperTy
{
public:
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy ICFGEdgeWrapperSetTy;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::iterator iterator;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::const_iterator const_iterator;
private:
    const ICFGNode *_icfgNode;
    ICFGNodeWrapper *_callICFGNodeWrapper{nullptr};
    ICFGNodeWrapper *_retICFGNodeWrapper{nullptr};
    ICFGEdgeWrapperSetTy InEdges; ///< all incoming edge of this node
    ICFGEdgeWrapperSetTy OutEdges; ///< all outgoing edge of this node
public:
    ICFGNodeWrapper(const ICFGNode *node) : GenericICFGNodeWrapperTy(node->getId(), 0), _icfgNode(node) {}

    virtual ~ICFGNodeWrapper()
    {
        for (auto *edge: OutEdges)
            delete edge;
    }

    virtual const std::string toString() const
    {
        return _icfgNode->toString();
    }

    const ICFGNode *getICFGNode() const
    {
        return _icfgNode;
    }

    ICFGNodeWrapper *getCallICFGNodeWrapper() const
    {
        return _callICFGNodeWrapper;
    }

    void setCallICFGNodeWrapper(ICFGNodeWrapper *node)
    {
        _callICFGNodeWrapper = node;
    }

    ICFGNodeWrapper *getRetICFGNodeWrapper() const
    {
        return _retICFGNodeWrapper;
    }

    void setRetICFGNodeWrapper(ICFGNodeWrapper *node)
    {
        _retICFGNodeWrapper = node;
    }


    /// Get incoming/outgoing edge set
    ///@{
    inline const ICFGEdgeWrapperSetTy &getOutEdges() const
    {
        return OutEdges;
    }

    inline const ICFGEdgeWrapperSetTy &getInEdges() const
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
    inline bool addIncomingEdge(ICFGEdgeWrapper *inEdge)
    {
        return InEdges.insert(inEdge).second;
    }

    inline bool addOutgoingEdge(ICFGEdgeWrapper *outEdge)
    {
        return OutEdges.insert(outEdge).second;
    }
    //@}

    /// Remove incoming and outgoing edges
    ///@{
    inline u32_t removeIncomingEdge(ICFGEdgeWrapper *edge)
    {
        iterator it = InEdges.find(edge);
        assert(it != InEdges.end() && "can not find in edge in SVFG node");
        return InEdges.erase(edge);
    }

    inline u32_t removeOutgoingEdge(ICFGEdgeWrapper *edge)
    {
        iterator it = OutEdges.find(edge);
        assert(it != OutEdges.end() && "can not find out edge in SVFG node");
        return OutEdges.erase(edge);
    }
    ///@}

    /// Find incoming and outgoing edges
    //@{
    inline ICFGEdgeWrapper *hasIncomingEdge(ICFGEdgeWrapper *edge) const
    {
        const_iterator it = InEdges.find(edge);
        if (it != InEdges.end())
            return *it;
        else
            return nullptr;
    }

    inline ICFGEdgeWrapper *hasOutgoingEdge(ICFGEdgeWrapper *edge) const
    {
        const_iterator it = OutEdges.find(edge);
        if (it != OutEdges.end())
            return *it;
        else
            return nullptr;
    }
    //@}
};

typedef std::vector<std::pair<NodeID, NodeID>> NodePairVector;
typedef GenericGraph<ICFGNodeWrapper, ICFGEdgeWrapper> GenericICFGWrapperTy;

class ICFGWrapper : public GenericICFGWrapperTy
{
public:

    typedef Map<NodeID, ICFGNodeWrapper *> ICFGWrapperNodeIDToNodeMapTy;
    typedef ICFGEdgeWrapper::ICFGEdgeWrapperSetTy ICFGEdgeWrapperSetTy;
    typedef ICFGWrapperNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGWrapperNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef std::vector<const ICFGNodeWrapper *> ICFGNodeWrapperVector;
    typedef std::vector<std::pair<const ICFGNodeWrapper *, const ICFGNodeWrapper *>> ICFGNodeWrapperPairVector;
    typedef Map<const SVFFunction *, const ICFGNodeWrapper *> SVFFuncToICFGNodeWrapperMap;
private:
    static std::unique_ptr<ICFGWrapper> _icfgWrapper; ///< Singleton pattern here
    SVFFuncToICFGNodeWrapperMap _funcToFunEntry;
    SVFFuncToICFGNodeWrapperMap _funcToFunExit;
    u32_t _edgeWrapperNum;        ///< total num of node
    u32_t _nodeWrapperNum;        ///< total num of edge
    ICFG *_icfg;

    /// Constructor
    ICFGWrapper(ICFG *icfg) : _edgeWrapperNum(0), _nodeWrapperNum(0), _icfg(icfg)
    {
        assert(_icfg && "ICFGWrapper constructor cannot accept nullptr of icfg");
    }

public:
    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper(ICFG *_icfg)
    {
        if (_icfgWrapper == nullptr)
        {
            _icfgWrapper = std::make_unique<ICFGWrapper>(ICFGWrapper(_icfg));
        }
        return _icfgWrapper;
    }

    static inline const std::unique_ptr<ICFGWrapper> &getICFGWrapper()
    {
        assert(_icfgWrapper && "icfg wrapper not init?");
        return _icfgWrapper;
    }

    static void releaseICFGWrapper()
    {
        ICFGWrapper *w = _icfgWrapper.release();
        delete w;
        _icfgWrapper = nullptr;
    }
    //@}

    /// Destructor
    virtual ~ICFGWrapper() = default;

    /// Get a ICFG node wrapper
    inline ICFGNodeWrapper *getICFGNodeWrapper(NodeID id) const
    {
        if (!hasICFGNodeWrapper(id))
            return nullptr;
        return getGNode(id);
    }

    /// Whether has the ICFGNodeWrapper
    inline bool hasICFGNodeWrapper(NodeID id) const
    {
        return hasGNode(id);
    }

    /// Whether we has a ICFG Edge Wrapper
    bool hasICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst, ICFGEdge *icfgEdge)
    {
        ICFGEdgeWrapper edge(src, dst, icfgEdge);
        ICFGEdgeWrapper *outEdge = src->hasOutgoingEdge(&edge);
        ICFGEdgeWrapper *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge)
        {
            assert(outEdge == inEdge && "edges not match");
            return true;
        }
        else
            return false;
    }

    ICFGEdgeWrapper *hasICFGEdgeWrapper(ICFGNodeWrapper *src, ICFGNodeWrapper *dst)
    {
        for (const auto &e: src->getOutEdges())
        {
            if (e->getDstNode() == dst)
                return e;
        }
        return nullptr;
    }

    /// Get a ICFG edge wrapper according to src, dst and icfgEdge
    ICFGEdgeWrapper *
    getICFGEdgeWrapper(const ICFGNodeWrapper *src, const ICFGNodeWrapper *dst, ICFGEdge *icfgEdge)
    {
        ICFGEdgeWrapper *edge = nullptr;
        size_t counter = 0;
        for (ICFGEdgeWrapper::ICFGEdgeWrapperSetTy::iterator iter = src->OutEdgeBegin();
                iter != src->OutEdgeEnd(); ++iter)
        {
            if ((*iter)->getDstID() == dst->getId())
            {
                counter++;
                edge = (*iter);
            }
        }
        assert(counter <= 1 && "there's more than one edge between two ICFGNodeWrappers");
        return edge;
    }

    /// View graph from the debugger
    void view();

    /// Dump graph into dot file
    void dump(const std::string &filename);

    /// Remove a ICFGEdgeWrapper
    inline void removeICFGEdgeWrapper(ICFGEdgeWrapper *edge)
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
        _edgeWrapperNum--;
    }

    /// Remove a ICFGNodeWrapper
    inline void removeICFGNodeWrapper(ICFGNodeWrapper *node)
    {
        std::set<ICFGEdgeWrapper *> temp;
        for (ICFGEdgeWrapper *e: node->getInEdges())
            temp.insert(e);
        for (ICFGEdgeWrapper *e: node->getOutEdges())
            temp.insert(e);
        for (ICFGEdgeWrapper *e: temp)
        {
            removeICFGEdgeWrapper(e);
        }
        removeGNode(node);
        _nodeWrapperNum--;
    }

    /// Remove node from nodeID
    inline bool removeICFGNodeWrapper(NodeID id)
    {
        if (hasICFGNodeWrapper(id))
        {
            removeICFGNodeWrapper(getICFGNodeWrapper(id));
            return true;
        }
        return false;
    }

    /// Add ICFGEdgeWrapper
    inline bool addICFGEdgeWrapper(ICFGEdgeWrapper *edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        _edgeWrapperNum++;
        return true;
    }

    /// Add a ICFGNodeWrapper
    virtual inline void addICFGNodeWrapper(ICFGNodeWrapper *node)
    {
        addGNode(node->getId(), node);
        _nodeWrapperNum++;
    }

    const ICFGNodeWrapper *getFunEntry(const SVFFunction *func) const
    {
        auto it = _funcToFunEntry.find(func);
        assert(it != _funcToFunEntry.end() && "no entry?");
        return it->second;
    }

    const ICFGNodeWrapper *getFunExit(const SVFFunction *func) const
    {
        auto it = _funcToFunExit.find(func);
        assert(it != _funcToFunExit.end() && "no exit?");
        return it->second;
    }

    /// Add ICFGEdgeWrappers from nodeid pair
    void addICFGNodeWrapperFromICFGNode(const ICFGNode *src);

    inline u32_t getNodeWrapperNum() const
    {
        return _nodeWrapperNum;
    }

    inline u32_t getEdgeWrapperNum() const
    {
        return _edgeWrapperNum;
    }

};

class ICFGWrapperBuilder
{
public:
    ICFGWrapperBuilder() {}

    ~ICFGWrapperBuilder() {}

    void build(ICFG *icfg);
};
}

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::ICFGNodeWrapper *>
    : public GenericGraphTraits<SVF::GenericNode<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *>
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse<SVF::ICFGNodeWrapper *> > : public GenericGraphTraits<
    Inverse<SVF::GenericNode<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *> >
{
};

template<>
struct GenericGraphTraits<SVF::ICFGWrapper *>
    : public GenericGraphTraits<SVF::GenericGraph<SVF::ICFGNodeWrapper, SVF::ICFGEdgeWrapper> *>
{
    typedef SVF::ICFGNodeWrapper *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::ICFGWrapper *> : public DOTGraphTraits<SVF::SVFIR *>
{

    typedef SVF::ICFGNodeWrapper NodeType;

    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVF::SVFIR *>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::ICFGWrapper *)
    {
        return "ICFGWrapper";
    }

    static bool isNodeHidden(NodeType *node, SVF::ICFGWrapper *graph)
    {
        return false;
    }

    std::string getNodeLabel(NodeType *node, SVF::ICFGWrapper *graph)
    {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::ICFGWrapper *)
    {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if (const SVF::IntraICFGNode *bNode = SVF::SVFUtil::dyn_cast<SVF::IntraICFGNode>(node->getICFGNode()))
        {
            rawstr << "IntraICFGNode ID: " << bNode->getId() << " \t";
            SVF::SVFIR::SVFStmtList &edges = SVF::SVFIR::getPAG()->getSVFStmtList(bNode);
            if (edges.empty())
            {
                rawstr << bNode->getInst()->toString() << " \t";
            }
            else
            {
                for (SVF::SVFIR::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
                {
                    const SVF::PAGEdge *edge = *it;
                    rawstr << edge->toString();
                }
            }
            rawstr << " {fun: " << bNode->getFun()->getName() << "}";
        }
        else if (const SVF::FunEntryICFGNode *entry = SVF::SVFUtil::dyn_cast<SVF::FunEntryICFGNode>(
                     node->getICFGNode()))
        {
            rawstr << entry->toString();
        }
        else if (const SVF::FunExitICFGNode *exit = SVF::SVFUtil::dyn_cast<SVF::FunExitICFGNode>(
                     node->getICFGNode()))
        {
            rawstr << exit->toString();
        }
        else if (const SVF::CallICFGNode *call = SVF::SVFUtil::dyn_cast<SVF::CallICFGNode>(node->getICFGNode()))
        {
            rawstr << call->toString();
        }
        else if (const SVF::RetICFGNode *ret = SVF::SVFUtil::dyn_cast<SVF::RetICFGNode>(node->getICFGNode()))
        {
            rawstr << ret->toString();
        }
        else if (const SVF::GlobalICFGNode *glob = SVF::SVFUtil::dyn_cast<SVF::GlobalICFGNode>(
                     node->getICFGNode()))
        {
            SVF::SVFIR::SVFStmtList &edges = SVF::SVFIR::getPAG()->getSVFStmtList(glob);
            for (SVF::SVFIR::SVFStmtList::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it)
            {
                const SVF::PAGEdge *edge = *it;
                rawstr << edge->toString();
            }
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::ICFGWrapper *)
    {
        std::string str;
        std::stringstream rawstr(str);

        if (SVF::SVFUtil::isa<SVF::IntraICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=black";
        }
        else if (SVF::SVFUtil::isa<SVF::FunEntryICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=yellow";
        }
        else if (SVF::SVFUtil::isa<SVF::FunExitICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=green";
        }
        else if (SVF::SVFUtil::isa<SVF::CallICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=red";
        }
        else if (SVF::SVFUtil::isa<SVF::RetICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=blue";
        }
        else if (SVF::SVFUtil::isa<SVF::GlobalICFGNode>(node->getICFGNode()))
        {
            rawstr << "color=purple";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr << "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::ICFGWrapper *)
    {
        SVF::ICFGEdgeWrapper *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (!edge->getICFGEdge())
            return "style=solid";
        if (SVF::SVFUtil::isa<SVF::CallCFGEdge>(edge->getICFGEdge()))
            return "style=solid,color=red";
        else if (SVF::SVFUtil::isa<SVF::RetCFGEdge>(edge->getICFGEdge()))
            return "style=solid,color=blue";
        else
            return "style=solid";
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI)
    {
        SVF::ICFGEdgeWrapper *edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        if (!edge->getICFGEdge())
            return rawstr.str();
        if (SVF::CallCFGEdge *dirCall = SVF::SVFUtil::dyn_cast<SVF::CallCFGEdge>(edge->getICFGEdge()))
            rawstr << dirCall->getCallSite();
        else if (SVF::RetCFGEdge *dirRet = SVF::SVFUtil::dyn_cast<SVF::RetCFGEdge>(edge->getICFGEdge()))
            rawstr << dirRet->getCallSite();

        return rawstr.str();
    }
};

} // End namespace SVF

#endif //SVF_ICFGWRAPPER_H
