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

#include <llvm/IR/InstVisitor.h>	// for instruction visitor


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
                 Size_t offset = 0, llvm::Instruction* cs = NULL);
    // @}

    /// Sanity check for PAG
    void sanityCheck();

    /// Get different kinds of node
    //@{
    // GetValNode - Return the value node according to a LLVM Value.
    NodeID getValueNode(const llvm::Value *V) {
        // first handle gep edge if val if a constant expression
        processCE(V);

        // strip off the constant cast and return the value node
        return pag->getValueNode(V);
    }

    /// GetObject - Return the object node (stack/global/heap/function) according to a LLVM Value
    inline NodeID getObjectNode(const llvm::Value *V) {
        return pag->getObjectNode(V);
    }

    /// getReturnNode - Return the node representing the unique return value of a function.
    inline NodeID getReturnNode(const llvm::Function *func) {
        return pag->getReturnNode(func);
    }

    /// getVarargNode - Return the node representing the unique variadic argument of a function.
    inline NodeID getVarargNode(const llvm::Function *func) {
        return pag->getVarargNode(func);
    }
    //@}

    /// Handle globals including (global variable and functions)
    //@{
    void visitGlobal(SVFModule svfModule);
    void InitialGlobal(const llvm::GlobalVariable *gvar, llvm::Constant *C,
                       u32_t offset, u32_t fieldidx);
    NodeID getGlobalVarField(const llvm::GlobalVariable *gvar, u32_t offset, u32_t fieldidx);
    //@}

    /// Process constant expression
    void processCE(const llvm::Value *val);

    /// Compute offset of a gep instruction or gep constant expression
    bool computeGepOffset(const llvm::User *V, LocationSet& ls);

    /// Handle direct call
    void handleDirectCall(llvm::CallSite cs, const llvm::Function *F);

    /// Handle indirect call
    void handleIndCall(llvm::CallSite cs);

    /// Handle external call
    //@{
    virtual void handleExtCall(llvm::CallSite cs, const llvm::Function *F);
    const llvm::Type *getBaseTypeAndFlattenedFields(llvm::Value* v, std::vector<LocationSet> &fields);
    void addComplexConsForExt(llvm::Value *D, llvm::Value *S,u32_t sz = 0);
    //@}

    /// Our visit overrides.
    //@{
    // Instructions that cannot be folded away.
    virtual void visitAllocaInst(llvm::AllocaInst &AI);
    void visitPHINode(llvm::PHINode &I);
    void visitStoreInst(llvm::StoreInst &I);
    void visitLoadInst(llvm::LoadInst &I);
    void visitGetElementPtrInst(llvm::GetElementPtrInst &I);
    void visitCallInst(llvm::CallInst &I) {
        visitCallSite(&I);
    }
    void visitInvokeInst(llvm::InvokeInst &II) {
        visitCallSite(&II);
        visitTerminatorInst(II);
    }
    void visitCallSite(llvm::CallSite cs);
    void visitReturnInst(llvm::ReturnInst &I);
    void visitCastInst(llvm::CastInst &I);
    void visitSelectInst(llvm::SelectInst &I);
    void visitIntToPtrInst(llvm::IntToPtrInst &inst);
    void visitExtractValueInst(llvm::ExtractValueInst &EVI);
    void visitInsertValueInst(llvm::InsertValueInst &IVI) {
    }
    // Terminators
    void visitTerminatorInst(llvm::TerminatorInst &TI) {
    }
    void visitBinaryOperator(llvm::BinaryOperator &I) {
    }
    void visitCmpInst(llvm::CmpInst &I) {
    }

    /// TODO: do we need to care about these corner cases?
    void visitPtrToIntInst(llvm::PtrToIntInst &inst) {
    }
    void visitVAArgInst(llvm::VAArgInst &I) {
    }
    void visitExtractElementInst(llvm::ExtractElementInst &I);

    void visitInsertElementInst(llvm::InsertElementInst &I) {
    }
    void visitShuffleVectorInst(llvm::ShuffleVectorInst &I) {
    }
    void visitLandingPadInst(llvm::LandingPadInst &I) {
    }

    /// Instruction not that often
    void visitResumeInst(llvm::TerminatorInst &I) { /*returns void*/
    }
    void visitUnwindInst(llvm::TerminatorInst &I) { /*returns void*/
    }
    void visitUnreachableInst(llvm::TerminatorInst &I) { /*returns void*/
    }
    void visitFenceInst(llvm::FenceInst &I) { /*returns void*/
    }
    void visitAtomicCmpXchgInst(llvm::AtomicCmpXchgInst &I) {
    }
    void visitAtomicRMWInst(llvm::AtomicRMWInst &I) {
    }

    /// Provide base case for our instruction visit.
    inline void visitInstruction(llvm::Instruction &I) {
        // If a new instruction is added to LLVM that we don't handle.
        // TODO: ignore here:
    }
    //}@
};

/*!
 * Build PAG from a user specified file (for debugging purpose)
 */
class PAGBuilderFromFile {

private:
    PAG* pag;
    std::string file;
public:
    /// Constructor
    PAGBuilderFromFile(std::string f) :
        pag(PAG::getPAG(true)), file(f) {
    }
    /// Destructor
    ~PAGBuilderFromFile() {
    }

    /// Return PAG
    PAG* getPAG() const {
        return pag;
    }

    /// Return file name
    std::string getFileName() const {
        return file;
    }

    /// Start building
    PAG* build();

    // Add edges
    void addEdge(NodeID nodeSrc, NodeID nodeDst, Size_t offset,
                 std::string edge);
};

#endif /* PAGBUILDER_H_ */
