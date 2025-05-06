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
// Created by Jiawei Wang on 2024/9/9.
//
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"

using namespace SVF;
AbsExtAPI::AbsExtAPI(Map<const ICFGNode*, AbstractState>& traces): abstractTrace(traces)
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
        AbstractState& as = getAbsStateFromTrace(callNode); \
        u32_t rhs_id = callNode->getArgument(0)->getId(); \
        if (!as.inVarToValTable(rhs_id)) return; \
        u32_t rhs = as[rhs_id].getInterval().lb().getIntNumeral(); \
        s32_t res = FUNC_NAME(rhs);            \
        u32_t lhsId = callNode->getRetICFGNode()->getActualRet()->getId();               \
        as[lhsId] = IntervalValue(res);           \
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
        AbstractInterpretation::getAEInstance().checkpoints.erase(callNode);
        u32_t arg0 = callNode->getArgument(0)->getId();
        AbstractState&as = getAbsStateFromTrace(callNode);
        if (as[arg0].getInterval().equals(IntervalValue(1, 1)))
        {
            SVFUtil::errs() << SVFUtil::sucMsg("The assertion is successfully verified!!\n");
        }
        else
        {
            SVFUtil::errs() <<"svf_assert Fail. " << callNode->toString() << "\n";
            assert(false);
        }
        return;
    };
    func_map["svf_assert"] = sse_svf_assert;

    auto svf_assert_eq = [this](const CallICFGNode* callNode)
    {
        u32_t arg0 = callNode->getArgument(0)->getId();
        u32_t arg1 = callNode->getArgument(1)->getId();
        AbstractState&as = getAbsStateFromTrace(callNode);
        if (as[arg0].getInterval().equals(as[arg1].getInterval()))
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
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t num_id = callNode->getArgument(0)->getId();
        std::string text = strRead(as, callNode->getArgument(1));
        assert(as.inVarToValTable(num_id) && "print() should pass integer");
        IntervalValue itv = as[num_id].getInterval();
        std::cout << "Text: " << text <<", Value: " << callNode->getArgument(0)->toString()
                  << ", PrintVal: " << itv.toString() << ", Loc:" << callNode->getSourceLoc() << std::endl;
        return;
    };
    func_map["svf_print"] = svf_print;

    auto svf_set_value = [&](const CallICFGNode* callNode)
    {
        if (callNode->arg_size() < 2) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        AbstractValue& num = as[callNode->getArgument(0)->getId()];
        AbstractValue& lb = as[callNode->getArgument(1)->getId()];
        AbstractValue& ub = as[callNode->getArgument(2)->getId()];
        assert(lb.getInterval().is_numeral() && ub.getInterval().is_numeral());
        num.getInterval().set_to_top();
        num.getInterval().meet_with(IntervalValue(lb.getInterval().lb(), ub.getInterval().ub()));
        const ICFGNode* node = SVFUtil::cast<ValVar>(callNode->getArgument(0))->getICFGNode();
        for (const SVFStmt* stmt: node->getSVFStmts())
        {
            if (SVFUtil::isa<LoadStmt>(stmt))
            {
                const LoadStmt* load = SVFUtil::cast<LoadStmt>(stmt);
                NodeID rhsId = load->getRHSVarID();
                as.storeValue(rhsId, num);
            }
        }
        return;
    };
    func_map["set_value"] = svf_set_value;

    auto sse_scanf = [&](const CallICFGNode* callNode)
    {
        AbstractState& as = getAbsStateFromTrace(callNode);
        //scanf("%d", &data);
        if (callNode->arg_size() < 2) return;

        u32_t dst_id = callNode->getArgument(1)->getId();
        if (!as.inVarToAddrsTable(dst_id))
        {
            return;
        }
        else
        {
            AbstractValue Addrs = as[dst_id];
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = as.getIDFromAddr(vaddr);
                AbstractValue range = getRangeLimitFromType(svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
        }
    };
    auto sse_fscanf = [&](const CallICFGNode* callNode)
    {
        //fscanf(stdin, "%d", &data);
        if (callNode->arg_size() < 3) return;
        AbstractState& as = getAbsStateFromTrace(callNode);
        u32_t dst_id = callNode->getArgument(2)->getId();
        if (!as.inVarToAddrsTable(dst_id))
        {
        }
        else
        {
            AbstractValue Addrs = as[dst_id];
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = as.getIDFromAddr(vaddr);
                AbstractValue range = getRangeLimitFromType(svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
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
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t block_count_id = callNode->getArgument(2)->getId();
        u32_t block_size_id = callNode->getArgument(1)->getId();
        IntervalValue block_count = as[block_count_id].getInterval();
        IntervalValue block_size = as[block_size_id].getInterval();
        IntervalValue block_byte = block_count * block_size;
    };
    func_map["fread"] = sse_fread;

    auto sse_sprintf = [&](const CallICFGNode *callNode)
    {
        // printf is difficult to predict since it has no byte size arguments
    };

    auto sse_snprintf = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 2) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t size_id = callNode->getArgument(1)->getId();
        u32_t dst_id = callNode->getArgument(0)->getId();
        // get elem size of arg2
        u32_t elemSize = 1;
        if (callNode->getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(
                           callNode->getArgument(2)->getType())->getTypeOfElement()->getByteSize();
        }
        else if (callNode->getArgument(2)->getType()->isPointerTy())
        {
            elemSize = as.getPointeeElement(callNode->getArgument(2)->getId())->getByteSize();
        }
        else
        {
            return;
            // assert(false && "we cannot support this type");
        }
        IntervalValue size = as[size_id].getInterval() * IntervalValue(elemSize) - IntervalValue(1);
        if (!as.inVarToAddrsTable(dst_id))
        {
        }
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
        // itoa(num, ch, 10);
        // num: int, ch: char*, 10 is decimal
        if (callNode->arg_size() < 3) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t num_id = callNode->getArgument(0)->getId();

        u32_t num = (u32_t) as[num_id].getInterval().getNumeral();
        std::string snum = std::to_string(num);
    };
    func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallICFGNode *callNode)
    {
        // check the arg size
        if (callNode->arg_size() < 1) return;
        const SVFVar* strValue = callNode->getArgument(0);
        AbstractState& as = getAbsStateFromTrace(callNode);
        NodeID value_id = strValue->getId();
        u32_t lhsId = callNode->getRetICFGNode()->getActualRet()->getId();
        u32_t dst_size = 0;
        for (const auto& addr : as[value_id].getAddrs())
        {
            NodeID objId = as.getIDFromAddr(addr);
            if (svfir->getBaseObject(objId)->isConstantByteSize())
            {
                dst_size = svfir->getBaseObject(objId)->getByteSizeOfObj();
            }
            else
            {
                const ICFGNode* addrNode = svfir->getBaseObject(objId)->getICFGNode();
                for (const SVFStmt* stmt2: addrNode->getSVFStmts())
                {
                    if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                    {
                        dst_size = as.getAllocaInstByteSize(addrStmt);
                    }
                }
            }
        }
        u32_t len = 0;
        NodeID dstid = strValue->getId();
        if (as.inVarToAddrsTable(dstid))
        {
            for (u32_t index = 0; index < dst_size; index++)
            {
                AbstractValue expr0 =
                    as.getGepObjAddrs(dstid, IntervalValue(index));
                AbstractValue val;
                for (const auto &addr: expr0.getAddrs())
                {
                    val.join_with(as.load(addr));
                }
                if (val.getInterval().is_numeral() && (char) val.getInterval().getIntNumeral() == '\0')
                {
                    break;
                }
                ++len;
            }
        }
        if (len == 0)
        {
            as[lhsId] = IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
        }
        else
        {
            as[lhsId] = IntervalValue(len);
        }
    };
    func_map["strlen"] = sse_strlen;
    func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallICFGNode *callNode)
    {
        // recv(sockfd, buf, len, flags);
        if (callNode->arg_size() < 4) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t len_id = callNode->getArgument(2)->getId();
        IntervalValue len = as[len_id].getInterval() - IntervalValue(1);
        u32_t lhsId = callNode->getRetICFGNode()->getActualRet()->getId();
        as[lhsId] = len;
    };
    func_map["recv"] = sse_recv;
    func_map["__recv"] = sse_recv;

    auto sse_free = [&](const CallICFGNode *callNode)
    {
        if (callNode->arg_size() < 1) return;
        AbstractState& as = getAbsStateFromTrace(callNode);
        const u32_t freePtr = callNode->getArgument(0)->getId();
        for (auto addr: as[freePtr].getAddrs())
        {
            if (AbstractState::isInvalidMem(addr))
            {
                // Detected a double free — the address has already been freed.
                // No action is taken at this point.
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

AbstractState& AbsExtAPI::getAbsStateFromTrace(const SVF::ICFGNode* node)
{
    const ICFGNode* repNode = icfg->getRepNode(node);
    if (abstractTrace.count(repNode) == 0)
    {
        assert(0 && "No preAbsTrace for this node");
    }
    else
    {
        return abstractTrace[repNode];
    }
}

std::string AbsExtAPI::strRead(AbstractState& as, const SVFVar* rhs)
{
    // sse read string nodeID->string
    std::string str0;

    for (u32_t index = 0; index < Options::MaxFieldLimit(); index++)
    {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (!as.inVarToAddrsTable(rhs->getId())) continue;
        AbstractValue expr0 =
            as.getGepObjAddrs(rhs->getId(), IntervalValue(index));

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
    AbstractState& as = getAbsStateFromTrace(call);
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
                u32_t lhsId = ret->getId();
                if (as.inVarToAddrsTable(lhsId))
                {

                }
                else
                {
                    as[lhsId] = IntervalValue();
                }
            }
            return;
        }
    }
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    else if (extType == MEMCPY)
    {
        IntervalValue len = as[call->getArgument(2)->getId()].getInterval();
        svfir->getGNode(call->getArgument(0)->getId());
        handleMemcpy(as, call->getArgument(0), call->getArgument(1), len, 0);
    }
    else if (extType == MEMSET)
    {
        // memset dst is arg0, elem is arg1, size is arg2
        IntervalValue len = as[call->getArgument(2)->getId()].getInterval();
        IntervalValue elem = as[call->getArgument(1)->getId()].getInterval();
        handleMemset(as, call->getArgument(0), elem, len);
    }
    else if (extType == STRCPY)
    {
        handleStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        handleStrcat(call);
    }
    else
    {

    }
    return;
}

void AbsExtAPI::handleStrcpy(const CallICFGNode *call)
{
    // strcpy, __strcpy_chk, stpcpy , wcscpy, __wcscpy_chk
    // get the dst and src
    AbstractState& as = getAbsStateFromTrace(call);
    const SVFVar* arg0Val = call->getArgument(0);
    const SVFVar* arg1Val = call->getArgument(1);
    IntervalValue strLen = getStrlen(as, arg1Val);
    // no need to -1, since it has \0 as the last byte
    handleMemcpy(as, arg0Val, arg1Val, strLen, strLen.lb().getIntNumeral());
}

IntervalValue AbsExtAPI::getStrlen(AbstractState& as, const SVF::SVFVar *strValue)
{
    NodeID value_id = strValue->getId();
    u32_t dst_size = 0;
    for (const auto& addr : as[value_id].getAddrs())
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
                    dst_size = as.getAllocaInstByteSize(addrStmt);
                }
            }
        }
    }
    u32_t len = 0;
    u32_t elemSize = 1;
    if (as.inVarToAddrsTable(value_id))
    {
        for (u32_t index = 0; index < dst_size; index++)
        {
            AbstractValue expr0 =
                as.getGepObjAddrs(value_id, IntervalValue(index));
            AbstractValue val;
            for (const auto &addr: expr0.getAddrs())
            {
                val.join_with(as.load(addr));
            }
            if (val.getInterval().is_numeral() && (char) val.getInterval().getIntNumeral() == '\0')
            {
                break;
            }
            ++len;
        }
        if (strValue->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(strValue->getType())->getTypeOfElement()->getByteSize();
        }
        else if (strValue->getType()->isPointerTy())
        {
            if (const SVFType* elemType = as.getPointeeElement(value_id))
            {
                if (elemType->isArrayTy())
                    elemSize = SVFUtil::dyn_cast<SVFArrayType>(elemType)->getTypeOfElement()->getByteSize();
                else
                    elemSize = elemType->getByteSize();
            }
            else
            {
                elemSize = 1;
            }
        }
        else
        {
            assert(false && "we cannot support this type");
        }
    }
    if (len == 0)
    {
        return IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
    }
    else
    {
        return IntervalValue(len * elemSize);
    }
}


