//===- SVFIRBuilder.cpp -- SVFIR builder-----------------------------------------//
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
 * SVFIRBuilder.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/SVFIRBuilder.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/BasicTypes.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CPPUtil.h"
#include "Util/BasicTypes.h"
#include "MemoryModel/PAGBuilderFromFile.h"
#include "Util/Options.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;


/*!
 * Start building SVFIR here
 */
SVFIR* SVFIRBuilder::build(SVFModule* svfModule)
{

    // We read SVFIR from a user-defined txt instead of parsing SVFIR from LLVM IR
    if (SVFModule::pagReadFromTXT())
    {
        PAGBuilderFromFile fileBuilder(SVFModule::pagFileName());
        return fileBuilder.build();
    }

    // If the SVFIR has been built before, then we return the unique SVFIR of the program
    if(pag->getNodeNumAfterPAGBuild() > 1)
    	return pag;

    svfMod = svfModule;

    /// initial external library information
    /// initial SVFIR nodes
    initialiseNodes();
    /// initial SVFIR edges:
    ///// handle globals
    visitGlobal(svfModule);
    ///// collect exception vals in the program

    /// handle functions
    for (SVFModule::iterator fit = svfModule->begin(), efit = svfModule->end();
            fit != efit; ++fit)
    {
        const SVFFunction& fun = **fit;
        /// collect return node of function fun
        if(!fun.isDeclaration())
        {
            /// Return SVFIR node will not be created for function which can not
            /// reach the return instruction due to call to abort(), exit(),
            /// etc. In 176.gcc of SPEC 2000, function build_objc_string() from
            /// c-lang.c shows an example when fun.doesNotReturn() evaluates
            /// to TRUE because of abort().
            if(fun.getLLVMFun()->doesNotReturn() == false && fun.getLLVMFun()->getReturnType()->isVoidTy() == false)
                pag->addFunRet(&fun,pag->getGNode(pag->getReturnNode(&fun)));

            /// To be noted, we do not record arguments which are in declared function without body
            /// TODO: what about external functions with SVFIR imported by commandline?
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
                pag->addFunArgs(&fun,pag->getGNode(argValNodeId));
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

    // dump SVFIR
    if (Options::PAGDotGraph)
        pag->dump("svfir_initial");

    // print to command line of the SVFIR graph
    if (Options::PAGPrint)
        pag->print();

    // dump ICFG
    if (Options::DumpICFG)
    	pag->getICFG()->dump("icfg_initial");

    return pag;
}

/*
 * Initial all the nodes from symbol table
 */
void SVFIRBuilder::initialiseNodes()
{
    DBOUT(DPAGBuild, outs() << "Initialise SVFIR Nodes ...\n");

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

/*
    https://github.com/SVF-tools/SVF/issues/524
    Handling single value types, for constant index, including pointer, integer, etc 
    e.g. field_idx = getelementptr i8, %i8* %p, i64 -4
    We can obtain the field index by inferring the byteoffset if %p is casted from a pointer to a struct
    For another example, the following can be an array access.
    e.g. field_idx = getelementptr i8, %struct_type %p, i64 1

*/
u32_t SVFIRBuilder::inferFieldIdxFromByteOffset(const llvm::GEPOperator* gepOp, DataLayout *dl, LocationSet& ls, s32_t idx){
    return 0;
}

/*!
 * Return the object node offset according to GEP insn (V).
 * Given a gep edge p = q + i, if "i" is a constant then we return its offset size
 * otherwise if "i" is a variable determined by runtime, then it is a variant offset
 * Return TRUE if the offset of this GEP insn is a constant.
 */
bool SVFIRBuilder::computeGepOffset(const User *V, LocationSet& ls)
{
    assert(V);

    const llvm::GEPOperator *gepOp = SVFUtil::dyn_cast<const llvm::GEPOperator>(V);
    DataLayout * dataLayout = getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule());
    llvm::APInt byteOffset(dataLayout->getIndexSizeInBits(gepOp->getPointerAddressSpace()),0,true);
    if(gepOp && dataLayout && gepOp->accumulateConstantOffset(*dataLayout,byteOffset))
    {
        //s32_t bo = byteOffset.getSExtValue();
    }

    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi)
    {
        const Type* gepTy = *gi;
        const Value* offsetVal = gi.getOperand();
        ls.addOffsetValue(offsetVal, gepTy);

        //The int value of the current index operand
        const ConstantInt *op = SVFUtil::dyn_cast<ConstantInt>(offsetVal);

        // if Options::ModelConsts is disabled. We will treat whole array as one, 
        // but we can distinguish different field of an array of struct, e.g. s[1].f1 is differet from s[0].f2
        if(SVFUtil::isa<ArrayType>(gepTy)){
            if(!op) 
                continue;
            s32_t idx = op->getSExtValue();
            u32_t offset = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(gepTy, idx);
            ls.setFldIdx(ls.accumulateConstantFieldIdx() + offset);  
        }
        else if (const StructType *ST = SVFUtil::dyn_cast<StructType>(gepTy))
        {
            assert(op && "non-const offset accessing a struct");
            //The actual index
            s32_t idx = op->getSExtValue();
            u32_t offset = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(ST, idx);
            ls.setFldIdx(ls.accumulateConstantFieldIdx() + offset);
        }
        else if (gepTy->isSingleValueType())
        {
            // If it's a non-constant offset access
            // If its point-to target is struct or array, it's likely an array accessing (%result = gep %struct.A* %a, i32 %non-const-index)
            // If its point-to target is single value (pointer arithmetic), then it's a variant gep (%result = gep i8* %p, i32 %non-const-index)
            if(!op && gepTy->isPointerTy() && gepTy->getPointerElementType()->isSingleValueType())
                return false;              
            
            // The actual index
            //s32_t idx = op->getSExtValue();

            // For pointer arithmetic we ignore the byte offset
            // consider using inferFieldIdxFromByteOffset(geopOp,dataLayout,ls,idx)?
            // ls.setFldIdx(ls.accumulateConstantFieldIdx() + inferFieldIdxFromByteOffset(geopOp,idx));
        }
    }
    return true;
}

/*!
 * Handle constant expression, and connect the gep edge
 */
void SVFIRBuilder::processCE(const Value *val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* gepce = isGepConstantExpr(ref))
        {
            DBOUT(DPAGBuild, outs() << "handle gep constant expression " << SVFUtil::value2String(ref) << "\n");
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
            DBOUT(DPAGBuild, outs() << "handle cast constant expression " << SVFUtil::value2String(ref) << "\n");
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
            DBOUT(DPAGBuild, outs() << "handle select constant expression " << SVFUtil::value2String(ref) << "\n");
            const Constant* src1 = selectce->getOperand(1);
            const Constant* src2 = selectce->getOperand(2);
            processCE(src1);
            processCE(src2);
            const Value* cval = getCurrentValue();
            const BasicBlock* cbb = getCurrentBB();
            setCurrentLocation(selectce, nullptr);
            NodeID cond = pag->getValueNode(selectce->getOperand(0));
            NodeID nsrc1 = pag->getValueNode(src1);
            NodeID nsrc2 = pag->getValueNode(src2);
            NodeID nres = pag->getValueNode(selectce);
            addSelectStmt(nres,nsrc1, nsrc2, cond);
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
			// is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and SVFIRBuilder.cpp:183-194
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
NodeID SVFIRBuilder::getGlobalVarField(const GlobalVariable *gvar, u32_t offset, Type* tpy)
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
        return getGepValVar(gvar, LocationSet(offset), tpy);
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
void SVFIRBuilder::InitialGlobal(const GlobalVariable *gvar, Constant *C,
                               u32_t offset)
{
    DBOUT(DPAGBuild, outs() << "global " << SVFUtil::value2String(gvar) << " constant initializer: " << SVFUtil::value2String(C) << "\n"); 
    if (C->getType()->isSingleValueType())
    {
        NodeID src = getValueNode(C);
        // get the field value if it is avaiable, otherwise we create a dummy field node.
        setCurrentLocation(gvar, nullptr);
        NodeID field = getGlobalVarField(gvar, offset, C->getType());

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
			// is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and SVFIRBuilder.cpp:183-194
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
    else if (SVFUtil::isa<ConstantArray>(C) || SVFUtil::isa<ConstantStruct>(C))
    {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++){
            u32_t off = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(C->getType(), i);
            InitialGlobal(gvar, SVFUtil::cast<Constant>(C->getOperand(i)), offset + off);
        }
    }
    else if(ConstantData* data = SVFUtil::dyn_cast<ConstantData>(C))
    {
        if(Options::ModelConsts){
            if(ConstantDataSequential* seq = SVFUtil::dyn_cast<ConstantDataSequential>(data)){
                for(u32_t i = 0; i < seq->getNumElements(); i++){
                    u32_t off = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(C->getType(), i);
                    Constant* ct = seq->getElementAsConstant(i);
                    InitialGlobal(gvar, ct, offset + off);
                }
            }
            else{
                InitialGlobal(gvar, data, offset);
            }
        }
    }
    else{
        //TODO:assert(SVFUtil::isa<ConstantVector>(C),"what else do we have");
    }
}

/*!
 *  Visit global variables for building SVFIR
 */
void SVFIRBuilder::visitGlobal(SVFModule* svfModule)
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
            DBOUT(DPAGBuild, outs() << "add global var node " << SVFUtil::value2String(gvar) << "\n");
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

        DBOUT(DPAGBuild, outs() << "add global function node " << fun->getName().str() << "\n");
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
 * Add edge V (dst) <-- O (src), V here is a value node on SVFIR, O is object node on SVFIR
 */
void SVFIRBuilder::visitAllocaInst(AllocaInst &inst)
{

    // AllocaInst should always be a pointer type
    assert(SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process alloca  " << SVFUtil::value2String(&inst) << " \n");
    NodeID dst = getValueNode(&inst);

    NodeID src = getObjectNode(&inst);

    addAddrEdge(src, dst);

}

/*!
 * Visit phi instructions
 */
void SVFIRBuilder::visitPHINode(PHINode &inst)
{

    DBOUT(DPAGBuild, outs() << "process phi " << SVFUtil::value2String(&inst) << "  \n");

    NodeID dst = getValueNode(&inst);

    for (u32_t i = 0; i < inst.getNumIncomingValues(); ++i)
    {
        const Value* val = inst.getIncomingValue(i);
        const Instruction* incomingInst = SVFUtil::dyn_cast<Instruction>(val);
        assert((incomingInst==nullptr) || (incomingInst->getFunction() == inst.getFunction()));
        const Instruction* predInst = &inst.getIncomingBlock(i)->back();
        const ICFGNode* icfgNode = pag->getICFG()->getICFGNode(predInst);
        NodeID src = getValueNode(val);
        addPhiStmt(dst,src,icfgNode);
    }
}

/*
 * Visit load instructions
 */
void SVFIRBuilder::visitLoadInst(LoadInst &inst)
{
    DBOUT(DPAGBuild, outs() << "process load  " << SVFUtil::value2String(&inst) << " \n");

    NodeID dst = getValueNode(&inst);

    NodeID src = getValueNode(inst.getPointerOperand());

    addLoadEdge(src, dst);
}

/*!
 * Visit store instructions
 */
void SVFIRBuilder::visitStoreInst(StoreInst &inst)
{
    // StoreInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process store " << SVFUtil::value2String(&inst) << " \n");

    NodeID dst = getValueNode(inst.getPointerOperand());

    NodeID src = getValueNode(inst.getValueOperand());

    addStoreEdge(src, dst);

}

/*!
 * Visit getelementptr instructions
 */
void SVFIRBuilder::visitGetElementPtrInst(GetElementPtrInst &inst)
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

    DBOUT(DPAGBuild, outs() << "process gep  " << SVFUtil::value2String(&inst) << " \n");

    NodeID src = getValueNode(inst.getPointerOperand());

    LocationSet ls;
    bool constGep = computeGepOffset(&inst, ls);
    addGepEdge(src, dst, ls, constGep);
}

/*
 * Visit cast instructions
 */
void SVFIRBuilder::visitCastInst(CastInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process cast  " << SVFUtil::value2String(&inst) << " \n");
    NodeID dst = getValueNode(&inst);

    if (SVFUtil::isa<IntToPtrInst>(&inst))
    {
        addBlackHoleAddrEdge(dst);
    }
    else
    {
        const Value * opnd = inst.getOperand(0);
        if (!SVFUtil::isa<PointerType>(opnd->getType()))
            opnd = stripAllCasts(opnd);

        NodeID src = getValueNode(opnd);
        addCopyEdge(src, dst);
    }
}

/*!
 * Visit Binary Operator
 */
void SVFIRBuilder::visitBinaryOperator(BinaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 2 && "not two operands for BinaryOperator?");
    Value* op1 = inst.getOperand(0);
    NodeID op1Node = getValueNode(op1);
    Value* op2 = inst.getOperand(1);
    NodeID op2Node = getValueNode(op2);
    u32_t opcode = inst.getOpcode();
    addBinaryOPEdge(op1Node, op2Node, dst, opcode);
}

/*!
 * Visit Unary Operator
 */
void SVFIRBuilder::visitUnaryOperator(UnaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 1 && "not one operand for Unary instruction?");
    Value* opnd = inst.getOperand(0);
    NodeID src = getValueNode(opnd);
    u32_t opcode = inst.getOpcode();
    addUnaryOPEdge(src, dst, opcode);
}

