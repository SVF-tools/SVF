//===- SymbolTableBuilder.cpp -- Symbol Table builder---------------------//
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
 * SymbolTableBuilder.cpp
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei Sui
 */

#include <memory>

#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/CppUtil.h"
#include "SVF-LLVM/GEPTypeBridgeIterator.h" // include bridge_gep_iterator
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SymbolTableBuilder.h"
#include "Util/NodeIDAllocator.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "SVF-LLVM/ObjTypeInference.h"

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

ObjTypeInfo* SymbolTableBuilder::createBlkObjTypeInfo(NodeID symId)
{
    assert(svfir->isBlkObj(symId));
    LLVMModuleSet* llvmset = llvmModuleSet();
    if (svfir->objTypeInfoMap.find(symId)==svfir->objTypeInfoMap.end())
    {
        ObjTypeInfo* ti =svfir->createObjTypeInfo(llvmset->getSVFType(
                             IntegerType::get(llvmset->getContext(), 32)));
        ti->setNumOfElements(0);
        svfir->objTypeInfoMap[symId] = ti;
    }
    ObjTypeInfo* ti = svfir->objTypeInfoMap[symId];
    return ti;
}

ObjTypeInfo* SymbolTableBuilder::createConstantObjTypeInfo(NodeID symId)
{
    assert(IRGraph::isConstantSym(symId));
    LLVMModuleSet* llvmset = llvmModuleSet();
    if (svfir->objTypeInfoMap.find(symId)==svfir->objTypeInfoMap.end())
    {
        ObjTypeInfo* ti = svfir->createObjTypeInfo(
                              llvmset->getSVFType(IntegerType::get(llvmset->getContext(), 32)));
        ti->setNumOfElements(0);
        svfir->objTypeInfoMap[symId] = ti;
    }
    ObjTypeInfo* ti = svfir->objTypeInfoMap[symId];
    return ti;
}


/*!
 *  This method identify which is value sym and which is object sym
 */
