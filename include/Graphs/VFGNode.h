//===- VFGNode.h ----------------------------------------------------------------//
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
 * VFGNode.h
 *
 *  Created on: 18 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_UTIL_VFGNODE_H_
#define INCLUDE_UTIL_VFGNODE_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/GenericGraph.h"
#include "Graphs/VFGEdge.h"

class VFGNode;

/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<VFGNode,VFGEdge> GenericVFGNodeTy;
class VFGNode : public GenericVFGNodeTy {

public:
    /// 20 kinds of ICFG node
    /// Gep represents offset edge for field sensitivity
    enum VFGNodeK {
        Addr, Copy, Gep, Store, Load, Cmp, BinaryOp, TPhi, TIntraPhi, TInterPhi,
        MPhi, MIntraPhi, MInterPhi, FRet, ARet, AParm, FParm,
		FunRet, APIN, APOUT, FPIN, FPOUT, NPtr
    };

    typedef VFGEdge::VFGEdgeSetTy::iterator iterator;
    typedef VFGEdge::VFGEdgeSetTy::const_iterator const_iterator;
    typedef std::set<const CallPE*> CallPESet;
    typedef std::set<const RetPE*> RetPESet;

public:
    /// Constructor
    VFGNode(NodeID i, VFGNodeK k): GenericVFGNodeTy(i,k), bb(NULL) {

    }
    /// We should know the program location (basic block level) of each ICFG node
    virtual const BasicBlock* getBB() const {
        return bb;
    }

    /// Get the function of this SVFGNode
    virtual const Function* getFun() const {
        if(bb)
            return bb->getParent();
        else
            return NULL;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const VFGNode &node) {
        o << "VFGNode ID:" << node.getId();
        return o;
    }
    //@}
protected:
    const BasicBlock* bb;
};

/*!
 * ICFG node stands for a program statement
 */
class StmtVFGNode : public VFGNode {

private:
    const PAGEdge* pagEdge;

public:
    /// Constructor
    StmtVFGNode(NodeID id, const PAGEdge* e, VFGNodeK k): VFGNode(id,k), pagEdge(e) {
        bb = e->getBB();
    }

    /// Whether this node is used for pointer analysis. Both src and dst PAGNodes are of ptr type.
    inline bool isPTANode() const{
		return pagEdge->isPTAEdge();
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
    static inline bool classof(const StmtVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
		return node->getNodeKind() == Addr
				|| node->getNodeKind() == Copy
				|| node->getNodeKind() == Gep
				|| node->getNodeKind() == Store
				|| node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
		return node->getNodeKind() == Addr
				|| node->getNodeKind() == Copy
				|| node->getNodeKind() == Gep
				|| node->getNodeKind() == Store
				|| node->getNodeKind() == Load;
    }

    inline const Instruction* getInst() const {
        /// should return a valid instruction unless it is a global PAGEdge
        return pagEdge->getInst();
    }
    //@}
};

/*!
 * VFGNode for loads
 */
class LoadVFGNode: public StmtVFGNode {
private:
    LoadVFGNode();                      ///< place holder
    LoadVFGNode(const LoadVFGNode &);  ///< place holder
    void operator=(const LoadVFGNode &); ///< place holder

public:
    /// Constructor
    LoadVFGNode(NodeID id, const LoadPE* edge): StmtVFGNode(id, edge,Load) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtVFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Load;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Load;
    }
    //@}
};

/*!
 * VFGNode for stores
 */
class StoreVFGNode: public StmtVFGNode {
private:
    StoreVFGNode();                      ///< place holder
    StoreVFGNode(const StoreVFGNode &);  ///< place holder
    void operator=(const StoreVFGNode &); ///< place holder

public:
    /// Constructor
    StoreVFGNode(NodeID id,const StorePE* edge): StmtVFGNode(id,edge,Store) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtVFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Store;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Store;
    }
    //@}
};

/*!
 * VFGNode for copies
 */
