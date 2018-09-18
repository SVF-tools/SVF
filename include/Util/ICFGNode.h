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
class StmtICFGNode;
class FormalParmICFGNode;
class ActualParmICFGNode;
class FormalRetICFGNode;
class ActualRetICFGNode;

/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<ICFGNode,ICFGEdge> GenericICFGNodeTy;
class ICFGNode : public GenericICFGNodeTy {

public:
    /// 22 kinds of ICFG node
    /// Gep represents offset edge for field sensitivity
    enum ICFGNodeK {
        Addr, Copy, Gep, Store, Load, TPhi, TIntraPhi, TInterPhi,
        MPhi, MIntraPhi, MInterPhi, FRet, ARet, AParm, FParm,
        BasicBlock, FunEntry, FunExit, FunCall, FunRet, APIN, APOUT, FPIN, FPOUT, NPtr
    };

    typedef ICFGEdge::ICFGEdgeSetTy::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSetTy::const_iterator const_iterator;
    typedef std::set<const CallPE*> CallPESet;
    typedef std::set<const RetPE*> RetPESet;

public:
    /// Constructor
    ICFGNode(NodeID i, ICFGNodeK k): GenericICFGNodeTy(i,k), bb(NULL) {

    }
    /// We should know the program location (basic block level) of each ICFG node
    virtual const llvm::BasicBlock* getBB() const {
        return bb;
    }

    /// Overloading operator << for dumping ICFG node ID
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
 * ICFG node stands for a program statement
 */
class IntraBlockNode : public ICFGNode {
public:
    typedef std::vector<const StmtICFGNode*> StmtICFGNodeVec;
    typedef StmtICFGNodeVec::iterator iterator;
    typedef StmtICFGNodeVec::const_iterator const_iterator;

private:
    const llvm::Instruction* inst;
    StmtICFGNodeVec stmts;
public:
    IntraBlockNode(NodeID id, const llvm::Instruction* i) : ICFGNode(id, BasicBlock), inst(i){
    }

	inline const llvm::Instruction* getInst() const {
		return inst;
	}

	inline void addStmtICFGNode(const StmtICFGNode* s) {
		stmts.push_back(s);
	}

	inline iterator stmtBegin() {
		return stmts.begin();
	}

	inline iterator stmtEnd() {
		return stmts.end();
	}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraBlockNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == BasicBlock;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == BasicBlock;
    }
    //@}
};

class InterBlockNode : public ICFGNode {

public:
    /// Constructor
	InterBlockNode(NodeID id, ICFGNodeK k): ICFGNode(id,k) {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterBlockNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
		return node->getNodeKind() == FunEntry
				|| node->getNodeKind() == FunExit
				|| node->getNodeKind() == FunCall
				|| node->getNodeKind() == FunRet;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
		return node->getNodeKind() == FunEntry
				|| node->getNodeKind() == FunExit
				|| node->getNodeKind() == FunCall
				|| node->getNodeKind() == FunRet;
    }
    //@}
};


/*!
 * Function entry ICFGNode containing a set of FormalParmICFGNodes of a function
 */
class FunEntryBlockNode : public InterBlockNode {

public:
    typedef std::vector<const FormalParmICFGNode*> FormalParmICFGNodeVec;
private:
    const llvm::Function* fun;
    FormalParmICFGNodeVec FPNodes;
public:
    FunEntryBlockNode(NodeID id, const llvm::Function* f): InterBlockNode(id, FunEntry), fun(f){
    }
    /// Return function
    inline const llvm::Function* getFun() const {
        return fun;
    }
	/// Return the set of formal parameters
	inline const FormalParmICFGNodeVec& getActualParms() const {
		return FPNodes;
	}
	/// Add formal parameters
	inline void addFormalParms(const FormalParmICFGNode* fp) {
		FPNodes.push_back(fp);
	}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *) {
        return true;
    }
	static inline bool classof(const InterBlockNode * node) {
		return node->getNodeKind() == FunEntry;
	}
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunEntry;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunEntry;
    }
    //@}
};

/*!
 * Function exit ICFGNode containing (at most one) FormalRetICFGNodes of a function
 */
class FunExitBlockNode : public InterBlockNode {

private:
    const llvm::Function* fun;
    const FormalRetICFGNode* formalRet;
public:
    FunExitBlockNode(NodeID id, const llvm::Function* f): InterBlockNode(id, FunExit), fun(f), formalRet(NULL){
    }
    /// Return function
    inline const llvm::Function* getFun() const {
        return fun;
    }
	/// Return actual return parameter
	inline const FormalRetICFGNode* getFormalRet() const {
		return formalRet;
	}
	/// Add actual return parameter
	inline void addFormalRet(const FormalRetICFGNode* fr) {
		formalRet = fr;
	}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunExit;
    }
    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunExit;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunExit;
    }
    //@}
};

/*!
 * Call ICFGNode containing a set of ActualParmICFGNodes at a callsite
 */