void SymbolTableBuilder::buildMemModel()
{
    SVFUtil::increaseStackSize();

    // Pointer #0 always represents the null pointer.
    assert(svfir->totalSymNum++ == IRGraph::NullPtr && "Something changed!");

    // Pointer #1 always represents the pointer points-to black hole.
    assert(svfir->totalSymNum++ == IRGraph::BlkPtr && "Something changed!");

    // Object #2 is black hole the object that may point to any object
    assert(svfir->totalSymNum++ == IRGraph::BlackHole && "Something changed!");
    createBlkObjTypeInfo(IRGraph::BlackHole);

    // Object #3 always represents the unique constant of a program (merging all constants if Options::ModelConsts is disabled)
    assert(svfir->totalSymNum++ == IRGraph::ConstantObj && "Something changed!");
    createConstantObjTypeInfo(IRGraph::ConstantObj);

    for (Module &M : llvmModuleSet()->getLLVMModules())
    {
        // Add symbols for all the globals .
        for (const GlobalVariable& gv : M.globals())
        {
            collectSym(&gv);
        }

        // Add symbols for all the global aliases
        for (const GlobalAlias& ga : M.aliases())
        {
            collectSym(&ga);
            collectSym(ga.getAliasee());
        }

        // Add symbols for all of the functions and the instructions in them.
        for (const Function& fun : M.functions())
        {
            collectSym(&fun);
            collectRet(&fun);
            if (fun.getFunctionType()->isVarArg())
                collectVararg(&fun);

            // Add symbols for all formal parameters.
            for (const Argument& arg : fun.args())
            {
                collectSym(&arg);
            }

            // collect and create symbols inside the function body
            for (const Instruction& inst : instructions(fun))
            {
                collectSym(&inst);

                // initialization for some special instructions
                //{@
                if (const StoreInst* st = SVFUtil::dyn_cast<StoreInst>(&inst))
                {
                    collectSym(st->getPointerOperand());
                    collectSym(st->getValueOperand());
                }
                else if (const LoadInst* ld =
                             SVFUtil::dyn_cast<LoadInst>(&inst))
                {
                    collectSym(ld->getPointerOperand());
                }
                else if (const AllocaInst* alloc =
                             SVFUtil::dyn_cast<AllocaInst>(&inst))
                {
                    collectSym(alloc->getArraySize());
                }
                else if (const PHINode* phi = SVFUtil::dyn_cast<PHINode>(&inst))
                {
                    for (u32_t i = 0; i < phi->getNumIncomingValues(); ++i)
                    {
                        collectSym(phi->getIncomingValue(i));
                    }
                }
                else if (const GetElementPtrInst* gep =
                             SVFUtil::dyn_cast<GetElementPtrInst>(&inst))
                {
                    collectSym(gep->getPointerOperand());
                    for (u32_t i = 0; i < gep->getNumOperands(); ++i)
                    {
                        collectSym(gep->getOperand(i));
                    }
                }
                else if (const SelectInst* sel =
                             SVFUtil::dyn_cast<SelectInst>(&inst))
                {
                    collectSym(sel->getTrueValue());
                    collectSym(sel->getFalseValue());
                    collectSym(sel->getCondition());
                }
                else if (const BinaryOperator* binary =
                             SVFUtil::dyn_cast<BinaryOperator>(&inst))
                {
                    for (u32_t i = 0; i < binary->getNumOperands(); i++)
                        collectSym(binary->getOperand(i));
                }
                else if (const UnaryOperator* unary =
                             SVFUtil::dyn_cast<UnaryOperator>(&inst))
                {
                    for (u32_t i = 0; i < unary->getNumOperands(); i++)
                        collectSym(unary->getOperand(i));
                }
                else if (const CmpInst* cmp = SVFUtil::dyn_cast<CmpInst>(&inst))
                {
                    for (u32_t i = 0; i < cmp->getNumOperands(); i++)
                        collectSym(cmp->getOperand(i));
                }
                else if (const CastInst* cast =
                             SVFUtil::dyn_cast<CastInst>(&inst))
                {
                    collectSym(cast->getOperand(0));
                }
                else if (const ReturnInst* ret =
                             SVFUtil::dyn_cast<ReturnInst>(&inst))
                {
                    if (ret->getReturnValue())
                        collectSym(ret->getReturnValue());
                }
                else if (const BranchInst* br =
                             SVFUtil::dyn_cast<BranchInst>(&inst))
                {
                    Value* opnd = br->isConditional() ? br->getCondition() : br->getOperand(0);
                    collectSym(opnd);
                }
                else if (const SwitchInst* sw =
                             SVFUtil::dyn_cast<SwitchInst>(&inst))
                {
                    collectSym(sw->getCondition());
                }
                else if (const FreezeInst* fz = SVFUtil::dyn_cast<FreezeInst>(&inst))
                {

                    for (u32_t i = 0; i < fz->getNumOperands(); i++)
                    {
                        Value* opnd = inst.getOperand(i);
                        collectSym(opnd);
                    }
                }
                else if (isNonInstricCallSite(&inst))
                {

                    const CallBase* cs = LLVMUtil::getLLVMCallSite(&inst);
                    for (u32_t i = 0; i < cs->arg_size(); i++)
                    {
                        collectSym(cs->getArgOperand(i));
                    }
                    // Calls to inline asm need to be added as well because the
                    // callee isn't referenced anywhere else.
                    const Value* Callee = cs->getCalledOperand();
                    collectSym(Callee);

                    // TODO handle inlineAsm
                    /// if (SVFUtil::isa<InlineAsm>(Callee))
                    if (Options::EnableTypeCheck())
                    {
                        getTypeInference()->validateTypeCheck(cs);
                    }
                }
                //@}
            }
        }
    }

    svfir->totalSymNum = NodeIDAllocator::get()->endSymbolAllocation();
    if (Options::SymTabPrint())
    {
        llvmModuleSet()->dumpSymTable();
    }
}

void SymbolTableBuilder::collectSVFTypeInfo(const Value* val)
{
    Type *valType = val->getType();
    (void)getOrAddSVFTypeInfo(valType);
    if(isGepConstantExpr(val) || SVFUtil::isa<GetElementPtrInst>(val))
    {
        for (bridge_gep_iterator
                gi = bridge_gep_begin(SVFUtil::cast<User>(val)),
                ge = bridge_gep_end(SVFUtil::cast<User>(val));
                gi != ge; ++gi)
        {
            const Type* gepTy = *gi;
            (void)getOrAddSVFTypeInfo(gepTy);
        }
    }
}

/*!
 * Collect symbols, including value and object syms
 */
