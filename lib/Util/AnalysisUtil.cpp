//===- AnalysisUtil.cpp -- Helper functions for pointer analysis--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * AnalysisUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "Util/AnalysisUtil.h"

#include <llvm/Transforms/Utils/Local.h>	// for FindAllocaDbgDeclare
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable
#include <llvm/IR/Module.h>	// for Module
#include <llvm/IR/InstrTypes.h>	// for TerminatorInst
#include <llvm/IR/IntrinsicInst.h>	// for intrinsic instruction
//#include <llvm/Analysis/DebugInfo.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/InstIterator.h>	// for inst iteration
#include <llvm/Analysis/CFG.h>	// for CFG
#include <llvm/IR/CFG.h>		// for CFG
#include "Util/Conditions.h"
#include <sys/resource.h>		/// increase stack size

using namespace llvm;

/// Color for output format
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KPUR  "\x1B[1;35m"
#define KCYA  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"

static cl::opt<bool> DisableWarn("dwarn", cl::init(true),
                                 cl::desc("Disable warning"));

/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool analysisUtil::isObject(const Value * ref) {
    bool createobj = false;
    if (isa<Instruction>(ref) && isHeapAllocOrStaticExtCall(cast<Instruction>(ref)))
        createobj = true;
    if (isa<GlobalVariable>(ref))
        createobj = true;
    if (isa<Function>(ref) || isa<AllocaInst>(ref) )
        createobj = true;

    return createobj;
}

/*!
 * Return true if it is an intric debug instruction
 */
bool analysisUtil::isInstrinsicDbgInst(const Instruction* inst) {
    return isa<DbgInfoIntrinsic>(inst);
}

/*!
 * Return reachable bbs from function entry
 */
