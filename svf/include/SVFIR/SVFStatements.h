//===- SVFStatements.h -- SVF statements-------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SVFStatements.h
 *
 *  Created on: Nov 10, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SVFSTATEMENT_H_
#define INCLUDE_SVFIR_SVFSTATEMENT_H_

#include "Graphs/GenericGraph.h"
#include "MemoryModel/AccessPath.h"

namespace SVF
{

class SVFVar;
class ICFGNode;
class IntraICFGNode;
class CallICFGNode;
class FunEntryICFGNode;
class FunExitICFGNode;

/*
 * SVFIR program statements (PAGEdges)
 */
typedef GenericEdge<SVFVar> GenericPAGEdgeTy;
class SVFStmt : public GenericPAGEdgeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    /// Types of SVFIR statements
    /// Gep represents (base + offset) for field sensitivity
    /// ThreadFork/ThreadJoin is to model parameter passings between thread
    /// spawners and spawnees.
    enum PEDGEK
    {
        Addr,
        Copy,
        Store,
        Load,
        Call,
        Ret,
        Gep,
        Phi,
        Select,
        Cmp,
        BinaryOp,
        UnaryOp,
        Branch,
        ThreadFork,
        ThreadJoin
    };

private:
    const SVFValue* value;           ///< LLVM value
    const SVFBasicBlock* basicBlock; ///< LLVM BasicBlock
    ICFGNode* icfgNode;              ///< ICFGNode
    EdgeID edgeId;                   ///< Edge ID

protected:
    /// Private constructor for reading SVFIR from file without side-effect
    SVFStmt(GEdgeFlag k)
        : GenericPAGEdgeTy({}, {}, k), value{}, basicBlock{}, icfgNode{}
    {
    }

public:
    static u32_t totalEdgeNum; ///< Total edge number

    /// Constructor
    SVFStmt(SVFVar* s, SVFVar* d, GEdgeFlag k, bool real = true);
    /// Destructor
    ~SVFStmt() {}

