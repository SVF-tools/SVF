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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <AE/Svfexe/AEDetector.h>
#include <AE/Svfexe/AbsExtAPI.h>
#include <AE/Svfexe/AbstractInterpretation.h>
#include "AE/Core/AddressValue.h"

using namespace SVF;

namespace
{
bool reportCallContextEnabled()
{
    static const bool enabled = (std::getenv("AE_REPORT_CALL_CONTEXT") != nullptr);
    return enabled;
}

unsigned long reportCallContextDepth()
{
    static const unsigned long depth = []() {
        const char* env = std::getenv("AE_REPORT_CALL_CONTEXT_DEPTH");
        if (!env || !*env)
            return 3UL;
        char* end = nullptr;
        unsigned long value = std::strtoul(env, &end, 10);
        return (end == env) ? 3UL : value;
    }();
    return depth;
}

bool reportCallContextSameFileOnly()
{
    static const bool enabled = (std::getenv("AE_REPORT_CALL_CONTEXT_SAME_FILE") != nullptr);
    return enabled;
}

bool reportStaticCallersEnabled()
{
    static const bool enabled = (std::getenv("AE_REPORT_STATIC_CALLERS") != nullptr);
    return enabled;
}

unsigned long reportStaticCallerDepth()
{
    static const unsigned long depth = []() {
        const char* env = std::getenv("AE_REPORT_STATIC_CALLER_DEPTH");
        if (!env || !*env)
            return 2UL;
        char* end = nullptr;
        unsigned long value = std::strtoul(env, &end, 10);
        return (end == env) ? 2UL : value;
    }();
    return depth;
}

bool reportNearbyCallsiteEnabled()
{
    static const bool enabled = (std::getenv("AE_REPORT_NEARBY_CALLSITE") != nullptr);
    return enabled;
}

bool reportContextLocTextEnabled()
{
    static const bool enabled = (std::getenv("AE_REPORT_CONTEXT_LOC_TEXT") != nullptr);
    return enabled;
}

bool envFlag(const char* name)
{
    const char* env = std::getenv(name);
    if (!env || !*env)
        return false;
    std::string value(env);
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "on" || value == "yes";
}

std::string evidenceToken(std::string value)
{
    if (value.empty())
        return "<unnamed>";
    for (char& ch : value)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch) || ch == '=')
            ch = '_';
    }
    return value;
}

bool isBoundedStringReadAPI(const std::string& name)
{
    return name == "strncmp" || name == "strncasecmp" ||
           name == "strncasecmp_l" || name == "strnlen" ||
           name == "strndup";
}

IntervalValue capStringReadLastByte(const IntervalValue& byteLimit,
                                    const IntervalValue& stringLength)
{
    if (byteLimit.isBottom() || stringLength.isBottom())
        return IntervalValue::bottom();
    return IntervalValue(min(byteLimit.lb(), stringLength.lb()),
                         min(byteLimit.ub(), stringLength.ub()));
}

bool actionableReportsOnlyEnabled()
{
    static const bool enabled =
        envFlag("AE_BO_ACTIONABLE_REPORTS") || envFlag("AE_BO_ACTIONABLE_ONLY");
    return enabled;
}

std::string partialReportPath(bool actionable)
{
    const char* explicitPath = std::getenv(actionable
                                           ? "AE_BO_PARTIAL_REPORT_FILE"
                                           : "AE_BO_SUPPORT_REPORT_FILE");
    if (explicitPath && *explicitPath)
        return explicitPath;

    if (!envFlag("AE_BO_PARTIAL_REPORT"))
        return "";

    const char* prefix = std::getenv("AE_TMP_PREFIX");
    if (!prefix || !*prefix)
        return "";

    std::string path = "/tmp/";
    path += prefix;
    path += (actionable ? "_svf_bo_actionable_partial.log"
                        : "_svf_bo_supporting_partial.log");
    return path;
}

bool suppressGepOnlyOverflowReports()
{
    static const bool enabled = envFlag("AE_BO_SUPPRESS_GEP_ONLY");
    return enabled;
}

bool suppressNonAddrOverflowReports()
{
    static const bool enabled = envFlag("AE_BO_SUPPRESS_NONADDR");
    return enabled;
}

bool suppressUnknownSizeOverflowReports()
{
    static const bool enabled = envFlag("AE_BO_SUPPRESS_UNKNOWN_SIZE");
    return enabled;
}

bool explainBoundChecksEnabled()
{
    static const bool enabled = envFlag("AE_BO_EXPLAIN_CHECKS");
    return enabled;
}

unsigned boundActionableMinRank()
{
    static const unsigned rank = []() {
        const char* env = std::getenv("AE_BO_ACTIONABLE_MIN_CONFIDENCE");
        if (!env || !*env)
            return 1U; // Backward compatible: inconclusive checks are actionable.
        const std::string value(env);
        if (value == "must" || value == "must_overflow")
            return 3U;
        if (value == "may" || value == "may_overflow" || value == "concrete")
            return 2U;
        return 1U;
    }();
    return rank;
}