void analysisUtil::getFunReachableBBs (const llvm::Function * fun, llvm::DominatorTree* dt, std::vector<const llvm::BasicBlock*> &reachableBBs) {
    std::set<const BasicBlock*> visited;
    std::vector<const BasicBlock*> bbVec;
    bbVec.push_back(&fun->getEntryBlock());
    while(!bbVec.empty()) {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        reachableBBs.push_back(bb);
        if(DomTreeNode *dtNode = dt->getNode(const_cast<BasicBlock*>(bb))) {
            for (DomTreeNode::iterator DI = dtNode->begin(), DE = dtNode->end();
                    DI != DE; ++DI) {
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
bool analysisUtil::functionDoesNotRet (const llvm::Function * fun) {

    std::vector<const BasicBlock*> bbVec;
    std::set<const BasicBlock*> visited;
    bbVec.push_back(&fun->getEntryBlock());
    while(!bbVec.empty()) {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        for (llvm::BasicBlock::const_iterator it = bb->begin(), eit = bb->end();
                it != eit; ++it) {
            if(isa<ReturnInst>(*it))
                return false;
        }

        for (succ_const_iterator sit = succ_begin(bb), esit = succ_end(bb);
                sit != esit; ++sit) {
            const BasicBlock* succbb = (*sit);
            if(visited.find(succbb)==visited.end())
                visited.insert(succbb);
            else
                continue;
            bbVec.push_back(succbb);
        }
    }
    if(isProgEntryFunction(fun)==false) {
        wrnMsg(fun->getName().str() + " does not have return");
    }
    return true;
}

/*!
 * Return true if this is a function without any possible caller
 */
bool analysisUtil::isDeadFunction (const llvm::Function * fun) {
    if(fun->hasAddressTaken())
        return false;
    if(isProgEntryFunction(fun))
        return false;
    for (Value::const_user_iterator i = fun->user_begin(), e = fun->user_end(); i != e; ++i) {
        if (isa<CallInst>(*i) || isa<InvokeInst>(*i))
            return false;
    }
    return true;
}

/*!
 * Return true if this is a value in a dead function (function without any caller)
 */
bool analysisUtil::isPtrInDeadFunction (const llvm::Value * value) {
    if(const llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(value)) {
        if(isDeadFunction(inst->getParent()->getParent()))
            return true;
    }
    else if(const llvm::Argument* arg = llvm::dyn_cast<llvm::Argument>(value)) {
        if(isDeadFunction(arg->getParent()))
            return true;
    }
    return false;
}

/*!
 * Strip constant casts
 */
const Value * analysisUtil::stripConstantCasts(const Value *val) {
    if (isa<GlobalValue>(val) || isInt2PtrConstantExpr(val))
        return val;
    else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(val)) {
        if (Instruction::isCast(CE->getOpcode()))
            return stripConstantCasts(CE->getOperand(0));
    }
    return val;
}

/*!
 * Strip all casts
 */
Value * analysisUtil::stripAllCasts(Value *val) {
    while (true) {
        if (CastInst *ci = dyn_cast<CastInst>(val)) {
            val = ci->getOperand(0);
        } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(val)) {
            if(ce->isCast())
                val = ce->getOperand(0);
        } else {
            return val;
        }
    }
    return NULL;
}

/*!
 * Get position of a successor basic block
 */
u32_t analysisUtil::getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ) {
    const TerminatorInst *Term = BB->getTerminator();
    u32_t e = Term->getNumSuccessors();
    for (u32_t i = 0; ; ++i) {
        assert(i != e && "Didn't find succesor edge?");
        if (Term->getSuccessor(i) == Succ)
            return i;
    }
    return 0;
}

/*!
 * Get position of a predecessor basic block
 */
u32_t analysisUtil::getBBPredecessorPos(const llvm::BasicBlock *BB, const llvm::BasicBlock *Pred) {
    u32_t pos = 0;
    for (const_pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it, ++pos) {
        if(*it==Pred)
            return pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

/*!
 *  Get the num of BB's successors
 */
u32_t analysisUtil::getBBSuccessorNum(const llvm::BasicBlock *BB) {
    return BB->getTerminator()->getNumSuccessors();
}

/*!
 * Get the num of BB's predecessors
 */
u32_t analysisUtil::getBBPredecessorNum(const llvm::BasicBlock *BB) {
    u32_t num = 0;
    for (const_pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
        num++;
    return num;
}

/*!
 * Get source code line number of a function according to debug info
 */
std::string analysisUtil::getSourceLocOfFunction(const llvm::Function *F)
{
    std::string str;
    raw_string_ostream rawstr(str);
    NamedMDNode* CU_Nodes = F->getParent()->getNamedMetadata("llvm.dbg.cu");
    if(CU_Nodes) {
      /*
       * Looks like the DICompileUnt->getSubprogram was moved into Function::
       */
        for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i) {
            DICompileUnit *CUNode = cast<DICompileUnit>(CU_Nodes->getOperand(i));
	    /*
	     * https://reviews.llvm.org/D18074?id=50385 
	     * looks like the relevant
	     */
            if (DISubprogram *SP =  F->getSubprogram()) {
                if (SP->describes(F))
                    rawstr << "in line: " << SP->getLine()
                           << " file: " << SP->getFilename();
            }
        }
    }
    return rawstr.str();
}

/*!
 * Get the meta data (line number and file name) info of a LLVM value
 */
std::string analysisUtil::getSourceLoc(const Value* val) {
    if(val==NULL)  return "empty val";

    std::string str;
    raw_string_ostream rawstr(str);
    if (const Instruction *inst = dyn_cast<Instruction>(val)) {
        if (isa<AllocaInst>(inst)) {
            DbgDeclareInst* DDI = llvm::FindAllocaDbgDeclare(const_cast<Instruction*>(inst));
            if (DDI) {
                DIVariable *DIVar = cast<DIVariable>(DDI->getVariable());
                rawstr << "ln: " << DIVar->getLine() << " fl: " << DIVar->getFilename();
            }
        }
        else if (MDNode *N = inst->getMetadata("dbg")) { // Here I is an LLVM instruction
            DILocation* Loc = cast<DILocation>(N);                   // DILocation is in DebugInfo.h
            unsigned Line = Loc->getLine();
            StringRef File = Loc->getFilename();
            //StringRef Dir = Loc.getDirectory();
            rawstr << "ln: " << Line << " fl: " << File;
        }
    }
    else if (const Argument* argument = dyn_cast<Argument>(val)) {
        if (argument->getArgNo()%10 == 1)
            rawstr << argument->getArgNo() << "st";
        else if (argument->getArgNo()%10 == 2)
            rawstr << argument->getArgNo() << "nd";
        else if (argument->getArgNo()%10 == 3)
            rawstr << argument->getArgNo() << "rd";
        else
            rawstr << argument->getArgNo() << "th";
        rawstr << " arg " << argument->getParent()->getName() << " "
               << getSourceLocOfFunction(argument->getParent());
    }
    else if (const GlobalVariable* gvar = dyn_cast<GlobalVariable>(val)) {
        rawstr << "Glob ";
        NamedMDNode* CU_Nodes = gvar->getParent()->getNamedMetadata("llvm.dbg.cu");
        if(CU_Nodes) {
            for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i) {
                DICompileUnit *CUNode = cast<DICompileUnit>(CU_Nodes->getOperand(i));
                for (DIGlobalVariableExpression *GV : CUNode->getGlobalVariables()) {
		  DIGlobalVariable * DGV = GV->getVariable();

		  /*
		  if ( gvar == DGV )
                        rawstr << "ln: " << DGV->getLine() << " fl: " << DGV->getFilename();
		  */
                }
            }
        }
    }
    else if (const Function* func = dyn_cast<Function>(val)) {
        rawstr << getSourceLocOfFunction(func);
    }
    else {
        rawstr << "Can only get source location for instruction, argument, global var or function.";
    }
    return rawstr.str();
}

/*!
 * print successful message by converting a string into green string output
 */
std::string analysisUtil::sucMsg(std::string msg) {
    return KGRN + msg + KNRM;
}

/*!
 * print warning message by converting a string into yellow string output
 */
void analysisUtil::wrnMsg(std::string msg) {
    if(DisableWarn) return;
    outs() << KYEL + msg + KNRM << "\n";
}

/*!
 * print error message by converting a string into red string output
 */
std::string analysisUtil::errMsg(std::string msg) {
    return KRED + msg + KNRM;
}

std::string analysisUtil::bugMsg1(std::string msg) {
    return KYEL + msg + KNRM;
}

std::string analysisUtil::bugMsg2(std::string msg) {
    return KPUR + msg + KNRM;
}

std::string analysisUtil::bugMsg3(std::string msg) {
    return KCYA + msg + KNRM;
}

/*!
 * print each pass/phase message by converting a string into blue string output
 */
std::string analysisUtil::pasMsg(std::string msg) {
    return KBLU + msg + KNRM;
}

/*!
 * Dump points-to set
 */
void analysisUtil::dumpPointsToSet(unsigned node, llvm::SparseBitVector<> bs) {
    outs() << "node " << node << " points-to: {";
    dumpSet(bs);
    outs() << "}\n";
}


/*!
 * Dump alias set
 */
void analysisUtil::dumpAliasSet(unsigned node, llvm::SparseBitVector<> bs) {
    outs() << "node " << node << " alias set: {";
    dumpSet(bs);
    outs() << "}\n";
}

/*!
 * Dump bit vector set
 */
void analysisUtil::dumpSet(llvm::SparseBitVector<> bs, llvm::raw_ostream & O) {
    for (llvm::SparseBitVector<>::iterator ii = bs.begin(), ie = bs.end();
            ii != ie; ii++) {
        O << " " << *ii << " ";
    }
}

/*!
 * Print memory usage
 */
void analysisUtil::reportMemoryUsageKB(const std::string& infor, llvm::raw_ostream & O)
{
    u32_t vmrss, vmsize;
    if (getMemoryUsageKB(&vmrss, &vmsize))
        O << infor << "\tVmRSS: " << vmrss << "\tVmSize: " << vmsize << "\n";
}

/*!
 * Get memory usage
 */
bool analysisUtil::getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb)
{
    /* Get the the current process' status file from the proc filesystem */
    char buffer[8192];
    FILE* procfile = fopen("/proc/self/status", "r");
    if(procfile) {
        u32_t result = fread(buffer, sizeof(char), 8192, procfile);
        if (result == 0) {
            fputs ("Reading error\n",stderr);
        }
    }
    else {
        fputs ("/proc/self/status file not exit\n",stderr);
        return false;
    }
    fclose(procfile);

    /* Look through proc status contents line by line */
    char delims[] = "\n";
    char* line = strtok(buffer, delims);

    bool found_vmrss = false;
    bool found_vmsize = false;

    while (line != NULL && (found_vmrss == false || found_vmsize == false)) {
        if (strstr(line, "VmRSS:") != NULL)	{
            sscanf(line, "%*s %u", vmrss_kb);
            found_vmrss = true;
        }

        if (strstr(line, "VmSize:") != NULL) {
            sscanf(line, "%*s %u", vmsize_kb);
            found_vmsize = true;
        }

        line = strtok(NULL, delims);
    }

    return (found_vmrss && found_vmsize);
}

/*!
 * Increase stack size
 */
void analysisUtil::increaseStackSize()
{
    const rlim_t kStackSize = 256L * 1024L * 1024L;   // min stack size = 256 Mb
    struct rlimit rl;
    int result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0) {
        if (rl.rlim_cur < kStackSize) {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
                llvm::outs() << "setrlimit returned result = " << result << "\n";
        }
    }
}
