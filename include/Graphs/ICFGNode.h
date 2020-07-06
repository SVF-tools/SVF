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

#include "Util/SVFUtil.h"
#include "Graphs/GenericGraph.h"
#include "Graphs/ICFGEdge.h"

class ICFGNode;
class CallPE;
class RetPE;
class PAGEdge;
class PAGNode;
class VFGNode;

/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValPN) and address-taken objects (ObjPN)
 */
typedef GenericNode<ICFGNode, ICFGEdge> GenericICFGNodeTy;

class ICFGNode : public GenericICFGNodeTy
{

public:
    /// 22 kinds of ICFG node
    /// Gep represents offset edge for field sensitivity
    enum ICFGNodeK
    {
        IntraBlock, FunEntryBlock, FunExitBlock, FunCallBlock, FunRetBlock, GlobalBlock
    };

    typedef ICFGEdge::ICFGEdgeSetTy::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSetTy::const_iterator const_iterator;
    typedef DenseSet<const CallPE *> CallPESet;
    typedef DenseSet<const RetPE *> RetPESet;
    typedef std::list<const VFGNode*> VFGNodeList;

public:
    /// Constructor
    ICFGNode(NodeID i, ICFGNodeK k) : GenericICFGNodeTy(i, k), fun(NULL)
    {

    }

    /// Return the function of this ICFGNode
    virtual const SVFFunction* getFun() const
    {
        return fun;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const ICFGNode &node)
    {
        o << "ICFGNode ID:" << node.getId();
        return o;
    }
    //@}

    /// Set/Get methods of VFGNodes
    ///@{
    inline void addVFGNode(const VFGNode *vfgNode)
    {
        VFGNodes.push_back(vfgNode);
    }

    inline const VFGNodeList& getVFGNodes() const
    {
        return VFGNodes;
    }
    ///@}
protected:
    const SVFFunction* fun;
    VFGNodeList VFGNodes; //< a set of VFGNodes

};

/*!
 * Unique ICFG node stands for all global initializations
 */
class GlobalBlockNode : public ICFGNode
{

public:
    GlobalBlockNode(NodeID id) : ICFGNode(id, GlobalBlock)
    {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GlobalBlockNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == GlobalBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == GlobalBlock;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const GlobalBlockNode &node)
    {
        o << "GlobalBlockNode ICFGNode ID:" << node.getId();
        return o;
    }
    //@}
};

/*!
 * ICFG node stands for a program statement
 */
class IntraBlockNode : public ICFGNode
{
private:
    const Instruction *inst;

public:
    IntraBlockNode(NodeID id, const Instruction *i) : ICFGNode(id, IntraBlock), inst(i)
    {
        fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(inst->getFunction());
    }

    inline const Instruction *getInst() const
    {
        return inst;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const IntraBlockNode &node)
    {
        o << "IntraBlockNode ICFGNode ID:" << node.getId();
        o << " instruction: " << node.getInst() << " (fun: " << node.getFun()->getName() << ")";
        return o;
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraBlockNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == IntraBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == IntraBlock;
    }
    //@}
};