    /// ClassOf
    //@{
    static inline bool classof(const SVFStmt*)
    {
        return true;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr ||
               edge->getEdgeKind() == SVFStmt::Copy ||
               edge->getEdgeKind() == SVFStmt::Store ||
               edge->getEdgeKind() == SVFStmt::Load ||
               edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::Gep ||
               edge->getEdgeKind() == SVFStmt::Phi ||
               edge->getEdgeKind() == SVFStmt::Select ||
               edge->getEdgeKind() == SVFStmt::Cmp ||
               edge->getEdgeKind() == SVFStmt::BinaryOp ||
               edge->getEdgeKind() == SVFStmt::UnaryOp ||
               edge->getEdgeKind() == SVFStmt::Branch ||
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
    inline const SVFInstruction* getInst() const
    {
        if (const SVFInstruction* i = SVFUtil::dyn_cast<SVFInstruction>(value))
            return i;
        return nullptr;
    }
    inline void setValue(const SVFValue* val)
    {
        value = val;
    }
    inline const SVFValue* getValue() const
    {
        return value;
    }
    inline void setBB(const SVFBasicBlock* bb)
    {
        basicBlock = bb;
    }
    inline const SVFBasicBlock* getBB() const
    {
        return basicBlock;
    }
    inline void setICFGNode(ICFGNode* node)
    {
        icfgNode = node;
    }
    inline ICFGNode* getICFGNode() const
    {
        return icfgNode;
    }
    //@}

    /// Compute the unique edgeFlag value from edge kind and second variable
    /// operand for MultiOpndStmt.
    static inline GEdgeFlag makeEdgeFlagWithAddionalOpnd(GEdgeKind k,
            const SVFVar* var)
    {
        auto it_inserted = var2LabelMap.emplace(var, multiOpndLabelCounter);
        if (it_inserted.second)
            ++multiOpndLabelCounter;
        u64_t label = it_inserted.first->second;
        return (label << EdgeKindMaskBits) | k;
    }

    /// Compute the unique edgeFlag value from edge kind and call site
    /// Instruction.
    static inline GEdgeFlag makeEdgeFlagWithCallInst(GEdgeKind k,
            const ICFGNode* cs)
    {
        auto it_inserted = inst2LabelMap.emplace(cs, callEdgeLabelCounter);
        if (it_inserted.second)
            ++callEdgeLabelCounter;
        u64_t label = it_inserted.first->second;
        return (label << EdgeKindMaskBits) | k;
    }

    /// Compute the unique edgeFlag value from edge kind and store Instruction.
    /// Two store instructions may share the same StorePAGEdge
    static inline GEdgeFlag makeEdgeFlagWithStoreInst(GEdgeKind k,
            const ICFGNode* store)
    {
        auto it_inserted = inst2LabelMap.emplace(store, storeEdgeLabelCounter);
        if (it_inserted.second)
            ++storeEdgeLabelCounter;
        u64_t label = it_inserted.first->second;
        return (label << EdgeKindMaskBits) | k;
    }

    virtual const std::string toString() const;

    //@}
    /// Overloading operator << for dumping SVFVar value
    //@{
    friend OutStream& operator<<(OutStream& o, const SVFStmt& edge)
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

/*
 Parent class of Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, ThreadFork, ThreadJoin
 connecting RHS expression and LHS expression with an assignment  (e.g., LHSExpr = RHSExpr)
 Only one operand on the right handside of an assignment
*/
class AssignStmt : public SVFStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    AssignStmt();                      ///< place holder
    AssignStmt(const AssignStmt &);  ///< place holder
    void operator=(const AssignStmt &); ///< place holder
    SVFVar* getSrcNode();  ///< not allowed, use getRHSVar() instead
    SVFVar* getDstNode();    ///< not allowed, use getLHSVar() instead
    NodeID getSrcID();  ///< not allowed, use getRHSVarID() instead
    NodeID getDstID();    ///< not allowed, use getLHSVarID() instead

protected:
    /// constructor
    AssignStmt(SVFVar* s, SVFVar* d, GEdgeFlag k) : SVFStmt(s, d, k) {}
    /// Constructor to create empty AssignStmt (for SVFIRReader/serilization)
    AssignStmt(GEdgeFlag k) : SVFStmt(k) {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AssignStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr ||
               edge->getEdgeKind() == SVFStmt::Copy ||
               edge->getEdgeKind() == SVFStmt::Store ||
               edge->getEdgeKind() == SVFStmt::Load ||
               edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::Gep ||
               edge->getEdgeKind() == SVFStmt::ThreadFork ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr ||
               edge->getEdgeKind() == SVFStmt::Copy ||
               edge->getEdgeKind() == SVFStmt::Store ||
               edge->getEdgeKind() == SVFStmt::Load ||
               edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::Gep ||
               edge->getEdgeKind() == SVFStmt::ThreadFork ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    //@}

    inline SVFVar* getRHSVar() const
    {
        return SVFStmt::getSrcNode();
    }
    inline SVFVar* getLHSVar() const
    {
        return SVFStmt::getDstNode();
    }
    inline NodeID getRHSVarID() const
    {
        return SVFStmt::getSrcID();
    }
    inline NodeID getLHSVarID() const
    {
        return SVFStmt::getDstID();
    }

    virtual const std::string toString() const = 0;
};

/*!
 * Address statement (memory allocations)
 */
class AddrStmt: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty AddrStmt (for SVFIRReader/serilization)
    AddrStmt() : AssignStmt(SVFStmt::Addr) {}
    AddrStmt(const AddrStmt&);       ///< place holder
    void operator=(const AddrStmt&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Addr;
    }
    //@}

    /// constructor
    AddrStmt(SVFVar* s, SVFVar* d) : AssignStmt(s, d, SVFStmt::Addr) {}

    virtual const std::string toString() const override;
};

/*!
 * Copy statements (simple assignment and casting)
 */
class CopyStmt: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty CopyStmt (for SVFIRReader/serilization)
    CopyStmt() : AssignStmt(SVFStmt::Copy) {}
    CopyStmt(const CopyStmt&);       ///< place holder
    void operator=(const CopyStmt&); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Copy;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Copy;
    }
    //@}

    /// constructor
    CopyStmt(SVFVar* s, SVFVar* d) : AssignStmt(s, d, SVFStmt::Copy) {}

    virtual const std::string toString() const override;
};

