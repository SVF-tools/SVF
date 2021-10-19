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
 * SVFUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/LLVMUtil.h"

using namespace SVF;

/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool SVFUtil::isObject(const Value * ref)
{
    bool createobj = false;
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isStaticExtCall(SVFUtil::cast<Instruction>(ref)) )
        createobj = true;
    if (SVFUtil::isa<Instruction>(ref) && SVFUtil::isHeapAllocExtCallViaRet(SVFUtil::cast<Instruction>(ref)))
        createobj = true;
    if (SVFUtil::isa<GlobalVariable>(ref))
        createobj = true;
    if (SVFUtil::isa<Function>(ref) || SVFUtil::isa<AllocaInst>(ref) )
        createobj = true;

    return createobj;
}

/*!
 * Return reachable bbs from function entry
 */
void SVFUtil::getFunReachableBBs (const Function * fun, DominatorTree* dt, std::vector<const BasicBlock*> &reachableBBs)
{
    Set<const BasicBlock*> visited;
    std::vector<const BasicBlock*> bbVec;
    bbVec.push_back(&fun->getEntryBlock());
    while(!bbVec.empty())
    {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        reachableBBs.push_back(bb);
        if(DomTreeNode *dtNode = dt->getNode(const_cast<BasicBlock*>(bb)))
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
bool SVFUtil::functionDoesNotRet (const Function * fun)
{

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
bool SVFUtil::isDeadFunction (const Function * fun)
{
    if(fun->hasAddressTaken())
        return false;
    if(isProgEntryFunction(fun))
        return false;
    for (Value::const_user_iterator i = fun->user_begin(), e = fun->user_end(); i != e; ++i)
    {
        if (SVFUtil::isCallSite(*i))
            return false;
    }
    if (LLVMModuleSet::getLLVMModuleSet()->hasDeclaration(fun))
    {
        const SVFModule::FunctionSetType &decls = LLVMModuleSet::getLLVMModuleSet()->getDeclaration(fun);
        for (SVFModule::FunctionSetType::const_iterator it = decls.begin(),
                eit = decls.end(); it != eit; ++it)
        {
            const Function *decl = (*it)->getLLVMFun();
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
bool SVFUtil::isPtrInDeadFunction (const Value * value)
{
    if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(value))
    {
        if(isDeadFunction(inst->getParent()->getParent()))
            return true;
    }
    else if(const Argument* arg = SVFUtil::dyn_cast<Argument>(value))
    {
        if(isDeadFunction(arg->getParent()))
            return true;
    }
    return false;
}

/*!
 * Strip constant casts
 */
const Value * SVFUtil::stripConstantCasts(const Value *val)
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
Value * SVFUtil::stripAllCasts(Value *val)
{
    while (true)
    {
        if (CastInst *ci = SVFUtil::dyn_cast<CastInst>(val))
        {
            val = ci->getOperand(0);
        }
        else if (ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(val))
        {
            if(ce->isCast())
                val = ce->getOperand(0);
        }
        else
        {
            return val;
        }
    }
    return nullptr;
}

/// Get the next instructions following control flow
void SVFUtil::getNextInsts(const Instruction* curInst, std::vector<const Instruction*>& instList)
{
    if (!curInst->isTerminator())
    {
        const Instruction* nextInst = curInst->getNextNode();
        if (isIntrinsicInst(nextInst))
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
            if (isIntrinsicInst(nextInst))
                getNextInsts(nextInst, instList);
            else
                instList.push_back(nextInst);
        }
    }
}


/// Get the previous instructions following control flow
void SVFUtil::getPrevInsts(const Instruction* curInst, std::vector<const Instruction*>& instList)
{
    if (curInst != &(curInst->getParent()->front()))
    {
        const Instruction* prevInst = curInst->getPrevNode();
        if (isIntrinsicInst(prevInst))
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
            if (isIntrinsicInst(prevInst))
                getPrevInsts(prevInst, instList);
            else
                instList.push_back(prevInst);
        }
    }
}


/*!
 * Return the type of the object from a heap allocation
 */
const Type* SVFUtil::getTypeOfHeapAlloc(const Instruction *inst)
{
    const PointerType* type = SVFUtil::dyn_cast<PointerType>(inst->getType());

    if(isHeapAllocExtCallViaRet(inst))
    {
        const Instruction* nextInst = inst->getNextNode();
        if(nextInst && nextInst->getOpcode() == Instruction::BitCast)
            // we only consider bitcast instructions and ignore others (e.g., IntToPtr and ZExt)
            type = SVFUtil::dyn_cast<PointerType>(inst->getNextNode()->getType());
    }
    else if(isHeapAllocExtCallViaArg(inst))
    {
        CallSite cs = getLLVMCallSite(inst);
        int arg_pos = getHeapAllocHoldingArgPosition(getCallee(cs));
        const Value *arg = cs.getArgument(arg_pos);
        type = SVFUtil::dyn_cast<PointerType>(arg->getType());
    }
    else
    {
        assert( false && "not a heap allocation instruction?");
    }

    assert(type && "not a pointer type?");
    return type->getElementType();
}

/*!
 * Get position of a successor basic block
 */
u32_t SVFUtil::getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ)
{
    u32_t i = 0;
    for (const BasicBlock *SuccBB: successors(BB))
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}


/*!
 * Return a position index from current bb to it successor bb
 */
u32_t SVFUtil::getBBPredecessorPos(const BasicBlock *bb, const BasicBlock *succbb)
{
    u32_t pos = 0;
    for (const_pred_iterator it = pred_begin(succbb), et = pred_end(succbb); it != et; ++it, ++pos)
    {
        if(*it==bb)
            return pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

/*!
 *  Get the num of BB's successors
 */
u32_t SVFUtil::getBBSuccessorNum(const BasicBlock *BB)
{
    return BB->getTerminator()->getNumSuccessors();
}

/*!
 * Get the num of BB's predecessors
 */
u32_t SVFUtil::getBBPredecessorNum(const BasicBlock *BB)
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
bool SVFUtil::isIRFile(const std::string &filename)
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
void SVFUtil::processArguments(int argc, char **argv, int &arg_num, char **arg_value,
                               std::vector<std::string> &moduleNameVec)
{
    bool first_ir_file = true;
    for (s32_t i = 0; i < argc; ++i)
    {
        std::string argument(argv[i]);
        if (SVFUtil::isIRFile(argument))
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

