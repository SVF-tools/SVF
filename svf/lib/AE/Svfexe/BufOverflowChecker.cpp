//===- BufOverflowChecker.cpp -- BufOVerflowChecker Client for Abstract Execution---//
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
// Created by Jiawei Wang on 2024/1/12.
//

#include "AE/Svfexe/BufOverflowChecker.h"
#include "Util/WorkList.h"
#include "SVFIR/SVFType.h"
#include "Util/Options.h"
#include <climits>

namespace SVF
{

std::string IntervalToIntStr(const IntervalValue& inv)
{
    if (inv.is_infinite())
    {
        return inv.toString();
    }
    else
    {
        s64_t lb_val = inv.lb().getNumeral();
        s64_t ub_val = inv.ub().getNumeral();

        // check lb
        s32_t lb_s32 = (lb_val < static_cast<s64_t>(INT_MIN)) ? INT_MIN :
                       (lb_val > static_cast<s64_t>(INT_MAX)) ? INT_MAX :
                       static_cast<s32_t>(lb_val);

        // check ub
        s32_t ub_s32 = (ub_val < static_cast<s64_t>(INT_MIN)) ? INT_MIN :
                       (ub_val > static_cast<s64_t>(INT_MAX)) ? INT_MAX :
                       static_cast<s32_t>(ub_val);

        return "[" + std::to_string(lb_s32) + ", " + std::to_string(ub_s32) + "]";
    }
}

void BufOverflowChecker::handleSVFStatement(const SVFStmt *stmt)
{
    AbstractInterpretation::handleSVFStatement(stmt);
    AbstractState& as = getAbsState(stmt->getICFGNode());
    // for gep stmt, add the gep stmt to the addrToGep map
    if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        for (NodeID addrID:
                _svfir2AbsState->getAddrs(as, gep->getLHSVarID()).getAddrs())
        {
            NodeID objId = AbstractState::getInternalID(addrID);
            _addrToGep[objId] = gep;
        }
    }
}

void BufOverflowChecker::initExtAPIBufOverflowCheckRules()
{
    //void llvm_memcpy_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void llvm_memcpy(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy"] = {{0, 2}, {1,2}};
    //void llvm_memmove(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void __memcpy_chk(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["__memcpy_chk"] = {{0, 2}, {1,2}};
    //void *memmove(void *str1, const void *str2, unsigned long n)
    _extAPIBufOverflowCheckRules["memmove"] = {{0, 2}, {1,2}};
    //void bcopy(const void *s1, void *s2, unsigned long n){}
    _extAPIBufOverflowCheckRules["bcopy"] = {{0, 2}, {1,2}};
    //void *memccpy( void * restrict dest, const void * restrict src, int c, unsigned long count)
    _extAPIBufOverflowCheckRules["memccpy"] = {{0, 3}, {1,3}};
    //void __memmove_chk(char* dst, char* src, int sz){}
    _extAPIBufOverflowCheckRules["__memmove_chk"] = {{0, 2}, {1,2}};
    //void llvm_memset(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset"] = {{0, 2}};
    //void llvm_memset_p0i8_i32(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i32"] = {{0, 2}};
    //void llvm_memset_p0i8_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i64"] = {{0, 2}};
    //void llvm_memset_p0_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0_i64"] = {{0, 2}};
    //char *__memset_chk(char * dest, int c, unsigned long destlen, int flag)
    _extAPIBufOverflowCheckRules["__memset_chk"] = {{0, 2}};
    //char *wmemset(wchar_t * dst, wchar_t elem, int sz, int flag) {
    _extAPIBufOverflowCheckRules["wmemset"] = {{0, 2}};
    //char *strncpy(char *dest, const char *src, unsigned long n)
    _extAPIBufOverflowCheckRules["strncpy"] = {{0, 2}, {1,2}};
    //unsigned long iconv(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft)
    _extAPIBufOverflowCheckRules["iconv"] = {{1, 2}, {3, 4}};
}


