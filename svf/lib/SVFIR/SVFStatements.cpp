//===- SVFStatements.cpp -- SVF program statements----------------------//
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
 * SVFStatements.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "SVFIR/GraphDBClient.h"

using namespace SVF;
using namespace SVFUtil;


u64_t SVFStmt::callEdgeLabelCounter = 0;
u64_t SVFStmt::storeEdgeLabelCounter = 0;
u64_t SVFStmt::multiOpndLabelCounter = 0;
SVFStmt::Inst2LabelMap SVFStmt::inst2LabelMap;
SVFStmt::Var2LabelMap SVFStmt::var2LabelMap;


/*!
 * SVFStmt constructor
 */
SVFStmt::SVFStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, bool real) :
    GenericPAGEdgeTy(s,d,k),value(nullptr),basicBlock(nullptr),icfgNode(nullptr),edgeId(UINT_MAX)
{
    if(real)
    {
        edgeId = SVFIR::getPAG()->getTotalEdgeNum();
        SVFIR::getPAG()->incEdgeNum();
    }
}

SVFStmt::SVFStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, ICFGNode* icfgNode, bool real) :
GenericPAGEdgeTy(s,d,k),value(value),basicBlock(nullptr),icfgNode(icfgNode),edgeId(eid)
{
    if(real)
    {
        edgeId = eid;
        SVFIR::getPAG()->incEdgeNum();
    }
}

/*!
 * Whether src and dst nodes are both pointer type
 */
bool SVFStmt::isPTAEdge() const
{
    return getSrcNode()->isPointer() && getDstNode()->isPointer();
}

const std::string SVFStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "SVFStmt: [Var" << getDstID() << " <-- Var" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string AddrStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "AddrStmt: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string CopyStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "CopyStmt: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string PhiStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "PhiStmt: [Var" << getResID() << " <-- (";
    for(u32_t i = 0; i < getOpVarNum(); i++)
        rawstr << "[Var" << getOpVar(i)->getId() << ", ICFGNode" << getOpICFGNode(i)->getId() <<  "],";
    rawstr << ")]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string SelectStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "SelectStmt: (Condition Var" <<  getCondition()->getId() << ") [Var" << getResID() << " <-- (Var";
    for(const SVFVar* op : getOpndVars())
        rawstr << op->getId() << ",";
    rawstr << ")]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string CmpStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "CmpStmt: [Var" << getResID() << " <-- (Var" << getOpVarID(0) << " predicate" << getPredicate() << " Var" << getOpVarID(1) << ")]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string BinaryOPStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "BinaryOPStmt: [Var" << getResID() << " <-- (Var" << getOpVarID(0) << " opcode" << getOpcode() << " Var" << getOpVarID(1) << ")]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string UnaryOPStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "UnaryOPStmt: [Var" << getResID() << " <-- " << " opcode" << getOpcode() << " Var" << getOpVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string BranchStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    if(isConditional())
        rawstr << "BranchStmt: [Condition Var" <<  getCondition()->getId() << "]\n";
    else
        rawstr << "BranchStmt: [" <<  " Unconditional branch" << "]\n";

    for(u32_t i = 0; i < getNumSuccessors(); i++)
        rawstr << "Successor " << i << " ICFGNode" << getSuccessor(i)->getId() << "   ";

    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}


const std::string LoadStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "LoadStmt: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string StoreStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "StoreStmt: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string GepStmt::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "GepStmt: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string CallPE::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "CallPE: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string RetPE::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "RetPE: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string TDForkPE::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "TDForkPE: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}

const std::string TDJoinPE::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "TDJoinPE: [Var" << getLHSVarID() << " <-- Var" << getRHSVarID() << "]\t";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getValue()->toString();
    }
    return rawstr.str();
}


NodeID MultiOpndStmt::getOpVarID(u32_t pos) const
{
    return getOpVar(pos)->getId();
}

NodeID MultiOpndStmt::getResID() const
{
    return getRes()->getId();
}

NodeID UnaryOPStmt::getOpVarID() const
{
    return getOpVar()->getId();
}
NodeID UnaryOPStmt::getResID() const
{
    return getRes()->getId();
}

