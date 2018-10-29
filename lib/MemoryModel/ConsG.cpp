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

#include "MemoryModel/ConsG.h"
#include "Util/AnalysisUtil.h"
#include "Util/GraphUtil.h"

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> ConsCGDotGraph("dump-consG", cl::init(false),
                                    cl::desc("Dump dot graph of Constraint Graph"));
static cl::opt<bool> PrintCGGraph("print-consG", cl::init(false),
                                    cl::desc("Print Constraint Graph to Terminal"));

/*!
 * Start building constraint graph
 */
void ConstraintGraph::buildCG() {

    // initialize nodes
    for(PAG::iterator it = pag->begin(), eit = pag->end(); it!=eit; ++it) {
        addConstraintNode(new ConstraintNode(it->first),it->first);
    }

    // initialize edges
    PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addAddrCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& calls = pag->getEdgeSet(PAGEdge::Call);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = calls.begin(), eiter =
                calls.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& rets = pag->getEdgeSet(PAGEdge::Ret);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = rets.begin(), eiter =
                rets.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& tdfks = pag->getEdgeSet(PAGEdge::ThreadFork);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = tdfks.begin(), eiter =
                tdfks.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& tdjns = pag->getEdgeSet(PAGEdge::ThreadJoin);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = tdjns.begin(), eiter =
                tdjns.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addCopyCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter) {
        NormalGepPE* edge = cast<NormalGepPE>(*iter);
        addNormalGepCGEdge(edge->getSrcID(),edge->getDstID(),edge->getLocationSet());
    }

    PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
                vgeps.end(); iter != eiter; ++iter) {
        VariantGepPE* edge = cast<VariantGepPE>(*iter);
        addVariantGepCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Load);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addLoadCGEdge(edge->getSrcID(),edge->getDstID());
    }

    PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter) {
        PAGEdge* edge = *iter;
        addStoreCGEdge(edge->getSrcID(),edge->getDstID());
    }
}


/*!
 * Memory has been cleaned up at GenericGraph
 */
void ConstraintGraph::destroy() {
}

/*!
 * Constructor for address constraint graph edge
 */
AddrCGEdge::AddrCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id)
    : ConstraintEdge(s,d,Addr,id) {
    PAGNode* node = PAG::getPAG()->getPAGNode(s->getId());
	if (!SVFModule::pagReadFromTXT())
		assert(!llvm::isa<DummyValPN>(node) && "a dummy node??");
}

/*!
 * Add an address edge
 */
bool ConstraintGraph::addAddrCGEdge(NodeID src, NodeID dst) {
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Addr))
        return false;
    AddrCGEdge* edge = new AddrCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = AddrCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingAddrEdge(edge);
    dstNode->addIncomingAddrEdge(edge);
    return added;
}

/*!
 * Add Copy edge
 */
bool ConstraintGraph::addCopyCGEdge(NodeID src, NodeID dst) {

    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Copy)
            || srcNode == dstNode)
        return false;

    CopyCGEdge* edge = new CopyCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingCopyEdge(edge);
    dstNode->addIncomingCopyEdge(edge);
    return added;
}


/*!
 * Add Gep edge
 */
bool ConstraintGraph::addNormalGepCGEdge(NodeID src, NodeID dst, const LocationSet& ls) {
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::NormalGep))
        return false;

    NormalGepCGEdge* edge = new NormalGepCGEdge(srcNode, dstNode,ls, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingGepEdge(edge);
    dstNode->addIncomingGepEdge(edge);
    return added;
}

/*!
 * Add variant gep edge
 */
bool ConstraintGraph::addVariantGepCGEdge(NodeID src, NodeID dst) {
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::VariantGep))
        return false;

    VariantGepCGEdge* edge = new VariantGepCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = directEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingGepEdge(edge);
    dstNode->addIncomingGepEdge(edge);
    return added;
}

/*!
 * Add Load edge
 */
bool ConstraintGraph::addLoadCGEdge(NodeID src, NodeID dst) {
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Load))
        return false;

    LoadCGEdge* edge = new LoadCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = LoadCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingLoadEdge(edge);
    dstNode->addIncomingLoadEdge(edge);
    return added;
}