bool BufOverflowChecker::detectStrcpy(const CallICFGNode *call)
{
    AbstractState& as = getAbsState(call);
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    const SVFValue* arg0Val = cs.getArgument(0);
    const SVFValue* arg1Val = cs.getArgument(1);
    IntervalValue strLen = getStrlen(as, arg1Val);
    // no need to -1, since it has \0 as the last byte
    return canSafelyAccessMemory(arg0Val, strLen, call);
}

void BufOverflowChecker::initExtFunMap()
{

    auto sse_scanf = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsState(callNode);
        //scanf("%d", &data);
        if (cs.arg_size() < 2) return;

        u32_t dst_id = _svfir->getValueNode(cs.getArgument(1));
        if (!_svfir2AbsState->inVarToAddrsTable(as, dst_id))
        {
            BufOverflowException bug("scanf may cause buffer overflow.\n", 0, 0, 0, 0, cs.getArgument(1));
            addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
            return;
        }
        else
        {
            AbstractValue Addrs = _svfir2AbsState->getAddrs(as, dst_id);
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = AbstractState::getInternalID(vaddr);
                AbstractValue range = _svfir2AbsState->getRangeLimitFromType(_svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
        }
    };
    auto sse_fscanf = [&](const CallSite &cs)
    {
        //fscanf(stdin, "%d", &data);
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsState(callNode);
        u32_t dst_id = _svfir->getValueNode(cs.getArgument(2));
        if (!_svfir2AbsState->inVarToAddrsTable(as, dst_id))
        {
            BufOverflowException bug("scanf may cause buffer overflow.\n", 0, 0, 0, 0, cs.getArgument(2));
            addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
            return;
        }
        else
        {
            AbstractValue Addrs = _svfir2AbsState->getAddrs(as, dst_id);
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = AbstractState::getInternalID(vaddr);
                AbstractValue range = _svfir2AbsState->getRangeLimitFromType(_svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
        }
    };

    _func_map["__isoc99_fscanf"] = sse_fscanf;
    _func_map["__isoc99_scanf"] = sse_scanf;
    _func_map["__isoc99_vscanf"] = sse_scanf;
    _func_map["fscanf"] = sse_fscanf;
    _func_map["scanf"] = sse_scanf;
    _func_map["sscanf"] = sse_scanf;
    _func_map["__isoc99_sscanf"] = sse_scanf;
    _func_map["vscanf"] = sse_scanf;

    auto sse_fread = [&](const CallSite &cs)
    {
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsState(callNode);
        u32_t block_count_id = _svfir->getValueNode(cs.getArgument(2));
        u32_t block_size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue block_count = as[block_count_id].getInterval();
        IntervalValue block_size = as[block_size_id].getInterval();
        IntervalValue block_byte = block_count * block_size;
        canSafelyAccessMemory(cs.getArgument(0), block_byte, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["fread"] = sse_fread;

    auto sse_sprintf = [&](const CallSite &cs)
    {
        // printf is difficult to predict since it has no byte size arguments
    };

    auto sse_snprintf = [&](const CallSite &cs)
    {
        if (cs.arg_size() < 2) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsState(callNode);
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        u32_t dst_id = _svfir->getValueNode(cs.getArgument(0));
        // get elem size of arg2
        u32_t elemSize = 1;
        if (cs.getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(cs.getArgument(2)->getType())->getTypeOfElement()->getByteSize();
        }
        else if (cs.getArgument(2)->getType()->isPointerTy())
        {
            elemSize = getPointeeElement(as, _svfir->getValueNode(cs.getArgument(2)))->getByteSize();
        }
        else
        {
            return;
            // assert(false && "we cannot support this type");
        }
        IntervalValue size = as[size_id].getInterval() * IntervalValue(elemSize) - IntervalValue(1);
        if (!as.inVarToAddrsTable(dst_id))
        {
            if (Options::BufferOverflowCheck())
            {
                BufOverflowException bug(
                    "snprintf dst_id or dst is not defined nor initializesd.\n",
                    0, 0, 0, 0, cs.getArgument(0));
                addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
                return;
            }
        }
        canSafelyAccessMemory(cs.getArgument(0), size, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["__snprintf_chk"] = sse_snprintf;
    _func_map["__vsprintf_chk"] = sse_sprintf;
    _func_map["__sprintf_chk"] = sse_sprintf;
    _func_map["snprintf"] = sse_snprintf;
    _func_map["sprintf"] = sse_sprintf;
    _func_map["vsprintf"] = sse_sprintf;
    _func_map["vsnprintf"] = sse_snprintf;
    _func_map["__vsnprintf_chk"] = sse_snprintf;
    _func_map["swprintf"] = sse_snprintf;
    _func_map["_snwprintf"] = sse_snprintf;


    auto sse_itoa = [&](const CallSite &cs)
    {
        // itoa(num, ch, 10);
        // num: int, ch: char*, 10 is decimal
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsState(callNode);
        u32_t num_id = _svfir->getValueNode(cs.getArgument(0));

        u32_t num = (u32_t) as[num_id].getInterval().getNumeral();
        std::string snum = std::to_string(num);
        canSafelyAccessMemory(cs.getArgument(1), IntervalValue((s32_t)snum.size()), _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallSite &cs)
    {
        // check the arg size
        if (cs.arg_size() < 1) return;
        const SVFValue* strValue = cs.getArgument(0);
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsState(callNode);
        IntervalValue dst_size = getStrlen(as, strValue);
        u32_t elemSize = 1;
        if (strValue->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(strValue->getType())->getTypeOfElement()->getByteSize();
        }
        else if (strValue->getType()->isPointerTy())
        {
            if (const SVFType* pointee = getPointeeElement(as, _svfir->getValueNode(strValue)))
                elemSize = pointee->getByteSize();
            else
                elemSize = 1;
        }
        u32_t lhsId = _svfir->getValueNode(cs.getInstruction());
        as[lhsId] = dst_size / IntervalValue(elemSize);
    };
    _func_map["strlen"] = sse_strlen;
    _func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallSite &cs)
    {
        // recv(sockfd, buf, len, flags);
        if (cs.arg_size() < 4) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsState(callNode);
        u32_t len_id = _svfir->getValueNode(cs.getArgument(2));
        IntervalValue len = as[len_id].getInterval() - IntervalValue(1);
        u32_t lhsId = _svfir->getValueNode(cs.getInstruction());
        as[lhsId] = len;
        canSafelyAccessMemory(cs.getArgument(1), len, _svfir->getICFG()->getICFGNode(cs.getInstruction()));;
    };
    _func_map["recv"] = sse_recv;
    _func_map["__recv"] = sse_recv;
    auto safe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        _checkpoints.erase(callNode);
        //void SAFE_BUFACCESS(void* data, int size);
        if (cs.arg_size() < 2) return;
        AbstractState&as = getAbsState(callNode);
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            val = IntervalValue(0);
            assert(false && "SAFE_BUFACCESS size is bottom");
        }
        bool isSafe = canSafelyAccessMemory(cs.getArgument(0), val, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
        if (isSafe)
        {
            std::cout << "safe buffer access success\n";
            return;
        }
        else
        {
            std::string err_msg = "this SAFE_BUFACCESS should be a safe access but detected buffer overflow. Pos: ";
            err_msg += cs.getInstruction()->getSourceLoc();
            std::cerr << err_msg << std::endl;
            assert(false);
        }
    };
    _func_map["SAFE_BUFACCESS"] = safe_bufaccess;

    auto unsafe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        _checkpoints.erase(callNode);
        //void UNSAFE_BUFACCESS(void* data, int size);
        if (cs.arg_size() < 2) return;
        AbstractState&as = getAbsState(callNode);
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            assert(false && "UNSAFE_BUFACCESS size is bottom");
        }
        bool isSafe = canSafelyAccessMemory(cs.getArgument(0), val, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
        if (!isSafe)
        {
            std::cout << "detect buffer overflow success\n";
            return;
        }
        else
        {
            // if it is safe, it means it is wrongly labeled, assert false.
            std::string err_msg = "this UNSAFE_BUFACCESS should be a buffer overflow but not detected. Pos: ";
            err_msg += cs.getInstruction()->getSourceLoc();
            std::cerr << err_msg << std::endl;
            assert(false);
        }
    };
    _func_map["UNSAFE_BUFACCESS"] = unsafe_bufaccess;

    // init _checkpoint_names
    _checkpoint_names.insert("SAFE_BUFACCESS");
    _checkpoint_names.insert("UNSAFE_BUFACCESS");
}

bool BufOverflowChecker::detectStrcat(const CallICFGNode *call)
{
    AbstractState& as = getAbsState(call);
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    // check the arg size
    // if it is strcat group, we need to check the length of string,
    // e.g. strcat(str1, str2); which checks AllocSize(str1) >= Strlen(str1) + Strlen(str2);
    // if it is strncat group, we do not need to check the length of string,
    // e.g. strncat(str1, str2, n); which checks AllocSize(str1) >= Strlen(str1) + n;

    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};
    if (std::find(strcatGroup.begin(), strcatGroup.end(), fun->getName()) != strcatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue strLen1 = getStrlen(as, arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), fun->getName()) != strncatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg2Val = cs.getArgument(2);
        IntervalValue arg2Num = as[_svfir->getValueNode(arg2Val)].getInterval();
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
    }
    else
    {
        assert(false && "unknown strcat function, please add it to strcatGroup or strncatGroup");
        abort();
    }
}

void BufOverflowChecker::handleExtAPI(const CallICFGNode *call)
{
    AbstractState& as = getAbsState(call);
    AbstractInterpretation::handleExtAPI(call);
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    assert(fun && "SVFFunction* is nullptr");
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    // check the type of mem api,
    // MEMCPY: like memcpy, memcpy_chk, llvm.memcpy etc.
    // MEMSET: like memset, memset_chk, llvm.memset etc.
    // STRCPY: like strcpy, strcpy_chk, wcscpy etc.
    // STRCAT: like strcat, strcat_chk, wcscat etc.
    // for other ext api like printf, scanf, etc., they have their own handlers
    ExtAPIType extType = UNCLASSIFIED;
    // get type of mem api
    for (const std::string &annotation: fun->getAnnotations())
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
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    if (extType == MEMCPY)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        // call parseMemcpyBufferCheckArgs to parse the BUF_CHECK annotation
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = as[_svfir->getValueNode(cs.getArgument(arg.second))].getInterval() - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    // 2. memset functions like memset, memset_chk, annotate("MEMSET"), annotate("BUF_CHECK:Arg0, Arg2")
    else if (extType == MEMSET)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = as[_svfir->getValueNode(cs.getArgument(arg.second))].getInterval() - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    else if (extType == STRCPY)
    {
        detectStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        detectStrcat(call);
    }
    else
    {

    }
    return;
}

bool BufOverflowChecker::canSafelyAccessMemory(const SVFValue *value, const IntervalValue &len, const ICFGNode *curNode)
{
    AbstractState& as = getAbsState(curNode);
    const SVFValue *firstValue = value;
    /// Usually called by a GepStmt overflow check, or external API (like memcpy) overflow check
    /// Defitions of Terms:
    /// source node: malloc or gepStmt(array), sink node: gepStmt or external API (like memcpy)
    /// e.g. 1) a = malloc(10), a[11] = 10, a[11] is the sink node, a is the source node (malloc)
    ///  2) A = struct {int a[10];}, A.a[11] = 10, A.a[11] is the sink, A.a is the source node (gepStmt(array))

    /// it tracks the value flow from sink to source, and accumulates offset
    /// then compare the accumulated offset and malloc size (or gepStmt array size)
    SVF::FILOWorkList<const SVFValue *> worklist;
    Set<const SVFValue *> visited;
    visited.insert(value);
    Map<const ICFGNode *, IntervalValue> gep_offsets;
    IntervalValue total_bytes = len;
    worklist.push(value);
    std::vector<const CallICFGNode *> callstack = _callSiteStack;
    while (!worklist.empty())
    {
        value = worklist.pop();
        if (const SVFInstruction *ins = SVFUtil::dyn_cast<SVFInstruction>(value))
        {
            const ICFGNode *node = _svfir->getICFG()->getICFGNode(ins);
            if (const CallICFGNode *callnode = SVFUtil::dyn_cast<CallICFGNode>(node))
            {
                AccessMemoryViaRetNode(callnode, worklist, visited);
            }
            for (const SVFStmt *stmt: node->getSVFStmts())
            {
                if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
                {
                    AccessMemoryViaCopyStmt(copy, worklist, visited);
                }
                else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
                {
                    AccessMemoryViaLoadStmt(as, load, worklist, visited);
                }
                else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
                {
                    // there are 3 type of gepStmt
                    // 1. ptr get offset
                    // 2. struct get field
                    // 3. array get element
                    // for array gep, there are two kind of overflow checking
                    //  Arr [Struct.C * 10] arr, Struct.C {i32 a, i32 b}
                    //     arr[11].a = **, it is "lhs = gep *arr, 0 (ptr), 11 (arrIdx), 0 (ptr), 0(struct field)"
                    //  1) in this case arrIdx 11 is overflow.
                    //  Other case,
                    //   Struct.C {i32 a, [i32*10] b, i32 c}, C.b[11] = 1
                    //   it is "lhs - gep *C, 0(ptr), 1(struct field), 0(ptr), 11(arrIdx)"
                    //  2) in this case arrIdx 11 is larger than its getOffsetVar.Type Array([i32*10])

                    // therefore, if last getOffsetVar.Type is not the Array, just check the overall offset and its
                    // gep source type size (together with totalOffset along the value flow).
                    //   so if curgepOffset + totalOffset >= gepSrc (overflow)
                    //      else totalOffset += curgepOffset

                    // otherwise, if last getOffsetVar.Type is the Array, check the last idx and array. (just offset,
                    //  not with totalOffset during check)
                    //  so if getOffsetVarVal > getOffsetVar.TypeSize (overflow)
                    //     else safe and return.
                    IntervalValue byteOffset;
                    if (gep->isConstantOffset())
                    {
                        byteOffset = IntervalValue(gep->accumulateConstantByteOffset());
                    }
                    else
                    {
                        byteOffset =
                            _svfir2AbsState->getByteOffset(as, gep);
                    }
                    // for variable offset, join with accumulate gep offset
                    gep_offsets[gep->getICFGNode()] = byteOffset;
                    if (byteOffset.ub().getNumeral() >= Options::MaxFieldLimit() && Options::GepUnknownIdx())
                    {
                        return true;
                    }

                    if (gep->getOffsetVarAndGepTypePairVec().size() > 0)
                    {
                        const SVFVar *gepVal = gep->getOffsetVarAndGepTypePairVec().back().first;
                        const SVFType *gepType = gep->getOffsetVarAndGepTypePairVec().back().second;

                        if (gepType->isArrayTy())
                        {
                            const SVFArrayType *gepArrType = SVFUtil::dyn_cast<SVFArrayType>(gepType);
                            IntervalValue gepArrTotalByte(0);
                            const SVFValue *idxValue = gepVal->getValue();
                            u32_t arrElemSize = gepArrType->getTypeOfElement()->getByteSize();
                            if (const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(idxValue))
                            {
                                u32_t lb = (double) Options::MaxFieldLimit() / arrElemSize >= op->getSExtValue() ?
                                           op->getSExtValue() * arrElemSize : Options::MaxFieldLimit();
                                gepArrTotalByte = gepArrTotalByte + IntervalValue(lb, lb);
                            }
                            else
                            {
                                u32_t idx = _svfir->getValueNode(idxValue);
                                IntervalValue idxVal = as[idx].getInterval();
                                if (idxVal.isBottom())
                                {
                                    gepArrTotalByte = gepArrTotalByte + IntervalValue(0, 0);
                                }
                                else
                                {
                                    u32_t ub = (idxVal.ub().getNumeral() < 0) ? 0 :
                                               (double) Options::MaxFieldLimit() / arrElemSize >=
                                               idxVal.ub().getNumeral() ?
                                               arrElemSize * idxVal.ub().getNumeral() : Options::MaxFieldLimit();
                                    u32_t lb = (idxVal.lb().getNumeral() < 0) ? 0 :
                                               ((double) Options::MaxFieldLimit() / arrElemSize >=
                                                idxVal.lb().getNumeral()) ?
                                               arrElemSize * idxVal.lb().getNumeral() : Options::MaxFieldLimit();
                                    gepArrTotalByte = gepArrTotalByte + IntervalValue(lb, ub);
                                }
                            }
                            total_bytes = total_bytes + gepArrTotalByte;
                            if (total_bytes.ub().getNumeral() >= gepArrType->getByteSize())
                            {
                                std::string msg =
                                    "Buffer overflow!! Accessing buffer range: " +
                                    IntervalToIntStr(total_bytes) +
                                    "\nAllocated Gep buffer size: " +
                                    std::to_string(gepArrType->getByteSize()) + "\n";
                                msg += "Position: " + firstValue->toString() + "\n";
                                msg += " The following is the value flow. [[\n";
                                for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                                {
                                    msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) +
                                           "\n";
                                }
                                msg += "]].\nAlloc Site: " + gep->toString() + "\n";

                                BufOverflowException bug(SVFUtil::errMsg(msg), gepArrType->getByteSize(),
                                                         gepArrType->getByteSize(),
                                                         total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(),
                                                         firstValue);
                                addBugToRecoder(bug, curNode);
                                return false;
                            }
                            else
                            {
                                // for gep last index's type is arr, stop here.
                                return true;
                            }
                        }
                        else
                        {
                            total_bytes = total_bytes + byteOffset;
                        }

                    }
                    if (!visited.count(gep->getRHSVar()->getValue()))
                    {
                        visited.insert(gep->getRHSVar()->getValue());
                        worklist.push(gep->getRHSVar()->getValue());
                    }
                }
                else if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
                {
                    // addrStmt is source node.
                    u32_t arr_type_size = getAllocaInstByteSize(as, addr);
                    if (total_bytes.ub().getNumeral() >= arr_type_size ||
                            total_bytes.lb().getNumeral() < 0)
                    {
                        std::string msg =
                            "Buffer overflow!! Accessing buffer range: " + IntervalToIntStr(total_bytes) +
                            "\nAllocated buffer size: " + std::to_string(arr_type_size) + "\n";
                        msg += "Position: " + firstValue->toString() + "\n";
                        msg += " The following is the value flow. [[\n";
                        for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                        {
                            msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) + "\n";
                        }
                        msg += "]].\n Alloc Site: " + addr->toString() + "\n";
                        BufOverflowException bug(SVFUtil::wrnMsg(msg), arr_type_size, arr_type_size,
                                                 total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(),
                                                 firstValue);
                        addBugToRecoder(bug, curNode);
                        return false;
                    }
                    else
                    {

                        return true;
                    }
                }
            }
        }
        else if (const SVF::SVFGlobalValue *gvalue = SVFUtil::dyn_cast<SVF::SVFGlobalValue>(value))
        {
            u32_t arr_type_size = 0;
            const SVFType *svftype = gvalue->getType();
            if (SVFUtil::isa<SVFPointerType>(svftype))
            {
                if (const SVFArrayType *ptrArrType = SVFUtil::dyn_cast<SVFArrayType>(
                        getPointeeElement(as, _svfir->getValueNode(gvalue))))
                    arr_type_size = ptrArrType->getByteSize();
                else
                    arr_type_size = svftype->getByteSize();
            }
            else
                arr_type_size = svftype->getByteSize();

            if (total_bytes.ub().getNumeral() >= arr_type_size || total_bytes.lb().getNumeral() < 0)
            {
                std::string msg = "Buffer overflow!! Accessing buffer range: " + IntervalToIntStr(total_bytes) +
                                  "\nAllocated buffer size: " + std::to_string(arr_type_size) + "\n";
                msg += "Position: " + firstValue->toString() + "\n";
                msg += " The following is the value flow.\n[[";
                for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                {
                    msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) + "\n";
                }
                msg += "]]. \nAlloc Site: " + gvalue->toString() + "\n";

                BufOverflowException bug(SVFUtil::wrnMsg(msg), arr_type_size, arr_type_size,
                                         total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(), firstValue);
                addBugToRecoder(bug, curNode);
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (const SVF::SVFArgument *arg = SVFUtil::dyn_cast<SVF::SVFArgument>(value))
        {
            AccessMemoryViaCallArgs(arg, worklist, visited);
        }
        else
        {
            // maybe SVFConstant
            // it may be cannot find the source, maybe we start from non-main function,
            // therefore it loses the value flow track
            return true;
        }
    }
    // it may be cannot find the source, maybe we start from non-main function,
    // therefore it loses the value flow track
    return true;
}



