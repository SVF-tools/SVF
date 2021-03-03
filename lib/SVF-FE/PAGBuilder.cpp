//===- PAGBuilder.cpp -- PAG builder-----------------------------------------//
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
 * PAGBuilder.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/PAGBuilder.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CPPUtil.h"
#include "Graphs/ExternalPAG.h"
#include "Util/BasicTypes.h"
#include "MemoryModel/PAGBuilderFromFile.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;


/*!
 * Start building PAG here
 */
PAG* PAGBuilder::build(SVFModule* svfModule)
{

    // We read PAG from a user-defined txt instead of parsing PAG from LLVM IR
    if (SVFModule::pagReadFromTXT())
    {
        PAGBuilderFromFile fileBuilder(SVFModule::pagFileName());
        return fileBuilder.build();
    }

    // If the PAG has been built before, then we return the unique PAG of the program
    if(pag->getNodeNumAfterPAGBuild() > 1)
    	return pag;

    svfMod = svfModule;

    /// initial external library information
    /// initial PAG nodes
    initialiseNodes();
    /// initial PAG edges:
    ///// handle globals
    visitGlobal(svfModule);
    ///// collect exception vals in the program

    ExternalPAG::initialise(svfModule);

    /// handle functions
    for (SVFModule::iterator fit = svfModule->begin(), efit = svfModule->end();
            fit != efit; ++fit)
    {
        const SVFFunction& fun = **fit;
        /// collect return node of function fun
        if(!SVFUtil::isExtCall(&fun))
        {
            /// Return PAG node will not be created for function which can not
            /// reach the return instruction due to call to abort(), exit(),
            /// etc. In 176.gcc of SPEC 2000, function build_objc_string() from
            /// c-lang.c shows an example when fun.doesNotReturn() evaluates
            /// to TRUE because of abort().
            if(fun.getLLVMFun()->doesNotReturn() == false && fun.getLLVMFun()->getReturnType()->isVoidTy() == false)
                pag->addFunRet(&fun,pag->getPAGNode(pag->getReturnNode(&fun)));

            /// To be noted, we do not record arguments which are in declared function without body
            /// TODO: what about external functions with PAG imported by commandline?
            for (Function::arg_iterator I = fun.getLLVMFun()->arg_begin(), E = fun.getLLVMFun()->arg_end();
                    I != E; ++I) {
                setCurrentLocation(&*I,&fun.getLLVMFun()->getEntryBlock());
                NodeID argValNodeId = pag->getValueNode(&*I);
                // if this is the function does not have caller (e.g. main)
                // or a dead function, shall we create a black hole address edge for it?
                // it is (1) too conservative, and (2) make FormalParmVFGNode defined at blackhole address PAGEdge.
                // if(SVFUtil::ArgInNoCallerFunction(&*I)) {
                //    if(I->getType()->isPointerTy())
                //        addBlackHoleAddrEdge(argValNodeId);
                //}
                pag->addFunArgs(&fun,pag->getPAGNode(argValNodeId));
            }
        }
        for (Function::iterator bit = fun.getLLVMFun()->begin(), ebit = fun.getLLVMFun()->end();
                bit != ebit; ++bit)
        {
            BasicBlock& bb = *bit;
            for (BasicBlock::iterator it = bb.begin(), eit = bb.end();
                    it != eit; ++it)
            {
                Instruction& inst = *it;
                setCurrentLocation(&inst,&bb);
                visit(inst);
            }
        }
    }

    sanityCheck();

    pag->initialiseCandidatePointers();

    pag->setNodeNumAfterPAGBuild(pag->getTotalNodeNum());

    return pag;
}

/*
 * Initial all the nodes from symbol table
 */
