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


    void setRelEs(const RelExeState &relEs)
    {
        _relEs = relEs;
    }

    RelExeState &getRelEs()
    {
        return _relEs;
    }

    void widenAddrs(AbstractState& es, AbstractState&lhs, const AbstractState&rhs);

    void narrowAddrs(AbstractState& es, AbstractState&lhs, const AbstractState&rhs);

    /// Return the field address given a pointer points to a struct object and an offset
    AddressValue getGepObjAddress(AbstractState& es, u32_t pointer, APOffset offset);

    /// Return the value range of Integer SVF Type, e.g. unsigned i8 Type->[0, 255], signed i8 Type->[-128, 127]
    IntervalValue getRangeLimitFromType(const SVFType* type);

    IntervalValue getZExtValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getSExtValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getFPToSIntValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getFPToUIntValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getSIntToFPValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getUIntToFPValue(const AbstractState& es, const SVFVar* var);
    IntervalValue getTruncValue(const AbstractState& es, const SVFVar* var, const SVFType* dstType);
    IntervalValue getFPTruncValue(const AbstractState& es, const SVFVar* var, const SVFType* dstType);

    /// Return the byte offset expression of a GepStmt
    /// elemBytesize is the element byte size of an static alloc or heap alloc array
    /// e.g. GepStmt* gep = [i32*10], x, and x is [0,3]
    /// std::pair<s32_t, s32_t> byteOffset = getByteOffset(gep);
    /// byteOffset should be [0, 12] since i32 is 4 bytes.
    IntervalValue getByteOffset(const AbstractState& es, const GepStmt *gep);

    /// Return the offset expression of a GepStmt
    IntervalValue getElementIndex(const AbstractState& es, const GepStmt *gep);


    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    void applySummary(AbstractState&es);


    /// Init ObjVar
    void initObjVar(AbstractState& as, const ObjVar* var);


    inline AbstractValue &getAddrs(AbstractState& es, u32_t id)
    {
        if (inVarToAddrsTable(es, id))
            return es[id];
        else
            return globalNulladdrs;
    }

    inline bool inVarTable(const AbstractState& es, u32_t id) const
    {
        return es.inVarToValTable(id) || es.inVarToAddrsTable(id);
    }

    inline bool inAddrTable(const AbstractState& es, u32_t id) const
    {
        return es.inAddrToValTable(id) || es.inAddrToAddrsTable(id);
    }

    /// whether the variable is in varToVal table
    inline bool inVarToValTable(const AbstractState& es, u32_t id) const
    {
        return es.inVarToValTable(id);
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(const AbstractState& es, u32_t id) const
    {
        return es.inVarToAddrsTable(id);
    }


    /// whether the memory address stores a interval value
    inline bool inLocToValTable(const AbstractState& es, u32_t id) const
    {
        return es.inAddrToValTable(id);
    }

    /// whether the memory address stores memory addresses
    inline bool inLocToAddrsTable(const AbstractState& es, u32_t id) const
    {
        return es.inAddrToAddrsTable(id);
    }

    void handleAddr(AbstractState& es, const AddrStmt *addr);

    void handleBinary(AbstractState& es, const BinaryOPStmt *binary);

    void handleCmp(AbstractState& es, const CmpStmt *cmp);

    void handleLoad(AbstractState& es, const LoadStmt *load);

    void handleStore(AbstractState& es, const StoreStmt *store);

    void handleCopy(AbstractState& es, const CopyStmt *copy);

    void handleCall(AbstractState& es, const CallPE *callPE);

    void handleRet(AbstractState& es, const RetPE *retPE);

    void handleGep(AbstractState& es, const GepStmt *gep);

    void handleSelect(AbstractState& es, const SelectStmt *select);

    void handlePhi(AbstractState& es, const PhiStmt *phi);

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

private:
    SVFIR *_svfir;
    RelExeState _relEs;
};
}

#endif //Z3_EXAMPLE_SVFIR2ITVEXESTATE_H
