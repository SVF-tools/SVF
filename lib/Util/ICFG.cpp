//===- ICFG.cpp -- Sparse value-flow graph-----------------------------------//
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
 * ICFG.cpp
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei Sui
 */

#include "Util/ICFG.h"
#include "Util/SVFUtil.h"
#include "Util/SVFModule.h"

using namespace SVFUtil;


static llvm::cl::opt<bool> DumpICFG("dump-icfg", llvm::cl::init(false),
                             llvm::cl::desc("Dump dot graph of ICFG"));

static llvm::cl::opt<bool> DumpLLVMInst("dump-inst", llvm::cl::init(false),
                             llvm::cl::desc("Dump LLVM instruction for each ICFG Node"));

/*!
 * Constructor
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(): totalICFGNode(0) {
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG ...\n"));
}


/*!
 * Whether we has an intra ICFG edge
 */
ICFGEdge* ICFG::hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Whether we has an inter ICFG edge
 */
ICFGEdge* ICFG::hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {
    ICFGEdge edge(src,dst, kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Whether we has an thread ICFG edge
 */
ICFGEdge* ICFG::hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}


/*!
 * Return the corresponding ICFGEdge
 */
ICFGEdge* ICFG::getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {

    ICFGEdge * edge = NULL;
    Size_t counter = 0;
    for (ICFGEdge::ICFGEdgeSetTy::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter) {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getEdgeKind() == kind) {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two ICFG nodes");
    return edge;

}

/*!
 * Add intraprocedural edges between two nodes
 */
ICFGEdge* ICFG::addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode){
    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF)) {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return NULL;
    }
    else {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        return (addICFGEdge(intraEdge) ? intraEdge : NULL);
    }
}

/*!
 * Add interprocedural call edges between two nodes
 */
ICFGEdge* ICFG::addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSite cs) {
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::CallCF)) {
        assert(edge->isCallCFGEdge() && "this should be a call CFG edge!");
        return NULL;
    }
    else {
        CallCFGEdge* callEdge = new CallCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*!
 * Add interprocedural return edges between two nodes
 */
ICFGEdge* ICFG::addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSite cs) {
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::RetCF)) {
        assert(edge->isRetCFGEdge() && "this should be a return CFG edge!");
        return NULL;
    }
    else {
        RetCFGEdge* retEdge = new RetCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(retEdge) ? retEdge : NULL);
    }
}

void ICFG::printICFGToJson(const std::string& filename){

    std::string str;
    raw_string_ostream rawstr(str);
	for(ICFG::iterator it = begin(), eit = end(); it!=eit; ++it){
		ICFGNode* node = it->second;
		if (IntraBlockNode* bNode = SVFUtil::dyn_cast<IntraBlockNode>(node)) {
			rawstr << getSourceLoc(bNode->getInst()) << "\n";

			IntraBlockNode::StmtOrPHIVec& edges = bNode->getPAGEdges();
			for (IntraBlockNode::StmtOrPHIVec::iterator it = edges.begin(),
					eit = edges.end(); it != eit; ++it) {
				const PAGEdge* edge = *it;
				NodeID src = edge->getSrcID();
				NodeID dst = edge->getDstID();
				rawstr << dst << "<--" << src << "\n";
				std::string srcValueName = edge->getSrcNode()->getValueName();
				std::string dstValueName = edge->getDstNode()->getValueName();
				rawstr << dstValueName << "<--" << srcValueName << "\n";

			}
		} else if (FunEntryBlockNode* entry = SVFUtil::dyn_cast<
				FunEntryBlockNode>(node)) {
			if (isExtCall(entry->getFun()))
				rawstr << "Entry(" << ")\n";
			else
				rawstr << "Entry(" << getSourceLoc(entry->getFun()) << ")\n";
			rawstr << "Fun[" << entry->getFun()->getName() << "]";
		} else if (FunExitBlockNode* exit = SVFUtil::dyn_cast<FunExitBlockNode>(
				node)) {
			if (isExtCall(exit->getFun()))
				rawstr << "Exit(" << ")\n";
			else
				rawstr << "Exit(" << getSourceLoc(&(exit->getBB()->back()))
						<< ")\n";
			rawstr << "Fun[" << exit->getFun()->getName() << "]";
		} else if (CallBlockNode* call = SVFUtil::dyn_cast<CallBlockNode>(
				node)) {
			rawstr << "Call("
					<< getSourceLoc(call->getCallSite().getInstruction())
					<< ")\n";
		} else if (RetBlockNode* ret = SVFUtil::dyn_cast<RetBlockNode>(node)) {
			rawstr << "Ret("
					<< getSourceLoc(ret->getCallSite().getInstruction())
					<< ")\n";
		} else
			assert(false && "what else kinds of nodes do we have??");

		for(ICFGNode::iterator sit = node->OutEdgeBegin(), esit = node->OutEdgeEnd(); sit!=esit; ++sit){
			ICFGEdge* edge = *sit;
			if (CallCFGEdge* call = SVFUtil::dyn_cast<CallCFGEdge>(edge))
				rawstr << "call edge " << call->getCallSite().getInstruction();
			else if (SVFUtil::isa<RetCFGEdge>(edge))
				rawstr << "return edge " << call->getCallSite().getInstruction();
			else
				rawstr << "intra edge";
		}
	}


}

