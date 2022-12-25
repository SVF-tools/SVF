//===- CFBG.h ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * CFBG.h
 *
 *  Created on: 24 Dec. 2022
 *      Author: Xiao, Jiawei
 */

#ifndef SVF_CFBG_H
#define SVF_CFBG_H
#include "Util/SVFUtil.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/GenericGraph.h"

namespace SVF {
class CFBGNode;
class SVFIR;

typedef GenericEdge<CFBGNode> GenericCFBGEdgeTy;

class CFBGEdge : public GenericCFBGEdgeTy {
public:
    CFBGEdge(CFBGNode *src, CFBGNode *dst) : GenericCFBGEdgeTy(src, dst, 0) {}


    friend std::ostream &operator<<(std::ostream &o, const CFBGEdge &edge) {
        o << edge.toString();
        return o;
    }

    virtual const std::string toString() const {
        return std::to_string(getSrcID()) + " --> " + std::to_string(getDstID());
    }
};

typedef GenericNode<CFBGNode, CFBGEdge> GenericCFBGNodeTy;

class CFBGNode : public GenericCFBGNodeTy {
private:
    const SVFBasicBlock *_svfBasicBlock; /// Every CFBGNode holds a SVFBasicBlock
    std::vector<const ICFGNode *> _icfgNodes; /// Every CBFGNode holds a vector of ICFGNodes

public:
    CFBGNode(u32_t id, const SVFBasicBlock *svfBasicBlock);

    friend std::ostream &operator<<(std::ostream &o, const CFBGNode &node) {
        o << node.toString();
        return o;
    }

    virtual const std::string toString() const {
        std::string rawStr;
        std::stringstream stringstream(rawStr);
        stringstream << "Block Name: " << _svfBasicBlock->getName() << "\n";
        for (const auto &icfgNode: _icfgNodes) {
            stringstream << icfgNode->toString() << "\n";
        }
        return stringstream.str();
    }

    inline std::string getName() const {
        return _svfBasicBlock->getName();
    }

    inline const SVFBasicBlock *getSVFBasicBlock() const {
        return _svfBasicBlock;
    }

    inline const SVFFunction *getFunction() const {
        return _svfBasicBlock->getFunction();
    }

    inline std::vector<const ICFGNode *>::const_iterator begin() const {
        return _icfgNodes.cbegin();
    }

    inline std::vector<const ICFGNode *>::const_iterator end() const {
        return _icfgNodes.cend();
    }

    inline void setSVFBasicBlock(const SVFBasicBlock *svfBasicBlock);
};

typedef GenericGraph<CFBGNode, CFBGEdge> GenericCFBGTy;

class CFBG : public GenericCFBGTy {
    friend class CFBGBuilder;

public:
    typedef Map<const SVFBasicBlock *, CFBGNode *> SVFBasicBlockToCFBGNodeMap;

private:
    SVFBasicBlockToCFBGNodeMap _bbToNode;
    const SVFFunction *_svfFunction;
    u32_t _totalCFBGNode{0};
    u32_t _totalCFBGEdge{0};

public:

    CFBG(const SVFFunction *svfFunction) : _svfFunction(svfFunction) {

    }

    ~CFBG() override = default;

    /// Dump graph into dot file
    void dump(const std::string &filename) {
        GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
    }

    inline CFBGNode *getCFBGNode(u32_t id) const {
        if (!hasGNode(id)) return nullptr;
        return getGNode(id);
    }

    inline CFBGNode *getCFBGNode(const SVFBasicBlock *bb) const {
        auto it = _bbToNode.find(bb);
        if (it == _bbToNode.end()) return nullptr;
        return it->second;
    }

    inline bool hasCFBGEdge(CFBGNode *src, CFBGNode *dst) {
        CFBGEdge edge(src, dst);
        CFBGEdge *outEdge = src->hasOutgoingEdge(&edge);
        CFBGEdge *inEdge = dst->hasIncomingEdge(&edge);
        if (outEdge && inEdge) {
            assert(outEdge == inEdge && "edges not match");
            return true;
        } else
            return false;
    }