void AbsExtAPI::handleStrcat(const SVF::CallICFGNode *call)
{
    // __strcat_chk, strcat, __wcscat_chk, wcscat, __strncat_chk, strncat, __wcsncat_chk, wcsncat
    // to check it is  strcat group or strncat group
    AbstractState& as = getAbsStateFromTrace(call);
    const FunObjVar *fun = call->getCalledFunction();
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};
    if (std::find(strcatGroup.begin(), strcatGroup.end(), fun->getName()) != strcatGroup.end())
    {
        const SVFVar* arg0Val = call->getArgument(0);
        const SVFVar* arg1Val = call->getArgument(1);
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue strLen1 = getStrlen(as, arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        handleMemcpy(as, arg0Val, arg1Val, strLen1, strLen0.lb().getIntNumeral());
        // do memcpy
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), fun->getName()) != strncatGroup.end())
    {
        const SVFVar* arg0Val = call->getArgument(0);
        const SVFVar* arg1Val = call->getArgument(1);
        const SVFVar* arg2Val = call->getArgument(2);
        IntervalValue arg2Num = as[arg2Val->getId()].getInterval();
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        handleMemcpy(as, arg0Val, arg1Val, arg2Num, strLen0.lb().getIntNumeral());
        // do memcpy
    }
    else
    {
        assert(false && "unknown strcat function, please add it to strcatGroup or strncatGroup");
    }
}