class InterBlockNode : public ICFGNode
{

public:
    /// Constructor
    InterBlockNode(NodeID id, ICFGNodeK k) : ICFGNode(id, k)
    {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterBlockNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunEntryBlock
               || node->getNodeKind() == FunExitBlock
               || node->getNodeKind() == FunCallBlock
               || node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
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
class FunEntryBlockNode : public InterBlockNode
{

public:
    typedef std::vector<const PAGNode *> FormalParmNodeVec;
private:
    FormalParmNodeVec FPNodes;
public:
    FunEntryBlockNode(NodeID id, const SVFFunction* f);

    /// Return function
    inline const SVFFunction* getFun() const
    {
        return fun;
    }

    /// Return the set of formal parameters
    inline const FormalParmNodeVec &getFormalParms() const
    {
        return FPNodes;
    }

    /// Add formal parameters
    inline void addFormalParms(const PAGNode *fp)
    {
        FPNodes.push_back(fp);
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *)
    {
        return true;
    }

    static inline bool classof(const InterBlockNode *node)
    {
        return node->getNodeKind() == FunEntryBlock;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunEntryBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunEntryBlock;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const FunEntryBlockNode &node)
    {
        o << "FunEntryBlockNode ICFGNode ID:" << node.getId();
        o << " (fun: " << node.getFun()->getName() << ")";
        return o;
    }
    //@}
};

/*!
 * Function exit ICFGNode containing (at most one) FormalRetVFGNodes of a function
 */
class FunExitBlockNode : public InterBlockNode
{

private:
    const SVFFunction* fun;
    const PAGNode *formalRet;
public:
    FunExitBlockNode(NodeID id, const SVFFunction* f);

    /// Return function
    inline const SVFFunction* getFun() const
    {
        return fun;
    }

    /// Return actual return parameter
    inline const PAGNode *getFormalRet() const
    {
        return formalRet;
    }

    /// Add actual return parameter
    inline void addFormalRet(const PAGNode *fr)
    {
        formalRet = fr;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryBlockNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const InterBlockNode *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const FunExitBlockNode &node)
    {
        o << "FunExitBlockNode ICFGNode ID:" << node.getId();
        o << " (fun: " << node.getFun()->getName() << ")";
        return o;
    }
    //@}
};

/*!
 * Call ICFGNode containing a set of ActualParmVFGNodes at a callsite
 */
class CallBlockNode : public InterBlockNode
{

public:
    typedef std::vector<const PAGNode *> ActualParmVFGNodeVec;
private:
    const Instruction* cs;
    ActualParmVFGNodeVec APNodes;
public:
    CallBlockNode(NodeID id, const Instruction* c) : InterBlockNode(id, FunCallBlock), cs(c)
    {
        fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(cs->getFunction());
    }

    /// Return callsite
    inline const Instruction* getCallSite() const
    {
        return cs;
    }

    /// Return callsite
    inline const SVFFunction* getCaller() const
    {
        return LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(cs->getFunction());
    }

    /// Return Basic Block
    inline const BasicBlock* getParent() const
    {
        return cs->getParent();
    }

    /// Return true if this is an indirect call
    inline bool isIndirectCall() const
    {
        return NULL == SVFUtil::getCallee(cs);
    }

    /// Return the set of actual parameters
    inline const ActualParmVFGNodeVec &getActualParms() const
    {
        return APNodes;
    }

    /// Add actual parameters
    inline void addActualParms(const PAGNode *ap)
    {
        APNodes.push_back(ap);
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallBlockNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const InterBlockNode *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const CallBlockNode &node)
    {
        o << "CallBlockNode ICFGNode ID:" << node.getId();
        o << " instruction: " << *node.getCallSite() << " (fun: " << node.getFun()->getName() << ")";
        return o;
    }
    //@}
};


/*!
 * Return ICFGNode containing (at most one) ActualRetVFGNode at a callsite
 */
class RetBlockNode : public InterBlockNode
{

private:
    const Instruction* cs;
    const PAGNode *actualRet;
    const CallBlockNode* callBlockNode;
public:
    RetBlockNode(NodeID id, const Instruction* c, CallBlockNode* cb) :
        InterBlockNode(id, FunRetBlock), cs(c), actualRet(NULL), callBlockNode(cb)
    {
        fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(cs->getFunction());
    }

    /// Return callsite
    inline const Instruction* getCallSite() const
    {
        return cs;
    }

    inline const CallBlockNode* getCallBlockNode() const
    {
        return callBlockNode;
    }
    /// Return actual return parameter
    inline const PAGNode *getActualRet() const
    {
        return actualRet;
    }

    /// Add actual return parameter
    inline void addActualRet(const PAGNode *ar)
    {
        actualRet = ar;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetBlockNode *)
    {
        return true;
    }

    static inline bool classof(const InterBlockNode *node)
    {
        return node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunRetBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunRetBlock;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream &operator<<(raw_ostream &o, const RetBlockNode &node)
    {
        o << "RetBlockNode ICFGNode ID:" << node.getId();
        o << " instruction: " << *node.getCallSite() << " (fun: " << node.getFun()->getName() << ")";
        return o;
    }
    //@}
};


#endif /* ICFGNode_H_ */