void SymbolTableBuilder::collectSym(const Value* val)
{

    //TODO: filter the non-pointer type // if (!SVFUtil::isa<PointerType>(val->getType()))  return;

    DBOUT(DMemModel,
          outs()
          << "collect sym from ##"
          << llvmModuleSet()->getSVFValue(val)->toString()
          << " \n");
    //TODO handle constant expression value here??
    handleCE(val);

    // create a value sym
    collectVal(val);

    collectSVFTypeInfo(val);
    collectSVFTypeInfo(LLVMUtil::getGlobalRep(val));

    // create an object If it is a heap, stack, global, function.
    if (isObject(val))
    {
        collectObj(val);
    }
}

/*!
 * Get value sym, if not available create a new one
 */
void SymbolTableBuilder::collectVal(const Value* val)
{
    // collect and record special sym here
    if (LLVMUtil::isNullPtrSym(val) || LLVMUtil::isBlackholeSym(val))
    {
        return;
    }
    LLVMModuleSet::ValueToIDMapTy::iterator iter = llvmModuleSet()->valSymMap.find(val);
    if (iter == llvmModuleSet()->valSymMap.end())
    {
        // create val sym and sym type
        NodeID id = NodeIDAllocator::get()->allocateValueId();
        llvmModuleSet()->valSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel,
              outs() << "create a new value sym " << id << "\n");
        ///  handle global constant expression here
        if (const GlobalVariable* globalVar = SVFUtil::dyn_cast<GlobalVariable>(val))
            handleGlobalCE(globalVar);
    }

    if (isConstantObjSym(val))
        collectObj(val);
}

/*!
 * Get memory object sym, if not available create a new one
 */
void SymbolTableBuilder::collectObj(const Value* val)
{
    val = LLVMUtil::getGlobalRep(val);
    LLVMModuleSet::ValueToIDMapTy::iterator iter = llvmModuleSet()->objSymMap.find(val);
    if (iter == llvmModuleSet()->objSymMap.end())
    {
        // if the object pointed by the pointer is a constant data (e.g., i32 0) or a global constant object (e.g. string)
        // then we treat them as one ConstantObj
        if (isConstantObjSym(val) && !Options::ModelConsts())
        {
            llvmModuleSet()->objSymMap.insert(std::make_pair(val, svfir->constantSymID()));
        }
        // otherwise, we will create an object for each abstract memory location
        else
        {
            // create obj sym and sym type
            NodeID id = NodeIDAllocator::get()->allocateObjectId();
            llvmModuleSet()->objSymMap.insert(std::make_pair(val, id));
            DBOUT(DMemModel,
                  outs() << "create a new obj sym " << id << "\n");

            // create a memory object
            ObjTypeInfo* ti = createObjTypeInfo(val);
            assert(svfir->objTypeInfoMap.find(id) == svfir->objTypeInfoMap.end());
            svfir->objTypeInfoMap[id] = ti;
        }
    }
}

/*!
 * Create unique return sym, if not available create a new one
 */
void SymbolTableBuilder::collectRet(const Function* val)
{

    LLVMModuleSet::FunToIDMapTy::iterator iter =
        llvmModuleSet()->returnSymMap.find(val);
    if (iter == llvmModuleSet()->returnSymMap.end())
    {
        NodeID id = NodeIDAllocator::get()->allocateValueId();
        llvmModuleSet()->returnSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel, outs() << "create a return sym " << id << "\n");
    }
}

/*!
 * Create vararg sym, if not available create a new one
 */
void SymbolTableBuilder::collectVararg(const Function* val)
{
    LLVMModuleSet::FunToIDMapTy::iterator iter =
        llvmModuleSet()->varargSymMap.find(val);
    if (iter == llvmModuleSet()->varargSymMap.end())
    {
        NodeID id = NodeIDAllocator::get()->allocateValueId();
        llvmModuleSet()->varargSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel, outs() << "create a vararg sym " << id << "\n");
    }
}

/*!
 * Handle constant expression
 */
