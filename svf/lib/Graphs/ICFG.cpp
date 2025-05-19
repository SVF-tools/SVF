//===- ICFG.cpp -- Sparse value-flow graph-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFG.cpp
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei Sui
 */

#include "Graphs/ICFG.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFIR.h"
#include <Util/Options.h>
#include "SVFIR/GraphDBClient.h"

using namespace SVF;
using namespace SVFUtil;


FunEntryICFGNode::FunEntryICFGNode(NodeID id, const FunObjVar* f) : InterICFGNode(id, FunEntryBlock)
{
    fun = f;
    // if function is implemented
    if (f->begin() != f->end())
    {
        bb = f->getEntryBlock();
    }
}

std::string FunEntryICFGNode::toDBString() const
{
    const std::string queryStatement ="CREATE (n:FunEntryICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) +
    ", fun_obj_var_id:" + std::to_string(getFun()->getId()) + 
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'" +
    ", bb_id:" + std::to_string(getBB()->getId()) +
    ", fp_nodes:'" + GraphDBClient::getInstance().extractNodesIds(getFormalParms()) +"'})";
    return queryStatement;
}

FunExitICFGNode::FunExitICFGNode(NodeID id, const FunObjVar* f)
    : InterICFGNode(id, FunExitBlock), formalRet(nullptr)
{
    fun = f;
    // if function is implemented
    if (f->begin() != f->end())
    {
        bb = f->getExitBB();
    }
}

std::string FunExitICFGNode::toDBString() const
{
    std::string formalRetId = "";
    if (nullptr == getFormalRet())
    {
        formalRetId = ",formal_ret_node_id:-1";
    } else {
        formalRetId = ",formal_ret_node_id:" + std::to_string(getFormalRet()->getId());
    }
    const std::string queryStatement ="CREATE (n:FunExitICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) +
    ", fun_obj_var_id:" + std::to_string(getFun()->getId()) + 
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'" +
    ", bb_id:" + std::to_string(getBB()->getId()) +
    formalRetId + "})";
    return queryStatement;
}

const std::string ICFGNode::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
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
    std::stringstream rawstr(str);
    rawstr << "GlobalICFGNode" << getId();
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}


const std::string IntraICFGNode::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "IntraICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << getSourceLoc() << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    if(getSVFStmts().empty())
        rawstr << "\n" << valueOnlyToString();
    return rawstr.str();
}


const std::string FunEntryICFGNode::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "FunEntryICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName();
    if (isExtCall(getFun())==false)
        rawstr << getFun()->getSourceLoc();
    rawstr << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}

const std::string FunEntryICFGNode::getSourceLoc() const
{
    return "function entry: " + fun->getSourceLoc();
}

const std::string FunExitICFGNode::toString() const
{
    const FunObjVar *fun = getFun();
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "FunExitICFGNode" << getId();
    rawstr << " {fun: " << fun->getName();
    // ensure the enclosing function has exit basic block
    if (!isExtCall(fun) && fun->hasReturn())
        if(const IntraICFGNode* intraICFGNode = dyn_cast<IntraICFGNode>(fun->getExitBB()->front()))
            rawstr << intraICFGNode->getSourceLoc();
    rawstr << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    return rawstr.str();
}

const std::string FunExitICFGNode::getSourceLoc() const
{
    return "function ret: " + fun->getSourceLoc();
}

const std::string CallICFGNode::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "CallICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << ICFGNode::getSourceLoc() << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    if(getSVFStmts().empty())
        rawstr << "\n" << valueOnlyToString();
    return rawstr.str();
}

