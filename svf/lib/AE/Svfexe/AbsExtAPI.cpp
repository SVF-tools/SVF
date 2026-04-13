//===- AbsExtAPI.cpp -- Abstract Interpretation External API handler-----//
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


//
//  Created on: Sep 9, 2024
//      Author: Xiao Cheng, Jiawei Wang
//
//
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"

using namespace SVF;
AbsExtAPI::AbsExtAPI(AbstractStateManager* mgr): mgr(mgr)
{
    svfir = PAG::getPAG();
    icfg = svfir->getICFG();
    initExtFunMap();
}

void AbsExtAPI::initExtFunMap()
{
#define SSE_FUNC_PROCESS(LLVM_NAME ,FUNC_NAME) \
        auto sse_##FUNC_NAME = [this](const CallICFGNode *callNode) { \
        /* run real ext function */            \
        const SVFVar* argVar = callNode->getArgument(0); \
        const AbstractValue& argVal = mgr->getAbstractValue(argVar, callNode); \
        if (!argVal.isInterval() && !argVal.isAddr()) return; \
        u32_t rhs = argVal.getInterval().lb().getIntNumeral(); \
        s32_t res = FUNC_NAME(rhs);            \
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet(); \
        mgr->updateAbstractValue(retVar, IntervalValue(res), callNode); \
        return; \
    };                                                                         \
    func_map[#FUNC_NAME] = sse_##FUNC_NAME;

    SSE_FUNC_PROCESS(isalnum, isalnum);
    SSE_FUNC_PROCESS(isalpha, isalpha);
    SSE_FUNC_PROCESS(isblank, isblank);
    SSE_FUNC_PROCESS(iscntrl, iscntrl);
    SSE_FUNC_PROCESS(isdigit, isdigit);
    SSE_FUNC_PROCESS(isgraph, isgraph);
    SSE_FUNC_PROCESS(isprint, isprint);
    SSE_FUNC_PROCESS(ispunct, ispunct);
    SSE_FUNC_PROCESS(isspace, isspace);
    SSE_FUNC_PROCESS(isupper, isupper);
    SSE_FUNC_PROCESS(isxdigit, isxdigit);
    SSE_FUNC_PROCESS(llvm.sin.f64, sin);
    SSE_FUNC_PROCESS(llvm.cos.f64, cos);
    SSE_FUNC_PROCESS(llvm.tan.f64, tan);
    SSE_FUNC_PROCESS(llvm.log.f64, log);
    SSE_FUNC_PROCESS(sinh, sinh);
    SSE_FUNC_PROCESS(cosh, cosh);
    SSE_FUNC_PROCESS(tanh, tanh);

    auto sse_svf_assert = [this](const CallICFGNode* callNode)
    {
        checkpoints.erase(callNode);
        const AbstractValue& arg0Val = mgr->getAbstractValue(callNode->getArgument(0), callNode);
        if (arg0Val.getInterval().equals(IntervalValue(1, 1)))
        {
            SVFUtil::errs() << SVFUtil::sucMsg("The assertion is successfully verified!!\n");
        }
        else
        {
            SVFUtil::errs() << SVFUtil::errMsg("Assertion failure, this svf_assert cannot be verified!!\n") << callNode->toString() << "\n";
            assert(false);
        }
        return;
    };
    func_map["svf_assert"] = sse_svf_assert;

    auto svf_assert_eq = [this](const CallICFGNode* callNode)
    {
        const AbstractValue& arg0Val = mgr->getAbstractValue(callNode->getArgument(0), callNode);
        const AbstractValue& arg1Val = mgr->getAbstractValue(callNode->getArgument(1), callNode);
        if (arg0Val.getInterval().equals(arg1Val.getInterval()))
        {
            SVFUtil::errs() << SVFUtil::sucMsg("The assertion is successfully verified!!\n");
        }
        else
        {
            SVFUtil::errs() <<"svf_assert_eq Fail. " << callNode->toString() << "\n";
            assert(false);
        }
        return;
    };
    func_map["svf_assert_eq"] = svf_assert_eq;

    auto svf_print = [&](const CallICFGNode* callNode)
    {
        if (callNode->arg_size() < 2) return;
        std::string text = strRead(callNode->getArgument(1), callNode);
        IntervalValue itv = mgr->getAbstractValue(callNode->getArgument(0), callNode).getInterval();
        std::cout << "Text: " << text <<", Value: " << callNode->getArgument(0)->toString()
                  << ", PrintVal: " << itv.toString() << ", Loc:" << callNode->getSourceLoc() << std::endl;
        return;
    };
    func_map["svf_print"] = svf_print;

    auto svf_set_value = [&](const CallICFGNode* callNode)
    {
        if (callNode->arg_size() < 2) return;
        AbstractState&as = getAbstractState(callNode);
        const AbstractValue& lbVal = mgr->getAbstractValue(callNode->getArgument(1), callNode);
        const AbstractValue& ubVal = mgr->getAbstractValue(callNode->getArgument(2), callNode);
        assert(lbVal.getInterval().is_numeral() && ubVal.getInterval().is_numeral());
        AbstractValue num;
        num.getInterval().set_to_top();
        num.getInterval().meet_with(IntervalValue(lbVal.getInterval().lb(), ubVal.getInterval().ub()));
        mgr->updateAbstractValue(callNode->getArgument(0), num, callNode);
        const ICFGNode* node = SVFUtil::cast<ValVar>(callNode->getArgument(0))->getICFGNode();
        for (const SVFStmt* stmt: node->getSVFStmts())
        {
            if (SVFUtil::isa<LoadStmt>(stmt))
            {
                const LoadStmt* load = SVFUtil::cast<LoadStmt>(stmt);
                const AbstractValue& ptrVal = mgr->getAbstractValue(load->getRHSVar(), callNode);
                for (auto addr : ptrVal.getAddrs())
                    as.store(addr, num);
            }
        }
        return;
    };
    func_map["set_value"] = svf_set_value;

    auto sse_scanf = [&](const CallICFGNode* callNode)
    {
        //scanf("%d", &data);
        if (callNode->arg_size() < 2) return;
        AbstractState& as = getAbstractState(callNode);
        const AbstractValue& dstVal = mgr->getAbstractValue(callNode->getArgument(1), callNode);
        if (!dstVal.isAddr()) return;
        for (auto vaddr: dstVal.getAddrs())
        {
            u32_t objId = as.getIDFromAddr(vaddr);
            AbstractValue range = getRangeLimitFromType(svfir->getSVFVar(objId)->getType());
            as.store(vaddr, range);
        }
    };
    auto sse_fscanf = [&](const CallICFGNode* callNode)
    {
        //fscanf(stdin, "%d", &data);
        if (callNode->arg_size() < 3) return;
        AbstractState& as = getAbstractState(callNode);
        const AbstractValue& dstVal = mgr->getAbstractValue(callNode->getArgument(2), callNode);
        if (!dstVal.isAddr()) return;
        for (auto vaddr: dstVal.getAddrs())
        {
            u32_t objId = as.getIDFromAddr(vaddr);
            AbstractValue range = getRangeLimitFromType(svfir->getSVFVar(objId)->getType());
            as.store(vaddr, range);
        }
    };

    func_map["__isoc99_fscanf"] = sse_fscanf;
    func_map["__isoc99_scanf"] = sse_scanf;
    func_map["__isoc99_vscanf"] = sse_scanf;
    func_map["fscanf"] = sse_fscanf;
    func_map["scanf"] = sse_scanf;
    func_map["sscanf"] = sse_scanf;
    func_map["__isoc99_sscanf"] = sse_scanf;
    func_map["vscanf"] = sse_scanf;

    auto sse_fread = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 3) return;
        IntervalValue block_count = mgr->getAbstractValue(callNode->getArgument(2), callNode).getInterval();
        IntervalValue block_size = mgr->getAbstractValue(callNode->getArgument(1), callNode).getInterval();
        IntervalValue block_byte = block_count * block_size;
        (void)block_byte;
    };
    func_map["fread"] = sse_fread;

    auto sse_sprintf = [&](const CallICFGNode *callNode)
    {
        // printf is difficult to predict since it has no byte size arguments
    };

    auto sse_snprintf = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 2) return;
        // get elem size of arg2
        u32_t elemSize = 1;
        if (callNode->getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(
                           callNode->getArgument(2)->getType())->getTypeOfElement()->getByteSize();
        }
        else if (callNode->getArgument(2)->getType()->isPointerTy())
        {
            elemSize = 1;
        }
        else
        {
            return;
        }
        IntervalValue size = mgr->getAbstractValue(callNode->getArgument(1), callNode).getInterval()
                             * IntervalValue(elemSize) - IntervalValue(1);
        (void)size;
    };
    func_map["__snprintf_chk"] = sse_snprintf;
    func_map["__vsprintf_chk"] = sse_sprintf;
    func_map["__sprintf_chk"] = sse_sprintf;
    func_map["snprintf"] = sse_snprintf;
    func_map["sprintf"] = sse_sprintf;
    func_map["vsprintf"] = sse_sprintf;
    func_map["vsnprintf"] = sse_snprintf;
    func_map["__vsnprintf_chk"] = sse_snprintf;
    func_map["swprintf"] = sse_snprintf;
    func_map["_snwprintf"] = sse_snprintf;


    auto sse_itoa = [&](const CallICFGNode* callNode)
    {
        if (callNode->arg_size() < 3) return;
        u32_t num = (u32_t) mgr->getAbstractValue(callNode->getArgument(0), callNode).getInterval().getNumeral();
        std::string snum = std::to_string(num);
        (void)snum;
    };
    func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 1) return;
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet();
        IntervalValue byteLen = getStrlen(callNode->getArgument(0), callNode);
        u32_t elemSize = getElementSize(callNode->getArgument(0));
        if (byteLen.is_numeral() && elemSize > 1)
            mgr->updateAbstractValue(retVar, IntervalValue(byteLen.getIntNumeral() / (s64_t)elemSize), callNode);
        else
            mgr->updateAbstractValue(retVar, byteLen, callNode);
    };
    func_map["strlen"] = sse_strlen;
    func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 4) return;
        IntervalValue len = mgr->getAbstractValue(callNode->getArgument(2), callNode).getInterval() - IntervalValue(1);
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet();
        mgr->updateAbstractValue(retVar, len, callNode);
    };
    func_map["recv"] = sse_recv;
    func_map["__recv"] = sse_recv;

    auto sse_free = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 1) return;
        AbstractState& as = getAbstractState(callNode);
        const AbstractValue& ptrVal = mgr->getAbstractValue(callNode->getArgument(0), callNode);
        for (auto addr: ptrVal.getAddrs())
        {
            if (AbstractState::isBlackHoleObjAddr(addr))
            {
            }
            else
            {
                as.addToFreedAddrs(addr);
            }
        }
    };
    // Add all free-related functions to func_map
    std::vector<std::string> freeFunctions =
    {
        "VOS_MemFree", "cfree", "free", "free_all_mem", "freeaddrinfo",
        "gcry_mpi_release", "gcry_sexp_release", "globfree", "nhfree",
        "obstack_free", "safe_cfree", "safe_free", "safefree", "safexfree",
        "sm_free", "vim_free", "xfree", "SSL_CTX_free", "SSL_free", "XFree"
    };

    for (const auto& name : freeFunctions)
    {
        func_map[name] = sse_free;
    }
};

