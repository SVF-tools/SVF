//===- SVFUtil.cpp -- Analysis helper functions----------------------------//
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
 * LLVMUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "SVFIR/SymbolTableInfo.h"
#include <sstream>
#include <llvm/Support/raw_ostream.h>

using namespace SVF;

// label for global vtbl value before demangle
const std::string vtblLabelBeforeDemangle = "_ZTV";

// label for virtual functions
const std::string vfunPreLabel = "_Z";

const std::string clsName = "class.";
const std::string structName = "struct.";


/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool LLVMUtil::isObject(const Value*  ref)
{
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isStaticExtCall(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(ref))) )
        return true;
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isHeapAllocExtCallViaRet(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(ref))))
        return true;
    if (SVFUtil::isa<GlobalVariable>(ref))
        return true;
    if (SVFUtil::isa<Function, AllocaInst>(ref))
        return true;

    return false;
}

/*!
 * Return reachable bbs from function entry
 */
void LLVMUtil::getFunReachableBBs (const Function* fun, std::vector<const SVFBasicBlock*> &reachableBBs)
{
    assert(!SVFUtil::isExtCall(LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun)) && "The calling function cannot be an external function.");
    //initial DominatorTree
    DominatorTree dt;
    dt.recalculate(const_cast<Function&>(*fun));

    Set<const BasicBlock*> visited;
    std::vector<const BasicBlock*> bbVec;
    bbVec.push_back(&fun->getEntryBlock());
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
    return true;
}

/*!
 * Return true if this is a function without any possible caller
 */