void PAGBuilder::initialiseNodes()
{
    DBOUT(DPAGBuild, outs() << "Initialise PAG Nodes ...\n");

    SymbolTableInfo* symTable = SymbolTableInfo::SymbolInfo();

    pag->addBlackholeObjNode();
    pag->addConstantObjNode();
    pag->addBlackholePtrNode();
    addNullPtrNode();

    for (SymbolTableInfo::ValueToIDMapTy::iterator iter =
                symTable->valSyms().begin(); iter != symTable->valSyms().end();
            ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add val node " << iter->second << "\n");
        if(iter->second == symTable->blkPtrSymID() || iter->second == symTable->nullPtrSymID())
            continue;
        pag->addValNode(iter->first, iter->second);
    }

    for (SymbolTableInfo::ValueToIDMapTy::iterator iter =
                symTable->objSyms().begin(); iter != symTable->objSyms().end();
            ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add obj node " << iter->second << "\n");
        if(iter->second == symTable->blackholeSymID() || iter->second == symTable->constantSymID())
            continue;
        pag->addObjNode(iter->first, iter->second);
    }

    for (SymbolTableInfo::FunToIDMapTy::iterator iter =
                symTable->retSyms().begin(); iter != symTable->retSyms().end();
            ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add ret node " << iter->second << "\n");
        const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(iter->first);
        pag->addRetNode(fun, iter->second);
    }

    for (SymbolTableInfo::FunToIDMapTy::iterator iter =
                symTable->varargSyms().begin();
            iter != symTable->varargSyms().end(); ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add vararg node " << iter->second << "\n");
        const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(iter->first);
        pag->addVarargNode(fun, iter->second);
    }

    /// add address edges for constant nodes.
    for (SymbolTableInfo::ValueToIDMapTy::iterator iter =
                symTable->objSyms().begin(); iter != symTable->objSyms().end(); ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add address edges for constant node " << iter->second << "\n");
        const Value* val = iter->first;
        if (symTable->isConstantObjSym(val))
        {
            NodeID ptr = pag->getValueNode(val);
            if(ptr!= pag->getBlkPtr() && ptr!= pag->getNullPtr())
            {
                setCurrentLocation(val, nullptr);
                addAddrEdge(iter->second, ptr);
            }
        }
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
bool PAGBuilder::computeGepOffset(const User *V, LocationSet& ls)
{
    return SymbolTableInfo::SymbolInfo()->computeGepOffset(V,ls);
}

/*!
 * Handle constant expression, and connect the gep edge
 */
void PAGBuilder::processCE(const Value *val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* gepce = isGepConstantExpr(ref))
        {
            DBOUT(DPAGBuild,
                  outs() << "handle gep constant expression " << *ref << "\n");
            const Constant* opnd = gepce->getOperand(0);
            // handle recursive constant express case (gep (bitcast (gep X 1)) 1)
            processCE(opnd);
            LocationSet ls;
            bool constGep = computeGepOffset(gepce, ls);
            // must invoke pag methods here, otherwise it will be a dead recursion cycle
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(gepce, nullptr);
            /*
             * The gep edge created are like constexpr (same edge may appear at multiple callsites)
             * so bb/inst of this edge may be rewritten several times, we treat it as global here.
             */
            addGepEdge(pag->getValueNode(opnd), pag->getValueNode(gepce), ls, constGep);
            setCurrentLocation(cval, cbb);
        }
        else if (const ConstantExpr* castce = isCastConstantExpr(ref))
        {
            DBOUT(DPAGBuild,
                  outs() << "handle cast constant expression " << *ref << "\n");
            const Constant* opnd = castce->getOperand(0);
            processCE(opnd);
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(castce, nullptr);
            addCopyEdge(pag->getValueNode(opnd), pag->getValueNode(castce));
            setCurrentLocation(cval, cbb);
        }
        else if (const ConstantExpr* selectce = isSelectConstantExpr(ref))
        {
            DBOUT(DPAGBuild,
                  outs() << "handle select constant expression " << *ref << "\n");
            const Constant* src1 = selectce->getOperand(1);
            const Constant* src2 = selectce->getOperand(2);
            processCE(src1);
            processCE(src2);
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(selectce, nullptr);
            NodeID nsrc1 = pag->getValueNode(src1);
            NodeID nsrc2 = pag->getValueNode(src2);
            NodeID nres = pag->getValueNode(selectce);
            const CopyPE* cpy1 = addCopyEdge(nsrc1, nres);
            const CopyPE* cpy2 = addCopyEdge(nsrc2, nres);
            pag->addPhiNode(pag->getPAGNode(nres),cpy1);
            pag->addPhiNode(pag->getPAGNode(nres),cpy2);
            setCurrentLocation(cval, cbb);
        }
        // if we meet a int2ptr, then it points-to black hole
        else if (const ConstantExpr* int2Ptrce = isInt2PtrConstantExpr(ref))
        {
            addGlobalBlackHoleAddrEdge(pag->getValueNode(int2Ptrce), int2Ptrce);
        }
        else if (const ConstantExpr* ptr2Intce = isPtr2IntConstantExpr(ref))
        {
            const Constant* opnd = ptr2Intce->getOperand(0);
            processCE(opnd);
            const BasicBlock* cbb = getCurrentBB();
            const Value* cval = getCurrentValue();
            setCurrentLocation(ptr2Intce, nullptr);
            addCopyEdge(pag->getValueNode(opnd), pag->getValueNode(ptr2Intce));
            setCurrentLocation(cval, cbb);
        }
        else if(isTruncConstantExpr(ref) || isCmpConstantExpr(ref))
        {
            // we don't handle trunc and cmp instruction for now
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, nullptr);
            NodeID dst = pag->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (isBinaryConstantExpr(ref))
        {
            // we don't handle binary constant expression like add(x,y) now
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, nullptr);
            NodeID dst = pag->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (isUnaryConstantExpr(ref))
        {
            // we don't handle unary constant expression like fneg(x) now
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, nullptr);
            NodeID dst = pag->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (SVFUtil::isa<ConstantAggregate>(ref))
        {
            // we don't handle constant agrgregate like constant vectors
        }
        else if (SVFUtil::isa<BlockAddress>(ref))
        {
			// blockaddress instruction (e.g. i8* blockaddress(@run_vm, %182))
			// is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and PAGBuilder.cpp:183-194
			const Value *cval = getCurrentValue();
			const BasicBlock *cbb = getCurrentBB();
			setCurrentLocation(ref, nullptr);
			NodeID dst = pag->getValueNode(ref);
			addAddrEdge(pag->getConstantNode(), dst);
			setCurrentLocation(cval, cbb);
        }
        else
        {
            if(SVFUtil::isa<ConstantExpr>(val))
                assert(false && "we don't handle all other constant expression for now!");
        }
    }
}
/*!
 * Get the field of the global variable node
 * FIXME:Here we only get the field that actually used in the program
 * We ignore the initialization of global variable field that not used in the program
 */
