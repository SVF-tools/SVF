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
// Created by Jiawei Wang on 2024/8/20.
//

#include <AE/Svfexe/AEDetector.h>
#include <AE/Svfexe/AbsExtAPI.h>
#include <AE/Svfexe/AbstractInterpretation.h>

using namespace SVF;

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
void BufOverflowDetector::detect(AbstractState& as, const ICFGNode* node)
{
    if (!SVFUtil::isa<CallICFGNode>(node))
    {
        // Handle non-call nodes by analyzing GEP instructions
        for (const SVFStmt* stmt : node->getSVFStmts())
        {
            if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
            {
                SVFIR* svfir = PAG::getPAG();
                NodeID lhs = gep->getLHSVarID();
                NodeID rhs = gep->getRHSVarID();

                // Update the GEP object offset from its base
                updateGepObjOffsetFromBase(as[lhs].getAddrs(), as[rhs].getAddrs(), as.getByteOffset(gep));

                IntervalValue baseObjSize = IntervalValue::bottom();
                AddressValue objAddrs = as[gep->getRHSVarID()].getAddrs();
                for (const auto& addr : objAddrs)
                {
                    NodeID objId = AbstractState::getInternalID(addr);
                    u32_t size = 0;

                    if (svfir->getBaseObject(objId)->isConstantByteSize())
                    {
                        size = svfir->getBaseObject(objId)->getByteSizeOfObj();
                    }
                    else
                    {
                        const ICFGNode* addrNode = svfir->getBaseObject(objId)->getICFGNode();
                        for (const SVFStmt* stmt2 : addrNode->getSVFStmts())
                        {
                            if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                            {
                                size = as.getAllocaInstByteSize(addrStmt);
                            }
                        }
                    }

                    // Calculate access offset and check for potential overflow
                    IntervalValue accessOffset = getAccessOffset(as, objId, gep);
                    if (accessOffset.ub().getIntNumeral() >= size)
                    {
                        AEException bug(stmt->toString());
                        addBugToReporter(bug, stmt->getICFGNode());
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
            detectExtAPI(as, callNode);
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
    if (funcName == "SAFE_BUFACCESS")
    {
        // void SAFE_BUFACCESS(void* data, int size);
        AbstractInterpretation::getAEInstance().checkpoints.erase(callNode);
        if (callNode->arg_size() < 2)
            return;
        AbstractState& as =
            AbstractInterpretation::getAEInstance().getAbsStateFromTrace(
                callNode);
        u32_t size_id = callNode->getArgument(1)->getId();
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            val = IntervalValue(0);
            assert(false && "SAFE_BUFACCESS size is bottom");
        }
        const SVFVar* arg0Val = callNode->getArgument(0);
        bool isSafe = canSafelyAccessMemory(as, arg0Val, val);
        if (isSafe)
        {
            std::cout << "safe buffer access success: " << callNode->toString()
                      << std::endl;
            return;
        }
        else
        {
            std::string err_msg = "this SAFE_BUFACCESS should be a safe access but detected buffer overflow. Pos: ";
            err_msg += callNode->getSourceLoc();
            std::cerr << err_msg << std::endl;
            assert(false);
        }
    }
    else if (funcName == "UNSAFE_BUFACCESS")
    {
        // handle other stub functions
        //void UNSAFE_BUFACCESS(void* data, int size);
        AbstractInterpretation::getAEInstance().checkpoints.erase(callNode);
        if (callNode->arg_size() < 2) return;
        AbstractState&as = AbstractInterpretation::getAEInstance().getAbsStateFromTrace(callNode);
        u32_t size_id = callNode->getArgument(1)->getId();
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            assert(false && "UNSAFE_BUFACCESS size is bottom");
        }
        const SVFVar* arg0Val = callNode->getArgument(0);
        bool isSafe = canSafelyAccessMemory(as, arg0Val, val);
        if (!isSafe)
        {
            std::cout << "detect buffer overflow success: " << callNode->toString() << std::endl;
            return;
        }
        else
        {
            std::string err_msg = "this UNSAFE_BUFACCESS should be a buffer overflow but not detected. Pos: ";
            err_msg += callNode->getSourceLoc();
            std::cerr << err_msg << std::endl;
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
 * @param as Reference to the abstract state.
 * @param call Pointer to the call ICFG node.
 */
void BufOverflowDetector::detectExtAPI(AbstractState& as,
                                       const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "FunObjVar* is nullptr");

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
            IntervalValue offset = as[call->getArgument(arg.second)->getId()].getInterval() - IntervalValue(1);
            const SVFVar* argVar = call->getArgument(arg.first);
            if (!canSafelyAccessMemory(as, argVar, offset))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call);
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
            IntervalValue offset = as[call->getArgument(arg.second)->getId()].getInterval() - IntervalValue(1);
            const SVFVar* argVar = call->getArgument(arg.first);
            if (!canSafelyAccessMemory(as, argVar, offset))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call);
            }
        }
    }
    else if (extType == AbsExtAPI::STRCPY)
    {
        if (!detectStrcpy(as, call))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call);
        }
    }
    else if (extType == AbsExtAPI::STRCAT)
    {
        if (!detectStrcat(as, call))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call);
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
 * @param as Reference to the abstract state.
 * @param objId The ID of the object.
 * @param gep Pointer to the GEP statement.
 * @return The interval value of the access offset.
 */
