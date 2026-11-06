//===- AEDetector.cpp -- Vulnerability Detectors---------------------------------//
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
//  Created on: May 1, 2025
//      Author: Xiao Cheng, Jiawei Wang, Mingxiu Wang
//

#include <AE/Svfexe/AEDetector.h>
#include <AE/Svfexe/AbsExtAPI.h>
#include <AE/Svfexe/AbstractInterpretation.h>
#include "AE/Core/AddressDomain.h"
#include <algorithm>

using namespace SVF;

namespace
{
Interval integerInterval(s64_t value)
{
    return Interval::singleton(Rational(value));
}

bool upperAtLeast(const Interval& interval, u32_t size)
{
    return interval.upper().isPlusInfinity() ||
           (interval.upper().isFinite() && interval.upper().value() >= Rational(size));
}
}

/**
 * @brief Detects buffer overflow issues within a given ICFG node.
 *
 * This function handles both non-call nodes, where it analyzes GEP (GetElementPtr)
 * instructions for potential buffer overflows, and call nodes, where it checks
 * for external API calls that may cause overflows.
 *
 * @param as Reference to the abstract state.
 * @param node Pointer to the ICFG node.
 */
void BufOverflowDetector::detect(const ICFGNode* node)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    if (!SVFUtil::isa<CallICFGNode>(node))
    {
        // Handle non-call nodes by analyzing GEP instructions
        for (const SVFStmt* stmt : node->getSVFStmts())
        {
            if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
            {
                SVFIR* svfir = PAG::getPAG();

                // Update the GEP object offset from its base
                const AddressSet lhsVal = ae.getAddressSet(gep->getLHSVar(), node);
                const AddressSet objAddrs = ae.getAddressSet(gep->getRHSVar(), node);
                updateGepObjOffsetFromBase(node, lhsVal, objAddrs, ae.getGepByteOffset(gep));
                for (const auto& addr : objAddrs)
                {
                    const ObjVar* object = ae.objectAt(addr);
                    NodeID objId = object ? object->getId() : IRGraph::NullPtr;
                    if (!object || svfir->isNullOrBlkObj(objId))
                    {
                        AEException bug(stmt->toString());
                        addBugToReporter(bug, stmt->getICFGNode());
                        continue;
                    }
                    u32_t size = 0;
                    u32_t lower = 0;
                    // like `int arr[10]` which has constant size before runtime
                    if (svfir->getBaseObject(objId)->isConstantByteSize())
                    {
                        size = svfir->getBaseObject(objId)->getByteSizeOfObj();
                        lower = size;
                    }
                    else
                    {
                        // like `int len = ***; int arr[len]`, whose size can only be known in runtime
                        const ICFGNode* addrNode = svfir->getBaseObject(objId)->getICFGNode();
                        if (!addrNode)
                        {
                            AEException bug(stmt->toString());
                            addBugToReporter(bug, stmt->getICFGNode());
                            continue;
                        }
                        for (const SVFStmt* stmt2 : addrNode->getSVFStmts())
                        {
                            if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                            {
                                const auto bounds = ae.getAllocaInstByteSize(addrStmt);
                                size = bounds.upperBound;
                                lower = bounds.lowerBound;
                            }
                        }
                    }

                    // Calculate access offset and check for potential overflow
                    Interval accessOffset = getAccessOffset(objId, gep);
                    if (upperAtLeast(accessOffset, size))
                    {
                        AEException bug(stmt->toString());
                        addBugToReporter(bug, stmt->getICFGNode(),
                        {size, lower, accessOffset});
                    }
                }
            }
        }
    }
    else
    {
        // Handle call nodes by checking for external API calls
        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(node);
        if (SVFUtil::isExtCall(callNode->getCalledFunction()))
        {
            detectExtAPI(callNode);
        }
    }
}

/**
 * @brief Handles stub functions within the ICFG node.
 *
 * This function is a placeholder for handling stub functions within the ICFG node.
 *
 * @param node Pointer to the ICFG node.
 */
