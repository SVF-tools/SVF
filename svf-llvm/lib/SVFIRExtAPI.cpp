//===- SVFIR.cpp -- Exteral function IR of SVF ---------------------------------------------//
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
 * SVFIRExtAPI.cpp
 *
 *  Created on: 18, 5, 2023
 *      Author: Shuangxiang Kan
 */

#include "SVF-LLVM/SVFIRBuilder.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;


/*!
 * Add the load/store constraints and temp. nodes for the complex constraint
 * *D = *S (where D/S may point to structs).
 */
void SVFIRBuilder::addComplexConsForExt(const SVFValue* D, const SVFValue* S, const SVFValue* szValue)
{
    assert(D && S);
    NodeID vnD= pag->getValueNode(D), vnS= pag->getValueNode(S);
    if(!vnD || !vnS)
        return;

    std::vector<AccessPath> fields;

    //Get the max possible size of the copy, unless it was provided.
    const SVFType* stype = pag->getTypeLocSetsMap(vnS).first;
    const SVFType* dtype = pag->getTypeLocSetsMap(vnD).first;
    std::vector<AccessPath> srcFields = pag->getTypeLocSetsMap(vnS).second;
    std::vector<AccessPath> dstFields = pag->getTypeLocSetsMap(vnD).second;

    if(srcFields.size() > dstFields.size())
        fields = dstFields;
    else
        fields = srcFields;

    /// If sz is 0, we will add edges for all fields.
    u32_t sz = fields.size();
    if (szValue && SVFUtil::dyn_cast<SVFConstantInt>(szValue))
    {
        const SVFConstantInt* arg2 = SVFUtil::dyn_cast<SVFConstantInt>(szValue);
        sz = (fields.size() > static_cast<u32_t>(arg2->getSExtValue())) ? arg2->getSExtValue() : fields.size();
    }

    if (fields.size() == 1 && (SVFUtil::isa<SVFConstantData>(D) || SVFUtil::isa<SVFConstantData>(S)))
    {
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(vnD,dummy);
        addStoreEdge(dummy,vnS);
        return;
    }

    //For each field (i), add (Ti = *S + i) and (*D + i = Ti).
    for (u32_t index = 0; index < sz; index++)
    {
        const SVFType* dElementType = pag->getSymbolInfo()->getFlatternedElemType(dtype,
                                      fields[index].getConstantFieldIdx());
        const SVFType* sElementType = pag->getSymbolInfo()->getFlatternedElemType(stype,
                                      fields[index].getConstantFieldIdx());
        NodeID dField = getGepValVar(D,fields[index],dElementType);
        NodeID sField = getGepValVar(S,fields[index],sElementType);
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(sField,dummy);
        addStoreEdge(dummy,dField);
    }
}

NodeID SVFIRBuilder::getExtID(ExtAPI::OperationType operationType, const std::string &s, const SVFCallInst* svfCall)
{
    s32_t nodeIDType = ExtAPI::getExtAPI()->getNodeIDType(s);

    // return value >= 0 is an argument node
    if (nodeIDType >= 0)
    {
        assert(svfCall->arg_size() > (u32_t) nodeIDType && "Argument out of bounds!");
        if (operationType == ExtAPI::OperationType::Memcpy_like || operationType == ExtAPI::OperationType::Memset_like)
            return nodeIDType;
        else
            return pag->getValueNode(svfCall->getArgOperand(nodeIDType));
    }
    // return value = -1 is an inst node
    else if (nodeIDType == -1)
        return pag->getValueNode(svfCall);
    // return value = -2 is a Dummy node
    else if (nodeIDType == -2)
        return pag->addDummyValNode();
    // return value = -3 is an object node
    else if (nodeIDType == -3)
    {
        assert(svfCall->getType()->isPointerTy() && "The operand should be a pointer type!");
        NodeID objId;
        // Indirect call
        if (getCallee(svfCall) == nullptr)
            objId = pag->addDummyObjNode(svfCall->getType());
        else // Direct call
            objId = pag->getObjectNode(svfCall);
        return objId;
    }
    // return value = -4 is a nullptr node
    else if (nodeIDType == -4)
        return pag->getNullPtr();
    // return value = -5 is an offset
    else if (nodeIDType == -5)
    {
        for (char const &c : s)
        {
            if (std::isdigit(c) == 0)
                assert(false && "Invalid offset!");
        }
        return atoi(s.c_str());
    }
    // return value = -6 is an illegal operand format
    else
    {
        assert(false && "The operand format of function operation is illegal!");
        abort();
    }
}

