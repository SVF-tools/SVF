//===- SVFIR2ItvExeState.h -- SVF IR Translation to Interval Domain-----//
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
//===----------------------------------------------------------------------===//
/*
 * SVFIR2ItvExeState.h
 *
 *  Created on: Aug 7, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 *
 */

#ifndef Z3_EXAMPLE_SVFIR2ITVEXESTATE_H
#define Z3_EXAMPLE_SVFIR2ITVEXESTATE_H

#include "SVFIR/SVFIR.h"
#include "AbstractExecution/ExeState.h"
#include "AbstractExecution/IntervalExeState.h"
#include "AbstractExecution/IntervalValue.h"
#include "AbstractExecution/RelExeState.h"

namespace SVF
{
class SVFIR2ItvExeState
{
public:
    typedef ExeState::Addrs Addrs;
    static Addrs globalNulladdrs;
public:
    SVFIR2ItvExeState(SVFIR *ir) : _svfir(ir) {}

    void setEs(const IntervalExeState &es)
    {
        _es = es;
    }

    IntervalExeState &getEs()
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

    void widenAddrs(IntervalExeState &lhs, const IntervalExeState &rhs);

    void narrowAddrs(IntervalExeState &lhs, const IntervalExeState &rhs);

    /// Return the field address given a pointer points to a struct object and an offset
    Addrs getGepObjAddress(u32_t pointer, APOffset offset);

    /// Return the value range of Integer SVF Type, e.g. unsigned i8 Type->[0, 255], signed i8 Type->[-128, 127]
    IntervalValue getRangeLimitFromType(const SVFType* type);

    /// Return the byte offset expression of a GepStmt
    /// elemBytesize is the element byte size of an static alloc or heap alloc array
    /// e.g. GepStmt* gep = [i32*10], x, and x is [0,3]
    /// std::pair<s32_t, s32_t> byteOffset = getByteOffset(gep);
    /// byteOffset should be [0, 12] since i32 is 4 bytes.
    IntervalValue getByteOffset(const GepStmt *gep);

    /// Return the offset expression of a GepStmt
    IntervalValue getItvOfFlattenedElemIndex(const GepStmt *gep);


    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

    void applySummary(IntervalExeState &es);


    /// Init ObjVar
    void initObjVar(const ObjVar *objVar, u32_t varId);

    /// Init SVFVar
    void initSVFVar(u32_t varId);

    inline Addrs &getAddrs(u32_t id)
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

    void moveToGlobal();

    void translateAddr(const AddrStmt *addr);

    void translateBinary(const BinaryOPStmt *binary);

    void translateCmp(const CmpStmt *cmp);

    void translateLoad(const LoadStmt *load);

    void translateStore(const StoreStmt *store);

    void translateCopy(const CopyStmt *copy);

    void translateCall(const CallPE *callPE);

    void translateRet(const RetPE *retPE);

    void translateGep(const GepStmt *gep);

    void translateSelect(const SelectStmt *select);

    void translatePhi(const PhiStmt *phi);

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return ExeState::getInternalID(idx);
    }

    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx)
    {
        return ExeState::getVirtualMemAddress(idx);
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    static inline bool isVirtualMemAddress(u32_t val)
    {
        return ExeState::isVirtualMemAddress(val);
    }

protected:

    void translateBinaryRel(const BinaryOPStmt *binary);

    void translateCmpRel(const CmpStmt *cmp);

    void translateLoadRel(const LoadStmt *load);

    void translateStoreRel(const StoreStmt *store);

    void translateCopyRel(const CopyStmt *copy);

    void translateCallRel(const CallPE *callPE);

    void translateRetRel(const RetPE *retPE);

    void translateSelectRel(const SelectStmt *select);

    void translatePhiRel(const PhiStmt *phi, const ICFGNode *srcNode, const std::vector<const ICFGEdge *> &path);

private:
    SVFIR *_svfir;
    IntervalExeState _es;
    RelExeState _relEs;

    Map<NodeID, IntervalExeState *> _br_cond;
};
}

#endif //Z3_EXAMPLE_SVFIR2ITVEXESTATE_H