class CopyVFGNode: public StmtVFGNode {
private:
    CopyVFGNode();                      ///< place holder
    CopyVFGNode(const CopyVFGNode &);  ///< place holder
    void operator=(const CopyVFGNode &); ///< place holder

public:
    /// Constructor
    CopyVFGNode(NodeID id,const CopyPE* copy): StmtVFGNode(id,copy,Copy) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtVFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Copy;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Copy;
    }
    //@}
};


/*!
 * VFGNode for compare instruction, e.g., bool b = (a!=c);
 */

class CmpVFGNode: public VFGNode {
public:
    typedef std::map<u32_t,const PAGNode*> OPVers;
protected:
    const PAGNode* res;
    OPVers opVers;

private:
    CmpVFGNode();                      ///< place holder
    CmpVFGNode(const CmpVFGNode &);  ///< place holder
    void operator=(const CmpVFGNode &); ///< place holder

public:
    /// Constructor
    CmpVFGNode(NodeID id,const PAGNode* r): VFGNode(id,Cmp), res(r) {
        const CmpInst* cmp = SVFUtil::dyn_cast<CmpInst>(r->getValue());
        assert(cmp && "not a binary operator?");
        bb = cmp->getParent();
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CmpVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Cmp;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Cmp;
    }
    //@}
    /// Operands at a BinaryNode
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
};


/*!
 * VFGNode for binary operator instructions, e.g., a = b + c;
 */
class BinaryOPVFGNode: public VFGNode {
public:
    typedef std::map<u32_t,const PAGNode*> OPVers;
protected:
    const PAGNode* res;
    OPVers opVers;

private:
	BinaryOPVFGNode();                      ///< place holder
	BinaryOPVFGNode(const BinaryOPVFGNode &);  ///< place holder
    void operator=(const BinaryOPVFGNode &); ///< place holder

public:
    /// Constructor
    BinaryOPVFGNode(NodeID id,const PAGNode* r): VFGNode(id,BinaryOp), res(r) {
        const BinaryOperator* binary = SVFUtil::dyn_cast<BinaryOperator>(r->getValue());
        assert(binary && "not a binary operator?");
        bb = binary->getParent();
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BinaryOPVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == BinaryOp;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == BinaryOp;
    }
    //@}
    /// Operands at a BinaryNode
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
};

/*!
 * VFGNode for Gep
 */
class GepVFGNode: public StmtVFGNode {
private:
    GepVFGNode();                      ///< place holder
    GepVFGNode(const GepVFGNode &);  ///< place holder
    void operator=(const GepVFGNode &); ///< place holder

public:
    /// Constructor
    GepVFGNode(NodeID id,const GepPE* edge): StmtVFGNode(id,edge,Gep) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtVFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Gep;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Gep;
    }
    //@}
};

/*
 * ICFG Node stands for a top level pointer ssa phi node or a formal parameter or a return parameter
 */
class PHIVFGNode : public VFGNode {

public:
    typedef std::map<u32_t,const PAGNode*> OPVers;
protected:
    const PAGNode* res;
    OPVers opVers;

public:
    /// Constructor
    PHIVFGNode(NodeID id, const PAGNode* r,VFGNodeK k = TPhi);

    /// Whether this phi node is of pointer type (used for pointer analysis).
    inline bool isPTANode() const{
		return res->isPointer();
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
    static inline bool classof(const PHIVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return (node->getNodeKind() == TPhi || node->getNodeKind() == TIntraPhi || node->getNodeKind() == TInterPhi);
    }
    //@}
};


/*
 * Intra LLVM PHI Node
 */
class IntraPHIVFGNode : public PHIVFGNode {

public:
    typedef llvm::DenseMap<u32_t,const ICFGNode*> OPIncomingBBs;

private:
    OPIncomingBBs opIncomingBBs;
public:
    /// Constructor
    IntraPHIVFGNode(NodeID id, const PAGNode* r): PHIVFGNode(id, r, TIntraPhi) {
    }

