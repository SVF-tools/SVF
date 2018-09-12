//===- ICFGNode.h -- ICFG node------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFGNode.h
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei
 */

#ifndef ICFGNODE_H_
#define ICFGNODE_H_

#include "MemoryModel/PointerAnalysis.h"
#include "MemoryModel/GenericGraph.h"
#include "Util/ICFGEdge.h"

class ICFGNode;
/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<ICFGNode,ICFGEdge> GenericICFGNodeTy;
class ICFGNode : public GenericICFGNodeTy {

public:
    /// five kinds of SVFG node
    /// Gep represents offset edge for field sensitivity
    enum ICFGNodeK {
        Addr, Copy, Gep, Store, Load, TPhi, TIntraPhi, TInterPhi,
        MPhi, MIntraPhi, MInterPhi, FRet, ARet,
        AParm, APIN, APOUT, FParm, FPIN, FPOUT, NPtr
    };

    typedef ICFGEdge::ICFGEdgeSetTy::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSetTy::const_iterator const_iterator;
    typedef std::set<const CallPE*> CallPESet;
    typedef std::set<const RetPE*> RetPESet;

public:
    /// Constructor
    ICFGNode(NodeID i, ICFGNodeK k): GenericICFGNodeTy(i,k), bb(NULL) {

    }
    /// We should know the program location (basic block level) of each SVFG node
    virtual const llvm::BasicBlock* getBB() const {
        return bb;
    }

    /// Overloading operator << for dumping SVFG node ID
    //@{
    friend llvm::raw_ostream& operator<< (llvm::raw_ostream &o, const ICFGNode &node) {
        o << "ICFGNode ID:" << node.getId();
        return o;
    }
    //@}
protected:
    const llvm::BasicBlock* bb;
};


/*!
 * SVFG node stands for a program statement
 */
class StmtICFGNode : public ICFGNode {

private:
    const PAGEdge* pagEdge;

public:
    /// Constructor
    StmtICFGNode(NodeID id, const PAGEdge* e, ICFGNodeK k): ICFGNode(id,k), pagEdge(e) {
        bb = e->getBB();
    }

    /// PAGNode and PAGEdge
    ///@{
    inline const PAGEdge* getPAGEdge() const {
        return pagEdge;
    }

    inline NodeID getPAGSrcNodeID() const {
        return pagEdge->getSrcID();
    }

    inline NodeID getPAGDstNodeID() const {
        return pagEdge->getDstID();
    }

    inline PAGNode* getPAGSrcNode() const {
        return pagEdge->getSrcNode();
    }

    inline PAGNode* getPAGDstNode() const {
        return pagEdge->getDstNode();
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StmtICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Addr ||
               node->getNodeKind() == Copy ||
               node->getNodeKind() == Gep ||
               node->getNodeKind() == Store ||
               node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Addr ||
               node->getNodeKind() == Copy ||
               node->getNodeKind() == Gep ||
               node->getNodeKind() == Store ||
               node->getNodeKind() == Load;
    }

    inline const llvm::Instruction* getInst() const {
        /// should return a valid instruction unless it is a global PAGEdge
        return pagEdge->getInst();
    }
    //@}
};

/*
 * SVFG Node stands for acutal parameter node (top level pointers)
 */
class ActualParmICFGNode : public ICFGNode {
private:
    const PAGNode* param;
    llvm::CallSite cs;
public:
    /// Constructor
    ActualParmICFGNode(NodeID id, const PAGNode* n, llvm::CallSite c):
        ICFGNode(id, AParm), param(n), cs(c) {
        bb = cs.getInstruction()->getParent();
    }

    /// Return callsite
    inline llvm::CallSite getCallSite() const {
        return cs;
    }

    /// Return parameter
    inline const PAGNode* getParam() const {
        return param;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualParmICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == AParm;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == AParm;
    }
    //@}
};


/*
 * SVFG Node stands for formal parameter node (top level pointers)
 */
class FormalParmICFGNode : public ICFGNode {
private:
    const PAGNode* param;
    const llvm::Function* fun;
    CallPESet callPEs;

public:
    /// Constructor
    FormalParmICFGNode(NodeID id, const PAGNode* n, const llvm::Function* f):
        ICFGNode(id, FParm), param(n), fun(f) {
        bb = &fun->getEntryBlock();
    }

    /// Return parameter
    inline const PAGNode* getParam() const {
        return param;
    }