/*!
 * Store statement
 */
class StoreStmt: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty StoreStmt (for SVFIRReader/serilization)
    StoreStmt() : AssignStmt(SVFStmt::Store) {}
    StoreStmt(const StoreStmt&);      ///< place holder
    void operator=(const StoreStmt&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Store;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Store;
    }
    //@}

    /// constructor
    StoreStmt(SVFVar* s, SVFVar* d, const IntraICFGNode* st);

    virtual const std::string toString() const override;
};

/*!
 * Load statement
 */
class LoadStmt: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty LoadStmt (for SVFIRReader/serilization)
    LoadStmt(): AssignStmt(SVFStmt::Load) {}
    LoadStmt(const LoadStmt&);       ///< place holder
    void operator=(const LoadStmt&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Load;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Load;
    }
    //@}

    /// constructor
    LoadStmt(SVFVar* s, SVFVar* d) : AssignStmt(s, d, SVFStmt::Load) {}

    virtual const std::string toString() const override;
};

/*!
 * Gep statement for struct field access, array access and pointer arithmetic
 */
class GepStmt: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty GepStmt (for SVFIRReader/serilization)
    GepStmt() : AssignStmt(SVFStmt::Gep) {}
    GepStmt(const GepStmt &);  ///< place holder
    void operator=(const GepStmt &); ///< place holder

    AccessPath ap;	///< Access path of the GEP edge
    bool variantField;  ///< Gep statement with a variant field index (pointer arithmetic) for struct field access (e.g., p = &(q + f), where f is a variable)
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return 	edge->getEdgeKind() == SVFStmt::Gep;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return	edge->getEdgeKind() == SVFStmt::Gep;
    }
    //@}

    inline const AccessPath& getAccessPath() const
    {
        return ap;
    }
    inline const AccessPath::OffsetVarAndGepTypePairs getOffsetVarAndGepTypePairVec() const
    {
        return getAccessPath().getOffsetVarAndGepTypePairVec();
    }
    /// Return TRUE if this is a constant location set.
    inline bool isConstantOffset() const
    {
        return getAccessPath().isConstantOffset();
    }
    /// Return accumulated constant offset (when accessing array or struct) if this offset is a constant.
    inline APOffset accumulateConstantOffset() const
    {
        return getAccessPath().computeConstantOffset();
    }
    /// Field index of the gep statement if it access the field of a struct
    inline APOffset getConstantFieldIdx() const
    {
        assert(isVariantFieldGep()==false && "Can't retrieve the AccessPath if using a variable field index (pointer arithmetic) for struct field access ");
        return getAccessPath().getConstantFieldIdx();
    }
    /// Gep statement with a variant field index (pointer arithmetic) for struct field access
    inline bool isVariantFieldGep() const
    {
        return variantField;
    }

    /// constructor
    GepStmt(SVFVar* s, SVFVar* d, const AccessPath& ap, bool varfld = false)
        : AssignStmt(s, d, SVFStmt::Gep), ap(ap), variantField(varfld)
    {
    }

    virtual const std::string toString() const;

};


/*!
 * Call
 */
class CallPE: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    CallPE(const CallPE&);         ///< place holder
    void operator=(const CallPE&); ///< place holder

    const CallICFGNode* call;      /// the callsite statement calling from
    const FunEntryICFGNode* entry; /// the function exit statement calling to
