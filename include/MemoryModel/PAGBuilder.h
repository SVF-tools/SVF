//===- PAGBuilder.h -- Building PAG-------------------------------------------//
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
 * PAGBuilder.h
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#ifndef PAGBUILDER_H_
#define PAGBUILDER_H_

#include "MemoryModel/PAG.h"
#include "Util/ExtAPI.h"

class SVFModule;
/*!
 *  PAG Builder
 */
class PAGBuilder: public llvm::InstVisitor<PAGBuilder> {
private:
    PAG* pag;
    SVFModule svfMod;
public:
    /// Constructor
    PAGBuilder(): pag(PAG::getPAG()) {
    }
    /// Destructor
    virtual ~PAGBuilder() {
    }

    /// Start building PAG here
    PAG* build(SVFModule svfModule);

    /// Return PAG
    PAG* getPAG() const {
        return pag;
    }

    /// Initialize nodes and edges
    //@{
    void initalNode();
    void addEdge(NodeID src, NodeID dst, PAGEdge::PEDGEK kind,
                 Size_t offset = 0, Instruction* cs = NULL);
    // @}

    /// Sanity check for PAG
    void sanityCheck();

    /// Get different kinds of node
    //@{
    // GetValNode - Return the value node according to a LLVM Value.
    NodeID getValueNode(const Value *V) {
        // first handle gep edge if val if a constant expression
        processCE(V);

        // strip off the constant cast and return the value node
        return pag->getValueNode(V);
    }

    /// GetObject - Return the object node (stack/global/heap/function) according to a LLVM Value
    inline NodeID getObjectNode(const Value *V) {
        return pag->getObjectNode(V);
    }

    /// getReturnNode - Return the node representing the unique return value of a function.
    inline NodeID getReturnNode(const Function *func) {
        return pag->getReturnNode(func);
    }

    /// getVarargNode - Return the node representing the unique variadic argument of a function.
    inline NodeID getVarargNode(const Function *func) {
        return pag->getVarargNode(func);
    }
    //@}

    /// Handle globals including (global variable and functions)
    //@{
    void visitGlobal(SVFModule svfModule);
    void InitialGlobal(const GlobalVariable *gvar, Constant *C,
                       u32_t offset);
    NodeID getGlobalVarField(const GlobalVariable *gvar, u32_t offset);
    //@}

    /// Process constant expression
    void processCE(const Value *val);

    /// Compute offset of a gep instruction or gep constant expression
    bool computeGepOffset(const User *V, LocationSet& ls);

    /// Handle direct call
    void handleDirectCall(CallSite cs, const Function *F);

    /// Handle indirect call
    void handleIndCall(CallSite cs);

    /// Handle external call
    //@{
    virtual void handleExtCall(CallSite cs, const Function *F);
    const Type *getBaseTypeAndFlattenedFields(Value* v, std::vector<LocationSet> &fields);
    void addComplexConsForExt(Value *D, Value *S,u32_t sz = 0);
    //@}

    /// Our visit overrides.
    //@{
    // Instructions that cannot be folded away.
    virtual void visitAllocaInst(AllocaInst &AI);
    void visitPHINode(PHINode &I);
    void visitStoreInst(StoreInst &I);
    void visitLoadInst(LoadInst &I);
    void visitGetElementPtrInst(GetElementPtrInst &I);
    void visitCallInst(CallInst &I) {
        visitCallSite(&I);
    }
    void visitInvokeInst(InvokeInst &II) {
        visitCallSite(&II);
    }
    void visitCallSite(CallSite cs);
    void visitReturnInst(ReturnInst &I);
    void visitCastInst(CastInst &I);
    void visitSelectInst(SelectInst &I);
    void visitExtractValueInst(ExtractValueInst  &EVI);
    void visitInsertValueInst(InsertValueInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }
    // TerminatorInst and UnwindInst have been removed since llvm-8.0.0
    // void visitTerminatorInst(TerminatorInst &TI) {}
    // void visitUnwindInst(UnwindInst &I) { /*returns void*/}

    void visitBinaryOperator(BinaryOperator &I);
    void visitCmpInst(CmpInst &I);

    /// TODO: do we need to care about these corner cases?
    void visitVAArgInst(VAArgInst &I) {
    }
    void visitExtractElementInst(ExtractElementInst &I);

    void visitInsertElementInst(InsertElementInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitShuffleVectorInst(ShuffleVectorInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitLandingPadInst(LandingPadInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Instruction not that often
    void visitResumeInst(ResumeInst &I) { /*returns void*/
    }
    void visitUnreachableInst(UnreachableInst &I) { /*returns void*/
    }
    void visitFenceInst(FenceInst &I) { /*returns void*/
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicRMWInst(AtomicRMWInst &I) {
		pag->addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Provide base case for our instruction visit.
    inline void visitInstruction(Instruction &I) {
        // If a new instruction is added to LLVM that we don't handle.
        // TODO: ignore here:
    }
    //}@
};

#endif /* PAGBUILDER_H_ */