/*!
 * Add Store edge
 */
bool ConstraintGraph::addStoreCGEdge(NodeID src, NodeID dst) {
    ConstraintNode* srcNode = getConstraintNode(src);
    ConstraintNode* dstNode = getConstraintNode(dst);
    if(hasEdge(srcNode,dstNode,ConstraintEdge::Store))
        return false;

    StoreCGEdge* edge = new StoreCGEdge(srcNode, dstNode, edgeIndex++);
    bool added = StoreCGEdgeSet.insert(edge).second;
    assert(added && "not added??");
    srcNode->addOutgoingStoreEdge(edge);
    dstNode->addIncomingStoreEdge(edge);
    return added;
}


/*!
 * Re-target dst node of an edge
 *
 * (1) Remove edge from old dst target,
 * (2) Change edge dst id and
 * (3) Add modifed edge into new dst
 */
void ConstraintGraph::reTargetDstOfEdge(ConstraintEdge* edge, ConstraintNode* newDstNode) {
    NodeID newDstNodeID = newDstNode->getId();
    NodeID srcId = edge->getSrcID();
    if(LoadCGEdge* load = dyn_cast<LoadCGEdge>(edge)) {
        removeLoadEdge(load);
        addLoadCGEdge(srcId,newDstNodeID);
    }
    else if(StoreCGEdge* store = dyn_cast<StoreCGEdge>(edge)) {
        removeStoreEdge(store);
        addStoreCGEdge(srcId,newDstNodeID);
    }
    else if(CopyCGEdge* copy = dyn_cast<CopyCGEdge>(edge)) {
        removeDirectEdge(copy);
        addCopyCGEdge(srcId,newDstNodeID);
    }
    else if(NormalGepCGEdge* gep = dyn_cast<NormalGepCGEdge>(edge)) {
        const LocationSet ls = gep->getLocationSet();
        removeDirectEdge(gep);
        addNormalGepCGEdge(srcId,newDstNodeID,ls);
    }
    else if(VariantGepCGEdge* gep = dyn_cast<VariantGepCGEdge>(edge)) {
        removeDirectEdge(gep);
        addVariantGepCGEdge(srcId,newDstNodeID);
    }
    /// Address edge is removed directly, because it won't participate in solving further
    /// To be noted: it can not retarget the address edge to newSrc, otherwise it might lead
    /// non object node flows to points-to set of a pointer (src of the edge maybe non object node after scc)
    else if(AddrCGEdge* addr = dyn_cast<AddrCGEdge>(edge)) {
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
void ConstraintGraph::reTargetSrcOfEdge(ConstraintEdge* edge, ConstraintNode* newSrcNode) {
    NodeID newSrcNodeID = newSrcNode->getId();
    NodeID dstId = edge->getDstID();
    if(LoadCGEdge* load = dyn_cast<LoadCGEdge>(edge)) {
        removeLoadEdge(load);
        addLoadCGEdge(newSrcNodeID,dstId);
    }
    else if(StoreCGEdge* store = dyn_cast<StoreCGEdge>(edge)) {
        removeStoreEdge(store);
        addStoreCGEdge(newSrcNodeID,dstId);
    }
    else if(CopyCGEdge* copy = dyn_cast<CopyCGEdge>(edge)) {
        removeDirectEdge(copy);
        addCopyCGEdge(newSrcNodeID,dstId);
    }
    else if(NormalGepCGEdge* gep = dyn_cast<NormalGepCGEdge>(edge)) {
        const LocationSet ls = gep->getLocationSet();
        removeDirectEdge(gep);
        addNormalGepCGEdge(newSrcNodeID,dstId,ls);
    }
    else if(VariantGepCGEdge* gep = dyn_cast<VariantGepCGEdge>(edge)) {
        removeDirectEdge(gep);
        addVariantGepCGEdge(newSrcNodeID,dstId);
    }
    /// Address edge is removed directly, because it won't participate in solving further
    /// To be noted: it can not retarget the address edge to newSrc, otherwise it might lead
    /// non object node flows to points-to set of a pointer (src of the edge maybe non object node after scc)
    else if(AddrCGEdge* addr = dyn_cast<AddrCGEdge>(edge)) {
        removeAddrEdge(addr);
    }
    else
        assert(false && "no other edge type!!");
}

/*!
 * Remove addr edge from their src and dst edge sets
 */
void ConstraintGraph::removeAddrEdge(AddrCGEdge* edge) {
    getConstraintNode(edge->getSrcID())->removeOutgoingAddrEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingAddrEdge(edge);
    Size_t num = AddrCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove load edge from their src and dst edge sets
 */
void ConstraintGraph::removeLoadEdge(LoadCGEdge* edge) {
    getConstraintNode(edge->getSrcID())->removeOutgoingLoadEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingLoadEdge(edge);
    Size_t num = LoadCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove store edge from their src and dst edge sets
 */
void ConstraintGraph::removeStoreEdge(StoreCGEdge* edge) {
    getConstraintNode(edge->getSrcID())->removeOutgoingStoreEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingStoreEdge(edge);
    Size_t num = StoreCGEdgeSet.erase(edge);
    delete edge;
    assert(num && "edge not in the set, can not remove!!!");
}

/*!
 * Remove edges from their src and dst edge sets
 */
void ConstraintGraph::removeDirectEdge(ConstraintEdge* edge) {

    getConstraintNode(edge->getSrcID())->removeOutgoingDirectEdge(edge);
    getConstraintNode(edge->getDstID())->removeIncomingDirectEdge(edge);
    Size_t num = directEdgeSet.erase(edge);

    assert(num && "edge not in the set, can not remove!!!");
    delete edge;
}

/*!
 * Move incoming direct edges of a sub node which is outside SCC to its rep node
 * Remove incoming direct edges of a sub node which is inside SCC from its rep node
 */
bool ConstraintGraph::moveInEdgesToRepNode(ConstraintNode* node, ConstraintNode* rep ) {
    std::vector<ConstraintEdge*> sccEdges;
    std::vector<ConstraintEdge*> nonSccEdges;
    for (ConstraintNode::const_iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit;
            ++it) {
        ConstraintEdge* subInEdge = *it;
        if(sccRepNode(subInEdge->getSrcID()) != rep->getId())
            nonSccEdges.push_back(subInEdge);
        else {
            sccEdges.push_back(subInEdge);
        }
    }
    // if this edge is outside scc, then re-target edge dst to rep
    while(!nonSccEdges.empty()) {
        ConstraintEdge* edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetDstOfEdge(edge,rep);
    }

    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty()) {
        ConstraintEdge* edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(isa<CopyCGEdge>(edge))
            removeDirectEdge(edge);
        else if (isa<GepCGEdge>(edge)) {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge)) {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(isa<LoadCGEdge>(edge) || isa<StoreCGEdge>(edge))
            reTargetDstOfEdge(edge,rep);
        else if(AddrCGEdge* addr = dyn_cast<AddrCGEdge>(edge)) {
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
bool ConstraintGraph::moveOutEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep ) {

    std::vector<ConstraintEdge*> sccEdges;
    std::vector<ConstraintEdge*> nonSccEdges;

    for (ConstraintNode::const_iterator it = node->OutEdgeBegin(), eit = node->OutEdgeEnd(); it != eit;
            ++it) {
        ConstraintEdge* subOutEdge = *it;
        if(sccRepNode(subOutEdge->getDstID()) != rep->getId())
            nonSccEdges.push_back(subOutEdge);
        else {
            sccEdges.push_back(subOutEdge);
        }
    }
    // if this edge is outside scc, then re-target edge src to rep
    while(!nonSccEdges.empty()) {
        ConstraintEdge* edge = nonSccEdges.back();
        nonSccEdges.pop_back();
        reTargetSrcOfEdge(edge,rep);
    }
    bool criticalGepInsideSCC = false;
    // if this edge is inside scc, then remove this edge and two end nodes
    while(!sccEdges.empty()) {
        ConstraintEdge* edge = sccEdges.back();
        sccEdges.pop_back();
        /// only copy and gep edge can be removed
        if(isa<CopyCGEdge>(edge))
            removeDirectEdge(edge);
        else if (isa<GepCGEdge>(edge)) {
            // If the GEP is critical (i.e. may have a non-zero offset),
            // then it brings impact on field-sensitivity.
            if (!isZeroOffsettedGepCGEdge(edge)) {
                criticalGepInsideSCC = true;
            }
            removeDirectEdge(edge);
        }
        else if(isa<LoadCGEdge>(edge) || isa<StoreCGEdge>(edge))
            reTargetSrcOfEdge(edge,rep);
        else if(AddrCGEdge* addr = dyn_cast<AddrCGEdge>(edge)) {
            removeAddrEdge(addr);
        }
        else
            assert(false && "no such edge");
    }
    return criticalGepInsideSCC;
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */
void ConstraintGraph::connectCaller2CalleeParams(llvm::CallSite cs, const llvm::Function *F,
        NodePairSet& cpySrcNodes) {

    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite " << *cs.getInstruction() << " to callee " << *F << "\n");

    if (pag->funHasRet(F) && pag->callsiteHasRet(cs)) {
        const PAGNode* cs_return = pag->getCallSiteRet(cs);
        const PAGNode* fun_return = pag->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer()) {
            NodeID dstrec = sccRepNode(cs_return->getId());
            NodeID srcret = sccRepNode(fun_return->getId());
            if(addCopyCGEdge(srcret, dstrec)) {
                cpySrcNodes.insert(std::make_pair(srcret,dstrec));
            }
        }
        else {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (pag->hasCallSiteArgsMap(cs) && pag->hasFunArgsMap(F)) {

        // connect actual and formal param
        const PAG::PAGNodeList& csArgList = pag->getCallSiteArgsList(cs);
        const PAG::PAGNodeList& funArgList = pag->getFunArgsList(F);
        //Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        PAG::PAGNodeList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        PAG::PAGNodeList::const_iterator csArgIt  = csArgList.begin(), csArgEit = csArgList.end();
        for (; funArgIt != funArgEit; ++csArgIt, ++funArgIt) {
            //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
            if (csArgIt  == csArgEit) {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode *cs_arg = *csArgIt ;
            const PAGNode *fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer()) {
                DBOUT(DAndersen, outs() << "process actual parm  " << *(cs_arg->getValue()) << " \n");
                NodeID srcAA = sccRepNode(cs_arg->getId());
                NodeID dstFA = sccRepNode(fun_arg->getId());
                if(addCopyCGEdge(srcAA, dstFA)) {
                    cpySrcNodes.insert(std::make_pair(srcAA,dstFA));
                }
            }
        }

        //Any remaining actual args must be varargs.
        if (F->isVarArg()) {
            NodeID vaF = sccRepNode(getVarargNode(F));
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt) {
                const PAGNode *cs_arg = *csArgIt;
                if (cs_arg->isPointer()) {
                    NodeID vnAA = sccRepNode(cs_arg->getId());
                    if (addCopyCGEdge(vnAA,vaF)) {
                        cpySrcNodes.insert(std::make_pair(vnAA,vaF));
                    }
                }
            }
        }
        if(csArgIt != csArgEit) {
            wrnMsg("too many args to non-vararg func.");
            wrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");
        }
    }
}

/*!
 * Dump constraint graph
 */
void ConstraintGraph::dump() {
    if(ConsCGDotGraph)
        GraphPrinter::WriteGraphToFile(llvm::outs(), "consCG_final", this);
}

/*!
 * Print this constraint graph including its nodes and edges
 */
void ConstraintGraph::print() {

	if (!PrintCGGraph)
		return;

	outs() << "-----------------ConstraintGraph--------------------------------------\n";

	ConstraintEdge::ConstraintEdgeSetTy& addrs = this->getAddrCGEdges();
	for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = addrs.begin(),
			eiter = addrs.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
				<< "\n";
	}

	ConstraintEdge::ConstraintEdgeSetTy& directs = this->getDirectCGEdges();
	for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = directs.begin(),
			eiter = directs.end(); iter != eiter; ++iter) {
		if (CopyCGEdge* copy = dyn_cast<CopyCGEdge>(*iter)) {
			outs() << copy->getSrcID() << " -- Copy --> " << copy->getDstID()
					<< "\n";
		} else if (NormalGepCGEdge* ngep = dyn_cast<NormalGepCGEdge>(*iter)) {
			outs() << ngep->getSrcID() << " -- NormalGep (" << ngep->getOffset()
					<< ") --> " << ngep->getDstID() << "\n";
		} else if (VariantGepCGEdge* vgep = dyn_cast<VariantGepCGEdge>(*iter)) {
			outs() << ngep->getSrcID() << " -- VarintGep --> "
					<< ngep->getDstID() << "\n";
		} else
			assert(false && "wrong constraint edge kind!");
	}

	ConstraintEdge::ConstraintEdgeSetTy& loads = this->getLoadCGEdges();
	for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = loads.begin(),
			eiter = loads.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
				<< "\n";
	}

	ConstraintEdge::ConstraintEdgeSetTy& stores = this->getStoreCGEdges();
	for (ConstraintEdge::ConstraintEdgeSetTy::iterator iter = stores.begin(),
			eiter = stores.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Store --> " << (*iter)->getDstID()
				<< "\n";
	}

	outs()
			<< "--------------------------------------------------------------\n";

}

/*!
 * GraphTraits specialization for constraint graph
 */
namespace llvm {
template<>
struct DOTGraphTraits<ConstraintGraph*> : public DOTGraphTraits<PAG*> {

    typedef ConstraintNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(ConstraintGraph *graph) {
        return "ConstraintG";
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(NodeType *n, ConstraintGraph *graph) {
        PAGNode* node = PAG::getPAG()->getPAGNode(n->getId());
        bool briefDisplay = true;
        bool nameDisplay = true;
        std::string str;
        raw_string_ostream rawstr(str);

        if (briefDisplay) {
            if (isa<ValPN>(node)) {
                if (nameDisplay)
                    rawstr << node->getId() << ":" << node->getValueName();
                else
                    rawstr << node->getId();
            } else
                rawstr << node->getId();
        } else {
            // print the whole value
            if (!isa<DummyValPN>(node) && !isa<DummyObjPN>(node))
                rawstr << *node->getValue();
            else
                rawstr << "";

        }

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *n, ConstraintGraph *graph) {
        PAGNode* node = PAG::getPAG()->getPAGNode(n->getId());

        if (isa<ValPN>(node)) {
            if(isa<GepValPN>(node))
                return "shape=hexagon";
            else if (isa<DummyValPN>(node))
                return "shape=diamond";
            else
                return "shape=circle";
        } else if (isa<ObjPN>(node)) {
            if(isa<GepObjPN>(node))
                return "shape=doubleoctagon";
            else if(isa<FIObjPN>(node))
                return "shape=septagon";
            else if (isa<DummyObjPN>(node))
                return "shape=Mcircle";
            else
                return "shape=doublecircle";
        } else if (isa<RetPN>(node)) {
            return "shape=Mrecord";
        } else if (isa<VarArgPN>(node)) {
            return "shape=octagon";
        } else {
            assert(0 && "no such kind node!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, ConstraintGraph *pag) {
        ConstraintEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (edge->getEdgeKind() == ConstraintEdge::Addr) {
            return "color=green";
        } else if (edge->getEdgeKind() == ConstraintEdge::Copy) {
            return "color=black";
        } else if (edge->getEdgeKind() == ConstraintEdge::NormalGep
                   || edge->getEdgeKind() == ConstraintEdge::VariantGep) {
            return "color=purple";
        } else if (edge->getEdgeKind() == ConstraintEdge::Store) {
            return "color=blue";
        } else if (edge->getEdgeKind() == ConstraintEdge::Load) {
            return "color=red";
        } else {
            assert(0 && "No such kind edge!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
        return "";
    }
};
}
