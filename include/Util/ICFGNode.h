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

#include "MemoryModel/GenericGraph.h"
#include "MemoryModel/PAG.h"
#include "Util/ICFGEdge.h"

class ICFGNode;

/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<ICFGNode, ICFGEdge> GenericICFGNodeTy;

class ICFGNode : public GenericICFGNodeTy {

public:
    /// 22 kinds of ICFG node
    /// Gep represents offset edge for field sensitivity
    enum ICFGNodeK {
        IntraBlock, FunEntryBlock, FunExitBlock, FunCallBlock, FunRetBlock
    };

    typedef ICFGEdge::ICFGEdgeSetTy::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSetTy::const_iterator const_iterator;
    typedef std::set<const CallPE *> CallPESet;
    typedef std::set<const RetPE *> RetPESet;

public:
    /// Constructor
    ICFGNode(NodeID i, ICFGNodeK k) : GenericICFGNodeTy(i, k), bb(NULL) {

    }

    /// We should know the program location (basic block level) of each ICFG node
    virtual const BasicBlock *getBB() const {
        return bb;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const ICFGNode &node) {
        o << "ICFGNode ID:" << node.getId();
        return o;
    }
    //@}
protected:
    const BasicBlock *bb;
};


/*!
 * ICFG node stands for a program statement
 */
class IntraBlockNode : public ICFGNode {
public:
    typedef std::vector<const PAGEdge *> StmtOrPHIVec;
    typedef StmtOrPHIVec::iterator iterator;
    typedef StmtOrPHIVec::const_iterator const_iterator;

private:
    const Instruction *inst;
    const std::string* str_value;
    StmtOrPHIVec vnodes;
public:
    IntraBlockNode(NodeID id, const Instruction *i) : ICFGNode(id, IntraBlock), inst(i) {
        bb = inst->getParent();
    }

    IntraBlockNode(NodeID id, const std::string *i) : ICFGNode(id, IntraBlock), str_value(i) {
    }

    inline const Instruction *getInst() const {
        return inst;
    }

    inline const std::string *getInst_str_value() const {
        return str_value;
    }

    inline void addPAGEdge(const PAGEdge *s) {
        // avoid duplicate element
        for(StmtOrPHIVec::const_iterator it = vnodes.begin(), eit = vnodes.end(); it!=eit; ++it)
            if(*it==s)
                return;

        vnodes.push_back(s);
    }

    inline StmtOrPHIVec &getPAGEdges() {
        return vnodes;
    }

    inline const_iterator vPAGEdgeBegin() const {
        return vnodes.begin();
    }

    inline const_iterator vPAGEdgeEnd() const {
        return vnodes.end();
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraBlockNode *) {
        return true;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == IntraBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == IntraBlock;
    }
    //@}
};

class InterBlockNode : public ICFGNode {

public:
    /// Constructor
    InterBlockNode(NodeID id, ICFGNodeK k) : ICFGNode(id, k) {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterBlockNode *) {
        return true;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunEntryBlock
               || node->getNodeKind() == FunExitBlock
               || node->getNodeKind() == FunCallBlock
               || node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunEntryBlock
               || node->getNodeKind() == FunExitBlock
               || node->getNodeKind() == FunCallBlock
               || node->getNodeKind() == FunRetBlock;
    }
    //@}
};


/*!
 * Function entry ICFGNode containing a set of FormalParmVFGNodes of a function
 */
class FunEntryBlockNode : public InterBlockNode {

public:
    typedef std::vector<const PAGNode *> FormalParmNodeVec;
private:
    const Function *fun;
    const std::string* str_fun;
    FormalParmNodeVec FPNodes;
public:
    FunEntryBlockNode(NodeID id, const Function *f) : InterBlockNode(id, FunEntryBlock), fun(f) {
        if (!SVFUtil::isExtCall(fun))
            bb = &(fun->getEntryBlock());
    }

    FunEntryBlockNode(NodeID id, const std::string *f) : InterBlockNode(id, FunEntryBlock), str_fun(f) {
    }

    /// Return function
    inline const Function *getFun() const {
        return fun;
    }

    inline const std::string *getStr_fun() const {
        return str_fun;
    }

    /// Return the set of formal parameters
    inline const FormalParmNodeVec &getFormalParms() const {
        return FPNodes;
    }

    /// Add formal parameters
    inline void addFormalParms(const PAGNode *fp) {
        FPNodes.push_back(fp);
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *) {
        return true;
    }

    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunEntryBlock;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunEntryBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunEntryBlock;
    }
    //@}
};

/*!
 * Function exit ICFGNode containing (at most one) FormalRetVFGNodes of a function
 */
class FunExitBlockNode : public InterBlockNode {

private:
    const std::string *str_fun;
    const Function *fun;
    const PAGNode *formalRet;
public:
    FunExitBlockNode(NodeID id, const Function *f) : InterBlockNode(id, FunExitBlock), fun(f), formalRet(NULL) {
        if (!SVFUtil::isExtCall(fun))
            bb = SVFUtil::getFunExitBB(fun);
    }

    FunExitBlockNode(NodeID id, const std::string *f) : InterBlockNode(id, FunExitBlock), str_fun(f), formalRet(NULL) {
    }

    /// Return function
    inline const Function *getFun() const {
        return fun;
    }


    inline const std::string *getStrFun() const {
        return str_fun;
    }

    /// Return actual return parameter
    inline const PAGNode *getFormalRet() const {
        return formalRet;
    }

    /// Add actual return parameter
    inline void addFormalRet(const PAGNode *fr) {
        formalRet = fr;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *) {
        return true;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunExitBlock;
    }
    //@}
};

/*!
 * Call ICFGNode containing a set of ActualParmVFGNodes at a callsite
 */
class CallBlockNode : public InterBlockNode {

public:
    typedef std::vector<const PAGNode *> ActualParmVFGNodeVec;
private:
    CallSite cs;
    ActualParmVFGNodeVec APNodes;
public:
    CallBlockNode(NodeID id, CallSite c) : InterBlockNode(id, FunCallBlock), cs(c) {
        bb = cs.getInstruction()->getParent();
    }

    /// Return callsite
    inline CallSite getCallSite() const {
        return cs;
    }

    /// Return the set of actual parameters
    inline const ActualParmVFGNodeVec &getActualParms() const {
        return APNodes;
    }

    /// Add actual parameters
    inline void addActualParms(const PAGNode *ap) {
        APNodes.push_back(ap);
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallBlockNode *) {
        return true;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunCallBlock;
    }
    //@}
};


/*!
 * Return ICFGNode containing (at most one) ActualRetVFGNode at a callsite
 */
class RetBlockNode : public InterBlockNode {

private:
    CallSite cs;
    const PAGNode *actualRet;
public:
    RetBlockNode(NodeID id, CallSite c) : InterBlockNode(id, FunRetBlock), cs(c), actualRet(NULL) {
        bb = cs.getInstruction()->getParent();
    }

    /// Return callsite
    inline CallSite getCallSite() const {
        return cs;
    }

    /// Return actual return parameter
    inline const PAGNode *getActualRet() const {
        return actualRet;
    }

    /// Add actual return parameter
    inline void addActualRet(const PAGNode *ar) {
        actualRet = ar;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetBlockNode *) {
        return true;
    }

    static inline bool classof(const InterBlockNode *node) {
        return node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const ICFGNode *node) {
        return node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node) {
        return node->getNodeKind() == FunRetBlock;
    }
    //@}
};


#endif /* ICFGNode_H_ */