void SVFIRBuilder::parseAtomaticOp(SVF::ExtAPI::Operand &atomaticOp, const SVFCallInst* svfCall, std::map<std::string, NodeID> &nodeIDMap)
{
    // Skip Rb_tree operation, which is handled in extFuncAtomaticOperation()
    if (atomaticOp.getType() == ExtAPI::OperationType::Rb_tree_ops)
        return;
    // Get src and dst node ID
    if (!atomaticOp.getSrcValue().empty())
    {
        const std::string s = atomaticOp.getSrcValue();
        if (nodeIDMap.find(s) != nodeIDMap.end())
            atomaticOp.setSrcID(nodeIDMap[s]);
        else
        {
            NodeID srcId = getExtID(atomaticOp.getType(), atomaticOp.getSrcValue(), svfCall);
            atomaticOp.setSrcID(srcId);
            nodeIDMap[s] = srcId;
        }
    }
    else
        assert(false && "The 'src' operand cannot be empty.");

    if (!atomaticOp.getDstValue().empty())
    {
        const std::string s = atomaticOp.getDstValue();
        if (nodeIDMap.find(s) != nodeIDMap.end())
            atomaticOp.setDstID(nodeIDMap[s]);
        else
        {
            NodeID dstId = getExtID(atomaticOp.getType(), atomaticOp.getDstValue(), svfCall);
            atomaticOp.setDstID(dstId);
            nodeIDMap[s] = dstId;
        }
    }
    else
        assert(false && "The 'dst' operand cannot be empty.");

    // Get offset or size
    if (!atomaticOp.getOffsetOrSizeStr().empty())
    {
        std::string s = atomaticOp.getOffsetOrSizeStr();
        if (nodeIDMap.find(s) != nodeIDMap.end())
            atomaticOp.setOffsetOrSize(nodeIDMap[s]);
        else
        {
            NodeID offsetOrSize = getExtID(atomaticOp.getType(), atomaticOp.getOffsetOrSizeStr(), svfCall);
            atomaticOp.setOffsetOrSize(offsetOrSize);
            nodeIDMap[s] = offsetOrSize;
        }
    }
}

void SVFIRBuilder::parseExtFunctionOps(ExtAPI::ExtFunctionOps &extFunctionOps, const SVFCallInst* svfCall)
{
    // CallStmt operation
    if (extFunctionOps.getCallStmtNum() != 0)
        handleExtCallStat(extFunctionOps, svfCall);
    // Record all dummy nodes
    std::map<std::string, NodeID> nodeIDMap;
    for (ExtAPI::ExtOperation& extOperation : extFunctionOps.getOperations())
    {
        // CondStmt operation
        if(extOperation.isConOp())
        {
            for (ExtAPI::Operand &atomaticOp : extOperation.getTrueBranchOperands())
                parseAtomaticOp(atomaticOp, svfCall, nodeIDMap);
            for (ExtAPI::Operand &atomaticOp : extOperation.getFalseBranchOperands())
                parseAtomaticOp(atomaticOp, svfCall, nodeIDMap);
        }
        // General operation, e.g., "AddrStmt", "CopyStmt", ....
        else if (!extOperation.isCallOp())
            parseAtomaticOp(extOperation.getBasicOp(), svfCall, nodeIDMap);
    }
}

