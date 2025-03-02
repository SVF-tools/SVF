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
#include "SVFIR/ObjTypeInfo.h"
#include <sstream>
#include <llvm/Support/raw_ostream.h>
#include "SVF-LLVM/LLVMModule.h"


using namespace SVF;

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
    if (SVFUtil::isa<Instruction>(ref) && isHeapAllocExtCallViaRet(SVFUtil::cast<Instruction>(ref)))
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
    assert(!LLVMUtil::isExtCall(fun) && "The calling function cannot be an external function.");
    //initial DominatorTree
    DominatorTree& dt = LLVMModuleSet::getLLVMModuleSet()->getDomTree(fun);

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
    if (LLVMUtil::isExtCall(fun))
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


bool LLVMUtil::isExtCall(const Function* fun)
{
    return fun && LLVMModuleSet::getLLVMModuleSet()->is_ext(fun);
}

bool LLVMUtil::isMemcpyExtFun(const Function *fun)
{
    return fun && LLVMModuleSet::getLLVMModuleSet()->is_memcpy(fun);
}


bool LLVMUtil::isMemsetExtFun(const Function* fun)
{
    return fun && LLVMModuleSet::getLLVMModuleSet()->is_memset(fun);
}


u32_t LLVMUtil::getHeapAllocHoldingArgPosition(const Function* fun)
{
    return LLVMModuleSet::getLLVMModuleSet()->get_alloc_arg_pos(fun);
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


const FunObjVar* LLVMUtil::getFunObjVar(const std::string& name)
{
    return LLVMModuleSet::getLLVMModuleSet()->getFunObjVar(name);
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

bool LLVMUtil::isHeapAllocExtCallViaRet(const Instruction* inst)
{
    LLVMModuleSet* pSet = LLVMModuleSet::getLLVMModuleSet();
    bool isPtrTy = inst->getType()->isPointerTy();
    if (const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
    {
        const Function* fun = call->getCalledFunction();
        return fun && isPtrTy &&
               (pSet->is_alloc(fun) ||
                pSet->is_realloc(fun));
    }
    else
        return false;
}

bool LLVMUtil::isHeapAllocExtCallViaArg(const Instruction* inst)
{
    if (const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
    {
        const Function* fun = call->getCalledFunction();
        return fun &&
               LLVMModuleSet::getLLVMModuleSet()->is_arg_alloc(fun);
    }
    else
    {
        return false;
    }
}

bool LLVMUtil::isStackAllocExtCallViaRet(const Instruction *inst)
{
    LLVMModuleSet* pSet = LLVMModuleSet::getLLVMModuleSet();
    bool isPtrTy = inst->getType()->isPointerTy();
    if (const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
    {
        const Function* fun = call->getCalledFunction();
        return fun && isPtrTy &&
               pSet->is_alloc_stack_ret(fun);
    }
    else
        return false;
}

/**
 * Check if a given value represents a heap object.
 *
 * @param val The value to check.
 * @return True if the value represents a heap object, false otherwise.
 */
bool LLVMUtil::isHeapObj(const Value* val)
{
    // Check if the value is an argument in the program entry function
    if (ArgInProgEntryFunction(val))
    {
        // Return true if the value does not have a first use via cast instruction
        return !getFirstUseViaCastInst(val);
    }
    // Check if the value is an instruction and if it is a heap allocation external call
    else if (SVFUtil::isa<Instruction>(val) &&
             LLVMUtil::isHeapAllocExtCall(SVFUtil::cast<Instruction>(val)))
    {
        return true;
    }
    // Return false if none of the above conditions are met
    return false;
}

/**
 * @param val The value to check.
 * @return True if the value represents a stack object, false otherwise.
 */
bool LLVMUtil::isStackObj(const Value* val)
{
    if (SVFUtil::isa<AllocaInst>(val))
    {
        return true;
    }
    // Check if the value is an instruction and if it is a stack allocation external call
    else if (SVFUtil::isa<Instruction>(val) &&
             LLVMUtil::isStackAllocExtCall(SVFUtil::cast<Instruction>(val)))
    {
        return true;
    }
    // Return false if none of the above conditions are met
    return false;
}

bool LLVMUtil::isNonInstricCallSite(const Instruction* inst)
{
    bool res = false;

    if(isIntrinsicInst(inst))
        res = false;
    else
        res = isCallSite(inst);
    return res;
}

namespace SVF
{


const std::string SVFValue::valueOnlyToString() const
{
    std::string str;
    llvm::raw_string_ostream rawstr(str);
    assert(
        !SVFUtil::isa<GepObjVar>(this) && !SVFUtil::isa<GepValVar>(this) &&
        !SVFUtil::isa<DummyObjVar>(this) &&!SVFUtil::isa<DummyValVar>(this) &&
        !SVFUtil::isa<BlackHoleValVar>(this) &&
        "invalid value, refer to their toString method");
    auto llvmVal =
        LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(this);
    if (llvmVal)
        rawstr << " " << *llvmVal << " ";
    else
        rawstr << "";
    rawstr << getSourceLoc();
    return rawstr.str();
}

} // namespace SVF
