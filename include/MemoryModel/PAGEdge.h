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

#include "MemoryModel/MemModel.h"
#include "MemoryModel/GenericGraph.h"
#include "Util/AnalysisUtil.h"

#include <llvm/IR/CallSite.h>	// for callsite
#include <llvm/ADT/STLExtras.h>			// for mapped_iter
#include <llvm/ADT/DepthFirstIterator.h>		// for depth first iterator, inverse iter
#include <llvm/ADT/StringExtras.h>	// for utostr_32


class PAGNode;

/*
 * PAG edge between nodes
 */
typedef GenericEdge<PAGNode> GenericPAGEdgeTy;
class PAGEdge : public GenericPAGEdgeTy {

public:
    /// Ten kinds of PAG edges
    /// Gep represents offset edge for field sensitivity
    /// ThreadFork/ThreadJoin is to model parameter passings between thread spawners and spawnees.
    enum PEDGEK {
        Addr, Copy, Store, Load, Call, Ret, NormalGep, VariantGep, ThreadFork, ThreadJoin
    };

private:
    const llvm::Value* value;	///< LLVM value
    const llvm::BasicBlock *basicBlock;   ///< LLVM BasicBlock
    EdgeID edgeId;					///< Edge ID
public:
    static Size_t totalEdgeNum;		///< Total edge number

    /// Constructor
    PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k);
    /// Destructor
    ~PAGEdge() {
    }

    /// ClassOf
    //@{
    static inline bool classof(const PAGEdge *edge) {
        return true;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Addr ||
               edge->getEdgeKind() == PAGEdge::Copy ||
               edge->getEdgeKind() == PAGEdge::Store ||
               edge->getEdgeKind() == PAGEdge::Load ||
               edge->getEdgeKind() == PAGEdge::Call ||
               edge->getEdgeKind() == PAGEdge::Ret ||
               edge->getEdgeKind() == PAGEdge::NormalGep ||
               edge->getEdgeKind() == PAGEdge::VariantGep ||
               edge->getEdgeKind() == PAGEdge::ThreadFork ||
               edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    ///@}

    /// Return Edge ID
    inline EdgeID getEdgeID() const {
        return edgeId;
    }

    /// Get/set methods for llvm instruction
    //@{
    inline const llvm::Instruction* getInst() const {
        return llvm::dyn_cast<llvm::Instruction>(value);
    }
    inline void setValue(const llvm::Value *val) {
        value = val;
    }
    inline const llvm::Value* getValue() const {
        return value;
    }
    inline void setBB(const llvm::BasicBlock *bb) {
        basicBlock = bb;
    }
    inline const llvm::BasicBlock* getBB() const {
        return basicBlock;
    }
    //@}

    /// Compute the unique edgeFlag value from edge kind and call site Instruction.
    static inline GEdgeFlag makeEdgeFlagWithCallInst(GEdgeKind k, const llvm::Instruction* cs) {
        Inst2LabelMap::const_iterator iter = inst2LabelMap.find(cs);
        u64_t label = (iter != inst2LabelMap.end()) ?
                      iter->second : callEdgeLabelCounter++;
        return (label << EdgeKindMaskBits) | k;
    }

    typedef GenericNode<PAGNode,PAGEdge>::GEdgeSetTy PAGEdgeSetTy;
    typedef llvm::DenseMap<EdgeID, PAGEdgeSetTy> PAGEdgeToSetMapTy;
    typedef PAGEdgeToSetMapTy PAGKindToEdgeSetMapTy;

private:
    typedef llvm::DenseMap<const llvm::Instruction*, u32_t> Inst2LabelMap;
    static Inst2LabelMap inst2LabelMap; ///< Call site Instruction to label map
    static u64_t callEdgeLabelCounter;  ///< Call site Instruction counter
};



/*!
 * Copy edge
 */
class AddrPE: public PAGEdge {
private:
    AddrPE();                      ///< place holder
    AddrPE(const AddrPE &);  ///< place holder
    void operator=(const AddrPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Addr;
    }
    //@}

    /// constructor
    AddrPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Addr) {
    }
};


/*!
 * Copy edge
 */
class CopyPE: public PAGEdge {
private:
    CopyPE();                      ///< place holder
    CopyPE(const CopyPE &);  ///< place holder
    void operator=(const CopyPE &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Copy;
    }
    //@}

    /// constructor
    CopyPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Copy) {
    }
};


/*!
 * Store edge
 */
class StorePE: public PAGEdge {
private:
    StorePE();                      ///< place holder
    StorePE(const StorePE &);  ///< place holder
    void operator=(const StorePE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StorePE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Store;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Store;
    }
    //@}

    /// constructor
    StorePE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Store) {
    }
};


/*!
 * Load edge
 */
class LoadPE: public PAGEdge {
private:
    LoadPE();                      ///< place holder
    LoadPE(const LoadPE &);  ///< place holder
    void operator=(const LoadPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Load;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Load;
    }
    //@}