    inline const ICFGNode* getOpIncomingBB(u32_t pos) const {
        OPIncomingBBs::const_iterator it = opIncomingBBs.find(pos);
        assert(it!=opIncomingBBs.end() && "version is NULL, did not rename?");
        return it->second;
    }
    inline void setOpVerAndBB(u32_t pos, const PAGNode* node, const ICFGNode* bb) {
        opVers[pos] = node;
        opIncomingBBs[pos] = bb;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraPHIVFGNode *node) {
        return true;
    }
    static inline bool classof(const PHIVFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == TIntraPhi;
    }
    //@}
};


class AddrVFGNode: public StmtVFGNode {
private:
    AddrVFGNode();                      ///< place holder
    AddrVFGNode(const AddrVFGNode &);  ///< place holder
    void operator=(const AddrVFGNode &); ///< place holder

public:
    /// Constructor
    AddrVFGNode(NodeID id, const AddrPE* edge): StmtVFGNode(id, edge,Addr) {

    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrVFGNode *) {
        return true;
    }
    static inline bool classof(const StmtVFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == Addr;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == Addr;
    }
    //@}
};


class ArgumentVFGNode : public VFGNode {

protected:
    const PAGNode* param;

public:
    /// Constructor
    ArgumentVFGNode(NodeID id, const PAGNode* p, VFGNodeK k): VFGNode(id,k), param(p) {
    }

    /// Whether this argument node is of pointer type (used for pointer analysis).
    inline bool isPTANode() const{
		return param->isPointer();
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ArgumentVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
		return node->getNodeKind() == FRet
				|| node->getNodeKind() == ARet
				|| node->getNodeKind() == AParm
				|| node->getNodeKind() == FParm;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
		return node->getNodeKind() == FRet
				|| node->getNodeKind() == ARet
				|| node->getNodeKind() == AParm
				|| node->getNodeKind() == FParm;
    }
    //@}

};

/*
 * ICFG Node stands for acutal parameter node (top level pointers)
 */
class ActualParmVFGNode : public ArgumentVFGNode {
private:
    const CallBlockNode* cs;
public:
    /// Constructor
	ActualParmVFGNode(NodeID id, const PAGNode* n, const CallBlockNode* c) :
			ArgumentVFGNode(id, n, AParm), cs(c) {
		bb = cs->getCallSite().getInstruction()->getParent();
	}

    /// Return callsite
    inline const CallBlockNode* getCallSite() const {
        return cs;
    }

    /// Return parameter
    inline const PAGNode* getParam() const {
        return param;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualParmVFGNode *) {
        return true;
    }
    static inline bool classof(const ArgumentVFGNode *node) {
    		return node->getNodeKind() == AParm;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == AParm;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == AParm;
    }
    //@}
};


/*
 * ICFG Node stands for formal parameter node (top level pointers)
 */
class FormalParmVFGNode : public ArgumentVFGNode {
private:
    const Function* fun;
    CallPESet callPEs;

public:
    /// Constructor
    FormalParmVFGNode(NodeID id, const PAGNode* n, const Function* f):
    		ArgumentVFGNode(id, n, FParm),  fun(f) {
        bb = &fun->getEntryBlock();
    }

    /// Return parameter
    inline const PAGNode* getParam() const {
        return param;
    }

    /// Return function
    inline const Function* getFun() const {
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
    static inline bool classof(const FormalParmVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == FParm;
    }
    static inline bool classof(const ArgumentVFGNode *node) {
        return node->getNodeKind() == FParm;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == FParm;
    }
    //@}
};

/*!
 * Callsite receive paramter
 */
class ActualRetVFGNode: public ArgumentVFGNode {
private:
    const CallBlockNode* cs;

