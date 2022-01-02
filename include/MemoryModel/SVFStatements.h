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
 * SVFIR edge between nodes
 */
typedef GenericEdge<SVFVar> GenericPAGEdgeTy;
class SVFStmt : public GenericPAGEdgeTy
{

public:
    /// Thirteen kinds of SVFIR edges
    /// Gep represents offset edge for field sensitivity
    /// ThreadFork/ThreadJoin is to model parameter passings between thread spawners and spawnees.
    enum PEDGEK
    {
        Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, ThreadFork, ThreadJoin, Cmp, BinaryOp, UnaryOp
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
               edge->getEdgeKind() == SVFStmt::ThreadFork ||
               edge->getEdgeKind() == SVFStmt::ThreadJoin ||
               edge->getEdgeKind() == SVFStmt::Cmp ||
               edge->getEdgeKind() == SVFStmt::BinaryOp ||
               edge->getEdgeKind() == SVFStmt::UnaryOp;
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

    typedef GenericNode<SVFVar,SVFStmt>::GEdgeSetTy PAGEdgeSetTy;
    typedef Map<EdgeID, PAGEdgeSetTy> PAGEdgeToSetMapTy;
    typedef PAGEdgeToSetMapTy PAGKindToEdgeSetMapTy;

private:
    typedef Map<const ICFGNode*, u32_t> Inst2LabelMap;
    static Inst2LabelMap inst2LabelMap; ///< Call site Instruction to label map
    static u64_t callEdgeLabelCounter;  ///< Call site Instruction counter
    static u64_t storeEdgeLabelCounter;  ///< Store Instruction counter
};



/*!
 * Copy edge
 */
class AddrPE: public SVFStmt
{
private:
    AddrPE();                      ///< place holder
    AddrPE(const AddrPE &);  ///< place holder
    void operator=(const AddrPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrPE *)
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
    AddrPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Addr)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Copy edge
 */
class CopyPE: public SVFStmt
{
private:
    CopyPE();                      ///< place holder
    CopyPE(const CopyPE &);  ///< place holder
    void operator=(const CopyPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyPE *)
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
    CopyPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Copy)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Compare instruction edge
 */
class CmpPE: public SVFStmt
{
private:
    CmpPE();                      ///< place holder
    CmpPE(const CmpPE &);  ///< place holder
    void operator=(const CmpPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CmpPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::Cmp;
    }
    //@}

    /// constructor
    CmpPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Cmp)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Binary instruction edge
 */
class BinaryOPPE: public SVFStmt
{
private:
    BinaryOPPE();                      ///< place holder
    BinaryOPPE(const BinaryOPPE &);  ///< place holder
    void operator=(const BinaryOPPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BinaryOPPE *)
    {
        return true;
    }
    static inline bool classof(const SVFStmt *edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == SVFStmt::BinaryOp;
    }
    //@}

    /// constructor
    BinaryOPPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::BinaryOp)
    {
    }

    virtual const std::string toString() const;
};

/*!
 * Unary instruction edge
 */
class UnaryOPPE: public SVFStmt
{
private:
    UnaryOPPE();                      ///< place holder
    UnaryOPPE(const UnaryOPPE &);  ///< place holder
    void operator=(const UnaryOPPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const UnaryOPPE *)
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
    UnaryOPPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::UnaryOp)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Store edge
 */
class StorePE: public SVFStmt
{
private:
    StorePE();                      ///< place holder
    StorePE(const StorePE &);  ///< place holder
    void operator=(const StorePE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StorePE *)
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
    StorePE(SVFVar* s, SVFVar* d, const IntraBlockNode* st) :
        SVFStmt(s, d, makeEdgeFlagWithStoreInst(SVFStmt::Store, st))
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Load edge
 */
class LoadPE: public SVFStmt
{
private:
    LoadPE();                      ///< place holder
    LoadPE(const LoadPE &);  ///< place holder
    void operator=(const LoadPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadPE *)
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
    LoadPE(SVFVar* s, SVFVar* d) : SVFStmt(s,d,SVFStmt::Load)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Gep edge
 */
class GepPE: public SVFStmt
{
private:
    GepPE();                      ///< place holder
    GepPE(const GepPE &);  ///< place holder
    void operator=(const GepPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepPE *)
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

protected:
    /// constructor
    GepPE(SVFVar* s, SVFVar* d, PEDGEK k) : SVFStmt(s,d,k)
    {

    }

    virtual const std::string toString() const;
};


/*!
 * Gep edge with a fixed offset
 */
class NormalGepPE : public GepPE
{
private:
    NormalGepPE(); ///< place holder
    NormalGepPE(const NormalGepPE&); ///< place holder
    void operator=(const NormalGepPE&); ///< place holder

    LocationSet ls;	///< location set of the gep edge

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NormalGepPE *)
    {
        return true;
    }
    static inline bool classof(const GepPE *edge)
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
    NormalGepPE(SVFVar* s, SVFVar* d, const LocationSet& l) : GepPE(s,d,SVFStmt::NormalGep), ls(l)
    {}

    /// offset of the gep edge
    inline u32_t getOffset() const
    {
        return ls.getOffset();
    }
    inline const LocationSet& getLocationSet() const
    {
        return ls;
    }

    virtual const std::string toString() const;
};

/*!
 * Gep edge with a variant offset
 */
class VariantGepPE : public GepPE
{
private:
    VariantGepPE(); ///< place holder
    VariantGepPE(const VariantGepPE&); ///< place holder
    void operator=(const VariantGepPE&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const VariantGepPE *)
    {
        return true;
    }
    static inline bool classof(const GepPE *edge)
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
    VariantGepPE(SVFVar* s, SVFVar* d) : GepPE(s,d,SVFStmt::VariantGep) {}

    virtual const std::string toString() const;

};


/*!
 * Call edge
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

    virtual const std::string toString() const;
};


/*!
 * Return edge
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

    virtual const std::string toString() const;
};


/*!
 * Thread Fork Edge
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
 * Thread Join Edge
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