AbstractState& AbsExtAPI::getAbstractState(const SVF::ICFGNode* node)
{
    return mgr->getAbstractState(node);
}

void AbsExtAPI::collectCheckPoint()
{
    // traverse every ICFGNode
    Set<std::string> ae_checkpoint_names = {"svf_assert"};
    Set<std::string> buf_checkpoint_names = {"UNSAFE_BUFACCESS", "SAFE_BUFACCESS"};
    Set<std::string> nullptr_checkpoint_names = {"UNSAFE_LOAD", "SAFE_LOAD"};

    for (auto it = svfir->getICFG()->begin(); it != svfir->getICFG()->end(); ++it)
    {
        const ICFGNode* node = it->second;
        if (const CallICFGNode *call = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            if (const FunObjVar *fun = call->getCalledFunction())
            {
                if (ae_checkpoint_names.find(fun->getName()) !=
                        ae_checkpoint_names.end())
                {
                    checkpoints.insert(call);
                }
                if (Options::BufferOverflowCheck())
                {
                    if (buf_checkpoint_names.find(fun->getName()) !=
                            buf_checkpoint_names.end())
                    {
                        checkpoints.insert(call);
                    }
                }
                if (Options::NullDerefCheck())
                {
                    if (nullptr_checkpoint_names.find(fun->getName()) !=
                            nullptr_checkpoint_names.end())
                    {
                        checkpoints.insert(call);
                    }
                }
            }
        }
    }
}

