//===- SVFStatements.h -- SVF statements-------------------------------------------//
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
 * SVFStatements.h
 *
 *  Created on: Nov 10, 2013
 *      Author: Yulei Sui
 */

#ifndef PAGEDGE_H_
#define PAGEDGE_H_

#include "Graphs/GenericGraph.h"
#include "MemoryModel/LocationSet.h"
#include "Graphs/ICFGNode.h"

namespace SVF
{

class SVFVar;

/*
 * SVFIR program statements (PAGEdges)
 */
typedef GenericEdge<SVFVar> GenericPAGEdgeTy;
class SVFStmt : public GenericPAGEdgeTy
{

public:
    /// Types of SVFIR statements
    /// Gep represents (base + offset) for field sensitivity
    /// ThreadFork/ThreadJoin is to model parameter passings between thread spawners and spawnees.
    enum PEDGEK
    {
        Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, Phi, Cmp, BinaryOp, UnaryOp, Branch, ThreadFork, ThreadJoin
    };

private:
    const Value* value;	///< LLVM value
    const BasicBlock *basicBlock;   ///< LLVM BasicBlock
    ICFGNode *icfgNode;   ///< ICFGNode
    EdgeID edgeId;					///< Edge ID
public:
    static Size_t totalEdgeNum;		///< Total edge number

    /// Constructor
    SVFStmt(SVFVar* s, SVFVar* d, GEdgeFlag k);
    /// Destructor
    ~SVFStmt()
    {
    }

    /// ClassOf
    //@{
    static inline bool classof(const SVFStmt*)
    {
        return true;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr ||
               edge->getEdgeKind() == SVFStmt::Copy ||
               edge->getEdgeKind() == SVFStmt::Store ||
               edge->getEdgeKind() == SVFStmt::Load ||
               edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::NormalGep ||
               edge->getEdgeKind() == SVFStmt::VariantGep ||
               edge->getEdgeKind() == SVFStmt::Cmp ||
               edge->getEdgeKind() == SVFStmt::BinaryOp ||
               edge->getEdgeKind() == SVFStmt::UnaryOp  ||
               edge->getEdgeKind() == SVFStmt::ThreadFork ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    ///@}

    /// Return Edge ID
    inline EdgeID getEdgeID() const
    {
        return edgeId;
    }
    /// Whether src and dst nodes are both of pointer type
    bool isPTAEdge() const;

    /// Get/set methods for llvm instruction
    //@{
    inline const Instruction* getInst() const
    {
        return SVFUtil::dyn_cast<Instruction>(value);
    }
    inline void setValue(const Value *val)
    {
        value = val;
    }
    inline const Value* getValue() const
    {
        return value;
    }
    inline void setBB(const BasicBlock *bb)
    {
        basicBlock = bb;
    }
    inline const BasicBlock* getBB() const
    {
        return basicBlock;
    }
    inline void setICFGNode(ICFGNode *node)
    {
        icfgNode = node;
    }
    inline ICFGNode* getICFGNode() const
    {
        return icfgNode;
    }
    //@}

    /// Compute the unique edgeFlag value from edge kind and second variable operand for MultiOpndStmt.
    static inline GEdgeFlag makeEdgeFlagWithAddionalOpnd(GEdgeKind k, const SVFVar* var)
    {
        Var2LabelMap::const_iterator iter = var2LabelMap.find(var);
        u64_t label = (iter != var2LabelMap.end()) ?
                      iter->second : multiOpndLabelCounter++;
        return (label << EdgeKindMaskBits) | k;
    }

    /// Compute the unique edgeFlag value from edge kind and call site Instruction.
    static inline GEdgeFlag makeEdgeFlagWithCallInst(GEdgeKind k, const ICFGNode* cs)
    {
        Inst2LabelMap::const_iterator iter = inst2LabelMap.find(cs);
        u64_t label = (iter != inst2LabelMap.end()) ?
                      iter->second : callEdgeLabelCounter++;
        return (label << EdgeKindMaskBits) | k;
    }

    /// Compute the unique edgeFlag value from edge kind and store Instruction.
    /// Two store instructions may share the same StorePAGEdge
    static inline GEdgeFlag makeEdgeFlagWithStoreInst(GEdgeKind k, const ICFGNode* store)
    {
        Inst2LabelMap::const_iterator iter = inst2LabelMap.find(store);
        u64_t label = (iter != inst2LabelMap.end()) ?
                      iter->second : storeEdgeLabelCounter++;
        return (label << EdgeKindMaskBits) | k;
    }

    virtual const std::string toString() const;

    //@}
    /// Overloading operator << for dumping SVFVar value
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const SVFStmt &edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    typedef GenericNode<SVFVar,SVFStmt>::GEdgeSetTy SVFStmtSetTy;
    typedef Map<EdgeID, SVFStmtSetTy> PAGEdgeToSetMapTy;
    typedef PAGEdgeToSetMapTy KindToSVFStmtMapTy;
    typedef SVFStmtSetTy PAGEdgeSetTy;

private:
    typedef Map<const ICFGNode*, u32_t> Inst2LabelMap;
    typedef Map<const SVFVar*, u32_t> Var2LabelMap;
    static Inst2LabelMap inst2LabelMap; ///< Call site Instruction to label map
    static Var2LabelMap var2LabelMap;  ///< Second operand of MultiOpndStmt to label map
    static u64_t callEdgeLabelCounter;  ///< Call site Instruction counter
    static u64_t storeEdgeLabelCounter;  ///< Store Instruction counter
    static u64_t multiOpndLabelCounter;  ///< MultiOpndStmt counter
};


/*!
 * Address statement (memory allocations)
 */
class AddrStmt: public SVFStmt
{
private:
    AddrStmt();                      ///< place holder
    AddrStmt(const AddrStmt &);  ///< place holder
    void operator=(const AddrStmt &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr;
    }
    //@}

