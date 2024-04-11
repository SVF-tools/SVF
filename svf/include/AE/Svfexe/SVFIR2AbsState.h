//===- SVFIR2AbsState.h -- SVF IR Translation to Interval Domain-----//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)
//===----------------------------------------------------------------------===//
/*
 * SVFIR2AbsState.h
 *
 *  Created on: Aug 7, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 *
 */

#ifndef Z3_EXAMPLE_SVFIR2ITVEXESTATE_H
#define Z3_EXAMPLE_SVFIR2ITVEXESTATE_H

#include "AE/Core/AbstractState.h"
#include "AE/Core/RelExeState.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{
class SVFIR2AbsState
{
public:
    static AbstractValue globalNulladdrs;
public:
    SVFIR2AbsState(SVFIR *ir) : _svfir(ir) {}

    void setEs(const AbstractState&es)
    {
        _es = es;
    }

    AbstractState& getAbsState()
    {
        return _es;
    }

    void setRelEs(const RelExeState &relEs)
    {
        _relEs = relEs;
    }

    RelExeState &getRelEs()
    {
        return _relEs;
    }

    void widenAddrs(AbstractState&lhs, const AbstractState&rhs);

    void narrowAddrs(AbstractState&lhs, const AbstractState&rhs);

    /// Return the field address given a pointer points to a struct object and an offset
    AbstractValue getGepObjAddress(u32_t pointer, APOffset offset);

    /// Return the value range of Integer SVF Type, e.g. unsigned i8 Type->[0, 255], signed i8 Type->[-128, 127]
    AbstractValue getRangeLimitFromType(const SVFType* type);

    AbstractValue getZExtValue(const SVFVar* var);
    AbstractValue getSExtValue(const SVFVar* var);
    AbstractValue getFPToSIntValue(const SVFVar* var);
    AbstractValue getFPToUIntValue(const SVFVar* var);
    AbstractValue getSIntToFPValue(const SVFVar* var);
    AbstractValue getUIntToFPValue(const SVFVar* var);
    AbstractValue getTruncValue(const SVFVar* var, const SVFType* dstType);
    AbstractValue getFPTruncValue(const SVFVar* var, const SVFType* dstType);

    /// Return the byte offset expression of a GepStmt
    /// elemBytesize is the element byte size of an static alloc or heap alloc array
    /// e.g. GepStmt* gep = [i32*10], x, and x is [0,3]
    /// std::pair<s32_t, s32_t> byteOffset = getByteOffset(gep);
    /// byteOffset should be [0, 12] since i32 is 4 bytes.
    AbstractValue getByteOffset(const GepStmt *gep);

    /// Return the offset expression of a GepStmt
    AbstractValue getItvOfFlattenedElemIndex(const GepStmt *gep);


    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    void applySummary(AbstractState&es);


    /// Init ObjVar
    void initObjVar(const ObjVar *objVar, u32_t varId);

    /// Init SVFVar
    void initSVFVar(u32_t varId);

    inline AbstractValue &getAddrs(u32_t id)
    {
        if (inVarToAddrsTable(id))
            return _es.getAddrs(id);
        else
            return globalNulladdrs;
    }


    /// whether the variable is in varToVal table
    inline bool inVarToValTable(u32_t id) const
    {
        return _es.inVarToValTable(id);
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(u32_t id) const
    {
        return _es.inVarToAddrsTable(id);
    }


    /// whether the memory address stores a interval value
    inline bool inLocToValTable(u32_t id) const
    {
        return _es.inLocToValTable(id);
    }

    /// whether the memory address stores memory addresses
    inline bool inLocToAddrsTable(u32_t id) const
    {
        return _es.inLocToAddrsTable(id);
    }

    void handleAddr(const AddrStmt *addr);

    void handleBinary(const BinaryOPStmt *binary);

    void handleCmp(const CmpStmt *cmp);

    void handleLoad(const LoadStmt *load);

    void handleStore(const StoreStmt *store);

    void handleCopy(const CopyStmt *copy);

    void handleCall(const CallPE *callPE);

    void handleRet(const RetPE *retPE);

    void handleGep(const GepStmt *gep);

    void handleSelect(const SelectStmt *select);

    void handlePhi(const PhiStmt *phi);

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return AbstractState::getInternalID(idx);
    }

    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx)
    {
        return AbstractState::getVirtualMemAddress(idx);
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    static inline bool isVirtualMemAddress(u32_t val)
    {
        return AbstractState::isVirtualMemAddress(val);
    }

protected:

    void handleBinaryRel(const BinaryOPStmt *binary);

    void handleCmpRel(const CmpStmt *cmp);

    void handleLoadRel(const LoadStmt *load);

    void handleStoreRel(const StoreStmt *store);

    void handleCopyRel(const CopyStmt *copy);

    void handleCallRel(const CallPE *callPE);

    void handleRetRel(const RetPE *retPE);

    void handleSelectRel(const SelectStmt *select);

    void handlePhiRel(const PhiStmt *phi, const ICFGNode *srcNode, const std::vector<const ICFGEdge *> &path);

private:
    SVFIR *_svfir;
    AbstractState _es;
    RelExeState _relEs;
};
}

#endif //Z3_EXAMPLE_SVFIR2ITVEXESTATE_H