void BufOverflowChecker::handleICFGNode(const SVF::ICFGNode *node)
{
    AbstractInterpretation::handleICFGNode(node);
    detectBufOverflow(node);
}

//
bool BufOverflowChecker::detectBufOverflow(const ICFGNode *node)
{
    AbstractState &as = getAbsState(node);
    for (auto* stmt: node->getSVFStmts())
    {
        if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            const SVFVar* gepRhs = gep->getRHSVar();
            if (const SVFInstruction* inst = SVFUtil::dyn_cast<SVFInstruction>(gepRhs->getValue()))
            {
                const ICFGNode* icfgNode = _svfir->getICFG()->getICFGNode(inst);
                for (const SVFStmt* stmt2: icfgNode->getSVFStmts())
                {
                    if (const GepStmt *gep2 = SVFUtil::dyn_cast<GepStmt>(stmt2))
                    {
                        return canSafelyAccessMemory(gep2->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
        else if (const LoadStmt* load =  SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            if (_svfir2AbsState->inVarToAddrsTable(as, load->getRHSVarID()))
            {
                AbstractValue Addrs =
                    _svfir2AbsState->getAddrs(as, load->getRHSVarID());
                for (auto vaddr: Addrs.getAddrs())
                {
                    u32_t objId = AbstractState::getInternalID(vaddr);
                    if (_addrToGep.find(objId) != _addrToGep.end())
                    {
                        const GepStmt* gep = _addrToGep.at(objId);
                        return canSafelyAccessMemory(gep->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
        else if (const StoreStmt* store =  SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            if (_svfir2AbsState->inVarToAddrsTable(as, store->getLHSVarID()))
            {
                AbstractValue Addrs =
                    _svfir2AbsState->getAddrs(as, store->getLHSVarID());
                for (auto vaddr: Addrs.getAddrs())
                {
                    u32_t objId = AbstractState::getInternalID(vaddr);
                    if (_addrToGep.find(objId) != _addrToGep.end())
                    {
                        const GepStmt* gep = _addrToGep.at(objId);
                        return canSafelyAccessMemory(gep->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
    }
    return true;
}

void BufOverflowChecker::addBugToRecoder(const BufOverflowException& e, const ICFGNode* node)
{
    const SVFInstruction* inst = nullptr;
    if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        inst = call->getCallSite();
    }
    else
    {
        inst = node->getSVFStmts().back()->getInst();
    }
    GenericBug::EventStack eventStack;
    SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, inst);
    for (const auto &callsite: _callSiteStack)
    {
        SVFBugEvent callSiteEvent(SVFBugEvent::EventType::CallSite, callsite->getCallSite());
        eventStack.push_back(callSiteEvent);
    }
    eventStack.push_back(sourceInstEvent);
    if (eventStack.size() == 0) return;
    std::string loc = eventStack.back().getEventLoc();
    if (_bugLoc.find(loc) != _bugLoc.end())
    {
        return;
    }
    else
    {
        _bugLoc.insert(loc);
    }
    _recoder.addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, e.getAllocLb(), e.getAllocUb(), e.getAccessLb(),
                           e.getAccessUb());
    _nodeToBugInfo[node] = e.what();
}

}
