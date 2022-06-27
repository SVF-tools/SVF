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

#include <Util/Options.h>
#include "SVF-FE/LLVMUtil.h"
#include "Util/SVFModule.h"
#include "Graphs/ICFG.h"
#include "MemoryModel/SVFIR.h"
#include "Graphs/PTACallGraph.h"

using namespace SVF;
using namespace SVFUtil;


FunEntryICFGNode::FunEntryICFGNode(NodeID id, const SVFFunction* f) : InterICFGNode(id, FunEntryBlock)
{
    fun = f;
    // if function is implemented
    if (f->getLLVMFun()->begin() != f->getLLVMFun()->end())
    {
        bb = &(f->getLLVMFun()->getEntryBlock());
    }
}

FunExitICFGNode::FunExitICFGNode(NodeID id, const SVFFunction* f) : InterICFGNode(id, FunExitBlock), formalRet(nullptr)
{
    fun = f;
    // if function is implemented
    if (f->getLLVMFun()->begin() != f->getLLVMFun()->end())
    {
        bb = LLVMUtil::getFunExitBB(f->getLLVMFun());
    }

}


const std::string ICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ICFGNode" << getId();
    return rawstr.str();
}

void ICFGNode::dump() const
{
    outs() << this->toString() << "\n";
}

const std::string GlobalICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GlobalICFGNode" << getId();
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}


const std::string IntraICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << getSourceLoc(getInst()) << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    if(getSVFStmts().empty())
        rawstr << "\n" << value2String(getInst());
    return rawstr.str();
}


const std::string FunEntryICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FunEntryICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName();
    if (isExtCall(getFun())==false)
        rawstr << getSourceLoc(getFun()->getLLVMFun());
    rawstr << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}

const std::string FunExitICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FunExitICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName();
    if (isExtCall(getFun())==false)
        rawstr << getSourceLoc(LLVMUtil::getFunExitBB(getFun()->getLLVMFun())->getFirstNonPHI());
    rawstr << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}


const std::string CallICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << getSourceLoc(getCallSite()) << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}

const std::string RetICFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << getSourceLoc(getCallSite()) << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}

const std::string ICFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ICFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string IntraCFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    if(getCondition() == nullptr)
        rawstr << "IntraCFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t";
    else
        rawstr << "IntraCFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "] (branchCondition:" << *getCondition() << ") (succCondValue: " << getSuccessorCondValue() << ") \t";

    return rawstr.str();
}

const std::string CallCFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallCFGEdge " << " [ICFGNode";
    rawstr << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t CallSite: " << *cs << "\t";
    return rawstr.str();
}

const std::string RetCFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetCFGEdge " << " [ICFGNode";
    rawstr << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t CallSite: " << *cs << "\t";
    return rawstr.str();
}

/*!
 * Constructor
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(): totalICFGNode(0)
{
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG ...\n"));
    globalBlockNode = new GlobalICFGNode(totalICFGNode++);
    addICFGNode(globalBlockNode);
}

ICFG::~ICFG()
{
    Set<const SVFLoop*> loops;
    for (const auto &it: icfgNodeToSVFLoopVec)
    {
        for (const auto &loop: it.second)
        {
            loops.insert(loop);
        }
    }

    for (const auto &it: loops)
    {
        delete it;
    }
    icfgNodeToSVFLoopVec.clear();
}

/// Get a basic block ICFGNode
ICFGNode* ICFG::getICFGNode(const Instruction* inst)
{
    ICFGNode* node;
    if(SVFUtil::isNonInstricCallSite(inst))
        node = getCallICFGNode(inst);
    else if(SVFUtil::isIntrinsicInst(inst))
        node = getIntraICFGNode(inst);
//			assert (false && "associating an intrinsic instruction with an ICFGNode!");
    else
        node = getIntraICFGNode(inst);

    assert (node!=nullptr && "no ICFGNode for this instruction?");
    return node;
}


CallICFGNode* ICFG::getCallICFGNode(const Instruction* inst)
{
    if(SVFUtil::isCallSite(inst) ==false)
        outs() << SVFUtil::value2String(inst) << "\n";
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    CallICFGNode* node = getCallBlock(inst);
    if(node==nullptr)
        node = addCallBlock(inst);
    assert (node!=nullptr && "no CallICFGNode for this instruction?");
    return node;
}

RetICFGNode* ICFG::getRetICFGNode(const Instruction* inst)
{
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    RetICFGNode* node = getRetBlock(inst);
    if(node==nullptr)
        node = addRetBlock(inst);
    assert (node!=nullptr && "no RetICFGNode for this instruction?");
    return node;
}

IntraICFGNode* ICFG::getIntraICFGNode(const Instruction* inst)
{
    IntraICFGNode* node = getIntraBlock(inst);
    if(node==nullptr)
        node = addIntraBlock(inst);
    return node;
}

/// Add a function entry node
FunEntryICFGNode* ICFG::getFunEntryICFGNode(const SVFFunction*  fun)
{
    FunEntryICFGNode* b = getFunEntryBlock(fun);
    if (b == nullptr)
        return addFunEntryBlock(fun);
    else
        return b;
}
/// Add a function exit node
FunExitICFGNode* ICFG::getFunExitICFGNode(const SVFFunction*  fun)
{
    FunExitICFGNode* b = getFunExitBlock(fun);
    if (b == nullptr)
        return addFunExitBlock(fun);
    else
        return b;
}

/*!
 * Whether we has an intra ICFG edge
 */
ICFGEdge* ICFG::hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * Whether we has an inter ICFG edge
 */
ICFGEdge* ICFG::hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst, kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * Whether we has an thread ICFG edge
 */