    /// constructor
    AddrStmt(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Addr)
    {
    }

    virtual const std::string toString() const override;
};


/*!
 * Copy statements (simple assignment and casting)
 */
class CopyStmt: public SVFStmt
{
private:
    CopyStmt();                      ///< place holder
    CopyStmt(const CopyStmt &);  ///< place holder
    void operator=(const CopyStmt &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Copy;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Copy;
    }
    //@}

    /// constructor
    CopyStmt(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Copy)
    {
    }

    virtual const std::string toString() const override;
};

/*
* Program statements with multiple operands including BinaryOPStmt, CmpStmt and PhiStmt
*/
class MultiOpndStmt : public SVFStmt{
public:
    typedef std::vector<SVFVar*> OPVars;
protected:
    OPVars opVars;
public:
    /// Constructor, only used by subclassess but not external users
    MultiOpndStmt(SVFVar* r, const OPVars& opnds, GEdgeFlag k): SVFStmt(opnds.at(0), r, k), opVars(opnds)
    {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MultiOpndStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *node)
    {
        return node->getEdgeKind() == Phi ||
               node->getEdgeKind() == BinaryOp ||
               node->getEdgeKind() == Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy *node)
    {
        return node->getEdgeKind() == Phi ||
               node->getEdgeKind() == BinaryOp ||
               node->getEdgeKind() == Cmp;
    }
    //@}
    /// Operands and result at a BinaryNode e.g., p = q + r, `p` is resVar and `r` is OpVar
    //@{
    /// Operand SVFVars
    inline const SVFVar* getOpVar(u32_t pos) const
    {
        return opVars.at(pos);
    }
    /// Result SVFVar
    inline const SVFVar* getRes() const
    {
        return getDstNode();
    }
    
    NodeID getOpVarID(u32_t pos) const;
    NodeID getResID() const;