void SymbolTableBuilder::handleCE(const Value* val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* ce = isGepConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << llvmModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);

            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            for (u32_t i = 0; i < ce->getNumOperands(); ++i)
            {
                collectVal(ce->getOperand(i));
                handleCE(ce->getOperand(i));
            }
        }
        else if (const ConstantExpr* ce = isCastConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << llvmModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isSelectConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << llvmModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            collectVal(ce->getOperand(1));
            collectVal(ce->getOperand(2));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
            handleCE(ce->getOperand(1));
            handleCE(ce->getOperand(2));
        }
        // if we meet a int2ptr, then it points-to black hole
        else if (const ConstantExpr* int2Ptrce = isInt2PtrConstantExpr(ref))
        {
            collectVal(int2Ptrce);
            const Constant* opnd = int2Ptrce->getOperand(0);
            handleCE(opnd);
        }
        else if (const ConstantExpr* ptr2Intce = isPtr2IntConstantExpr(ref))
        {
            collectVal(ptr2Intce);
            const Constant* opnd = ptr2Intce->getOperand(0);
            handleCE(opnd);
        }
        else if (isTruncConstantExpr(ref) || isCmpConstantExpr(ref))
        {
            collectVal(ref);
        }
        else if (isBinaryConstantExpr(ref))
        {
            collectVal(ref);
        }
        else if (isUnaryConstantExpr(ref))
        {
            // we don't handle unary constant expression like fneg(x) now
            collectVal(ref);
        }
        else if (SVFUtil::isa<ConstantAggregate>(ref))
        {
            // we don't handle constant aggregate like constant vectors
            collectVal(ref);
        }
        else
        {
            assert(!SVFUtil::isa<ConstantExpr>(val) &&
                   "we don't handle all other constant expression for now!");
            collectVal(ref);
        }
    }
}

/*!
 * Handle global constant expression
 */
void SymbolTableBuilder::handleGlobalCE(const GlobalVariable* G)
{
    assert(G);

    //The type this global points to
    const Type* T = G->getValueType();
    bool is_array = 0;
    //An array is considered a single variable of its type.
    while (const ArrayType* AT = SVFUtil::dyn_cast<ArrayType>(T))
    {
        T = AT->getElementType();
        is_array = true;
    }

    if (SVFUtil::isa<StructType>(T))
    {
        //A struct may be used in constant GEP expr.
        for (const User* user : G->users())
        {
            handleCE(user);
        }
    }
    else if (is_array)
    {
        for (const User* user : G->users())
        {
            handleCE(user);
        }
    }

    if (G->hasInitializer())
    {
        handleGlobalInitializerCE(G->getInitializer());
    }
}

/*!
 * Handle global variable initialization
 */
void SymbolTableBuilder::handleGlobalInitializerCE(const Constant* C)
{

    if (C->getType()->isSingleValueType())
    {
        if (const ConstantExpr* E = SVFUtil::dyn_cast<ConstantExpr>(C))
        {
            handleCE(E);
        }
        else
        {
            collectVal(C);
        }
    }
    else if (SVFUtil::isa<ConstantArray>(C))
    {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)));
        }
    }
    else if (SVFUtil::isa<ConstantStruct>(C))
    {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)));
        }
    }
    else if(const ConstantData* data = SVFUtil::dyn_cast<ConstantData>(C))
    {
        if (Options::ModelConsts())
        {
            if (const ConstantDataSequential* seq =
                        SVFUtil::dyn_cast<ConstantDataSequential>(data))
            {
                for(u32_t i = 0; i < seq->getNumElements(); i++)
                {
                    const Constant* ct = seq->getElementAsConstant(i);
                    handleGlobalInitializerCE(ct);
                }
            }
            else
            {
                assert(
                    (SVFUtil::isa<ConstantAggregateZero, UndefValue>(data)) &&
                    "Single value type data should have been handled!");
            }
        }
    }
    else
    {
        //TODO:assert(SVFUtil::isa<ConstantVector>(C),"what else do we have");
    }
}

ObjTypeInference *SymbolTableBuilder::getTypeInference()
{
    return llvmModuleSet()->getTypeInference();
}


const Type* SymbolTableBuilder::inferObjType(const Value *startValue)
{
    return getTypeInference()->inferObjType(startValue);
}

/*!
 * Return the type of the object from a heap allocation
 */