    ActualRetVFGNode();                      ///< place holder
    ActualRetVFGNode(const ActualRetVFGNode &);  ///< place holder
    void operator=(const ActualRetVFGNode &); ///< place holder

public:
    /// Constructor
	ActualRetVFGNode(NodeID id, const PAGNode* n, const CallBlockNode* c) :
			ArgumentVFGNode(id, n, ARet), cs(c) {
		bb = cs->getCallSite().getInstruction()->getParent();
	}
    /// Return callsite
    inline const CallBlockNode* getCallSite() const {
        return cs;
    }
    /// Receive parameter at callsite
    inline const Function* getCaller() const {
        return cs->getCaller();
    }
    /// Receive parameter at callsite
    inline const PAGNode* getRev() const {
        return param;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualRetVFGNode *) {
        return true;
    }
    static inline bool classof(const ArgumentVFGNode *node) {
        return node->getNodeKind() == ARet;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == ARet;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == ARet;
    }
    //@}
};

/*!
 * Callee return ICFG node
 */
class FormalRetVFGNode: public ArgumentVFGNode {
private:
    const Function* fun;
    RetPESet retPEs;

    FormalRetVFGNode();                      ///< place holder
    FormalRetVFGNode(const FormalRetVFGNode &);  ///< place holder
    void operator=(const FormalRetVFGNode &); ///< place holder

public:
    /// Constructor
	FormalRetVFGNode(NodeID id, const PAGNode* n, const Function* f);

    /// Return value at callee
    inline const PAGNode* getRet() const {
        return param;
    }
    /// Function
    inline const Function* getFun() const {
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
    static inline bool classof(const FormalRetVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == FRet;
    }
    static inline bool classof(const ArgumentVFGNode *node) {
        return node->getNodeKind() == FRet;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == FRet;
    }
    //@}
};

/*
 * Inter LLVM PHI node (formal parameter)
 */
class InterPHIVFGNode : public PHIVFGNode {

public:
    /// Constructor interPHI for formal parameter
    InterPHIVFGNode(NodeID id, const FormalParmVFGNode* fp) : PHIVFGNode(id, fp->getParam(), TInterPhi),fun(fp->getFun()),callInst(NULL) {}
    /// Constructor interPHI for actual return
    InterPHIVFGNode(NodeID id, const ActualRetVFGNode* ar) : PHIVFGNode(id, ar->getRev(), TInterPhi), fun(ar->getCaller()),callInst(ar->getCallSite()) {}

    inline bool isFormalParmPHI() const {
        return (fun!=NULL) && (callInst == NULL);
    }

    inline bool isActualRetPHI() const {
        return (fun!=NULL) && (callInst != NULL);
    }

    inline const Function* getFun() const {
        assert((isFormalParmPHI() || isActualRetPHI())  && "expect a formal parameter phi");
        return fun;
    }

    inline const CallBlockNode* getCallSite() const {
        assert(isActualRetPHI() && "expect a actual return phi");
        return callInst;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterPHIVFGNode *node) {
        return true;
    }
    static inline bool classof(const PHIVFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == TInterPhi;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == TInterPhi;
    }
    //@}

private:
    const Function* fun;
    const CallBlockNode* callInst;
};



/*!
 * Dummy Definition for undef and null pointers
 */
class NullPtrVFGNode : public VFGNode {
private:
    const PAGNode* node;
public:
    /// Constructor
    NullPtrVFGNode(NodeID id, const PAGNode* n) : VFGNode(id,NPtr), node(n) {

    }
    /// Whether this node is of pointer type (used for pointer analysis).
    inline bool isPTANode() const{
		return node->isPointer();
    }
    /// Return corresponding PAGNode
    const PAGNode* getPAGNode() const {
        return node;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NullPtrVFGNode *) {
        return true;
    }
    static inline bool classof(const VFGNode *node) {
        return node->getNodeKind() == NPtr;
    }
    static inline bool classof(const GenericVFGNodeTy *node) {
        return node->getNodeKind() == NPtr;
    }
    //@}
};


#endif /* INCLUDE_UTIL_VFGNODE_H_ */