/// Return true if this is a phi at the function exit
/// to receive one or multiple return values of this function
bool PhiStmt::isFunctionRetPhi() const
{
    return SVFUtil::isa<RetValPN>(getRes());
}


/// The branch is unconditional if cond is a null value
bool BranchStmt::isUnconditional() const
{
    return cond->getId() == PAG::getPAG()->nullPtrSymID();
}
/// The branch is conditional if cond is not a null value
bool BranchStmt::isConditional() const
{
    return cond->getId() != PAG::getPAG()->nullPtrSymID();;
}
/// Return the condition
const SVFVar* BranchStmt::getCondition() const
{
    //assert(isConditional() && "this is a unconditional branch");
    return cond;
}

StoreStmt::StoreStmt(SVFVar* s, SVFVar* d, const ICFGNode* st)
    : AssignStmt(s, d, makeEdgeFlagWithStoreInst(SVFStmt::Store, st))
{
}
StoreStmt::StoreStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, ICFGNode* icfgNode)
: AssignStmt(s, d, k, eid, value, icfgNode) {}

CallPE::CallPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
               const FunEntryICFGNode* e, GEdgeKind k)
    : AssignStmt(s, d, makeEdgeFlagWithCallInst(k, i)), call(i), entry(e)
{
}

CallPE::CallPE(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, ICFGNode* icfgNode, const CallICFGNode* call,
    const FunEntryICFGNode* entry): AssignStmt(s, d, k, eid, value, icfgNode), call(call), entry(entry) 
{

}

RetPE::RetPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
             const FunExitICFGNode* e, GEdgeKind k)
    : AssignStmt(s, d, makeEdgeFlagWithCallInst(k, i)), call(i), exit(e)
{
}

RetPE::RetPE(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, ICFGNode* icfgNode, const CallICFGNode* call,
    const FunExitICFGNode* exit): AssignStmt(s, d, k, eid, value, icfgNode), call(call), exit(exit) {}

MultiOpndStmt::MultiOpndStmt(SVFVar* r, const OPVars& opnds, GEdgeFlag k)
    : SVFStmt(opnds.at(0), r, k), opVars(opnds)
{
}

CmpStmt::CmpStmt(SVFVar* s, const OPVars& opnds, u32_t pre)
    : MultiOpndStmt(s, opnds,
                    makeEdgeFlagWithAddionalOpnd(SVFStmt::Cmp, opnds.at(1))),
      predicate(pre)
{
    assert(opnds.size() == 2 && "CmpStmt can only have two operands!");
}

CmpStmt::CmpStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, u32_t predicate, ICFGNode* icfgNode, const OPVars& opnds)
: MultiOpndStmt(s, d, k, eid, value, icfgNode, opnds), predicate(predicate) 
{
    assert(opnds.size() == 2 && "CmpStmt can only have two operands!");
}

SelectStmt::SelectStmt(SVFVar* s, const OPVars& opnds, const SVFVar* cond)
    : MultiOpndStmt(s, opnds,
                    makeEdgeFlagWithAddionalOpnd(SVFStmt::Select, opnds.at(1))),
      condition(cond)
{
    assert(opnds.size() == 2 && "SelectStmt can only have two operands!");
}

SelectStmt::SelectStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, SVFVar* condition, ICFGNode* icfgNode, const OPVars& opnds)
    : MultiOpndStmt(s, d, k, eid, value, icfgNode, opnds), condition(condition) 
{
    assert(opnds.size() == 2 && "SelectStmt can only have two operands!");
}

BinaryOPStmt::BinaryOPStmt(SVFVar* s, const OPVars& opnds, u32_t oc)
    : MultiOpndStmt(
          s, opnds,
          makeEdgeFlagWithAddionalOpnd(SVFStmt::BinaryOp, opnds.at(1))),
      opcode(oc)
{
    assert(opnds.size() == 2 && "BinaryOPStmt can only have two operands!");
}

BinaryOPStmt::BinaryOPStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, EdgeID eid, SVFVar* value, u32_t opcode, ICFGNode* icfgNode, const OPVars& opnds)
    : MultiOpndStmt(s, d, k, eid, value, icfgNode, opnds), opcode(opcode) 
{
    assert(opnds.size() == 2 && "BinaryOPStmt can only have two operands!");
}