void AbsExtAPI::checkPointAllSet()
{
    if (checkpoints.size() == 0)
    {
        return;
    }
    else
    {
        SVFUtil::errs() << SVFUtil::errMsg("At least one svf_assert has not been checked!!") << "\n";
        for (const CallICFGNode* call: checkpoints)
            SVFUtil::errs() << call->toString() + "\n";
        assert(false);
    }
}

std::string AbsExtAPI::strRead(const ValVar* rhs, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    std::string str0;

    for (u32_t index = 0; index < Options::MaxFieldLimit(); index++)
    {
        if (!mgr->getAbstractValue(rhs, node).isAddr()) continue;
        AbstractValue expr0 =
            mgr->getGepObjAddrs(rhs, IntervalValue(index));

        AbstractValue val;
        for (const auto &addr: expr0.getAddrs())
        {
            val.join_with(as.load(addr));
        }
        if (!val.getInterval().is_numeral())
        {
            break;
        }
        if ((char) val.getInterval().getIntNumeral() == '\0')
        {
            break;
        }
        str0.push_back((char) val.getInterval().getIntNumeral());
    }
    return str0;
}

void AbsExtAPI::handleExtAPI(const CallICFGNode *call)
{
    const FunObjVar *fun = call->getCalledFunction();
    assert(fun && "FunObjVar* is nullptr");
    ExtAPIType extType = UNCLASSIFIED;
    // get type of mem api
    for (const std::string &annotation: ExtAPI::getExtAPI()->getExtFuncAnnotations(fun))
    {
        if (annotation.find("MEMCPY") != std::string::npos)
            extType =  MEMCPY;
        if (annotation.find("MEMSET") != std::string::npos)
            extType =  MEMSET;
        if (annotation.find("STRCPY") != std::string::npos)
            extType = STRCPY;
        if (annotation.find("STRCAT") != std::string::npos)
            extType =  STRCAT;
    }
    if (extType == UNCLASSIFIED)
    {
        if (func_map.find(fun->getName()) != func_map.end())
        {
            func_map[fun->getName()](call);
        }
        else
        {
            if (const SVFVar* ret = call->getRetICFGNode()->getActualRet())
            {
                const AbstractValue& retVal = mgr->getAbstractValue(ret, call);
                if (!retVal.isAddr())
                {
                    mgr->updateAbstractValue(ret, IntervalValue(), call);
                }
            }
            return;
        }
    }
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    else if (extType == MEMCPY)
    {
        IntervalValue len = mgr->getAbstractValue(call->getArgument(2), call).getInterval();
        handleMemcpy(call->getArgument(0), call->getArgument(1), len, 0, call);
    }
    else if (extType == MEMSET)
    {
        IntervalValue len = mgr->getAbstractValue(call->getArgument(2), call).getInterval();
        IntervalValue elem = mgr->getAbstractValue(call->getArgument(1), call).getInterval();
        handleMemset(call->getArgument(0), elem, len, call);
    }
    else if (extType == STRCPY)
    {
        handleStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        // Both strcat and strncat are annotated as STRCAT.
        // Distinguish by name: strncat/wcsncat contain "ncat".
        const std::string& name = fun->getName();
        if (name.find("ncat") != std::string::npos)
            handleStrncat(call);
        else
            handleStrcat(call);
    }
    else
    {

    }
    return;
}