NodeID PAGBuilder::getGlobalVarField(const GlobalVariable *gvar, u32_t offset)
{

    // if the global variable do not have any field needs to be initialized
    if (offset == 0 && gvar->getInitializer()->getType()->isSingleValueType())
    {
        return getValueNode(gvar);
    }
    /// if we did not find the constant expression in the program,
    /// then we need to create a gep node for this field
    else
    {
        const Type *gvartype = gvar->getType();
        while (const PointerType *ptype = SVFUtil::dyn_cast<PointerType>(gvartype))
            gvartype = ptype->getElementType();
        return getGepValNode(gvar, LocationSet(offset), gvartype, offset);
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
                               u32_t offset)
{
    DBOUT(DPAGBuild,
          outs() << "global " << *gvar << " constant initializer: " << *C
          << "\n");

    if (C->getType()->isSingleValueType())
    {
        NodeID src = getValueNode(C);
        // get the field value if it is avaiable, otherwise we create a dummy field node.
        setCurrentLocation(gvar, nullptr);
        NodeID field = getGlobalVarField(gvar, offset);

        if (SVFUtil::isa<GlobalVariable>(C) || SVFUtil::isa<Function>(C))
        {
            setCurrentLocation(C, nullptr);
            addStoreEdge(src, field);
        }
        else if (SVFUtil::isa<ConstantExpr>(C))
        {
            // add gep edge of C1 itself is a constant expression
            processCE(C);
            setCurrentLocation(C, nullptr);
            addStoreEdge(src, field);
        }
        else if (SVFUtil::isa<BlockAddress>(C))
        {
			// blockaddress instruction (e.g. i8* blockaddress(@run_vm, %182))
			// is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and PAGBuilder.cpp:183-194
			processCE(C);
			setCurrentLocation(C, nullptr);
			addAddrEdge(pag->getConstantNode(), src);
        }
        else
        {
            setCurrentLocation(C, nullptr);
            addStoreEdge(src, field);
            /// src should not point to anything yet
            if (C->getType()->isPtrOrPtrVectorTy() && src != pag->getNullPtr())
                addCopyEdge(pag->getNullPtr(), src);
        }
    }
    else if (SVFUtil::isa<ConstantArray>(C))
    {
        if (cppUtil::isValVtbl(gvar) == false)
            for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
                InitialGlobal(gvar, SVFUtil::cast<Constant>(C->getOperand(i)), offset);

    }
    else if (SVFUtil::isa<ConstantStruct>(C))
    {
        const StructType *sty = SVFUtil::cast<StructType>(C->getType());
        const std::vector<u32_t>& offsetvect =
            SymbolTableInfo::SymbolInfo()->getFattenFieldIdxVec(sty);
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            u32_t off = offsetvect[i];
            InitialGlobal(gvar, SVFUtil::cast<Constant>(C->getOperand(i)), offset + off);
        }

    }
    else
    {
        //TODO:assert(false,"what else do we have");
    }
}

/*!
 *  Visit global variables for building PAG
 */
void PAGBuilder::visitGlobal(SVFModule* svfModule)
{

    /// initialize global variable
    for (SVFModule::global_iterator I = svfModule->global_begin(), E =
                svfModule->global_end(); I != E; ++I)
    {
        GlobalVariable *gvar = *I;
        NodeID idx = getValueNode(gvar);
        NodeID obj = getObjectNode(gvar);

        setCurrentLocation(gvar, nullptr);
        addAddrEdge(obj, idx);

        if (gvar->hasInitializer())
        {
            Constant *C = gvar->getInitializer();
            DBOUT(DPAGBuild, outs() << "add global var node " << *gvar << "\n");
            InitialGlobal(gvar, C, 0);
        }
    }

    /// initialize global functions
    for (SVFModule::llvm_const_iterator I = svfModule->llvmFunBegin(), E =
                svfModule->llvmFunEnd(); I != E; ++I)
    {
        const Function *fun = *I;
        NodeID idx = getValueNode(fun);
        NodeID obj = getObjectNode(fun);

        DBOUT(DPAGBuild, outs() << "add global function node " << fun->getName() << "\n");
        setCurrentLocation(fun, nullptr);
        addAddrEdge(obj, idx);
    }

    // Handle global aliases (due to linkage of multiple bc files), e.g., @x = internal alias @y. We need to add a copy from y to x.
    for (SVFModule::alias_iterator I = svfModule->alias_begin(), E = svfModule->alias_end(); I != E; I++)
    {
        NodeID dst = pag->getValueNode(*I);
        NodeID src = pag->getValueNode((*I)->getAliasee());
        processCE((*I)->getAliasee());
        setCurrentLocation(*I, nullptr);
        addCopyEdge(src,dst);
    }
}

/*!
 * Visit alloca instructions
 * Add edge V (dst) <-- O (src), V here is a value node on PAG, O is object node on PAG
 */
