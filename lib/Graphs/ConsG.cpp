//===- ConsG.cpp -- Constraint graph representation-----------------------------//
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
 * ConstraintGraph.cpp
 *
 *  Created on: Oct 14, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/ConsG.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;


ConstraintNode::SCCEdgeFlag ConstraintNode::sccEdgeFlag = ConstraintNode::Direct;

/*!
 * Start building constraint graph
 */
void ConstraintGraph::buildCG()
{

    // initialize nodes
    for(SVFIR::iterator it = pag->begin(), eit = pag->end(); it!=eit; ++it)
    {
        addConstraintNode(new ConstraintNode(it->first), it->first);
    }

    // initialize edges
    SVFStmt::SVFStmtSetTy& addrs = getPAGEdgeSet(SVFStmt::Addr);
    for (SVFStmt::SVFStmtSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter)
    {
        const AddrStmt* edge = SVFUtil::cast<AddrStmt>(*iter);
        addAddrCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& copys = getPAGEdgeSet(SVFStmt::Copy);
    for (SVFStmt::SVFStmtSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter)
    {
        const CopyStmt* edge = SVFUtil::cast<CopyStmt>(*iter);
        addCopyCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& phis = getPAGEdgeSet(SVFStmt::Phi);
    for (SVFStmt::SVFStmtSetTy::iterator iter = phis.begin(), eiter =
                phis.end(); iter != eiter; ++iter)
    {
        const PhiStmt* edge = SVFUtil::cast<PhiStmt>(*iter);
        for(const auto opVar : edge->getOpndVars())
            addCopyCGEdge(opVar->getId(),edge->getResID());
    }

    SVFStmt::SVFStmtSetTy& selects = getPAGEdgeSet(SVFStmt::Select);
    for (SVFStmt::SVFStmtSetTy::iterator iter = selects.begin(), eiter =
                selects.end(); iter != eiter; ++iter)
    {
        const SelectStmt* edge = SVFUtil::cast<SelectStmt>(*iter);
        for(const auto opVar : edge->getOpndVars())
            addCopyCGEdge(opVar->getId(),edge->getResID());
    }

    SVFStmt::SVFStmtSetTy& calls = getPAGEdgeSet(SVFStmt::Call);
    for (SVFStmt::SVFStmtSetTy::iterator iter = calls.begin(), eiter =
                calls.end(); iter != eiter; ++iter)
    {
        const CallPE* edge = SVFUtil::cast<CallPE>(*iter);
        addCopyCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& rets = getPAGEdgeSet(SVFStmt::Ret);
    for (SVFStmt::SVFStmtSetTy::iterator iter = rets.begin(), eiter =
                rets.end(); iter != eiter; ++iter)
    {
        const RetPE* edge = SVFUtil::cast<RetPE>(*iter);
        addCopyCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& tdfks = getPAGEdgeSet(SVFStmt::ThreadFork);
    for (SVFStmt::SVFStmtSetTy::iterator iter = tdfks.begin(), eiter =
                tdfks.end(); iter != eiter; ++iter)
    {
        const TDForkPE* edge = SVFUtil::cast<TDForkPE>(*iter);
        addCopyCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& tdjns = getPAGEdgeSet(SVFStmt::ThreadJoin);
    for (SVFStmt::SVFStmtSetTy::iterator iter = tdjns.begin(), eiter =
                tdjns.end(); iter != eiter; ++iter)
    {
        const TDJoinPE* edge = SVFUtil::cast<TDJoinPE>(*iter);
        addCopyCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& ngeps = getPAGEdgeSet(SVFStmt::Gep);
    for (SVFStmt::SVFStmtSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter)
    {
        GepStmt* edge = SVFUtil::cast<GepStmt>(*iter);
        if(edge->isVariantFieldGep())
            addVariantGepCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
        else
            addNormalGepCGEdge(edge->getRHSVarID(),edge->getLHSVarID(),edge->getLocationSet());
    }

    SVFStmt::SVFStmtSetTy& loads = getPAGEdgeSet(SVFStmt::Load);
    for (SVFStmt::SVFStmtSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        LoadStmt* edge = SVFUtil::cast<LoadStmt>(*iter);
        addLoadCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }

    SVFStmt::SVFStmtSetTy& stores = getPAGEdgeSet(SVFStmt::Store);
    for (SVFStmt::SVFStmtSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        StoreStmt* edge = SVFUtil::cast<StoreStmt>(*iter);
        addStoreCGEdge(edge->getRHSVarID(),edge->getLHSVarID());
    }
}


/*!
 * Memory has been cleaned up at GenericGraph
 */
void ConstraintGraph::destroy()
{
}

/*!
 * Constructor for address constraint graph edge
 */
AddrCGEdge::AddrCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id)
    : ConstraintEdge(s,d,Addr,id)
{
    // Retarget addr edges may lead s to be a dummy node
    PAGNode* node = SVFIR::getPAG()->getGNode(s->getId());
    if (!SVFModule::pagReadFromTXT())
        assert(!SVFUtil::isa<DummyValVar>(node) && "a dummy node??");
}

/*!
 * Add an address edge
 */
AddrCGEdge* ConstraintGraph::addAddrCGEdge(NodeID src, NodeID dst)
{
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Addr))
        return nullptr;
    AddrCGEdge* edge = new AddrCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = AddrCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingAddrEdge(edge);
    dstNode->addIncomingAddrEdge(edge);
    return edge;
}

/*!
 * Add Copy edge
 */
CopyCGEdge* ConstraintGraph::addCopyCGEdge(NodeID src, NodeID dst)
{

    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Copy)
            || srcNode == dstNode)
        return nullptr;

    CopyCGEdge* edge = new CopyCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingCopyEdge(edge);
    dstNode->addIncomingCopyEdge(edge);
    return edge;
}


/*!
 * Add Gep edge
 */
NormalGepCGEdge*  ConstraintGraph::addNormalGepCGEdge(NodeID src, NodeID dst, const LocationSet& ls)
{
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::NormalGep))
        return nullptr;

    NormalGepCGEdge* edge = new NormalGepCGEdge(srcNode, dstNode,ls, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingGepEdge(edge);
    dstNode->addIncomingGepEdge(edge);
    return edge;
}

