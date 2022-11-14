//===- SVFUtil.cpp -- Analysis helper functions----------------------------//
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
 * LLVMUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/LLVMUtil.h"
#include "MemoryModel/SymbolTableInfo.h"

using namespace SVF;

/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool LLVMUtil::isObject(const Value*  ref)
{
    bool createobj = false;
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isStaticExtCall(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(ref))) )
        createobj = true;
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isHeapAllocExtCallViaRet(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(ref))))
        createobj = true;
    if (SVFUtil::isa<GlobalVariable>(ref))
        createobj = true;
    if (SVFUtil::isa<Function>(ref) || SVFUtil::isa<AllocaInst>(ref) )
        createobj = true;

    return createobj;
}

/*!
 * Check whether this value points-to a constant object
 */
bool LLVMUtil::isConstantObjSym(const Value* val)
{
    if (const GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (cppUtil::isValVtbl(v))
            return false;
        else if (!v->hasInitializer())
        {
            if(v->isExternalLinkage(v->getLinkage()))
                return false;
            else
                return true;
        }
        else
        {
            StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(v->getInitializer()->getType());
            const std::vector<const Type*> &fields = stInfo->getFlattenFieldTypes();
            for (std::vector<const Type*>::const_iterator it = fields.begin(), eit = fields.end(); it != eit; ++it)
            {
                const Type *elemTy = *it;
                assert(!SVFUtil::isa<FunctionType>(elemTy) && "Initializer of a global is a function?");
                if (SVFUtil::isa<PointerType>(elemTy))
                    return false;
            }

            return v->isConstant();
        }
    }
    return SVFUtil::isConstantData(val);
}

/*!
 * Return reachable bbs from function entry
 */
void LLVMUtil::getFunReachableBBs (const SVFFunction* svfFun, std::vector<const SVFBasicBlock*> &reachableBBs)
{
    assert(!SVFUtil::isExtCall(svfFun) && "The calling function cannot be an external function.");
    //initial DominatorTree
    DominatorTree dt;
    dt.recalculate(const_cast<Function&>(*svfFun->getLLVMFun()));

    Set<const BasicBlock*> visited;
    std::vector<const BasicBlock*> bbVec;
    bbVec.push_back(&svfFun->getLLVMFun()->getEntryBlock());
    while(!bbVec.empty())
    {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        const SVFBasicBlock* svfbb = LLVMModuleSet::getLLVMModuleSet()->getSVFBasicBlock(bb);
        reachableBBs.push_back(svfbb);
        if(DomTreeNode *dtNode = dt.getNode(const_cast<BasicBlock*>(bb)))
        {
            for (DomTreeNode::iterator DI = dtNode->begin(), DE = dtNode->end();
                    DI != DE; ++DI)
            {
                const BasicBlock* succbb = (*DI)->getBlock();
                if(visited.find(succbb)==visited.end())
                    visited.insert(succbb);
                else
                    continue;
                bbVec.push_back(succbb);
            }
        }
    }
}

/*!
 * Return true if the function has a return instruction reachable from function entry
 */
bool LLVMUtil::functionDoesNotRet (const Function*  fun)
{
    const SVFFunction* svffun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
    if (SVFUtil::isExtCall(svffun))
    {
        return fun->getReturnType()->isVoidTy();
    }
    std::vector<const BasicBlock*> bbVec;
    Set<const BasicBlock*> visited;
    bbVec.push_back(&fun->getEntryBlock());
    while(!bbVec.empty())
    {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        for (BasicBlock::const_iterator it = bb->begin(), eit = bb->end();
                it != eit; ++it)
        {
            if(SVFUtil::isa<ReturnInst>(*it))
                return false;
        }

        for (succ_const_iterator sit = succ_begin(bb), esit = succ_end(bb);
                sit != esit; ++sit)
        {
            const BasicBlock* succbb = (*sit);
            if(visited.find(succbb)==visited.end())
                visited.insert(succbb);
            else
                continue;
            bbVec.push_back(succbb);
        }
    }
//    if(isProgEntryFunction(fun)==false) {
//        writeWrnMsg(fun->getName().str() + " does not have return");
//    }
    return true;
}

/*!
 * Return true if this is a function without any possible caller
 */