void PAGBuilder::visitAllocaInst(AllocaInst &inst)
{

    // AllocaInst should always be a pointer type
    assert(SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process alloca  " << inst << " \n");
    NodeID dst = getValueNode(&inst);

    NodeID src = getObjectNode(&inst);

    addAddrEdge(src, dst);

}

/*!
 * Visit phi instructions
 */
void PAGBuilder::visitPHINode(PHINode &inst)
{

    DBOUT(DPAGBuild, outs() << "process phi " << inst << "  \n");

    NodeID dst = getValueNode(&inst);

    for (Size_t i = 0; i < inst.getNumIncomingValues(); ++i)
    {
        const Value* val = inst.getIncomingValue(i);
        const Instruction* incomingInst = SVFUtil::dyn_cast<Instruction>(val);
        assert((incomingInst==nullptr) || (incomingInst->getFunction() == inst.getFunction()));

        NodeID src = getValueNode(val);
        const CopyPE* copy = addCopyEdge(src, dst);
        pag->addPhiNode(pag->getPAGNode(dst), copy);
    }
}

/*
 * Visit load instructions
 */
void PAGBuilder::visitLoadInst(LoadInst &inst)
{
    DBOUT(DPAGBuild, outs() << "process load  " << inst << " \n");

    NodeID dst = getValueNode(&inst);

    NodeID src = getValueNode(inst.getPointerOperand());

    addLoadEdge(src, dst);
}

/*!
 * Visit store instructions
 */
void PAGBuilder::visitStoreInst(StoreInst &inst)
{
    // StoreInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process store " << inst << " \n");

    NodeID dst = getValueNode(inst.getPointerOperand());

    NodeID src = getValueNode(inst.getValueOperand());

    addStoreEdge(src, dst);

}

/*!
 * Visit getelementptr instructions
 */
void PAGBuilder::visitGetElementPtrInst(GetElementPtrInst &inst)
{

    NodeID dst = getValueNode(&inst);
    // GetElementPtrInst should always be a pointer or a vector contains pointers
    // for now we don't handle vector type here
    if(SVFUtil::isa<VectorType>(inst.getType()))
    {
        addBlackHoleAddrEdge(dst);
        return;
    }

    assert(SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process gep  " << inst << " \n");

    NodeID src = getValueNode(inst.getPointerOperand());

    LocationSet ls;
    bool constGep = computeGepOffset(&inst, ls);
    addGepEdge(src, dst, ls, constGep);
}

/*
 * Visit cast instructions
 */
void PAGBuilder::visitCastInst(CastInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process cast  " << inst << " \n");
    NodeID dst = getValueNode(&inst);

    if (SVFUtil::isa<IntToPtrInst>(&inst))
    {
        addBlackHoleAddrEdge(dst);
    }
    else
    {
        Value * opnd = inst.getOperand(0);
        if (!SVFUtil::isa<PointerType>(opnd->getType()))
            opnd = stripAllCasts(opnd);

        NodeID src = getValueNode(opnd);
        addCopyEdge(src, dst);
    }
}

/*!
 * Visit Binary Operator
 */
void PAGBuilder::visitBinaryOperator(BinaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    for (u32_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value* opnd = inst.getOperand(i);
        NodeID src = getValueNode(opnd);
        const BinaryOPPE* binayPE = addBinaryOPEdge(src, dst);
        pag->addBinaryNode(pag->getPAGNode(dst),binayPE);
    }
}

/*!
 * Visit Unary Operator
 */
void PAGBuilder::visitUnaryOperator(UnaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    for (u32_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value* opnd = inst.getOperand(i);
        NodeID src = getValueNode(opnd);
        const UnaryOPPE* unaryPE = addUnaryOPEdge(src, dst);
        pag->addUnaryNode(pag->getPAGNode(dst),unaryPE);
    }
}

/*!
 * Visit compare instruction
 */
void PAGBuilder::visitCmpInst(CmpInst &inst)
{
    NodeID dst = getValueNode(&inst);
    for (u32_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value* opnd = inst.getOperand(i);
        NodeID src = getValueNode(opnd);
        const CmpPE* cmpPE = addCmpEdge(src, dst);
        pag->addCmpNode(pag->getPAGNode(dst),cmpPE);
    }
}


/*!
 * Visit select instructions
 */
void PAGBuilder::visitSelectInst(SelectInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process select  " << inst << " \n");

    NodeID dst = getValueNode(&inst);
    NodeID src1 = getValueNode(inst.getTrueValue());
    NodeID src2 = getValueNode(inst.getFalseValue());
    const CopyPE* cpy1 = addCopyEdge(src1, dst);
    const CopyPE* cpy2 = addCopyEdge(src2, dst);

    /// Two operands have same incoming basic block, both are the current BB
    pag->addPhiNode(pag->getPAGNode(dst), cpy1);
    pag->addPhiNode(pag->getPAGNode(dst), cpy2);
}

/*
 * Visit callsites
 */
void PAGBuilder::visitCallSite(CallSite cs)
{

    // skip llvm intrinsics
    if(isIntrinsicInst(cs.getInstruction()))
        return;

    DBOUT(DPAGBuild,
          outs() << "process callsite " << *cs.getInstruction() << "\n");

    CallBlockNode* callBlockNode = pag->getICFG()->getCallBlockNode(cs.getInstruction());
    RetBlockNode* retBlockNode = pag->getICFG()->getRetBlockNode(cs.getInstruction());

    pag->addCallSite(callBlockNode);

    /// Collect callsite arguments and returns
    for(CallSite::arg_iterator itA = cs.arg_begin(), ieA = cs.arg_end(); itA!=ieA; ++itA)
        pag->addCallSiteArgs(callBlockNode,pag->getPAGNode(getValueNode(*itA)));

    if(!cs.getType()->isVoidTy())
        pag->addCallSiteRets(retBlockNode,pag->getPAGNode(getValueNode(cs.getInstruction())));

    const SVFFunction *callee = getCallee(cs);

    if (callee)
    {
        if (isExtCall(callee))
        {
            if (ExternalPAG::hasExternalPAG(callee))
            {
                ExternalPAG::connectCallsiteToExternalPAG(&cs);
            }
            else
            {
                // There is no extpag for the function, use the old method.
                handleExtCall(cs, callee);
            }
        }
        else
        {
            handleDirectCall(cs, callee);
        }
    }
    else
    {
        //If the callee was not identified as a function (null F), this is indirect.
        handleIndCall(cs);
    }
}

/*!
 * Visit return instructions of a function
 */
void PAGBuilder::visitReturnInst(ReturnInst &inst)
{

    // ReturnInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process return  " << inst << " \n");

    if(Value *src = inst.getReturnValue())
    {
        const SVFFunction *F = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(inst.getParent()->getParent());

        NodeID rnF = getReturnNode(F);
        NodeID vnS = getValueNode(src);
        //vnS may be null if src is a null ptr
        const CopyPE* copy = addCopyEdge(vnS, rnF);
        pag->addPhiNode(pag->getPAGNode(rnF), copy);
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
void PAGBuilder::visitExtractValueInst(ExtractValueInst  &inst)
{
    NodeID dst = getValueNode(&inst);
    addBlackHoleAddrEdge(dst);
}

/*!
 * The �extractelement� instruction extracts a single scalar element from a vector at a specified index.
 * TODO: for now we just assume the pointer after extraction points to blackhole
 * The first operand of an �extractelement� instruction is a value of vector type.
 * The second operand is an index indicating the position from which to extract the element.
 *
 * <result> = extractelement <4 x i32> %vec, i32 0    ; yields i32
 */
void PAGBuilder::visitExtractElementInst(ExtractElementInst &inst)
{
    NodeID dst = getValueNode(&inst);
    addBlackHoleAddrEdge(dst);
}

/*!
 * Branch and switch instructions are treated as UnaryOP
 * br %cmp label %if.then, label %if.else
 */
void PAGBuilder::visitBranchInst(BranchInst &inst){
    NodeID dst = getValueNode(&inst);
    NodeID src;
	if (inst.isConditional())
		src = getValueNode(inst.getCondition());
	else
		src = pag->getNullPtr();
	const UnaryOPPE *unaryPE = addUnaryOPEdge(src, dst);
    pag->addUnaryNode(pag->getPAGNode(dst),unaryPE);
}

void PAGBuilder::visitSwitchInst(SwitchInst &inst){
    NodeID dst = getValueNode(&inst);
    Value* opnd = inst.getCondition();
    NodeID src = getValueNode(opnd);
    const UnaryOPPE* unaryPE = addUnaryOPEdge(src, dst);
    pag->addUnaryNode(pag->getPAGNode(dst),unaryPE);
}


/*!
 * Add the constraints for a direct, non-external call.
 */
void PAGBuilder::handleDirectCall(CallSite cs, const SVFFunction *F)
{

    assert(F);

    DBOUT(DPAGBuild,
          outs() << "handle direct call " << *cs.getInstruction() << " callee " << *F << "\n");

    //Only handle the ret.val. if it's used as a ptr.
    NodeID dstrec = getValueNode(cs.getInstruction());
    //Does it actually return a ptr?
    if (!cs.getType()->isVoidTy())
    {
        NodeID srcret = getReturnNode(F);
        CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(cs.getInstruction());
        addRetEdge(srcret, dstrec,icfgNode);
    }
    //Iterators for the actual and formal parameters
    CallSite::arg_iterator itA = cs.arg_begin(), ieA = cs.arg_end();
    Function::const_arg_iterator itF = F->getLLVMFun()->arg_begin(), ieF = F->getLLVMFun()->arg_end();
    //Go through the fixed parameters.
    DBOUT(DPAGBuild, outs() << "      args:");
    for (; itF != ieF; ++itA, ++itF)
    {
        //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
        if (itA == ieA)
        {
            DBOUT(DPAGBuild, outs() << " !! not enough args\n");
            break;
        }
        const Value *AA = *itA, *FA = &*itF; //current actual/formal arg

        DBOUT(DPAGBuild, outs() << "process actual parm  " << *AA << " \n");

        NodeID dstFA = getValueNode(FA);
        NodeID srcAA = getValueNode(AA);
        CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(cs.getInstruction());
        addCallEdge(srcAA, dstFA, icfgNode);
    }
    //Any remaining actual args must be varargs.
    if (F->getLLVMFun()->isVarArg())
    {
        NodeID vaF = getVarargNode(F);
        DBOUT(DPAGBuild, outs() << "\n      varargs:");
        for (; itA != ieA; ++itA)
        {
            Value *AA = *itA;
            NodeID vnAA = getValueNode(AA);
            CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(cs.getInstruction());
            addCallEdge(vnAA,vaF, icfgNode);
        }
    }
    if(itA != ieA)
    {
        /// FIXME: this assertion should be placed for correct checking except
        /// bug program like 188.ammp, 300.twolf
        writeWrnMsg("too many args to non-vararg func.");
        writeWrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");

    }
}


/*!
 * Find the base type and the max possible offset of an object pointed to by (V).
 */
const Type *PAGBuilder::getBaseTypeAndFlattenedFields(Value *V, std::vector<LocationSet> &fields)
{
    return SymbolTableInfo::SymbolInfo()->getBaseTypeAndFlattenedFields(V, fields);
}

/*!
 * Add the load/store constraints and temp. nodes for the complex constraint
 * *D = *S (where D/S may point to structs).
 */
void PAGBuilder::addComplexConsForExt(Value *D, Value *S, u32_t sz)
{
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
    for (u32_t index = 0; index < sz; index++)
    {
        NodeID dField = getGepValNode(D,fields[index],dtype,index);
        NodeID sField = getGepValNode(S,fields[index],stype,index);
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(sField,dummy);
        addStoreEdge(dummy,dField);
    }
}


/*!
 * Handle external calls
 */
void PAGBuilder::handleExtCall(CallSite cs, const SVFFunction *callee)
{
    const Instruction* inst = cs.getInstruction();
    if (isHeapAllocOrStaticExtCall(cs))
    {
        // case 1: ret = new obj
        if (isHeapAllocExtCallViaRet(cs) || isStaticExtCall(cs))
        {
            NodeID val = getValueNode(inst);
            NodeID obj = getObjectNode(inst);
            addAddrEdge(obj, val);
        }
        // case 2: *arg = new obj
        else
        {
            assert(isHeapAllocExtCallViaArg(cs) && "Must be heap alloc call via arg.");
            int arg_pos = getHeapAllocHoldingArgPosition(callee);
            const Value *arg = cs.getArgument(arg_pos);
            if (arg->getType()->isPointerTy())
            {
                NodeID vnArg = getValueNode(arg);
                NodeID dummy = pag->addDummyValNode();
                NodeID obj = pag->addBlackholeObjNode();
                if (vnArg && dummy && obj)
                {
                    addAddrEdge(obj, dummy);
                    addStoreEdge(dummy, vnArg);
                }
            }
            else
            {
                writeWrnMsg("Arg receiving new object must be pointer type");
            }
        }
    }
    else
    {
        if(isExtCall(callee))
        {
            ExtAPI::extf_t tF= extCallTy(callee);
            switch(tF)
            {
            case ExtAPI::EFT_REALLOC:
            {
                if(!SVFUtil::isa<PointerType>(inst->getType()))
                    break;
                // e.g. void *realloc(void *ptr, size_t size)
                // if ptr is null then we will treat it as a malloc
                // if ptr is not null, then we assume a new data memory will be attached to
                // the tail of old allocated memory block.
                if(SVFUtil::isa<ConstantPointerNull>(cs.getArgument(0)))
                {
                    NodeID val = getValueNode(inst);
                    NodeID obj = getObjectNode(inst);
                    addAddrEdge(obj, val);
                }
                break;
            }
            case ExtAPI::EFT_L_A0:
            case ExtAPI::EFT_L_A1:
            case ExtAPI::EFT_L_A2:
            case ExtAPI::EFT_L_A8:
            {
                if(!SVFUtil::isa<PointerType>(inst->getType()))
                    break;
                NodeID dstNode = getValueNode(inst);
                Size_t arg_pos;
                switch(tF)
                {
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
                if(SVFUtil::isa<PointerType>(src->getType()))
                {
                    NodeID srcNode = getValueNode(src);
                    addCopyEdge(srcNode, dstNode);
                }
                else
                    addBlackHoleAddrEdge(dstNode);
                break;
                break;
            }
            case ExtAPI::EFT_L_A0__A0R_A1:
            {
				// this is only for memset(void *str, int c, size_t n)
				// which copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str
				// However, the second argument is non-pointer, thus we can not use addComplexConsForExt
				// addComplexConsForExt(cs.getArgument(0), cs.getArgument(1));
                if(SVFUtil::isa<PointerType>(inst->getType()))
                    addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
                break;
            }
            case ExtAPI::EFT_L_A0__A0R_A1R:
            {
                addComplexConsForExt(cs.getArgument(0), cs.getArgument(1));
                //memcpy returns the dest.
                if(SVFUtil::isa<PointerType>(inst->getType()))
                {
                    addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
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
            case ExtAPI::EFT_A1R_A0:
            {
                NodeID vnD= getValueNode(cs.getArgument(1));
                NodeID vnS= getValueNode(cs.getArgument(0));
                if(vnD && vnS)
                    addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A2R_A1:
            {
                NodeID vnD= getValueNode(cs.getArgument(2));
                NodeID vnS= getValueNode(cs.getArgument(1));
                if(vnD && vnS)
                    addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A4R_A1:
            {
                NodeID vnD= getValueNode(cs.getArgument(4));
                NodeID vnS= getValueNode(cs.getArgument(1));
                if(vnD && vnS)
                    addStoreEdge(vnS,vnD);
                break;
            }
			case ExtAPI::EFT_L_A0__A1_A0:
			{
				/// this case is for strcpy(dst,src); to maintain its semantics
				/// we will store src to the base of dst instead of dst.
				/// dst = load base
				/// store src base
				if (const LoadInst *load = SVFUtil::dyn_cast<LoadInst>(cs.getArgument(0))) {
					addStoreEdge(getValueNode(cs.getArgument(1)), getValueNode(load->getPointerOperand()));
					if (SVFUtil::isa<PointerType>(inst->getType()))
						addLoadEdge(getValueNode(load->getPointerOperand()), getValueNode(inst));
				}
//				else if (const GetElementPtrInst *gep = SVFUtil::dyn_cast<GetElementPtrInst>(cs.getArgument(0))) {
//					addStoreEdge(getValueNode(cs.getArgument(1)), getValueNode(cs.getArgument(0)));
//					if (SVFUtil::isa<PointerType>(inst->getType()))
//						addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
//				}
				break;
			}
            case ExtAPI::EFT_L_A0__A2R_A0:
            {
                if(SVFUtil::isa<PointerType>(inst->getType()))
                {
                    //Do the L_A0 part if the retval is used.
                    NodeID vnD= getValueNode(inst);
                    Value *src= cs.getArgument(0);
                    if(SVFUtil::isa<PointerType>(src->getType()))
                    {
                        NodeID vnS= getValueNode(src);
                        if(vnS)
                            addCopyEdge(vnS,vnD);
                    }
                    else
                        addBlackHoleAddrEdge(vnD);
                }
                //Do the A2R_A0 part.
                NodeID vnD= getValueNode(cs.getArgument(2));
                NodeID vnS= getValueNode(cs.getArgument(0));
                if(vnD && vnS)
                    addStoreEdge(vnS,vnD);
                break;
            }
            case ExtAPI::EFT_A0R_NEW:
            case ExtAPI::EFT_A1R_NEW:
            case ExtAPI::EFT_A2R_NEW:
            case ExtAPI::EFT_A4R_NEW:
            case ExtAPI::EFT_A11R_NEW:
            {
                assert(!"Alloc via arg cases are not handled here.");
                break;
            }
            case ExtAPI::EFT_ALLOC:
            case ExtAPI::EFT_NOSTRUCT_ALLOC:
            case ExtAPI::EFT_STAT:
            case ExtAPI::EFT_STAT2:
                if(SVFUtil::isa<PointerType>(inst->getType()))
                    assert(!"alloc type func. are not handled here");
                else
                {
                    // fdopen will return an integer in LLVM IR.
                    writeWrnMsg("alloc type func do not return pointer type");
                }
                break;
            case ExtAPI::EFT_NOOP:
            case ExtAPI::EFT_FREE:
                break;
            case ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE:
            {
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
                for (int i = offset + 1; i <= offset + 3; ++i)
                {
                    NodeID vnD = getGepValNode(vArg3, fields[i], type, i);
                    NodeID vnS = getValueNode(vArg1);
                    if(vnD && vnS)
                        addStoreEdge(vnS,vnD);
                }
                break;
            }
            case ExtAPI::EFT_STD_RB_TREE_INCREMENT:
            {
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
                for (int i = offset + 1; i <= offset + 3; ++i)
                {
                    NodeID vnS = getGepValNode(vArg, fields[i], type, i);
                    if(vnD && vnS)
                        addStoreEdge(vnS,vnD);
                }
                break;
            }
            case ExtAPI::EFT_STD_LIST_HOOK:
            {
                Value *vSrc = cs.getArgument(0);
                Value *vDst = cs.getArgument(1);
                NodeID src = pag->getValueNode(vSrc);
                NodeID dst = pag->getValueNode(vDst);
                addStoreEdge(src, dst);
                break;
            }
            case ExtAPI::CPP_EFT_A0R_A1:
            {
                SymbolTableInfo* symTable = SymbolTableInfo::SymbolInfo();
                if (symTable->getModelConstants())
                {
                    NodeID vnD = pag->getValueNode(cs.getArgument(0));
                    NodeID vnS = pag->getValueNode(cs.getArgument(1));
                    addStoreEdge(vnS, vnD);
                }
                break;
            }
            case ExtAPI::CPP_EFT_A0R_A1R:
            {
                SymbolTableInfo* symTable = SymbolTableInfo::SymbolInfo();
                if (symTable->getModelConstants())
                {
                    NodeID vnD = getValueNode(cs.getArgument(0));
                    NodeID vnS = getValueNode(cs.getArgument(1));
                    assert(vnD && vnS && "dst or src not exist?");
                    NodeID dummy = pag->addDummyValNode();
                    addLoadEdge(vnS,dummy);
                    addStoreEdge(dummy,vnD);
                }
                break;
            }
            case ExtAPI::CPP_EFT_A1R:
            {
                SymbolTableInfo* symTable = SymbolTableInfo::SymbolInfo();
                if (symTable->getModelConstants())
                {
                    NodeID vnS = getValueNode(cs.getArgument(1));
                    assert(vnS && "src not exist?");
                    NodeID dummy = pag->addDummyValNode();
                    addLoadEdge(vnS,dummy);
                }
                break;
            }
            case ExtAPI::CPP_EFT_DYNAMIC_CAST:
            {
                Value *vArg0 = cs.getArgument(0);
                Value *retVal = cs.getInstruction();
                NodeID src = getValueNode(vArg0);
                assert(src && "src not exist?");
                NodeID dst = getValueNode(retVal);
                assert(dst && "dst not exist?");
                addCopyEdge(src, dst);
                break;
            }
            case ExtAPI::EFT_CXA_BEGIN_CATCH:
            {
                break;
            }
            //default:
            case ExtAPI::EFT_OTHER:
            {
                if(SVFUtil::isa<PointerType>(inst->getType()))
                {
                    std::string str;
                    raw_string_ostream rawstr(str);
                    rawstr << "function " << callee->getName() << " not in the external function summary list";
                    writeWrnMsg(rawstr.str());
                    //assert("unknown ext.func type");
                }
            }
            }
        }

        /// create inter-procedural PAG edges for thread forks
        if(isThreadForkCall(inst))
        {
            if(const Function* forkedFun = getLLVMFunction(getForkedFun(inst)) )
            {
                forkedFun = getDefFunForMultipleModule(forkedFun)->getLLVMFun();
                const Value* actualParm = getActualParmAtForkSite(inst);
                /// pthread_create has 1 arg.
                /// apr_thread_create has 2 arg.
                assert((forkedFun->arg_size() <= 2) && "Size of formal parameter of start routine should be one");
                if(forkedFun->arg_size() <= 2 && forkedFun->arg_size() >= 1)
                {
                    const Argument* formalParm = &(*forkedFun->arg_begin());
                    /// Connect actual parameter to formal parameter of the start routine
                    if(SVFUtil::isa<PointerType>(actualParm->getType()) && SVFUtil::isa<PointerType>(formalParm->getType()) )
                    {
                        CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(inst);
                        addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),icfgNode);
                    }
                }
            }
            else
            {
                /// handle indirect calls at pthread create APIs e.g., pthread_create(&t1, nullptr, fp, ...);
                ///const Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                ///if(!SVFUtil::isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
            /// If forkedFun does not pass to spawnee as function type but as void pointer
            /// remember to update inter-procedural callgraph/PAG/SVFG etc. when indirect call targets are resolved
            /// We don't connect the callgraph here, further investigation is need to hanle mod-ref during SVFG construction.
        }

        /// create inter-procedural PAG edges for hare_parallel_for calls
        else if(isHareParForCall(inst))
        {
            if(const Function* taskFunc = getLLVMFunction(getTaskFuncAtHareParForSite(inst)) )
            {
                /// The task function of hare_parallel_for has 3 args.
                assert((taskFunc->arg_size() == 3) && "Size of formal parameter of hare_parallel_for's task routine should be 3");
                const Value* actualParm = getTaskDataAtHareParForSite(inst);
                const Argument* formalParm = &(*taskFunc->arg_begin());
                /// Connect actual parameter to formal parameter of the start routine
                if(SVFUtil::isa<PointerType>(actualParm->getType()) && SVFUtil::isa<PointerType>(formalParm->getType()) )
                {
                    CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(inst);
                    addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),icfgNode);
                }
            }
            else
            {
                /// handle indirect calls at hare_parallel_for (e.g., hare_parallel_for(..., fp, ...);
                ///const Value* fun = ThreadAPI::getThreadAPI()->getForkedFun(inst);
                ///if(!SVFUtil::isa<Function>(fun))
                ///    pag->addIndirectCallsites(cs,pag->getValueNode(fun));
            }
        }

        /// TODO: inter-procedural PAG edges for thread joins
    }
}

/*!
 * Indirect call is resolved on-the-fly during pointer analysis
 */
void PAGBuilder::handleIndCall(CallSite cs)
{
    const CallBlockNode* cbn = pag->getICFG()->getCallBlockNode(cs.getInstruction());
    pag->addIndirectCallsites(cbn,pag->getValueNode(cs.getCalledValue()));
}

/*
 * TODO: more sanity checks might be needed here
 */
void PAGBuilder::sanityCheck()
{
    for (PAG::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
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


/*!
 * Add a temp field value node according to base value and offset
 * this node is after the initial node method, it is out of scope of symInfo table
 */
NodeID PAGBuilder::getGepValNode(const Value* val, const LocationSet& ls, const Type *baseType, u32_t fieldidx)
{
    NodeID base = pag->getBaseValNode(getValueNode(val));
    NodeID gepval = pag->getGepValNode(curVal, base, ls);
    if (gepval==UINT_MAX)
    {
		assert(UINT_MAX==-1 && "maximum limit of unsigned int is not -1?");
        /*
         * getGepValNode can only be called from two places:
         * 1. PAGBuilder::addComplexConsForExt to handle external calls
         * 2. PAGBuilder::getGlobalVarField to initialize global variable
         * so curVal can only be
         * 1. Instruction
         * 2. GlobalVariable
         */
        assert((SVFUtil::isa<Instruction>(curVal) || SVFUtil::isa<GlobalVariable>(curVal)) && "curVal not an instruction or a globalvariable?");
        const std::vector<FieldInfo> &fieldinfo = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(baseType);
        const Type *type = fieldinfo[fieldidx].getFlattenElemTy();

        // We assume every GepValNode and its GepEdge to the baseNode are unique across the whole program
        // We preserve the current BB information to restore it after creating the gepNode
        const Value* cval = getCurrentValue();
        const BasicBlock* cbb = getCurrentBB();
        setCurrentLocation(curVal, nullptr);
        NodeID gepNode= pag->addGepValNode(curVal, val,ls, NodeIDAllocator::get()->allocateValueId(),type,fieldidx);
        addGepEdge(base, gepNode, ls, true);
        setCurrentLocation(cval, cbb);
        return gepNode;
    }
    else
        return gepval;
}


/*
 * curVal   <-------->  PAGEdge
 * Instruction          Any Edge
 * Argument             CopyEdge  (PAG::addFormalParamBlackHoleAddrEdge)
 * ConstantExpr         CopyEdge  (Int2PtrConstantExpr   CastConstantExpr  PAGBuilder::processCE)
 *                      GepEdge   (GepConstantExpr   PAGBuilder::processCE)
 * ConstantPointerNull  CopyEdge  (3-->2 NullPtr-->BlkPtr PAG::addNullPtrNode)
 *  				    AddrEdge  (0-->2 BlkObj-->BlkPtr PAG::addNullPtrNode)
 * GlobalVariable       AddrEdge  (PAGBuilder::visitGlobal)
 *                      GepEdge   (PAGBuilder::getGlobalVarField)
 * Function             AddrEdge  (PAGBuilder::visitGlobal)
 * Constant             StoreEdge (PAGBuilder::InitialGlobal)
 */
void PAGBuilder::setCurrentBBAndValueForPAGEdge(PAGEdge* edge)
{
    if (SVFModule::pagReadFromTXT())
        return;

    assert(curVal && "current Val is nullptr?");
    edge->setBB(curBB);
    edge->setValue(curVal);
    ICFGNode* icfgNode = pag->getICFG()->getGlobalBlockNode();
    if (const Instruction *curInst = SVFUtil::dyn_cast<Instruction>(curVal))
    {
        const Function* srcFun = edge->getSrcNode()->getFunction();
        const Function* dstFun = edge->getDstNode()->getFunction();
        if(srcFun!=nullptr && !SVFUtil::isa<RetPE>(edge) && !SVFUtil::isa<Function>(edge->getSrcNode()->getValue())) {
            assert(srcFun==curInst->getFunction() && "SrcNode of the PAGEdge not in the same function?");
        }
        if(dstFun!=nullptr && !SVFUtil::isa<CallPE>(edge) && !SVFUtil::isa<Function>(edge->getDstNode()->getValue())) {
            assert(dstFun==curInst->getFunction() && "DstNode of the PAGEdge not in the same function?");
        }

        /// We assume every GepValPN and its GepPE are unique across whole program
        if (!(SVFUtil::isa<GepPE>(edge) && SVFUtil::isa<GepValPN>(edge->getDstNode())))
            assert(curBB && "instruction does not have a basic block??");

        icfgNode = pag->getICFG()->getBlockICFGNode(curInst);
    }
    else if (const Argument* arg = SVFUtil::dyn_cast<Argument>(curVal))
    {
        assert(curBB && (&curBB->getParent()->getEntryBlock() == curBB));
        const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(arg->getParent());
        icfgNode = pag->getICFG()->getFunEntryBlockNode(fun);
    }
    else if (SVFUtil::isa<ConstantExpr>(curVal))
    {
        if (!curBB)
            pag->addGlobalPAGEdge(edge);
        else
    		icfgNode = pag->getICFG()->getBlockICFGNode(&curBB->front());
    }
    else if (SVFUtil::isa<GlobalVariable>(curVal) ||
             SVFUtil::isa<Function>(curVal) ||
             SVFUtil::isa<Constant>(curVal) ||
			 SVFUtil::isa<MetadataAsValue>(curVal))
    {
        pag->addGlobalPAGEdge(edge);
    }
    else
    {
        assert(false && "what else value can we have?");
    }

    pag->addToInstPAGEdgeList(icfgNode,edge);
    icfgNode->addPAGEdge(edge);
}