class CallBlockNode : public InterBlockNode {

public:
    typedef std::vector<const ActualParmICFGNode*> ActualParmICFGNodeVec;
private:
    llvm::CallSite cs;
    ActualParmICFGNodeVec APNodes;
public:
    CallBlockNode(NodeID id, llvm::CallSite c): InterBlockNode(id, FunCall), cs(c){
    }

    /// Return callsite
	inline llvm::CallSite getCallSite() const {
		return cs;
	}
	/// Return the set of actual parameters
	inline const ActualParmICFGNodeVec& getActualParms() const {
		return APNodes;
	}
	/// Add actual parameters
	inline void addActualParms(const ActualParmICFGNode* ap) {
		APNodes.push_back(ap);
	}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallBlockNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunCall;
    }
    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunCall;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunCall;
    }
    //@}
};




/*!
 * Return ICFGNode containing (at most one) ActualRetICFGNode at a callsite
 */
class RetBlockNode : public InterBlockNode {

private:
    llvm::CallSite cs;
    const ActualRetICFGNode* actualRet;
public:
    RetBlockNode(NodeID id, llvm::CallSite c): InterBlockNode(id, FunRet), cs(c), actualRet(NULL){
    }

	/// Return callsite
	inline llvm::CallSite getCallSite() const {
		return cs;
	}
	/// Return actual return parameter
	inline const ActualRetICFGNode* getActualRet() const {
		return actualRet;
	}
	/// Add actual return parameter
	inline void addActualRet(const ActualRetICFGNode* ar) {
		actualRet = ar;
	}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetBlockNode *) {
        return true;
    }
    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunRet;
    }
    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunRet;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunRet;
    }
    //@}
};

/*!
 * ICFG node stands for a program statement
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

/*
 * ICFG Node stands for a top level pointer ssa phi node or a formal parameter or a return parameter
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


class ArgumentICFGNode : public ICFGNode {

protected:
    const PAGNode* param;

public:
    /// Constructor
    ArgumentICFGNode(NodeID id, const PAGNode* p, ICFGNodeK k): ICFGNode(id,k), param(p) {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ArgumentICFGNode *) {
        return true;
    }
    static inline bool classof(const ICFGNode *node) {
		return node->getNodeKind() == FRet
				|| node->getNodeKind() == ARet
				|| node->getNodeKind() == AParm
				|| node->getNodeKind() == FParm;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
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
class ActualParmICFGNode : public ArgumentICFGNode {
private:
    llvm::CallSite cs;
public:
    /// Constructor
	ActualParmICFGNode(NodeID id, const PAGNode* n, llvm::CallSite c) :
			ArgumentICFGNode(id, n, AParm), cs(c) {
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
    static inline bool classof(const ArgumentICFGNode *node) {
    		return node->getNodeKind() == AParm;
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
 * ICFG Node stands for formal parameter node (top level pointers)
 */
class FormalParmICFGNode : public ArgumentICFGNode {
private:
    const llvm::Function* fun;
    CallPESet callPEs;

public:
    /// Constructor
    FormalParmICFGNode(NodeID id, const PAGNode* n, const llvm::Function* f):
    		ArgumentICFGNode(id, n, FParm),  fun(f) {
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
    static inline bool classof(const ArgumentICFGNode *node) {
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
class ActualRetICFGNode: public ArgumentICFGNode {
private:
    llvm::CallSite cs;

    ActualRetICFGNode();                      ///< place holder
    ActualRetICFGNode(const ActualRetICFGNode &);  ///< place holder
    void operator=(const ActualRetICFGNode &); ///< place holder

public:
    /// Constructor
	ActualRetICFGNode(NodeID id, const PAGNode* n, llvm::CallSite c) :
			ArgumentICFGNode(id, n, ARet), cs(c) {
		bb = cs.getInstruction()->getParent();
	}
    /// Return callsite
    inline llvm::CallSite getCallSite() const {
        return cs;
    }
    /// Receive parameter at callsite
    inline const PAGNode* getRev() const {
        return param;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ActualRetICFGNode *) {
        return true;
    }
    static inline bool classof(const ArgumentICFGNode *node) {
        return node->getNodeKind() == ARet;
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
 * Callee return ICFG node
 */
class FormalRetICFGNode: public ArgumentICFGNode {
private:
    const llvm::Function* fun;
    RetPESet retPEs;

    FormalRetICFGNode();                      ///< place holder
    FormalRetICFGNode(const FormalRetICFGNode &);  ///< place holder
    void operator=(const FormalRetICFGNode &); ///< place holder

public:
    /// Constructor
	FormalRetICFGNode(NodeID id, const PAGNode* n, const llvm::Function* f) :
			ArgumentICFGNode(id, n, FRet), fun(f) {
		bb = analysisUtil::getFunExitBB(fun);
	}
    /// Return value at callee
    inline const PAGNode* getRet() const {
        return param;
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
    static inline bool classof(const ArgumentICFGNode *node) {
        return node->getNodeKind() == FRet;
    }
    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FRet;
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


#endif /* ICFGNode_H_ */
