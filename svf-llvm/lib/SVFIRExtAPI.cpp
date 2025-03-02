//===- SVFIRExtAPI.cpp -- External function IR of SVF ---------------------------------------------//
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
#include "Util/SVFUtil.h"
#include "SVF-LLVM/SymbolTableBuilder.h"
#include "SVF-LLVM/ObjTypeInference.h"
#include "Graphs/CallGraph.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

/*!
 * Find the base type and the max possible offset of an object pointed to by (V).
 */
const Type* SVFIRBuilder::getBaseTypeAndFlattenedFields(const Value* V, std::vector<AccessPath> &fields, const Value* szValue)
{
    assert(V);
    const Value* value = getBaseValueForExtArg(V);
    const Type *objType = LLVMModuleSet::getLLVMModuleSet()->getTypeInference()->inferObjType(value);
    u32_t numOfElems = pag->getNumOfFlattenElements(LLVMModuleSet::getLLVMModuleSet()->getSVFType(objType));
    /// use user-specified size for this copy operation if the size is a constaint int
    if(szValue && SVFUtil::isa<ConstantInt>(szValue))
    {
        auto szIntVal = LLVMUtil::getIntegerValue(SVFUtil::cast<ConstantInt>(szValue));
        numOfElems = (numOfElems > szIntVal.first) ? szIntVal.first : numOfElems;
    }

    LLVMContext& context = LLVMModuleSet::getLLVMModuleSet()->getContext();
    for(u32_t ei = 0; ei < numOfElems; ei++)
    {
        AccessPath ls(ei);
        // make a ConstantInt and create char for the content type due to byte-wise copy
        const ConstantInt* offset = ConstantInt::get(context, llvm::APInt(32, ei));
        if (!llvmModuleSet()->hasValueNode(offset))
        {
            SymbolTableBuilder builder(pag);
            builder.collectSym(offset);
            NodeID id = llvmModuleSet()->getValueNode(offset);
            pag->addConstantIntValNode(id, LLVMUtil::getIntegerValue(offset), nullptr, llvmModuleSet()->getSVFType(offset->getType()));
            llvmModuleSet()->addToSVFVar2LLVMValueMap(offset,
                    pag->getGNode(id));
        }
        ls.addOffsetVarAndGepTypePair(getPAG()->getGNode(llvmModuleSet()->getValueNode(offset)), nullptr);
        fields.push_back(ls);
    }
    return objType;
}

/*!
 * Add the load/store constraints and temp. nodes for the complex constraint
 * *D = *S (where D/S may point to structs).
 */
void SVFIRBuilder::addComplexConsForExt(Value *D, Value *S, const Value* szValue)
{
    assert(D && S);
    NodeID vnD= getValueNode(D), vnS= getValueNode(S);
    if(!vnD || !vnS)
        return;

    std::vector<AccessPath> fields;

    //Get the max possible size of the copy, unless it was provided.
    std::vector<AccessPath> srcFields;
    std::vector<AccessPath> dstFields;
    const Type* stype = getBaseTypeAndFlattenedFields(S, srcFields, szValue);
    const Type* dtype = getBaseTypeAndFlattenedFields(D, dstFields, szValue);
    if(srcFields.size() > dstFields.size())
        fields = dstFields;
    else
        fields = srcFields;

    /// If sz is 0, we will add edges for all fields.
    u32_t sz = fields.size();

    if (fields.size() == 1 && (LLVMUtil::isConstDataOrAggData(D) || LLVMUtil::isConstDataOrAggData(S)))
    {
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(vnD,dummy);
        addStoreEdge(dummy,vnS);
        return;
    }

    //For each field (i), add (Ti = *S + i) and (*D + i = Ti).
    for (u32_t index = 0; index < sz; index++)
    {
        LLVMModuleSet* llvmmodule = LLVMModuleSet::getLLVMModuleSet();
        const SVFType* dElementType = pag->getFlatternedElemType(llvmmodule->getSVFType(dtype),
                                      fields[index].getConstantStructFldIdx());
        const SVFType* sElementType = pag->getFlatternedElemType(llvmmodule->getSVFType(stype),
                                      fields[index].getConstantStructFldIdx());
        NodeID dField = getGepValVar(D,fields[index],dElementType);
        NodeID sField = getGepValVar(S,fields[index],sElementType);
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(sField,dummy);
        addStoreEdge(dummy,dField);
    }
}