SVFCallInst* SVFIRBuilder::addSVFExtCallInst(const SVFCallInst* svfInst, SVFBasicBlock* svfBB, const SVFFunction* svfCaller, const SVFFunction* svfCallee)
{
    SVFCallInst* svfCall = new SVFCallInst(svfCallee->getFunctionType(), svfBB, false, false);
    svfCall->setName("ext_inst");
    LLVMModuleSet::getLLVMModuleSet()->SVFValue2LLVMValue[svfCall] = nullptr;
    svfCall->setCalledOperand(svfCallee);
    setCurrentLocation(svfCall, svfBB);
    SymbolTableInfo::ValueToIDMapTy::iterator iter = pag->getSymbolInfo()->valSyms().find(svfCall);
    if (iter == pag->getSymbolInfo()->valSyms().end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        pag->getSymbolInfo()->valSyms().insert(std::make_pair(svfCall, id));
        DBOUT(DMemModel, outs() << "create a new value sym " << id << "\n");
        pag->addValNode(svfCall, id);
    }
    svfCall->setName(svfCall->getName() + "_" + std::to_string(pag->getValueNode(svfCall)));

    for(u32_t i = 0; i < svfCallee->arg_size(); i++)
    {
        SVFInstruction* svfArg = addSVFExtInst("ext_inst", svfInst, svfBB, ExtAPI::OperationType::Other, svfCallee->getArg(i)->getType());
        svfArg->setName(svfArg->getName() + "_" + std::to_string(pag->getValueNode(svfArg)));
        svfCall->addArgument(svfArg);
    }

    svfBB->addInstruction(svfCall);

    CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(svfCall);
    pag->addCallSite(callBlockNode);

    if (!svfCallee->isNotRetFunction() && !isExtCall(svfCallee))
    {
        NodeID srcret = getReturnNode(svfCallee);
        NodeID dstrec = pag->getValueNode(svfCall);
        CallICFGNode* callICFGNode = pag->getICFG()->getCallICFGNode(svfCall);
        FunExitICFGNode* exitICFGNode = pag->getICFG()->getFunExitICFGNode(svfCallee);
        RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(svfCall);
        addRetEdge(srcret, dstrec,callICFGNode, exitICFGNode);
        pag->addCallSiteRets(retBlockNode,pag->getGNode(pag->getValueNode(svfCall)));
    }

    return svfCall;
}

void SVFIRBuilder::addSVFExtRetInst(SVFCallInst* svfCall, SVFBasicBlock* svfBB, SVFFunction* svfCaller)
{
    SVFInstruction* retInst = addSVFExtInst("ext_inst", svfCall, svfBB, ExtAPI::OperationType::Return, nullptr);
    retInst->setName(retInst->getName() + "_" + std::to_string(pag->getValueNode(retInst)));
    setCurrentLocation(retInst, svfBB);

    NodeID rnF = getReturnNode(svfCaller);
    NodeID vnS = pag->getValueNode(svfCall);
    const ICFGNode* icfgNode = pag->getICFG()->getICFGNode(retInst);
    //vnS may be null if src is a null ptr
    addPhiStmt(rnF,vnS,icfgNode);
}

