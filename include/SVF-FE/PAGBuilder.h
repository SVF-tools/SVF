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

#include "Graphs/PAG.h"
#include "Util/ExtAPI.h"
#include "SVF-FE/ICFGBuilder.h"


class SVFModule;
/*!
 *  PAG Builder
 */
class PAGBuilder: public llvm::InstVisitor<PAGBuilder> {

private:
    PAG* pag;
    SVFModule* svfMod;
    const BasicBlock* curBB;	///< Current basic block during PAG construction when visiting the module
    const Value* curVal;	///< Current Value during PAG construction when visiting the module

public:
    /// Constructor
    PAGBuilder(): pag(PAG::getPAG()), curBB(NULL),curVal(NULL){
    }
    /// Destructor
    virtual ~PAGBuilder() {
    }

    /// Start building PAG here
    PAG* build(SVFModule* svfModule);

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
    void visitGlobal(SVFModule* svfModule);
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
		addBlackHoleAddrEdge(getValueNode(&I));
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
		addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitShuffleVectorInst(ShuffleVectorInst &I) {
		addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitLandingPadInst(LandingPadInst &I) {
		addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Instruction not that often
    void visitResumeInst(ResumeInst &I) { /*returns void*/
    }
    void visitUnreachableInst(UnreachableInst &I) { /*returns void*/
    }
    void visitFenceInst(FenceInst &I) { /*returns void*/
		addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
		addBlackHoleAddrEdge(getValueNode(&I));
    }
    void visitAtomicRMWInst(AtomicRMWInst &I) {
		addBlackHoleAddrEdge(getValueNode(&I));
    }

    /// Provide base case for our instruction visit.
    inline void visitInstruction(Instruction &I) {
        // If a new instruction is added to LLVM that we don't handle.
        // TODO: ignore here:
    }
    //}@

    /// Set current basic block in order to keep track of control flow information
    inline void setCurrentLocation(const Value* val, const BasicBlock* bb) {
        curBB = bb;
        curVal = val;
    }
    inline const Value *getCurrentValue() const {
        return curVal;
    }
    inline const BasicBlock *getCurrentBB() const {
        return curBB;
    }

    /// Add global black hole Address edge
    void addGlobalBlackHoleAddrEdge(NodeID node, const ConstantExpr *int2Ptrce) {
        const Value* cval = getCurrentValue();
        const BasicBlock* cbb = getCurrentBB();
        setCurrentLocation(int2Ptrce,NULL);
        addBlackHoleAddrEdge(node);
        setCurrentLocation(cval,cbb);
    }

    /// Add NullPtr PAGNode
    inline NodeID addNullPtrNode() {
        NodeID nullPtr = pag->addDummyValNode(pag->getNullPtr());
        /// let all undef value or non-determined pointers points-to black hole
        LLVMContext &cxt = LLVMModuleSet::getLLVMModuleSet()->getContext();
        ConstantPointerNull *constNull = ConstantPointerNull::get(Type::getInt8PtrTy(cxt));
        setCurrentLocation(constNull, NULL);
        addBlackHoleAddrEdge(pag->getBlkPtr());
        return nullPtr;
    }

    NodeID getGepValNode(const Value* val, const LocationSet& ls, const Type *baseType, u32_t fieldidx);

    void setCurrentBBAndValueForPAGEdge(PAGEdge* edge);

    void connectGlobalToProgEntry();

    inline void addBlackHoleAddrEdge(NodeID node) {
    	if(PAGEdge* edge = pag->addBlackHoleAddrPE(node))
    		setCurrentBBAndValueForPAGEdge(edge);
    }

    /// Add Address edge
    inline void addAddrEdge(NodeID src, NodeID dst){
    	if(AddrPE* edge = pag->addAddrPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Copy edge
    inline void addCopyEdge(NodeID src, NodeID dst){
    	if(CopyPE* edge = pag->addCopyPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Copy edge
    inline void addCmpEdge(NodeID src, NodeID dst){
    	if(CmpPE* edge = pag->addCmpPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Copy edge
    inline void addBinaryOPEdge(NodeID src, NodeID dst){
    	if(BinaryOPPE* edge = pag->addBinaryOPPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Load edge
    inline void addLoadEdge(NodeID src, NodeID dst){
    	if(LoadPE* edge = pag->addLoadPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Store edge
    inline void addStoreEdge(NodeID src, NodeID dst){
    	IntraBlockNode* node;
    	if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(curVal))
    		node = pag->getICFG()->getIntraBlockNode(inst);
		else
			node = NULL;
    	if(StorePE* edge = pag->addStorePE(src,dst,node)){
    		setCurrentBBAndValueForPAGEdge(edge);
    	}
    }
    /// Add Call edge
    inline void addCallEdge(NodeID src, NodeID dst, const CallBlockNode* cs){
    	if(CallPE* edge = pag->addCallPE(src,dst,cs))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Return edge
    inline void addRetEdge(NodeID src, NodeID dst, const CallBlockNode* cs){
    	if(RetPE* edge = pag->addRetPE(src,dst,cs))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Gep edge
    inline void addGepEdge(NodeID src, NodeID dst, const LocationSet& ls, bool constGep){
    	if(GepPE* edge = pag->addGepPE(src,dst,ls, constGep))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Offset(Gep) edge
    void addNormalGepEdge(NodeID src, NodeID dst, const LocationSet& ls){
    	if(NormalGepPE* edge = pag->addNormalGepPE(src,dst,ls))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Variant(Gep) edge
    inline void addVariantGepEdge(NodeID src, NodeID dst){
    	if(VariantGepPE* edge = pag->addVariantGepPE(src,dst))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Thread fork edge for parameter passing
    inline void addThreadForkEdge(NodeID src, NodeID dst, const CallBlockNode* cs){
    	if(TDForkPE* edge = pag->addThreadForkPE(src,dst,cs))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    /// Add Thread join edge for parameter passing
    inline void addThreadJoinEdge(NodeID src, NodeID dst, const CallBlockNode* cs){
    	if(TDJoinPE* edge = pag->addThreadJoinPE(src,dst,cs))
    		setCurrentBBAndValueForPAGEdge(edge);
    }
    //@}

};

#endif /* PAGBUILDER_H_ */
