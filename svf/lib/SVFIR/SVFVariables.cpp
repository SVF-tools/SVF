//===- SVFVariables.cpp -- SVF symbols and variables----------------------//
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
 * SVFVariables.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "SVFIR/SVFVariables.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "Graphs/CallGraph.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * SVFVar constructor
 */
SVFVar::SVFVar(NodeID i, const SVFType* svfType, PNODEK k) :
    GenericPAGNodeTy(i,k, svfType)
{
}

bool SVFVar::ptrInUncalledFunction() const
{
    if (const FunObjVar* fun = getFunction())
    {
        return fun->isUncalledFunction();
    }
    else
    {
        return false;
    }
}

bool SVFVar::isIsolatedNode() const
{
    if (getInEdges().empty() && getOutEdges().empty())
        return true;
    else if (isConstDataOrAggDataButNotNullPtr())
        return true;
    else
        return false;
}


const std::string SVFVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "SVFVar ID: " << getId();
    return rawstr.str();
}

void SVFVar::dump() const
{
    outs() << this->toString() << "\n";
}

const FunObjVar* ValVar::getFunction() const
{
    if(icfgNode)
        return icfgNode->getFun();
    return nullptr;
}

const std::string ValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

ArgValVar::ArgValVar(NodeID i, u32_t argNo, const ICFGNode* icn,
                     const SVF::FunObjVar* callGraphNode, const SVFType* svfType)
    : ValVar(i, svfType, icn, ArgValNode),
      cgNode(callGraphNode), argNo(argNo)
{

}

const FunObjVar* ArgValVar::getFunction() const
{
    return getParent();
}

const FunObjVar* ArgValVar::getParent() const
{
    return cgNode;
}

bool ArgValVar::isArgOfUncalledFunction() const
{
    return getFunction()->isUncalledFunction();
}

bool ArgValVar::isPointer() const
{
    return cgNode->getArg(argNo)->getType()->isPointerTy();
}

const std::string ArgValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ArgValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

GepValVar::GepValVar(const ValVar* baseNode, NodeID i,
                     const AccessPath& ap, const SVFType* ty, const ICFGNode* node)
    : ValVar(i, ty, node, GepValNode), ap(ap), base(baseNode), gepValType(ty)
{

}

const std::string GepValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "GepValVar ID: " << getId() << " with offset_" + std::to_string(getConstantFieldIdx());
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getBaseNode()->valueOnlyToString();
    }
    return rawstr.str();
}

RetValPN::RetValPN(NodeID i, const FunObjVar* node, const SVFType* svfType, const ICFGNode* icn)
    : ValVar(i, svfType, icn, RetValNode), callGraphNode(node)
{
}

const FunObjVar* RetValPN::getFunction() const
{
    return callGraphNode;
}

bool RetValPN::isPointer() const
{
    return getFunction()->getReturnType()->isPointerTy();
}


const std::string RetValPN::getValueName() const
{
    return callGraphNode->getName() + "_ret";
}

const std::string GepObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "GepObjVar ID: " << getId() << " with offset_" + std::to_string(apOffset);
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getBaseObj()->valueOnlyToString();
    }
    return rawstr.str();
}

const SVFType *GepObjVar::getType() const
{
    return SVFIR::getPAG()->getFlatternedElemType(type, apOffset);
}

bool BaseObjVar::isBlackHoleObj() const
{
    return IRGraph::isBlkObj(getId());
}


const FunObjVar* BaseObjVar::getFunction() const
{
    if(icfgNode)
        return icfgNode->getFun();
    return nullptr;
}
const std::string BaseObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "BaseObjVar ID: " << getId() << " (base object)";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}


const std::string HeapObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "HeapObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string StackObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "StackObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}



FunValVar::FunValVar(NodeID i, const ICFGNode* icn, const FunObjVar* cgn, const SVFType* svfType)
    : ValVar(i, svfType, icn, FunValNode), funObjVar(cgn)
{
}

const std::string FunValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "FunValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << funObjVar->getFunction()->getName();
    }
    return rawstr.str();
}

const std::string ConstAggValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstAggValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}
const std::string ConstDataValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstDataValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string GlobalValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "GlobalValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstFPValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstFPValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstIntValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstIntValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstNullPtrValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstNullPtrValVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string GlobalObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "GlobalObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}
const std::string ConstAggObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstAggObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}
const std::string ConstDataObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstDataObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstFPObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstFPObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstIntObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstIntObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

const std::string ConstNullPtrObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "ConstNullPtrObjVar ID: " << getId();
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << valueOnlyToString();
    }
    return rawstr.str();
}

FunObjVar::FunObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node)
    : BaseObjVar(i, ti, svfType, node, FunObjNode)
{
}

void FunObjVar::initFunObjVar(bool decl, bool intrinc, bool addr, bool uncalled, bool notret, bool vararg,
                              const SVFFunctionType *ft, SVFLoopAndDomInfo *ld, const FunObjVar *real, BasicBlockGraph *bbg,
                              const std::vector<const ArgValVar *> &allarg, const SVFBasicBlock *exit)
{
    isDecl = decl;
    intrinsic = intrinc;
    isAddrTaken = addr;
    isUncalled = uncalled;
    isNotRet = notret;
    supVarArg = vararg;
    funcType = ft;
    loopAndDom = ld;
    realDefFun = real;
    bbGraph = bbg;
    allArgs = allarg;
    exitBlock = exit;
}


bool FunObjVar::isIsolatedNode() const
{
    return isIntrinsic();
}

const FunObjVar* FunObjVar::getFunction() const
{
    return this;
}

const std::string FunObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "FunObjVar ID: " << getId() << " (base object)";
    if (Options::ShowSVFIRValue())
    {
        rawstr << "\n";
        rawstr << getName();
    }
    return rawstr.str();
}

const std::string RetValPN::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "RetValPN ID: " << getId() << " unique return node for function " << callGraphNode->getName();
    return rawstr.str();
}

const FunObjVar* VarArgValPN::getFunction() const
{
    return callGraphNode;
}

const std::string VarArgValPN::getValueName() const
{
    return callGraphNode->getName() + "_vararg";
}

const std::string VarArgValPN::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "VarArgValPN ID: " << getId() << " Var arg node for function " << callGraphNode->getName();
    return rawstr.str();
}

const std::string DummyValVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "DummyValVar ID: " << getId();
    return rawstr.str();
}

const std::string DummyObjVar::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "DummyObjVar ID: " << getId();
    return rawstr.str();
}