void BufOverflowDetector::handleStubFunctions(const SVF::CallICFGNode* callNode)
{
    // get function name
    std::string funcName = callNode->getCalledFunction()->getName();
    auto& ae = AbstractInterpretation::getAEInstance();
    if (funcName == "SAFE_BUFACCESS")
    {
        ae.getUtils()->checkpoints.erase(callNode);
        if (callNode->arg_size() < 2)
            return;
        Interval val = ae.getInterval(callNode->getArgument(1), callNode);
        if (val.isBottom())
        {
            val = integerInterval(0);
            assert(false && "SAFE_BUFACCESS size is bottom");
        }
        const ValVar* arg0Val = callNode->getArgument(0);
        bool isSafe = canSafelyAccessMemory(arg0Val, val, callNode);
        if (isSafe)
        {
            SVFUtil::outs() << SVFUtil::sucMsg("success: expected safe buffer access at SAFE_BUFACCESS")
                            << " — " << callNode->toString() << "\n";
            return;
        }
        else
        {
            SVFUtil::outs() << SVFUtil::errMsg("failure: unexpected buffer overflow at SAFE_BUFACCESS")
                            << " — Position: " << callNode->getSourceLoc() << "\n";
            assert(false);
        }
    }
    else if (funcName == "UNSAFE_BUFACCESS")
    {
        ae.getUtils()->checkpoints.erase(callNode);
        if (callNode->arg_size() < 2) return;
        Interval val = ae.getInterval(callNode->getArgument(1), callNode);
        if (val.isBottom())
        {
            assert(false && "UNSAFE_BUFACCESS size is bottom");
        }
        const ValVar* arg0Val = callNode->getArgument(0);
        bool isSafe = canSafelyAccessMemory(arg0Val, val, callNode);
        if (!isSafe)
        {
            SVFUtil::outs() << SVFUtil::sucMsg("success: expected buffer overflow at UNSAFE_BUFACCESS")
                            << " — " << callNode->toString() << "\n";
            return;
        }
        else
        {
            SVFUtil::outs() << SVFUtil::errMsg("failure: buffer overflow expected at UNSAFE_BUFACCESS, but none detected")
                            << " — Position: " << callNode->getSourceLoc() << "\n";
            assert(false);
        }
    }
}

/**
 * @brief Initializes external API buffer overflow check rules.
 *
 * This function sets up rules for various memory-related functions like memcpy,
 * memset, etc., defining which arguments should be checked for buffer overflows.
 */