    CFBGEdge* getCFBGEdge(const CFBGNode *src, const CFBGNode *dst) {
        CFBGEdge *edge = nullptr;
        size_t counter = 0;
        for (auto iter = src->OutEdgeBegin();
             iter != src->OutEdgeEnd(); ++iter) {
            if ((*iter)->getDstID() == dst->getId()) {
                counter++;
                edge = (*iter);
            }
        }
        assert(counter <= 1 && "there's more than one edge between two nodes");
        return edge;
    }

    CFBGEdge* getCFBGEdge(const SVFBasicBlock *src, const SVFBasicBlock *dst) {
        return getCFBGEdge(getCFBGNode(src), getCFBGNode(dst));
    }


private:

    /// Add a CFBGNode
    virtual inline const CFBGNode *getOrAddCFBGNode(const SVFBasicBlock *bb) {
        auto it = _bbToNode.find(bb);
        if (it != _bbToNode.end()) return it->second;
        CFBGNode *node = new CFBGNode(_totalCFBGNode++, bb);
        _bbToNode[bb] = node;
        addGNode(node->getId(), node);
        return node;
    }

    inline const CFBGEdge *getOrAddCFBGEdge(CFBGNode *src, CFBGNode *dst) {
        if (const CFBGEdge *edge = getCFBGEdge(src, dst)) return edge;
        CFBGEdge *edge = new CFBGEdge(src, dst);
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        _totalCFBGEdge++;
        return edge;
    }

};

class CFBGBuilder {

private:
    std::unique_ptr<CFBG> _CFBG;

public:
    CFBGBuilder(const SVFFunction *func) : _CFBG(std::make_unique<CFBG>(func)) {}

    void build() {
        for (const auto &bb: *_CFBG->_svfFunction) {
            _CFBG->getOrAddCFBGNode(bb);
        }
        for (const auto &bb: *_CFBG->_svfFunction) {
            for (const auto &succ: bb->getSuccessors()) {
                _CFBG->getOrAddCFBGEdge(_CFBG->getCFBGNode(bb),
                                               _CFBG->getCFBGNode(succ));
            }
        }
    }

    inline std::unique_ptr<CFBG> &getCFBG() {
        return _CFBG;
    }
};
}


namespace SVF {
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph ICFGTraversals.
 */
template<>
struct GenericGraphTraits<SVF::CFBGNode *>
        : public GenericGraphTraits<SVF::GenericNode<SVF::CFBGNode, SVF::CFBGEdge> *> {
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse ICFGTraversal.
template<>
struct GenericGraphTraits<Inverse<SVF::CFBGNode *> > : public GenericGraphTraits<
        Inverse<SVF::GenericNode<SVF::CFBGNode, SVF::CFBGEdge> *> > {
};

template<>
struct GenericGraphTraits<SVF::CFBG *>
        : public GenericGraphTraits<SVF::GenericGraph<SVF::CFBGNode, SVF::CFBGEdge> *> {
    typedef SVF::CFBGNode *NodeRef;
};

template<>
struct DOTGraphTraits<SVF::CFBG *> : public DOTGraphTraits<SVF::SVFIR *> {

    typedef SVF::CFBGNode NodeType;

    DOTGraphTraits(bool isSimple = false) :
            DOTGraphTraits<SVF::SVFIR *>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(SVF::CFBG *) {
        return "CFBG";
    }

    std::string getNodeLabel(NodeType *node, SVF::CFBG *graph) {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, SVF::CFBG *) {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        rawstr << node->toString();

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVF::CFBG *) {
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "color=black";
        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *, EdgeIter EI, SVF::CFBG *) {
        return "style=solid";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *, EdgeIter EI) {
        return "";
    }
};

} // End namespace SVF
#endif //SVF_CFBG_H
