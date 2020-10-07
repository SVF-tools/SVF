//===- PAGEdge.h -- PAG edge class-------------------------------------------//
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
 * PAGEdge.h
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

class PAGNode;

/*
 * PAG edge between nodes
 */
typedef GenericEdge<PAGNode> GenericPAGEdgeTy;
class PAGEdge : public GenericPAGEdgeTy
{

public:
    /// Thirteen kinds of PAG edges
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
    PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k);
    /// Destructor
    ~PAGEdge()
    {
    }

    /// ClassOf
    //@{
    static inline bool classof(const PAGEdge*)
    {
        return true;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Addr ||
               edge->getEdgeKind() == PAGEdge::Copy ||
               edge->getEdgeKind() == PAGEdge::Store ||
               edge->getEdgeKind() == PAGEdge::Load ||
               edge->getEdgeKind() == PAGEdge::Call ||
               edge->getEdgeKind() == PAGEdge::Ret ||
               edge->getEdgeKind() == PAGEdge::NormalGep ||
               edge->getEdgeKind() == PAGEdge::VariantGep ||
               edge->getEdgeKind() == PAGEdge::ThreadFork ||
               edge->getEdgeKind() == PAGEdge::ThreadJoin ||
               edge->getEdgeKind() == PAGEdge::Cmp ||
               edge->getEdgeKind() == PAGEdge::BinaryOp ||
               edge->getEdgeKind() == PAGEdge::UnaryOp;
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
    /// Overloading operator << for dumping PAGNode value
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const PAGEdge &edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    typedef GenericNode<PAGNode,PAGEdge>::GEdgeSetTy PAGEdgeSetTy;
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
class AddrPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }
    //@}

    /// constructor
    AddrPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Addr)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Copy edge
 */
class CopyPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }
    //@}

    /// constructor
    CopyPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Copy)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Compare instruction edge
 */
class CmpPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Cmp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Cmp;
    }
    //@}

    /// constructor
    CmpPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Cmp)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Binary instruction edge
 */
class BinaryOPPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::BinaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::BinaryOp;
    }
    //@}

    /// constructor
    BinaryOPPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::BinaryOp)
    {
    }

    virtual const std::string toString() const;
};

/*!
 * Unary instruction edge
 */
class UnaryOPPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::UnaryOp;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::UnaryOp;
    }
    //@}

    /// constructor
    UnaryOPPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::UnaryOp)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Store edge
 */
class StorePE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Store;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Store;
    }
    //@}

    /// constructor
    StorePE(PAGNode* s, PAGNode* d, const IntraBlockNode* st) :
        PAGEdge(s, d, makeEdgeFlagWithStoreInst(PAGEdge::Store, st))
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Load edge
 */
class LoadPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Load;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Load;
    }
    //@}

    /// constructor
    LoadPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Load)
    {
    }

    virtual const std::string toString() const;
};


/*!
 * Gep edge
 */
class GepPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return 	edge->getEdgeKind() == PAGEdge::NormalGep ||
                edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return	edge->getEdgeKind() == PAGEdge::NormalGep ||
                edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    //@}

protected:
    /// constructor
    GepPE(PAGNode* s, PAGNode* d, PEDGEK k) : PAGEdge(s,d,k)
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
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    //@}

    /// constructor
    NormalGepPE(PAGNode* s, PAGNode* d, const LocationSet& l) : GepPE(s,d,PAGEdge::NormalGep), ls(l)
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
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    //@}

    /// constructor
    VariantGepPE(PAGNode* s, PAGNode* d) : GepPE(s,d,PAGEdge::VariantGep) {}

    virtual const std::string toString() const;

};


/*!
 * Call edge
 */
class CallPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Call
        	|| edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Call
        	|| edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    //@}

    /// constructor
    CallPE(PAGNode* s, PAGNode* d, const CallBlockNode* i, GEdgeKind k = PAGEdge::Call) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(k,i)), inst(i)
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
class RetPE: public PAGEdge
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Ret
			|| edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::Ret
    		|| edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    //@}

    /// constructor
    RetPE(PAGNode* s, PAGNode* d, const CallBlockNode* i, GEdgeKind k = PAGEdge::Ret) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(k,i)), inst(i)
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    //@}

    /// constructor
    TDForkPE(PAGNode* s, PAGNode* d, const CallBlockNode* i) :
        CallPE(s,d,i,PAGEdge::ThreadFork)
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
    static inline bool classof(const PAGEdge *edge)
    {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge)
    {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    //@}

    /// Constructor
    TDJoinPE(PAGNode* s, PAGNode* d, const CallBlockNode* i) :
        RetPE(s,d,i,PAGEdge::ThreadJoin)
    {
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* PAGEDGE_H_ */