void BufOverflowDetector::initExtAPIBufOverflowCheckRules()
{
    extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memcpy_p0_p0_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i32"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memcpy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i32"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["__memcpy_chk"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["memmove"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["bcopy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["memccpy"] = {{0, 3}, {1, 3}};
    extAPIBufOverflowCheckRules["__memmove_chk"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memset"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0i8_i32"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0i8_i64"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0_i64"] = {{0, 2}};
    extAPIBufOverflowCheckRules["__memset_chk"] = {{0, 2}};
    extAPIBufOverflowCheckRules["wmemset"] = {{0, 2}};
    extAPIBufOverflowCheckRules["strncpy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["iconv"] = {{1, 2}, {3, 4}};
}

/**
 * @brief Handles external API calls related to buffer overflow detection.
 *
 * This function checks the type of external memory API (e.g., memcpy, memset, strcpy, strcat)
 * and applies the corresponding buffer overflow checks based on predefined rules.
 *
 * @param call Pointer to the call ICFG node.
 */
void BufOverflowDetector::detectExtAPI(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "FunObjVar* is nullptr");
    auto& ae = AbstractInterpretation::getAEInstance();

    AbsExtAPI::ExtAPIType extType = AbsExtAPI::UNCLASSIFIED;

    // Determine the type of external memory API
    for (const std::string &annotation : ExtAPI::getExtAPI()->getExtFuncAnnotations(call->getCalledFunction()))
    {
        if (annotation.find("MEMCPY") != std::string::npos)
            extType = AbsExtAPI::MEMCPY;
        if (annotation.find("MEMSET") != std::string::npos)
            extType = AbsExtAPI::MEMSET;
        if (annotation.find("STRCPY") != std::string::npos)
            extType = AbsExtAPI::STRCPY;
        if (annotation.find("STRCAT") != std::string::npos)
            extType = AbsExtAPI::STRCAT;
    }

    // Apply buffer overflow checks based on the determined API type
    BufferOverflowInfo info;
    if (extType == AbsExtAPI::MEMCPY)
    {
        if (extAPIBufOverflowCheckRules.count(call->getCalledFunction()->getName()) == 0)
        {
            SVFUtil::errs() << "Warning: " << call->getCalledFunction()->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args =
            extAPIBufOverflowCheckRules.at(call->getCalledFunction()->getName());
        for (auto arg : args)
        {
            Interval offset = subtract(ae.getInterval(call->getArgument(arg.second), call), integerInterval(1));
            const ValVar* argVar = call->getArgument(arg.first);
            if (!canSafelyAccessMemory(argVar, offset, call, &info))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call, info);
            }
        }
    }
    else if (extType == AbsExtAPI::MEMSET)
    {
        if (extAPIBufOverflowCheckRules.count(call->getCalledFunction()->getName()) == 0)
        {
            SVFUtil::errs() << "Warning: " << call->getCalledFunction()->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args =
            extAPIBufOverflowCheckRules.at(call->getCalledFunction()->getName());
        for (auto arg : args)
        {
            Interval offset = subtract(ae.getInterval(call->getArgument(arg.second), call), integerInterval(1));
            const ValVar* argVar = call->getArgument(arg.first);
            if (!canSafelyAccessMemory(argVar, offset, call, &info))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call, info);
            }
        }
    }
    else if (extType == AbsExtAPI::STRCPY)
    {
        if (!detectStrcpy(call, info))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call, info);
        }
    }
    else if (extType == AbsExtAPI::STRCAT)
    {
        if (!detectStrcat(call, info))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call, info);
        }
    }
    else
    {
        // Handle other cases
    }
}

/**
 * @brief Retrieves the access offset for a given object and GEP statement.
 *
 * This function calculates the access offset for a base object or a sub-object of an
 * aggregate object (using GEP). If the object is a dummy object, it returns a top interval value.
 *
 * @param objId The ID of the object.
 * @param gep Pointer to the GEP statement.
 * @return The interval value of the access offset.
 */
Interval BufOverflowDetector::getAccessOffset(SVF::NodeID objId, const SVF::GepStmt* gep)
{
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();
    auto obj = svfir->getSVFVar(objId);

    if (SVFUtil::isa<BaseObjVar>(obj))
    {
        return ae.getGepByteOffset(gep);
    }
    else if (SVFUtil::isa<GepObjVar>(obj))
    {
        return add(getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(obj)), ae.getGepByteOffset(gep));
    }
    else
    {
        assert(SVFUtil::isa<DummyObjVar>(obj) && "Unknown object type");
        return Interval::top();
    }
}

/**
 * @brief Updates the offset of a GEP object from its base.
 *
 * This function calculates and stores the offset of a GEP object from its base
 * object using the addresses and offsets provided.
 *
 * @param gepAddrs The addresses of the GEP objects.
 * @param objAddrs The addresses of the base objects.
 * @param offset The interval value of the offset.
 */
