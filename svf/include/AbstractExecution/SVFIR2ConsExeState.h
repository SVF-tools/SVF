//===- SVFIR2ConsExeState.h ----SVFIR2ConsExeState-------------------------//
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

//
// Created by jiawei and xiao on 6/1/23.
//

#ifndef SVF_SVFIR2CONSEXESTATE_H
#define SVF_SVFIR2CONSEXESTATE_H

#include "AbstractExecution/ConsExeState.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{
class SVFIR2ConsExeState
{
public:
    typedef ExeState::VAddrs VAddrs;

    SVFIR2ConsExeState() = default;

    void setEs(ConsExeState *es)
    {
        _es = es;
    }

    ConsExeState *getEs()
    {
        return _es;
    }

    virtual ~SVFIR2ConsExeState();

    /// Translator for llvm ir
    //{%
    /// https://llvm.org/docs/LangRef.html#alloca-instruction
    void translateAddr(const AddrStmt *addr);

    /// https://llvm.org/docs/LangRef.html#binary-operations
    void translateBinary(const BinaryOPStmt *binary);

    /// https://llvm.org/docs/LangRef.html#icmp-instruction
    void translateCmp(const CmpStmt *cmp);

    /// https://llvm.org/docs/LangRef.html#load-instruction
    void translateLoad(const LoadStmt *load);

    /// https://llvm.org/docs/LangRef.html#store-instruction
    void translateStore(const StoreStmt *store);

    /// https://llvm.org/docs/LangRef.html#conversion-operations
    void translateCopy(const CopyStmt *copy);

    /// https://llvm.org/docs/LangRef.html#call-instruction
    void translateCall(const CallPE *callPE);

    void translateRet(const RetPE *retPE);

    /// https://llvm.org/docs/LangRef.html#getelementptr-instruction
    void translateGep(const GepStmt *gep, bool isGlobal);

    /// https://llvm.org/docs/LangRef.html#select-instruction
    void translateSelect(const SelectStmt *select);

    /// https://llvm.org/docs/LangRef.html#i-phi
    void translatePhi(const PhiStmt *phi);

    //%}
    //%}

    /// Return the expr of gep object given a base and offset
    VAddrs getGepObjAddress(u32_t base, s32_t offset);

    /// Return the offset expression of a GepStmt
    std::pair<s32_t, s32_t> getGepOffset(const GepStmt *gep);

    /// Init ConZ3Expr for ObjVar
    void initObjVar(const ObjVar *objVar, u32_t varId);

    void initValVar(const ValVar *objVar, u32_t varId);

    void initSVFVar(u32_t varId);

    void moveToGlobal();

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

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return ExeState::getInternalID(idx);
    }

    inline bool inVarToValTable(u32_t id) const
    {
        return _es->inVarToVal(id);
    }

    inline bool inLocToValTable(u32_t id) const
    {
        return _es->inLocToVal(id);
    }

    inline bool inVarToAddrsTable(u32_t id) const
    {
        return _es->inVarToAddrsTable(id);
    }

    inline bool inLocToAddrsTable(u32_t id) const
    {
        return _es->inLocToAddrsTable(id);
    }

protected:
    ConsExeState *_es{nullptr};
}; // end class SVFIR2ConsExeState
} // end namespace SVF

#endif // SVF_SVFIR2CONSEXESTATE_H
