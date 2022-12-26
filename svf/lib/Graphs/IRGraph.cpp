//===- SVFIR.cpp -- Program assignment graph------------------------------------//
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
 * SVFIR.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/IRGraph.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

IRGraph::~IRGraph()
{
    SymbolTableInfo::releaseSymbolInfo();
    symInfo = nullptr;
}

/*!
 * Add a SVFIR edge into edge map
 */
bool IRGraph::addEdge(SVFVar* src, SVFVar* dst, SVFStmt* edge)
{

    DBOUT(DPAGBuild,
          outs() << "add edge from " << src->getId() << " kind :"
          << src->getNodeKind() << " to " << dst->getId()
          << " kind :" << dst->getNodeKind() << "\n");
    src->addOutEdge(edge);
    dst->addInEdge(edge);
    return true;
}

/*!
 * Return true if it is an intra-procedural edge
 */
SVFStmt* IRGraph::hasNonlabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind)
{
    SVFStmt edge(src,dst,kind, false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Return an MultiOpndStmt if found
 */
SVFStmt* IRGraph::hasLabeledEdge(SVFVar* src, SVFVar* op1, SVFStmt::PEDGEK kind, const SVFVar* op2)
{
    SVFStmt edge(src,op1,SVFStmt::makeEdgeFlagWithAddionalOpnd(kind,op2), false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Return an inter-procedural edge if found
 */
SVFStmt* IRGraph::hasLabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind, const ICFGNode* callInst)
{
    SVFStmt edge(src,dst,SVFStmt::makeEdgeFlagWithCallInst(kind,callInst), false);
    SVFStmt::SVFStmtSetTy::iterator it = KindToSVFStmtSetMap[kind].find(&edge);
    if (it != KindToSVFStmtSetMap[kind].end())
    {
        return *it;
    }
    return nullptr;
}

/*!
 * Dump this IRGraph
 */
void IRGraph::dump(std::string name)
{
    GraphPrinter::WriteGraphToFile(outs(), name, this);
}

/*!
 * View IRGraph
 */
void IRGraph::view()
{
    SVF::ViewGraph(this, "ProgramAssignmentGraph");
}


namespace SVF
{
/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<IRGraph*> : public DefaultDOTGraphTraits
{

    typedef SVFVar NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(IRGraph *graph)
    {
        return graph->getGraphName();
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
    static bool isNodeHidden(SVFVar *node, IRGraph *)
    {
        if (Options::ShowHiddenNode()) return false;
        else return node->isIsolatedNode();
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(SVFVar *node, IRGraph*)
    {
        std::string str;
        std::stringstream rawstr(str);
        // print function info
        if (node->getFunction())
            rawstr << "[" << node->getFunction()->getName() << "] ";

        rawstr << node->toString();

        return rawstr.str();

    }

    static std::string getNodeAttributes(SVFVar *node, IRGraph*)
    {
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
    static std::string getEdgeAttributes(SVFVar*, EdgeIter EI, IRGraph*)
    {
        const SVFStmt* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<AddrStmt>(edge))
        {
            return "color=green";
        }
        else if (SVFUtil::isa<CopyStmt>(edge))
        {
            return "color=black";
        }
        else if (SVFUtil::isa<GepStmt>(edge))
        {
            return "color=purple";
        }
        else if (SVFUtil::isa<StoreStmt>(edge))
        {
            return "color=blue";
        }
        else if (SVFUtil::isa<LoadStmt>(edge))
        {
            return "color=red";
        }
        else if (SVFUtil::isa<PhiStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<SelectStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<CmpStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<BinaryOPStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<UnaryOPStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<BranchStmt>(edge))
        {
            return "color=grey";
        }
        else if (SVFUtil::isa<TDForkPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<TDJoinPE>(edge))
        {
            return "color=Turquoise";
        }
        else if (SVFUtil::isa<CallPE>(edge))
        {
            return "color=black,style=dashed";
        }
        else if (SVFUtil::isa<RetPE>(edge))
        {
            return "color=black,style=dotted";
        }

        assert(false && "No such kind edge!!");
        exit(1);
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(SVFVar*, EdgeIter EI)
    {
        const SVFStmt* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if(const CallPE* calledge = SVFUtil::dyn_cast<CallPE>(edge))
        {
            const SVFInstruction* callInst= calledge->getCallSite()->getCallSite();
            return callInst->getSourceLoc();
        }
        else if(const RetPE* retedge = SVFUtil::dyn_cast<RetPE>(edge))
        {
            const SVFInstruction* callInst= retedge->getCallSite()->getCallSite();
            return callInst->getSourceLoc();
        }
        return "";
    }
};
} // End namespace llvm
