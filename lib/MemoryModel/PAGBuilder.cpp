//===- PAGBuilder.cpp -- PAG builder-----------------------------------------//
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
 * PAGBuilder.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/PAGBuilder.h"
#include "Util/AnalysisUtil.h"
#include "Util/CPPUtil.h"

#include <fstream>	// for PAGBuilderFromFile
#include <string>	// for PAGBuilderFromFile
#include <sstream>	// for PAGBuilderFromFile
#include <llvm/Support/CommandLine.h> // for tool output file

using namespace llvm;
using namespace std;
using namespace analysisUtil;


/*!
 * Start building PAG here
 */
PAG* PAGBuilder::build(llvm::Module& module) {
    /// initial external library information
    /// initial PAG nodes
    initalNode();
    /// initial PAG edges:
    /// handle globals
    visitGlobal(module);
    /// handle functions
    for (llvm::Module::iterator fit = module.begin(), efit = module.end();
            fit != efit; ++fit) {
        llvm::Function& fun = *fit;
        /// collect return node of function fun
        if(!analysisUtil::isExtCall(&fun)) {
            /// Return PAG node will not be created for function which can not
            /// reach the return instruction due to call to abort(), exit(),
            /// etc. In 176.gcc of SPEC 2000, function build_objc_string() from
            /// c-lang.c shows an example when fun.doesNotReturn() evaluates
            /// to TRUE because of abort().
            if(fun.doesNotReturn() == false && fun.getReturnType()->isPointerTy())
                pag->addFunRet(&fun,pag->getPAGNode(pag->getReturnNode(&fun)));
        }
        for (llvm::Function::arg_iterator I = fun.arg_begin(), E = fun.arg_end();
                I != E; ++I) {
            /// To be noted, we do not record arguments which are in declared function without body
            if(!analysisUtil::isExtCall(&fun)) {
                pag->setCurrentLocation(&*I,&fun.getEntryBlock());
                NodeID argValNodeId = pag->getValueNode(&*I);
                // if this is the function does not have caller (e.g. main)
                // or a dead function, we may create a black hole address edge for it
                if(analysisUtil::ArgInNoCallerFunction(&*I)) {
                    if(I->getType()->isPointerTy())
                        pag->addBlackHoleAddrEdge(argValNodeId);
                }
                /// We do not add arguments of main (program entry) into argument list
                /// Because, later we don't manipulate them for connection of formal and actual parameters
                else
                    pag->addFunArgs(&fun,pag->getPAGNode(argValNodeId));
            }
        }
        for (llvm::Function::iterator bit = fun.begin(), ebit = fun.end();
                bit != ebit; ++bit) {
            llvm::BasicBlock& bb = *bit;
            for (llvm::BasicBlock::iterator it = bb.begin(), eit = bb.end();
                    it != eit; ++it) {
                llvm::Instruction& inst = *it;
                pag->setCurrentLocation(&inst,&bb);
                visit(inst);
            }
        }
    }
    sanityCheck();

    pag->initialiseCandidatePointers();

    return pag;
}

/*
 * Initial all the nodes from symbol table
 */
void PAGBuilder::initalNode() {
    DBOUT(DPAGBuild, outs() << "Inital PAG Node ...\n");

    SymbolTableInfo* symTable = SymbolTableInfo::Symbolnfo();

    pag->addBlackholeObjNode();
    pag->addConstantObjNode();
    pag->addBlackholePtrNode();
    pag->addNullPtrNode();

    for (SymbolTableInfo::ValueToIDMapTy::iterator iter =
                symTable->valSyms().begin(); iter != symTable->valSyms().end();
            ++iter) {
        DBOUT(DPAGBuild, outs() << "add val node " << iter->second << "\n");
        if(iter->second == symTable->blkPtrSymID() || iter->second == symTable->nullPtrSymID())
            continue;
        pag->addValNode(iter->first, iter->second);
    }

    for (SymbolTableInfo::ValueToIDMapTy::iterator iter =
                symTable->objSyms().begin(); iter != symTable->objSyms().end();
            ++iter) {
        DBOUT(DPAGBuild, outs() << "add obj node " << iter->second << "\n");
        if(iter->second == symTable->blackholeSymID() || iter->second == symTable->constantSymID())
            continue;
        pag->addObjNode(iter->first, iter->second);
    }

    for (SymbolTableInfo::FunToIDMapTy::iterator iter =
                symTable->retSyms().begin(); iter != symTable->retSyms().end();
            ++iter) {
        DBOUT(DPAGBuild, outs() << "add ret node " << iter->second << "\n");
        pag->addRetNode(iter->first, iter->second);
    }

    for (SymbolTableInfo::FunToIDMapTy::iterator iter =
                symTable->varargSyms().begin();
            iter != symTable->varargSyms().end(); ++iter) {
        DBOUT(DPAGBuild, outs() << "add vararg node " << iter->second << "\n");
        pag->addVarargNode(iter->first, iter->second);
    }

    assert(pag->getTotalNodeNum() >= symTable->getTotalSymNum()
           && "not all node been inititalize!!!");

}

/*!
 * Return the object node offset according to GEP insn (V).
 * Given a gep edge p = q + i, if "i" is a constant then we return its offset size
 * otherwise if "i" is a variable determined by runtime, then it is a variant offset
 * Return TRUE if the offset of this GEP insn is a constant.
 */
