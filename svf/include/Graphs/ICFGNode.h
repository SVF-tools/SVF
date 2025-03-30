//===- ICFGNode.h -- ICFG node------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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

namespace SVF
{

class ICFGNode;
class RetICFGNode;
class CallPE;
class RetPE;
class SVFStmt;
class SVFVar;
class VFGNode;

/*!
 * Interprocedural control-flow graph node, representing different kinds of program statements
 * including top-level pointers (ValVar) and address-taken objects (ObjVar)
 */
typedef GenericNode<ICFGNode, ICFGEdge> GenericICFGNodeTy;

class ICFGNode : public GenericICFGNodeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:

    typedef ICFGEdge::ICFGEdgeSetTy::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSetTy::const_iterator const_iterator;
    typedef Set<const CallPE *> CallPESet;
    typedef Set<const RetPE *> RetPESet;
    typedef std::list<const VFGNode*> VFGNodeList;
    typedef std::list<const SVFStmt*> SVFStmtList;
    typedef GNodeK ICFGNodeK;

public:
    /// Constructor
    ICFGNode(NodeID i, GNodeK k) : GenericICFGNodeTy(i, k), fun(nullptr), bb(nullptr)
    {
    }

    /// Return the function of this ICFGNode
    virtual const FunObjVar* getFun() const
    {
        return fun;
    }

    /// Return the basic block of this ICFGNode
    virtual const SVFBasicBlock* getBB() const
    {
        return bb;
    }


    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream &operator<<(OutStream &o, const ICFGNode &node)
    {
        o << node.toString();
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

    /// Set/Get methods of VFGNodes
    ///@{
    inline void addSVFStmt(const SVFStmt *edge)
    {
        pagEdges.push_back(edge);
    }

    inline const SVFStmtList& getSVFStmts() const
    {
        return pagEdges;
    }
    ///@}

    virtual const std::string toString() const;



    void dump() const;


    static inline bool classof(const ICFGNode *)
    {
        return true;
    }

    static inline bool classof(const GenericICFGNodeTy* node)
    {
        return isICFGNodeKinds(node->getNodeKind());
    }

    static inline bool classof(const SVFValue* node)
    {
        return isICFGNodeKinds(node->getNodeKind());
    }



protected:
    const FunObjVar* fun;
    const SVFBasicBlock* bb;
    VFGNodeList VFGNodes; //< a list of VFGNodes
    SVFStmtList pagEdges; //< a list of PAGEdges

};

/*!
 * Unique ICFG node stands for all global initializations
 */
class GlobalICFGNode : public ICFGNode
{

public:
    GlobalICFGNode(NodeID id) : ICFGNode(id, GlobalBlock)
    {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GlobalICFGNode *)
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

    const std::string toString() const override;

    const std::string getSourceLoc() const override
    {
        return "Global ICFGNode";
    }
};

/*!
 * ICFG node stands for a program statement
 */
class IntraICFGNode : public ICFGNode
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

protected:
    IntraICFGNode(NodeID id, const SVFBasicBlock* bb, const FunObjVar* funObjVar, bool isReturn): ICFGNode(id, IntraBlock),isRet(isReturn)
    {

        this->fun = funObjVar;
        this->bb = bb;
    }

private:
    bool isRet;

    /// Constructor to create empty IntraICFGNode (for SVFIRReader/deserialization)
    IntraICFGNode(NodeID id) : ICFGNode(id, IntraBlock), isRet(false) {}

public:
    IntraICFGNode(NodeID id, const SVFBasicBlock* b, bool isReturn) : ICFGNode(id, IntraBlock), isRet(isReturn)
    {
        fun = b->getFunction();
        bb = b;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraICFGNode *)
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

    const std::string toString() const override;

    inline bool isRetInst() const
    {
        return isRet;
    }
};

class InterICFGNode : public ICFGNode
{

public:
    /// Constructor
    InterICFGNode(NodeID id, ICFGNodeK k) : ICFGNode(id, k)
    {
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const InterICFGNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode* node)
    {
        return isInterICFGNodeKind(node->getNodeKind());
    }

    static inline bool classof(const GenericICFGNodeTy* node)
    {
        return isInterICFGNodeKind(node->getNodeKind());
    }

    static inline bool classof(const SVFValue* node)
    {
        return isInterICFGNodeKind(node->getNodeKind());
    }

    //@}
};




/*!
 * Function entry ICFGNode containing a set of FormalParmVFGNodes of a function
 */
class FunEntryICFGNode : public InterICFGNode
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