// ===----------------------------------------------------------------------===//
//  Shared primitives for string/memory handlers
// ===----------------------------------------------------------------------===//

/// Get the byte size of each element for a pointer/array variable.
/// Shared by handleMemcpy, handleMemset, and getStrlen to avoid duplication.
u32_t AbsExtAPI::getElementSize(const ValVar* var)
{
    if (var->getType()->isArrayTy())
    {
        return SVFUtil::dyn_cast<SVFArrayType>(var->getType())
               ->getTypeOfElement()->getByteSize();
    }
    if (var->getType()->isPointerTy())
        return 1;
    assert(false && "unsupported type for element size");
    return 1;
}

/// Check if an interval length is usable for memory operations.
/// Returns false for bottom (no information) or unbounded lower bound
/// (cannot determine a concrete start for iteration).
bool AbsExtAPI::isValidLength(const IntervalValue& len)
{
    return !len.isBottom() && !len.lb().is_minus_infinity();
}

/// Calculate the length of a null-terminated string in abstract state.
/// Scans memory from the base of strValue looking for a '\0' byte.
/// Returns an IntervalValue: exact length if '\0' found, otherwise [0, MaxFieldLimit].
IntervalValue AbsExtAPI::getStrlen(const ValVar *strValue, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    // Step 1: determine the buffer size (in bytes) backing this pointer
    u32_t dst_size = 0;
    const AbstractValue& ptrVal = mgr->getAbstractValue(strValue, node);
    for (const auto& addr : ptrVal.getAddrs())
    {
        NodeID objId = as.getIDFromAddr(addr);
        if (svfir->getBaseObject(objId)->isConstantByteSize())
        {
            dst_size = svfir->getBaseObject(objId)->getByteSizeOfObj();
        }
        else
        {
            const ICFGNode* icfgNode = svfir->getBaseObject(objId)->getICFGNode();
            for (const SVFStmt* stmt2: icfgNode->getSVFStmts())
            {
                if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                {
                    dst_size = mgr->getAllocaInstByteSize(addrStmt);
                }
            }
        }
    }

    // Step 2: scan for '\0' terminator
    u32_t len = 0;
    if (mgr->getAbstractValue(strValue, node).isAddr())
    {
        for (u32_t index = 0; index < dst_size; index++)
        {
            AbstractValue expr0 =
                mgr->getGepObjAddrs(strValue, IntervalValue(index));
            AbstractValue val;
            for (const auto &addr: expr0.getAddrs())
            {
                val.join_with(as.load(addr));
            }
            if (val.getInterval().is_numeral() &&
                    (char) val.getInterval().getIntNumeral() == '\0')
            {
                break;
            }
            ++len;
        }
    }

    // Step 3: scale by element size and return
    u32_t elemSize = getElementSize(strValue);
    if (len == 0)
        return IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
    return IntervalValue(len * elemSize);
}

