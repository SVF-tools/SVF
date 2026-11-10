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
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"

using namespace SVF;
AbsExtAPI::AbsExtAPI(AbstractInterpretation* ae): ae(ae)
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
        assert(callNode->arg_size() >= 1 && "external function expects one argument"); \
        const SVFVar* argVar = callNode->getArgument(0); \
        const AbstractValue& argVal = ae->getAbsValue(argVar, callNode); \
        IntervalValue result = IntervalValue::top(); \
        if (argVal.isInterval() && argVal.getInterval().is_numeral()) { \
            u32_t rhs = argVal.getInterval().lb().getIntNumeral(); \
            result = IntervalValue(FUNC_NAME(rhs)); \
        }                                      \
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet(); \
        ae->updateAbsValue(retVar, result, callNode); \
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
        const AbstractValue& arg0Val = ae->getAbsValue(callNode->getArgument(0), callNode);
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
        const AbstractValue& arg0Val = ae->getAbsValue(callNode->getArgument(0), callNode);
        const AbstractValue& arg1Val = ae->getAbsValue(callNode->getArgument(1), callNode);
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
        assert(callNode->arg_size() >= 2 && "svf_print expects two arguments");
        std::string text = strRead(callNode->getArgument(1), callNode);
        IntervalValue itv = ae->getAbsValue(callNode->getArgument(0), callNode).getInterval();
        std::cout << "Text: " << text <<", Value: " << callNode->getArgument(0)->toString()
                  << ", PrintVal: " << itv.toString() << ", Loc:" << callNode->getSourceLoc() << std::endl;
        return;
    };
    func_map["svf_print"] = svf_print;

    auto svf_set_value = [&](const CallICFGNode* callNode)
    {
        assert(callNode->arg_size() >= 3 && "set_value expects three arguments");
        AbstractState&as = getAbsState(callNode);
        const AbstractValue& lbVal = ae->getAbsValue(callNode->getArgument(1), callNode);
        const AbstractValue& ubVal = ae->getAbsValue(callNode->getArgument(2), callNode);
        assert(lbVal.getInterval().is_numeral() && ubVal.getInterval().is_numeral());
        AbstractValue num;
        num.getInterval().set_to_top();
        num.getInterval().meet_with(IntervalValue(lbVal.getInterval().lb(), ubVal.getInterval().ub()));
        ae->updateAbsValue(callNode->getArgument(0), num, callNode);
        const ICFGNode* node = SVFUtil::cast<ValVar>(callNode->getArgument(0))->getICFGNode();
        for (const SVFStmt* stmt: node->getSVFStmts())
        {
            if (SVFUtil::isa<LoadStmt>(stmt))
            {
                const LoadStmt* load = SVFUtil::cast<LoadStmt>(stmt);
                const AbstractValue& ptrVal = ae->getAbsValue(load->getRHSVar(), callNode);
                for (auto addr : ptrVal.getAddrs())
                    as.store(addr, num);
            }
        }
        return;
    };
    func_map["set_value"] = svf_set_value;

    auto sse_fread = [&](const CallICFGNode *callNode)
    {
        assert(callNode->arg_size() >= 3 && "fread expects at least three arguments");
        IntervalValue block_count = ae->getAbsValue(callNode->getArgument(2), callNode).getInterval();
        IntervalValue block_size = ae->getAbsValue(callNode->getArgument(1), callNode).getInterval();
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
        assert(callNode->arg_size() >= 2 &&
               "formatted-output function expects at least two arguments");
        u32_t elemSize = 1;
        if (callNode->arg_size() > 2 &&
                callNode->getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::cast<SVFArrayType>(
                           callNode->getArgument(2)->getType())
                       ->getTypeOfElement()->getByteSize();
        }
        IntervalValue size = ae->getAbsValue(callNode->getArgument(1), callNode).getInterval()
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
        assert(callNode->arg_size() >= 3 && "itoa expects three arguments");
        u32_t num = (u32_t) ae->getAbsValue(callNode->getArgument(0), callNode).getInterval().getNumeral();
        std::string snum = std::to_string(num);
        (void)snum;
    };
    func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallICFGNode *callNode)
    {
        assert(callNode->arg_size() >= 1 && "strlen expects one argument");
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet();
        ae->updateAbsValue(retVar, getStrlen(callNode->getArgument(0), callNode), callNode);
    };
    func_map["strlen"] = sse_strlen;
    func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallICFGNode *callNode)
    {
        assert(callNode->arg_size() >= 4 && "recv expects four arguments");
        IntervalValue len = ae->getAbsValue(callNode->getArgument(2), callNode).getInterval() - IntervalValue(1);
        const SVFVar* retVar = callNode->getRetICFGNode()->getActualRet();
        ae->updateAbsValue(retVar, len, callNode);
    };
    func_map["recv"] = sse_recv;
    func_map["__recv"] = sse_recv;

    auto sse_free = [&](const CallICFGNode *callNode)
    {
        assert(callNode->arg_size() >= 1 && "free expects one argument");
        AbstractState& as = getAbsState(callNode);
        const AbstractValue& ptrVal = ae->getAbsValue(callNode->getArgument(0), callNode);
        for (auto addr: ptrVal.getAddrs())
        {
            if (!AbstractState::isNullOrBlackHoleAddr(addr))
                as.addToFreedAddrs(addr);
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

AbstractState& AbsExtAPI::getAbsState(const SVF::ICFGNode* node)
{
    return ae->getAbsState(node);
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
    if (!checkpoints.empty())
    {
        SVFUtil::errs() << SVFUtil::errMsg("At least one svf_assert has not been checked!!") << "\n";
        for (const CallICFGNode* call: checkpoints)
            SVFUtil::errs() << call->toString() + "\n";
        assert(false);
    }
}

std::string AbsExtAPI::strRead(const ValVar* rhs, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    std::string str0;

    for (u32_t index = 0; index < Options::MaxFieldLimit(); index++)
    {
        AbstractValue expr0 =
            ae->getGepObjAddrs(rhs, IntervalValue(index));

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
            const SVFVar* ret = call->getRetICFGNode()->getActualRet();
            if (ret && !ret->getType()->isPointerTy())
            {
                ae->updateAbsValue(ret, IntervalValue(), call);
            }
        }
    }
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    else if (extType == MEMCPY)
    {
        IntervalValue len = ae->getAbsValue(call->getArgument(2), call).getInterval();
        handleMemcpy(call->getArgument(0), call->getArgument(1), len, 0, call);
    }
    else if (extType == MEMSET)
    {
        IntervalValue len = ae->getAbsValue(call->getArgument(2), call).getInterval();
        IntervalValue elem = ae->getAbsValue(call->getArgument(1), call).getInterval();
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

u32_t AbsExtAPI::getBoundedMinimumByteCount(const IntervalValue& len)
{
    u32_t byteCount = 0;
    if (isValidLength(len) && len.lb().getIntNumeral() > 0)
    {
        byteCount = std::min(Options::MaxFieldLimit(),
                             (u32_t)len.lb().getIntNumeral());
    }
    return byteCount;
}

u32_t AbsExtAPI::getStringElementSize(const ValVar* var, const ICFGNode* node)
{
    u32_t width = 0;
    const AbstractValue& pointer = ae->getAbsValue(var, node);
    if (!pointer.isAddr() || pointer.getAddrs().empty())
        return 0;
    for (auto addr : pointer.getAddrs())
    {
        const NodeID obj = getAbsState(node).getIDFromAddr(addr);
        if (svfir->isNullOrBlkObj(obj))
            return 0;
        const SVFType* type = svfir->getBaseObject(obj)->getType();
        while (const auto* array = SVFUtil::dyn_cast<SVFArrayType>(type))
            type = array->getTypeOfElement();
        if (!SVFUtil::isa<SVFIntegerType>(type) || !type->getByteSize() ||
                (width && width != type->getByteSize()))
            return 0;
        width = type->getByteSize();
    }
    return width;
}

IntervalValue AbsExtAPI::getStringLastByte(const CallICFGNode* call,
        const IntervalValue& length)
{
    IntervalValue width(1);
    if (call->getCalledFunction()->getName().find("wcs") != std::string::npos)
    {
        const u32_t dst = getStringElementSize(call->getArgument(0), call);
        const u32_t src = getStringElementSize(call->getArgument(1), call);
        // Byte storage alone does not establish the wide character width.
        width = (dst > 1 || src > 1) && (dst <= 1 || src <= 1 || dst == src)
                ? IntervalValue(dst > 1 ? dst : src)
                : IntervalValue(1, IntervalValue::plus_infinity());
    }
    return (length + IntervalValue(1)) * width - IntervalValue(1);
}

/// Only a nonzero prefix proves a lower bound, and only a definite NUL
/// proves an upper bound. The scan budget is not a termination guarantee.
IntervalValue AbsExtAPI::getStrlen(const ValVar *strValue, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    const IntervalValue unknown(0, IntervalValue::plus_infinity());
    IntervalValue result = IntervalValue::bottom();
    const AbstractValue& ptrVal = ae->getAbsValue(strValue, node);
    if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(node))
        if (call->getCalledFunction() &&
                call->getCalledFunction()->getName().find("wcs") == std::string::npos &&
                getStringElementSize(strValue, node) != 1)
            return unknown;
    if (!ptrVal.isAddr() || ptrVal.getAddrs().empty())
        return unknown;
    for (const auto& addr : ptrVal.getAddrs())
    {
        const NodeID objId = as.getIDFromAddr(addr);
        if (svfir->isNullOrBlkObj(objId))
        {
            result.join_with(unknown);
            continue;
        }
        const BaseObjVar* base = svfir->getBaseObject(objId);
        const auto* gep = SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(objId));
        const APOffset start = gep ? gep->getConstantFieldIdx() : 0;
        u32_t lower = 0;
        bool nonzeroPrefix = true;
        IntervalValue length = unknown;
        for (u32_t index = 0; index < Options::MaxFieldLimit(); ++index)
        {
            if (start < 0 || start + index >= base->getNumOfElements() ||
                    start + index >= base->getMaxFieldOffsetLimit() ||
                    start + index >= Options::MaxFieldLimit())
                break;
            const NodeID field = svfir->getGepObjVar(objId, index);
            const AbstractValue& cell = as.load(AbstractState::getVirtualMemAddress(field));
            if (cell.isInterval() && !cell.isAddr() && cell.getInterval().is_zero())
            {
                length = IntervalValue(lower, index);
                break;
            }
            if (!cell.isInterval() || cell.isAddr() || cell.getInterval().isBottom() ||
                    cell.getInterval().contains(0))
                nonzeroPrefix = false;
            if (nonzeroPrefix)
                ++lower;
            length = IntervalValue(lower, IntervalValue::plus_infinity());
        }
        result.join_with(length);
    }
    return result;
}

// ===----------------------------------------------------------------------===//
//  String/memory operation handlers
// ===----------------------------------------------------------------------===//

/// strcpy(dst, src): copy all of src (including '\0') into dst.
/// Covers: strcpy, __strcpy_chk, stpcpy, wcscpy, __wcscpy_chk
void AbsExtAPI::handleStrcpy(const CallICFGNode *call)
{
    const ValVar* src = call->getArgument(1);
    IntervalValue srcLen = getStrlen(src, call);
    handleStringCopy(call, srcLen, IntervalValue(0));
}

/// strcat(dst, src): append all of src after the end of dst.
/// Covers: strcat, __strcat_chk, wcscat, __wcscat_chk
void AbsExtAPI::handleStrcat(const CallICFGNode *call)
{
    const ValVar* dst = call->getArgument(0);
    const ValVar* src = call->getArgument(1);
    IntervalValue dstLen = getStrlen(dst, call);
    IntervalValue srcLen = getStrlen(src, call);
    strcatLastBytes[call] = getStringLastByte(call, dstLen + srcLen);
    handleStringCopy(call, srcLen, dstLen);
}

/// strncat(dst, src, n): append at most n elements after the end of dst.
/// Covers: strncat, __strncat_chk, wcsncat, __wcsncat_chk
void AbsExtAPI::handleStrncat(const CallICFGNode *call)
{
    const ValVar* dst = call->getArgument(0);
    IntervalValue n = getStrncatLength(call);
    IntervalValue dstLen = getStrlen(dst, call);
    strcatLastBytes[call] = getStringLastByte(call, dstLen + n);
    handleStringCopy(call, n, dstLen);
}

bool AbsExtAPI::takeStrcatLastByte(const CallICFGNode* call, IntervalValue& lastByte)
{
    auto it = strcatLastBytes.find(call);
    if (it == strcatLastBytes.end())
        return false;
    lastByte = it->second;
    // Detection immediately follows the handler in dense and semi-sparse AE.
    // Consume the record so a later check without a handler cannot reuse it.
    // Each handler visit overwrites it, including loop and context revisits.
    strcatLastBytes.erase(it);
    return true;
}

IntervalValue AbsExtAPI::getStrncatLength(const CallICFGNode* call)
{
    const IntervalValue n = ae->getAbsValue(call->getArgument(2), call).getInterval();
    if (n.isBottom() || n.lb().leq(-1))
        return IntervalValue(0, IntervalValue::plus_infinity());
    const IntervalValue source = getStrlen(call->getArgument(1), call);
    return IntervalValue(min(n.lb(), source.lb()), min(n.ub(), source.ub()));
}

void AbsExtAPI::handleStringCopy(const CallICFGNode* call,
        const IntervalValue& length, const IntervalValue& start)
{
    const ValVar* dst = call->getArgument(0);
    const ValVar* src = call->getArgument(1);
    AbstractState& as = getAbsState(call);
    const bool precise = length.is_numeral() && start.is_numeral();
    const bool weak = ae->getAbsValue(dst, call).getAddrs().size() != 1;
    for (u32_t index = 0; index < Options::MaxFieldLimit(); ++index)
    {
        AbstractValue value(IntervalValue::top());
        u32_t destination = index;
        if (precise)
        {
            if (index > length.getIntNumeral() ||
                    start.getIntNumeral() + index >= Options::MaxFieldLimit())
                break;
            destination += start.getIntNumeral();
            if (index == length.getIntNumeral())
                value = IntervalValue(0);
            else
            {
                value = AbstractValue();
                for (auto addr : ae->getGepObjAddrs(src, IntervalValue(index)))
                    value.join_with(as.load(addr));
                if (!value.isInterval() || value.isAddr())
                    value = IntervalValue::top();
            }
        }
        // Uncertain positions/lengths cannot justify a copy at the lower bound.
        else if (index < start.lb().getIntNumeral())
            continue;
        for (auto target : ae->getAbsValue(dst, call).getAddrs())
        {
            const NodeID obj = as.getIDFromAddr(target);
            if (svfir->isNullOrBlkObj(obj))
                continue;
            const BaseObjVar* base = svfir->getBaseObject(obj);
            const auto* gep = SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(obj));
            const APOffset offset = (gep ? gep->getConstantFieldIdx() : 0) + destination;
            // Do not wrap an out-of-object write back onto the prefix.
            if (offset < 0 || offset >= base->getNumOfElements() ||
                    offset >= base->getMaxFieldOffsetLimit() || offset >= Options::MaxFieldLimit())
                continue;
            const auto addr = AbstractState::getVirtualMemAddress(svfir->getGepObjVar(obj, destination));
            AbstractValue stored = value;
            if (weak)
            {
                const AbstractValue& old = as.load(addr);
                stored.join_with(old.isInterval() || old.isAddr()
                                 ? old : AbstractValue(IntervalValue::top()));
            }
            as.store(addr, stored);
        }
    }
}

/// Core memcpy: copy `len` bytes from src to dst starting at dst[start_idx].
void AbsExtAPI::handleMemcpy(const ValVar *dst,
                             const ValVar *src, const IntervalValue& len,
                             u32_t start_idx, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);

    u32_t elemSize = getElementSize(dst);
    u32_t size = getBoundedMinimumByteCount(len);
    u32_t range_val = size / elemSize;

    for (u32_t index = 0; index < range_val; index++)
    {
        AbstractValue expr_src =
            ae->getGepObjAddrs(src, IntervalValue(index));
        AbstractValue expr_dst =
            ae->getGepObjAddrs(dst, IntervalValue(index + start_idx));
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
    AbstractState& as = getAbsState(node);

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
    u32_t size = getBoundedMinimumByteCount(len);
    u32_t range_val = size / elemSize;

    for (u32_t index = 0; index < range_val; index++)
    {
        AbstractValue lhs_gep = ae->getGepObjAddrs(dst, IntervalValue(index));
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