bool PAGBuilder::computeGepOffset(const User *V, LocationSet& ls) {
    return SymbolTableInfo::Symbolnfo()->computeGepOffset(V,ls);
}

/*!
 * Handle constant expression, and connect the gep edge
 */
void PAGBuilder::processCE(const Value *val) {
    if (const Constant* ref = dyn_cast<Constant>(val)) {
        if (!isa<PointerType>(ref->getType()))
            return;
        if (const ConstantExpr* gepce = isGepConstantExpr(ref)) {
            DBOUT(DPAGBuild,
                  outs() << "handle gep constant expression " << *ref << "\n");
            const Constant* opnd = gepce->getOperand(0);
            LocationSet ls;
            bool constGep = computeGepOffset(gepce, ls);
            // must invoke pag methods here, otherwise it will be a dead recursion cycle
            const llvm::Value* cval = pag->getCurrentValue();
            const llvm::BasicBlock* cbb = pag->getCurrentBB();
            pag->setCurrentLocation(gepce, NULL);
            /*
             * The gep edge created are like constexpr (same edge may appear at multiple callsites)
             * so bb/inst of this edge may be rewritten several times, we treat it as global here.
             */
            pag->addGepEdge(pag->getValueNode(opnd), pag->getValueNode(gepce), ls, constGep);
            pag->setCurrentLocation(cval, cbb);
            // handle recursive constant express case (gep (bitcast (gep X 1)) 1)
            processCE(opnd);
        }
        else if (const ConstantExpr* castce = isCastConstantExpr(ref)) {
            DBOUT(DPAGBuild,
                  outs() << "handle cast constant expression " << *ref << "\n");
            const Constant* opnd = castce->getOperand(0);
            const llvm::Value* cval = pag->getCurrentValue();
            const llvm::BasicBlock* cbb = pag->getCurrentBB();
            pag->setCurrentLocation(castce, NULL);
            pag->addCopyEdge(pag->getValueNode(opnd), pag->getValueNode(castce));
            pag->setCurrentLocation(cval, cbb);
            processCE(opnd);
        }
        else if (const ConstantExpr* selectce = isSelectConstantExpr(ref)) {
            DBOUT(DPAGBuild,
                  outs() << "handle select constant expression " << *ref << "\n");
            const Constant* src1 = selectce->getOperand(1);
            const Constant* src2 = selectce->getOperand(2);
            const llvm::Value* cval = pag->getCurrentValue();
            const llvm::BasicBlock* cbb = pag->getCurrentBB();
            pag->setCurrentLocation(selectce, NULL);
            NodeID nsrc1 = pag->getValueNode(src1);
            NodeID nsrc2 = pag->getValueNode(src2);
            NodeID nres = pag->getValueNode(selectce);
            pag->addCopyEdge(nsrc1, nres);
            pag->addCopyEdge(nsrc2, nres);
            pag->addPhiNode(pag->getPAGNode(nres),pag->getPAGNode(nsrc1),NULL);
            pag->addPhiNode(pag->getPAGNode(nres),pag->getPAGNode(nsrc2),NULL);
            pag->setCurrentLocation(cval, cbb);
            processCE(src1);
            processCE(src2);
        }
        // if we meet a int2ptr, then it points-to black hole
        else if (const ConstantExpr* int2Ptrce = isInt2PtrConstantExpr(ref)) {
            pag->addGlobalBlackHoleAddrEdge(pag->getValueNode(int2Ptrce), int2Ptrce);
        }
    }
}
/*!
 * Get the field of the global variable node
 * FIXME:Here we only get the field that actually used in the program
 * We ignore the initialization of global variable field that not used in the program
 */
NodeID PAGBuilder::getGlobalVarField(const GlobalVariable *gvar, u32_t offset, u32_t fieldidx) {

    // if the global variable do not have any field needs to be initialized
    if (offset == 0 && gvar->getInitializer()->getType()->isSingleValueType()) {
        return getValueNode(gvar);
    }
    /// if we did not find the constant expression in the program,
    /// then we need to create a gep node for this field
    else {
        const Type *gvartype = gvar->getType();
        while (const PointerType *ptype = dyn_cast<PointerType>(gvartype))
            gvartype = ptype->getElementType();
        return pag->getGepValNode(gvar, LocationSet(offset), gvartype, fieldidx);
    }
}

/*For global variable initialization
 * Give a simple global variable
 * int x = 10;     // store 10 x  (constant, non pointer)                                      |
 * int *y = &x;    // store x y   (pointer type)
 * Given a struct
 * struct Z { int s; int *t;};
 * Global initialization:
 * struct Z z = {10,&x}; // store x z.t  (struct type)
 * struct Z *m = &z;       // store z m  (pointer type)
 * struct Z n = {10,&z.s}; // store z.s n ,  &z.s constant expression (constant expression)
 */