protected:
    /// Constructs empty CallPE (for SVFIRReader/serilization)
    CallPE(GEdgeFlag k = SVFStmt::Call) : AssignStmt(k), call{}, entry{} {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallPE*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Call ||
               edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    //@}

    /// constructor
    CallPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
           const FunEntryICFGNode* e, GEdgeKind k = SVFStmt::Call);

    /// Get method for the call instruction
    //@{
    inline const CallICFGNode* getCallInst() const
    {
        return call;
    }
    inline const CallICFGNode* getCallSite() const
    {
        return call;
    }
    inline const FunEntryICFGNode* getFunEntryICFGNode() const
    {
        return entry;
    }
    //@}

    virtual const std::string toString() const override;
};

/*!
 * Return
 */
class RetPE: public AssignStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    RetPE(const RetPE&);          ///< place holder
    void operator=(const RetPE&); ///< place holder

    const CallICFGNode* call;    /// the callsite statement returning to
    const FunExitICFGNode* exit; /// the function exit statement returned from

protected:
    /// Constructs empty RetPE (for SVFIRReader/serilization)
    RetPE(GEdgeFlag k = SVFStmt::Ret) : AssignStmt(k), call{}, exit{} {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetPE*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Ret ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    //@}

    /// constructor
    RetPE(SVFVar* s, SVFVar* d, const CallICFGNode* i, const FunExitICFGNode* e,
          GEdgeKind k = SVFStmt::Ret);

    /// Get method for call instruction at caller
    //@{
    inline const CallICFGNode* getCallInst() const
    {
        return call;
    }
    inline const CallICFGNode* getCallSite() const
    {
        return call;
    }
    inline const FunExitICFGNode* getFunExitICFGNode() const
    {
        return exit;
    }
    //@}

    virtual const std::string toString() const override;
};

/*
* Program statements with multiple operands including BinaryOPStmt, CmpStmt and PhiStmt
*/
class MultiOpndStmt : public SVFStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    typedef std::vector<SVFVar*> OPVars;

private:
    MultiOpndStmt();                      ///< place holder
    MultiOpndStmt(const MultiOpndStmt&);  ///< place holder
    void operator=(const MultiOpndStmt&); ///< place holder
    SVFVar* getSrcNode(); ///< not allowed, use getOpVar(idx) instead
    SVFVar* getDstNode(); ///< not allowed, use getRes() instead
    NodeID getSrcID();    ///< not allowed, use getOpVarID(idx) instead
    NodeID getDstID();    ///< not allowed, use getResID() instead

