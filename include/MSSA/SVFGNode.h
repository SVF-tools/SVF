//===- SVFGNode.h -- SVFG node------------------------------------------------//
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
 * SVFGNode.h
 *
 *  Created on: Nov 11, 2013
 *      Author: rockysui
 */

#ifndef SVFGNODE_H_
#define SVFGNODE_H_

#include "MemoryModel/PointerAnalysis.h"
#include "MSSA/SVFGEdge.h"
#include "MemoryModel/GenericGraph.h"

/*!
 * Sparse Value Flow Graph Node, representing different kinds of variable definitions
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<SVFGNode,SVFGEdge> GenericSVFGNodeTy;
class SVFGNode : public GenericSVFGNodeTy {

public:
    /// five kinds of SVFG node
    /// Gep represents offset edge for field sensitivity
    enum SVFGNodeK {
        Addr, Copy, Gep, Store, Load, TPhi, TIntraPhi, TInterPhi,
        MPhi, MIntraPhi, MInterPhi, FRet, ARet,
        AParm, APIN, APOUT, FParm, FPIN, FPOUT, NPtr
    };

    typedef SVFGEdge::SVFGEdgeSetTy::iterator iterator;
    typedef SVFGEdge::SVFGEdgeSetTy::const_iterator const_iterator;
    typedef std::set<const CallPE*> CallPESet;
    typedef std::set<const RetPE*> RetPESet;

public:
    /// Constructor
    SVFGNode(NodeID i, SVFGNodeK k): GenericSVFGNodeTy(i,k), bb(NULL) {

    }
    /// We should know the program location (basic block level) of each SVFG node
    virtual const llvm::BasicBlock* getBB() const {
        return bb;
    }

    /// Overloading operator << for dumping SVFG node ID
    //@{
    friend llvm::raw_ostream& operator<< (llvm::raw_ostream &o, const SVFGNode &node) {
        o << "SVFGNode ID:" << node.getId();
        return o;
    }
    //@}
protected:
    const llvm::BasicBlock* bb;
};


/*!
 * SVFG node stands for a program statement
 */