void PAGBuilder::InitialGlobal(const GlobalVariable *gvar, Constant *C,
                               u32_t offset, u32_t fieldidx) {
    DBOUT(DPAGBuild,
          outs() << "global " << *gvar << " constant initializer: " << *C
          << "\n");

    if (C->getType()->isSingleValueType() && isa<PointerType>(C->getType())) {
        NodeID src = getValueNode(C);
        // get the field value if it is avaiable, otherwise we create a dummy field node.
        pag->setCurrentLocation(gvar, NULL);
        NodeID field = getGlobalVarField(gvar, offset, fieldidx);

        if (isa<GlobalVariable>(C) || isa<Function>(C)) {
            pag->setCurrentLocation(C, NULL);
            pag->addStoreEdge(src, field);
        } else if (isa<ConstantExpr>(C)) {
            // add gep edge of C1 itself is a constant expression
            processCE(C);
            pag->setCurrentLocation(C, NULL);
            pag->addStoreEdge(src, field);
        } else {
            //TODO:assert(false,"what else do we have");
        }

    } else if (isa<ConstantArray>(C)) {
        if (cppUtil::isValVtbl(gvar) == false)
            for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
                InitialGlobal(gvar, cast<Constant>(C->getOperand(i)), offset, i);

    } else if (isa<ConstantStruct>(C)) {
        const StructType *sty = cast<StructType>(C->getType());
        const std::vector<u32_t>& offsetvect =
            SymbolTableInfo::Symbolnfo()->getStructOffsetVec(sty);
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++) {
            u32_t off = offsetvect[i];
            InitialGlobal(gvar, cast<Constant>(C->getOperand(i)), offset + off, i);
        }

    } else {
        //TODO:assert(false,"what else do we have");
    }
}

/*!
 *  Visit global variables for building PAG
 */
void PAGBuilder::visitGlobal(llvm::Module& module) {

    /// initialize global variable
    for (Module::global_iterator I = module.global_begin(), E =
                module.global_end(); I != E; ++I) {
        GlobalVariable * gvar = &*I;
        NodeID idx = getValueNode(gvar);
        NodeID obj = getObjectNode(gvar);
        pag->setCurrentLocation(gvar, NULL);
        pag->addAddrEdge(obj, idx);

        if (gvar->hasDefinitiveInitializer()) {
            Constant *C = gvar->getInitializer();
            DBOUT(DPAGBuild, outs() << "add global var node " << *gvar << "\n");
            InitialGlobal(gvar, C, 0, 0);
        }
    }

    /// initialize global functions
    for (Module::iterator I = module.begin(), E =
                module.end(); I != E; ++I) {
        Function * fun = &*I;
        NodeID idx = getValueNode(fun);
        NodeID obj = getObjectNode(fun);

        DBOUT(DPAGBuild, outs() << "add global function node " << fun->getName() << "\n");
        pag->setCurrentLocation(fun, NULL);
        pag->addAddrEdge(obj, idx);
    }

    // Handle global aliases (due to linkage of multiple bc files), e.g., @x = internal alias @y. We need to add a copy from y to x.
    for (Module::alias_iterator I = module.alias_begin(), E = module.alias_end(); I != E; I++) {
        NodeID dst = pag->getValueNode(&*I);
        NodeID src = pag->getValueNode((*I).getAliasee());
        processCE((*I).getAliasee());
        pag->setCurrentLocation(&*I, NULL);
        pag->addCopyEdge(src,dst);
    }
}

/*!
 * Visit alloca instructions
 * Add edge V (dst) <-- O (src), V here is a value node on PAG, O is object node on PAG
 */