bool LLVMUtil::isUncalledFunction (const Function*  fun)
{
    if(fun->hasAddressTaken())
        return false;
    if (LLVMUtil::isProgEntryFunction(fun))
        return false;
    for (Value::const_user_iterator i = fun->user_begin(), e = fun->user_end(); i != e; ++i)
    {
        if (LLVMUtil::isCallSite(*i))
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
                if (LLVMUtil::isCallSite(*i))
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

bool LLVMUtil::isIntrinsicFun(const Function* func)
{
    if (func && (func->getIntrinsicID() == llvm::Intrinsic::donothing ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_addr ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_declare ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_label ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_value))
    {
        return true;
    }
    return false;
}

/// Return true if it is an intrinsic instruction
bool LLVMUtil::isIntrinsicInst(const Instruction* inst)
{
    if (const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
    {
        const Function* func = call->getCalledFunction();
        if (isIntrinsicFun(func))
        {
            return true;
        }
    }
    return false;
}

/*!
 * Strip constant casts
 */
const Value* LLVMUtil::stripConstantCasts(const Value* val)
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

void LLVMUtil::viewCFG(const Function* fun)
{
    if (fun != nullptr)
    {
        fun->viewCFG();
    }
}

void LLVMUtil::viewCFGOnly(const Function* fun)
{
    if (fun != nullptr)
    {
        fun->viewCFGOnly();
    }
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
void LLVMUtil::getNextInsts(const Instruction* curInst, std::vector<const SVFInstruction*>& instList)
{
    if (!curInst->isTerminator())
    {
        const Instruction* nextInst = curInst->getNextNode();
        const SVFInstruction* svfNextInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(nextInst);
        if (LLVMUtil::isIntrinsicInst(nextInst))
            getNextInsts(nextInst, instList);
        else
            instList.push_back(svfNextInst);
    }
    else
    {
        const BasicBlock* BB = curInst->getParent();
        // Visit all successors of BB in the CFG
        for (succ_const_iterator it = succ_begin(BB), ie = succ_end(BB); it != ie; ++it)
        {
            const Instruction* nextInst = &((*it)->front());
            const SVFInstruction* svfNextInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(nextInst);
            if (LLVMUtil::isIntrinsicInst(nextInst))
                getNextInsts(nextInst, instList);
            else
                instList.push_back(svfNextInst);
        }
    }
}


/// Get the previous instructions following control flow
void LLVMUtil::getPrevInsts(const Instruction* curInst, std::vector<const SVFInstruction*>& instList)
{

    if (curInst != &(curInst->getParent()->front()))
    {
        const Instruction* prevInst = curInst->getPrevNode();
        const SVFInstruction* svfPrevInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(prevInst);
        if (LLVMUtil::isIntrinsicInst(prevInst))
            getPrevInsts(prevInst, instList);
        else
            instList.push_back(svfPrevInst);
    }
    else
    {
        const BasicBlock* BB = curInst->getParent();
        // Visit all successors of BB in the CFG
        for (const_pred_iterator it = pred_begin(BB), ie = pred_end(BB); it != ie; ++it)
        {
            const Instruction* prevInst = &((*it)->back());
            const SVFInstruction* svfPrevInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(prevInst);
            if (LLVMUtil::isIntrinsicInst(prevInst))
                getPrevInsts(prevInst, instList);
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
const Type* LLVMUtil::getTypeOfHeapAlloc(const Instruction *inst)
{
    const PointerType* type = SVFUtil::dyn_cast<PointerType>(inst->getType());
    const SVFInstruction* svfinst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(inst);
    if(SVFUtil::isHeapAllocExtCallViaRet(svfinst))
    {
        if(const Value* v = getUniqueUseViaCastInst(inst))
        {
            if(const PointerType* newTy = SVFUtil::dyn_cast<PointerType>(v->getType()))
                type = newTy;
        }
    }
    else if(SVFUtil::isHeapAllocExtCallViaArg(svfinst))
    {
        const CallBase* cs = LLVMUtil::getLLVMCallSite(inst);
        int arg_pos = SVFUtil::getHeapAllocHoldingArgPosition(SVFUtil::getSVFCallSite(svfinst));
        const Value* arg = cs->getArgOperand(arg_pos);
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

/*!
 * Get the meta data (line number and file name) info of a LLVM value
 */
const std::string LLVMUtil::getSourceLoc(const Value* val )
{
    if(val==nullptr)  return "{ empty val }";

    std::string str;
    std::stringstream rawstr(str);
    rawstr << "{ ";

    if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(val))
    {
        if (SVFUtil::isa<AllocaInst>(inst))
        {
            for (llvm::DbgInfoIntrinsic *DII : FindDbgAddrUses(const_cast<Instruction*>(inst)))
            {
                if (llvm::DbgDeclareInst *DDI = SVFUtil::dyn_cast<llvm::DbgDeclareInst>(DII))
                {
                    llvm::DIVariable *DIVar = SVFUtil::cast<llvm::DIVariable>(DDI->getVariable());
                    rawstr << "\"ln\": " << DIVar->getLine() << ", \"fl\": \"" << DIVar->getFilename().str() << "\"";
                    break;
                }
            }
        }
        else if (MDNode *N = inst->getMetadata("dbg"))   // Here I is an LLVM instruction
        {
            llvm::DILocation* Loc = SVFUtil::cast<llvm::DILocation>(N);                   // DILocation is in DebugInfo.h
            unsigned Line = Loc->getLine();
            unsigned Column = Loc->getColumn();
            std::string File = Loc->getFilename().str();
            //StringRef Dir = Loc.getDirectory();
            if(File.empty() || Line == 0)
            {
                auto inlineLoc = Loc->getInlinedAt();
                if(inlineLoc)
                {
                    Line = inlineLoc->getLine();
                    Column = inlineLoc->getColumn();
                    File = inlineLoc->getFilename().str();
                }
            }
            rawstr << "\"ln\": " << Line << ", \"cl\": " << Column << ", \"fl\": \"" << File << "\"";
        }
    }
    else if (const Argument* argument = SVFUtil::dyn_cast<Argument>(val))
    {
        if (argument->getArgNo()%10 == 1)
            rawstr << argument->getArgNo() << "st";
        else if (argument->getArgNo()%10 == 2)
            rawstr << argument->getArgNo() << "nd";
        else if (argument->getArgNo()%10 == 3)
            rawstr << argument->getArgNo() << "rd";
        else
            rawstr << argument->getArgNo() << "th";
        rawstr << " arg " << argument->getParent()->getName().str() << " "
               << getSourceLocOfFunction(argument->getParent());
    }
    else if (const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        rawstr << "Glob ";
        NamedMDNode* CU_Nodes = gvar->getParent()->getNamedMetadata("llvm.dbg.cu");
        if(CU_Nodes)
        {
            for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i)
            {
                llvm::DICompileUnit *CUNode = SVFUtil::cast<llvm::DICompileUnit>(CU_Nodes->getOperand(i));
                for (llvm::DIGlobalVariableExpression *GV : CUNode->getGlobalVariables())
                {
                    llvm::DIGlobalVariable * DGV = GV->getVariable();

                    if(DGV->getName() == gvar->getName())
                    {
                        rawstr << "\"ln\": " << DGV->getLine() << ", \"fl\": \"" << DGV->getFilename().str() << "\"";
                    }

                }
            }
        }
    }
    else if (const Function* func = SVFUtil::dyn_cast<Function>(val))
    {
        rawstr << getSourceLocOfFunction(func);
    }
    else if (const BasicBlock* bb = SVFUtil::dyn_cast<BasicBlock>(val))
    {
        rawstr << "\"basic block\": " << bb->getName().str() << ", \"location\": " << getSourceLoc(bb->getFirstNonPHI());
    }
    else if(LLVMUtil::isConstDataOrAggData(val))
    {
        rawstr << "constant data";
    }
    else
    {
        rawstr << "N/A";
    }
    rawstr << " }";

    if(rawstr.str()=="{  }")
        return "";
    return rawstr.str();
}


/*!
 * Get source code line number of a function according to debug info
 */
const std::string LLVMUtil::getSourceLocOfFunction(const Function* F)
{
    std::string str;
    std::stringstream rawstr(str);
    /*
     * https://reviews.llvm.org/D18074?id=50385
     * looks like the relevant
     */
    if (llvm::DISubprogram *SP =  F->getSubprogram())
    {
        if (SP->describes(F))
            rawstr << "\"ln\": " << SP->getLine() << ", \"file\": \"" << SP->getFilename().str() << "\"";
    }
    return rawstr.str();
}

/// Get the next instructions following control flow
void LLVMUtil::getNextInsts(const Instruction* curInst, std::vector<const Instruction*>& instList)
{
    if (!curInst->isTerminator())
    {
        const Instruction* nextInst = curInst->getNextNode();
        if (LLVMUtil::isIntrinsicInst(nextInst))
            getNextInsts(nextInst, instList);
        else
            instList.push_back(nextInst);
    }
    else
    {
        const BasicBlock *BB = curInst->getParent();
        // Visit all successors of BB in the CFG
        for (succ_const_iterator it = succ_begin(BB), ie = succ_end(BB); it != ie; ++it)
        {
            const Instruction* nextInst = &((*it)->front());
            if (LLVMUtil::isIntrinsicInst(nextInst))
                getNextInsts(nextInst, instList);
            else
                instList.push_back(nextInst);
        }
    }
}


/// Get the previous instructions following control flow
void LLVMUtil::getPrevInsts(const Instruction* curInst, std::vector<const Instruction*>& instList)
{
    if (curInst != &(curInst->getParent()->front()))
    {
        const Instruction* prevInst = curInst->getPrevNode();
        if (LLVMUtil::isIntrinsicInst(prevInst))
            getPrevInsts(prevInst, instList);
        else
            instList.push_back(prevInst);
    }
    else
    {
        const BasicBlock *BB = curInst->getParent();
        // Visit all successors of BB in the CFG
        for (const_pred_iterator it = pred_begin(BB), ie = pred_end(BB); it != ie; ++it)
        {
            const Instruction* prevInst = &((*it)->back());
            if (LLVMUtil::isIntrinsicInst(prevInst))
                getPrevInsts(prevInst, instList);
            else
                instList.push_back(prevInst);
        }
    }
}

/// Check whether this value points-to a constant object
bool LLVMUtil::isConstantObjSym(const SVFValue* val)
{
    return isConstantObjSym(LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(val));
}

/*!
 * Check whether this value points-to a constant object
 */
bool LLVMUtil::isConstantObjSym(const Value* val)
{
    if (const GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (LLVMUtil::isValVtbl(v))
            return false;
        else if (!v->hasInitializer())
        {
            return !v->isExternalLinkage(v->getLinkage());
        }
        else
        {
            StInfo *stInfo = LLVMModuleSet::getLLVMModuleSet()->getSVFType(v->getInitializer()->getType())->getTypeInfo();
            const std::vector<const SVFType*> &fields = stInfo->getFlattenFieldTypes();
            for (std::vector<const SVFType*>::const_iterator it = fields.begin(), eit = fields.end(); it != eit; ++it)
            {
                const SVFType* elemTy = *it;
                assert(!SVFUtil::isa<SVFFunctionType>(elemTy) && "Initializer of a global is a function?");
                if (SVFUtil::isa<SVFPointerType>(elemTy))
                    return false;
            }

            return v->isConstant();
        }
    }
    return LLVMUtil::isConstDataOrAggData(val);
}

const ConstantStruct *LLVMUtil::getVtblStruct(const GlobalValue *vtbl)
{
    const ConstantStruct *vtblStruct = SVFUtil::dyn_cast<ConstantStruct>(vtbl->getOperand(0));
    assert(vtblStruct && "Initializer of a vtable not a struct?");

    if (vtblStruct->getNumOperands() == 2 &&
            SVFUtil::isa<ConstantStruct>(vtblStruct->getOperand(0)) &&
            vtblStruct->getOperand(1)->getType()->isArrayTy())
        return SVFUtil::cast<ConstantStruct>(vtblStruct->getOperand(0));

    return vtblStruct;
}

bool LLVMUtil::isValVtbl(const Value* val)
{
    if (!SVFUtil::isa<GlobalVariable>(val))
        return false;
    std::string valName = val->getName().str();
    return valName.compare(0, vtblLabelBeforeDemangle.size(),
                           vtblLabelBeforeDemangle) == 0;
}

bool LLVMUtil::isLoadVtblInst(const LoadInst* loadInst)
{
    const Value* loadSrc = loadInst->getPointerOperand();
    const Type* valTy = loadSrc->getType();
    const Type* elemTy = valTy;
    for (u32_t i = 0; i < 3; ++i)
    {
        if (const PointerType* ptrTy = SVFUtil::dyn_cast<PointerType>(elemTy))
            elemTy = LLVMUtil::getPtrElementType(ptrTy);
        else
            return false;
    }
    if (const FunctionType* functy = SVFUtil::dyn_cast<FunctionType>(elemTy))
    {
        const Type* paramty = functy->getParamType(0);
        std::string className = LLVMUtil::getClassNameFromType(paramty);
        if (className.size() > 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * a virtual callsite follows the following instruction sequence pattern:
 * %vtable = load this
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (this)
 */
bool LLVMUtil::isVirtualCallSite(const CallBase* cs)
{
    // the callsite must be an indirect one with at least one argument (this
    // ptr)
    if (cs->getCalledFunction() != nullptr || cs->arg_empty())
        return false;

    // the first argument (this pointer) must be a pointer type and must be a
    // class name
    if (cs->getArgOperand(0)->getType()->isPointerTy() == false)
        return false;

    const Value* vfunc = cs->getCalledOperand();
    if (const LoadInst* vfuncloadinst = SVFUtil::dyn_cast<LoadInst>(vfunc))
    {
        const Value* vfuncptr = vfuncloadinst->getPointerOperand();
        if (const GetElementPtrInst* vfuncptrgepinst =
                    SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr))
        {
            if (vfuncptrgepinst->getNumIndices() != 1)
                return false;
            const Value* vtbl = vfuncptrgepinst->getPointerOperand();
            if (SVFUtil::isa<LoadInst>(vtbl))
            {
                return true;
            }
        }
    }
    return false;
}

bool LLVMUtil::isCPPThunkFunction(const Function* F)
{
    cppUtil::DemangledName dname = cppUtil::demangle(F->getName().str());
    return dname.isThunkFunc;
}

const Function* LLVMUtil::getThunkTarget(const Function* F)
{
    const Function* ret = nullptr;

    for (auto& bb : *F)
    {
        for (auto& inst : bb)
        {
            if (const CallBase* callbase = SVFUtil::dyn_cast<CallBase>(&inst))
            {
                // assert(cs.getCalledFunction() &&
                //        "Indirect call detected in thunk func");
                // assert(ret == nullptr && "multiple callsites in thunk func");

                ret = callbase->getCalledFunction();
            }
        }
    }

    return ret;
}

const Value* LLVMUtil::getVCallThisPtr(const CallBase* cs)
{
    if (cs->paramHasAttr(0, llvm::Attribute::StructRet))
    {
        return cs->getArgOperand(1);
    }
    else
    {
        return cs->getArgOperand(0);
    }
}

/*!
 * Given a inheritance relation B is a child of A
 * We assume B::B(thisPtr1){ A::A(thisPtr2) } such that thisPtr1 == thisPtr2
 * In the following code thisPtr1 is "%class.B1* %this" and thisPtr2 is
 * "%class.A* %0".
 *
 *
 * define linkonce_odr dso_local void @B1::B1()(%class.B1* %this) unnamed_addr #6 comdat
 *   %this.addr = alloca %class.B1*, align 8
 *   store %class.B1* %this, %class.B1** %this.addr, align 8
 *   %this1 = load %class.B1*, %class.B1** %this.addr, align 8
 *   %0 = bitcast %class.B1* %this1 to %class.A*
 *   call void @A::A()(%class.A* %0)
 */
bool LLVMUtil::isSameThisPtrInConstructor(const Argument* thisPtr1,
        const Value* thisPtr2)
{
    if (thisPtr1 == thisPtr2)
        return true;
    for (const Value* thisU : thisPtr1->users())
    {
        if (const StoreInst* store = SVFUtil::dyn_cast<StoreInst>(thisU))
        {
            for (const Value* storeU : store->getPointerOperand()->users())
            {
                if (const LoadInst* load = SVFUtil::dyn_cast<LoadInst>(storeU))
                {
                    if (load->getNextNode() &&
                            SVFUtil::isa<CastInst>(load->getNextNode()))
                        return SVFUtil::cast<CastInst>(load->getNextNode()) ==
                               (thisPtr2->stripPointerCasts());
                }
            }
        }
    }
    return false;
}

const Argument* LLVMUtil::getConstructorThisPtr(const Function* fun)
{
    assert((LLVMUtil::isConstructor(fun) || LLVMUtil::isDestructor(fun)) &&
           "not a constructor?");
    assert(fun->arg_size() >= 1 && "argument size >= 1?");
    const Argument* thisPtr = &*(fun->arg_begin());
    return thisPtr;
}

bool LLVMUtil::isConstructor(const Function* F)
{
    if (F->isDeclaration())
        return false;
    std::string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0)
    {
        return false;
    }
    struct cppUtil::DemangledName dname = cppUtil::demangle(funcName.c_str());
    if (dname.className.size() == 0)
    {
        return false;
    }
    dname.funcName = cppUtil::getBeforeBrackets(dname.funcName);
    dname.className = cppUtil::getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == std::string::npos)
    {
        dname.className = cppUtil::getBeforeBrackets(dname.className);
    }
    else
    {
        dname.className =
            cppUtil::getBeforeBrackets(dname.className.substr(colon + 2));
    }
    /// TODO: on mac os function name is an empty string after demangling
    return dname.className.size() > 0 &&
           dname.className.compare(dname.funcName) == 0;
}

bool LLVMUtil::isDestructor(const Function* F)
{
    if (F->isDeclaration())
        return false;
    std::string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0)
    {
        return false;
    }
    struct cppUtil::DemangledName dname = cppUtil::demangle(funcName.c_str());
    if (dname.className.size() == 0)
    {
        return false;
    }
    dname.funcName = cppUtil::getBeforeBrackets(dname.funcName);
    dname.className = cppUtil::getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == std::string::npos)
    {
        dname.className = cppUtil::getBeforeBrackets(dname.className);
    }
    else
    {
        dname.className =
            cppUtil::getBeforeBrackets(dname.className.substr(colon + 2));
    }
    return (dname.className.size() > 0 && dname.funcName.size() > 0 &&
            dname.className.size() + 1 == dname.funcName.size() &&
            dname.funcName.compare(0, 1, "~") == 0 &&
            dname.className.compare(dname.funcName.substr(1)) == 0);
}

/*
 * get the ptr "vtable" for a given virtual callsite:
 * %vtable = load ...
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (...)
 */
const Value* LLVMUtil::getVCallVtblPtr(const CallBase* cs)
{
    const LoadInst* loadInst =
        SVFUtil::dyn_cast<LoadInst>(cs->getCalledOperand());
    assert(loadInst != nullptr);
    const Value* vfuncptr = loadInst->getPointerOperand();
    const GetElementPtrInst* gepInst =
        SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr);
    assert(gepInst != nullptr);
    const Value* vtbl = gepInst->getPointerOperand();
    return vtbl;
}

/*
 * Is this virtual call inside its own constructor or destructor?
 */
bool LLVMUtil::VCallInCtorOrDtor(const CallBase* cs)
{
    std::string classNameOfThisPtr = LLVMUtil::getClassNameOfThisPtr(cs);
    const Function* func = cs->getCaller();
    if (LLVMUtil::isConstructor(func) || LLVMUtil::isDestructor(func))
    {
        cppUtil::DemangledName dname = cppUtil::demangle(func->getName().str());
        if (classNameOfThisPtr.compare(dname.className) == 0)
            return true;
    }
    return false;
}

std::string LLVMUtil::getClassNameFromType(const Type* ty)
{
    std::string className = "";
    if (const PointerType* ptrType = SVFUtil::dyn_cast<PointerType>(ty))
    {
        const Type* elemType = LLVMUtil::getPtrElementType(ptrType);
        if (SVFUtil::isa<StructType>(elemType) &&
                !((SVFUtil::cast<StructType>(elemType))->isLiteral()))
        {
            std::string elemTypeName = elemType->getStructName().str();
            if (elemTypeName.compare(0, clsName.size(), clsName) == 0)
            {
                className = elemTypeName.substr(clsName.size());
            }
            else if (elemTypeName.compare(0, structName.size(), structName) ==
                     0)
            {
                className = elemTypeName.substr(structName.size());
            }
        }
    }
    return className;
}

std::string LLVMUtil::getClassNameOfThisPtr(const CallBase* inst)
{
    std::string thisPtrClassName;
    if (const MDNode* N = inst->getMetadata("VCallPtrType"))
    {
        const MDString* mdstr = SVFUtil::cast<MDString>(N->getOperand(0).get());
        thisPtrClassName = mdstr->getString().str();
    }
    if (thisPtrClassName.size() == 0)
    {
        const Value* thisPtr = LLVMUtil::getVCallThisPtr(inst);
        thisPtrClassName = getClassNameFromType(thisPtr->getType());
    }

    size_t found = thisPtrClassName.find_last_not_of("0123456789");
    if (found != std::string::npos)
    {
        if (found != thisPtrClassName.size() - 1 &&
                thisPtrClassName[found] == '.')
        {
            return thisPtrClassName.substr(0, found);
        }
    }

    return thisPtrClassName;
}

std::string LLVMUtil::getFunNameOfVCallSite(const CallBase* inst)
{
    std::string funName;
    if (const MDNode* N = inst->getMetadata("VCallFunName"))
    {
        const MDString* mdstr = SVFUtil::cast<MDString>(N->getOperand(0).get());
        funName = mdstr->getString().str();
    }
    return funName;
}

s32_t LLVMUtil::getVCallIdx(const CallBase* cs)
{
    const LoadInst* vfuncloadinst =
        SVFUtil::dyn_cast<LoadInst>(cs->getCalledOperand());
    assert(vfuncloadinst != nullptr);
    const Value* vfuncptr = vfuncloadinst->getPointerOperand();
    const GetElementPtrInst* vfuncptrgepinst =
        SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr);
    User::const_op_iterator oi = vfuncptrgepinst->idx_begin();
    const ConstantInt* idx = SVFUtil::dyn_cast<ConstantInt>(oi->get());
    s32_t idx_value;
    if (idx == nullptr)
    {
        SVFUtil::errs() << "vcall gep idx not constantint\n";
        idx_value = 0;
    }
    else
    {
        idx_value = (s32_t)idx->getSExtValue();
    }
    return idx_value;
}

namespace SVF
{
std::string SVFValue::toString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    if (const SVF::SVFFunction* fun = SVFUtil::dyn_cast<SVFFunction>(this))
    {
        rawstr << "Function: " << fun->getName() << " ";
    }
    else if (const SVFBasicBlock* bb = SVFUtil::dyn_cast<SVFBasicBlock>(this))
    {
        rawstr << "BasicBlock: " << bb->getName() << " ";
    }
    else
    {
        auto llvmVal = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(this);
        if (llvmVal)
            rawstr << " " << *llvmVal << " ";
        else
            rawstr << " No llvmVal found";
    }
    rawstr << this->getSourceLoc();
    return rawstr.str();
}

} // namespace SVF