bool LLVMUtil::isUncalledFunction (const Function*  fun)
{
    if(fun->hasAddressTaken())
        return false;
    if(SVFUtil::isProgEntryFunction(fun))
        return false;
    for (Value::const_user_iterator i = fun->user_begin(), e = fun->user_end(); i != e; ++i)
    {
        if (SVFUtil::isCallSite(*i))
            return false;
    }
    if (LLVMModuleSet::getLLVMModuleSet()->hasDeclaration(fun))
    {
        const LLVMModuleSet::FunctionSetType &decls = LLVMModuleSet::getLLVMModuleSet()->getDeclaration(fun);
        for (LLVMModuleSet::FunctionSetType::const_iterator it = decls.begin(),
                eit = decls.end(); it != eit; ++it)
        {
            const Function* decl = *it;
            if(decl->hasAddressTaken())
                return false;
            for (Value::const_user_iterator i = decl->user_begin(), e = decl->user_end(); i != e; ++i)
            {
                if (SVFUtil::isCallSite(*i))
                    return false;
            }
        }
    }
    return true;
}

/*!
 * Return true if this is a value in a dead function (function without any caller)
 */
bool LLVMUtil::isPtrInUncalledFunction (const Value*  value)
{
    if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(value))
    {
        if(isUncalledFunction(inst->getParent()->getParent()))
            return true;
    }
    else if(const Argument* arg = SVFUtil::dyn_cast<Argument>(value))
    {
        if(isUncalledFunction(arg->getParent()))
            return true;
    }
    return false;
}

/*!
 * Strip constant casts
 */
const Value*  LLVMUtil::stripConstantCasts(const Value* val)
{
    if (SVFUtil::isa<GlobalValue>(val) || isInt2PtrConstantExpr(val))
        return val;
    else if (const ConstantExpr *CE = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (Instruction::isCast(CE->getOpcode()))
            return stripConstantCasts(CE->getOperand(0));
    }
    return val;
}

/*!
 * Strip all casts
 */
const Value*  LLVMUtil::stripAllCasts(const Value* val)
{
    while (true)
    {
        if (const CastInst *ci = SVFUtil::dyn_cast<CastInst>(val))
        {
            val = ci->getOperand(0);
        }
        else if (const ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(val))
        {
            if(ce->isCast())
                val = ce->getOperand(0);
            else
                return val;
        }
        else
        {
            return val;
        }
    }
    return nullptr;
}

/// Get the next instructions following control flow
void LLVMUtil::getNextInsts(const SVFInstruction* curInst, std::vector<const SVFInstruction*>& instList)
{
    if (!curInst->isTerminator())
    {
        const Instruction* nextInst = curInst->getLLVMInstruction()->getNextNode();
        const SVFInstruction* svfNextInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(nextInst);
        if (SVFUtil::isIntrinsicInst(svfNextInst))
            getNextInsts(svfNextInst, instList);
        else
            instList.push_back(svfNextInst);
    }
    else
    {
        const BasicBlock* BB = curInst->getParent()->getLLVMBasicBlock();
        // Visit all successors of BB in the CFG
        for (succ_const_iterator it = succ_begin(BB), ie = succ_end(BB); it != ie; ++it)
        {
            const Instruction* nextInst = &((*it)->front());
            const SVFInstruction* svfNextInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(nextInst);
            if (SVFUtil::isIntrinsicInst(svfNextInst))
                getNextInsts(svfNextInst, instList);
            else
                instList.push_back(svfNextInst);
        }
    }
}


/// Get the previous instructions following control flow
void LLVMUtil::getPrevInsts(const SVFInstruction* curInst, std::vector<const SVFInstruction*>& instList)
{
    
    const SVFInstruction* entryInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(&(curInst->getParent()->getLLVMBasicBlock()->front()));
    if (curInst != entryInst)
    {
        const Instruction* prevInst = curInst->getLLVMInstruction()->getPrevNode();
        const SVFInstruction* svfPrevInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(prevInst);
        if (SVFUtil::isIntrinsicInst(svfPrevInst))
            getPrevInsts(svfPrevInst, instList);
        else
            instList.push_back(svfPrevInst);
    }
    else
    {
        const BasicBlock* BB = curInst->getParent()->getLLVMBasicBlock();
        // Visit all successors of BB in the CFG
        for (const_pred_iterator it = pred_begin(BB), ie = pred_end(BB); it != ie; ++it)
        {
            const Instruction* prevInst = &((*it)->back());
            const SVFInstruction* svfPrevInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(prevInst);
            if (SVFUtil::isIntrinsicInst(svfPrevInst))
                getPrevInsts(svfPrevInst, instList);
            else
                instList.push_back(svfPrevInst);
        }
    }
}