protected:
    OPVars opVars;
    /// Constructor, only used by subclassess but not external users
    MultiOpndStmt(SVFVar* r, const OPVars& opnds, GEdgeFlag k);
    /// Constructs empty MultiOpndStmt (for SVFIRReader/serilization)
    MultiOpndStmt(GEdgeFlag k) : SVFStmt(k) {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const MultiOpndStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* node)
    {
        return node->getEdgeKind() == Phi || node->getEdgeKind() == Select ||
               node->getEdgeKind() == BinaryOp || node->getEdgeKind() == Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy* node)
    {
        return node->getEdgeKind() == Phi || node->getEdgeKind() == Select ||
               node->getEdgeKind() == BinaryOp || node->getEdgeKind() == Cmp;
    }
    //@}
    /// Operands and result at a BinaryNode e.g., p = q + r, `p` is resVar and
    /// `r` is OpVar
    //@{
    /// Operand SVFVars
    inline const SVFVar* getOpVar(u32_t pos) const
    {
        return opVars.at(pos);
    }
    /// Result SVFVar
    inline const SVFVar* getRes() const
    {
        return SVFStmt::getDstNode();
    }

    NodeID getOpVarID(u32_t pos) const;
    NodeID getResID() const;

    inline u32_t getOpVarNum() const
    {
        return opVars.size();
    }
    inline const OPVars& getOpndVars() const
    {
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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    typedef std::vector<const ICFGNode*> OpICFGNodeVec;

private:
    /// Constructs empty PhiStmt (for SVFIRReader/serilization)
    PhiStmt() : MultiOpndStmt(SVFStmt::Phi) {}
    PhiStmt(const PhiStmt&);        ///< place holder
    void operator=(const PhiStmt&); ///< place holder

    OpICFGNodeVec opICFGNodes;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const PhiStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    static inline bool classof(const MultiOpndStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Phi;
    }
    //@}

    /// constructor
    PhiStmt(SVFVar* s, const OPVars& opnds, const OpICFGNodeVec& icfgNodes)
        : MultiOpndStmt(s, opnds, SVFStmt::Phi), opICFGNodes(icfgNodes)
    {
        assert(opnds.size() == icfgNodes.size() &&
               "Numbers of operands and their ICFGNodes are not consistent?");
    }
    void addOpVar(SVFVar* op, const ICFGNode* inode)
    {
        opVars.push_back(op);
        opICFGNodes.push_back(inode);
        assert(opVars.size() == opICFGNodes.size() &&
               "Numbers of operands and their ICFGNodes are not consistent?");
    }

    /// Return the corresponding ICFGNode of this operand
    inline const ICFGNode* getOpICFGNode(u32_t op_idx) const
    {
        return opICFGNodes.at(op_idx);
    }

    /// Return true if this is a phi at the function exit
    /// to receive one or multiple return values of this function
    bool isFunctionRetPhi() const;

    virtual const std::string toString() const override;
};

/*!
 * Select statement (e.g., p ? q: r which receives values from variables q and r based on condition p)
 */
class SelectStmt: public MultiOpndStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty SelectStmt (for SVFIRReader/serilization)
    SelectStmt() : MultiOpndStmt(SVFStmt::Select), condition{} {}
    SelectStmt(const SelectStmt&);     ///< place holder
    void operator=(const SelectStmt&); ///< place holder

    const SVFVar* condition;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const SelectStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Select;
    }
    static inline bool classof(const MultiOpndStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Select;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Select;
    }
    //@}

    /// constructor
    SelectStmt(SVFVar* s, const OPVars& opnds, const SVFVar* cond);
    virtual const std::string toString() const override;

    inline const SVFVar* getCondition() const
    {
        return condition;
    }
    inline const SVFVar* getTrueValue() const
    {
        return getOpVar(0);
    }
    inline const SVFVar* getFalseValue() const
    {
        return getOpVar(1);
    }
};

/*!
 * Comparison statement
 */
class CmpStmt: public MultiOpndStmt
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty CmpStmt (for SVFIRReader/serilization)
    CmpStmt() : MultiOpndStmt(SVFStmt::Cmp) {}
    CmpStmt(const CmpStmt&);        ///< place holder
    void operator=(const CmpStmt&); ///< place holder

    u32_t predicate;