std::string SVFStmt::generateSVFStmtEdgeFieldsStmt() const
{
    std::string valueStr = "";
    if (nullptr != getValue())
    {
        valueStr += ", svf_var_node_id:"+ std::to_string(getValue()->getId());
    }
    else
    {
        valueStr += ", svf_var_node_id:-1";
    }
    std::string bb_id_str = "";
    if (nullptr != getBB())
    {
        bb_id_str += ", bb_id:'" + std::to_string(getBB()->getParent()->getId()) + ":" + std::to_string(getBB()->getId())+"'";
    }
    else 
    {
        bb_id_str += ", bb_id:''";
    }

    std::string icfg_node_id_str = "";
    if (nullptr != getICFGNode())
    {
        icfg_node_id_str += ", icfg_node_id:" + std::to_string(getICFGNode()->getId());
    }
    else 
    {
        icfg_node_id_str += ", icfg_node_id:-1";
    }

    std::string inst2_label_map = "";
    if (nullptr != getInst2LabelMap() && !getInst2LabelMap()->empty())
    {
        inst2_label_map += ", inst2_label_map:'"+ GraphDBClient::getInstance().extractLabelMap2String(getInst2LabelMap()) +"'";
    }

    std::string var2_label_map = "";
    if (nullptr != getVar2LabelMap() && !getVar2LabelMap()->empty())
    {
        var2_label_map += ", var2_label_map:'"+ GraphDBClient::getInstance().extractLabelMap2String(getVar2LabelMap()) +"'";
    }
    std::string fieldsStr = "";
    fieldsStr += "edge_id: " + std::to_string(getEdgeID()) + 
    valueStr +
    bb_id_str +
    icfg_node_id_str +
    inst2_label_map +
    var2_label_map +
    ", call_edge_label_counter:" + std::to_string(*(getCallEdgeLabelCounter())) +
    ", store_edge_label_counter:" + std::to_string(*(getStoreEdgeLabelCounter())) +
    ", multi_opnd_label_counter:" + std::to_string(*(getMultiOpndLabelCounter())) +
    ", edge_flag:" + std::to_string(getEdgeKindWithoutMask());
    return fieldsStr;
}

std::string SVFStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getSrcNode());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(getDstNode()->getId()) +
        " CREATE (n)-[r:SVFStmt{"+
        generateSVFStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string AddrStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:AddrStmt{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", arr_size:'" + GraphDBClient::getInstance().extractNodesIds(getArrSize()) +"'"+
        "}]->(m)";
    return queryStatement;
}

std::string CopyStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:CopyStmt{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", copy_kind:" + std::to_string(getCopyKind()) +
        "}]->(m)";
    return queryStatement;  
}