SVFInstruction* SVFIRBuilder::addSVFExtInst(const std::string& instName, const SVFCallInst* svfInst, SVFBasicBlock* svfBB, SVF::ExtAPI::OperationType opType, const SVFType* svfType)
{
    // Get new SVFInstruction type;
    const SVFType* ptType = nullptr;
    if (svfType != nullptr)
        ptType = svfType;
    else
    {
        if (opType == ExtAPI::OperationType::Addr
                || opType == ExtAPI::OperationType::Copy
                || opType == ExtAPI::OperationType::Load
                || opType == ExtAPI::OperationType::Gep)
        {
            for (u32_t i = 0; i < svfInst->arg_size(); ++i)
                if (svfInst->getArgOperand(i)->getType()->isPointerTy())
                {
                    ptType = svfInst->getArgOperand(i)->getType();
                    break;
                }
        }
        else
            ptType = svfInst->getParent()->getType();
    }

    assert(ptType != nullptr && "At least one argument of an external call is of pointer type!");
    SVFInstruction* inst = new SVFInstruction(ptType, svfBB, false, false);
    inst->setName(instName);
    LLVMModuleSet::getLLVMModuleSet()->SVFValue2LLVMValue[inst] = nullptr;
    svfBB->addInstruction(inst);
    SymbolTableInfo::ValueToIDMapTy::iterator iter = pag->getSymbolInfo()->valSyms().find(inst);
    if (iter == pag->getSymbolInfo()->valSyms().end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        pag->getSymbolInfo()->valSyms().insert(std::make_pair(inst, id));
        DBOUT(DMemModel, outs() << "create a new value sym " << id << "\n");
        pag->addValNode(inst, id);
    }
    inst->setName(inst->getName() + "_" + std::to_string(pag->getValueNode(inst)));
    return inst;
}

SVFBasicBlock* SVFIRBuilder::extFuncInitialization(const SVFCallInst* svfInst, SVFFunction* svfCaller)
{
    // Initialization, linking actual parameters with formal parameters,
    // adding basic blocks for external functions,
    // and creating return edges (if the external function has a return value)
    CallICFGNode* callSiteICFGNode = pag->getICFG()->getCallICFGNode(svfInst);
    FunEntryICFGNode* funEntryICFGNode = pag->getICFG()->getFunEntryICFGNode(svfCaller);
    FunExitICFGNode* funExitICFGNode = pag->getICFG()->getFunExitICFGNode(svfCaller);
    for(u32_t i = 0; i < svfCaller->arg_size(); i++)
    {
        const SVFValue *AA = svfInst->getArgOperand(i);
        const SVFValue *FA = svfCaller->getArg(i);
        NodeID srcAA = pag->getValueNode(AA);
        NodeID dstFA = pag->getValueNode(FA);
        addCallEdge(srcAA, dstFA, callSiteICFGNode, funEntryICFGNode);
        pag->addFunArgs(svfCaller,pag->getGNode(dstFA));
    }

    SVFBasicBlock* svfBB = new SVFBasicBlock(svfInst->getParent()->getType(), svfCaller);
    svfBB->setName("ext_bb");
    LLVMModuleSet::getLLVMModuleSet()->SVFValue2LLVMValue[svfBB] = nullptr;

    if (!svfCaller->isNotRetFunction())
    {
        NodeID srcret = getReturnNode(svfCaller);
        NodeID dstrec = pag->getValueNode(svfInst);
        addRetEdge(srcret, dstrec,callSiteICFGNode, funExitICFGNode);
        pag->addFunRet(svfCaller,pag->getGNode(srcret));
    }
    return svfBB;
}