    inline u32_t getOpVarNum() const
    {
        return opVars.size();
    }
    inline const OPVars& getOpndVars() const{
        return opVars;
    }
    inline OPVars::const_iterator opVarBegin() const
    {
        return opVars.begin();
    }
    inline OPVars::const_iterator opVerEnd() const
    {
        return opVars.end();
    }
    //@}
};

/*!
 * Phi statement (e.g., p = phi(q,r) which receives values from variables q and r from different paths)
 * it is typically at a joint point of the control-flow graph
 */
class PhiStmt: public MultiOpndStmt
{
private:
    PhiStmt();                      ///< place holder
    PhiStmt(const PhiStmt &);  ///< place holder
    void operator=(const PhiStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< place holder, not allowed since this SVFStmt has multiple operands but not a single source
    SVFVar* getDstNode();    ///< place holder, use getRes() instead
    SVFVar* getSrcID();  ///< place holder, use getOpVarID(pos) instead
    SVFVar* getDstID();    ///< place holder, use getResID() instead

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const PhiStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    static inline bool classof(const MultiOpndStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    //@}

    /// constructor
    PhiStmt(SVFVar* s, const OPVars& opnds) : MultiOpndStmt(s,opnds,SVFStmt::Phi)
    {
    }
    void addOpVar(SVFVar* op){
        opVars.push_back(op);
    }
    virtual const std::string toString() const override;
};


/*!
 * Comparison statement
 */
class CmpStmt: public MultiOpndStmt
{
private:
    CmpStmt();                      ///< place holder
    CmpStmt(const CmpStmt &);  ///< place holder
    void operator=(const CmpStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< place holder, not allowed since this SVFStmt has multiple operands but not a single source
    SVFVar* getDstNode();    ///< place holder, use getRes() instead
    SVFVar* getSrcID();  ///< place holder, use getOpVarID(pos) instead
    SVFVar* getDstID();    ///< place holder, use getResID() instead

    u32_t predicate;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CmpStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    static inline bool classof(const MultiOpndStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    //@}

    /// constructor
    CmpStmt(SVFVar* s, const OPVars& opnds, u32_t pre) : MultiOpndStmt(s,opnds,SVFStmt::Cmp), predicate(pre)
    {
    }

    u32_t getPredicate() const
    {
        return predicate;
    }
    
    virtual const std::string toString() const override;
};


/*!
 * Binary statement
 */
class BinaryOPStmt: public MultiOpndStmt
{
private:
    BinaryOPStmt();                      ///< place holder
    BinaryOPStmt(const BinaryOPStmt &);  ///< place holder
    void operator=(const BinaryOPStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< place holder, not allowed since this SVFStmt has multiple operands but not a single source
    SVFVar* getDstNode();    ///< place holder, use getRes() instead
    SVFVar* getSrcID();  ///< place holder, use getOpVarID(pos) instead
    SVFVar* getDstID();    ///< place holder, use getResID() instead

    u32_t opcode;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BinaryOPStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    static inline bool classof(const MultiOpndStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    //@}

    /// constructor
    BinaryOPStmt(SVFVar* s, const OPVars& opnds, u32_t oc) : MultiOpndStmt(s,opnds,SVFStmt::BinaryOp), opcode(oc)
    {
    }

    u32_t getOpcode() const
    {
        return opcode;
    }

    virtual const std::string toString() const override;
};

/*!
 * Unary statement
 */
class UnaryOPStmt: public SVFStmt
{
private:
    UnaryOPStmt();                      ///< place holder
    UnaryOPStmt(const UnaryOPStmt &);  ///< place holder
    void operator=(const UnaryOPStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< place holder, use getOpVar() instead
    SVFVar* getDstNode();    ///< place holder, use getRes() instead
    SVFVar* getSrcID();  ///< place holder, use getOpVarID() instead
    SVFVar* getDstID();    ///< place holder, use getResID() instead

    u32_t opcode;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const UnaryOPStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::UnaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::UnaryOp;
    }
    //@}

    /// constructor
    UnaryOPStmt(SVFVar* s, SVFVar* d, u32_t oc) : SVFStmt(s,d,SVFStmt::UnaryOp), opcode(oc)
    {
    }

    u32_t getOpcode() const
    {
        return opcode;
    }
    inline const SVFVar* getOpVar() const{
        return SVFStmt::getSrcNode();
    }
    inline const SVFVar* getRes() const{
        return SVFStmt::getDstNode();
    }
    NodeID getOpVarID() const;
    NodeID getResID() const;

    virtual const std::string toString() const override;
};


/*!
 * Branch statements including if/else and switch 
 */
class BranchStmt: public SVFStmt
{
private:
    BranchStmt();                      ///< place holder
    BranchStmt(const BranchStmt &);  ///< place holder
    void operator=(const BranchStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< place holder, not allowed 
    SVFVar* getDstNode();    ///< place holder, not allowed 
    SVFVar* getSrcID();  ///< place holder, not allowed 
    SVFVar* getDstID();    ///< place holder, not allowed 

    std::vector<const ICFGNode*> successors;
    const SVFVar* cond;
    const SVFVar* brInst;
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BranchStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Branch;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Branch;
    }
    //@}

    /// constructor
    BranchStmt(SVFVar* inst, SVFVar* c, const std::vector<const ICFGNode*>& succs) : SVFStmt(c,inst,SVFStmt::Branch), successors(succs), cond(c), brInst(inst)
    {
    }

    /// The branch is unconditional if cond is a null value
    bool isUnconditional() const;
    /// The branch is conditional if cond is not a null value
    bool isConditional() const;
    /// Return the condition 
    const SVFVar* getCondition() const;
    const SVFVar* getBranchInst() const{
        return brInst;
    }
    /// Successors of this branch statement
    ///@{
    u32_t getNumSuccessors() const{
        return successors.size();
    }
    const std::vector<const ICFGNode*>& getSuccessors() const{
        return successors;
    }
    const ICFGNode* getSuccessor (u32_t i) const{
        return successors.at(i);
    }
    ///@}
    virtual const std::string toString() const override;
};

/*!
 * Store statement
 */
class StoreStmt: public SVFStmt
{
private:
    StoreStmt();                      ///< place holder
    StoreStmt(const StoreStmt &);  ///< place holder
    void operator=(const StoreStmt &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Store;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Store;
    }
    //@}

    /// constructor
    StoreStmt(SVFVar* s, SVFVar* d, const IntraBlockNode* st) :
        SVFStmt(s, d, makeEdgeFlagWithStoreInst(SVFStmt::Store, st))
    {
    }

    virtual const std::string toString() const override;
};


/*!
 * Load statement
 */
class LoadStmt: public SVFStmt
{
private:
    LoadStmt();                      ///< place holder
    LoadStmt(const LoadStmt &);  ///< place holder
    void operator=(const LoadStmt &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Load;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Load;
    }
    //@}

    /// constructor
    LoadStmt(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Load)
    {
    }

    virtual const std::string toString() const override;
};


/*!
 * Gep statement for struct field access, array access and pointer arithmetic
 */
class GepStmt: public SVFStmt
{
private:
    GepStmt();                      ///< place holder
    GepStmt(const GepStmt &);  ///< place holder
    void operator=(const GepStmt &); ///< place holder

    LocationSet ls;	///< location set of the gep edge

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepStmt *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return 	edge->getEdgeKind() == SVFStmt::NormalGep ||
                edge->getEdgeKind() == SVFStmt::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return	edge->getEdgeKind() == SVFStmt::NormalGep ||
                edge->getEdgeKind() == SVFStmt::VariantGep;
    }
    //@}

    inline const LocationSet& getLocationSet() const
    {
        return ls;
    }

protected:
    /// constructor
    GepStmt(SVFVar* s, SVFVar* d, const LocationSet& l, PEDGEK k) : SVFStmt(s,d,k), ls(l)
    {
    }

    virtual const std::string toString() const;

};


/*!
 * Gep statement with a fixed field index for struct field access 
 */
class NormalGepStmt : public GepStmt
{
private:
    NormalGepStmt(); ///< place holder
    NormalGepStmt(const NormalGepStmt&); ///< place holder
    void operator=(const NormalGepStmt&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NormalGepStmt *)
    {
        return true;
    }
    static inline bool classof(const GepStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::NormalGep;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::NormalGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::NormalGep;
    }
    //@}

    /// constructor
    NormalGepStmt(SVFVar* s, SVFVar* d, const LocationSet& l) : GepStmt(s,d,l, SVFStmt::NormalGep)
    {}

    /// offset of the gep edge
    inline u32_t getFieldOffset() const
    {
        return getLocationSet().accumulateConstantFieldIdx();
    }

    virtual const std::string toString() const override;
};

/*!
 * Gep statement with a variant field index for struct field access 
 */
class VariantGepStmt : public GepStmt
{
private:
    VariantGepStmt(); ///< place holder
    VariantGepStmt(const VariantGepStmt&); ///< place holder
    void operator=(const VariantGepStmt&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const VariantGepStmt *)
    {
        return true;
    }
    static inline bool classof(const GepStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::VariantGep;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::VariantGep;
    }
    //@}

    /// constructor
    VariantGepStmt(SVFVar* s, SVFVar* d, const LocationSet& l) : GepStmt(s,d,l, SVFStmt::VariantGep) {}

    virtual const std::string toString() const override;

};


/*!
 * Call 
 */
class CallPE: public SVFStmt
{
private:
    CallPE();                      ///< place holder
    CallPE(const CallPE &);  ///< place holder
    void operator=(const CallPE &); ///< place holder

    const CallBlockNode* inst;		///< llvm instruction for this call
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Call
        	|| edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Call
        	|| edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    //@}

    /// constructor
    CallPE(SVFVar* s, SVFVar* d, const CallBlockNode* i, GEdgeKind k = SVFStmt::Call) :
        SVFStmt(s,d,makeEdgeFlagWithCallInst(k,i)), inst(i)
    {
    }

    /// Get method for the call instruction
    //@{
    inline const CallBlockNode* getCallInst() const
    {
        return inst;
    }
    inline const CallBlockNode* getCallSite() const
    {
        return inst;
    }
    //@}

    virtual const std::string toString() const override;
};


/*!
 * Return 
 */
class RetPE: public SVFStmt
{
private:
    RetPE();                      ///< place holder
    RetPE(const RetPE &);  ///< place holder
    void operator=(const RetPE &); ///< place holder

    const CallBlockNode* inst;		/// the callsite instruction return to
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Ret
			|| edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Ret
    		|| edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    //@}

    /// constructor
    RetPE(SVFVar* s, SVFVar* d, const CallBlockNode* i, GEdgeKind k = SVFStmt::Ret) :
        SVFStmt(s,d,makeEdgeFlagWithCallInst(k,i)), inst(i)
    {
    }

    /// Get method for call instruction at caller
    //@{
    inline const CallBlockNode* getCallInst() const
    {
        return inst;
    }
    inline const CallBlockNode* getCallSite() const
    {
        return inst;
    }
    //@}

    virtual const std::string toString() const override;
};


/*!
 * Thread Fork 
 */
class TDForkPE: public CallPE
{
private:
    TDForkPE();                      ///< place holder
    TDForkPE(const TDForkPE &);  ///< place holder
    void operator=(const TDForkPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDForkPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    //@}

    /// constructor
    TDForkPE(SVFVar* s, SVFVar* d, const CallBlockNode* i) :
        CallPE(s,d,i,SVFStmt::ThreadFork)
    {
    }

    virtual const std::string toString() const;
};



/*!
 * Thread Join 
 */
class TDJoinPE: public RetPE
{
private:
    TDJoinPE();                      ///< place holder
    TDJoinPE(const TDJoinPE &);  ///< place holder
    void operator=(const TDJoinPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDJoinPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    //@}

    /// Constructor
    TDJoinPE(SVFVar* s, SVFVar* d, const CallBlockNode* i) :
        RetPE(s,d,i,SVFStmt::ThreadJoin)
    {
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* PAGEDGE_H_ */
