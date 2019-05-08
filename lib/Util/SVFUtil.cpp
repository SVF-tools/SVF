//===- AnalysisUtil.cpp -- Helper functions for pointer analysis--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * AnalysisUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "Util/SVFUtil.h"
#include "Util/Conditions.h"
#include <sys/resource.h>		/// increase stack size



/// Color for output format
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KPUR  "\x1B[1;35m"
#define KCYA  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"

static llvm::cl::opt<bool> DisableWarn("dwarn", llvm::cl::init(true),
                                 llvm::cl::desc("Disable warning"));

/*!
 * A value represents an object if it is
 * 1) function,
 * 2) global
 * 3) stack
 * 4) heap
 */
bool SVFUtil::isObject(const Value * ref) {
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
 * Return true if it is an intric debug instruction
 */
bool SVFUtil::isInstrinsicDbgInst(const Instruction* inst) {
    return SVFUtil::isa<llvm::DbgInfoIntrinsic>(inst);
}

/*!
 * Return reachable bbs from function entry
 */
void SVFUtil::getFunReachableBBs (const Function * fun, DominatorTree* dt, std::vector<const BasicBlock*> &reachableBBs) {
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
bool SVFUtil::functionDoesNotRet (const Function * fun) {

    std::vector<const BasicBlock*> bbVec;
    std::set<const BasicBlock*> visited;
    bbVec.push_back(&fun->getEntryBlock());
    while(!bbVec.empty()) {
        const BasicBlock* bb = bbVec.back();
        bbVec.pop_back();
        for (BasicBlock::const_iterator it = bb->begin(), eit = bb->end();
                it != eit; ++it) {
            if(SVFUtil::isa<ReturnInst>(*it))
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
bool SVFUtil::isDeadFunction (const Function * fun) {
    if(fun->hasAddressTaken())
        return false;
    if(isProgEntryFunction(fun))
        return false;
    for (Value::const_user_iterator i = fun->user_begin(), e = fun->user_end(); i != e; ++i) {
        if (SVFUtil::isa<CallInst>(*i) || SVFUtil::isa<InvokeInst>(*i))
            return false;
    }
    SVFModule svfModule;
    if (svfModule.hasDeclaration(fun)) {
        const SVFModule::FunctionSetType &decls = svfModule.getDeclaration(fun);
        for (SVFModule::FunctionSetType::const_iterator it = decls.begin(),
                eit = decls.end(); it != eit; ++it) {
            const Function *decl = *it;
            if(decl->hasAddressTaken())
                return false;
            for (Value::const_user_iterator i = decl->user_begin(), e = decl->user_end(); i != e; ++i) {
                if (SVFUtil::isa<CallInst>(*i) || SVFUtil::isa<InvokeInst>(*i))
                    return false;
            }
        }
    }
    return true;
}

/*!
 * Return true if this is a value in a dead function (function without any caller)
 */
bool SVFUtil::isPtrInDeadFunction (const Value * value) {
    if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(value)) {
        if(isDeadFunction(inst->getParent()->getParent()))
            return true;
    }
    else if(const Argument* arg = SVFUtil::dyn_cast<Argument>(value)) {
        if(isDeadFunction(arg->getParent()))
            return true;
    }
    return false;
}

/*!
 * Strip constant casts
 */
const Value * SVFUtil::stripConstantCasts(const Value *val) {
    if (SVFUtil::isa<GlobalValue>(val) || isInt2PtrConstantExpr(val))
        return val;
    else if (const ConstantExpr *CE = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if (Instruction::isCast(CE->getOpcode()))
            return stripConstantCasts(CE->getOperand(0));
    }
    return val;
}

/*!
 * Strip all casts
 */
Value * SVFUtil::stripAllCasts(Value *val) {
    while (true) {
        if (CastInst *ci = SVFUtil::dyn_cast<CastInst>(val)) {
            val = ci->getOperand(0);
        } else if (ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(val)) {
            if(ce->isCast())
                val = ce->getOperand(0);
        } else {
            return val;
        }
    }
    return NULL;
}

/// Get the next instructions following control flow
void SVFUtil::getNextInsts(const Instruction* curInst, std::vector<const Instruction*>& instList) {
	if (!curInst->isTerminator()) {
		const Instruction* nextInst = curInst->getNextNode();
		if (isInstrinsicDbgInst(nextInst))
			getNextInsts(nextInst, instList);
		else
			instList.push_back(nextInst);
	} else {
		const BasicBlock *BB = curInst->getParent();
		// Visit all successors of BB in the CFG
		for (succ_const_iterator it = succ_begin(BB), ie = succ_end(BB); it != ie; ++it) {
			const Instruction* nextInst = &((*it)->front());
			if (isInstrinsicDbgInst(nextInst))
				getNextInsts(nextInst, instList);
			else
				instList.push_back(nextInst);
		}
	}
}


/// Get the previous instructions following control flow
void SVFUtil::getPrevInsts(const Instruction* curInst, std::vector<const Instruction*>& instList) {
	if (curInst != &(curInst->getParent()->front())) {
		const Instruction* prevInst = curInst->getPrevNode();
		if (isInstrinsicDbgInst(prevInst))
			getPrevInsts(prevInst, instList);
		else
			instList.push_back(prevInst);
	} else {
		const BasicBlock *BB = curInst->getParent();
		// Visit all successors of BB in the CFG
		for (const_pred_iterator it = pred_begin(BB), ie = pred_end(BB); it != ie; ++it) {
			const Instruction* prevInst = &((*it)->back());
			if (isInstrinsicDbgInst(prevInst))
				getPrevInsts(prevInst, instList);
			else
				instList.push_back(prevInst);
		}
	}
}


/*!
 * Return the type of the object from a heap allocation
 */
const Type* SVFUtil::getTypeOfHeapAlloc(const Instruction *inst){
    const PointerType* type = SVFUtil::dyn_cast<PointerType>(inst->getType());

	if(isHeapAllocExtCallViaRet(inst)){
		const Instruction* nextInst = inst->getNextNode();
		if(nextInst && nextInst->getOpcode() == Instruction::BitCast)
	           // we only consider bitcast instructions and ignore others (e.g., IntToPtr and ZExt)
	            type = SVFUtil::dyn_cast<PointerType>(inst->getNextNode()->getType());
	}
	else if(isHeapAllocExtCallViaArg(inst)){
	    CallSite cs = getLLVMCallSite(inst);
        int arg_pos = getHeapAllocHoldingArgPosition(getCallee(cs));
        const Value *arg = cs.getArgument(arg_pos);
        type = SVFUtil::dyn_cast<PointerType>(arg->getType());
	}
	else{
	    assert( false && "not a heap allocation instruction?");
	}

	assert(type && "not a pointer type?");
	return type->getElementType();
}

/*!
 * Get position of a successor basic block
 */
u32_t SVFUtil::getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ) {
    u32_t i = 0;
    for (const BasicBlock *SuccBB: successors(BB)) {
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
u32_t SVFUtil::getBBPredecessorPos(const BasicBlock *bb, const BasicBlock *succbb) {
    u32_t pos = 0;
    for (const_pred_iterator it = pred_begin(succbb), et = pred_end(succbb); it != et; ++it, ++pos) {
        if(*it==bb)
            return pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

/*!
 *  Get the num of BB's successors
 */
u32_t SVFUtil::getBBSuccessorNum(const BasicBlock *BB) {
    return BB->getTerminator()->getNumSuccessors();
}

/*!
 * Get the num of BB's predecessors
 */
u32_t SVFUtil::getBBPredecessorNum(const BasicBlock *BB) {
    u32_t num = 0;
    for (const_pred_iterator it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
        num++;
    return num;
}

/*!
 * Get source code line number of a function according to debug info
 */
std::string SVFUtil::getSourceLocOfFunction(const Function *F)
{
    std::string str;
    raw_string_ostream rawstr(str);
   /*	
    * https://reviews.llvm.org/D18074?id=50385	
    * looks like the relevant	
    */
    if (llvm::DISubprogram *SP =  F->getSubprogram()) {
        if (SP->describes(F))
            rawstr << "in line: " << SP->getLine() << " file: " << SP->getFilename();
    }
    return rawstr.str();
}

/*!
 * Get the meta data (line number and file name) info of a LLVM value
 */
std::string SVFUtil::getSourceLoc(const Value* val) {
    if(val==NULL)  return "empty val";

    std::string str;
    raw_string_ostream rawstr(str);
    if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(val)) {
        if (SVFUtil::isa<AllocaInst>(inst)) {
            for (llvm::DbgInfoIntrinsic *DII : FindDbgAddrUses(const_cast<Instruction*>(inst))) {
                if (llvm::DbgDeclareInst *DDI = SVFUtil::dyn_cast<llvm::DbgDeclareInst>(DII)) {
                	llvm::DIVariable *DIVar = SVFUtil::cast<llvm::DIVariable>(DDI->getVariable());
                    rawstr << "ln: " << DIVar->getLine() << " fl: " << DIVar->getFilename();
                    break;
                }
            }
        }
        else if (MDNode *N = inst->getMetadata("dbg")) { // Here I is an LLVM instruction
        	llvm::DILocation* Loc = SVFUtil::cast<llvm::DILocation>(N);                   // DILocation is in DebugInfo.h
            unsigned Line = Loc->getLine();
            StringRef File = Loc->getFilename();
            //StringRef Dir = Loc.getDirectory();
            rawstr << "ln: " << Line << " fl: " << File;
        }
    }
    else if (const Argument* argument = SVFUtil::dyn_cast<Argument>(val)) {
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
    else if (const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val)) {
        rawstr << "Glob ";
        NamedMDNode* CU_Nodes = gvar->getParent()->getNamedMetadata("llvm.dbg.cu");
        if(CU_Nodes) {
            for (unsigned i = 0, e = CU_Nodes->getNumOperands(); i != e; ++i) {
            	llvm::DICompileUnit *CUNode = SVFUtil::cast<llvm::DICompileUnit>(CU_Nodes->getOperand(i));
                for (llvm::DIGlobalVariableExpression *GV : CUNode->getGlobalVariables()) {
                	llvm::DIGlobalVariable * DGV = GV->getVariable();

                    if(DGV->getName() == gvar->getName()){
                        rawstr << "ln: " << DGV->getLine() << " fl: " << DGV->getFilename();
                    }

                }
            }
        }
    }
    else if (const Function* func = SVFUtil::dyn_cast<Function>(val)) {
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
std::string SVFUtil::sucMsg(std::string msg) {
    return KGRN + msg + KNRM;
}

/*!
 * print warning message by converting a string into yellow string output
 */
void SVFUtil::wrnMsg(std::string msg) {
    if(DisableWarn) return;
    outs() << KYEL + msg + KNRM << "\n";
}

/*!
 * print error message by converting a string into red string output
 */
std::string SVFUtil::errMsg(std::string msg) {
    return KRED + msg + KNRM;
}

std::string SVFUtil::bugMsg1(std::string msg) {
    return KYEL + msg + KNRM;
}

std::string SVFUtil::bugMsg2(std::string msg) {
    return KPUR + msg + KNRM;
}

std::string SVFUtil::bugMsg3(std::string msg) {
    return KCYA + msg + KNRM;
}

/*!
 * print each pass/phase message by converting a string into blue string output
 */
std::string SVFUtil::pasMsg(std::string msg) {
    return KBLU + msg + KNRM;
}

/*!
 * Dump points-to set
 */
void SVFUtil::dumpPointsToSet(unsigned node, NodeBS bs) {
    outs() << "node " << node << " points-to: {";
    dumpSet(bs);
    outs() << "}\n";
}


/*!
 * Dump alias set
 */
void SVFUtil::dumpAliasSet(unsigned node, NodeBS bs) {
    outs() << "node " << node << " alias set: {";
    dumpSet(bs);
    outs() << "}\n";
}

/*!
 * Dump bit vector set
 */
void SVFUtil::dumpSet(NodeBS bs, raw_ostream & O) {
    for (NodeBS::iterator ii = bs.begin(), ie = bs.end();
            ii != ie; ii++) {
        O << " " << *ii << " ";
    }
}

/*!
 * Print memory usage
 */
void SVFUtil::reportMemoryUsageKB(const std::string& infor, raw_ostream & O)
{
    u32_t vmrss, vmsize;
    if (getMemoryUsageKB(&vmrss, &vmsize))
        O << infor << "\tVmRSS: " << vmrss << "\tVmSize: " << vmsize << "\n";
}

/*!
 * Get memory usage
 */
bool SVFUtil::getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb)
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
void SVFUtil::increaseStackSize()
{
    const rlim_t kStackSize = 256L * 1024L * 1024L;   // min stack size = 256 Mb
    struct rlimit rl;
    int result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0) {
        if (rl.rlim_cur < kStackSize) {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
                outs() << "setrlimit returned result = " << result << "\n";
        }
    }
}

/*
 * Reference functions:
 * llvm::parseIRFile (lib/IRReader/IRReader.cpp)
 * llvm::parseIR (lib/IRReader/IRReader.cpp)
 */
bool SVFUtil::isIRFile(const std::string &filename) {
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
                                    std::vector<std::string> &moduleNameVec) {
    bool first_ir_file = true;
    for (s32_t i = 0; i < argc; ++i) {
        std::string argument(argv[i]);
        if (SVFUtil::isIRFile(argument)) {
            if (find(moduleNameVec.begin(), moduleNameVec.end(), argument)
                    == moduleNameVec.end())
                moduleNameVec.push_back(argument);
            if (first_ir_file) {
                arg_value[arg_num] = argv[i];
                arg_num++;
                first_ir_file = false;
            }
        } else {
            arg_value[arg_num] = argv[i];
            arg_num++;
        }
    }
}