const Type* SymbolTableBuilder::inferTypeOfHeapObjOrStaticObj(const Instruction *inst)
{
    const Value* startValue = inst;
    const PointerType *originalPType = SVFUtil::dyn_cast<PointerType>(inst->getType());
    const Type* inferedType = nullptr;
    assert(originalPType && "empty type?");
    if(LLVMUtil::isHeapAllocExtCallViaRet(inst))
    {
        if(const Value* v = getFirstUseViaCastInst(inst))
        {
            if (const PointerType *newTy = SVFUtil::dyn_cast<PointerType>(v->getType()))
            {
                originalPType = newTy;
            }
        }
        inferedType = inferObjType(startValue);
    }
    else if(LLVMUtil::isHeapAllocExtCallViaArg(inst))
    {
        const CallBase* cs = LLVMUtil::getLLVMCallSite(inst);
        u32_t arg_pos = LLVMUtil::getHeapAllocHoldingArgPosition(cs->getCalledFunction());
        const Value* arg = cs->getArgOperand(arg_pos);
        originalPType = SVFUtil::dyn_cast<PointerType>(arg->getType());
        inferedType = inferObjType(startValue = arg);
    }
    else
    {
        assert( false && "not a heap allocation instruction?");
    }

    getTypeInference()->typeSizeDiffTest(originalPType, inferedType, startValue);

    return inferedType;
}

/*
 * Initial the memory object here
 */