void BufOverflowDetector::updateGepObjOffsetFromBase(const ICFGNode* node,
        SVF::AddressSet gepAddrs,
        SVF::AddressSet objAddrs,
        SVF::Interval offset)
{
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();
    (void)node;

    if (gepAddrs.hasUnknownObject() || objAddrs.hasUnknownObject())
        return;

    for (SVF::Location objLocation : objAddrs)
    {
        const ObjVar* mappedObject = ae.objectAt(objLocation);
        if (!mappedObject)
            continue;
        NodeID objId = mappedObject->getId();
        auto obj = svfir->getSVFVar(objId);

        if (SVFUtil::isa<BaseObjVar>(obj))
        {
            // if the object is a BaseObjVar, add the offset directly
            // like llvm bc `arr = alloc i8 12; p = gep arr, 4`
            // we write key value pair {gep, 4}
            for (SVF::Location gepLocation : gepAddrs)
            {
                const ObjVar* mappedGep = ae.objectAt(gepLocation);
                if (!mappedGep)
                    continue;
                NodeID gepObj = mappedGep->getId();
                if (const GepObjVar* gepObjVar =
                            SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(gepObj)))
                {
                    addToGepObjOffsetFromBase(gepObjVar, offset);
                }
            }
        }
        else if (SVFUtil::isa<GepObjVar>(obj))
        {
            // if the object is a GepObjVar, add the offset from the base object
            // like llvm bc `arr = alloc i8 12; p = gep arr, 4; q = gep p, 6`
            // we retreive {p, 4} and write {q, 4+6}
            const GepObjVar* objVar = SVFUtil::cast<GepObjVar>(obj);
            for (SVF::Location gepLocation : gepAddrs)
            {
                const ObjVar* mappedGep = ae.objectAt(gepLocation);
                if (!mappedGep)
                    continue;
                NodeID gepObj = mappedGep->getId();
                if (const GepObjVar* gepObjVar =
                            SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(gepObj)))
                {
                    if (hasGepObjOffsetFromBase(objVar))
                    {
                        SVF::Interval objOffsetFromBase =
                            getGepObjOffsetFromBase(objVar);
                        if (!hasGepObjOffsetFromBase(gepObjVar))
                            addToGepObjOffsetFromBase(
                                gepObjVar, SVF::add(objOffsetFromBase, offset));
                    }
                    else
                    {
                        assert(false && "GEP RHS object has no offset from base");
                    }
                }
            }
        }
    }
}

/**
 * @brief Detects buffer overflow in 'strcpy' function calls.
 *
 * This function checks if the destination buffer can safely accommodate the
 * source string being copied, accounting for the null terminator.
 *
 * @param as Reference to the abstract state.
 * @param call Pointer to the call ICFG node.
 * @return True if the memory access is safe, false otherwise.
 */
bool BufOverflowDetector::detectStrcpy(const CallICFGNode *call, BufferOverflowInfo& info)
{
    const ValVar* arg0Val = call->getArgument(0);
    const ValVar* arg1Val = call->getArgument(1);
    auto& ae = AbstractInterpretation::getAEInstance();
    Interval strLen = ae.getUtils()->getStrlen(arg1Val, call);
    return canSafelyAccessMemory(arg0Val, strLen, call, &info);
}

bool BufOverflowDetector::detectStrcat(const CallICFGNode *call, BufferOverflowInfo& info)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};

    if (std::find(strcatGroup.begin(), strcatGroup.end(), call->getCalledFunction()->getName()) != strcatGroup.end())
    {
        const ValVar* arg0Val = call->getArgument(0);
        const ValVar* arg1Val = call->getArgument(1);
        Interval strLen0 = ae.getUtils()->getStrlen(arg0Val, call);
        Interval strLen1 = ae.getUtils()->getStrlen(arg1Val, call);
        Interval totalLen = add(strLen0, strLen1);
        return canSafelyAccessMemory(arg0Val, totalLen, call, &info);
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), call->getCalledFunction()->getName()) != strncatGroup.end())
    {
        const ValVar* arg0Val = call->getArgument(0);
        const ValVar* arg2Val = call->getArgument(2);
        Interval arg2Num = ae.getInterval(arg2Val, call);
        Interval strLen0 = ae.getUtils()->getStrlen(arg0Val, call);
        Interval totalLen = add(strLen0, arg2Num);
        return canSafelyAccessMemory(arg0Val, totalLen, call, &info);
    }
    else
    {
        assert(false && "Unknown strcat function, please add it to strcatGroup or strncatGroup");
        abort();
    }
}

/**
 * @brief Checks if a memory access is safe given a specific buffer length.
 *
 * This function ensures that a given memory access, starting at a specific value,
 * does not exceed the allocated size of the buffer.
 *
 * @param as Reference to the abstract state.
 * @param value Pointer to the SVF var.
 * @param len The interval value representing the length of the memory access.
 * @return True if the memory access is safe, false otherwise.
 */