/*!
 * Dump ICFG
 */
void ICFG::dump(const std::string& file, bool simple) {
    if(DumpICFG)
        GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
}


/*!
 * GraphTraits specialization
 */
namespace llvm {
template<>
struct DOTGraphTraits<ICFG*> : public DOTGraphTraits<PAG*> {

    typedef ICFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(ICFG *graph) {
        return "ICFG";
    }

    std::string getNodeLabel(NodeType *node, ICFG *graph) {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
		if (IntraBlockNode* bNode = SVFUtil::dyn_cast<IntraBlockNode>(node)) {
			rawstr << getSourceLoc(bNode->getInst()) << "\n";

			IntraBlockNode::StmtOrPHIVec& edges = bNode->getPAGEdges();
			for (IntraBlockNode::StmtOrPHIVec::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it){
			    const PAGEdge* edge = *it;
			        NodeID src = edge->getSrcID();
			        NodeID dst = edge->getDstID();
			        rawstr << dst << "<--" << src << "\n";
			        std::string srcValueName = edge->getSrcNode()->getValueName();
			        std::string dstValueName = edge->getDstNode()->getValueName();
			        rawstr << dstValueName << "<--" << srcValueName << "\n";

			}

			if(DumpLLVMInst)
				rawstr << *(bNode->getInst()) << "\n";
		} else if (FunEntryBlockNode* entry = SVFUtil::dyn_cast<FunEntryBlockNode>(node)) {
			if (isExtCall(entry->getFun()))
				rawstr << "Entry(" << ")\n";
			else
				rawstr << "Entry(" << getSourceLoc(entry->getFun()) << ")\n";
			rawstr << "Fun[" << entry->getFun()->getName() << "]";
		} else if (FunExitBlockNode* exit = SVFUtil::dyn_cast<FunExitBlockNode>(node)) {
			if (isExtCall(exit->getFun()))
				rawstr << "Exit(" << ")\n";
			else
				rawstr << "Exit(" << getSourceLoc(&(exit->getBB()->back()))
						<< ")\n";
			rawstr << "Fun[" << exit->getFun()->getName() << "]";
		} else if (CallBlockNode* call = SVFUtil::dyn_cast<CallBlockNode>(node)) {
			rawstr << "Call("
					<< getSourceLoc(call->getCallSite().getInstruction())
					<< ")\n";
		} else if (RetBlockNode* ret = SVFUtil::dyn_cast<RetBlockNode>(node)) {
			rawstr << "Ret("
					<< getSourceLoc(ret->getCallSite().getInstruction())
					<< ")\n";
		}
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);

        if(SVFUtil::isa<IntraBlockNode>(node)) {
        		rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<FunEntryBlockNode>(node)) {
            rawstr <<  "color=yellow";
        }
        else if(SVFUtil::isa<FunExitBlockNode>(node)) {
            rawstr <<  "color=green";
        }
        else if(SVFUtil::isa<CallBlockNode>(node)) {
            rawstr <<  "color=red";
        }
        else if(SVFUtil::isa<RetBlockNode>(node)) {
            rawstr <<  "color=blue";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, ICFG *pag) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
		if (SVFUtil::isa<CallCFGEdge>(edge))
			return "style=solid,color=red";
		else if (SVFUtil::isa<RetCFGEdge>(edge))
			return "style=solid,color=blue";
		else
			return "style=solid";
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getCallSite().getInstruction();
        else if (RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge))
            rawstr << dirRet->getCallSite().getInstruction();

        return rawstr.str();
    }
};
}
