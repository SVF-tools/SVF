//===- SVFStatements.cpp -- SVF program statements----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SVFStatements.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/SVFStatements.h"
#include "MemoryModel/SVFIR.h"
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
SVFStmt::SVFStmt(SVFVar* s, SVFVar* d, GEdgeFlag k) :
    GenericPAGEdgeTy(s,d,k),value(nullptr),basicBlock(nullptr),icfgNode(nullptr)
{
    edgeId = SVFIR::getPAG()->getTotalEdgeNum();
    SVFIR::getPAG()->incEdgeNum();
}

/*!
 * Whether src and dst nodes are both pointer type
 */
bool SVFStmt::isPTAEdge() const
{
    return getSrcNode()->isPointer() && getDstNode()->isPointer();
}

const std::string SVFStmt::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "SVFStmt: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string AddrStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "AddrStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CopyStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CopyStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string PhiStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "PhiStmt: [" << getResID() << "<--(" << getOpVarID(0) << "," << getOpVarID(1) << ")]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CmpStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CmpStmt: [" << getResID() << "<--(" << getOpVarID(0) << "," << getOpVarID(1) << ")]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string BinaryOPStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BinaryOPStmt: [" << getResID() << "<--(" << getOpVarID(0) << "," << getOpVarID(1) << ")]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string UnaryOPStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "UnaryOPStmt: [" << getResID() << "<--" << getOpVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string BranchStmt::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    if(isConditional())
        rawstr << "BranchStmt: [" <<  getCondition()->toString() << "]\t";
    else
        rawstr << "BranchStmt: [" <<  " Unconditional branch" << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}


const std::string LoadStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "LoadStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string StoreStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "StoreStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string GepStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string NormalGepStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "NormalGepStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string VariantGepStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VariantGepStmt: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string CallPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallPE: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string RetPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetPE: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string TDForkPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "TDForkPE: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
    return rawstr.str();
}

const std::string TDJoinPE::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "TDJoinPE: [" << getLHSVarID() << "<--" << getRHSVarID() << "]\t";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(getValue());
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

NodeID UnaryOPStmt::getOpVarID() const{
    return getOpVar()->getId();
}
NodeID UnaryOPStmt::getResID() const{
    return getRes()->getId();
}


/// The branch is unconditional if cond is a null value
bool BranchStmt::isUnconditional() const {
    return cond->getId() == SymbolTableInfo::SymbolInfo()->nullPtrSymID();
}
/// The branch is conditional if cond is not a null value
bool BranchStmt::isConditional() const{
    return cond->getId() != SymbolTableInfo::SymbolInfo()->nullPtrSymID();;
}
/// Return the condition 
const SVFVar* BranchStmt::getCondition() const 
{
    //assert(isConditional() && "this is a unconditional branch"); 
    return cond;
}