/*!
 * Visit compare instruction
 */
void SVFIRBuilder::visitCmpInst(CmpInst &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 2 && "not two operands for compare instruction?");
    Value* op1 = inst.getOperand(0);
    NodeID op1Node = getValueNode(op1);
    Value* op2 = inst.getOperand(1);
    NodeID op2Node = getValueNode(op2);
    u32_t predicate = inst.getPredicate();
    addCmpEdge(op1Node, op2Node, dst, predicate);
}


/*!
 * Visit select instructions
 */
void SVFIRBuilder::visitSelectInst(SelectInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process select  " << SVFUtil::value2String(&inst) << " \n");

    NodeID dst = getValueNode(&inst);
    NodeID src1 = getValueNode(inst.getTrueValue());
    NodeID src2 = getValueNode(inst.getFalseValue());
    NodeID cond = getValueNode(inst.getCondition());
    /// Two operands have same incoming basic block, both are the current BB
    addSelectStmt(dst,src1,src2, cond);
}

/*
 * Visit callsites
 */
void SVFIRBuilder::visitCallSite(CallSite cs)
{

    // skip llvm intrinsics
    if(isIntrinsicInst(cs.getInstruction()))
        return;

    DBOUT(DPAGBuild,
          outs() << "process callsite " << SVFUtil::value2String(cs.getInstruction()) << "\n");

    CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(cs.getInstruction());
    RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(cs.getInstruction());

    pag->addCallSite(callBlockNode);

    /// Collect callsite arguments and returns
    for(CallSite::arg_iterator itA = cs.arg_begin(), ieA = cs.arg_end(); itA!=ieA; ++itA)
        pag->addCallSiteArgs(callBlockNode,pag->getGNode(getValueNode(*itA)));

    if(!cs.getType()->isVoidTy())
        pag->addCallSiteRets(retBlockNode,pag->getGNode(getValueNode(cs.getInstruction())));

    const SVFFunction *callee = getCallee(cs);

    if (callee)
    {
        if (isExtCall(callee))
        {
            // There is no extpag for the function, use the old method.
            handleExtCall(cs, callee);
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
void SVFIRBuilder::visitReturnInst(ReturnInst &inst)
{

    // ReturnInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process return  " << SVFUtil::value2String(&inst) << " \n");

    if(Value *src = inst.getReturnValue())
    {
        const SVFFunction *F = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(inst.getParent()->getParent());

        NodeID rnF = getReturnNode(F);
        NodeID vnS = getValueNode(src);
        const ICFGNode* icfgNode = pag->getICFG()->getICFGNode(&inst);
        //vnS may be null if src is a null ptr
        addPhiStmt(rnF,vnS,icfgNode);
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
void SVFIRBuilder::visitExtractValueInst(ExtractValueInst  &inst)
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
void SVFIRBuilder::visitExtractElementInst(ExtractElementInst &inst)
{
    NodeID dst = getValueNode(&inst);
    addBlackHoleAddrEdge(dst);
}

/*!
 * Branch and switch instructions are treated as UnaryOP
 * br %cmp label %if.then, label %if.else
 */
void SVFIRBuilder::visitBranchInst(BranchInst &inst){
    NodeID brinst = getValueNode(&inst);
    NodeID cond;
	if (inst.isConditional())
		cond = getValueNode(inst.getCondition());
	else
		cond = pag->getNullPtr();

    assert(inst.getNumSuccessors() <= 2 && "if/else has more than two branches?");

    BranchStmt::SuccAndCondPairVec successors;
    for (u32_t i = 0; i < inst.getNumSuccessors(); ++i)
    {
        const Instruction* succInst = &inst.getSuccessor(i)->front();
        const ICFGNode* icfgNode = pag->getICFG()->getICFGNode(succInst);
        successors.push_back(std::make_pair(icfgNode, 1-i));
    }
    addBranchStmt(brinst, cond,successors);
}

void SVFIRBuilder::visitSwitchInst(SwitchInst &inst){
    NodeID brinst = getValueNode(&inst);
    NodeID cond = getValueNode(inst.getCondition());

    BranchStmt::SuccAndCondPairVec successors;
    for (u32_t i = 0; i < inst.getNumSuccessors(); ++i)
    {
        const Instruction* succInst = &inst.getSuccessor(i)->front();
        const ConstantInt* condVal = inst.findCaseDest(inst.getSuccessor(i));
        /// default case is set to -1;
        s32_t val = condVal ? condVal->getSExtValue() : -1;
        const ICFGNode* icfgNode = pag->getICFG()->getICFGNode(succInst);
        successors.push_back(std::make_pair(icfgNode,val));
    }
    addBranchStmt(brinst, cond,successors);
}

///   %ap = alloca %struct.va_list
///  %ap2 = bitcast %struct.va_list* %ap to i8*
/// ; Read a single integer argument from %ap2
/// %tmp = va_arg i8* %ap2, i32 (VAArgInst)
/// TODO: for now, create a copy edge from %ap2 to %tmp, we assume here %tmp should point to the n-th argument of the var_args
void SVFIRBuilder::visitVAArgInst(VAArgInst &inst){
    NodeID dst = getValueNode(&inst);
    Value* opnd = inst.getPointerOperand();
    NodeID src = getValueNode(opnd);
    addCopyEdge(src,dst);
}

/// <result> = freeze ty <val>
/// If <val> is undef or poison, ‘freeze’ returns an arbitrary, but fixed value of type `ty`
/// Otherwise, this instruction is a no-op and returns the input <val>
/// For now, we assume <val> is never a posion or undef.
void SVFIRBuilder::visitFreezeInst(FreezeInst &inst){
    NodeID dst = getValueNode(&inst);
    for (u32_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value* opnd = inst.getOperand(i);
        NodeID src = getValueNode(opnd);
        addCopyEdge(src,dst);
    }
}


/*!
 * Add the constraints for a direct, non-external call.
 */
void SVFIRBuilder::handleDirectCall(CallSite cs, const SVFFunction *F)
{

    assert(F);

    DBOUT(DPAGBuild,
           outs() << "handle direct call " << SVFUtil::value2String(cs.getInstruction()) << " callee " << *F << "\n");

    //Only handle the ret.val. if it's used as a ptr.
    NodeID dstrec = getValueNode(cs.getInstruction());
    //Does it actually return a ptr?
    if (!cs.getType()->isVoidTy())
    {
        NodeID srcret = getReturnNode(F);
        CallICFGNode* callICFGNode = pag->getICFG()->getCallICFGNode(cs.getInstruction());
        FunExitICFGNode* exitICFGNode = pag->getICFG()->getFunExitICFGNode(F);
        addRetEdge(srcret, dstrec,callICFGNode, exitICFGNode);
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

        DBOUT(DPAGBuild, outs() << "process actual parm  " << SVFUtil::value2String(AA) << " \n");

        NodeID dstFA = getValueNode(FA);
        NodeID srcAA = getValueNode(AA);
        CallICFGNode* icfgNode = pag->getICFG()->getCallICFGNode(cs.getInstruction());
        FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(F);
        addCallEdge(srcAA, dstFA, icfgNode, entry);
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
            CallICFGNode* icfgNode = pag->getICFG()->getCallICFGNode(cs.getInstruction());
            FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(F);
            addCallEdge(vnAA,vaF, icfgNode,entry);
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

const Value* SVFIRBuilder::getBaseValueForExtArg(const Value* V){
    const Value * value = stripAllCasts(V);
    assert(value && "null ptr?");
    if(const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(value)){
        s32_t totalidx = 0;
        for (bridge_gep_iterator gi = bridge_gep_begin(gep), ge = bridge_gep_end(gep); gi != ge; ++gi){
            if(const ConstantInt *op = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand()))
                totalidx += op->getSExtValue();
        }
        if(totalidx == 0 && !SVFUtil::isa<StructType>(value->getType()))
            value = gep->getPointerOperand();
    }
    return value;
}

/*!
 * Find the base type and the max possible offset of an object pointed to by (V).
 */
const Type *SVFIRBuilder::getBaseTypeAndFlattenedFields(const Value *V, std::vector<LocationSet> &fields, const Value* szValue)
{
    assert(V);
    const Value* value = getBaseValueForExtArg(V);
    const Type *T = value->getType();
    while (const PointerType *ptype = SVFUtil::dyn_cast<PointerType>(T))
        T = ptype->getElementType();

    u32_t numOfElems = SymbolTableInfo::SymbolInfo()->getNumOfFlattenElements(T);
    /// use user-specified size for this copy operation if the size is a constaint int
    if(szValue && SVFUtil::isa<ConstantInt>(szValue)){
        numOfElems = (numOfElems > SVFUtil::cast<ConstantInt>(szValue)->getSExtValue()) ? SVFUtil::cast<ConstantInt>(szValue)->getSExtValue() : numOfElems;
    }

    LLVMContext& context = LLVMModuleSet::getLLVMModuleSet()->getContext();
    for(u32_t ei = 0; ei < numOfElems; ei++){
        LocationSet ls(ei); 
        // make a ConstantInt and create char for the content type due to byte-wise copy 
        const ConstantInt* offset = ConstantInt::get(context, llvm::APInt(32, ei));
        ls.addOffsetValue(offset, nullptr);
        fields.push_back(ls);
    }
    return T;
}

/*!
 * Add the load/store constraints and temp. nodes for the complex constraint
 * *D = *S (where D/S may point to structs).
 */
void SVFIRBuilder::addComplexConsForExt(Value *D, Value *S, const Value* szValue)
{
    assert(D && S);
    NodeID vnD= getValueNode(D), vnS= getValueNode(S);
    if(!vnD || !vnS)
        return;

    std::vector<LocationSet> fields;

    //Get the max possible size of the copy, unless it was provided.
    std::vector<LocationSet> srcFields;
    std::vector<LocationSet> dstFields;
    const Type *stype = getBaseTypeAndFlattenedFields(S, srcFields, szValue);
    const Type *dtype = getBaseTypeAndFlattenedFields(D, dstFields, szValue);
    if(srcFields.size() > dstFields.size())
        fields = dstFields;
    else
        fields = srcFields;

    /// If sz is 0, we will add edges for all fields.
    u32_t sz = fields.size();

    if (fields.size() == 1 && (isConstantData(D) || isConstantData(S))) {
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(vnD,dummy);
        addStoreEdge(dummy,vnS);
        return;
    }

    //For each field (i), add (Ti = *S + i) and (*D + i = Ti).
    for (u32_t index = 0; index < sz; index++)
    {
        const Type* dElementType = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(dtype, fields[index].accumulateConstantFieldIdx());
        const Type* sElementType = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(stype, fields[index].accumulateConstantFieldIdx());
        NodeID dField = getGepValVar(D,fields[index],dElementType);
        NodeID sField = getGepValVar(S,fields[index],sElementType);
        NodeID dummy = pag->addDummyValNode();
        addLoadEdge(sField,dummy);
        addStoreEdge(dummy,dField);
    }
}


/*!
 * Handle external calls
 */
void SVFIRBuilder::handleExtCall(CallSite cs, const SVFFunction *callee)
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
                NodeID obj = pag->addDummyObjNode(arg->getType());
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
            case ExtAPI::EFT_L_A0: {
                NodeID dstNode = getValueNode(inst);
                Value *src= cs.getArgument(0);
                NodeID srcNode = getValueNode(src);
                addCopyEdge(srcNode, dstNode);
                break;
            }

            case ExtAPI::EFT_L_A1:
            case ExtAPI::EFT_L_A2:
            case ExtAPI::EFT_L_A8:
            {
                if(!SVFUtil::isa<PointerType>(inst->getType()))
                    break;
                NodeID dstNode = getValueNode(inst);
                u32_t arg_pos;
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
		// this is for memset(void *str, int c, size_t n)
		// which copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str
                std::vector<LocationSet> dstFields;
                const Type *dtype = getBaseTypeAndFlattenedFields(cs.getArgument(0), dstFields, cs.getArgument(2));
                u32_t sz = dstFields.size();
                //For each field (i), add store edge *(arg0 + i) = arg1
                for (u32_t index = 0; index < sz; index++)
                {
                    const Type* dElementType = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(dtype, dstFields[index].accumulateConstantFieldIdx());
                    NodeID dField = getGepValVar(cs.getArgument(0), dstFields[index], dElementType);
                    addStoreEdge(pag->getValueNode(cs.getArgument(1)),dField);
                }
                if(SVFUtil::isa<PointerType>(inst->getType()))
                    addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
                break;
            }
            case ExtAPI::EFT_L_A0__A0R_A1R:
            {
                addComplexConsForExt(cs.getArgument(0), cs.getArgument(1), cs.getArgument(2));
                //memcpy returns the dest.
                addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
                break;
            }
            case ExtAPI::EFT_A1R_A0R:
                addComplexConsForExt(cs.getArgument(1), cs.getArgument(0), cs.getArgument(2));
                break;
            case ExtAPI::EFT_L_A1__FunPtr:
            {
                /// handling external function e.g., void *dlsym(void *handle, const char *funname); 
                const Value *src = cs.getArgument(1);
                if(const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(src))
                    src = stripConstantCasts(gep->getPointerOperand());
                if(const GlobalVariable* glob = SVFUtil::dyn_cast<GlobalVariable>(src)){
                    if(const ConstantDataArray* constarray = SVFUtil::dyn_cast<ConstantDataArray>(glob->getInitializer())){
                        if(const SVFFunction* fun = getProgFunction(svfMod,constarray->getAsCString().str())){
                            NodeID srcNode = getValueNode(fun->getLLVMFun());
                            addCopyEdge(srcNode,  getValueNode(inst));
                        }
                    }
                }
                break;
            }
            case ExtAPI::EFT_A3R_A1R_NS:
                //These func. are never used to copy structs, so the size is 1.
                addComplexConsForExt(cs.getArgument(3), cs.getArgument(1), nullptr);
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
                /// handle strcpy
                if(cs.arg_size()>=3)
                    addComplexConsForExt(cs.getArgument(0), cs.getArgument(1), cs.getArgument(2));
                else
                    addComplexConsForExt(cs.getArgument(0), cs.getArgument(1), nullptr);
                //strcpy returns the dest.
                addCopyEdge(getValueNode(cs.getArgument(0)), getValueNode(inst));
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
            case ExtAPI::EFT_FREE_MULTILEVEL:
            {
                // For any argument with 2-level pointer passing too a free function e.g., XFree(void** p), 
                // we will need to add a load edge (dummy = *p) and a store edge (*dummy = nullptr).
                for(u32_t i = 0; i < cs.arg_size(); i++)
                {
                    Value *arg = cs.getArgument(i);
                    if(const PointerType* pty = SVFUtil::dyn_cast<PointerType>(arg->getType()))
                    {
                        if(pty->getElementType()->isPointerTy())
                        {
                            NodeID dummy = pag->addDummyValNode();
                            addLoadEdge(pag->getValueNode(arg),dummy);
                            addStoreEdge(pag->getNullPtr(),dummy);
                        }
                    }
                }
                break;
            }  
            case ExtAPI::EFT_STD_RB_TREE_INSERT_AND_REBALANCE:
            {
                Value *vArg1 = cs.getArgument(1);
                Value *vArg3 = cs.getArgument(3);

                // We have vArg3 points to the entry of _Rb_tree_node_base { color; parent; left; right; }.
                // Now we calculate the offset from base to vArg3
                NodeID vnArg3 = pag->getValueNode(vArg3);
                s32_t offset = pag->getLocationSetFromBaseNode(vnArg3).accumulateConstantFieldIdx();

                // We get all flattened fields of base
                vector<LocationSet> fields;
                const Type *type = getBaseTypeAndFlattenedFields(vArg3, fields, nullptr);
                assert(fields.size() >= 4 && "_Rb_tree_node_base should have at least 4 fields.\n");

                // We summarize the side effects: arg3->parent = arg1, arg3->left = arg1, arg3->right = arg1
                // Note that arg0 is aligned with "offset".
                for (s32_t i = offset + 1; i <= offset + 3; ++i)
                {
                    if((u32_t)i >= fields.size())
                        break;
                    const Type* elementType = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(type, fields[i].accumulateConstantFieldIdx());
                    NodeID vnD = getGepValVar(vArg3, fields[i], elementType);
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
                s32_t offset = pag->getLocationSetFromBaseNode(vnArg).accumulateConstantFieldIdx();

                // We get all fields
                vector<LocationSet> fields;
                const Type *type = getBaseTypeAndFlattenedFields(vArg,fields,nullptr);
                assert(fields.size() >= 4 && "_Rb_tree_node_base should have at least 4 fields.\n");

                // We summarize the side effects: ret = arg->parent, ret = arg->left, ret = arg->right
                // Note that arg0 is aligned with "offset".
                for (s32_t i = offset + 1; i <= offset + 3; ++i)
                {
                    if((u32_t)i >= fields.size())
                        break;
                    const Type* elementType = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(type, fields[i].accumulateConstantFieldIdx());
                    NodeID vnS = getGepValVar(vArg, fields[i], elementType);
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

        /// create inter-procedural SVFIR edges for thread forks
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
                        CallICFGNode* icfgNode = pag->getICFG()->getCallICFGNode(inst);
                        FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(forkedFun));
                        addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),icfgNode, entry);
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
            /// remember to update inter-procedural callgraph/SVFIR/SVFG etc. when indirect call targets are resolved
            /// We don't connect the callgraph here, further investigation is need to hanle mod-ref during SVFG construction.
        }

        /// create inter-procedural SVFIR edges for hare_parallel_for calls
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
                    CallICFGNode* icfgNode = pag->getICFG()->getCallICFGNode(inst);
                    FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(taskFunc));
                    addThreadForkEdge(pag->getValueNode(actualParm), pag->getValueNode(formalParm),icfgNode, entry);
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

        /// TODO: inter-procedural SVFIR edges for thread joins
    }
}

/*!
 * Indirect call is resolved on-the-fly during pointer analysis
 */
void SVFIRBuilder::handleIndCall(CallSite cs)
{
    const CallICFGNode* cbn = pag->getICFG()->getCallICFGNode(cs.getInstruction());
    pag->addIndirectCallsites(cbn,pag->getValueNode(cs.getCalledValue()));
}

/*
 * TODO: more sanity checks might be needed here
 */
void SVFIRBuilder::sanityCheck()
{
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
        (void) pag->getGNode(nIter->first);
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
NodeID SVFIRBuilder::getGepValVar(const Value* val, const LocationSet& ls, const Type *elementType)
{
    NodeID base = pag->getBaseValVar(getValueNode(val));
    NodeID gepval = pag->getGepValVar(curVal, base, ls);
    if (gepval==UINT_MAX)
    {
        assert(((int) UINT_MAX)==-1 && "maximum limit of unsigned int is not -1?");
        /*
         * getGepValVar can only be called from two places:
         * 1. SVFIRBuilder::addComplexConsForExt to handle external calls
         * 2. SVFIRBuilder::getGlobalVarField to initialize global variable
         * so curVal can only be
         * 1. Instruction
         * 2. GlobalVariable
         */
        assert((SVFUtil::isa<Instruction>(curVal) || SVFUtil::isa<GlobalVariable>(curVal)) && "curVal not an instruction or a globalvariable?");

        // We assume every GepValNode and its GepEdge to the baseNode are unique across the whole program
        // We preserve the current BB information to restore it after creating the gepNode
        const Value* cval = getCurrentValue();
        const BasicBlock* cbb = getCurrentBB();
        setCurrentLocation(curVal, nullptr);
        NodeID gepNode= pag->addGepValNode(curVal, val,ls, NodeIDAllocator::get()->allocateValueId(),elementType->getPointerTo());
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
 * Argument             CopyEdge  (SVFIR::addFormalParamBlackHoleAddrEdge)
 * ConstantExpr         CopyEdge  (Int2PtrConstantExpr   CastConstantExpr  SVFIRBuilder::processCE)
 *                      GepEdge   (GepConstantExpr   SVFIRBuilder::processCE)
 * ConstantPointerNull  CopyEdge  (3-->2 NullPtr-->BlkPtr SVFIR::addNullPtrNode)
 *  				    AddrEdge  (0-->2 BlkObj-->BlkPtr SVFIR::addNullPtrNode)
 * GlobalVariable       AddrEdge  (SVFIRBuilder::visitGlobal)
 *                      GepEdge   (SVFIRBuilder::getGlobalVarField)
 * Function             AddrEdge  (SVFIRBuilder::visitGlobal)
 * Constant             StoreEdge (SVFIRBuilder::InitialGlobal)
 */
void SVFIRBuilder::setCurrentBBAndValueForPAGEdge(PAGEdge* edge)
{
    if (SVFModule::pagReadFromTXT())
        return;

    assert(curVal && "current Val is nullptr?");
    edge->setBB(curBB);
    edge->setValue(curVal);
    // backmap in valuToEdgeMap
    pag->mapValueToEdge(curVal, edge);
    ICFGNode* icfgNode = pag->getICFG()->getGlobalICFGNode();
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

        /// We assume every GepValVar and its GepStmt are unique across whole program
        if (!(SVFUtil::isa<GepStmt>(edge) && SVFUtil::isa<GepValVar>(edge->getDstNode())))
            assert(curBB && "instruction does not have a basic block??");

        /// We will have one unique function exit ICFGNode for all returns
        if(const ReturnInst* retInst = SVFUtil::dyn_cast<ReturnInst>(curInst)){
            const SVFFunction *fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(retInst->getParent()->getParent());
            icfgNode = pag->getICFG()->getFunExitICFGNode(fun);
        }
        else{
            if(SVFUtil::isa<RetPE>(edge))
                icfgNode = pag->getICFG()->getRetICFGNode(curInst);
            else
                icfgNode = pag->getICFG()->getICFGNode(curInst);
        }
    }
    else if (const Argument* arg = SVFUtil::dyn_cast<Argument>(curVal))
    {
        assert(curBB && (&curBB->getParent()->getEntryBlock() == curBB));
        const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(arg->getParent());
        icfgNode = pag->getICFG()->getFunEntryICFGNode(fun);
    }
    else if (SVFUtil::isa<ConstantExpr>(curVal))
    {
        if (!curBB)
            pag->addGlobalPAGEdge(edge);
        else
    		icfgNode = pag->getICFG()->getICFGNode(&curBB->front());
    }
    else if (SVFUtil::isa<GlobalVariable>(curVal) ||
             SVFUtil::isa<Constant>(curVal) ||
			 SVFUtil::isa<MetadataAsValue>(curVal))
    {
        pag->addGlobalPAGEdge(edge);
    }
    else if(const Function* fun = SVFUtil::dyn_cast<Function>(curVal)){
        const SVFFunction* f = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
        if((&fun->getEntryBlock() == curBB) && isExtCall(f)){
            /// all external function connected to a indirect call, we will put SVFStmts in the FunctionEntryICFGNode
            icfgNode = pag->getICFG()->getFunEntryICFGNode(f);
        }
        else
            pag->addGlobalPAGEdge(edge);
    }
    else
    {
        assert(false && "what else value can we have?");
    }

    pag->addToSVFStmtList(icfgNode,edge);
    icfgNode->addSVFStmt(edge);
    if(const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(edge)){
        CallICFGNode* callNode = const_cast<CallICFGNode*>(callPE->getCallSite());
        FunEntryICFGNode* entryNode = const_cast<FunEntryICFGNode*>(callPE->getFunEntryICFGNode());
        if(ICFGEdge* edge = pag->getICFG()->hasInterICFGEdge(callNode,entryNode, ICFGEdge::CallCF))
            SVFUtil::cast<CallCFGEdge>(edge)->addCallPE(callPE);
    }
    else if(const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(edge)){
        RetICFGNode* retNode = const_cast<RetICFGNode*>(retPE->getCallSite()->getRetICFGNode());
        FunExitICFGNode* exitNode = const_cast<FunExitICFGNode*>(retPE->getFunExitICFGNode());
        if(ICFGEdge* edge = pag->getICFG()->hasInterICFGEdge(exitNode, retNode, ICFGEdge::RetCF))
            SVFUtil::cast<RetCFGEdge>(edge)->addRetPE(retPE);
    }
}

void SVFIRBuilder::updateCallGraph(PTACallGraph* callgraph){
    PTACallGraph::CallEdgeMap::const_iterator iter = callgraph->getIndCallMap().begin();
    PTACallGraph::CallEdgeMap::const_iterator eiter = callgraph->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallICFGNode* callBlock = iter->first;
        CallSite cs = getLLVMCallSite(callBlock->getCallSite());
        assert(callBlock->isIndirectCall() && "this is not an indirect call?");
        const PTACallGraph::FunctionSet & functions = iter->second;
        for (PTACallGraph::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction*  callee = *func_iter;
            if (isExtCall(callee)){
                setCurrentLocation(callee->getLLVMFun(), &callee->getLLVMFun()->getEntryBlock());
                handleExtCall(cs, callee);
            }
            else{
                setCurrentLocation(callBlock->getCallSite(), callBlock->getCallSite()->getParent());
                handleDirectCall(cs, callee);
            }
        }
    }

    // dump SVFIR
    if (Options::PAGDotGraph)
        pag->dump("svfir_final");
}