void AbsExtAPI::handleMemcpy(AbstractState& as, const SVF::SVFVar *dst, const SVF::SVFVar *src, IntervalValue len,  u32_t start_idx)
{
    u32_t dstId = dst->getId(); // pts(dstId) = {objid}  objbar objtypeinfo->getType().
    u32_t srcId = src->getId();
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy())
    {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    // memcpy(i32*, i32*, 40)
    else if (dst->getType()->isPointerTy())
    {
        if (const SVFType* elemType = as.getPointeeElement(dstId))
        {
            if (elemType->isArrayTy())
                elemSize = SVFUtil::dyn_cast<SVFArrayType>(elemType)->getTypeOfElement()->getByteSize();
            else
                elemSize = elemType->getByteSize();
        }
        else
        {
            elemSize = 1;
        }
    }
    else
    {
        assert(false && "we cannot support this type");
    }
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getIntNumeral());
    u32_t range_val = size / elemSize;
    if (as.inVarToAddrsTable(srcId) && as.inVarToAddrsTable(dstId))
    {
        for (u32_t index = 0; index < range_val; index++)
        {
            // dead loop for string and break if there's a \0. If no \0, it will throw err.
            AbstractValue expr_src =
                as.getGepObjAddrs(srcId, IntervalValue(index));
            AbstractValue expr_dst =
                as.getGepObjAddrs(dstId, IntervalValue(index + start_idx));
            for (const auto &dst: expr_dst.getAddrs())
            {
                for (const auto &src: expr_src.getAddrs())
                {
                    u32_t objId = as.getIDFromAddr(src);
                    if (as.inAddrToValTable(objId))
                    {
                        as.store(dst, as.load(src));
                    }
                    else if (as.inAddrToAddrsTable(objId))
                    {
                        as.store(dst, as.load(src));
                    }
                }
            }
        }
    }
}

void AbsExtAPI::handleMemset(AbstractState& as, const SVF::SVFVar *dst, IntervalValue elem, IntervalValue len)
{
    u32_t dstId = dst->getId();
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getIntNumeral());
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy())
    {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    else if (dst->getType()->isPointerTy())
    {
        if (const SVFType* elemType = as.getPointeeElement(dstId))
        {
            elemSize = elemType->getByteSize();
        }
        else
        {
            elemSize = 1;
        }
    }
    else
    {
        assert(false && "we cannot support this type");
    }

    u32_t range_val = size / elemSize;
    for (u32_t index = 0; index < range_val; index++)
    {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (as.inVarToAddrsTable(dstId))
        {
            AbstractValue lhs_gep = as.getGepObjAddrs(dstId, IntervalValue(index));
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
        else
            break;
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
                ub = static_cast<s64_t>(std::numeric_limits<u_int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u_int8_t>::min());
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