void PAGBuilder::visitAllocaInst(AllocaInst &inst) {

    // AllocaInst should always be a pointer type
    assert(isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process alloca  " << inst << " \n");
    NodeID dst = getValueNode(&inst);

    NodeID src = getObjectNode(&inst);

    pag->addAddrEdge(src, dst);

}

/*!
 * Visit phi instructions
 */
void PAGBuilder::visitPHINode(PHINode &inst) {

    if (isa<PointerType>(inst.getType())) {

        DBOUT(DPAGBuild, outs() << "process phi " << inst << "  \n");

        NodeID dst = getValueNode(&inst);

        for (Size_t i = 0; i < inst.getNumIncomingValues(); ++i) {
            NodeID src = getValueNode(inst.getIncomingValue(i));
            const BasicBlock* bb = inst.getIncomingBlock(i);
            pag->addCopyEdge(src, dst);
            pag->addPhiNode(pag->getPAGNode(dst),pag->getPAGNode(src),bb);
        }
    }

}

/*
 * Visit load instructions
 */
void PAGBuilder::visitLoadInst(LoadInst &inst) {
    pag->loadInstNum++;
    if (isa<PointerType>(inst.getType())) {
        DBOUT(DPAGBuild, outs() << "process load  " << inst << " \n");

        NodeID dst = getValueNode(&inst);

        NodeID src = getValueNode(inst.getPointerOperand());

        pag->addLoadEdge(src, dst);
    }
}

/*!
 * Visit store instructions
 */
void PAGBuilder::visitStoreInst(StoreInst &inst) {
    pag->storeInstNum++;
    // StoreInst itself should always not be a pointer type
    assert(!isa<PointerType>(inst.getType()));

    if (isa<PointerType>(inst.getValueOperand()->getType())) {

        DBOUT(DPAGBuild, outs() << "process store " << inst << " \n");

        NodeID dst = getValueNode(inst.getPointerOperand());

        NodeID src = getValueNode(inst.getValueOperand());

        pag->addStoreEdge(src, dst);
    }

}

/*!
 * Visit getelementptr instructions
 */
void PAGBuilder::visitGetElementPtrInst(GetElementPtrInst &inst) {

    // GetElementPtrInst should always be a pointer or a vector contains pointers
    // TODO: for now we don't handle vector type here
    if(isa<VectorType>(inst.getType()))
        return;

    assert(isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process gep  " << inst << " \n");

    NodeID dst = getValueNode(&inst);

    NodeID src = getValueNode(inst.getPointerOperand());

    LocationSet ls;
    bool constGep = computeGepOffset(&inst, ls);
    pag->addGepEdge(src, dst, ls, constGep);
}

/*!
 * Visit intToPtr instructions
 */
void PAGBuilder::visitIntToPtrInst(llvm::IntToPtrInst &inst) {

    DBOUT(DPAGBuild, outs() << "process cast  " << inst << " \n");
    NodeID dst = getValueNode(&inst);
    pag->addBlackHoleAddrEdge(dst);
}

/*
 * Visit cast instructions
 */
void PAGBuilder::visitCastInst(CastInst &inst) {

    if (isa<PointerType>(inst.getType())) {

        DBOUT(DPAGBuild, outs() << "process cast  " << inst << " \n");
        NodeID dst = getValueNode(&inst);

        Value * opnd = inst.getOperand(0);
        if (!isa<PointerType>(opnd->getType()))
            opnd = stripAllCasts(opnd);

        if (isa<PointerType>(opnd->getType())) {
            NodeID src = getValueNode(opnd);
            pag->addCopyEdge(src, dst);
        }
        else {
            assert(isa<IntToPtrInst>(&inst) && "what else do we have??");
            // This is a int2ptr cast
            pag->addBlackHoleAddrEdge(dst);
        }
    }

}

/*!
 * Visit select instructions
 */
void PAGBuilder::visitSelectInst(SelectInst &inst) {

    if (isa<PointerType>(inst.getType())) {
        DBOUT(DPAGBuild, outs() << "process select  " << inst << " \n");

        NodeID dst = getValueNode(&inst);
        NodeID src1 = getValueNode(inst.getTrueValue());
        NodeID src2 = getValueNode(inst.getFalseValue());
        pag->addCopyEdge(src1, dst);
        pag->addCopyEdge(src2, dst);
        /// Two operands have same incoming basic block, both are the current BB
        pag->addPhiNode(pag->getPAGNode(dst),pag->getPAGNode(src1),inst.getParent());
        pag->addPhiNode(pag->getPAGNode(dst),pag->getPAGNode(src2),inst.getParent());
    }
}

/*
 * Visit callsites
 */
void PAGBuilder::visitCallSite(CallSite cs) {

    // skip llvm debug info intrinsic
    if(isInstrinsicDbgInst(cs.getInstruction()))
        return;

    DBOUT(DPAGBuild,
          outs() << "process callsite " << *cs.getInstruction() << "\n");

    /// Collect callsite arguments and returns
    for(CallSite::arg_iterator itA = cs.arg_begin(), ieA = cs.arg_end(); itA!=ieA; ++itA)
        pag->addCallSiteArgs(cs,pag->getPAGNode(getValueNode(*itA)));

    if(!cs.getType()->isVoidTy())
        pag->addCallSiteRets(cs,pag->getPAGNode(getValueNode(cs.getInstruction())));

    const Function *callee = getCallee(cs);

    if (callee) {
        if (isExtCall(callee))
            handleExtCall(cs, callee);
        else
            handleDirectCall(cs, callee);
    } else {
        //If the callee was not identified as a function (null F), this is indirect.
        handleIndCall(cs);
    }
}

/*!
 * Visit return instructions of a function
 */
void PAGBuilder::visitReturnInst(ReturnInst &inst) {

    // ReturnInst itself should always not be a pointer type
    assert(!isa<PointerType>(inst.getType()));

    //ignore void and non-ptr return statements
    if (inst.getNumOperands()
            && (isa<PointerType>(inst.getOperand(0)->getType()))) {

        DBOUT(DPAGBuild, outs() << "process return  " << inst << " \n");

        Value *src = inst.getReturnValue();
        if (!isa<PointerType>(src->getType()))
            return;

        Function *F = inst.getParent()->getParent();

        NodeID rnF = getReturnNode(F);
        NodeID vnS = getValueNode(src);
        //vnS may be null if src is a null ptr
        pag->addCopyEdge(vnS, rnF);
    }
}


/*!
 * visit extract value instructions for structures in registers
 * TODO: for now we just assume the pointer after extraction points to blackhole
 * for example %24 = extractvalue { i32, %struct.s_hash* } %call34, 0
 * %24 is a pointer points to first field of a register value %call34
 * however we can not create %call34 as an memory object, as it is register value.
 * Is that necessary treat extract value as getelementptr instruction later to get more precise results?
 */
void PAGBuilder::visitExtractValueInst(llvm::ExtractValueInst &inst) {

    if (isa<PointerType>(inst.getType())) {
        NodeID dst = getValueNode(&inst);
        pag->addBlackHoleAddrEdge(dst);
    }
}

/*!
 * The �extractelement� instruction extracts a single scalar element from a vector at a specified index.
 * TODO: for now we just assume the pointer after extraction points to blackhole
 * The first operand of an �extractelement� instruction is a value of vector type.
 * The second operand is an index indicating the position from which to extract the element.
 *
 * <result> = extractelement <4 x i32> %vec, i32 0    ; yields i32
 */
void PAGBuilder::visitExtractElementInst(llvm::ExtractElementInst &inst) {
    if (isa<PointerType>(inst.getType())) {
        NodeID dst = getValueNode(&inst);
        pag->addBlackHoleAddrEdge(dst);
    }
}

/*!
 * Add the constraints for a direct, non-external call.
 */
void PAGBuilder::handleDirectCall(CallSite cs, const Function *F) {

    assert(F);

    DBOUT(DPAGBuild,
          outs() << "handle direct call " << *cs.getInstruction() << " callee " << *F << "\n");

    //Only handle the ret.val. if it's used as a ptr.
    if (isa<PointerType>(cs.getType())) {
        NodeID dstrec = getValueNode(cs.getInstruction());
        //Does it actually return a ptr?
        if (isa<PointerType>(F->getReturnType())) {
            NodeID srcret = getReturnNode(F);
            pag->addRetEdge(srcret, dstrec, cs.getInstruction());
        } else {
            // This is a int2ptr cast during parameter passing
            pag->addBlackHoleAddrEdge(dstrec);
        }

    } else {
        DBOUT(DPAGBuild, outs() << "not a pointer, ignored\n");
    }
    //Iterators for the actual and formal parameters
    CallSite::arg_iterator itA = cs.arg_begin(), ieA = cs.arg_end();
    Function::const_arg_iterator itF = F->arg_begin(), ieF = F->arg_end();
    //Go through the fixed parameters.
    DBOUT(DPAGBuild, outs() << "      args:");
    for (; itF != ieF; ++itA, ++itF) {
        //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
        if (itA == ieA) {
            DBOUT(DPAGBuild, outs() << " !! not enough args\n");
            break;
        }
        const Value *AA = *itA, *FA = &*itF; //current actual/formal arg
        //Non-ptr formal args don't need constraints.
        if (!isa<PointerType>(FA->getType()))
            continue;

        DBOUT(DPAGBuild, outs() << "process actual parm  " << *AA << " \n");

        NodeID dstFA = getValueNode(FA);
        if (isa<PointerType>(AA->getType())) {
            NodeID srcAA = getValueNode(AA);
            pag->addCallEdge(srcAA, dstFA, cs.getInstruction());
        } else {
            // This is a int2ptr cast during parameter passing
            pag->addFormalParamBlackHoleAddrEdge(dstFA, &*itF);
        }
    }
    //Any remaining actual args must be varargs.
    if (F->isVarArg()) {
        NodeID vaF = getVarargNode(F);
        DBOUT(DPAGBuild, outs() << "\n      varargs:");
        for (; itA != ieA; ++itA) {
            Value *AA = *itA;
            if (isa<PointerType>(AA->getType())) {
                NodeID vnAA = getValueNode(AA);
                pag->addCallEdge(vnAA,vaF, cs.getInstruction());
            } else {
                // This is a int2ptr cast during parameter passing
                // pag->addBlackHoleAddrEdge(vaF);
            }
        }
    }
    if(itA != ieA) {
        /// FIXME: this assertion should be placed for correct checking except
        /// bug program like 188.ammp, 300.twolf
        wrnMsg("too many args to non-vararg func.");
        wrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");

    }
}


/*!
 * Find the base type and the max possible offset of an object pointed to by (V).
 */
const Type *PAGBuilder::getBaseTypeAndFlattenedFields(Value *V, std::vector<LocationSet> &fields) {
    return SymbolTableInfo::Symbolnfo()->getBaseTypeAndFlattenedFields(V, fields);
}

/*!
 * Add the load/store constraints and temp. nodes for the complex constraint
 * *D = *S (where D/S may point to structs).
 */
void PAGBuilder::addComplexConsForExt(Value *D, Value *S, u32_t sz) {
    assert(D && S);
    NodeID vnD= getValueNode(D), vnS= getValueNode(S);
    if(!vnD || !vnS)
        return;

    std::vector<LocationSet> fields;

    //Get the max possible size of the copy, unless it was provided.
    std::vector<LocationSet> srcFields;
    std::vector<LocationSet> dstFields;
    const Type *stype = getBaseTypeAndFlattenedFields(S, srcFields);
    const Type *dtype = getBaseTypeAndFlattenedFields(D, dstFields);
    if(srcFields.size() > dstFields.size())
        fields = dstFields;
    else
        fields = srcFields;

    /// If sz is 0, we will add edges for all fields.
    if (sz == 0)
        sz = fields.size();

    assert(fields.size() >= sz && "the number of flattened fields is smaller than size");

    //For each field (i), add (Ti = *S + i) and (*D + i = Ti).
    for (u32_t index = 0; index < sz; index++) {
        NodeID dField = pag->getGepValNode(D,fields[index],dtype,index);
        NodeID sField = pag->getGepValNode(S,fields[index],stype,index);
        NodeID dummy = pag->addDummyValNode();
        pag->addLoadEdge(sField,dummy);
        pag->addStoreEdge(dummy,dField);
    }
}


/*!
 * Handle external calls
 */
void PAGBuilder::handleExtCall(CallSite cs, const Function *callee) {
    const Instruction* inst = cs.getInstruction();
    if (isHeapAllocExtCall(inst)) {
        NodeID val = getValueNode(inst);
        NodeID obj = getObjectNode(inst);
        pag->addAddrEdge(obj, val);
    }
    else if(isStaticExtCall(inst)) {
        NodeID val = getValueNode(inst);
        NodeID obj = getObjectNode(inst);
        pag->addAddrEdge(obj, val);
    }
    else {
        if(isExtCall(callee)) {
            ExtAPI::extf_t tF= extCallTy(callee);
            switch(tF) {
            case ExtAPI::EFT_REALLOC: {
                if(!isa<PointerType>(inst->getType()))
                    break;
                // e.g. void *realloc(void *ptr, size_t size)
                // if ptr is null then we will treat it as a malloc
                // if ptr is not null, then we assume a new data memory will be attached to
                // the tail of old allocated memory block.
                if(isa<ConstantPointerNull>(cs.getArgument(0))) {
                    NodeID val = getValueNode(inst);
                    NodeID obj = getObjectNode(inst);
                    pag->addAddrEdge(obj, val);
                }
                break;
            }
            case ExtAPI::EFT_L_A0:
            case ExtAPI::EFT_L_A1:
            case ExtAPI::EFT_L_A2:
            case ExtAPI::EFT_L_A8: {
                if(!isa<PointerType>(inst->getType()))
                    break;
                NodeID dstNode = getValueNode(inst);
                Size_t arg_pos;
                switch(tF) {
                case ExtAPI::EFT_L_A1:
                    arg_pos= 1;
                    break;
                case ExtAPI::EFT_L_A2:
                    arg_pos= 2;
                    break;
                case ExtAPI::EFT_L_A8:
                    arg_pos= 8;
                    break;
                default:
                    arg_pos= 0;
                }
                Value *src= cs.getArgument(arg_pos);
                if(isa<PointerType>(src->getType())) {
                    NodeID srcNode = getValueNode(src);
                    pag->addCopyEdge(srcNode, dstNode);
                } else
                    pag->addBlackHoleAddrEdge(dstNode);
                break;
                break;
            }
            case ExtAPI::EFT_L_A0__A0R_A1R: {
                addComplexConsForExt(cs.getArgument(0), cs.getArgument(1));
                //memcpy returns the dest.
                if(isa<PointerType>(inst->getType())) {
                    pag->addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
                }
                break;
            }
            case ExtAPI::EFT_A1R_A0R:
                addComplexConsForExt(cs.getArgument(1), cs.getArgument(0));
                break;
            case ExtAPI::EFT_A3R_A1R_NS:
                //These func. are never used to copy structs, so the size is 1.
                addComplexConsForExt(cs.getArgument(3), cs.getArgument(1), 1);
                break;
            case ExtAPI::EFT_A1R_A0: {
                NodeID vnD= getValueNode(cs.getArgument(1));
                NodeID vnS= getValueNode(cs.getArgument(0));
                if(vnD && vnS)
                    pag->addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A2R_A1: {
                NodeID vnD= getValueNode(cs.getArgument(2));
                NodeID vnS= getValueNode(cs.getArgument(1));
                if(vnD && vnS)
                    pag->addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A4R_A1: {
                NodeID vnD= getValueNode(cs.getArgument(4));
                NodeID vnS= getValueNode(cs.getArgument(1));
                if(vnD && vnS)
                    pag->addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_L_A0__A2R_A0: {
                if(isa<PointerType>(inst->getType())) {
                    //Do the L_A0 part if the retval is used.
                    NodeID vnD= getValueNode(inst);
                    Value *src= cs.getArgument(0);
                    if(isa<PointerType>(src->getType())) {
                        NodeID vnS= getValueNode(src);
                        if(vnS)
                            pag->addCopyEdge(vnS,vnD);
                    } else
                        pag->addBlackHoleAddrEdge(vnD);
                }
                //Do the A2R_A0 part.
                NodeID vnD= getValueNode(cs.getArgument(2));
                NodeID vnS= getValueNode(cs.getArgument(0));
                if(vnD && vnS)
                    pag->addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A0R_NEW:
            case ExtAPI::EFT_A1R_NEW:
            case ExtAPI::EFT_A2R_NEW:
            case ExtAPI::EFT_A4R_NEW:
            case ExtAPI::EFT_A11R_NEW: {
                //TODO:: handle case here
                break;
            }
            case ExtAPI::EFT_ALLOC:
            case ExtAPI::EFT_NOSTRUCT_ALLOC:
            case ExtAPI::EFT_STAT:
            case ExtAPI::EFT_STAT2:
                if(isa<PointerType>(inst->getType()))
                    assert(!"alloc type func. are not handled here");
                else {
                    // fdopen will return an integer in LLVM IR.
                    wrnMsg("alloc type func do not return pointer type");
                }
                break;
            case ExtAPI::EFT_NOOP:
            case ExtAPI::EFT_FREE:
                break;
            case ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE: {
                Value *vArg1 = cs.getArgument(1);
                Value *vArg3 = cs.getArgument(3);

                // We have vArg3 points to the entry of _Rb_tree_node_base { color; parent; left; right; }.
                // Now we calculate the offset from base to vArg3
                NodeID vnArg3 = pag->getValueNode(vArg3);
                Size_t offset = pag->getLocationSetFromBaseNode(vnArg3).getOffset();

                // We get all flattened fields of base
                vector<LocationSet> fields;
                const Type *type = getBaseTypeAndFlattenedFields(vArg3, fields);
                assert(fields.size() >= 4 && "_Rb_tree_node_base should have at least 4 fields.\n");

                // We summarize the side effects: arg3->parent = arg1, arg3->left = arg1, arg3->right = arg1
                // Note that arg0 is aligned with "offset".
                for (int i = offset + 1; i <= offset + 3; ++i) {
                    NodeID vnD = pag->getGepValNode(vArg3, fields[i], type, i);
                    NodeID vnS = getValueNode(vArg1);
                    if(vnD && vnS)
                        pag->addStoreEdge(vnS,vnD);
                }
                break;
            }
            case ExtAPI::EFT_STD_RB_TREE_INCREMENT: {
                NodeID vnD = pag->getValueNode(inst);

                Value *vArg = cs.getArgument(0);
                NodeID vnArg = pag->getValueNode(vArg);
                Size_t offset = pag->getLocationSetFromBaseNode(vnArg).getOffset();

                // We get all fields
                vector<LocationSet> fields;
                const Type *type = getBaseTypeAndFlattenedFields(vArg,fields);
                assert(fields.size() >= 4 && "_Rb_tree_node_base should have at least 4 fields.\n");

                // We summarize the side effects: ret = arg->parent, ret = arg->left, ret = arg->right
                // Note that arg0 is aligned with "offset".
                for (int i = offset + 1; i <= offset + 3; ++i) {
                    NodeID vnS = pag->getGepValNode(vArg, fields[i], type, i);
                    if(vnD && vnS)
                        pag->addStoreEdge(vnS,vnD);
                }
                break;
            }
            case ExtAPI::CPP_EFT_A0R_A1: {
                SymbolTableInfo* symTable = SymbolTableInfo::Symbolnfo();
                if (symTable->getModelConstants()) {
                    NodeID vnD = pag->getValueNode(cs.getArgument(0));
                    NodeID vnS = pag->getValueNode(cs.getArgument(1));
                    pag->addStoreEdge(vnS, vnD);
                }
                break;
            }
            case ExtAPI::CPP_EFT_A0R_A1R: {
                SymbolTableInfo* symTable = SymbolTableInfo::Symbolnfo();
                if (symTable->getModelConstants()) {
                    NodeID vnD = getValueNode(cs.getArgument(0));
                    NodeID vnS = getValueNode(cs.getArgument(1));
                    assert(vnD && vnS && "dst or src not exist?");
                    NodeID dummy = pag->addDummyValNode();
                    pag->addLoadEdge(vnS,dummy);
                    pag->addStoreEdge(dummy,vnD);
                }
                break;
            }
            case ExtAPI::CPP_EFT_A1R: {
                SymbolTableInfo* symTable = SymbolTableInfo::Symbolnfo();
                if (symTable->getModelConstants()) {
                    NodeID vnS = getValueNode(cs.getArgument(1));
                    assert(vnS && "src not exist?");
                    NodeID dummy = pag->addDummyValNode();
                    pag->addLoadEdge(vnS,dummy);
                }
                break;
            }
            //default:
            case ExtAPI::EFT_OTHER: {
                if(isa<PointerType>(inst->getType())) {
                    std::string str;
                    raw_string_ostream rawstr(str);
                    rawstr << "function " << callee->getName() << " not in the external function summary list";
                    wrnMsg(rawstr.str());
                    //assert("unknown ext.func type");
                }
            }
            }
        }

        /// create inter-procedural PAG edges for thread forks
        if(isThreadForkCall(inst)) {
            if(const Function* forkedFun = getLLVMFunction(getForkedFun(inst)) ) {
                const Value* actualParm = getActualParmAtForkSite(inst);
                /// pthread_create has 1 arg.
                /// apr_thread_create has 2 arg.
                assert((forkedFun->arg_size() <= 2) && "Size of formal parameter of start routine should be one");
                if(forkedFun->arg_size() <= 2 && forkedFun->arg_size() >= 1) {
                    const Argument* formalParm = &(forkedFun->getArgumentList().front());
                    /// Connect actual parameter to formal parameter of the start routine
                    if(isa<PointerType>(actualParm->getType()) && isa<PointerType>(formalParm->getType()) )
                        pag->addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),inst);
                }
            }
            else {
                /// handle indirect calls at pthread create APIs e.g., pthread_create(&t1, NULL, fp, ...);
                ///const llvm::Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                ///if(!isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
            /// If forkedFun does not pass to spawnee as function type but as void pointer
            /// remember to update inter-procedural callgraph/PAG/SVFG etc. when indirect call targets are resolved
            /// We don't connect the callgraph here, further investigation is need to hanle mod-ref during SVFG construction.
        }

        /// create inter-procedural PAG edges for hare_parallel_for calls
        else if(isHareParForCall(inst)) {
            if(const Function* taskFunc = getLLVMFunction(getTaskFuncAtHareParForSite(inst)) ) {
                /// The task function of hare_parallel_for has 3 args.
                assert((taskFunc->arg_size() == 3) && "Size of formal parameter of hare_parallel_for's task routine should be 3");
                const Value* actualParm = getTaskDataAtHareParForSite(inst);
                const Argument* formalParm = &(taskFunc->getArgumentList().front());
                /// Connect actual parameter to formal parameter of the start routine
                if(isa<PointerType>(actualParm->getType()) && isa<PointerType>(formalParm->getType()) )
                    pag->addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),inst);
            }
            else {
                /// handle indirect calls at hare_parallel_for (e.g., hare_parallel_for(..., fp, ...);
                ///const llvm::Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                ///if(!isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
        }

        /// TODO: inter-procedural PAG edges for thread joins
    }
}

/*!
 * Indirect call is resolved on-the-fly during pointer analysis
 */
void PAGBuilder::handleIndCall(CallSite cs) {
    pag->addIndirectCallsites(cs,pag->getValueNode(cs.getCalledValue()));
}

/*
 * TODO: more sanity checks might be needed here
 */
void PAGBuilder::sanityCheck() {
    for (PAG::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter) {
        (void) pag->getPAGNode(nIter->first);
        //TODO::
        // (1)  every source(root) node of a pag tree should be object node
        //       if a node has no incoming edge, but has outgoing edges
        //       then it has to be an object node.
        // (2)  make sure every variable should be initialized
        //      otherwise it causes the a null pointer, the aliasing relation may not be captured
        //      when loading a pointer value should make sure
        //      some value has been store into this pointer before
        //      q = load p, some value should stored into p first like store w p;
        // (3)  make sure PAGNode should not have a const expr value (pointer should have unique def)
        // (4)  look closely into addComplexConsForExt, make sure program locations(e.g.,inst bb)
        //      are set correctly for dummy gepval node
        // (5)  reduce unnecessary copy edge (const casts) and ensure correctness.
    }
}

/*
 * You can build a PAG from a file written by yourself
 *
 * The file should follow the format:
 * Node:  nodeID Nodetype
 * Edge:  nodeID edgetype NodeID Offset
 *
 * like:
 * 1 o
 * 2 v
 * 3 v
 * 4 v
 * 1 addr 2 0
 * 1 addr 3 0
 * 3 gep 4 4
 */
PAG* PAGBuilderFromFile::build() {

    string line;
    ifstream myfile(file.c_str());
    if (myfile.is_open()) {
        while (myfile.good()) {
            getline(myfile, line);

            Size_t token_count = 0;
            string tmps;
            istringstream ss(line);
            while (ss.good()) {
                ss >> tmps;
                token_count++;
            }

            if (token_count == 0)
                continue;
            else if (token_count == 2) {
                NodeID nodeId;
                string nodetype;
                istringstream ss(line);
                ss >> nodeId;
                ss >> nodetype;
                outs() << "reading node :" << nodeId << "\n";
                if (nodetype == "v")
                    pag->addDummyValNode(nodeId);
                else if (nodetype == "o") {
                    pag->addDummyObjNode();
                }
                else
                    assert(
                        false
                        && "format not support, pls specify node type");
            }

            // do not consider gep edge
            else if (token_count == 3) {
                NodeID nodeSrc;
                NodeID nodeDst;
                string edge;
                istringstream ss(line);
                ss >> nodeSrc;
                ss >> edge;
                ss >> nodeDst;
                outs() << "reading edge :" << nodeSrc << " " << edge << " "
                       << nodeDst << " \n";
                addEdge(nodeSrc, nodeDst, 0, edge);
            }

            // do consider gep edge
            else if (token_count == 4) {
                NodeID nodeSrc;
                NodeID nodeDst;
                Size_t offsetOrCSId;
                string edge;
                istringstream ss(line);
                ss >> nodeSrc;
                ss >> edge;
                ss >> nodeDst;
                ss >> offsetOrCSId;
                outs() << "reading edge :" << nodeSrc << " " << edge << " "
                       << nodeDst << " offsetOrCSId=" << offsetOrCSId << " \n";
                addEdge(nodeSrc, nodeDst, offsetOrCSId, edge);
            } else
                outs() << "format not support, token count = " << token_count
                       << "\n";
        }
        myfile.close();
    }

    else
        outs() << "Unable to open file\n";

    return pag;

}

/*!
 * Add PAG edge according to a file format
 */
void PAGBuilderFromFile::addEdge(NodeID srcID, NodeID dstID,
                                 Size_t offsetOrCSId, std::string edge) {

    //check whether these two nodes available
    PAGNode* srcNode = pag->getPAGNode(srcID);
    PAGNode* dstNode = pag->getPAGNode(dstID);

    if (edge == "addr")
        pag->addAddrEdge(srcID, dstID);
    else if (edge == "copy")
        pag->addCopyEdge(srcID, dstID);
    else if (edge == "load")
        pag->addLoadEdge(srcID, dstID);
    else if (edge == "store")
        pag->addStoreEdge(srcID, dstID);
    else if (edge == "normal-gep")
        pag->addNormalGepEdge(srcID, dstID, LocationSet(offsetOrCSId));
    else if (edge == "variant-gep")
        pag->addVariantGepEdge(srcID, dstID);
    else if (edge == "call")
        pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, NULL));
    else if (edge == "ret")
        pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, NULL));
    else
        assert(false && "format not support, can not create such edge");
}