ObjTypeInfo* SymbolTableBuilder::createObjTypeInfo(const Value* val)
{
    const Type* objTy = nullptr;

    const Instruction* I = SVFUtil::dyn_cast<Instruction>(val);

    // We consider two types of objects:
    // (1) A heap/static object from a callsite
    if (I && isNonInstricCallSite(I))
    {
        objTy = inferTypeOfHeapObjOrStaticObj(I);
    }
    // (2) Other objects (e.g., alloca, global, etc.)
    else
    {
        if (SVFUtil::isa<PointerType>(val->getType()))
        {
            if (const AllocaInst *allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
            {
                // get the type of the allocated memory
                // e.g., for `%retval = alloca i64, align 4`, we return i64
                objTy = allocaInst->getAllocatedType();
            }
            else if (const GlobalValue *global = SVFUtil::dyn_cast<GlobalValue>(val))
            {
                // get the pointee type of the global pointer (begins with @ symbol in llvm)
                objTy = global->getValueType();
            }
            else
            {
                SVFUtil::errs() << dumpValueAndDbgInfo(val) << "\n";
                assert(false && "not an allocation or global?");
            }
        }
    }

    if (objTy)
    {
        (void) getOrAddSVFTypeInfo(objTy);
        ObjTypeInfo* typeInfo = new ObjTypeInfo(
            llvmModuleSet()->getSVFType(objTy),
            Options::MaxFieldLimit());
        initTypeInfo(typeInfo,val, objTy);
        return typeInfo;
    }
    else
    {
        writeWrnMsg("try to create an object with a non-pointer type.");
        writeWrnMsg(val->getName().str());
        writeWrnMsg("(" + getSourceLoc(val) + ")");
        if (isConstantObjSym(val))
        {
            ObjTypeInfo* typeInfo = new ObjTypeInfo(
                llvmModuleSet()->getSVFType(val->getType()),
                0);
            initTypeInfo(typeInfo,val, val->getType());
            return typeInfo;
        }
        else
        {
            assert(false && "Memory object must be either (1) held by a pointer-typed ref value or (2) a constant value (e.g., 10).");
            abort();
        }
    }
}

/*!
 * Analyse types of all flattened fields of this object
 */
void SymbolTableBuilder::analyzeObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    const Type *elemTy = llvmModuleSet()->getLLVMType(typeinfo->getType());
    // Find the inter nested array element
    while (const ArrayType* AT = SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        elemTy = AT->getElementType();
        if (SVFUtil::isa<GlobalVariable>(val) &&
                SVFUtil::cast<GlobalVariable>(val)->hasInitializer() &&
                SVFUtil::isa<ConstantArray>(
                    SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_ARRAY_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_ARRAY_OBJ);
    }
    if (SVFUtil::isa<StructType>(elemTy))
    {
        if (SVFUtil::isa<GlobalVariable>(val) &&
                SVFUtil::cast<GlobalVariable>(val)->hasInitializer() &&
                SVFUtil::isa<ConstantStruct>(
                    SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_STRUCT_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_STRUCT_OBJ);
    }
}

/*!
 * Analyze byte size of heap alloc function (e.g. malloc/calloc/...)
 * 1) __attribute__((annotate("ALLOC_HEAP_RET"), annotate("AllocSize:Arg0")))
     void* safe_malloc(unsigned long size).
     Byte Size is the size(Arg0)
   2)__attribute__((annotate("ALLOC_HEAP_RET"), annotate("AllocSize:Arg0*Arg1")))
    char* safecalloc(int a, int b)
    Byte Size is a(Arg0) * b(Arg1)
   3)__attribute__((annotate("ALLOC_HEAP_RET"), annotate("UNKNOWN")))
    void* __sysv_signal(int a, void *b)
    Byte Size is Unknown
    If all required arg values are constant, byte Size is also constant,
    otherwise return ByteSize 0
 */
u32_t SymbolTableBuilder::analyzeHeapAllocByteSize(const Value* val)
{
    if(const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(val))
    {
        if (const llvm::Function* calledFunction =
                    callInst->getCalledFunction())
        {
            std::vector<const Value*> args;
            // Heap alloc functions have annoation like "AllocSize:Arg1"
            for (std::string annotation : llvmModuleSet()->getExtFuncAnnotations(calledFunction))
            {
                if (annotation.find("AllocSize:") != std::string::npos)
                {
                    std::string allocSize = annotation.substr(10);
                    std::stringstream ss(allocSize);
                    std::string token;
                    // Analyaze annotation string and attract Arg list
                    while (std::getline(ss, token, '*'))
                    {
                        if (token.rfind("Arg", 0) == 0)
                        {
                            u32_t argIndex;
                            std::istringstream(token.substr(3)) >> argIndex;
                            if (argIndex < callInst->getNumOperands() - 1)
                            {
                                args.push_back(
                                    callInst->getArgOperand(argIndex));
                            }
                        }
                    }
                }
            }
            u64_t product = 1;
            if (args.size() > 0)
            {
                // for annotations like "AllocSize:Arg0*Arg1"
                for (const llvm::Value* arg : args)
                {
                    if (const llvm::ConstantInt* constIntArg =
                                llvm::dyn_cast<llvm::ConstantInt>(arg))
                    {
                        // Multiply the constant Value if all Args are const
                        product *= LLVMUtil::getIntegerValue(constIntArg).second;
                    }
                    else
                    {
                        // if Arg list has non-const value, return 0 to indicate it is non const byte size
                        return 0;
                    }
                }
                // If all the Args are const, return product
                return product;
            }
            else
            {
                // for annotations like "AllocSize:UNKNOWN"
                return 0;
            }
        }
    }
    // if it is not CallInst or CallInst has no CalledFunction, return 0 to indicate it is non const byte size
    return 0;
}

/*!
 * Analyse types of heap and static objects
 */
u32_t SymbolTableBuilder::analyzeHeapObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    analyzeObjType(typeinfo, val);
    const Type* objTy = llvmModuleSet()->getLLVMType(typeinfo->getType());
    if(SVFUtil::isa<ArrayType>(objTy))
        return getNumOfElements(objTy);
    else if(const StructType* st = SVFUtil::dyn_cast<StructType>(objTy))
    {
        /// For an C++ class, it can have variant elements depending on the vtable size,
        /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as default PointerType
        if(cppUtil::classTyHasVTable(st))
            typeinfo->resetTypeForHeapStaticObj(llvmModuleSet()->getSVFType(
                                                    llvmModuleSet()->getTypeInference()->ptrType()));
        else
            return getNumOfElements(objTy);
    }
    return typeinfo->getMaxFieldOffsetLimit();
}

/*!
 * Analyse types of heap and static objects
 */
void SymbolTableBuilder::analyzeStaticObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    if(const Value* castUse = getFirstUseViaCastInst(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STATIC_OBJ);
        analyzeObjType(typeinfo,castUse);
    }
    else
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    }
}

/*!
 * Initialize the type info of an object
 */
