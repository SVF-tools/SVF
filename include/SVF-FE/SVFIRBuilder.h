//===- SVFIRBuilder.h -- Building SVFIR-------------------------------------------//
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
 * SVFIRBuilder.h
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#ifndef PAGBUILDER_H_
#define PAGBUILDER_H_

#include "MemoryModel/SVFIR.h"
#include "Util/ExtAPI.h"
#include "SVF-FE/BasicTypes.h"
#include "SVF-FE/ICFGBuilder.h"

namespace SVF
{

class SVFModule;
/*!
 *  SVFIR Builder to create SVF variables and statements and PAG
 */
class SVFIRBuilder: public llvm::InstVisitor<SVFIRBuilder>
{

private:
    SVFIR* pag;
    SVFModule* svfMod;
    const BasicBlock* curBB;	///< Current basic block during SVFIR construction when visiting the module
    const Value* curVal;	///< Current Value during SVFIR construction when visiting the module

public:
    /// Constructor
    SVFIRBuilder(): pag(SVFIR::getPAG()), svfMod(nullptr), curBB(nullptr),curVal(nullptr)
    {
    }
    /// Destructor
    virtual ~SVFIRBuilder()
    {
    }

    /// Start building SVFIR here
    virtual SVFIR* build(SVFModule* svfModule);

    /// Return SVFIR
    SVFIR* getPAG() const
    {
        return pag;
    }

    /// Initialize nodes and edges
    //@{
    void initialiseNodes();
    void addEdge(NodeID src, NodeID dst, SVFStmt::PEDGEK kind,
                 s32_t offset = 0, Instruction* cs = nullptr);
    // @}

    /// Sanity check for SVFIR
    void sanityCheck();

    /// Get different kinds of node
    //@{
    // GetValNode - Return the value node according to a LLVM Value.
    NodeID getValueNode(const Value *V)
    {
        // first handle gep edge if val if a constant expression
        processCE(V);

        // strip off the constant cast and return the value node
        return pag->getValueNode(V);
    }

    /// GetObject - Return the object node (stack/global/heap/function) according to a LLVM Value
    inline NodeID getObjectNode(const Value *V)
    {
        return pag->getObjectNode(V);
    }

    /// getReturnNode - Return the node representing the unique return value of a function.
    inline NodeID getReturnNode(const SVFFunction *func)
    {
        return pag->getReturnNode(func);
    }

    /// getVarargNode - Return the node representing the unique variadic argument of a function.
    inline NodeID getVarargNode(const SVFFunction *func)
    {
        return pag->getVarargNode(func);
    }
    //@}
    /// Update SVFIR given a call graph (creating new CallPE and RetPE for resolved indirect calls)
    void updateCallGraph(PTACallGraph* callgraph);


    /// Our visit overrides.
    //@{
    // Instructions that cannot be folded away.
    virtual void visitAllocaInst(AllocaInst &AI);
    void visitPHINode(PHINode &I);
    void visitStoreInst(StoreInst &I);
    void visitLoadInst(LoadInst &I);
    void visitGetElementPtrInst(GetElementPtrInst &I);
    void visitCallInst(CallInst &I)
    {
        visitCallSite(&I);
    }
    void visitInvokeInst(InvokeInst &II)
    {
        visitCallSite(&II);
    }
    void visitCallBrInst(CallBrInst &I) {
      return visitCallSite(&I);
    }
    void visitCallSite(CallSite cs);
    void visitReturnInst(ReturnInst &I);
    void visitCastInst(CastInst &I);
    void visitSelectInst(SelectInst &I);
    void visitExtractValueInst(ExtractValueInst  &EVI);
    void visitBranchInst(BranchInst &I);
    void visitSwitchInst(SwitchInst &I);
    void visitInsertValueInst(InsertValueInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }
    // TerminatorInst and UnwindInst have been removed since llvm-8.0.0
    // void visitTerminatorInst(TerminatorInst &TI) {}
    // void visitUnwindInst(UnwindInst &I) { /*returns void*/}

    void visitBinaryOperator(BinaryOperator &I);
    void visitUnaryOperator(UnaryOperator &I);
    void visitCmpInst(CmpInst &I);