class StmtSVFGNode : public SVFGNode {

private:
    const PAGEdge* pagEdge;

public:
    /// Constructor
    StmtSVFGNode(NodeID id, const PAGEdge* e, SVFGNodeK k): SVFGNode(id,k), pagEdge(e) {
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
    static inline bool classof(const StmtSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Addr ||
               node->getNodeKind() == Copy ||
               node->getNodeKind() == Gep ||
               node->getNodeKind() == Store ||
               node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
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
class ActualParmSVFGNode : public SVFGNode {
private:
    const PAGNode* param;
    llvm::CallSite cs;
public:
    /// Constructor
    ActualParmSVFGNode(NodeID id, const PAGNode* n, llvm::CallSite c):
        SVFGNode(id, AParm), param(n), cs(c) {
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
    static inline bool classof(const ActualParmSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == AParm;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == AParm;
    }
    //@}
};


/*
 * SVFG Node stands for formal parameter node (top level pointers)
 */
class FormalParmSVFGNode : public SVFGNode {
private:
    const PAGNode* param;
    const llvm::Function* fun;
    CallPESet callPEs;

public:
    /// Constructor
    FormalParmSVFGNode(NodeID id, const PAGNode* n, const llvm::Function* f):
        SVFGNode(id, FParm), param(n), fun(f) {
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
    static inline bool classof(const FormalParmSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == FParm;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == FParm;
    }
    //@}
};

/*!
 * Callsite receive paramter
 */
class ActualRetSVFGNode: public SVFGNode {
private:
    const PAGNode* rev;
    llvm::CallSite cs;

    ActualRetSVFGNode();                      ///< place holder
    ActualRetSVFGNode(const ActualRetSVFGNode &);  ///< place holder
    void operator=(const ActualRetSVFGNode &); ///< place holder

public:
    /// Constructor
    ActualRetSVFGNode(NodeID id, const PAGNode* n, llvm::CallSite c):
        SVFGNode(id,ARet), rev(n), cs (c) {
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
    static inline bool classof(const ActualRetSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == ARet;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == ARet;
    }
    //@}
};

/*!
 * Callee return SVFG node
 */
class FormalRetSVFGNode: public SVFGNode {
private:
    const PAGNode* ret;
    const llvm::Function* fun;
    RetPESet retPEs;

    FormalRetSVFGNode();                      ///< place holder
    FormalRetSVFGNode(const FormalRetSVFGNode &);  ///< place holder
    void operator=(const FormalRetSVFGNode &); ///< place holder

public:
    /// Constructor
    FormalRetSVFGNode(NodeID id, const PAGNode* n,const llvm::Function* f):
        SVFGNode(id,FRet), ret(n), fun(f) {
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
    static inline bool classof(const FormalRetSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == FRet;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == FRet;
    }
    //@}
};

/*!
 * Memory region SVFGNode (for address-taken objects)
 */
class MRSVFGNode : public SVFGNode {
protected:
    PointsTo cpts;

    // This constructor can only be used by derived classes
    MRSVFGNode(NodeID id, SVFGNodeK k) : SVFGNode(id, k) {}

public:
    /// Return points-to of the MR
    inline const PointsTo& getPointsTo() const {
        return cpts;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MRSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == FPIN ||
               node->getNodeKind() == FPOUT ||
               node->getNodeKind() == APIN ||
               node->getNodeKind() == APOUT ||
               node->getNodeKind() == MPhi ||
               node->getNodeKind() == MIntraPhi ||
               node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
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
class FormalINSVFGNode : public MRSVFGNode {
private:
    const MemSSA::ENTRYCHI* chi;

public:
    /// Constructor
    FormalINSVFGNode(NodeID id, const MemSSA::ENTRYCHI* entry): MRSVFGNode(id, FPIN), chi(entry) {
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
    static inline bool classof(const FormalINSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == FPIN;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == FPIN;
    }
    //@}
};


/*
 * SVFG Node stands for return mu node (address-taken variables)
 */
class FormalOUTSVFGNode : public MRSVFGNode {
private:
    const MemSSA::RETMU* mu;

public:
    /// Constructor
    FormalOUTSVFGNode(NodeID id, const MemSSA::RETMU* exit): MRSVFGNode(id, FPOUT), mu(exit) {
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
    static inline bool classof(const FormalOUTSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == FPOUT;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == FPOUT;
    }
    //@}
};


/*
 * SVFG Node stands for callsite mu node (address-taken variables)
 */
class ActualINSVFGNode : public MRSVFGNode {
private:
    const MemSSA::CALLMU* mu;
    llvm::CallSite cs;
public:
    /// Constructor
    ActualINSVFGNode(NodeID id, const MemSSA::CALLMU* m, llvm::CallSite c):
        MRSVFGNode(id, APIN), mu(m), cs(c) {
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
    static inline bool classof(const ActualINSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == APIN;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == APIN;
    }
    //@}
};


/*
 * SVFG Node stands for callsite chi node (address-taken variables)
 */
class ActualOUTSVFGNode : public MRSVFGNode {
private:
    const MemSSA::CALLCHI* chi;
    llvm::CallSite cs;

public:
    /// Constructor
    ActualOUTSVFGNode(NodeID id, const MemSSA::CALLCHI* c, llvm::CallSite cal):
        MRSVFGNode(id, APOUT), chi(c), cs(cal) {
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
    static inline bool classof(const ActualOUTSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == APOUT;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == APOUT;
    }
    //@}
};

/*
 * SVFG Node stands for a top level pointer ssa phi node or a formal parameter or a return parameter
 */
class PHISVFGNode : public SVFGNode {

public:
    typedef llvm::DenseMap<u32_t,const PAGNode*> OPVers;
protected:
    const PAGNode* res;
    OPVers opVers;

public:
    /// Constructor
    PHISVFGNode(NodeID id, const PAGNode* r,SVFGNodeK k = TPhi): SVFGNode(id, k), res(r) {
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
    static inline bool classof(const PHISVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    //@}
};


/*
 * Intra LLVM PHI Node
 */
class IntraPHISVFGNode : public PHISVFGNode {

public:
    typedef llvm::DenseMap<u32_t,const llvm::BasicBlock*> OPIncomingBBs;

private:
    OPIncomingBBs opIncomingBBs;
public:
    /// Constructor
    IntraPHISVFGNode(NodeID id, const PAGNode* r): PHISVFGNode(id, r, TIntraPhi) {
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
    static inline bool classof(const IntraPHISVFGNode *node) {
        return true;
    }
    static inline bool classof(const PHISVFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    //@}
};


/*
 * Inter LLVM PHI node (formal parameter)
 */
class InterPHISVFGNode : public PHISVFGNode {

public:
    /// Constructor interPHI for formal parameter
    InterPHISVFGNode(NodeID id, const FormalParmSVFGNode* fp) : PHISVFGNode(id, fp->getParam(), TInterPhi),fun(fp->getFun()),callInst(NULL) {}
    /// Constructor interPHI for actual return
    InterPHISVFGNode(NodeID id, const ActualRetSVFGNode* ar) : PHISVFGNode(id, ar->getRev(), TInterPhi), fun(NULL),callInst(ar->getCallSite().getInstruction()) {}

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
    static inline bool classof(const InterPHISVFGNode *node) {
        return true;
    }
    static inline bool classof(const PHISVFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
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
class MSSAPHISVFGNode : public MRSVFGNode {
public:
    typedef llvm::DenseMap<u32_t,const MRVer*> OPVers;

protected:
    const MemSSA::MDEF* res;
    OPVers opVers;

public:
    /// Constructor
    MSSAPHISVFGNode(NodeID id, const MemSSA::MDEF* def,SVFGNodeK k = MPhi): MRSVFGNode(id, k), res(def) {
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
    static inline bool classof(const MSSAPHISVFGNode *) {
        return true;
    }
    static inline bool classof(const MRSVFGNode *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const SVFGNode *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return (node->getNodeKind() == MPhi || node->getNodeKind() == MIntraPhi || node->getNodeKind() == MInterPhi);
    }
    //@}
};

/*
 * Intra MSSA PHI
 */
class IntraMSSAPHISVFGNode : public MSSAPHISVFGNode {

public:
    /// Constructor
    IntraMSSAPHISVFGNode(NodeID id, const MemSSA::PHI* phi): MSSAPHISVFGNode(id, phi, MIntraPhi) {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraMSSAPHISVFGNode *) {
        return true;
    }
    static inline bool classof(const MSSAPHISVFGNode * node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const MRSVFGNode *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == MIntraPhi;
    }
    //@}
};


/*
 * Inter MSSA PHI (formalIN/ActualOUT)
 */
class InterMSSAPHISVFGNode : public MSSAPHISVFGNode {

public:
    /// Constructor interPHI for formal parameter
    InterMSSAPHISVFGNode(NodeID id, const FormalINSVFGNode* fi) : MSSAPHISVFGNode(id, fi->getEntryChi(), MInterPhi),fun(fi->getFun()),callInst(NULL) {}
    /// Constructor interPHI for actual return
    InterMSSAPHISVFGNode(NodeID id, const ActualOUTSVFGNode* ao) : MSSAPHISVFGNode(id, ao->getCallCHI(), MInterPhi), fun(NULL),callInst(ao->getCallSite().getInstruction()) {}

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
    static inline bool classof(const InterMSSAPHISVFGNode *) {
        return true;
    }
    static inline bool classof(const MSSAPHISVFGNode * node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const MRSVFGNode *node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == MInterPhi;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
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
class NullPtrSVFGNode : public SVFGNode {
private:
    const PAGNode* node;
public:
    /// Constructor
    NullPtrSVFGNode(NodeID id, const PAGNode* n) : SVFGNode(id,NPtr), node(n) {

    }
    const PAGNode* getPAGNode() const {
        return node;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NullPtrSVFGNode *) {
        return true;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == NPtr;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == NPtr;
    }
    //@}
};

class AddrSVFGNode: public StmtSVFGNode {
private:
    AddrSVFGNode();                      ///< place holder
    AddrSVFGNode(const AddrSVFGNode &);  ///< place holder
    void operator=(const AddrSVFGNode &); ///< place holder

public:
    /// Constructor
    AddrSVFGNode(NodeID id, const AddrPE* edge): StmtSVFGNode(id, edge,Addr) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrSVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtSVFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == Addr;
    }
    //@}
};

/*!
 * SVFGNode for loads
 */
class LoadSVFGNode: public StmtSVFGNode {
private:
    LoadSVFGNode();                      ///< place holder
    LoadSVFGNode(const LoadSVFGNode &);  ///< place holder
    void operator=(const LoadSVFGNode &); ///< place holder

public:
    /// Constructor
    LoadSVFGNode(NodeID id, const LoadPE* edge): StmtSVFGNode(id, edge,Load) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadSVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtSVFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == Load;
    }
    //@}
};

/*!
 * SVFGNode for stores
 */
class StoreSVFGNode: public StmtSVFGNode {
private:
    StoreSVFGNode();                      ///< place holder
    StoreSVFGNode(const StoreSVFGNode &);  ///< place holder
    void operator=(const StoreSVFGNode &); ///< place holder

public:
    /// Constructor
    StoreSVFGNode(NodeID id,const StorePE* edge): StmtSVFGNode(id,edge,Store) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreSVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtSVFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == Store;
    }
    //@}
};

/*!
 * SVFGNode for copies
 */
class CopySVFGNode: public StmtSVFGNode {
private:
    CopySVFGNode();                      ///< place holder
    CopySVFGNode(const CopySVFGNode &);  ///< place holder
    void operator=(const CopySVFGNode &); ///< place holder

public:
    /// Constructor
    CopySVFGNode(NodeID id,const CopyPE* copy): StmtSVFGNode(id,copy,Copy) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopySVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtSVFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == Copy;
    }
    //@}
};

/*!
 * SVFGNode for Gep
 */
class GepSVFGNode: public StmtSVFGNode {
private:
    GepSVFGNode();                      ///< place holder
    GepSVFGNode(const GepSVFGNode &);  ///< place holder
    void operator=(const GepSVFGNode &); ///< place holder

public:
    /// Constructor
    GepSVFGNode(NodeID id,const GepPE* edge): StmtSVFGNode(id,edge,Gep) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepSVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtSVFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const SVFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const GenericSVFGNodeTy *node) {
        return node->getNodeKind() == Gep;
    }
    //@}
};




#endif /* SVFGNODE_H_ */