std::string StoreStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:StoreStmt{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string LoadStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:LoadStmt{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GepStmt::toDBString() const
{
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    std::ostringstream accessPathStr;
    accessPathStr << "";
    if (!isVariantFieldGep())
    {
        accessPathStr << ", ap_fld_idx:"
                      << std::to_string(getConstantStructFldIdx());
    }
    else
    {
        accessPathStr << ", ap_fld_idx:-1";
    }

    if (nullptr != getAccessPath().gepSrcPointeeType())
    {
        accessPathStr << ", ap_gep_pointee_type_name:'"
                      << getAccessPath().gepSrcPointeeType()->toString()
                      << "'";
    }
    if (!getAccessPath().getIdxOperandPairVec().empty())
    {
        accessPathStr << ", ap_idx_operand_pairs:'"
                      << GraphDBClient::getInstance().IdxOperandPairsToString(
                             &(getAccessPath().getIdxOperandPairVec()))
                      << "'";
    }

    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:GepStmt{" + generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        accessPathStr.str() +
        ", variant_field:" + (isVariantFieldGep()? "true" : "false") +
        "}]->(m)";
    return queryStatement;
}

std::string CallPE::toDBString() const
{
    std::string callInstStr = "";
    std::string funEntryICFGNodeStr = "";
    if (nullptr != getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(getCallInst()->getId());
    }
    else
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(getFunEntryICFGNode()->getId());
    }
    else 
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:-1";
    }
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:CallPE{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        callInstStr +
        funEntryICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string RetPE::toDBString() const
{
    std::string callInstStr = "";
    std::string funExitICFGNodeStr = "";
    if (nullptr != getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(getCallInst()->getId());
    }
    else 
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(getFunExitICFGNode()->getId());
    }
    else 
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:-1";
    }
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:RetPE{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        callInstStr +
        funExitICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string TDForkPE::toDBString() const
{
    std::string callInstStr = "";
    std::string funEntryICFGNodeStr = "";
    if (nullptr != getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(getCallInst()->getId());
    }
    else 
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(getFunEntryICFGNode()->getId());
    }
    else 
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:-1";
    }
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:TDForkPE{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        callInstStr +
        funEntryICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string TDJoinPE::toDBString() const
{
    std::string callInstStr = "";
    std::string funExitICFGNodeStr = "";
    if (nullptr != getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(getCallInst()->getId());
    }
    else
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(getFunExitICFGNode()->getId());
    }
    else 
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:-1";
    }
    std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:TDJoinPE{"+
        generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        callInstStr +
        funExitICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string MultiOpndStmt::generateMultiOpndStmtEdgeFieldsStmt() const
{
    std::string stmt = generateSVFStmtEdgeFieldsStmt();
    if (!getOpndVars().empty())
    {
        stmt += ", op_var_node_ids:'" + GraphDBClient::getInstance().extractNodesIds(getOpndVars())+"'";
    }
    else 
    {
        stmt += ", op_var_node_ids:''";
    }
    return stmt;
}

std::string MultiOpndStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:MultiOpndStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string PhiStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:PhiStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", op_icfg_nodes_ids:'" + GraphDBClient::getInstance().extractNodesIds(*(getOpICFGNodeVec())) + "'"+
        "}]->(m)";
    return queryStatement;
}

std::string SelectStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
       "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:SelectStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", condition_svf_var_node_id:" + std::to_string(getCondition()->getId()) + 
        "}]->(m)";
    return queryStatement;
}

std::string CmpStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:CmpStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", predicate:" + std::to_string(getPredicate()) + 
        "}]->(m)";
    return queryStatement;
}

std::string BinaryOPStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
       "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:BinaryOPStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", op_code:" + std::to_string(getOpcode()) + 
        "}]->(m)";
    return queryStatement;
}

std::string UnaryOPStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:UnaryOPStmt{"+
        generateSVFStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", op_code:" + std::to_string(getOpcode()) + 
        "}]->(m)";
    return queryStatement;
}

std::string BranchStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getSrcNode());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(SVFStmt::getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(SVFStmt::getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(SVFStmt::getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(SVFStmt::getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(SVFStmt::getDstNode()->getId()) +
        " CREATE (n)-[r:BranchStmt{"+
        generateSVFStmtEdgeFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) +
        ", successors:'" + GraphDBClient::getInstance().extractSuccessorsPairSet2String(&(getSuccessors())) + "'"+ 
        ", condition_svf_var_node_id:" + std::to_string(getCondition()->getId()) +
        ", br_inst_svf_var_node_id:" + std::to_string(getBranchInst()->getId()) +
        "}]->(m)";
    return queryStatement;
}

std::string AssignStmt::toDBString() const
{
    const std::string srcKind = GraphDBClient::getInstance().getPAGNodeKindString(getRHSVar());
    const std::string dstKind = GraphDBClient::getInstance().getPAGNodeKindString(getLHSVar());
    const std::string queryStatement =
        "MATCH (n:" + srcKind + "{id:" + std::to_string(getRHSVar()->getId()) +
        "}), (m:" + dstKind + "{id:" + std::to_string(getLHSVar()->getId()) +
        "}) WHERE n.id = " + std::to_string(getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(getLHSVar()->getId()) +
        " CREATE (n)-[r:AssignStmt{" + generateAssignStmtFieldsStmt() +
        ", kind:" + std::to_string(getEdgeKind()) + "}]->(m)";
    return queryStatement;
}