protected:
    FunEntryICFGNode(NodeID id, const FunObjVar* f, SVFBasicBlock* bb)
    : InterICFGNode(id, FunEntryBlock)
    {
        this->fun = f;
        this->bb = bb;
    }

public:
    typedef std::vector<const SVFVar *> FormalParmNodeVec;
private:
    FormalParmNodeVec FPNodes;

    /// Constructor to create empty FunEntryICFGNode (for SVFIRReader/deserialization)
    FunEntryICFGNode(NodeID id) : InterICFGNode(id, FunEntryBlock) {}

public:
    FunEntryICFGNode(NodeID id, const FunObjVar* f);

    /// Return function
    inline const FunObjVar* getFun() const override
    {
        return fun;
    }

    /// Return the set of formal parameters
    inline const FormalParmNodeVec &getFormalParms() const
    {
        return FPNodes;
    }

    /// Add formal parameters
    inline void addFormalParms(const SVFVar *fp)
    {
        FPNodes.push_back(fp);
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryICFGNode *)
    {
        return true;
    }

    static inline bool classof(const InterICFGNode *node)
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

    static inline bool classof(const SVFValue*node)
    {
        return node->getNodeKind() == FunEntryBlock;
    }
    //@}

    const std::string toString() const override;

    const std::string getSourceLoc() const override;
};

/*!
 * Function exit ICFGNode containing (at most one) FormalRetVFGNodes of a function
 */
class FunExitICFGNode : public InterICFGNode
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

protected:
    FunExitICFGNode(NodeID id, const FunObjVar* f, SVFBasicBlock* bb) 
    : InterICFGNode(id, FunExitBlock), formalRet(nullptr)
    {
        this->fun = f;
        this->bb = bb;
    }

private:
    const SVFVar *formalRet;

    /// Constructor to create empty FunExitICFGNode (for SVFIRReader/deserialization)
    FunExitICFGNode(NodeID id) : InterICFGNode(id, FunExitBlock), formalRet{} {}

public:
    FunExitICFGNode(NodeID id, const FunObjVar* f);

    /// Return function
    inline const FunObjVar* getFun() const override
    {
        return fun;
    }

    /// Return formal return parameter
    inline const SVFVar *getFormalRet() const
    {
        return formalRet;
    }

    /// Add formal return parameter
    inline void addFormalRet(const SVFVar *fr)
    {
        formalRet = fr;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunEntryICFGNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const InterICFGNode *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunExitBlock;
    }

    static inline bool classof(const SVFValue*node)
    {
        return node->getNodeKind() == FunExitBlock;
    }
    //@}

    const std::string toString() const override;

    const std::string getSourceLoc() const override;
};

/*!
 * Call ICFGNode containing a set of ActualParmVFGNodes at a callsite
 */
class CallICFGNode : public InterICFGNode
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

public:
    typedef std::vector<const ValVar *> ActualParmNodeVec;

protected:
    const RetICFGNode* ret;
    ActualParmNodeVec APNodes;      /// arguments
    const FunObjVar* calledFunc;  /// called function
    bool isvararg;                  /// is variable argument
    bool isVirCallInst;             /// is virtual call inst
    SVFVar* vtabPtr;                /// virtual table pointer
    s32_t virtualFunIdx;            /// virtual function index of the virtual table(s) at a virtual call
    std::string funNameOfVcall;     /// the function name of this virtual call

    /// Constructor to create empty CallICFGNode (for SVFIRReader/deserialization)
    CallICFGNode(NodeID id) : InterICFGNode(id, FunCallBlock), ret{} {}
    
    CallICFGNode(NodeID id, const SVFBasicBlock* bb, const SVFType* type,
                 const FunObjVar* fun, const FunObjVar* cf, const RetICFGNode* ret, 
                 bool iv, bool ivc, s32_t vfi, SVFVar* vtabPtr, const std::string& fnv)
        : InterICFGNode(id, FunCallBlock), ret(ret), calledFunc(cf),
          isvararg(iv), isVirCallInst(ivc), vtabPtr(vtabPtr),
          virtualFunIdx(vfi), funNameOfVcall(fnv)
    {
        this->fun = fun;
        this->bb = bb;
        this->type = type;
    }
                