    /// Return function
    inline const llvm::Function* getFun() const {
        return fun;
    }
    /// Return call edge
    inline void addCallPE(const CallPE* call) {
        callPEs.insert(call);
    }
    /// Call edge iterator
    ///@{
    inline CallPESet::const_iterator callPEBegin() const {
        return callPEs.begin();
    }
    inline CallPESet::const_iterator callPEEnd() const {
        return callPEs.end();
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalParmICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FParm;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FParm;
    }
    //@}
};

/*!
 * Callsite receive paramter
 */
class ActualRetICFGNode: public ICFGNode {
private:
    const PAGNode* rev;
    llvm::CallSite cs;

    ActualRetICFGNode();                      ///< place holder
    ActualRetICFGNode(const ActualRetICFGNode &);  ///< place holder
    void operator=(const ActualRetICFGNode &); ///< place holder

public:
    /// Constructor
    ActualRetICFGNode(NodeID id, const PAGNode* n, llvm::CallSite c):
        ICFGNode(id,ARet), rev(n), cs (c) {
        bb = cs.getInstruction()->getParent();
    }
    /// Return callsite
    inline llvm::CallSite getCallSite() const {
        return cs;
    }
    /// Receive parameter at callsite
    inline const PAGNode* getRev() const {
        return rev;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualRetICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == ARet;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == ARet;
    }
    //@}
};

/*!
 * Callee return SVFG node
 */
class FormalRetICFGNode: public ICFGNode {
private:
    const PAGNode* ret;
    const llvm::Function* fun;
    RetPESet retPEs;

    FormalRetICFGNode();                      ///< place holder
    FormalRetICFGNode(const FormalRetICFGNode &);  ///< place holder
    void operator=(const FormalRetICFGNode &); ///< place holder

public:
    /// Constructor
    FormalRetICFGNode(NodeID id, const PAGNode* n,const llvm::Function* f):
        ICFGNode(id,FRet), ret(n), fun(f) {
        bb = analysisUtil::getFunExitBB(fun);
    }
    /// Return value at callee
    inline const PAGNode* getRet() const {
        return ret;
    }
    /// Function
    inline const llvm::Function* getFun() const {
        return fun;
    }
    /// RetPE
    inline void addRetPE(const RetPE* retPE) {
        retPEs.insert(retPE);
    }
    /// RetPE iterators
    inline RetPESet::const_iterator retPEBegin() const {
        return retPEs.begin();
    }
    inline RetPESet::const_iterator retPEEnd() const {
        return retPEs.end();
    }
    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalRetICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FRet;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FRet;
    }
    //@}
};

/*!
 * Memory region ICFGNode (for address-taken objects)
 */
class MRICFGNode : public ICFGNode {
protected:
    PointsTo cpts;

    // This constructor can only be used by derived classes
    MRICFGNode(NodeID id, ICFGNodeK k) : ICFGNode(id, k) {}

public:
    /// Return points-to of the MR
    inline const PointsTo& getPointsTo() const {
        return cpts;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MRICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FPIN ||
               node->getNodeKind() == FPOUT ||
               node->getNodeKind() == APIN ||
               node->getNodeKind() == APOUT ||
               node->getNodeKind() == MPhi ||
               node->getNodeKind() == MIntraPhi ||
               node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FPIN ||
               node->getNodeKind() == FPOUT ||
               node->getNodeKind() == APIN ||
               node->getNodeKind() == APOUT ||
               node->getNodeKind() == MPhi ||
               node->getNodeKind() == MIntraPhi ||
               node->getNodeKind() == MInterPhi;
    }
    //@}
};

/*
 * SVFG Node stands for entry chi node (address-taken variables)
 */
class FormalINICFGNode : public MRICFGNode {
private:
    const MemSSA::ENTRYCHI* chi;

public:
    /// Constructor
    FormalINICFGNode(NodeID id, const MemSSA::ENTRYCHI* entry): MRICFGNode(id, FPIN), chi(entry) {
        cpts = entry->getMR()->getPointsTo();
        bb = &entry->getFunction()->getEntryBlock();
    }
    /// EntryCHI
    inline const MemSSA::ENTRYCHI* getEntryChi() const {
        return chi;
    }
    /// Return function
    inline const llvm::Function* getFun() const {
        return bb->getParent();
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalINICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FPIN;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FPIN;
    }
    //@}
};


/*
 * SVFG Node stands for return mu node (address-taken variables)
 */