bool reportFilterNoiseEnabled()
{
    static const bool enabled = []() {
        const char* env = std::getenv("AE_REPORT_FILTER_NOISE");
        if (!env || !*env)
            return false;
        return std::string(env) == "1" ||
               std::string(env) == "true" ||
               std::string(env) == "TRUE" ||
               std::string(env) == "on" ||
               std::string(env) == "yes";
    }();
    return enabled;
}

unsigned long reportNearbyCallsiteWindow()
{
    static const unsigned long window = []() {
        const char* env = std::getenv("AE_REPORT_NEARBY_CALLSITE_WINDOW");
        if (!env || !*env)
            return 8UL;
        char* end = nullptr;
        unsigned long value = std::strtoul(env, &end, 10);
        return (end == env) ? 8UL : value;
    }();
    return window;
}

std::string sourceFileBase(const ICFGNode* node)
{
    if (!node)
        return "";

    const std::string loc = node->getSourceLoc();
    const std::string key = "\"fl\": \"";
    size_t pos = loc.find(key);
    if (pos == std::string::npos)
        return "";
    pos += key.size();
    size_t end = loc.find('"', pos);
    if (end == std::string::npos)
        return "";

    std::string file = loc.substr(pos, end - pos);
    size_t slash = file.find_last_of("/\\");
    if (slash != std::string::npos)
        file = file.substr(slash + 1);
    return file;
}

long sourceLine(const ICFGNode* node)
{
    if (!node)
        return -1;

    const std::string loc = node->getSourceLoc();
    const std::string key = "\"ln\": ";
    size_t pos = loc.find(key);
    if (pos == std::string::npos)
        return -1;
    pos += key.size();

    char* end = nullptr;
    long value = std::strtol(loc.c_str() + pos, &end, 10);
    return (end == loc.c_str() + pos) ? -1 : value;
}

bool isNoiseReportText(const std::string& text)
{
    return text.find("__sancov") != std::string::npos ||
           text.find("__sanitizer_cov") != std::string::npos ||
           text.find("@__sancov_lowest_stack") != std::string::npos ||
           text.find("!nosanitize") != std::string::npos;
}

bool shouldSuppressReport(const AEException& e, const ICFGNode* reportNode)
{
    if (!reportFilterNoiseEnabled())
        return false;

    if (sourceLine(reportNode) == 0)
        return true;

    const std::string reportText = e.what();
    if (isNoiseReportText(reportText))
        return true;

    if (reportNode)
    {
        const std::string nodeText = reportNode->toString();
        if (isNoiseReportText(nodeText) || isNoiseReportText(reportNode->getSourceLoc()))
            return true;
    }

    return false;
}

const std::vector<const CallICFGNode*>& functionCallsites(ICFG* icfg, const FunObjVar* fun)
{
    static ICFG* cachedICFG = nullptr;
    static Map<const FunObjVar*, std::vector<const CallICFGNode*>> callsByFun;

    if (cachedICFG != icfg)
    {
        cachedICFG = icfg;
        callsByFun.clear();
        if (icfg)
        {
            for (ICFG::const_iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
            {
                const ICFGNode* n = it->second;
                if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(n))
                    callsByFun[call->getFun()].push_back(call);
            }
        }
    }

    return callsByFun[fun];
}
} // namespace

void BufOverflowDetector::streamBugReport(const std::string& reportText, bool actionable)
{
    const std::string path = partialReportPath(actionable);
    if (path.empty())
        return;

    std::ofstream out(path.c_str(), std::ios::app);
    if (!out)
        return;

    unsigned long& count = actionable ? streamedActionableCount : streamedSupportCount;
    if (count == 0)
    {
        out << "######################Buffer Overflow "
            << (actionable ? "Actionable Partial" : "Supporting Partial")
            << "######################\n";
    }
    ++count;

    out << "---------------------------------------------\n";
    out << (actionable ? "[AE_ACTIONABLE]\n" : "[AE_SUPPORTING]\n");
    out << reportText << "\n";
    out.flush();
}

bool BufOverflowDetector::isLastBoundCheckActionable() const
{
    if (lastBoundCheckConfidence == BoundConfidence::None)
        return true;
    return (unsigned)lastBoundCheckConfidence >= boundActionableMinRank();
}