// ===----------------------------------------------------------------------===//
//  String/memory operation handlers
// ===----------------------------------------------------------------------===//

/// strcpy(dst, src): copy all of src (including '\0') into dst.
/// Covers: strcpy, __strcpy_chk, stpcpy, wcscpy, __wcscpy_chk
void AbsExtAPI::handleStrcpy(const CallICFGNode *call)
{
    const ValVar* dst = call->getArgument(0);
    const ValVar* src = call->getArgument(1);
    IntervalValue srcLen = getStrlen(src, call);
    if (!isValidLength(srcLen)) return;
    handleMemcpy(dst, src, srcLen, 0, call);
}

/// strcat(dst, src): append all of src after the end of dst.
/// Covers: strcat, __strcat_chk, wcscat, __wcscat_chk
void AbsExtAPI::handleStrcat(const CallICFGNode *call)
{
    const ValVar* dst = call->getArgument(0);
    const ValVar* src = call->getArgument(1);
    IntervalValue dstLen = getStrlen(dst, call);
    IntervalValue srcLen = getStrlen(src, call);
    if (!isValidLength(dstLen)) return;
    handleMemcpy(dst, src, srcLen, dstLen.lb().getIntNumeral(), call);
}

/// strncat(dst, src, n): append at most n bytes of src after the end of dst.
/// Covers: strncat, __strncat_chk, wcsncat, __wcsncat_chk
void AbsExtAPI::handleStrncat(const CallICFGNode *call)
{
    const ValVar* dst = call->getArgument(0);
    const ValVar* src = call->getArgument(1);
    IntervalValue n = mgr->getAbstractValue(call->getArgument(2), call).getInterval();
    IntervalValue dstLen = getStrlen(dst, call);
    if (!isValidLength(dstLen)) return;
    handleMemcpy(dst, src, n, dstLen.lb().getIntNumeral(), call);
}