void SVFIRBuilder::handleExtCallStat(ExtAPI::ExtFunctionOps &extFunctionOps, const SVFCallInst* svfInst)
{
    SVFFunction* svfCaller =  const_cast<SVFFunction *>(svfModule->getSVFFunction(svfInst->getCalledFunction()->getName()));
    SVFBasicBlock* svfBB = extFuncInitialization(svfInst, svfCaller);
    // Map an operand to its new created SVFInstruction
    std::map<std::string, SVFValue*> operandToSVFValueMap;
    for (ExtAPI::ExtOperation& extOperation : extFunctionOps.getOperations())
    {
        if (extOperation.isCallOp())
        {
            // To create a CallInst for the callee
            const SVFFunction* svfCallee = svfModule->getSVFFunction(extOperation.getCalleeName());
            SVFCallInst* svfCall = addSVFExtCallInst(svfInst, svfBB, svfCaller, svfCallee);
            setCurrentLocation(svfCall, getCurrentBB());
            operandToSVFValueMap[svfCallee->getName()] = svfCall;
            CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(svfCall);

            assert(extOperation.getCalleeOperands().size() >= svfCallee->arg_size()
                   && "Number of arguments set in CallStmt in ExtAPI.json is inconsistent with the number of arguments required by the Callee?");
            // To parse the operations contained in ‘CallStmt’, obtain the NodeID, and add the callEdge
            for (ExtAPI::Operand &operand : extOperation.getCalleeOperands())
            {
                std::string src = operand.getSrcValue();
                std::string dst = operand.getDstValue();
                // ReturnStmt
                if (operand.getType() == ExtAPI::OperationType::Return && !svfCaller->isNotRetFunction())
                {
                    addSVFExtRetInst(svfCall, svfBB, svfCaller);
                    continue;
                }

                auto getCallStmtOperands = [](const std::string& str) -> std::pair<std::string, std::string>
                {
                    size_t pos = str.find('_');
                    assert(pos != std::string::npos && "The operand format in CallStmt is incorrect! It should be either 'funName_Argi' or 'funName_Ret'!");
                    return std::make_pair(str.substr(0, pos), str.substr(pos + 1));
                };
                // 'src' operand
                if (src.find('_') != std::string::npos)
                {
                    std::pair<std::string, std::string> srcRet = getCallStmtOperands(src);
                    s32_t argPos = ExtAPI::getExtAPI()->getNodeIDType(srcRet.second);
                    // operand like "caller_Argi"
                    if (svfCaller->getName() == srcRet.first)
                    {
                        assert(argPos >= 0 && static_cast<u32_t>(argPos) < svfCaller->arg_size() && "The argument index is out of bounds in CallStmt?");
                        operand.setSrcID(pag->getValueNode(svfCaller->getArg(argPos)));
                    }
                    // operand like "callee_Ret"
                    else
                    {
                        assert(argPos == -1 && "The operand format in CallStmt is incorrect! It should be either 'funName_Argi' or 'funName_Ret'!");
                        assert(operandToSVFValueMap.find(srcRet.first) != operandToSVFValueMap.end() && "No created SVFCallInst in external functions?");
                        operand.setSrcID(pag->getValueNode(operandToSVFValueMap[srcRet.first]));
                    }
                }
                // operand like self-defined "x", which should be created beforehand
                else
                {
                    assert(operandToSVFValueMap.find(src) != operandToSVFValueMap.end()
                           && "Cannot find manual create ext inst, incorrect invocation order for external functions?");
                    operand.setSrcID(pag->getValueNode(operandToSVFValueMap[src]));
                }
                // 'dst' operand
                if (dst.find('_') != std::string::npos)
                {
                    // operand like "callee_Argi"
                    std::pair<std::string, std::string> dstRet = getCallStmtOperands(dst);
                    assert(svfCallee->getName() == dstRet.first && "The operand format of 'dst' in external CallStmt is illegal!");
                    s32_t argPos = ExtAPI::getExtAPI()->getNodeIDType(dstRet.second);
                    assert(argPos >= 0 && static_cast<u32_t>(argPos) < svfCallee->arg_size() && "The argument index is out of bounds of callee in CallStmt?");
                    // Create an new SVFInstruction for "callee_Argi".
                    if(operandToSVFValueMap.find(dst) == operandToSVFValueMap.end())
                    {
                        SVFInstruction* inst = addSVFExtInst("ext_inst", svfInst, svfBB, operand.getType(), nullptr);
                        operand.setDstID(pag->getValueNode(inst));
                        operandToSVFValueMap[dst] = inst;
                        pag->addCallSiteArgs(callBlockNode,pag->getGNode(pag->getValueNode(inst)));
                    }
                    CallICFGNode* icfgNode = pag->getICFG()->getCallICFGNode(svfCall);
                    FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(svfCallee);
                    addCallEdge(operand.getDstID(), pag->getValueNode(svfCallee->getArg(argPos)), icfgNode, entry);
                }
                else
                {
                    // operand like self-defined "x", if there are no SVFInstructions for 'x', create an new SVFInstruction.
                    SVFInstruction* inst = addSVFExtInst(dst, svfInst, svfBB, operand.getType(), nullptr);
                    operand.setDstID(pag->getValueNode(inst));
                    operandToSVFValueMap[dst] = inst;
                }
            }
        }
    }
    svfCaller->addBasicBlock(svfBB);
}