std::string CallICFGNode::toDBString() const
{
    std::string fun_name_of_v_call = "";
    std::string vtab_ptr_node_id = "";
    std::string virtual_fun_idx = "";
    std::string is_vir_call_inst = isVirtualCall() ? "true" : "false";
    std::string virtualFunAppendix = "";
    if (isVirtualCall())
    {
        fun_name_of_v_call = ", fun_name_of_v_call: '"+getFunNameOfVirtualCall()+"'";
        vtab_ptr_node_id = ", vtab_ptr_node_id:" + std::to_string(getVtablePtr()->getId());
        virtual_fun_idx = ", virtual_fun_idx:" + std::to_string(getFunIdxInVtable());
        virtualFunAppendix = vtab_ptr_node_id+virtual_fun_idx+fun_name_of_v_call;
    }
    else 
    {
        vtab_ptr_node_id = ", vtab_ptr_node_id:-1";
        virtual_fun_idx = ", virtual_fun_idx:-1";
        virtualFunAppendix = vtab_ptr_node_id+virtual_fun_idx;
    }
    std::string called_fun_obj_var_id = "";
    if (getCalledFunction() != nullptr)
    {
        called_fun_obj_var_id = ", called_fun_obj_var_id:" + std::to_string(getCalledFunction()->getId());
    }
    else 
    {
        called_fun_obj_var_id = ", called_fun_obj_var_id: -1";
    }
    std::string ret_icfg_node_id = "";
    if (getRetICFGNode() != nullptr)
    {
        ret_icfg_node_id = ", ret_icfg_node_id: " + std::to_string(getRetICFGNode()->getId());
    }
    else 
    {
        ret_icfg_node_id = ", ret_icfg_node_id: -1";
    }
    const std::string queryStatement ="CREATE (n:CallICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) +
    ret_icfg_node_id +
    ", bb_id: " + std::to_string(getBB()->getId()) +
    ", fun_obj_var_id: " + std::to_string(getFun()->getId()) +
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'" +
    ", svf_type:'" + getType()->toString() + "'" +
    ", ap_nodes:'" + GraphDBClient::getInstance().extractNodesIds(getActualParms()) +"'" +
    called_fun_obj_var_id +
    ", is_vararg: " + (isVarArg() ? "true" : "false") +
    ", is_vir_call_inst: " + (isVirtualCall() ? "true" : "false") +
    virtualFunAppendix+"})";
    return queryStatement;
}

const std::string RetICFGNode::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "RetICFGNode" << getId();
    rawstr << " {fun: " << getFun()->getName() << ICFGNode::getSourceLoc() << "}";
    for (const SVFStmt *stmt : getSVFStmts())
        rawstr << "\n" << stmt->toString();
    if(getSVFStmts().empty())
        rawstr << "\n" << valueOnlyToString();
    return rawstr.str();
}

std::string RetICFGNode::toDBString() const
{
    std::string actual_ret_node_id="";
    if (getActualRet() != nullptr)
    {
        actual_ret_node_id = ", actual_ret_node_id: " + std::to_string(getActualRet()->getId()) ;
    } else {
        actual_ret_node_id = ", actual_ret_node_id: -1";
    }
    const std::string queryStatement ="CREATE (n:RetICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) +
    actual_ret_node_id+
    ", call_block_node_id: " + std::to_string(getCallICFGNode()->getId()) +
    ", bb_id: " + std::to_string(getBB()->getId()) +
    ", fun_obj_var_id: " + std::to_string(getFun()->getId()) +
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'" +
    ", svf_type:'" + getType()->toString() + "'"+"})";
    return queryStatement;
}

const std::string ICFGEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ICFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string IntraCFGEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    if(getCondition() == nullptr)
        rawstr << "IntraCFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t";
    else
        rawstr << "IntraCFGEdge: [ICFGNode" << getDstID() << " <-- ICFGNode" << getSrcID() << "] (branchCondition:" << getCondition()->toString() << ") (succCondValue: " << getSuccessorCondValue() << ") \t";

    return rawstr.str();
}

std::string IntraCFGEdge::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getICFGNodeKindString(getSrcNode());
    std::string dstKind = GraphDBClient::getInstance().getICFGNodeKindString(getDstNode());
    std::string condition = "";
    if (getCondition() != nullptr)
    {
        condition =
            ", condition_var_id:" + std::to_string(getCondition()->getId()) +
            ", branch_cond_val:" + std::to_string(getSuccessorCondValue());
    }
    else
    {
        condition = ", condition_var_id:-1, branch_cond_val:-1";
    }
    const std::string queryStatement =
        "MATCH (n:" + srcKind + "{id:" + std::to_string(getSrcNode()->getId()) +
        "}), (m:" + dstKind + "{id:" + std::to_string(getDstNode()->getId()) +
        "}) WHERE n.id = " + std::to_string(getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(getDstNode()->getId()) +
        " CREATE (n)-[r:IntraCFGEdge{kind:" + std::to_string(getEdgeKind()) +
        condition + "}]->(m)";
    return queryStatement;
}