class FormalOUTICFGNode : public MRICFGNode {
private:
    const MemSSA::RETMU* mu;

public:
    /// Constructor
    FormalOUTICFGNode(NodeID id, const MemSSA::RETMU* exit): MRICFGNode(id, FPOUT), mu(exit) {
        cpts = exit->getMR()->getPointsTo();
        bb = analysisUtil::getFunExitBB(exit->getFunction());
    }
    /// RetMU
    inline const MemSSA::RETMU* getRetMU() const {
        return mu;
    }
    /// Function
    inline const llvm::Function* getFun() const {
        return bb->getParent();
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FormalOUTICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FPOUT;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FPOUT;
    }
    //@}
};


/*
 * SVFG Node stands for callsite mu node (address-taken variables)
 */
class ActualINICFGNode : public MRICFGNode {
private:
    const MemSSA::CALLMU* mu;
    llvm::CallSite cs;
public:
    /// Constructor
    ActualINICFGNode(NodeID id, const MemSSA::CALLMU* m, llvm::CallSite c):
        MRICFGNode(id, APIN), mu(m), cs(c) {
        cpts = m->getMR()->getPointsTo();
        bb = cs.getInstruction()->getParent();
    }
    /// Callsite
    inline llvm::CallSite getCallSite() const {
        return cs;
    }
    /// CallMU
    inline const MemSSA::CALLMU* getCallMU() const {
        return mu;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualINICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == APIN;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == APIN;
    }
    //@}
};


/*
 * SVFG Node stands for callsite chi node (address-taken variables)
 */
class ActualOUTICFGNode : public MRICFGNode {
private:
    const MemSSA::CALLCHI* chi;
    llvm::CallSite cs;

public:
    /// Constructor
    ActualOUTICFGNode(NodeID id, const MemSSA::CALLCHI* c, llvm::CallSite cal):
        MRICFGNode(id, APOUT), chi(c), cs(cal) {
        cpts = c->getMR()->getPointsTo();
        bb = cs.getInstruction()->getParent();
    }
    /// Callsite
    inline llvm::CallSite getCallSite() const {
        return cs;
    }
    /// CallCHI
    inline const MemSSA::CALLCHI* getCallCHI() const {
        return chi;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualOUTICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == APOUT;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == APOUT;
    }
    //@}
};

/*
 * SVFG Node stands for a top level pointer ssa phi node or a formal parameter or a return parameter
 */
class PHIICFGNode : public ICFGNode {

public:
    typedef llvm::DenseMap<u32_t,const PAGNode*> OPVers;
protected:
    const PAGNode* res;
    OPVers opVers;

public:
    /// Constructor
    PHIICFGNode(NodeID id, const PAGNode* r,ICFGNodeK k = TPhi): ICFGNode(id, k), res(r) {
        const llvm::Value* val = r->getValue();
        if(const llvm::Function* fun =  llvm::dyn_cast<llvm::Function>(val)) {
            assert(llvm::isa<VarArgPN>(r) && "not a varag function?");
            bb = &fun->getEntryBlock();
        }
        /// the value can be an instruction phi, or a formal argument at function entry (due to SVFGOPT)
        else if(const llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(val)) {
            bb = inst->getParent();
        }
        else {
            assert((llvm::isa<llvm::Argument>(val) || analysisUtil::isSelectConstantExpr(val))
                   && "Phi svf node is not an instruction, a select constantExpr or an formal parameter??");
            if(const llvm::Argument* arg = llvm::dyn_cast<llvm::Argument>(val))
                bb = &arg->getParent()->getEntryBlock();
            else
                bb = NULL;	/// bb is null when we have a select constant expression
        }
    }

    /// Operands at a llvm PHINode
    //@{
    inline const PAGNode* getOpVer(u32_t pos) const {
        OPVers::const_iterator it = opVers.find(pos);
        assert(it!=opVers.end() && "version is NULL, did not rename?");
        return it->second;
    }
    inline void setOpVer(u32_t pos, const PAGNode* node) {
        opVers[pos] = node;
    }
    inline const PAGNode* getRes() const {
        return res;
    }
    inline u32_t getOpVerNum() const {
        return opVers.size();
    }
    inline OPVers::const_iterator opVerBegin() const {
        return opVers.begin();
    }
    inline OPVers::const_iterator opVerEnd() const {
        return opVers.end();
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const PHIICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    //@}
};


/*
 * Intra LLVM PHI Node
 */
class IntraPHIICFGNode : public PHIICFGNode {

public:
    typedef llvm::DenseMap<u32_t,const llvm::BasicBlock*> OPIncomingBBs;

private:
    OPIncomingBBs opIncomingBBs;
public:
    /// Constructor
    IntraPHIICFGNode(NodeID id, const PAGNode* r): PHIICFGNode(id, r, TIntraPhi) {
    }