public:
    /// OpCode for CmpStmt, enum value is same to llvm CmpInst
    enum Predicate : unsigned
    {
        // Opcode            U L G E    Intuitive operation
        FCMP_FALSE = 0, ///< 0 0 0 0    Always false (always folded)
        FCMP_OEQ = 1,   ///< 0 0 0 1    True if ordered and equal
        FCMP_OGT = 2,   ///< 0 0 1 0    True if ordered and greater than
        FCMP_OGE = 3,   ///< 0 0 1 1    True if ordered and greater than or equal
        FCMP_OLT = 4,   ///< 0 1 0 0    True if ordered and less than
        FCMP_OLE = 5,   ///< 0 1 0 1    True if ordered and less than or equal
        FCMP_ONE = 6,   ///< 0 1 1 0    True if ordered and operands are unequal
        FCMP_ORD = 7,   ///< 0 1 1 1    True if ordered (no nans)
        FCMP_UNO = 8,   ///< 1 0 0 0    True if unordered: isnan(X) | isnan(Y)
        FCMP_UEQ = 9,   ///< 1 0 0 1    True if unordered or equal
        FCMP_UGT = 10,  ///< 1 0 1 0    True if unordered or greater than
        FCMP_UGE = 11,  ///< 1 0 1 1    True if unordered, greater than, or equal
        FCMP_ULT = 12,  ///< 1 1 0 0    True if unordered or less than
        FCMP_ULE = 13,  ///< 1 1 0 1    True if unordered, less than, or equal
        FCMP_UNE = 14,  ///< 1 1 1 0    True if unordered or not equal
        FCMP_TRUE = 15, ///< 1 1 1 1    Always true (always folded)
        FIRST_FCMP_PREDICATE = FCMP_FALSE,
        LAST_FCMP_PREDICATE = FCMP_TRUE,
        BAD_FCMP_PREDICATE = FCMP_TRUE + 1,
        ICMP_EQ = 32,  ///< equal
        ICMP_NE = 33,  ///< not equal
        ICMP_UGT = 34, ///< unsigned greater than
        ICMP_UGE = 35, ///< unsigned greater or equal
        ICMP_ULT = 36, ///< unsigned less than
        ICMP_ULE = 37, ///< unsigned less or equal
        ICMP_SGT = 38, ///< signed greater than
        ICMP_SGE = 39, ///< signed greater or equal
        ICMP_SLT = 40, ///< signed less than
        ICMP_SLE = 41, ///< signed less or equal
        FIRST_ICMP_PREDICATE = ICMP_EQ,
        LAST_ICMP_PREDICATE = ICMP_SLE,
        BAD_ICMP_PREDICATE = ICMP_SLE + 1
    };

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CmpStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    static inline bool classof(const MultiOpndStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    //@}

    /// constructor
    CmpStmt(SVFVar* s, const OPVars& opnds, u32_t pre);

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
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty BinaryOPStmt (for SVFIRReader/serilization)
    BinaryOPStmt() : MultiOpndStmt(SVFStmt::BinaryOp) {}
    BinaryOPStmt(const BinaryOPStmt&);   ///< place holder
    void operator=(const BinaryOPStmt&); ///< place holder
    u32_t opcode;

public:
    /// OpCode for BinaryOPStmt, enum value is same to llvm BinaryOperator
    enum OpCode : unsigned
    {
        Add = 13,
        FAdd = 14,
        Sub = 15,
        FSub = 16,
        Mul = 17,
        FMul = 18,
        UDiv = 19,
        SDiv = 20,
        FDiv = 21,
        URem = 22,
        SRem = 23,
        FRem = 24,
        Shl = 25,
        LShr = 26,
        AShr = 27,
        And = 28,
        Or = 29,
        Xor = 30
    };

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BinaryOPStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    static inline bool classof(const MultiOpndStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    //@}

    /// constructor
    BinaryOPStmt(SVFVar* s, const OPVars& opnds, u32_t oc);

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
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty UnaryOPStmt (for SVFIRReader/serilization)
    UnaryOPStmt() : SVFStmt(SVFStmt::UnaryOp) {}
    UnaryOPStmt(const UnaryOPStmt&);    ///< place holder
    void operator=(const UnaryOPStmt&); ///< place holder
    SVFVar* getSrcNode(); ///< place holder, use getOpVar() instead
    SVFVar* getDstNode(); ///< place holder, use getRes() instead
    NodeID getSrcID();    ///< place holder, use getOpVarID(pos) instead
    NodeID getDstID();    ///< place holder, use getResID() instead

    u32_t opcode;

public:
    /// OpCode for UnaryOPStmt, enum value is same to llvm::UnaryOperator
    enum OpCode : unsigned
    {
        FNeg = 12
    };

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const UnaryOPStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::UnaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::UnaryOp;
    }
    //@}

    /// constructor
    UnaryOPStmt(SVFVar* s, SVFVar* d, u32_t oc)
        : SVFStmt(s, d, SVFStmt::UnaryOp), opcode(oc)
    {
    }

    u32_t getOpcode() const
    {
        return opcode;
    }
    inline const SVFVar* getOpVar() const
    {
        return SVFStmt::getSrcNode();
    }
    inline const SVFVar* getRes() const
    {
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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    typedef std::vector<std::pair<const ICFGNode*, s32_t>> SuccAndCondPairVec;

private:
    /// Constructs empty BranchStmt (for SVFIRReader/serilization)
    BranchStmt() : SVFStmt(SVFStmt::Branch), cond{}, brInst{} {}
    BranchStmt(const BranchStmt&);     ///< place holder
    void operator=(const BranchStmt&); ///< place holder
    SVFVar* getSrcNode();              ///< place holder, not allowed
    SVFVar* getDstNode();              ///< place holder, not allowed
    NodeID getSrcID(); ///< place holder, use getOpVarID(pos) instead
    NodeID getDstID(); ///< place holder, use getResID() instead

    SuccAndCondPairVec successors;
    const SVFVar* cond;
    const SVFVar* brInst;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BranchStmt*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Branch;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::Branch;
    }
    //@}

    /// constructor
    BranchStmt(SVFVar* inst, SVFVar* c, const SuccAndCondPairVec& succs)
        : SVFStmt(c, inst, SVFStmt::Branch), successors(succs), cond(c),
          brInst(inst)
    {
    }

    /// The branch is unconditional if cond is a null value
    bool isUnconditional() const;
    /// The branch is conditional if cond is not a null value
    bool isConditional() const;
    /// Return the condition
    const SVFVar* getCondition() const;
    const SVFVar* getBranchInst() const
    {
        return brInst;
    }

    /// For example if(c) {stmt1} else {stmt2}
    /// successor(0): stmt1, 1
    /// successor(1): stmt2, 0

    /// For example switch(c) case 0: {stmt1; break;} case 1: {stmt2; break;}
    /// default {stmt3: break} successor(0): stmt1, 0 successor(1): stmt2, 1
    /// successor(3): stmt3, -1

    /// Successors of this branch statement
    ///@{
    u32_t getNumSuccessors() const
    {
        return successors.size();
    }
    const SuccAndCondPairVec& getSuccessors() const
    {
        return successors;
    }
    const ICFGNode* getSuccessor(u32_t i) const
    {
        return successors.at(i).first;
    }
    s64_t getSuccessorCondValue(u32_t i) const
    {
        return successors.at(i).second;
    }
    //@}
    virtual const std::string toString() const override;
};