void SymbolTableBuilder::initTypeInfo(ObjTypeInfo* typeinfo, const Value* val,
                                      const Type* objTy)
{

    u32_t elemNum = 1;
    // init byteSize = 0, If byteSize is changed in the following process,
    // it means that ObjTypeInfo has a Constant Byte Size
    u32_t byteSize = 0;
    // Global variable
    // if val is Function Obj, byteSize is not set
    if (SVFUtil::isa<Function>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::FUNCTION_OBJ);
        analyzeObjType(typeinfo,val);
        elemNum = 0;
    }
    /// if val is AllocaInst, byteSize is Type's LLVM ByteSize * ArraySize
    /// e.g. alloc i32, 10. byteSize is 4 (i32's size) * 10 (ArraySize) = 40
    else if(const AllocaInst* allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STACK_OBJ);
        analyzeObjType(typeinfo,val);
        /// This is for `alloca <ty> <NumElements>`. For example, `alloca i64 3` allocates 3 i64 on the stack (objSize=3)
        /// In most cases, `NumElements` is not specified in the instruction, which means there is only one element (objSize=1).
        if(const ConstantInt* sz = SVFUtil::dyn_cast<ConstantInt>(allocaInst->getArraySize()))
        {
            elemNum = LLVMUtil::getIntegerValue(sz).second * getNumOfElements(objTy);
            byteSize = LLVMUtil::getIntegerValue(sz).second * typeinfo->getType()->getByteSize();
        }
        /// if ArraySize is not constant, byteSize is not static determined.
        else
        {
            elemNum = getNumOfElements(objTy);
            byteSize = 0;
        }
    }
    /// if val is GlobalVar, byteSize is Type's LLVM ByteSize
    /// All GlobalVariable must have constant size
    else if(SVFUtil::isa<GlobalVariable>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::GLOBVAR_OBJ);
        if(isConstantObjSym(val))
            typeinfo->setFlag(ObjTypeInfo::CONST_GLOBAL_OBJ);
        analyzeObjType(typeinfo,val);
        elemNum = getNumOfElements(objTy);
        byteSize = typeinfo->getType()->getByteSize();
    }
    /// if val is heap alloc
    else if (SVFUtil::isa<Instruction>(val) &&
             LLVMUtil::isHeapAllocExtCall(
                 SVFUtil::cast<Instruction>(val)))
    {
        elemNum = analyzeHeapObjType(typeinfo,val);
        // analyze heap alloc like (malloc/calloc/...), the alloc functions have
        // annotation like "AllocSize:Arg1". Please refer to extapi.c.
        // e.g. calloc(4, 10), annotation is "AllocSize:Arg0*Arg1",
        // it means byteSize = 4 (Arg0) * 10 (Arg1) = 40
        byteSize = analyzeHeapAllocByteSize(val);
    }
    else if(ArgInProgEntryFunction(val))
    {
        analyzeStaticObjType(typeinfo,val);
        // user input data, label its field as infinite here
        elemNum = typeinfo->getMaxFieldOffsetLimit();
        byteSize = typeinfo->getType()->getByteSize();
    }
    else if(LLVMUtil::isConstDataOrAggData(val))
    {
        typeinfo->setFlag(ObjTypeInfo::CONST_DATA);
        elemNum = getNumOfFlattenElements(val->getType());
        byteSize = typeinfo->getType()->getByteSize();
    }
    else
    {
        assert("what other object do we have??");
        abort();
    }

    // Reset maxOffsetLimit if it is over the total fieldNum of this object
    if(typeinfo->getMaxFieldOffsetLimit() > elemNum)
        typeinfo->setNumOfElements(elemNum);

    // set ByteSize. If ByteSize > 0, this typeinfo has constant type.
    // If ByteSize == 0, this typeinfo has 1) zero byte 2) non-const byte size
    // If ByteSize>MaxFieldLimit, set MaxFieldLimit to the byteSize;
    byteSize = Options::MaxFieldLimit() > byteSize? byteSize: Options::MaxFieldLimit();
    typeinfo->setByteSizeOfObj(byteSize);
}

/*!
 * Return size of this Object
 */
u32_t SymbolTableBuilder::getNumOfElements(const Type* ety)
{
    assert(ety && "type is null?");
    u32_t numOfFields = 1;
    if (SVFUtil::isa<StructType, ArrayType>(ety))
    {
        numOfFields = getNumOfFlattenElements(ety);
    }
    return numOfFields;
}

/// Number of flattened elements of an array or struct
u32_t SymbolTableBuilder::getNumOfFlattenElements(const Type* T)
{
    if(Options::ModelArrays())
        return getOrAddSVFTypeInfo(T)->getNumOfFlattenElements();
    else
        return getOrAddSVFTypeInfo(T)->getNumOfFlattenFields();
}

StInfo* SymbolTableBuilder::getOrAddSVFTypeInfo(const Type* T)
{
    return llvmModuleSet()->getSVFType(T)->getTypeInfo();
}