bool BufOverflowDetector::canSafelyAccessMemory(const ValVar* value, const Interval& len,
        const ICFGNode* node, BufferOverflowInfo* info)
{
    if (info) *info = {};
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();

    AddressSet addresses = ae.getAddressSet(value, node);
    if (addresses.isBottom())
    {
        addresses = ae.blackHoleAddressSet();
        ae.updateValue(value, Interval::bottom(), addresses, node);
    }
    if (!addresses.isFinite())
        return false;
    for (Location location : addresses)
    {
        const ObjVar* object = ae.objectAt(location);
        if (location.isNull() || !object ||
                svfir->getBaseObject(object->getId())->isBlackHoleObj())
            return false;
    }

    for (const auto& addr : addresses)
    {
        const ObjVar* object = ae.objectAt(addr);
        NodeID objId = object ? object->getId() : IRGraph::NullPtr;
        u32_t size = 0;
        u32_t lower = 0;
        // if the object is a constant size object, get the size directly
        if (svfir->getBaseObject(objId)->isConstantByteSize())
        {
            size = svfir->getBaseObject(objId)->getByteSizeOfObj();
            lower = size;
        }
        else
        {
            // if the object is not a constant size object, get the size from the addrStmt
            const ICFGNode* addrNode = svfir->getBaseObject(objId)->getICFGNode();
            if (!addrNode)
                return false;
            for (const SVFStmt* stmt2 : addrNode->getSVFStmts())
            {
                if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                {
                    const auto bounds = ae.getAllocaInstByteSize(addrStmt);
                    size = bounds.upperBound;
                    lower = bounds.lowerBound;
                }
            }
        }

        Interval offset = integerInterval(0);
        // if the object is a GepObjVar, get the offset from the base object
        if (SVFUtil::isa<GepObjVar>(svfir->getSVFVar(objId)))
        {
            offset = add(getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(svfir->getSVFVar(objId))), len);
        }
        else if (SVFUtil::isa<BaseObjVar>(svfir->getSVFVar(objId)))
        {
            // if the object is a BaseObjVar, get the offset directly
            offset = len;
        }

        // if the offset is greater than the size, return false
        if (upperAtLeast(offset, size))
        {
            if (info) *info = {size, lower, offset};
            return false;
        }
    }
    return true;
}

void NullptrDerefDetector::detect(const ICFGNode* node)
{
    if (SVFUtil::isa<CallICFGNode>(node))
    {
        // external API like memset(*dst, elem, sz)
        // we check if it's external api and check the corrisponding index
        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(node);
        if (SVFUtil::isExtCall(callNode->getCalledFunction()))
        {
            detectExtAPI(callNode);
        }
    }
    else
    {
        for (const auto& stmt: node->getSVFStmts())
        {
            if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
            {
                // like llvm bitcode `p = gep p, idx`
                // we check rhs p's all address are valid mem
                const ValVar* rhs = gep->getRHSVar();
                if (!canSafelyDerefPtr(rhs, node))
                {
                    AEException bug(stmt->toString());
                    addBugToReporter(bug, stmt->getICFGNode());
                }
            }
            else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
            {
                // like llvm bitcode `p = load q`
                // we check lhs p's all address are valid mem
                const ValVar* lhs = load->getLHSVar();
                if (!canSafelyDerefPtr(lhs, node))
                {
                    AEException bug(stmt->toString());
                    addBugToReporter(bug, stmt->getICFGNode());
                }
            }
        }
    }
}