/*!
 * Thread Fork
 */
class TDForkPE: public CallPE
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty TDForkPE (for SVFIRReader/serilization)
    TDForkPE() : CallPE(SVFStmt::ThreadFork) {}
    TDForkPE(const TDForkPE&);       ///< place holder
    void operator=(const TDForkPE&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDForkPE*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadFork;
    }
    //@}

    /// constructor
    TDForkPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
             const FunEntryICFGNode* entry)
        : CallPE(s, d, i, entry, SVFStmt::ThreadFork)
    {
    }

    virtual const std::string toString() const;
};

/*!
 * Thread Join
 */
class TDJoinPE: public RetPE
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructs empty TDJoinPE (for SVFIRReader/serilization)
    TDJoinPE() : RetPE(SVFStmt::ThreadJoin) {}
    TDJoinPE(const TDJoinPE&);       ///< place holder
    void operator=(const TDJoinPE&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDJoinPE*)
    {
        return true;
    }
    static inline bool classof(const SVFStmt* edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy* edge)
    {
        return edge->getEdgeKind() == SVFStmt::ThreadJoin;
    }
    //@}

    /// Constructor
    TDJoinPE(SVFVar* s, SVFVar* d, const CallICFGNode* i,
             const FunExitICFGNode* e)
        : RetPE(s, d, i, e, SVFStmt::ThreadJoin)
    {
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* INCLUDE_SVFIR_SVFSTATEMENT_H_ */