/*!
 * Add variant gep edge
 */
VariantGepCGEdge* ConstraintGraph::addVariantGepCGEdge(NodeID src, NodeID dst)
{
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::VariantGep))
        return nullptr;

    VariantGepCGEdge* edge = new VariantGepCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingGepEdge(edge);
    dstNode->addIncomingGepEdge(edge);
    return edge;
}

/*!
 * Add Load edge
 */
LoadCGEdge* ConstraintGraph::addLoadCGEdge(NodeID src, NodeID dst)
{
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Load))
        return nullptr;

    LoadCGEdge* edge = new LoadCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = LoadCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingLoadEdge(edge);
    dstNode->addIncomingLoadEdge(edge);
    return edge;
}

/*!
 * Add Store edge
 */
StoreCGEdge* ConstraintGraph::addStoreCGEdge(NodeID src, NodeID dst)
{
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Store))
        return nullptr;

    StoreCGEdge* edge = new StoreCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = StoreCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingStoreEdge(edge);
    dstNode->addIncomingStoreEdge(edge);
    return edge;
}


/*!
 * Re-target dst node of an edge
 *
 * (1) Remove edge from old dst target,
 * (2) Change edge dst id and
 * (3) Add modifed edge into new dst
 */
void ConstraintGraph::reTargetDstOfEdge(ConstraintEdge* edge, ConstraintNode* newDstNode)
{
    NodeID newDstNodeID = newDstNode->getId();
    NodeID srcId = edge->getSrcID();
    if(LoadCGEdge* load = SVFUtil::dyn_cast<LoadCGEdge>(edge))
    {
        removeLoadEdge(load);
        addLoadCGEdge(srcId,newDstNodeID);
    }
    else if(StoreCGEdge* store = SVFUtil::dyn_cast<StoreCGEdge>(edge))
    {
        removeStoreEdge(store);
        addStoreCGEdge(srcId,newDstNodeID);
    }
    else if(CopyCGEdge* copy = SVFUtil::dyn_cast<CopyCGEdge>(edge))
    {
        removeDirectEdge(copy);
        addCopyCGEdge(srcId,newDstNodeID);
    }
    else if(NormalGepCGEdge* gep = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
    {
        const LocationSet ls = gep->getLocationSet();
        removeDirectEdge(gep);
        addNormalGepCGEdge(srcId,newDstNodeID,ls);
    }
    else if(VariantGepCGEdge* gep = SVFUtil::dyn_cast<VariantGepCGEdge>(edge))
    {
        removeDirectEdge(gep);
        addVariantGepCGEdge(srcId,newDstNodeID);
    }
    else if(AddrCGEdge* addr = SVFUtil::dyn_cast<AddrCGEdge>(edge))
    {
        removeAddrEdge(addr);
    }
    else
        assert(false && "no other edge type!!");
}

/*!
 * Re-target src node of an edge
 * (1) Remove edge from old src target,
 * (2) Change edge src id and
 * (3) Add modified edge into new src
 */
void ConstraintGraph::reTargetSrcOfEdge(ConstraintEdge* edge, ConstraintNode* newSrcNode)
{
    NodeID newSrcNodeID = newSrcNode->getId();
    NodeID dstId = edge->getDstID();
    if(LoadCGEdge* load = SVFUtil::dyn_cast<LoadCGEdge>(edge))
    {
        removeLoadEdge(load);
        addLoadCGEdge(newSrcNodeID,dstId);
    }
    else if(StoreCGEdge* store = SVFUtil::dyn_cast<StoreCGEdge>(edge))
    {
        removeStoreEdge(store);
        addStoreCGEdge(newSrcNodeID,dstId);
    }
    else if(CopyCGEdge* copy = SVFUtil::dyn_cast<CopyCGEdge>(edge))
    {
        removeDirectEdge(copy);
        addCopyCGEdge(newSrcNodeID,dstId);
    }
    else if(NormalGepCGEdge* gep = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
    {
        const LocationSet ls = gep->getLocationSet();
        removeDirectEdge(gep);
        addNormalGepCGEdge(newSrcNodeID,dstId,ls);
    }
    else if(VariantGepCGEdge* gep = SVFUtil::dyn_cast<VariantGepCGEdge>(edge))
    {
        removeDirectEdge(gep);
        addVariantGepCGEdge(newSrcNodeID,dstId);
    }
    else if(AddrCGEdge* addr = SVFUtil::dyn_cast<AddrCGEdge>(edge))
    {
        removeAddrEdge(addr);
    }
    else
        assert(false && "no other edge type!!");
}

/*!
 * Remove addr edge from their src and dst edge sets
 */
void ConstraintGraph::removeAddrEdge(AddrCGEdge* edge)
{
    getConstraintNode(edge->getSrcID())->removeOutgoingAddrEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingAddrEdge(edge);
    u32_t num = AddrCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove load edge from their src and dst edge sets
 */
void ConstraintGraph::removeLoadEdge(LoadCGEdge* edge)
{
    getConstraintNode(edge->getSrcID())->removeOutgoingLoadEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingLoadEdge(edge);
    u32_t num = LoadCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove store edge from their src and dst edge sets
 */
void ConstraintGraph::removeStoreEdge(StoreCGEdge* edge)
{
    getConstraintNode(edge->getSrcID())->removeOutgoingStoreEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingStoreEdge(edge);
    u32_t num = StoreCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove edges from their src and dst edge sets
 */
void ConstraintGraph::removeDirectEdge(ConstraintEdge* edge)
{

    getConstraintNode(edge->getSrcID())->removeOutgoingDirectEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingDirectEdge(edge);
    u32_t num = directEdgeSet.erase(edge);

    assert(num && "edge not in the set, can not remove!!!");
    delete edge;
}

/*!
 * Move incoming direct edges of a sub node which is outside SCC to its rep node
 * Remove incoming direct edges of a sub node which is inside SCC from its rep node
 */
bool ConstraintGraph::moveInEdgesToRepNode(ConstraintNode* node, ConstraintNode* rep )
{
    std::vector<ConstraintEdge*> sccEdges;
    std::vector<ConstraintEdge*> nonSccEdges;
    for (ConstraintNode::const_iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit;
            ++it)
    {
        ConstraintEdge* subInEdge = *it;
        if(sccRepNode(subInEdge->getSrcID()) != rep->getId())
            nonSccEdges.push_back(subInEdge);
        else
        {
            sccEdges.push_back(subInEdge);
        }
    }
    // if this edge is outside scc, then re-target edge dst to rep
    while(!nonSccEdges.empty())
    {
        ConstraintEdge* edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetDstOfEdge(edge,rep);
    }

    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty())
    {
        ConstraintEdge* edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(SVFUtil::isa<CopyCGEdge>(edge))
            removeDirectEdge(edge);
        else if (SVFUtil::isa<GepCGEdge>(edge))
        {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge))
            {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(SVFUtil::isa<LoadCGEdge>(edge) || SVFUtil::isa<StoreCGEdge>(edge))
            reTargetDstOfEdge(edge,rep);
        else if(AddrCGEdge* addr = SVFUtil::dyn_cast<AddrCGEdge>(edge))
        {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no such edge");
    }
    return criticalGepInsideSCC;
}

/*!
 * Move outgoing direct edges of a sub node which is outside SCC to its rep node
 * Remove outgoing direct edges of a sub node which is inside SCC from its rep node
 */
bool ConstraintGraph::moveOutEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep )
{

    std::vector<ConstraintEdge*> sccEdges;
    std::vector<ConstraintEdge*> nonSccEdges;

    for (ConstraintNode::const_iterator it = node->OutEdgeBegin(), eit = node->OutEdgeEnd(); it != eit;
            ++it)
    {
        ConstraintEdge* subOutEdge = *it;
        if(sccRepNode(subOutEdge->getDstID()) != rep->getId())
            nonSccEdges.push_back(subOutEdge);
        else
        {
            sccEdges.push_back(subOutEdge);
        }
    }
    // if this edge is outside scc, then re-target edge src to rep
    while(!nonSccEdges.empty())
    {
        ConstraintEdge* edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetSrcOfEdge(edge,rep);
    }
    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty())
    {
        ConstraintEdge* edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(SVFUtil::isa<CopyCGEdge>(edge))
            removeDirectEdge(edge);
        else if (SVFUtil::isa<GepCGEdge>(edge))
        {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge))
            {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(SVFUtil::isa<LoadCGEdge>(edge) || SVFUtil::isa<StoreCGEdge>(edge))
            reTargetSrcOfEdge(edge,rep);
        else if(AddrCGEdge* addr = SVFUtil::dyn_cast<AddrCGEdge>(edge))
        {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no such edge");
    }
    return criticalGepInsideSCC;
}


/*!
 * Dump constraint graph
 */
void ConstraintGraph::dump(std::string name)
{
    GraphPrinter::WriteGraphToFile(outs(), name, this);
}

/*!
 * Print this constraint graph including its nodes and edges
 */
void ConstraintGraph::print()
{

    outs() << "-----------------ConstraintGraph--------------------------------------\n";

    ConstraintEdge::ConstraintEdgeSetTy& addrs = this->getAddrCGEdges();
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = addrs.begin(),
            eiter = addrs.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
               << "\n";
    }

    ConstraintEdge::ConstraintEdgeSetTy& directs = this->getDirectCGEdges();
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = directs.begin(),
            eiter = directs.end(); iter != eiter; ++iter)
    {
        if (CopyCGEdge* copy = SVFUtil::dyn_cast<CopyCGEdge>(*iter))
        {
            outs() << copy->getSrcID() << " -- Copy --> " << copy->getDstID()
                   << "\n";
        }
        else if (NormalGepCGEdge* ngep = SVFUtil::dyn_cast<NormalGepCGEdge>(*iter))
        {
            outs() << ngep->getSrcID() << " -- NormalGep (" << ngep->getConstantFieldIdx()
                   << ") --> " << ngep->getDstID() << "\n";
        }
        else if (VariantGepCGEdge* vgep = SVFUtil::dyn_cast<VariantGepCGEdge>(*iter))
        {
            outs() << vgep->getSrcID() << " -- VarintGep --> "
                   << vgep->getDstID() << "\n";
        }
        else
            assert(false && "wrong constraint edge kind!");
    }

    ConstraintEdge::ConstraintEdgeSetTy& loads = this->getLoadCGEdges();
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = loads.begin(),
            eiter = loads.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
               << "\n";
    }

    ConstraintEdge::ConstraintEdgeSetTy& stores = this->getStoreCGEdges();
    for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = stores.begin(),
            eiter = stores.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Store --> " << (*iter)->getDstID()
               << "\n";
    }

    outs()
            << "--------------------------------------------------------------\n";

}