    inline const llvm::BasicBlock* getOpIncomingBB(u32_t pos) const {
        OPIncomingBBs::const_iterator it = opIncomingBBs.find(pos);
        assert(it!=opIncomingBBs.end() && "version is NULL, did not rename?");
        return it->second;
    }
    inline void setOpVerAndBB(u32_t pos, const PAGNode* node, const llvm::BasicBlock* bb) {
        opVers[pos] = node;
        opIncomingBBs[pos] = bb;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraPHIICFGNode *node) {
        return true;
    }
    static inline bool classof(const PHIICFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    //@}
};


/*
 * Inter LLVM PHI node (formal parameter)
 */
class InterPHIICFGNode : public PHIICFGNode {

public:
    /// Constructor interPHI for formal parameter
    InterPHIICFGNode(NodeID id, const FormalParmICFGNode* fp) : PHIICFGNode(id, fp->getParam(), TInterPhi),fun(fp->getFun()),callInst(NULL) {}
    /// Constructor interPHI for actual return
    InterPHIICFGNode(NodeID id, const ActualRetICFGNode* ar) : PHIICFGNode(id, ar->getRev(), TInterPhi), fun(NULL),callInst(ar->getCallSite().getInstruction()) {}

    inline bool isFormalParmPHI() const {
        return (fun!=NULL) && (callInst == NULL);
    }

    inline bool isActualRetPHI() const {
        return (fun==NULL) && (callInst != NULL);
    }

    inline const llvm::Function* getFun() const {
        assert(isFormalParmPHI() && "expect a formal parameter phi");
        return fun;
    }

    inline llvm::CallSite getCallSite() const {
        assert(isActualRetPHI() && "expect a actual return phi");
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(callInst);
        return cs;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterPHIICFGNode *node) {
        return true;
    }
    static inline bool classof(const PHIICFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == TInterPhi;
    }
    //@}

private:
    const llvm::Function* fun;
    llvm::Instruction* callInst;
};

/*
 * SVFG Node stands for a memory ssa phi node or formalIn or ActualOut
 */
class MSSAPHIICFGNode : public MRICFGNode {
public:
    typedef llvm::DenseMap<u32_t,const MRVer*> OPVers;

protected:
    const MemSSA::MDEF* res;
    OPVers opVers;

public:
    /// Constructor
    MSSAPHIICFGNode(NodeID id, const MemSSA::MDEF* def,ICFGNodeK k = MPhi): MRICFGNode(id, k), res(def) {
        cpts = def->getMR()->getPointsTo();
        if(const MemSSA::PHI* phi = llvm::dyn_cast<const MemSSA::PHI>(def))
            bb = phi->getBasicBlock();
        else if(const MemSSA::ENTRYCHI* enChi = llvm::dyn_cast<const MemSSA::ENTRYCHI>(def))
            bb = &enChi->getFunction()->getEntryBlock();
        else if(const MemSSA::CALLCHI* calChi = llvm::dyn_cast<const MemSSA::CALLCHI>(def))
            bb = calChi->getBasicBlock();
        else
            assert("what else def for MSSAPHI node?");
    }
    /// MSSA phi operands
    //@{
    inline const MRVer* getOpVer(u32_t pos) const {
        OPVers::const_iterator it = opVers.find(pos);
        assert(it!=opVers.end() && "version is NULL, did not rename?");
        return it->second;
    }
    inline void setOpVer(u32_t pos, const MRVer* node) {
        opVers[pos] = node;
    }
    inline const MemSSA::MDEF* getRes() const {
        return res;
    }
    inline u32_t getOpVerNum() const {
        return opVers.size();
    }
    inline OPVers::const_iterator opVerBegin() const {
        return opVers.begin();
    }
    inline OPVers::const_iterator opVerEnd() const {
        return opVers.end();
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MSSAPHIICFGNode *) {
        return true;
    }
    static inline bool classof(const MRICFGNode *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const ICFGNode *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    //@}
};

/*
 * Intra MSSA PHI
 */
class IntraMSSAPHIICFGNode : public MSSAPHIICFGNode {

public:
    /// Constructor
    IntraMSSAPHIICFGNode(NodeID id, const MemSSA::PHI* phi): MSSAPHIICFGNode(id, phi, MIntraPhi) {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraMSSAPHIICFGNode *) {
        return true;
    }
    static inline bool classof(const MSSAPHIICFGNode * node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const MRICFGNode *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    //@}
};


/*
 * Inter MSSA PHI (formalIN/ActualOUT)
 */
class InterMSSAPHIICFGNode : public MSSAPHIICFGNode {

public:
    /// Constructor interPHI for formal parameter
    InterMSSAPHIICFGNode(NodeID id, const FormalINICFGNode* fi) : MSSAPHIICFGNode(id, fi->getEntryChi(), MInterPhi),fun(fi->getFun()),callInst(NULL) {}
    /// Constructor interPHI for actual return
    InterMSSAPHIICFGNode(NodeID id, const ActualOUTICFGNode* ao) : MSSAPHIICFGNode(id, ao->getCallCHI(), MInterPhi), fun(NULL),callInst(ao->getCallSite().getInstruction()) {}

