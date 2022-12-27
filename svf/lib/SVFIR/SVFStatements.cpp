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
    return SVFUtil::isa<RetPN>(getRes());
}


/// The branch is unconditional if cond is a null value
bool BranchStmt::isUnconditional() const
{
    return cond->getId() == SymbolTableInfo::SymbolInfo()->nullPtrSymID();
}
/// The branch is conditional if cond is not a null value
bool BranchStmt::isConditional() const
{
    return cond->getId() != SymbolTableInfo::SymbolInfo()->nullPtrSymID();;
}
/// Return the condition
const SVFVar* BranchStmt::getCondition() const
{
    //assert(isConditional() && "this is a unconditional branch");
    return cond;
}

StoreStmt::StoreStmt(SVFVar* s, SVFVar* d, const IntraICFGNode* st)
    : AssignStmt(s, d, makeEdgeFlagWithStoreInst(SVFStmt::Store, st))
{
}

CallPE::CallPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
               const FunEntryICFGNode* e, GEdgeKind k)
    : AssignStmt(s, d, makeEdgeFlagWithCallInst(k, i)), call(i), entry(e)
{
}

RetPE::RetPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
             const FunExitICFGNode* e, GEdgeKind k)
    : AssignStmt(s, d, makeEdgeFlagWithCallInst(k, i)), call(i), exit(e)
{
}

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

SelectStmt::SelectStmt(SVFVar* s, const OPVars& opnds, const SVFVar* cond)
    : MultiOpndStmt(s, opnds,
                    makeEdgeFlagWithAddionalOpnd(SVFStmt::Select, opnds.at(1))),
      condition(cond)
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