ICFGEdge* ICFG::hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}


/*!
 * Return the corresponding ICFGEdge
 */
ICFGEdge* ICFG::getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind)
{

    ICFGEdge * edge = nullptr;
    u32_t counter = 0;
    for (ICFGEdge::ICFGEdgeSetTy::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getEdgeKind() == kind)
        {
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
ICFGEdge* ICFG::addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode)
{
    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF))
    {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return nullptr;
    }
    else
    {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        return (addICFGEdge(intraEdge) ? intraEdge : nullptr);
    }
}

/*!
 * Add conditional intraprocedural edges between two nodes
 */
ICFGEdge* ICFG::addConditionalIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Value* condition, s32_t branchCondVal)
{

    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF))
    {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return nullptr;
    }
    else
    {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        intraEdge->setBranchCondition(condition,branchCondVal);
        return (addICFGEdge(intraEdge) ? intraEdge : nullptr);
    }
}


/*!
 * Add interprocedural call edges between two nodes
 */
ICFGEdge* ICFG::addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction*  cs)
{
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::CallCF))
    {
        assert(edge->isCallCFGEdge() && "this should be a call CFG edge!");
        return nullptr;
    }
    else
    {
        CallCFGEdge* callEdge = new CallCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(callEdge) ? callEdge : nullptr);
    }
}

/*!
 * Add interprocedural return edges between two nodes
 */
ICFGEdge* ICFG::addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, const Instruction*  cs)
{
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::RetCF))
    {
        assert(edge->isRetCFGEdge() && "this should be a return CFG edge!");
        return nullptr;
    }
    else
    {
        RetCFGEdge* retEdge = new RetCFGEdge(srcNode,dstNode,cs);
        return (addICFGEdge(retEdge) ? retEdge : nullptr);
    }
}


/*!
 * Dump ICFG
 */
void ICFG::dump(const std::string& file, bool simple)
{
    GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
}

/*!
 * View ICFG
 */
void ICFG::view()
{
    llvm::ViewGraph(this, "Interprocedural Control-Flow Graph");
}

/*!
 * Update ICFG for indirect calls
 */
void ICFG::updateCallGraph(PTACallGraph* callgraph)
{
    PTACallGraph::CallEdgeMap::const_iterator iter = callgraph->getIndCallMap().begin();
    PTACallGraph::CallEdgeMap::const_iterator eiter = callgraph->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallICFGNode* callBlock = iter->first;
        const Instruction* cs = callBlock->getCallSite();
        assert(callBlock->isIndirectCall() && "this is not an indirect call?");
        const PTACallGraph::FunctionSet & functions = iter->second;
        for (PTACallGraph::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction*  callee = *func_iter;
            CallICFGNode* callBlockNode = getCallICFGNode(cs);
            RetICFGNode* retBlockNode = getRetICFGNode(cs);
            /// if this is an external function (no function body), connect calleeEntryNode to calleeExitNode
            if (isExtCall(callee))
                addIntraEdge(callBlockNode, retBlockNode);
            else
            {
                FunEntryICFGNode* calleeEntryNode = getFunEntryBlock(callee);
                FunExitICFGNode* calleeExitNode = getFunExitBlock(callee);
                if(ICFGEdge* callEdge = addCallEdge(callBlockNode, calleeEntryNode, cs))
                {
                    for (const SVFStmt *stmt : callBlockNode->getSVFStmts())
                    {
                        if(const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
                        {
                            if(callPE->getFunEntryICFGNode() == calleeEntryNode)
                                SVFUtil::cast<CallCFGEdge>(callEdge)->addCallPE(callPE);
                        }
                    }
                }
                if(ICFGEdge* retEdge = addRetEdge(calleeExitNode, retBlockNode, cs))
                {
                    for (const SVFStmt *stmt : retBlockNode->getSVFStmts())
                    {
                        if(const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
                        {
                            if(retPE->getFunExitICFGNode() == calleeExitNode)
                                SVFUtil::cast<RetCFGEdge>(retEdge)->addRetPE(retPE);
                        }
                    }
                }

                /// Remove callBlockNode to retBlockNode intraICFGEdge since we found at least one inter procedural edge
                if(ICFGEdge* edge = hasIntraICFGEdge(callBlockNode,retBlockNode, ICFGEdge::IntraCF))
                    removeICFGEdge(edge);
            }

        }
    }
    // dump ICFG
    if (Options::DumpICFG)
    {
        dump("icfg_final");
    }
}

/*!
 * GraphTraits specialization
 */
namespace llvm
{
template<>
struct DOTGraphTraits<ICFG*> : public DOTGraphTraits<SVFIR*>
{

    typedef ICFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVFIR*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(ICFG*)
    {
        return "ICFG";
    }

    std::string getNodeLabel(NodeType *node, ICFG *graph)
    {
        return getSimpleNodeLabel(node, graph);
    }

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, ICFG*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);

        if(SVFUtil::isa<IntraICFGNode>(node))
        {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<FunEntryICFGNode>(node))
        {
            rawstr <<  "color=yellow";
        }
        else if(SVFUtil::isa<FunExitICFGNode>(node))
        {
            rawstr <<  "color=green";
        }
        else if(SVFUtil::isa<CallICFGNode>(node))
        {
            rawstr <<  "color=red";
        }
        else if(SVFUtil::isa<RetICFGNode>(node))
        {
            rawstr <<  "color=blue";
        }
        else if(SVFUtil::isa<GlobalICFGNode>(node))
        {
            rawstr <<  "color=purple";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, ICFG*)
    {
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
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getCallSite();
        else if (RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge))
            rawstr << dirRet->getCallSite();

        return rawstr.str();
    }
};
} // End namespace llvm