void SVFIRBuilder::extFuncAtomaticOperation(ExtAPI::Operand& atomicOp, const SVFCallInst* svfCall)
{
    if (atomicOp.getType() == ExtAPI::OperationType::Addr)
    {
        if (!atomicOp.getSrcValue().empty() && !atomicOp.getDstValue().empty())
            addAddrEdge(atomicOp.getSrcID(), atomicOp.getDstID());
        else
            writeWrnMsg("We need two valid NodeIDs to add an Addr edge");
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Copy)
    {
        if (!atomicOp.getSrcValue().empty() && !atomicOp.getDstValue().empty())
            addCopyEdge(atomicOp.getSrcID(), atomicOp.getDstID());
        else
            writeWrnMsg("We need two valid NodeIDs to add a Copy edge");
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Load)
    {
        if (!atomicOp.getSrcValue().empty() && !atomicOp.getDstValue().empty())
            addLoadEdge(atomicOp.getSrcID(), atomicOp.getDstID());
        else
            writeWrnMsg("We need two valid NodeIDs to add a Load edge");
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Store)
    {
        if (!atomicOp.getSrcValue().empty() && !atomicOp.getDstValue().empty())
            addStoreEdge(atomicOp.getSrcID(), atomicOp.getDstID());
        else
            writeWrnMsg("We need two valid NodeIDs to add a Store edge");
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Gep)
    {
        if (!atomicOp.getSrcValue().empty() && !atomicOp.getDstValue().empty() && !atomicOp.getOffsetOrSizeStr().empty())
        {
            AccessPath ap(atomicOp.getOffsetOrSize());
            addNormalGepEdge(atomicOp.getSrcID(), atomicOp.getDstID(), ap);
        }
        else
            writeWrnMsg("We need two valid NodeIDs and an offset to add a Gep edge");
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Return)
        return;
    else if (atomicOp.getType() == ExtAPI::OperationType::Memset_like)
    {
        // this is for memset(void *str, int c, size_t n)
        // which copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str
        // const SVFConstantInt* arg2 = SVFUtil::dyn_cast<SVFConstantInt>(svfCall->getArgOperand(op.getOperands()[2]));
        NodeID argId = pag->getValueNode(svfCall->getArgOperand(0));
        std::vector<AccessPath> dstFields =  pag->getTypeLocSetsMap(argId).second;
        u32_t sz = dstFields.size();
        if (const SVFConstantInt* arg2 = SVFUtil::dyn_cast<SVFConstantInt>(svfCall->getArgOperand(2)))
            sz = (dstFields.size() > static_cast<u32_t>(arg2->getSExtValue())) ? arg2->getSExtValue() : dstFields.size();
        //For each field (i), add store edge *(arg0 + i) = arg1
        for (u32_t index = 0; index < sz; index++)
        {
            const SVFType* dElementType = pag->getSymbolInfo()->getFlatternedElemType(pag->getTypeLocSetsMap(argId).first, dstFields[index].getConstantFieldIdx());
            NodeID dField = getGepValVar(svfCall->getArgOperand(0), dstFields[index], dElementType);
            addStoreEdge(pag->getValueNode(svfCall->getArgOperand(1)),dField);
        }
        if(svfCall->getType()->isPointerTy())
            addCopyEdge(pag->getValueNode(svfCall->getArgOperand(0)), pag->getValueNode(svfCall));
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Memcpy_like)
    {
        if(svfCall->arg_size() == 3)
            addComplexConsForExt(svfCall->getArgOperand(0), svfCall->getArgOperand(1), svfCall->getArgOperand(2));
        else
            addComplexConsForExt(svfCall->getArgOperand(0), svfCall->getArgOperand(1), nullptr);
    }
    else if (atomicOp.getType() == ExtAPI::OperationType::Rb_tree_ops)
    {
        assert(svfCall->arg_size() == 4 && "_Rb_tree_insert_and_rebalance should have 4 arguments.\n");

        // We have vArg3 points to the entry of _Rb_tree_node_base { color; parent; left; right; }.
        // Now we calculate the offset from base to vArg3
        NodeID vnArg3 = pag->getValueNode(svfCall->getArgOperand(3));
        APOffset offset = getAccessPathFromBaseNode(vnArg3).getConstantFieldIdx();

        // We get all flattened fields of base
        vector<AccessPath> fields =  pag->getTypeLocSetsMap(vnArg3).second;

        // We summarize the side effects: arg3->parent = arg1, arg3->left = arg1, arg3->right = arg1
        // Note that arg0 is aligned with "offset".
        for (APOffset i = offset + 1; i <= offset + 3; ++i)
        {
            if((u32_t)i >= fields.size())
                break;
            const SVFType* elementType = pag->getSymbolInfo()->getFlatternedElemType(pag->getTypeLocSetsMap(vnArg3).first,
                                         fields[i].getConstantFieldIdx());
            NodeID vnD = getGepValVar(svfCall->getArgOperand(3), fields[i], elementType);
            NodeID vnS = pag->getValueNode(svfCall->getArgOperand(1));
            if(vnD && vnS)
                addStoreEdge(vnS,vnD);
        }
    }
    // default
    // illegal function operation of external function
    else
    {
        assert(false && "new type of SVFStmt for external calls?");
    }
}