const std::string CallCFGEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "CallCFGEdge " << " [ICFGNode";
    rawstr << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t CallSite: " << getSrcNode()->toString() << "\t";
    return rawstr.str();
}

std::string CallCFGEdge::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getICFGNodeKindString(getSrcNode());
    std::string dstKind = GraphDBClient::getInstance().getICFGNodeKindString(getDstNode());
    const std::string queryStatement =
        "MATCH (n:" + srcKind + "{id:" + std::to_string(getSrcNode()->getId()) +
        "}), (m:" + dstKind + "{id:" + std::to_string(getDstNode()->getId()) +
        "}) WHERE n.id = " + std::to_string(getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(getDstNode()->getId()) +
        " CREATE (n)-[r:CallCFGEdge{kind:" + std::to_string(getEdgeKind()) +
        ", call_pe_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getCallPEs()) + "'}]->(m)";
    return queryStatement;
}

const std::string RetCFGEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "RetCFGEdge " << " [ICFGNode";
    rawstr << getDstID() << " <-- ICFGNode" << getSrcID() << "]\t CallSite: " << getDstNode()->toString() << "\t";
    return rawstr.str();
}

std::string RetCFGEdge::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getICFGNodeKindString(getSrcNode());
    std::string dstKind = GraphDBClient::getInstance().getICFGNodeKindString(getDstNode());
    std::string ret_pe_id = "";
    if (getRetPE() != nullptr)
    {
        ret_pe_id = ", ret_pe_id:" + std::to_string(getRetPE()->getEdgeID());
    }
    else
    {
        ret_pe_id = ", ret_pe_id:-1";
    }
    const std::string queryStatement =
        "MATCH (n:" + srcKind + "{id:" + std::to_string(getSrcNode()->getId()) +
        "}), (m:" + dstKind + "{id:" + std::to_string(getDstNode()->getId()) +
        "}) WHERE n.id = " + std::to_string(getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(getDstNode()->getId()) +
        " CREATE (n)-[r:RetCFGEdge{kind:" + std::to_string(getEdgeKind()) +
        ret_pe_id + "}]->(m)";
    return queryStatement;
}

/// Return call ICFGNode at the callsite
const CallICFGNode* RetCFGEdge::getCallSite() const
{
    assert(SVFUtil::isa<RetICFGNode>(getDstNode()) && "not a RetICFGNode?");
    return SVFUtil::cast<RetICFGNode>(getDstNode())->getCallICFGNode();
}

/*!
 * Constructor
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(): totalICFGNode(0), globalBlockNode(nullptr)
{
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


/// Add a function entry node
FunEntryICFGNode* ICFG::getFunEntryICFGNode(const FunObjVar*  fun)
{
    FunEntryICFGNode* entry = getFunEntryBlock(fun);
    assert (entry && "fun entry not created in ICFGBuilder?");
    return entry;
}
/// Add a function exit node
FunExitICFGNode* ICFG::getFunExitICFGNode(const FunObjVar*  fun)
{
    FunExitICFGNode* exit = getFunExitBlock(fun);
    assert (exit && "fun exit not created in ICFGBuilder?");
    return exit;
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
    ICFGEdge* edge = hasIntraICFGEdge(srcNode, dstNode, ICFGEdge::IntraCF);
    if (edge != nullptr)
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
ICFGEdge* ICFG::addConditionalIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode, s64_t branchCondVal)
{

    checkIntraEdgeParents(srcNode, dstNode);
    ICFGEdge* edge = hasIntraICFGEdge(srcNode, dstNode, ICFGEdge::IntraCF);
    if (edge != nullptr)
    {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return nullptr;
    }
    else
    {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        intraEdge->setBranchCondVal(branchCondVal);
        return (addICFGEdge(intraEdge) ? intraEdge : nullptr);
    }
}


/*!
 * Add interprocedural call edges between two nodes
 */