void NullptrDerefDetector::handleStubFunctions(const CallICFGNode* callNode)
{
    std::string funcName = callNode->getCalledFunction()->getName();
    auto& ae = AbstractInterpretation::getAEInstance();
    if (funcName == "UNSAFE_LOAD")
    {
        // void UNSAFE_LOAD(void* ptr);
        ae.getUtils()->checkpoints.erase(callNode);
        if (callNode->arg_size() < 1)
            return;

        const ValVar* arg0Val = callNode->getArgument(0);
        // opt may directly dereference a null pointer and call UNSAFE_LOAD(null)
        bool isSafe = canSafelyDerefPtr(arg0Val, callNode) && arg0Val->getId() != 0;
        SVFUtil::outs() << "[UNSAFE_LOAD] node=" << callNode->getId()
                        << " arg0=" << arg0Val->getId() << " isSafe=" << isSafe
                        << "\n";
        if (!isSafe)
        {
            SVFUtil::outs() << SVFUtil::sucMsg("success: expected null dereference at UNSAFE_LOAD")
                            << " — " << callNode->toString() << "\n";
            return;
        }
        else
        {
            SVFUtil::outs() << SVFUtil::errMsg("failure: null dereference expected at UNSAFE_LOAD, but none detected")
                            << " — Position: " << callNode->getSourceLoc() << "\n";
            assert(false);
        }
    }
    else if (funcName == "SAFE_LOAD")
    {
        // void SAFE_LOAD(void* ptr);
        ae.getUtils()->checkpoints.erase(callNode);
        if (callNode->arg_size() < 1) return;
        const ValVar* arg0Val = callNode->getArgument(0);
        // opt may directly dereference a null pointer and call UNSAFE_LOAD(null)ols
        bool isSafe = canSafelyDerefPtr(arg0Val, callNode) && arg0Val->getId() != 0;
        if (isSafe)
        {
            SVFUtil::outs() << SVFUtil::sucMsg("success: expected safe dereference at SAFE_LOAD")
                            << " — " << callNode->toString() << "\n";
            return;
        }
        else
        {
            SVFUtil::outs() << SVFUtil::errMsg("failure: unexpected null dereference at SAFE_LOAD")
                            << " — Position: " << callNode->getSourceLoc() << "\n";
            assert(false);
        }
    }
}

void NullptrDerefDetector::detectExtAPI(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "FunObjVar* is nullptr");
    // get ext type
    // get argument index which are nullptr deref checkpoints for extapi
    std::vector<u32_t> tmp_args;
    for (const std::string &annotation: ExtAPI::getExtAPI()->getExtFuncAnnotations(call->getCalledFunction()))
    {
        if (annotation.find("MEMCPY") != std::string::npos)
        {
            if (call->arg_size() < 4)
            {
                // for memcpy(void* dest, const void* src, size_t n)
                tmp_args.push_back(0);
                tmp_args.push_back(1);
            }
            else
            {
                // for unsigned long iconv(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft)
                tmp_args.push_back(1);
                tmp_args.push_back(2);
                tmp_args.push_back(3);
                tmp_args.push_back(4);
            }
        }
        else if (annotation.find("MEMSET") != std::string::npos)
        {
            // for memset(void* dest, elem, sz)
            tmp_args.push_back(0);
        }
        else if (annotation.find("STRCPY") != std::string::npos)
        {
            // for strcpy(void* dest, void* src)
            tmp_args.push_back(0);
            tmp_args.push_back(1);
        }
        else if (annotation.find("STRCAT") != std::string::npos)
        {
            // for strcat(void* dest, const void* src)
            // for strncat(void* dest, const void* src, size_t n)
            tmp_args.push_back(0);
            tmp_args.push_back(1);
        }
    }

    for (const auto &arg: tmp_args)
    {
        if (call->arg_size() <= arg)
            continue;
        const ValVar* argVal = call->getArgument(arg);
        if (argVal && !canSafelyDerefPtr(argVal, call))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call);
        }
    }
}

bool NullptrDerefDetector::canSafelyDerefPtr(const ValVar* value,
        const ICFGNode* node)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    const SVF::AddressSet addresses = ae.getAddressSet(value, node);
    if (addresses.isBottom())
        return !ae.getInterval(value, node).isBottom();
    if (!addresses.isFinite())
        return false;
    for (SVF::Location location : addresses)
    {
        if (location.isNull())
            return false;
        const ObjVar* object = ae.objectAt(location);
        const BaseObjVar* base =
            object ? PAG::getPAG()->getBaseObject(object->getId()) : nullptr;
        if (!base || base->isBlackHoleObj() ||
                ae.isFreedMemory(location, node))
            return false;
    }
    return true;
}