/*!
 * View dot graph of Constraint graph from debugger.
 */
void ConstraintGraph::view()
{
    llvm::ViewGraph(this, "Constraint Graph");
}

/*!
 * GraphTraits specialization for constraint graph
 */
namespace llvm
{
template<>
struct DOTGraphTraits<ConstraintGraph*> : public DOTGraphTraits<SVFIR*>
{

    typedef ConstraintNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVFIR*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(ConstraintGraph*)
    {
        return "ConstraintG";
    }

#if LLVM_VERSION_MAJOR >= 12
    static bool isNodeHidden(NodeType *n, ConstraintGraph *)
    {
#else
    static bool isNodeHidden(NodeType *n)
    {
#endif
        if (Options::ShowHiddenNode) return false;
        else return (n->getInEdges().empty() && n->getOutEdges().empty());
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(NodeType *n, ConstraintGraph*)
    {
        PAGNode* node = SVFIR::getPAG()->getGNode(n->getId());
        bool briefDisplay = Options::BriefConsCGDotGraph;
        bool nameDisplay = true;
        std::string str;
        raw_string_ostream rawstr(str);

        if (briefDisplay)
        {
            if (SVFUtil::isa<ValVar>(node))
            {
                if (nameDisplay)
                    rawstr << node->getId() << ":" << node->getValueName();
                else
                    rawstr << node->getId();
            }
            else
                rawstr << node->getId();
        }
        else
        {
            // print the whole value
            if (!SVFUtil::isa<DummyValVar>(node) && !SVFUtil::isa<DummyObjVar>(node))
                rawstr << node->getId() << ":" << value2String(node->getValue());
            else
                rawstr << node->getId() << ":";

        }

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *n, ConstraintGraph*)
    {
        PAGNode* node = SVFIR::getPAG()->getGNode(n->getId());
        if (SVFUtil::isa<ValVar>(node))
        {
            if(SVFUtil::isa<GepValVar>(node))
                return "shape=hexagon";
            else if (SVFUtil::isa<DummyValVar>(node))
                return "shape=diamond";
            else
                return "shape=box";
        }
        else if (SVFUtil::isa<ObjVar>(node))
        {
            if(SVFUtil::isa<GepObjVar>(node))
                return "shape=doubleoctagon";
            else if(SVFUtil::isa<FIObjVar>(node))
                return "shape=box3d";
            else if (SVFUtil::isa<DummyObjVar>(node))
                return "shape=tab";
            else
                return "shape=component";
        }
        else if (SVFUtil::isa<RetPN>(node))
        {
            return "shape=Mrecord";
        }
        else if (SVFUtil::isa<VarArgPN>(node))
        {
            return "shape=octagon";
        }
        else
        {
            assert(0 && "no such kind!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, ConstraintGraph*)
    {
        ConstraintEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (edge->getEdgeKind() == ConstraintEdge::Addr)
        {
            return "color=green";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Copy)
        {
            return "color=black";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::NormalGep
                 || edge->getEdgeKind() == ConstraintEdge::VariantGep)
        {
            return "color=purple";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Store)
        {
            return "color=blue";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Load)
        {
            return "color=red";
        }
        else
        {
            assert(0 && "No such kind edge!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter)
    {
        return "";
    }
};
} // End namespace llvm