public:
    CallICFGNode(NodeID id, const SVFBasicBlock* b, const SVFType* ty,
                 const FunObjVar* cf, bool iv, bool ivc, s32_t vfi,
                 const std::string& fnv)
        : InterICFGNode(id, FunCallBlock), ret(nullptr), calledFunc(cf),
          isvararg(iv), isVirCallInst(ivc), vtabPtr(nullptr),
          virtualFunIdx(vfi), funNameOfVcall(fnv)
    {
        fun = b->getFunction();
        bb = b;
        type = ty;
    }

    /// Return callsite
    inline const RetICFGNode* getRetICFGNode() const
    {
        assert(ret && "RetICFGNode not set?");
        return ret;
    }

    /// Return callsite
    inline void setRetICFGNode(const RetICFGNode* r)
    {
        ret = r;
    }

    /// Return callsite
    inline const FunObjVar* getCaller() const
    {
        return getFun();
    }

    /// Return Basic Block
    inline const SVFBasicBlock* getParent() const
    {
        return getBB();
    }

    /// Return true if this is an indirect call
    inline bool isIndirectCall() const
    {
        return nullptr == calledFunc;
    }

    /// Return the set of actual parameters
    inline const ActualParmNodeVec &getActualParms() const
    {
        return APNodes;
    }

    /// Add actual parameters
    inline void addActualParms(const ValVar *ap)
    {
        APNodes.push_back(ap);
    }
    /// Parameter operations
    //@{
    inline const ValVar* getArgument(u32_t ArgNo) const
    {
        return getActualParms()[ArgNo];
    }

    inline u32_t arg_size() const
    {
        return APNodes.size();
    }
    inline bool arg_empty() const
    {
        return APNodes.empty();
    }

    inline u32_t getNumArgOperands() const
    {
        return arg_size();
    }
    inline const FunObjVar* getCalledFunction() const
    {
        return calledFunc;
    }

    inline bool isVarArg() const
    {
        return isvararg;
    }
    inline bool isVirtualCall() const
    {
        return isVirCallInst;
    }

    inline void setVtablePtr(SVFVar* v)
    {
        vtabPtr = v;
    }

    inline const SVFVar* getVtablePtr() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        return vtabPtr;
    }


    inline s32_t getFunIdxInVtable() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        assert(virtualFunIdx >=0 && "virtual function idx is less than 0? not set yet?");
        return virtualFunIdx;
    }

    inline const std::string& getFunNameOfVirtualCall() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        return funNameOfVcall;
    }
    //@}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallICFGNode *)
    {
        return true;
    }

    static inline bool classof(const ICFGNode *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const InterICFGNode *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const GenericICFGNodeTy *node)
    {
        return node->getNodeKind() == FunCallBlock;
    }

    static inline bool classof(const SVFValue*node)
    {
        return node->getNodeKind() == FunCallBlock;
    }
    //@}

    const std::string toString() const override;

    const std::string getSourceLoc() const override
    {
        return "CallICFGNode: " + ICFGNode::getSourceLoc();
    }
};


/*!
 * Return ICFGNode containing (at most one) ActualRetVFGNode at a callsite
 */
class RetICFGNode : public InterICFGNode
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;

protected:
    RetICFGNode(NodeID id, const SVFType* type, const SVFBasicBlock* bb, const FunObjVar* funObjVar) : 
        InterICFGNode(id, FunRetBlock), actualRet(nullptr), callBlockNode(nullptr)
    {
        this->fun = funObjVar;
        this->bb = bb;
        this->type = type;
    }

private:
    const SVFVar *actualRet;
    const CallICFGNode* callBlockNode;

    /// Constructor to create empty RetICFGNode (for SVFIRReader/deserialization)
    RetICFGNode(NodeID id)
        : InterICFGNode(id, FunRetBlock), actualRet{}, callBlockNode{}
    {
    }

public:
    RetICFGNode(NodeID id, CallICFGNode* cb) :
        InterICFGNode(id, FunRetBlock), actualRet(nullptr), callBlockNode(cb)
    {
        fun = cb->getFun();
        bb = cb->getBB();
        type = cb->getType();
    }

    inline const CallICFGNode* getCallICFGNode() const
    {
        return callBlockNode;
    }
    /// Return actual return parameter
    inline const SVFVar *getActualRet() const
    {
        return actualRet;
    }

    /// Add actual return parameter
    inline void addActualRet(const SVFVar *ar)
    {
        actualRet = ar;
    }

    inline void addCallBlockNode(const CallICFGNode* cb)
    {
        callBlockNode = cb;
    }

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetICFGNode *)
    {
        return true;
    }

    static inline bool classof(const InterICFGNode *node)
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
    static inline bool classof(const SVFValue*node)
    {
        return node->getNodeKind() == FunRetBlock;
    }
    //@}

    const std::string toString() const override;

    const std::string getSourceLoc() const override
    {
        return "RetICFGNode: " + ICFGNode::getSourceLoc();
    }
};

} // End namespace SVF

#endif /* ICFGNode_H_ */