ICFGEdge* ICFG::addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode)
{
    ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::CallCF);
    if (edge != nullptr)
    {
        assert(edge->isCallCFGEdge() && "this should be a call CFG edge!");
        return nullptr;
    }
    else
    {
        CallCFGEdge* callEdge = new CallCFGEdge(srcNode,dstNode);
        return (addICFGEdge(callEdge) ? callEdge : nullptr);
    }
}

/*!
 * Add interprocedural return edges between two nodes
 */
ICFGEdge* ICFG::addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode)
{
    ICFGEdge* edge = hasInterICFGEdge(srcNode, dstNode, ICFGEdge::RetCF);
    if (edge != nullptr)
    {
        assert(edge->isRetCFGEdge() && "this should be a return CFG edge!");
        return nullptr;
    }
    else
    {
        RetCFGEdge* retEdge = new RetCFGEdge(srcNode,dstNode);
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
    SVF::ViewGraph(this, "Interprocedural Control-Flow Graph");
}

/*!
 * Update ICFG for indirect calls
 */
void ICFG::updateCallGraph(CallGraph* callgraph)
{
    CallGraph::CallEdgeMap::const_iterator iter = callgraph->getIndCallMap().begin();
    CallGraph::CallEdgeMap::const_iterator eiter = callgraph->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        CallICFGNode* callBlockNode = const_cast<CallICFGNode*>(iter->first);
        assert(callBlockNode->isIndirectCall() && "this is not an indirect call?");
        const CallGraph::FunctionSet & functions = iter->second;
        for (CallGraph::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const FunObjVar*  callee = *func_iter;
            RetICFGNode* retBlockNode = const_cast<RetICFGNode*>(callBlockNode->getRetICFGNode());
            /// if this is an external function (no function body), connect calleeEntryNode to calleeExitNode
            if (isExtCall(callee))
                addIntraEdge(callBlockNode, retBlockNode);
            else
            {
                FunEntryICFGNode* calleeEntryNode = getFunEntryBlock(callee);
                FunExitICFGNode* calleeExitNode = getFunExitBlock(callee);
                if(ICFGEdge* callEdge = addCallEdge(callBlockNode, calleeEntryNode))
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
                if(ICFGEdge* retEdge = addRetEdge(calleeExitNode, retBlockNode))
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
    if (Options::DumpICFG())
    {
        dump("icfg_final");
    }
}

std::string IntraICFGNode::toDBString() const
{
    const std::string queryStatement ="CREATE (n:IntraICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) +
    ", is_return: " + (isRetInst() ? "true" : "false") +
    ", fun_obj_var_id:" + std::to_string(getFun()->getId()) +
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'" +
    ", bb_id:" + std::to_string(getBB()->getId()) + "})";
    return queryStatement;
}

std::string InterICFGNode::toDBString() const{
    const std::string queryStatement ="CREATE (n:InterICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) + 
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'})";
    return queryStatement;
}

std::string GlobalICFGNode::toDBString() const
{
    const std::string queryStatement ="CREATE (n:GlobalICFGNode {id: " + std::to_string(getId()) +
    ", kind: " + std::to_string(getNodeKind()) + 
    ", pag_edge_ids:'" + GraphDBClient::getInstance().extractEdgesIds(getSVFStmts()) +"'})";
    return queryStatement;
}
/*!
 * GraphTraits specialization
 */
namespace SVF
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

    static bool isNodeHidden(ICFGNode *node, ICFG *)
    {
        if (Options::ShowHiddenNode())
            return false;
        else
            return node->getInEdges().empty() && node->getOutEdges().empty();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG*)
    {
        std::string str;
        std::stringstream rawstr(str);

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
        std::stringstream rawstr(str);
        if (CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getSrcNode();
        else if (RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge))
        {
            if(RetICFGNode* ret = SVFUtil::dyn_cast<RetICFGNode>(dirRet->getDstNode()))
                rawstr << ret->getCallICFGNode();
        }

        return rawstr.str();
    }
};
} // End namespace llvm