/*
 * Get the first dominated cast instruction for heap allocations since they typically come from void* (i8*)
 * for example, %4 = call align 16 i8* @malloc(i64 10); %5 = bitcast i8* %4 to i32*
 * return %5 whose type is i32* but not %4 whose type is i8*
 */
const Value* LLVMUtil::getUniqueUseViaCastInst(const Value* val)
{
    const PointerType * type = SVFUtil::dyn_cast<PointerType>(val->getType());
    assert(type && "this value should be a pointer type!");
    /// If type is void* (i8*) and val is only used at a bitcast instruction
    if (IntegerType *IT = SVFUtil::dyn_cast<IntegerType>(getPtrElementType(type)))
    {
        if (IT->getBitWidth() == 8 && val->getNumUses()==1)
        {
            const Use *u = &*val->use_begin();
            return SVFUtil::dyn_cast<BitCastInst>(u->getUser());
        }
    }
    return nullptr;
}

/*!
 * Return the type of the object from a heap allocation
 */
const Type* LLVMUtil::getTypeOfHeapAlloc(const SVFInstruction *inst)
{
    const PointerType* type = SVFUtil::dyn_cast<PointerType>(inst->getType());

    if(SVFUtil::isHeapAllocExtCallViaRet(inst))
    {
        if(const Value* v = getUniqueUseViaCastInst(inst->getLLVMInstruction()))
        {
            if(const PointerType* newTy = SVFUtil::dyn_cast<PointerType>(v->getType()))
                type = newTy;
        }
    }
    else if(SVFUtil::isHeapAllocExtCallViaArg(inst))
    {
        CallSite cs = SVFUtil::getLLVMCallSite(inst);
        int arg_pos = SVFUtil::getHeapAllocHoldingArgPosition(SVFUtil::getCallee(cs));
        const Value* arg = cs.getArgument(arg_pos);
        type = SVFUtil::dyn_cast<PointerType>(arg->getType());
    }
    else
    {
        assert( false && "not a heap allocation instruction?");
    }

    assert(type && "not a pointer type?");
    return getPtrElementType(type);
}

/*!
 * Get the num of BB's predecessors
 */
u32_t LLVMUtil::getBBPredecessorNum(const BasicBlock* BB)
{
    u32_t num = 0;
    for (const_pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
        num++;
    return num;
}

/*
 * Reference functions:
 * llvm::parseIRFile (lib/IRReader/IRReader.cpp)
 * llvm::parseIR (lib/IRReader/IRReader.cpp)
 */
bool LLVMUtil::isIRFile(const std::string &filename)
{
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(filename);
    if (FileOrErr.getError())
        return false;
    llvm::MemoryBufferRef Buffer = FileOrErr.get()->getMemBufferRef();
    const unsigned char *bufferStart =
        (const unsigned char *)Buffer.getBufferStart();
    const unsigned char *bufferEnd =
        (const unsigned char *)Buffer.getBufferEnd();
    return llvm::isBitcode(bufferStart, bufferEnd) ? true :
           Buffer.getBuffer().startswith("; ModuleID =");
}


/// Get the names of all modules into a vector
/// And process arguments
void LLVMUtil::processArguments(int argc, char **argv, int &arg_num, char **arg_value,
                                std::vector<std::string> &moduleNameVec)
{
    bool first_ir_file = true;
    for (int i = 0; i < argc; ++i)
    {
        std::string argument(argv[i]);
        if (LLVMUtil::isIRFile(argument))
        {
            if (find(moduleNameVec.begin(), moduleNameVec.end(), argument)
                    == moduleNameVec.end())
                moduleNameVec.push_back(argument);
            if (first_ir_file)
            {
                arg_value[arg_num] = argv[i];
                arg_num++;
                first_ir_file = false;
            }
        }
        else
        {
            arg_value[arg_num] = argv[i];
            arg_num++;
        }
    }
}


u32_t LLVMUtil::getTypeSizeInBytes(const Type* type)
{

    // if the type has size then simply return it, otherwise just return 0
    if(type->isSized())
        return getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule())->getTypeStoreSize(const_cast<Type*>(type));
    else
        return 0;
}

u32_t LLVMUtil::getTypeSizeInBytes(const StructType *sty, u32_t field_idx)
{

    const StructLayout *stTySL = getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule())->getStructLayout( const_cast<StructType *>(sty) );
    /// if this struct type does not have any element, i.e., opaque
    if(sty->isOpaque())
        return 0;
    else
        return stTySL->getElementOffset(field_idx);
}