    inline bool isFormalINPHI() const {
        return (fun!=NULL) && (callInst == NULL);
    }

    inline bool isActualOUTPHI() const {
        return (fun==NULL) && (callInst != NULL);
    }

    inline const llvm::Function* getFun() const {
        assert(isFormalINPHI() && "expect a formal parameter phi");
        return fun;
    }

    inline llvm::CallSite getCallSite() const {
        assert(isActualOUTPHI() && "expect a actual return phi");
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(callInst);
        return cs;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterMSSAPHIICFGNode *) {
        return true;
    }
    static inline bool classof(const MSSAPHIICFGNode * node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const MRICFGNode *node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == MInterPhi;
    }
    //@}
private:
    const llvm::Function* fun;
    llvm::Instruction* callInst;
};


/*!
 * Dummy Definition for undef and null pointers
 */
class NullPtrICFGNode : public ICFGNode {
private:
    const PAGNode* node;
public:
    /// Constructor
    NullPtrICFGNode(NodeID id, const PAGNode* n) : ICFGNode(id,NPtr), node(n) {

    }
    const PAGNode* getPAGNode() const {
        return node;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NullPtrICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == NPtr;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == NPtr;
    }
    //@}
};

class AddrICFGNode: public StmtICFGNode {
private:
    AddrICFGNode();                      ///< place holder
    AddrICFGNode(const AddrICFGNode &);  ///< place holder
    void operator=(const AddrICFGNode &); ///< place holder

public:
    /// Constructor
    AddrICFGNode(NodeID id, const AddrPE* edge): StmtICFGNode(id, edge,Addr) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrICFGNode *) {
        return true;
    }
    static inline bool classof(const StmtICFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Addr;
    }
    //@}
};

/*!
 * ICFGNode for loads
 */
class LoadICFGNode: public StmtICFGNode {
private:
    LoadICFGNode();                      ///< place holder
    LoadICFGNode(const LoadICFGNode &);  ///< place holder
    void operator=(const LoadICFGNode &); ///< place holder

public:
    /// Constructor
    LoadICFGNode(NodeID id, const LoadPE* edge): StmtICFGNode(id, edge,Load) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadICFGNode *) {
        return true;
    }
    static inline bool classof(const StmtICFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Load;
    }
    //@}
};

/*!
 * ICFGNode for stores
 */
class StoreICFGNode: public StmtICFGNode {
private:
    StoreICFGNode();                      ///< place holder
    StoreICFGNode(const StoreICFGNode &);  ///< place holder
    void operator=(const StoreICFGNode &); ///< place holder

public:
    /// Constructor
    StoreICFGNode(NodeID id,const StorePE* edge): StmtICFGNode(id,edge,Store) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreICFGNode *) {
        return true;
    }
    static inline bool classof(const StmtICFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Store;
    }
    //@}
};

/*!
 * ICFGNode for copies
 */
class CopyICFGNode: public StmtICFGNode {
private:
    CopyICFGNode();                      ///< place holder
    CopyICFGNode(const CopyICFGNode &);  ///< place holder
    void operator=(const CopyICFGNode &); ///< place holder

public:
    /// Constructor
    CopyICFGNode(NodeID id,const CopyPE* copy): StmtICFGNode(id,copy,Copy) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyICFGNode *) {
        return true;
    }
    static inline bool classof(const StmtICFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Copy;
    }
    //@}
};

/*!
 * ICFGNode for Gep
 */
class GepICFGNode: public StmtICFGNode {
private:
    GepICFGNode();                      ///< place holder
    GepICFGNode(const GepICFGNode &);  ///< place holder
    void operator=(const GepICFGNode &); ///< place holder

public:
    /// Constructor
    GepICFGNode(NodeID id,const GepPE* edge): StmtICFGNode(id,edge,Gep) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepICFGNode *) {
        return true;
    }
    static inline bool classof(const StmtICFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == Gep;
    }
    //@}
};




#endif /* ICFGNode_H_ */