/*!
 * Handle external calls
 */
void SVFIRBuilder::handleExtCall(const SVFInstruction* svfInst, const SVFFunction* svfCallee)
{
    const SVFCallInst* svfCall = SVFUtil::cast<SVFCallInst>(svfInst);

    if (isHeapAllocOrStaticExtCall(svfInst))
    {
        // case 1: ret = new obj
        if (isHeapAllocExtCallViaRet(svfInst) || isStaticExtCall(svfInst))
        {
            NodeID val = pag->getValueNode(svfInst);
            NodeID obj = pag->getObjectNode(svfInst);
            addAddrEdge(obj, val);
        }
        // case 2: *arg = new obj
        else
        {
            assert(isHeapAllocExtCallViaArg(svfInst) && "Must be heap alloc call via arg.");
            u32_t arg_pos = getHeapAllocHoldingArgPosition(svfCallee);
            const SVFValue* arg = svfCall->getArgOperand(arg_pos);
            if (arg->getType()->isPointerTy())
            {
                NodeID vnArg = pag->getValueNode(arg);
                NodeID dummy = pag->addDummyValNode();
                NodeID obj = pag->addDummyObjNode(arg->getType());
                if (vnArg && dummy && obj)
                {
                    addAddrEdge(obj, dummy);
                    addStoreEdge(dummy, vnArg);
                }
            }
            else
            {
                writeWrnMsg("Arg receiving new object must be pointer type");
            }
        }
    }
    else
    {
        if (isExtCall(svfCallee))
        {
            ExtAPI::ExtFunctionOps extFunctionOps = ExtAPI::getExtAPI()->getExtFunctionOps(svfCallee);
            if (extFunctionOps.getOperations().size() == 0)
            {
                std::string str;
                std::stringstream rawstr(str);
                rawstr << "function " << svfCallee->getName() << " not in the external function summary ExtAPI.json file";
                writeWrnMsg(rawstr.str());
            }
            else
            {
                parseExtFunctionOps(extFunctionOps, svfCall);
                for (ExtAPI::ExtOperation op : extFunctionOps.getOperations())
                {
                    if (op.isCallOp())
                        for (ExtAPI::Operand atomicOp : op.getCalleeOperands())
                            extFuncAtomaticOperation(atomicOp, svfCall);
                    else if(op.isConOp())
                    {
                        for (ExtAPI::Operand atomicOp : op.getTrueBranchOperands())
                            extFuncAtomaticOperation(atomicOp, svfCall);
                        for (ExtAPI::Operand atomicOp : op.getFalseBranchOperands())
                            extFuncAtomaticOperation(atomicOp, svfCall);
                    }
                    else
                    {
                        ExtAPI::Operand atomicOp = op.getBasicOp();
                        extFuncAtomaticOperation(atomicOp, svfCall);
                    }
                }
            }
        }

        /// create inter-procedural SVFIR edges for thread forks
        if (isThreadForkCall(svfInst))
        {
            if (const SVFFunction* forkedFun = SVFUtil::dyn_cast<SVFFunction>(getForkedFun(svfInst)))
            {
                forkedFun = forkedFun->getDefFunForMultipleModule();
                const SVFValue* actualParm = getActualParmAtForkSite(svfInst);
                /// pthread_create has 1 arg.
                /// apr_thread_create has 2 arg.
                assert((forkedFun->arg_size() <= 2) && "Size of formal parameter of start routine should be one");
                if (forkedFun->arg_size() <= 2 && forkedFun->arg_size() >= 1)
                {
                    const SVFArgument* formalParm = forkedFun->getArg(0);
                    /// Connect actual parameter to formal parameter of the start routine
                    if (actualParm->getType()->isPointerTy() && formalParm->getType()->isPointerTy())
                    {
                        CallICFGNode *icfgNode = pag->getICFG()->getCallICFGNode(svfInst);
                        FunEntryICFGNode *entry = pag->getICFG()->getFunEntryICFGNode(forkedFun);
                        addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm), icfgNode, entry);
                    }
                }
            }
            else
            {
                /// handle indirect calls at pthread create APIs e.g., pthread_create(&t1, nullptr, fp, ...);
                /// const Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                /// if(!SVFUtil::isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
            /// If forkedFun does not pass to spawnee as function type but as void pointer
            /// remember to update inter-procedural callgraph/SVFIR/SVFG etc. when indirect call targets are resolved
            /// We don't connect the callgraph here, further investigation is need to hanle mod-ref during SVFG construction.
        }

        /// create inter-procedural SVFIR edges for hare_parallel_for calls
        else if (isHareParForCall(svfInst))
        {
            if (const SVFFunction* taskFunc = SVFUtil::dyn_cast<SVFFunction>(getTaskFuncAtHareParForSite(svfInst)))
            {
                /// The task function of hare_parallel_for has 3 args.
                assert((taskFunc->arg_size() == 3) && "Size of formal parameter of hare_parallel_for's task routine should be 3");
                const SVFValue* actualParm = getTaskDataAtHareParForSite(svfInst);
                const SVFArgument* formalParm = taskFunc->getArg(0);
                /// Connect actual parameter to formal parameter of the start routine
                if (actualParm->getType()->isPointerTy() && formalParm->getType()->isPointerTy())
                {
                    CallICFGNode *icfgNode = pag->getICFG()->getCallICFGNode(svfInst);
                    FunEntryICFGNode *entry = pag->getICFG()->getFunEntryICFGNode(taskFunc);
                    addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm), icfgNode, entry);
                }
            }
            else
            {
                /// handle indirect calls at hare_parallel_for (e.g., hare_parallel_for(..., fp, ...);
                /// const Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                /// if(!SVFUtil::isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
        }

        /// TODO: inter-procedural SVFIR edges for thread joins
    }
}