    /// constructor
    LoadPE(PAGNode* s, PAGNode* d) : PAGEdge(s,d,PAGEdge::Load) {
    }
};


/*!
 * Gep edge
 */
class GepPE: public PAGEdge {
private:
    GepPE();                      ///< place holder
    GepPE(const GepPE &);  ///< place holder
    void operator=(const GepPE &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return 	edge->getEdgeKind() == PAGEdge::NormalGep ||
                edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return	edge->getEdgeKind() == PAGEdge::NormalGep ||
                edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    //@}

protected:
    /// constructor
    GepPE(PAGNode* s, PAGNode* d, PEDGEK k) : PAGEdge(s,d,k) {

    }
};


/*!
 * Gep edge with a fixed offset
 */
class NormalGepPE : public GepPE {
private:
    NormalGepPE(); ///< place holder
    NormalGepPE(const NormalGepPE&); ///< place holder
    void operator=(const NormalGepPE&); ///< place holder

    LocationSet ls;	///< location set of the gep edge

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NormalGepPE *) {
        return true;
    }
    static inline bool classof(const GepPE *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::NormalGep;
    }
    //@}

    /// constructor
    NormalGepPE(PAGNode* s, PAGNode* d, const LocationSet& l) : GepPE(s,d,PAGEdge::NormalGep), ls(l)
    {}

    /// offset of the gep edge
    inline u32_t getOffset() const {
        return ls.getOffset();
    }
    inline const LocationSet& getLocationSet() const {
        return ls;
    }
};

/*!
 * Gep edge with a variant offset
 */
class VariantGepPE : public GepPE {
private:
    VariantGepPE(); ///< place holder
    VariantGepPE(const VariantGepPE&); ///< place holder
    void operator=(const VariantGepPE&); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const VariantGepPE *) {
        return true;
    }
    static inline bool classof(const GepPE *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::VariantGep;
    }
    //@}

    /// constructor
    VariantGepPE(PAGNode* s, PAGNode* d) : GepPE(s,d,PAGEdge::VariantGep) {}
};


/*!
 * Call edge
 */
class CallPE: public PAGEdge {
private:
    CallPE();                      ///< place holder
    CallPE(const CallPE &);  ///< place holder
    void operator=(const CallPE &); ///< place holder

    const llvm::Instruction* inst;		///< llvm instruction for this call
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Call;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Call;
    }
    //@}

    /// constructor
    CallPE(PAGNode* s, PAGNode* d, const llvm::Instruction* i) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(PAGEdge::Call,i)), inst(i) {
    }

    /// Get method for the call instruction
    //@{
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
    //@}
};


/*!
 * Return edge
 */
class RetPE: public PAGEdge {
private:
    RetPE();                      ///< place holder
    RetPE(const RetPE &);  ///< place holder
    void operator=(const RetPE &); ///< place holder

    const llvm::Instruction* inst;		/// the callsite instruction return to
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::Ret;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::Ret;
    }
    //@}

    /// constructor
    RetPE(PAGNode* s, PAGNode* d, const llvm::Instruction* i) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(PAGEdge::Ret,i)), inst(i) {
    }

    /// Get method for call instruction at caller
    //@{
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
    //@}
};


/*!
 * Thread Fork Edge
 */
class TDForkPE: public PAGEdge {
private:
    TDForkPE();                      ///< place holder
    TDForkPE(const TDForkPE &);  ///< place holder
    void operator=(const TDForkPE &); ///< place holder

    const llvm::Instruction* inst;		///< llvm instruction at the fork site
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDForkPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadFork;
    }
    //@}

    /// constructor
    TDForkPE(PAGNode* s, PAGNode* d, const llvm::Instruction* i) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(PAGEdge::ThreadFork,i)), inst(i) {
    }

    /// Get method for the instruction at the fork site
    //@{
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
    //@}
};



/*!
 * Thread Join Edge
 */
class TDJoinPE: public PAGEdge {
private:
    TDJoinPE();                      ///< place holder
    TDJoinPE(const TDJoinPE &);  ///< place holder
    void operator=(const TDJoinPE &); ///< place holder

    const llvm::Instruction* inst;		/// the callsite instruction return to
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TDJoinPE *) {
        return true;
    }
    static inline bool classof(const PAGEdge *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    static inline bool classof(const GenericPAGEdgeTy *edge) {
        return edge->getEdgeKind() == PAGEdge::ThreadJoin;
    }
    //@}

    /// Constructor
    TDJoinPE(PAGNode* s, PAGNode* d, const llvm::Instruction* i) :
        PAGEdge(s,d,makeEdgeFlagWithCallInst(PAGEdge::ThreadJoin,i)), inst(i) {
    }

    /// Get method for the instruction at the join site
    //@{
    inline const llvm::Instruction* getCallInst() const {
        return inst;
    }
    inline llvm::CallSite getCallSite() const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(getCallInst());
        return cs;
    }
    //@}
};
#endif /* PAGEDGE_H_ */