void SVFIRBuilder::handleExtCall(const CallBase* cs, const Function* callee)
{
    const CallICFGNode *callICFGNode = llvmModuleSet()->getCallICFGNode(cs);

    if (isHeapAllocExtCallViaRet(callICFGNode))
    {
        NodeID val = llvmModuleSet()->getValueNode(cs);
        NodeID obj = llvmModuleSet()->getObjectNode(cs);
        addAddrWithHeapSz(obj, val, cs);
    }
    else if (isHeapAllocExtCallViaArg(callICFGNode))
    {
        u32_t arg_pos = LLVMUtil::getHeapAllocHoldingArgPosition(callee);
        Value* arg = cs->getArgOperand(arg_pos);
        if (cs->getArgOperand(arg_pos)->getType()->isPointerTy())
        {
            NodeID vnArg = llvmModuleSet()->getValueNode(arg);
            NodeID dummy = pag->addDummyValNode();
            NodeID obj = pag->addDummyObjNode(llvmModuleSet()->getSVFType(cs->getArgOperand(arg_pos)->getType()));
            if (vnArg && dummy && obj)
            {
                addAddrWithHeapSz(obj, dummy, cs);
                addStoreEdge(dummy, vnArg);
            }
        }
        else
        {
            writeWrnMsg("Arg receiving new object must be pointer type");
        }
    }
    else if (LLVMUtil::isMemcpyExtFun(callee))
    {
        // Side-effects similar to void *memcpy(void *dest, const void * src, size_t n)
        // which  copies n characters from memory area 'src' to memory area 'dest'.
        if(callee->getName().find("iconv") != std::string::npos)
            addComplexConsForExt(cs->getArgOperand(3), cs->getArgOperand(1), nullptr);
        else if(callee->getName().find("bcopy") != std::string::npos)
            addComplexConsForExt(cs->getArgOperand(1), cs->getArgOperand(0), cs->getArgOperand(2));
        if(cs->arg_size() == 3)
            addComplexConsForExt(cs->getArgOperand(0), cs->getArgOperand(1), cs->getArgOperand(2));
        else
            addComplexConsForExt(cs->getArgOperand(0), cs->getArgOperand(1), nullptr);
        if(SVFUtil::isa<PointerType>(cs->getType()))
            addCopyEdge(getValueNode(cs->getArgOperand(0)), getValueNode(cs), CopyStmt::COPYVAL);
    }
    else if(LLVMUtil::isMemsetExtFun(callee))
    {
        // Side-effects similar to memset(void *str, int c, size_t n)
        // which copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str
        std::vector<AccessPath> dstFields;
        const Type *dtype = getBaseTypeAndFlattenedFields(cs->getArgOperand(0), dstFields, cs->getArgOperand(2));
        u32_t sz = dstFields.size();
        //For each field (i), add store edge *(arg0 + i) = arg1
        for (u32_t index = 0; index < sz; index++)
        {
            LLVMModuleSet* llvmmodule = LLVMModuleSet::getLLVMModuleSet();
            const SVFType* dElementType = pag->getFlatternedElemType(llvmmodule->getSVFType(dtype),
                                          dstFields[index].getConstantStructFldIdx());
            NodeID dField = getGepValVar(cs->getArgOperand(0), dstFields[index], dElementType);
            addStoreEdge(getValueNode(cs->getArgOperand(1)),dField);
        }
        if(SVFUtil::isa<PointerType>(cs->getType()))
            addCopyEdge(getValueNode(cs->getArgOperand(0)), getValueNode(cs), CopyStmt::COPYVAL);
    }
    else if(callee->getName().compare("dlsym") == 0)
    {
        /*
        Side-effects of void* dlsym( void* handle, const char* funName),
        Locate the function with the name "funName," then add a "copy" edge between the callsite and that function.
        dlsym() example:
            int main() {
                // Open the shared library
                void* handle = dlopen("./my_shared_library.so", RTLD_LAZY);
                // Find the function address
                void (*myFunctionPtr)() = (void (*)())dlsym(handle, "myFunction");
                // Call the function
                myFunctionPtr();
            }
        */
        const Value* src = cs->getArgOperand(1);
        if(const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(src))
            src = stripConstantCasts(gep->getPointerOperand());

        auto getHookFn = [](const Value* src)->const Function*
        {
            if (!SVFUtil::isa<GlobalVariable>(src))
                return nullptr;

            auto *glob = SVFUtil::cast<GlobalVariable>(src);
            if (!glob->hasInitializer() || !SVFUtil::isa<ConstantDataArray>(glob->getInitializer()))
                return nullptr;

            auto *constarray = SVFUtil::cast<ConstantDataArray>(glob->getInitializer());
            return LLVMUtil::getProgFunction(constarray->getAsCString().str());
        };

        if (const Function *fn = getHookFn(src))
        {
            NodeID srcNode = getValueNode(fn);
            addCopyEdge(srcNode,  getValueNode(cs), CopyStmt::COPYVAL);
        }
    }
    else if(callee->getName().find("_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_") != std::string::npos)
    {
        // The purpose of this function is to insert a new node into the red-black tree and then rebalance the tree to ensure that the red-black tree properties are maintained.
        assert(cs->arg_size() == 4 && "_Rb_tree_insert_and_rebalance should have 4 arguments.\n");

        // We have vArg3 points to the entry of _Rb_tree_node_base { color; parent; left; right; }.
        // Now we calculate the offset from base to vArg3
        NodeID vnArg3 = llvmModuleSet()->getValueNode(cs->getArgOperand(3));
        APOffset offset =
            getAccessPathFromBaseNode(vnArg3).getConstantStructFldIdx();

        // We get all flattened fields of base
        vector<AccessPath> fields =  pag->getTypeLocSetsMap(vnArg3).second;

        // We summarize the side effects: arg3->parent = arg1, arg3->left = arg1, arg3->right = arg1
        // Note that arg0 is aligned with "offset".
        for (APOffset i = offset + 1; i <= offset + 3; ++i)
        {
            if((u32_t)i >= fields.size())
                break;
            const SVFType* elementType = pag->getFlatternedElemType(pag->getTypeLocSetsMap(vnArg3).first,
                                         fields[i].getConstantStructFldIdx());
            NodeID vnD = getGepValVar(cs->getArgOperand(3), fields[i], elementType);
            NodeID vnS = llvmModuleSet()->getValueNode(cs->getArgOperand(1));
            if(vnD && vnS)
                addStoreEdge(vnS,vnD);
        }
    }

    if (isThreadForkCall(callICFGNode))
    {
        const ValVar* valVar = getForkedFun(callICFGNode);
        if (const FunValVar* funcValVar = SVFUtil::dyn_cast<FunValVar>(valVar))
        {
            const FunObjVar* forkedFun = funcValVar->getFunction()->getDefFunForMultipleModule();
            const SVFVar* actualParm = getActualParmAtForkSite(callICFGNode);
            /// pthread_create has 1 arg.
            /// apr_thread_create has 2 arg.
            assert((forkedFun->arg_size() <= 2) && "Size of formal parameter of start routine should be one");
            if (forkedFun->arg_size() <= 2 && forkedFun->arg_size() >= 1)
            {
                const ArgValVar* formalParm = forkedFun->getArg(0);
                /// Connect actual parameter to formal parameter of the start routine
                if (actualParm->isPointer() && formalParm->getType()->isPointerTy())
                {
                    FunEntryICFGNode *entry = pag->getICFG()->getFunEntryICFGNode(forkedFun);
                    addThreadForkEdge(actualParm->getId(), formalParm->getId(), callICFGNode, entry);
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
        /// We don't connect the callgraph here, further investigation is need to handle mod-ref during SVFG construction.
    }

    /// TODO: inter-procedural SVFIR edges for thread joins
}