void BufOverflowDetector::addBugToReporter(const AEException& e, const ICFGNode* node, bool actionable)
{
    actionable = actionable && isLastBoundCheckActionable();
    const bool actionOnly = actionableReportsOnlyEnabled();

    auto addOne = [&](const ICFGNode* reportNode, bool nodeActionable) {
        if (!reportNode)
            return;
        if (shouldSuppressReport(e, reportNode))
            return;

        GenericBug::EventStack eventStack;
        SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, reportNode);
        eventStack.push_back(sourceInstEvent);

        if (eventStack.empty())
            return;

        const bool includeInFinalReport = nodeActionable || !actionOnly;
        std::string loc = eventStack.back().getEventLoc();
        Set<std::string>& seenLocs = includeInFinalReport ? bugLoc : supportBugLoc;
        if (seenLocs.find(loc) != seenLocs.end())
            return;

        seenLocs.insert(loc);

        std::string reportText = e.what();
        if (explainBoundChecksEnabled() && !lastBoundCheckEvidence.empty())
        {
            reportText += "\n";
            reportText += lastBoundCheckEvidence;
        }
        if (reportContextLocTextEnabled() && reportNode != node)
        {
            reportText += "\n[AE_REPORT_CONTEXT] ";
            reportText += reportNode->toString();
            const std::string reportLoc = reportNode->getSourceLoc();
            if (!reportLoc.empty())
            {
                reportText += " ";
                reportText += reportLoc;
            }
        }

        if (includeInFinalReport)
        {
            recoder.addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, 0, 0, 0, 0);
            nodeToBugInfo[reportNode] = reportText;
            streamBugReport(reportText, true);
        }
        else
        {
            streamBugReport(reportText, false);
        }
    };

    addOne(node, actionable);

    const bool sameFileOnly = reportCallContextSameFileOnly();
    const std::string bugFile = sameFileOnly ? sourceFileBase(node) : "";

    if (reportCallContextEnabled())
    {
        const auto& callStack = AbstractInterpretation::getAEInstance().getReportCallStack();
        const unsigned long maxDepth = reportCallContextDepth();
        unsigned long seen = 0;

        for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
        {
            if (maxDepth != 0 && seen >= maxDepth)
                break;
            ++seen;

            const ICFGNode* ctxNode = *it;
            if (sameFileOnly && sourceFileBase(ctxNode) != bugFile)
                continue;
            addOne(ctxNode, false);
        }
    }

    if (reportStaticCallersEnabled() && node && node->getFun())
    {
        CallGraph* callGraph = AbstractInterpretation::getAEInstance().getCallGraph();
        const unsigned long maxStaticDepth = reportStaticCallerDepth();

        if (callGraph && maxStaticDepth != 0)
        {
            std::vector<std::pair<const FunObjVar*, unsigned long>> worklist;
            Set<const FunObjVar*> seenFuns;
            worklist.push_back(std::make_pair(node->getFun(), 0UL));
            seenFuns.insert(node->getFun());

            for (size_t i = 0; i < worklist.size(); ++i)
            {
                const FunObjVar* callee = worklist[i].first;
                unsigned long depth = worklist[i].second;
                if (depth >= maxStaticDepth)
                    continue;

                CallGraphEdge::CallInstSet callsites;
                callGraph->getAllCallSitesInvokingCallee(callee, callsites);
                for (const CallICFGNode* callsite : callsites)
                {
                    if (!sameFileOnly || sourceFileBase(callsite) == bugFile)
                        addOne(callsite, false);

                    const FunObjVar* caller = callsite->getCaller();
                    if (caller && seenFuns.insert(caller).second)
                        worklist.push_back(std::make_pair(caller, depth + 1));
                }
            }
        }
    }

    if (!reportNearbyCallsiteEnabled() || !node || !node->getFun())
        return;

    const long line = sourceLine(node);
    if (line < 0)
        return;

    ICFG* icfg = AbstractInterpretation::getAEInstance().getICFG();
    const unsigned long window = reportNearbyCallsiteWindow();
    const std::vector<const CallICFGNode*>& callsites = functionCallsites(icfg, node->getFun());
    const CallICFGNode* best = nullptr;
    long bestLine = -1;

    for (const CallICFGNode* callsite : callsites)
    {
        if (sameFileOnly && sourceFileBase(callsite) != bugFile)
            continue;

        const long callLine = sourceLine(callsite);
        if (callLine < 0 || callLine >= line)
            continue;
        if ((unsigned long)(line - callLine) > window)
            continue;
        if (callLine > bestLine)
        {
            bestLine = callLine;
            best = callsite;
        }
    }

    if (best)
        addOne(best, false);
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
        // Handle non-call nodes by analyzing GEP and concrete memory accesses.
        for (const SVFStmt* stmt : node->getSVFStmts())
        {
            if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
            {
                // Update the GEP object offset from its base
                const AbstractValue& lhsVal = ae.getAbsValue(gep->getLHSVar(), node);
                const AbstractValue& rhsVal = ae.getAbsValue(gep->getRHSVar(), node);
                updateGepObjOffsetFromBase(node, gep->getLHSVar(), gep->getRHSVar(),
                                           lhsVal.getAddrs(), rhsVal.getAddrs(),
                                           ae.getGepByteOffset(gep));
            }
            detectReadWriteAccess(stmt, node);
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
        IntervalValue val = ae.getAbsValue(callNode->getArgument(1), callNode).getInterval();
        if (val.isBottom())
        {
            val = IntervalValue(0);
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
        IntervalValue val = ae.getAbsValue(callNode->getArgument(1), callNode).getInterval();
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
    extAPIBufOverflowCheckRules["llvm_memcpy_p0_p0_i32"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memcpy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i64"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i32"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i32"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["__memcpy_chk"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["memmove"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["bcopy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["memccpy"] = {{0, 3}, {1, 3}};
    extAPIBufOverflowCheckRules["__memmove_chk"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["llvm_memset"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0i8_i32"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0_i32"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0i8_i64"] = {{0, 2}};
    extAPIBufOverflowCheckRules["llvm_memset_p0_i64"] = {{0, 2}};
    extAPIBufOverflowCheckRules["__memset_chk"] = {{0, 2}};
    extAPIBufOverflowCheckRules["wmemset"] = {{0, 2}};
    extAPIBufOverflowCheckRules["strncpy"] = {{0, 2}, {1, 2}};
    extAPIBufOverflowCheckRules["iconv"] = {{1, 2}, {3, 4}};

    extAPIReadCheckRules["bcmp"] = {{0, 2}, {1, 2}};
    extAPIReadCheckRules["memcmp"] = {{0, 2}, {1, 2}};
    extAPIReadCheckRules["memchr"] = {{0, 2}};
    extAPIReadCheckRules["memrchr"] = {{0, 2}};
    extAPIReadCheckRules["strncmp"] = {{0, 2}, {1, 2}};
    extAPIReadCheckRules["strncasecmp"] = {{0, 2}, {1, 2}};
    extAPIReadCheckRules["strncasecmp_l"] = {{0, 2}, {1, 2}};
    extAPIReadCheckRules["strnlen"] = {{0, 1}};
    extAPIReadCheckRules["strndup"] = {{0, 1}};
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

    // Normalize the (possibly dotted) intrinsic name so LLVM-15+ opaque-
    // and typed-pointer names (e.g. llvm.memcpy.p0.p0.i64) match the
    // underscore-keyed rule table (llvm_memcpy_p0_p0_i64).
    std::string fname = call->getCalledFunction()->getName();
    std::replace(fname.begin(), fname.end(), '.', '_');

    if (detectReadOnlyAPI(call))
        return;

    // Apply buffer overflow checks based on the determined API type
    if (extType == AbsExtAPI::MEMCPY)
    {
        if (extAPIBufOverflowCheckRules.count(fname) == 0)
        {
            SVFUtil::errs() << "Warning: " << call->getCalledFunction()->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args =
                                              extAPIBufOverflowCheckRules.at(fname);
        for (auto arg : args)
        {
            IntervalValue offset = ae.getAbsValue(call->getArgument(arg.second), call).getInterval() - IntervalValue(1);
            const ValVar* argVar = call->getArgument(arg.first);
            // strncpy writes exactly n bytes to dst, but reads src only through
            // its first NUL (or n bytes when no NUL occurs first).
            if (fname == "strncpy" && arg.first == 1)
                offset = capStringReadLastByte(
                             offset, ae.getUtils()->getStrlen(argVar, call));
            if (!canSafelyAccessMemory(argVar, offset, call))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call);
            }
        }
    }
    else if (extType == AbsExtAPI::MEMSET)
    {
        if (extAPIBufOverflowCheckRules.count(fname) == 0)
        {
            SVFUtil::errs() << "Warning: " << call->getCalledFunction()->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args =
                                              extAPIBufOverflowCheckRules.at(fname);
        for (auto arg : args)
        {
            IntervalValue offset = ae.getAbsValue(call->getArgument(arg.second), call).getInterval() - IntervalValue(1);
            const ValVar* argVar = call->getArgument(arg.first);
            if (!canSafelyAccessMemory(argVar, offset, call))
            {
                AEException bug(call->toString());
                addBugToReporter(bug, call);
            }
        }
    }
    else if (extType == AbsExtAPI::STRCPY)
    {
        if (!detectStrcpy(call))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call);
        }
    }
    else if (extType == AbsExtAPI::STRCAT)
    {
        if (!detectStrcat(call))
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

u32_t BufOverflowDetector::getAccessByteSize(const ValVar* value) const
{
    if (!value || !value->getType())
        return 1;
    u32_t size = value->getType()->getByteSize();
    return size == 0 ? 1 : size;
}

bool BufOverflowDetector::getObjectByteSize(SVF::NodeID objId, const ICFGNode* node,
                                            IntervalValue& size) const
{
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();
    const BaseObjVar* base = svfir->getBaseObject(objId);
    if (!base)
        return false;

    if (base->isConstantByteSize())
    {
        size = IntervalValue(base->getByteSizeOfObj());
        return base->getByteSizeOfObj() != 0;
    }

    const ICFGNode* addrNode = base->getICFGNode();
    if (!addrNode)
        return false;

    for (const SVFStmt* stmt : addrNode->getSVFStmts())
    {
        if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            size = ae.getAllocaInstByteSizeInterval(addrStmt);
            return !size.isBottom() &&
                   !size.lb().is_minus_infinity() &&
                   !size.ub().is_plus_infinity();
        }
    }
    (void)node;
    return false;
}

bool BufOverflowDetector::canSafelyAccessBytes(const ValVar* value, u32_t accessBytes, const ICFGNode* node)
{
    if (accessBytes == 0)
        return true;
    return canSafelyAccessMemory(value, IntervalValue((s64_t)accessBytes - 1), node);
}

void BufOverflowDetector::detectReadWriteAccess(const SVFStmt* stmt, const ICFGNode* node)
{
    if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        if (suppressGepOnlyOverflowReports())
            return;
        auto& ae = AbstractInterpretation::getAEInstance();
        if (!canSafelyAccessMemory(gep->getRHSVar(), ae.getGepByteOffset(gep), node))
        {
            AEException bug(stmt->toString());
            addBugToReporter(bug, stmt->getICFGNode(), false);
        }
    }
    else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
    {
        if (!canSafelyAccessBytes(load->getRHSVar(), getAccessByteSize(load->getLHSVar()), node))
        {
            AEException bug(stmt->toString());
            addBugToReporter(bug, stmt->getICFGNode());
        }
    }
    else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        if (!canSafelyAccessBytes(store->getLHSVar(), getAccessByteSize(store->getRHSVar()), node))
        {
            AEException bug(stmt->toString());
            addBugToReporter(bug, stmt->getICFGNode());
        }
    }
}

bool BufOverflowDetector::detectReadOnlyAPI(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "FunObjVar* is nullptr");
    std::string fname = call->getCalledFunction()->getName();
    std::replace(fname.begin(), fname.end(), '.', '_');

    auto it = extAPIReadCheckRules.find(fname);
    if (it == extAPIReadCheckRules.end())
        return false;

    auto& ae = AbstractInterpretation::getAEInstance();
    for (auto arg : it->second)
    {
        if (call->arg_size() <= arg.first || call->arg_size() <= arg.second)
            continue;

        IntervalValue len = ae.getAbsValue(call->getArgument(arg.second), call).getInterval();
        const ValVar* argVar = call->getArgument(arg.first);
        IntervalValue lastByte = len - IntervalValue((s64_t)1);
        if (isBoundedStringReadAPI(fname))
            lastByte = capStringReadLastByte(
                           lastByte, ae.getUtils()->getStrlen(argVar, call));
        if (!canSafelyAccessMemory(argVar, lastByte, call))
        {
            AEException bug(call->toString());
            addBugToReporter(bug, call);
        }
    }
    return true;
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
IntervalValue BufOverflowDetector::getAccessOffset(SVF::NodeID objId, const SVF::GepStmt* gep)
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
        return getGepObjOffsetFromBase(SVFUtil::cast<GepObjVar>(obj)) + ae.getGepByteOffset(gep);
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
void BufOverflowDetector::updateGepObjOffsetFromBase(const SVF::ICFGNode* node,
        const ValVar* gepValue, const ValVar* baseValue,
        SVF::AddressValue gepAddrs, SVF::AddressValue objAddrs,
        SVF::IntervalValue offset)
{
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();
    const AbstractState& as = ae.getAbsState(node);

    IntervalValue valueOffset = IntervalValue::bottom();
    bool hasValueOffset = false;
    auto baseOffsetIt = valueOffsetFromBase.find(baseValue);
    if (baseOffsetIt != valueOffsetFromBase.end())
    {
        valueOffset = baseOffsetIt->second + offset;
        hasValueOffset = true;
    }
    else if (!objAddrs.empty())
    {
        bool allBaseObjects = true;
        for (const auto& objAddr : objAddrs)
        {
            const SVFVar* obj = svfir->getSVFVar(as.getIDFromAddr(objAddr));
            if (!SVFUtil::isa<BaseObjVar>(obj) || SVFUtil::isa<DummyObjVar>(obj))
            {
                allBaseObjects = false;
                break;
            }
        }
        if (allBaseObjects)
        {
            valueOffset = offset;
            hasValueOffset = true;
        }
    }

    if (hasValueOffset)
    {
        auto existing = valueOffsetFromBase.find(gepValue);
        if (existing == valueOffsetFromBase.end())
            valueOffsetFromBase[gepValue] = valueOffset;
        else
            existing->second.join_with(valueOffset);
    }

    for (const auto& objAddr : objAddrs)
    {
        NodeID objId = as.getIDFromAddr(objAddr);
        auto obj = svfir->getSVFVar(objId);

        if (SVFUtil::isa<BaseObjVar>(obj))
        {
            // if the object is a BaseObjVar, add the offset directly
            // like llvm bc `arr = alloc i8 12; p = gep arr, 4`
            // we write key value pair {gep, 4}
            for (const auto& gepAddr : gepAddrs)
            {
                NodeID gepObj = as.getIDFromAddr(gepAddr);
                if (const GepObjVar* gepObjVar = SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(gepObj)))
                {
                    addToGepObjOffsetFromBase(gepObjVar, offset);
                }
                else
                {
                    // Sound guard (AE_FIXES #5 family): the GEP result may
                    // decode to a base/unknown object (not a GepObjVar) once
                    // the dense flood no longer pre-populates trace.  Cannot
                    // record an offset-from-base for it; skip rather than abort.
                }
            }
        }
        else if (SVFUtil::isa<GepObjVar>(obj))
        {
            // if the object is a GepObjVar, add the offset from the base object
            // like llvm bc `arr = alloc i8 12; p = gep arr, 4; q = gep p, 6`
            // we retreive {p, 4} and write {q, 4+6}
            const GepObjVar* objVar = SVFUtil::cast<GepObjVar>(obj);
            for (const auto& gepAddr : gepAddrs)
            {
                NodeID gepObj = as.getIDFromAddr(gepAddr);
                if (const GepObjVar* gepObjVar = SVFUtil::dyn_cast<GepObjVar>(svfir->getSVFVar(gepObj)))
                {
                    if (hasGepObjOffsetFromBase(objVar))
                    {
                        IntervalValue objOffsetFromBase =
                            getGepObjOffsetFromBase(objVar);
                        if (!hasGepObjOffsetFromBase(gepObjVar))
                            addToGepObjOffsetFromBase(
                                gepObjVar, objOffsetFromBase + offset);
                    }
                    else
                    {
                        // Base GepObjVar's offset was never recorded (overlay /
                        // sparse path); skip — downstream getGepObjOffsetFromBase
                        // falls back to a conservative value.  Sound.
                    }
                }
                else
                {
                    // Sound guard (AE_FIXES #5 family): the GEP result may
                    // decode to a base/unknown object (not a GepObjVar) once
                    // the dense flood no longer pre-populates trace.  Cannot
                    // record an offset-from-base for it; skip rather than abort.
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
bool BufOverflowDetector::detectStrcpy(const CallICFGNode *call)
{
    const ValVar* arg0Val = call->getArgument(0);
    const ValVar* arg1Val = call->getArgument(1);
    auto& ae = AbstractInterpretation::getAEInstance();
    IntervalValue strLen = ae.getUtils()->getStrlen(arg1Val, call);
    return canSafelyAccessMemory(arg0Val, strLen, call);
}

bool BufOverflowDetector::detectStrcat(const CallICFGNode *call)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};

    if (std::find(strcatGroup.begin(), strcatGroup.end(), call->getCalledFunction()->getName()) != strcatGroup.end())
    {
        const ValVar* arg0Val = call->getArgument(0);
        const ValVar* arg1Val = call->getArgument(1);
        IntervalValue strLen0 = ae.getUtils()->getStrlen(arg0Val, call);
        IntervalValue strLen1 = ae.getUtils()->getStrlen(arg1Val, call);
        IntervalValue totalLen = strLen0 + strLen1;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), call->getCalledFunction()->getName()) != strncatGroup.end())
    {
        const ValVar* arg0Val = call->getArgument(0);
        const ValVar* arg2Val = call->getArgument(2);
        IntervalValue arg2Num = ae.getAbsValue(arg2Val, call).getInterval();
        IntervalValue strLen0 = ae.getUtils()->getStrlen(arg0Val, call);
        IntervalValue totalLen = strLen0 + arg2Num;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
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
bool BufOverflowDetector::canSafelyAccessMemory(const SVF::ValVar* value, const SVF::IntervalValue& len, const ICFGNode* node)
{
    SVFIR* svfir = PAG::getPAG();
    auto& ae = AbstractInterpretation::getAEInstance();
    lastBoundCheckEvidence.clear();
    lastBoundCheckConfidence = BoundConfidence::None;

    if (len.isBottom() || len.ub().is_plus_infinity())
    {
        lastBoundCheckConfidence = BoundConfidence::Inconclusive;
        if (explainBoundChecksEnabled())
        {
            std::ostringstream out;
            out << "[AE_BOUND_CHECK] reason=unknown_access_length"
                << " ptr_var=" << value->getId()
                << " access_last_byte=" << len.toString()
                << " confidence=inconclusive";
            lastBoundCheckEvidence = out.str();
        }
        return false;
    }

    AbstractValue ptrVal = ae.getAbsValue(value, node);
    if (!ptrVal.isAddr())
    {
        if (suppressNonAddrOverflowReports())
            return true;
        if (len.ub().getIntNumeral() >= 0)
        {
            lastBoundCheckConfidence = BoundConfidence::Inconclusive;
            if (explainBoundChecksEnabled())
            {
                std::ostringstream out;
                out << "[AE_BOUND_CHECK] reason=non_address"
                    << " ptr_var=" << value->getId()
                    << " access_last_byte=" << len.toString()
                    << " confidence=inconclusive";
                lastBoundCheckEvidence = out.str();
            }
            return false;
        }
        ptrVal = AddressValue(BlackHoleObjAddr);
        ae.updateAbsValue(value, ptrVal, node);
    }

    bool sawOverflow = false;
    bool sawSafeTarget = false;
    bool sawUnresolvedTarget = false;
    bool sawReportableUnknown = false;
    bool allViolationsDefinite = true;
    bool strongestViolationDefinite = false;
    unsigned checkedTargets = 0;
    unsigned safeTargets = 0;
    unsigned violatingTargets = 0;
    unsigned unknownTargets = 0;
    std::string strongestViolationEvidence;
    std::string unknownEvidence;
    std::string unresolvedEvidence;
    auto valueOffsetIt = valueOffsetFromBase.find(value);
    const bool hasValueOffset = valueOffsetIt != valueOffsetFromBase.end();

    auto markUnresolved = [&](const char* detail, NodeID objectId = 0)
    {
        sawUnresolvedTarget = true;
        ++unknownTargets;
        if (explainBoundChecksEnabled() && unresolvedEvidence.empty())
        {
            std::ostringstream out;
            out << "[AE_BOUND_CHECK] reason=unresolved_target"
                << " detail=" << detail
                << " ptr_var=" << value->getId()
                << " points_to=" << ptrVal.getAddrs().size();
            if (objectId != 0)
                out << " object_id=" << objectId;
            out << " access_last_byte=" << len.toString();
            unresolvedEvidence = out.str();
        }
    };

    for (const auto& addr : ptrVal.getAddrs())
    {
        if (AbstractState::isNullMem(addr))
            continue;
        if (AbstractState::isBlackHoleObjAddr(addr))
        {
            markUnresolved("blackhole_address");
            continue;
        }

        NodeID objId = ae.getAbsState(node).getIDFromAddr(addr);
        const SVFVar* obj = svfir->getSVFVar(objId);

        IntervalValue offset(0);
        if (hasValueOffset)
        {
            offset = valueOffsetIt->second + len;
        }
        // If the SSA value has no offset, fall back to the merged field object.
        else if (const GepObjVar* gepObj = SVFUtil::dyn_cast<GepObjVar>(obj))
        {
            if (!hasGepObjOffsetFromBase(gepObj))
            {
                markUnresolved("missing_gep_offset", objId);
                continue;
            }
            offset = getGepObjOffsetFromBase(gepObj) + len;
        }
        else if (SVFUtil::isa<DummyObjVar>(obj))
        {
            markUnresolved("dummy_object", objId);
            continue;
        }
        else if (SVFUtil::isa<BaseObjVar>(obj))
        {
            // if the object is a BaseObjVar, get the offset directly
            offset = len;
        }
        else
        {
            markUnresolved("unsupported_object_kind", objId);
            continue;
        }

        if (offset.isBottom() || offset.ub().is_plus_infinity())
        {
            markUnresolved("unbounded_offset", objId);
            continue;
        }

        IntervalValue size = IntervalValue::bottom();
        if (!getObjectByteSize(objId, node, size))
        {
            // External/input buffers often have no concrete object size in SVFIR.
            // An explicit suppression policy may hide these; otherwise retain
            // them as inconclusive rather than silently treating them as safe.
            if (suppressUnknownSizeOverflowReports())
                continue;
            sawUnresolvedTarget = true;
            if (offset.ub().getIntNumeral() >= 0)
            {
                sawReportableUnknown = true;
                ++unknownTargets;
                if (explainBoundChecksEnabled())
                {
                    std::ostringstream out;
                    out << "[AE_BOUND_CHECK] reason=unknown_object_size"
                        << " ptr_var=" << value->getId()
                        << " points_to=" << ptrVal.getAddrs().size()
                        << " object_id=" << objId;
                    if (const GepObjVar* gepObj = SVFUtil::dyn_cast<GepObjVar>(obj))
                    {
                        out << " object_kind=gep"
                            << " gep_field=" << gepObj->getConstantFieldIdx()
                            << " base_object_id=" << gepObj->getBaseObj()->getId()
                            << " base_field_limit=" << gepObj->getBaseObj()->getMaxFieldOffsetLimit();
                    }
                    else if (const BaseObjVar* baseObj = SVFUtil::dyn_cast<BaseObjVar>(obj))
                    {
                        out << " object_kind=base"
                            << " base_object_id=" << baseObj->getId()
                            << " base_name=" << evidenceToken(baseObj->getName())
                            << " base_field_limit=" << baseObj->getMaxFieldOffsetLimit();
                    }
                    out
                        << " access_last_byte=" << len.toString()
                        << " end_offset=" << offset.toString();
                    if (unknownEvidence.empty())
                        unknownEvidence = out.str();
                }
            }
            continue;
        }

        ++checkedTargets;
        const bool possible = offset.ub().getIntNumeral() >=
                              size.lb().getIntNumeral();
        if (possible)
        {
            const bool definite = !offset.lb().is_minus_infinity() &&
                                  offset.lb().getIntNumeral() >=
                                  size.ub().getIntNumeral();
            sawOverflow = true;
            ++violatingTargets;
            allViolationsDefinite = allViolationsDefinite && definite;
            if (explainBoundChecksEnabled())
            {
                std::ostringstream out;
                out << "[AE_BOUND_CHECK] reason=concrete_bound_violation"
                    << " ptr_var=" << value->getId()
                    << " points_to=" << ptrVal.getAddrs().size()
                    << " object_id=" << objId
                    << " object_size_interval=" << size.toString()
                    << " offset_source=" << (hasValueOffset ? "ssa_value" : "field_object");
                if (size.is_numeral())
                    out << " object_size=" << size.getIntNumeral();
                const BaseObjVar* baseObj = svfir->getBaseObject(objId);
                if (const GepObjVar* gepObj = SVFUtil::dyn_cast<GepObjVar>(obj))
                {
                    out << " object_kind=gep"
                        << " gep_field=" << gepObj->getConstantFieldIdx();
                }
                else
                {
                    out << " object_kind=base";
                }
                if (baseObj)
                {
                    out << " base_object_id=" << baseObj->getId()
                        << " base_name=" << evidenceToken(baseObj->getName())
                        << " base_field_limit=" << baseObj->getMaxFieldOffsetLimit()
                        << " base_elements=" << baseObj->getNumOfElements()
                        << " base_heap=" << (baseObj->isHeap() ? 1 : 0)
                        << " base_stack=" << (baseObj->isStack() ? 1 : 0)
                        << " base_global=" << (baseObj->isGlobalObj() ? 1 : 0)
                        << " base_array=" << (baseObj->isArray() ? 1 : 0)
                        << " base_struct=" << (baseObj->isStruct() ? 1 : 0);
                    if (const ICFGNode* allocNode = baseObj->getICFGNode())
                    {
                        out << " allocation_node=" << allocNode->getId()
                            << " allocation_function="
                            << (allocNode->getFun() ? allocNode->getFun()->getName() : "<none>")
                            << " allocation_file=" << sourceFileBase(allocNode)
                            << " allocation_line=" << sourceLine(allocNode);
                    }
                }
                out
                    << " access_last_byte=" << len.toString()
                    << " end_offset=" << offset.toString();
                if (strongestViolationEvidence.empty() ||
                        (definite && !strongestViolationDefinite))
                {
                    strongestViolationEvidence = out.str();
                    strongestViolationDefinite = definite;
                }
            }
        }
        else
        {
            sawSafeTarget = true;
            ++safeTargets;
        }
    }

    if (sawOverflow)
    {
        const bool mustOverflow = allViolationsDefinite &&
                                  !sawSafeTarget &&
                                  !sawUnresolvedTarget;
        lastBoundCheckConfidence = mustOverflow
                                   ? BoundConfidence::MustOverflow
                                   : BoundConfidence::MayOverflow;
        if (explainBoundChecksEnabled())
        {
            std::ostringstream out;
            out << strongestViolationEvidence
                << " confidence=" << (mustOverflow ? "must_overflow" : "may_overflow")
                << " checked_targets=" << checkedTargets
                << " violating_targets=" << violatingTargets
                << " safe_targets=" << safeTargets
                << " unknown_targets=" << unknownTargets;
            lastBoundCheckEvidence = out.str();
        }
        return false;
    }

    if (sawReportableUnknown)
    {
        lastBoundCheckConfidence = BoundConfidence::Inconclusive;
        if (explainBoundChecksEnabled())
        {
            std::ostringstream out;
            out << unknownEvidence
                << " confidence=inconclusive"
                << " checked_targets=" << checkedTargets
                << " safe_targets=" << safeTargets
                << " unknown_targets=" << unknownTargets;
            lastBoundCheckEvidence = out.str();
        }
        return false;
    }

    if (sawUnresolvedTarget && len.ub().getIntNumeral() >= 0)
    {
        lastBoundCheckConfidence = BoundConfidence::Inconclusive;
        if (explainBoundChecksEnabled())
        {
            std::ostringstream out;
            out << unresolvedEvidence
                << " confidence=inconclusive"
                << " checked_targets=" << checkedTargets
                << " safe_targets=" << safeTargets
                << " unknown_targets=" << unknownTargets;
            lastBoundCheckEvidence = out.str();
        }
        return false;
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


bool NullptrDerefDetector::canSafelyDerefPtr(const ValVar* value, const ICFGNode* node)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    const AbstractValue& AbsVal = ae.getAbsValue(value, node);
    if (isUninit(AbsVal)) return false;
    if (!AbsVal.isAddr()) return true;
    for (const auto &addr: AbsVal.getAddrs())
    {
        // if the addr itself is invalid mem, report unsafe
        if (AbstractState::isBlackHoleObjAddr(addr))
            return false;
        // if nullptr is detected, return unsafe
        else if (AbstractState::isNullMem(addr))
            return false;
        // if addr is labeled freed mem, report unsafe
        else if (ae.getAbsState(node).isFreedMem(addr))
            return false;
    }
    return true;
}