    /// TODO: var arguments need to be handled.
    /// https://llvm.org/docs/LangRef.html#id1911
    void visitVAArgInst(VAArgInst&);
    void visitVACopyInst(VACopyInst&){}
    void visitVAEndInst(VAEndInst&){}
    void visitVAStartInst(VAStartInst&){}

    /// <result> = freeze ty <val>
    /// If <val> is undef or poison, ‘freeze’ returns an arbitrary, but fixed value of type `ty`
    /// Otherwise, this instruction is a no-op and returns the input <val>
    void visitFreezeInst(FreezeInst& I);

    void visitExtractElementInst(ExtractElementInst &I);

    void visitInsertElementInst(InsertElementInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitShuffleVectorInst(ShuffleVectorInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitLandingPadInst(LandingPadInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Instruction not that often
    void visitResumeInst(ResumeInst&)   /*returns void*/
    {
    }
    void visitUnreachableInst(UnreachableInst&)   /*returns void*/
    {
    }
    void visitFenceInst(FenceInst &I)   /*returns void*/
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicRMWInst(AtomicRMWInst &I)
    {
        addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Provide base case for our instruction visit.
    inline void visitInstruction(Instruction&)
    {
        // If a new instruction is added to LLVM that we don't handle.
        // TODO: ignore here:
    }
    //}@

protected:
    /// Handle globals including (global variable and functions)
    //@{
    void visitGlobal(SVFModule* svfModule);
    void InitialGlobal(const GlobalVariable *gvar, Constant *C,
                       u32_t offset);
    NodeID getGlobalVarField(const GlobalVariable *gvar, u32_t offset, Type* tpy);
    //@}

    /// Process constant expression
    void processCE(const Value *val);

    /// Infer field index from byteoffset.
    u32_t inferFieldIdxFromByteOffset(const llvm::GEPOperator* gepOp, DataLayout *dl, LocationSet& ls, s32_t idx);

    /// Compute offset of a gep instruction or gep constant expression
    bool computeGepOffset(const User *V, LocationSet& ls);

    /// Get the base value of (i8* src and i8* dst) for external argument (e.g. memcpy(i8* dst, i8* src, int size))
    const Value* getBaseValueForExtArg(const Value* V);

    /// Get the base type and max offset
    const Type *getBaseTypeAndFlattenedFields(const Value *V, std::vector<LocationSet> &fields, const Value* sz);

    /// Handle direct call
    void handleDirectCall(CallSite cs, const SVFFunction *F);

    /// Handle indirect call
    void handleIndCall(CallSite cs);

    /// Handle external call
    //@{
    virtual void handleExtCall(CallSite cs, const SVFFunction *F);
    void addComplexConsForExt(Value *D, Value *S, const Value* sz);
    //@}

    /// Set current basic block in order to keep track of control flow information
    inline void setCurrentLocation(const Value* val, const BasicBlock* bb)
    {
        curBB = bb;
        curVal = val;
    }
    inline const Value *getCurrentValue() const
    {
        return curVal;
    }
    inline const BasicBlock *getCurrentBB() const
    {
        return curBB;
    }

    /// Add global black hole Address edge
    void addGlobalBlackHoleAddrEdge(NodeID node, const ConstantExpr *int2Ptrce)
    {
        const Value* cval = getCurrentValue();
        const BasicBlock* cbb = getCurrentBB();
        setCurrentLocation(int2Ptrce,nullptr);
        addBlackHoleAddrEdge(node);
        setCurrentLocation(cval,cbb);
    }

    /// Add NullPtr PAGNode
    inline NodeID addNullPtrNode()
    {
        NodeID nullPtr = pag->addDummyValNode(pag->getNullPtr());
        /// let all undef value or non-determined pointers points-to black hole
        LLVMContext &cxt = LLVMModuleSet::getLLVMModuleSet()->getContext();
        ConstantPointerNull *constNull = ConstantPointerNull::get(Type::getInt8PtrTy(cxt));
        setCurrentLocation(constNull, nullptr);
        addBlackHoleAddrEdge(pag->getBlkPtr());
        return nullPtr;
    }

    NodeID getGepValVar(const Value* val, const LocationSet& ls, const Type *baseType);

    void setCurrentBBAndValueForPAGEdge(PAGEdge* edge);

    inline void addBlackHoleAddrEdge(NodeID node)
    {
        if(PAGEdge *edge = pag->addBlackHoleAddrStmt(node))
            setCurrentBBAndValueForPAGEdge(edge);
    }

    /// Add Address edge
    inline AddrStmt* addAddrEdge(NodeID src, NodeID dst)
    {
        if(AddrStmt *edge = pag->addAddrStmt(src, dst)){
            setCurrentBBAndValueForPAGEdge(edge);
            return edge;
        }
        return nullptr;
    }
    /// Add Copy edge
    inline CopyStmt* addCopyEdge(NodeID src, NodeID dst)
    {
        if(CopyStmt *edge = pag->addCopyStmt(src, dst)){
            setCurrentBBAndValueForPAGEdge(edge);
            return edge;
        }
        return nullptr;
    }
    /// Add Copy edge
    inline void addPhiStmt(NodeID res, NodeID opnd, const ICFGNode* pred)
    {
        /// If we already added this phi node, then skip this adding
        if(PhiStmt *edge = pag->addPhiStmt(res,opnd,pred))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add SelectStmt
    inline void addSelectStmt(NodeID res, NodeID op1, NodeID op2, NodeID cond)
    {
        if(SelectStmt *edge = pag->addSelectStmt(res,op1,op2,cond))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Copy edge
    inline void addCmpEdge(NodeID op1, NodeID op2, NodeID dst, u32_t predict)
    {
        if(CmpStmt *edge = pag->addCmpStmt(op1, op2, dst, predict))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Copy edge
    inline void addBinaryOPEdge(NodeID op1, NodeID op2, NodeID dst, u32_t opcode)
    {
        if(BinaryOPStmt *edge = pag->addBinaryOPStmt(op1, op2, dst, opcode))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Unary edge
    inline void addUnaryOPEdge(NodeID src, NodeID dst, u32_t opcode)
    {
        if(UnaryOPStmt *edge = pag->addUnaryOPStmt(src, dst, opcode))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Branch statement 
    inline void addBranchStmt(NodeID br, NodeID cond, const BranchStmt::SuccAndCondPairVec& succs){
        if(BranchStmt *edge = pag->addBranchStmt(br, cond, succs))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Load edge
    inline void addLoadEdge(NodeID src, NodeID dst)
    {
        if(LoadStmt *edge = pag->addLoadStmt(src, dst))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Store edge
    inline void addStoreEdge(NodeID src, NodeID dst)
    {
        IntraICFGNode* node;
        if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(curVal))
            node = pag->getICFG()->getIntraICFGNode(inst);
        else
            node = nullptr;
        if(StoreStmt *edge = pag->addStoreStmt(src, dst, node))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Call edge
    inline void addCallEdge(NodeID src, NodeID dst, const CallICFGNode* cs, const FunEntryICFGNode* entry)
    {
        if(CallPE *edge = pag->addCallPE(src, dst, cs, entry))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Return edge
    inline void addRetEdge(NodeID src, NodeID dst, const CallICFGNode* cs, const FunExitICFGNode* exit)
    {
        if(RetPE *edge = pag->addRetPE(src, dst, cs, exit))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Gep edge
    inline void addGepEdge(NodeID src, NodeID dst, const LocationSet& ls, bool constGep)
    {
        if(GepStmt *edge = pag->addGepStmt(src, dst, ls, constGep))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Offset(Gep) edge
    inline void addNormalGepEdge(NodeID src, NodeID dst, const LocationSet& ls)
    {
        if(GepStmt *edge = pag->addNormalGepStmt(src, dst, ls))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Variant(Gep) edge
    inline void addVariantGepEdge(NodeID src, NodeID dst, const LocationSet& ls)
    {
        if(GepStmt *edge = pag->addVariantGepStmt(src, dst, ls))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Thread fork edge for parameter passing
    inline void addThreadForkEdge(NodeID src, NodeID dst, const CallICFGNode* cs, const FunEntryICFGNode* entry)
    {
        if(TDForkPE *edge = pag->addThreadForkPE(src, dst, cs, entry))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Thread join edge for parameter passing
    inline void addThreadJoinEdge(NodeID src, NodeID dst, const CallICFGNode* cs, const FunExitICFGNode* exit)
    {
        if(TDJoinPE *edge = pag->addThreadJoinPE(src, dst, cs, exit))
            setCurrentBBAndValueForPAGEdge(edge);
    }
    //@}

};

} // End namespace SVF

#endif /* PAGBUILDER_H_ */