/// Core memcpy: copy `len` bytes from src to dst starting at dst[start_idx].
void AbsExtAPI::handleMemcpy(const ValVar *dst,
                             const ValVar *src, const IntervalValue& len,
                             u32_t start_idx, const ICFGNode* node)
{
    if (!isValidLength(len)) return;
    AbstractState& as = getAbstractState(node);

    u32_t elemSize = getElementSize(dst);
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(),
                          (u32_t)len.lb().getIntNumeral());
    u32_t range_val = size / elemSize;

    if (!mgr->getAbstractValue(src, node).isAddr() || !mgr->getAbstractValue(dst, node).isAddr())
        return;

    for (u32_t index = 0; index < range_val; index++)
    {
        AbstractValue expr_src =
            mgr->getGepObjAddrs(src, IntervalValue(index));
        AbstractValue expr_dst =
            mgr->getGepObjAddrs(dst, IntervalValue(index + start_idx));
        for (const auto &dstAddr: expr_dst.getAddrs())
        {
            for (const auto &srcAddr: expr_src.getAddrs())
            {
                u32_t objId = as.getIDFromAddr(srcAddr);
                if (as.inAddrToValTable(objId) || as.inAddrToAddrsTable(objId))
                {
                    as.store(dstAddr, as.load(srcAddr));
                }
            }
        }
    }
}

/// Core memset: fill dst with `elem` for `len` bytes.
/// Note: elemSize here uses the pointee type's full size (not array element size)
/// to match how LLVM memset/wmemset intrinsics measure `len`. For a pointer to
/// wchar_t[100], elemSize = sizeof(wchar_t[100]), so range_val reflects the
/// number of top-level GEP fields, not individual array elements.
void AbsExtAPI::handleMemset(const ValVar *dst,
                             const IntervalValue& elem, const IntervalValue& len, const ICFGNode* node)
{
    if (!isValidLength(len)) return;
    AbstractState& as = getAbstractState(node);

    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy())
    {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())
                   ->getTypeOfElement()->getByteSize();
    }
    else if (dst->getType()->isPointerTy())
    {
        elemSize = 1;
    }
    else
    {
        assert(false && "unsupported type for element size");
    }
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(),
                          (u32_t)len.lb().getIntNumeral());
    u32_t range_val = size / elemSize;

    for (u32_t index = 0; index < range_val; index++)
    {
        if (!mgr->getAbstractValue(dst, node).isAddr())
            break;
        AbstractValue lhs_gep = mgr->getGepObjAddrs(dst, IntervalValue(index));
        for (const auto &addr: lhs_gep.getAddrs())
        {
            u32_t objId = as.getIDFromAddr(addr);
            if (as.inAddrToValTable(objId))
            {
                AbstractValue tmp = as.load(addr);
                tmp.join_with(elem);
                as.store(addr, tmp);
            }
            else
            {
                as.store(addr, elem);
            }
        }
    }
}

/**
 * This function, getRangeLimitFromType, calculates the lower and upper bounds of
 * a numeric range for a given SVFType. It is used to determine the possible value
 * range of integer types. If the type is an SVFIntegerType, it calculates the bounds
 * based on the size and signedness of the type. The calculated bounds are returned
 * as an IntervalValue representing the lower (lb) and upper (ub) limits of the range.
 *
 * @param type   The SVFType for which to calculate the value range.
 *
 * @return       An IntervalValue representing the lower and upper bounds of the range.
 */
IntervalValue AbsExtAPI::getRangeLimitFromType(const SVFType* type)
{
    if (const SVFIntegerType* intType = SVFUtil::dyn_cast<SVFIntegerType>(type))
    {
        u32_t bits = type->getByteSize() * 8;
        s64_t ub = 0;
        s64_t lb = 0;
        if (bits >= 32)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u32_t>::min());
            }
        }
        else if (bits == 16)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s16_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u16_t>::min());
            }
        }
        else if (bits == 8)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<int8_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<uint8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<uint8_t>::min());
            }
        }
        return IntervalValue(lb, ub);
    }
    else if (SVFUtil::isa<SVFOtherType>(type))
    {
        // handle other type like float double, set s32_t as the range
        s64_t ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
        s64_t lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
        return IntervalValue(lb, ub);
    }
    else
    {
        return IntervalValue::top();
        // other types, return top interval
    }
}