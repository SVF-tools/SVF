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
#include "SVF-LLVM/LLVMModule.h"


using namespace SVF;

const Function* LLVMUtil::getDefFunForMultipleModule(const Function* fun)
{
    if (fun == nullptr)
        return nullptr;
    LLVMModuleSet* llvmModuleset = LLVMModuleSet::getLLVMModuleSet();
    if (fun->isDeclaration() && llvmModuleset->hasDefinition(fun))
        fun = LLVMModuleSet::getLLVMModuleSet()->getDefinition(fun);
    return fun;
}

const Function* LLVMUtil::getProgFunction(const std::string& funName)
{
    for (const Module& M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for (const Function& fun : M)
        {
            if (fun.getName() == funName)
                return &fun;
        }
    }
    return nullptr;
}

/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool LLVMUtil::isObject(const Value*  ref)
{
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

/**
 * Return true if the basic block has a return instruction
 */
bool LLVMUtil::basicBlockHasRetInst(const BasicBlock* bb)
{
    for (BasicBlock::const_iterator it = bb->begin(), eit = bb->end();
            it != eit; ++it)
    {
        if(SVFUtil::isa<ReturnInst>(*it))
            return true;
    }
    return false;
}

/*!
 * Return true if the function has a return instruction reachable from function entry
 */
bool LLVMUtil::functionDoesNotRet(const Function*  fun)
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
        if (basicBlockHasRetInst(bb))
        {
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
const Value* LLVMUtil::getFirstUseViaCastInst(const Value* val)
{
    assert(SVFUtil::isa<PointerType>(val->getType()) && "this value should be a pointer type!");
    /// If type is void* (i8*) and val is immediately used at a bitcast instruction
    const Value *latestUse = nullptr;
    for (const auto &it : val->uses())
    {
        if (SVFUtil::isa<BitCastInst>(it.getUser()))
            latestUse = it.getUser();
        else
            latestUse = nullptr;
    }
    return latestUse;
}

/*!
 * Return size of this Object
 */
u32_t LLVMUtil::getNumOfElements(const Type* ety)
{
    assert(ety && "type is null?");
    u32_t numOfFields = 1;
    if (SVFUtil::isa<StructType, ArrayType>(ety))
    {
        if(Options::ModelArrays())
            return LLVMModuleSet::getLLVMModuleSet()->getSVFType(ety)->getTypeInfo()->getNumOfFlattenElements();
        else
            return LLVMModuleSet::getLLVMModuleSet()->getSVFType(ety)->getTypeInfo()->getNumOfFlattenFields();
    }
    return numOfFields;
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
    llvm::LLVMContext context;
    llvm::SMDiagnostic err;

    // Parse the input LLVM IR file into a module
    std::unique_ptr<llvm::Module> module = llvm::parseIRFile(filename, err, context);

    // Check if the parsing succeeded
    if (!module)
    {
        err.print("isIRFile", llvm::errs());
        return false; // Not an LLVM IR file
    }

    return true; // It is an LLVM IR file
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

void LLVMUtil::removeFunAnnotations(Set<Function*>& removedFuncList)
{
    if (removedFuncList.empty())
        return; // No functions to remove annotations in extapi.bc module

    Module* module = (*removedFuncList.begin())->getParent();
    GlobalVariable* glob = module->getGlobalVariable("llvm.global.annotations");
    if (glob == nullptr || !glob->hasInitializer())
        return;

    ConstantArray* ca = SVFUtil::dyn_cast<ConstantArray>(glob->getInitializer());
    if (ca == nullptr)
        return;

    std::vector<Constant*> newAnnotations;
    for (unsigned i = 0; i < ca->getNumOperands(); ++i)
    {
        ConstantStruct* structAn = SVFUtil::dyn_cast<ConstantStruct>(ca->getOperand(i));
        if (structAn == nullptr)
            continue;

        Function* annotatedFunc = nullptr;

        // Non-opague pointer, try to cast to ConstantExpr and check for BitCast
        if (ConstantExpr* expr = SVFUtil::dyn_cast<ConstantExpr>(structAn->getOperand(0)))
        {
            if (expr->getOpcode() == Instruction::BitCast)
            {
                annotatedFunc = SVFUtil::dyn_cast<Function>(expr->getOperand(0));
            }
        }

        // Opague pointer, If the above method didn't work, try casting directly to Function
        if (!annotatedFunc)
        {
            annotatedFunc = SVFUtil::dyn_cast<Function>(structAn->getOperand(0));
        }

        // Process the annotated function if it's not in the removed list
        if (annotatedFunc && std::find(removedFuncList.begin(), removedFuncList.end(), annotatedFunc) == removedFuncList.end())
        {
            newAnnotations.push_back(structAn);
        }
    }

    if (newAnnotations.size() == ca->getNumOperands())
        return; // No annotations to remove

    ArrayType* annotationsType = ArrayType::get(ca->getType()->getElementType(), newAnnotations.size());
    Constant* newCA = ConstantArray::get(annotationsType, newAnnotations);

    glob->setName("llvm.global.annotations.old");
    GlobalVariable *GV = new GlobalVariable(newCA->getType(), glob->isConstant(), glob->getLinkage(), newCA, "llvm.global.annotations");
    GV->setSection(glob->getSection());

#if (LLVM_VERSION_MAJOR < 17)
    module->getGlobalList().push_back(GV);
#elif (LLVM_VERSION_MAJOR >= 17)
    module->insertGlobalVariable(GV);
#else
    assert(false && "llvm version not supported!");
#endif

    glob->replaceAllUsesWith(GV);
    glob->eraseFromParent();
}

/// Get all called funcions in a parent function
std::vector<const Function *> LLVMUtil::getCalledFunctions(const Function *F)
{
    std::vector<const Function *> calledFunctions;
    for (const Instruction &I : instructions(F))
    {
        if (const CallBase *callInst = SVFUtil::dyn_cast<CallBase>(&I))
        {
            Function *calledFunction = callInst->getCalledFunction();
            if (calledFunction)
            {
                calledFunctions.push_back(calledFunction);
                std::vector<const Function *> nestedCalledFunctions = getCalledFunctions(calledFunction);
                calledFunctions.insert(calledFunctions.end(), nestedCalledFunctions.begin(), nestedCalledFunctions.end());
            }
        }
    }
    return calledFunctions;
}

bool LLVMUtil::isUnusedGlobalVariable(const GlobalVariable& global)
{
    // Check if it is an empty global annotations
    if (global.getName() == "llvm.global.annotations" && SVFUtil::isa<ConstantArray>(global.getInitializer()))
        return false;
    else
    {
        // Check if any global strings has at least one effective user
        for (auto& use : global.uses())
            if (use.getUser()->getNumUses() != 0)
                return false;
    }
    return true;
}

void LLVMUtil::removeUnusedGlobalVariables(Module* module)
{
    assert(module && "Null module pointer!");
    std::vector<GlobalVariable*> unusedGlobals;
    for (GlobalVariable& global : module->globals())
        if (isUnusedGlobalVariable(global))
            // Record unused global variables
            unusedGlobals.push_back(&global);

    // Delete unused global variables
    for (GlobalVariable* global : unusedGlobals)
        global->eraseFromParent();
}

/// Delete unused functions, annotations and global variables in extapi.bc
void LLVMUtil::removeUnusedFuncsAndAnnotationsAndGlobalVariables(Set<Function*> removedFuncList)
{
    if (removedFuncList.empty())
        return;

    Module* mod = (*removedFuncList.begin())->getParent();
    if (mod->getName().str() != ExtAPI::getExtAPI()->getExtBcPath())
        return;

    /// Delete unused function annotations
    LLVMUtil::removeFunAnnotations(removedFuncList);

    /// Delete unused functions
    /// The functions to be deleted from extapi.bc can be categorized into two types.
    /// The first type includes functions do not contain any invocation statements,
    /// The second type includes functions whose contain invocation statements.
    /// It is necessary to delete functions of the second type first before deleting those of the first type;
    /// Otherwise, errors may occur when calling eraseFromParent().
    std::vector<Function*> funcsToKeep;
    /// Check whether a function is called by other functions
    auto isCalledFunction = [](llvm::Function* F)
    {
        assert(F && "Null function pointer!");
        for (auto& use : F->uses())
        {
            llvm::User* user = use.getUser();
            if (llvm::isa<llvm::CallBase>(user))
                return true;
        }
        return false;
    };

    for (Function* func : removedFuncList)
    {
        if (isCalledFunction(func))
            // Record first kind function(which does not contain any invocation statements)
            funcsToKeep.push_back(func);
        else
            // Delete second kind function(which contains invocation statements)
            func->eraseFromParent();
    }
    // Delete first kind functions
    for (Function* func : funcsToKeep)
        func->eraseFromParent();
    // Delete unused global variables
    removeUnusedGlobalVariables(mod);
}

std::string LLVMUtil::restoreFuncName(std::string funcName)
{
    assert(!funcName.empty() && "Empty function name");
    // Some function names change due to mangling, such as "fopen" to "\01_fopen" on macOS.
    // Since C function names cannot include '.', change the function name from llvm.memcpy.p0i8.p0i8.i64 to llvm_memcpy_p0i8_p0i8_i64."
    bool hasSpecialPrefix = funcName[0] == '\01';
    bool hasDot = funcName.find('.') != std::string::npos;

    if (!hasDot && !hasSpecialPrefix)
        return funcName;

    // Remove prefix "\01_" or "\01"
    if (hasSpecialPrefix)
    {
        const std::string prefix1 = "\01_";
        const std::string prefix2 = "\01";
        if (funcName.substr(0, prefix1.length()) == prefix1)
            funcName = funcName.substr(prefix1.length());
        else if (funcName.substr(0, prefix2.length()) == prefix2)
            funcName = funcName.substr(prefix2.length());
    }
    // Replace '.' with '_'
    if (hasDot)
        std::replace(funcName.begin(), funcName.end(), '.', '_');

    return funcName;
}

const SVFFunction* LLVMUtil::getFunction(const std::string& name)
{
    return LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(name);
}

const Value* LLVMUtil::getGlobalRep(const Value* val)
{
    if (const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (LLVMModuleSet::getLLVMModuleSet()->hasGlobalRep(gvar))
            val = LLVMModuleSet::getLLVMModuleSet()->getGlobalRep(gvar);
    }
    return val;
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
            for (llvm::DbgInfoIntrinsic *DII : FindDbgDeclareUses(const_cast<Instruction*>(inst)))
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


std::string LLVMUtil::dumpValue(const Value* val)
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    if (val)
        rawstr << " " << *val << " ";
    else
        rawstr << " llvm Value is null";
    return rawstr.str();
}

std::string LLVMUtil::dumpType(const Type* type)
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    if (type)
        rawstr << " " << *type << " ";
    else
        rawstr << " llvm type is null";
    return rawstr.str();
}

std::string LLVMUtil::dumpValueAndDbgInfo(const Value *val)
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    if (val)
        rawstr << dumpValue(val) << getSourceLoc(val);
    else
        rawstr << " llvm Value is null";
    return rawstr.str();
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