IntervalValue BufOverflowDetector::getAccessOffset(SVF::AbstractState& as, SVF::NodeID objId, const SVF::GepStmt* gep)
{
    SVFIR* svfir = PAG::getPAG();
    auto obj = svfir->getGNode(objId);

    // if the object is a BaseObjVar, return the byte offset directly
    if (SVFUtil::isa<BaseObjVar>(obj))
    {
        return as.getByteOffset(gep);
    }
    else if (SVFUtil::isa<GepObjVar>(obj))
    {
        // if the object is a GepObjVar, return the offset from the base object
        return getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(obj)) + as.getByteOffset(gep);
    }
    else
    {
        assert(SVFUtil::isa<DummyObjVar>(obj) && "Unknown object type");
        return IntervalValue::top();
    }
}

/**
 * @brief Updates the offset of a GEP object from its base.
 *
 * This function calculates and stores the offset of a GEP object from its base object
 * using the addresses and offsets provided.
 *
 * @param gepAddrs The addresses of the GEP objects.
 * @param objAddrs The addresses of the base objects.
 * @param offset The interval value of the offset.
 */
void BufOverflowDetector::updateGepObjOffsetFromBase(SVF::AddressValue gepAddrs, SVF::AddressValue objAddrs, SVF::IntervalValue offset)
{
    SVFIR* svfir = PAG::getPAG();

    for (const auto& objAddr : objAddrs)
    {
        NodeID objId = AbstractState::getInternalID(objAddr);
        auto obj = svfir->getGNode(objId);
        // if the object is a BaseObjVar, add the offset directly
        if (SVFUtil::isa<BaseObjVar>(obj))
        {
            for (const auto& gepAddr : gepAddrs)
            {
                NodeID gepObj = AbstractState::getInternalID(gepAddr);
                const GepObjVar* gepObjVar = SVFUtil::cast<GepObjVar>(svfir->getGNode(gepObj));
                addToGepObjOffsetFromBase(gepObjVar, offset);
            }
        }
        else if (SVFUtil::isa<GepObjVar>(obj))
        {
            // if the object is a GepObjVar, add the offset from the base object
            const GepObjVar* objVar = SVFUtil::cast<GepObjVar>(obj);
            for (const auto& gepAddr : gepAddrs)
            {
                NodeID gepObj = AbstractState::getInternalID(gepAddr);
                const GepObjVar* gepObjVar = SVFUtil::cast<GepObjVar>(svfir->getGNode(gepObj));
                if (hasGepObjOffsetFromBase(objVar))
                {
                    IntervalValue objOffsetFromBase = getGepObjOffsetFromBase(objVar);
                    if (!hasGepObjOffsetFromBase(gepObjVar))
                        addToGepObjOffsetFromBase(gepObjVar, objOffsetFromBase + offset);
                }
                else
                {
                    assert(false && "GEP RHS object has no offset from base");
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
bool BufOverflowDetector::detectStrcpy(AbstractState& as, const CallICFGNode *call)
{
    const SVFVar* arg0Val = call->getArgument(0);
    const SVFVar* arg1Val = call->getArgument(1);
    IntervalValue strLen = AbstractInterpretation::getAEInstance().getUtils()->getStrlen(as, arg1Val);
    return canSafelyAccessMemory(as, arg0Val, strLen);
}

/**
 * @brief Detects buffer overflow in 'strcat' function calls.
 *
 * This function checks if the destination buffer can safely accommodate both the
 * existing string and the concatenated string from the source.
 *
 * @param as Reference to the abstract state.
 * @param call Pointer to the call ICFG node.
 * @return True if the memory access is safe, false otherwise.
 */
bool BufOverflowDetector::detectStrcat(AbstractState& as, const CallICFGNode *call)
{
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};

    if (std::find(strcatGroup.begin(), strcatGroup.end(), call->getCalledFunction()->getName()) != strcatGroup.end())
    {
        const SVFVar* arg0Val = call->getArgument(0);
        const SVFVar* arg1Val = call->getArgument(1);
        IntervalValue strLen0 = AbstractInterpretation::getAEInstance().getUtils()->getStrlen(as, arg0Val);
        IntervalValue strLen1 = AbstractInterpretation::getAEInstance().getUtils()->getStrlen(as, arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        return canSafelyAccessMemory(as, arg0Val, totalLen);
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), call->getCalledFunction()->getName()) != strncatGroup.end())
    {
        const SVFVar* arg0Val = call->getArgument(0);
        const SVFVar* arg2Val = call->getArgument(2);
        IntervalValue arg2Num = as[arg2Val->getId()].getInterval();
        IntervalValue strLen0 = AbstractInterpretation::getAEInstance().getUtils()->getStrlen(as, arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        return canSafelyAccessMemory(as, arg0Val, totalLen);
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
bool BufOverflowDetector::canSafelyAccessMemory(AbstractState& as, const SVF::SVFVar* value, const SVF::IntervalValue& len)
{
    SVFIR* svfir = PAG::getPAG();
    NodeID value_id = value->getId();

    assert(as[value_id].isAddr());
    for (const auto& addr : as[value_id].getAddrs())
    {
        NodeID objId = AbstractState::getInternalID(addr);
        u32_t size = 0;

        // if the object is a constant size object, get the size directly
        if (svfir->getBaseObject(objId)->isConstantByteSize())
        {
            size = svfir->getBaseObject(objId)->getByteSizeOfObj();
        }
        else
        {
            // if the object is not a constant size object, get the size from the addrStmt
            const ICFGNode* addrNode = svfir->getBaseObject(objId)->getICFGNode();
            for (const SVFStmt* stmt2 : addrNode->getSVFStmts())
            {
                if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                {
                    size = as.getAllocaInstByteSize(addrStmt);
                }
            }
        }

        IntervalValue offset(0);
        // if the object is a GepObjVar, get the offset from the base object
        if (SVFUtil::isa<GepObjVar>(svfir->getGNode(objId)))
        {
            offset = getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(svfir->getGNode(objId))) + len;
        }
        else
        {
            // if the object is a BaseObjVar, get the offset directly
            offset = len;
        }
        // if the offset is greater than the size, return false
        if (offset.ub().getIntNumeral() >= size)
        {
            return false;
        }
    }
    return true;
}
