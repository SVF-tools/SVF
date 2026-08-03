//===- AbstractExecution.cpp -- Abstract Execution---------------------------------//
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
//  Created on: Jan 10, 2024
//      Author: Xiao Cheng, Jiawei Wang
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/SparseAbstractInterpretation.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "SVFIR/SVFIR.h"
#include "Util/ExtAPI.h"
#include "Util/Options.h"
#include "Util/WorkList.h"
#include "Graphs/CallGraph.h"
#include "WPA/Andersen.h"
#include <cmath>
#include <memory>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <utility>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

using namespace SVF;
using namespace SVFUtil;

namespace
{
static std::string aeTmpPath(const char* name)
{
    const char* prefix = std::getenv("AE_TMP_PREFIX");
    if (prefix == nullptr || *prefix == '\0')
        return std::string("/tmp/") + name;
    return std::string("/tmp/") + prefix + "_" + name;
}

struct ValueProbeStats
{
    unsigned long calls = 0;
    unsigned long compared = 0;
    unsigned long fullSame = 0;
    unsigned long shapeSame = 0;
    unsigned long shapeSameButValueChanged = 0;
    unsigned long varValueChanged = 0;
    unsigned long varAddedRemoved = 0;
    unsigned long locValueChanged = 0;
    unsigned long locAddedRemoved = 0;
    unsigned long overlayReadSetMax = 0;
    unsigned long overlayStaleCalls = 0;
    unsigned long overlayStaleFields = 0;
    unsigned long sameStateButOverlayStale = 0;
    unsigned long lastVarN = 0;
    unsigned long lastLocN = 0;
};

static Map<const ICFGNode*, AbstractState> valueProbePrevInput;
static Map<std::string, ValueProbeStats> valueProbeStats;
static unsigned long valueProbeHotCalls = 0;

static bool valueProbeEnabled()
{
    static const bool enabled = (std::getenv("VALUEPROBE") != nullptr);
    return enabled;
}

static bool valueProbeHotFunction(const std::string& name)
{
    return name == "ndpi_workflow_node_cmp" ||
           name == "ndpi_default_ports_tree_node_t_cmp" ||
           name == "ndpi_malloc" ||
           name == "malloc_wrapper" ||
           name == "ndpi_free" ||
           name == "free_wrapper" ||
           name == "ndpi_tsearch" ||
           name == "cstrcasecmp" ||
           name == "memchr";
}

static void valueProbeDiffMap(const AbstractState::VarToAbsValMap& prev,
                              const AbstractState::VarToAbsValMap& cur,
                              unsigned long& valueChanged,
                              unsigned long& addedRemoved)
{
    for (const auto& kv : cur)
    {
        auto it = prev.find(kv.first);
        if (it == prev.end())
        {
            ++addedRemoved;
            continue;
        }
        if (!kv.second.equals(it->second))
            ++valueChanged;
    }
    for (const auto& kv : prev)
        if (cur.find(kv.first) == cur.end())
            ++addedRemoved;
}

static void valueProbeDumpSummary()
{
    if (!valueProbeEnabled())
        return;

    std::ofstream of(aeTmpPath("svf_valueprobe_summary.tsv"));
    of << "function\tcalls\tcompared\tfull_same\tfull_same_pct\tshape_same"
       << "\tshape_same_but_value_changed\tvar_value_changed"
       << "\tvar_added_removed\tloc_value_changed\tloc_added_removed"
       << "\toverlay_readset_max\toverlay_stale_calls"
       << "\toverlay_stale_fields\tsame_state_but_overlay_stale"
       << "\tlast_var_entries\tlast_loc_entries\n";
    for (const auto& kv : valueProbeStats)
    {
        const ValueProbeStats& s = kv.second;
        double pct = s.compared ? (100.0 * s.fullSame / s.compared) : 0.0;
        of << kv.first << "\t" << s.calls
           << "\t" << s.compared
           << "\t" << s.fullSame
           << "\t" << pct
           << "\t" << s.shapeSame
           << "\t" << s.shapeSameButValueChanged
           << "\t" << s.varValueChanged
           << "\t" << s.varAddedRemoved
           << "\t" << s.locValueChanged
           << "\t" << s.locAddedRemoved
           << "\t" << s.overlayReadSetMax
           << "\t" << s.overlayStaleCalls
           << "\t" << s.overlayStaleFields
           << "\t" << s.sameStateButOverlayStale
           << "\t" << s.lastVarN
           << "\t" << s.lastLocN
           << "\n";
    }
}

static void valueProbeAppendDetail(const std::string& name,
                                   unsigned long globalCall,
                                   unsigned long localCall,
                                   bool hadPrev,
                                   bool fullSame,
                                   unsigned long varN,
                                   unsigned long locN,
                                   unsigned long varChanged,
                                   unsigned long varAddedRemoved,
                                   unsigned long locChanged,
                                   unsigned long locAddedRemoved,
                                   unsigned long readSetSize,
                                   unsigned long staleFields)
{
    static bool wroteHeader = false;
    std::ofstream of;
    if (!wroteHeader)
    {
        of.open(aeTmpPath("svf_valueprobe_detail.tsv"));
        of << "global_call\tfunction\tlocal_call\thad_prev\tfull_same"
           << "\tvar_entries\tloc_entries\tvar_value_changed"
           << "\tvar_added_removed\tloc_value_changed\tloc_added_removed"
           << "\toverlay_readset\toverlay_stale_fields\n";
        wroteHeader = true;
    }
    else
    {
        of.open(aeTmpPath("svf_valueprobe_detail.tsv"), std::ios::app);
    }
    of << globalCall << "\t" << name << "\t" << localCall
       << "\t" << hadPrev
       << "\t" << fullSame
       << "\t" << varN
       << "\t" << locN
       << "\t" << varChanged
       << "\t" << varAddedRemoved
       << "\t" << locChanged
       << "\t" << locAddedRemoved
       << "\t" << readSetSize
       << "\t" << staleFields
       << "\n";
}

static const std::string& hotFuncMode()
{
    static const std::string mode =
        std::getenv("HOTFUNC_MODE") ? std::getenv("HOTFUNC_MODE") : "legacy";
    return mode;
}

static bool aeAllowMissingState()
{
    static const bool allow = []()
    {
        const char* raw = std::getenv("AE_ALLOW_MISSING_STATE");
        if (raw == nullptr || *raw == '\0')
            return false;
        std::string text(raw);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "off" && text != "OFF" && text != "no" && text != "NO";
    }();
    return allow;
}

static bool aeConservativeBranch()
{
    static const bool enabled = []()
    {
        const char* raw = std::getenv("AE_CONSERVATIVE_BRANCH");
        if (raw == nullptr || *raw == '\0')
            return false;
        std::string text(raw);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "off" && text != "OFF" && text != "no" && text != "NO";
    }();
    return enabled;
}

static bool aeBranchPruneTrace()
{
    static const bool enabled = []()
    {
        const char* raw = std::getenv("AE_BRANCH_PRUNE_TRACE");
        if (raw == nullptr || *raw == '\0')
            return false;
        std::string text(raw);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "off" && text != "OFF" && text != "no" && text != "NO";
    }();
    return enabled;
}

static unsigned long hotFuncEnvUL(const char* name, unsigned long fallback)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0')
        return fallback;

    char* end = nullptr;
    unsigned long value = std::strtoul(raw, &end, 10);
    return (end == raw) ? fallback : value;
}

static bool aeEnvBool(const char* name, bool fallback = false)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0')
        return fallback;
    std::string text(raw);
    return text != "0" && text != "false" && text != "FALSE" &&
           text != "off" && text != "OFF" && text != "no" && text != "NO";
}

static bool aeSkipNodePrevState()
{
    static const bool enabled = aeEnvBool("AE_SKIP_NODE_PREV_STATE", false);
    return enabled;
}

static bool aeCallsiteSensitivePE()
{
    static const bool enabled = aeEnvBool("AE_CALLSITE_SENSITIVE_PE", false);
    return enabled;
}

static unsigned long aeEntryNodeBudget()
{
    static const unsigned long budget = hotFuncEnvUL("AE_ENTRY_NODE_BUDGET", 0);
    return budget;
}

static unsigned long aeEntryBudgetAfter()
{
    static const unsigned long after = hotFuncEnvUL("AE_ENTRY_BUDGET_AFTER", 0);
    return after;
}

static unsigned long aeEntryBudgetDumpEvery()
{
    static const unsigned long every = hotFuncEnvUL("AE_ENTRY_BUDGET_DUMP_EVERY", 100);
    return every == 0 ? 100 : every;
}

static void aeTrimHeap()
{
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

static unsigned long hotFuncThreshold()
{
    static const unsigned long threshold = hotFuncEnvUL("HOTFUNC_THRESHOLD", 64);
    return threshold;
}

static unsigned long hotFuncDumpEvery()
{
    static const unsigned long every = hotFuncEnvUL("HOTFUNC_DUMP_EVERY", 1000);
    return every == 0 ? 1000 : every;
}

static bool hotFuncExactMode()
{
    const std::string& mode = hotFuncMode();
    return mode == "exact" || mode == "exact-replay";
}

static bool hotFuncLegacyMode()
{
    return hotFuncMode() == "legacy";
}

static bool spinProbeLiveEnabled()
{
    static const bool enabled = (std::getenv("SPINPROBE_LIVE") != nullptr);
    return enabled;
}

static unsigned long spinProbeLiveEvery()
{
    static const unsigned long every = hotFuncEnvUL("SPINPROBE_LIVE_EVERY", 1);
    return every == 0 ? 1 : every;
}

static const char* spinProbeNodeKind(const ICFGNode* node)
{
    if (SVFUtil::isa<CallICFGNode>(node))
        return "call";
    if (SVFUtil::isa<RetICFGNode>(node))
        return "ret";
    if (SVFUtil::isa<FunEntryICFGNode>(node))
        return "fun_entry";
    if (SVFUtil::isa<FunExitICFGNode>(node))
        return "fun_exit";
    if (SVFUtil::isa<GlobalICFGNode>(node))
        return "global";
    return "intra";
}

static bool spinProbeStmtEnabled()
{
    static const bool enabled = (std::getenv("SPINPROBE_STMT") != nullptr);
    return enabled;
}

static unsigned long spinProbeStmtNode()
{
    static const unsigned long node = hotFuncEnvUL("SPINPROBE_STMT_NODE", 0);
    return node;
}

static unsigned long spinProbeStmtMin()
{
    static const unsigned long min = hotFuncEnvUL("SPINPROBE_STMT_MIN", 10000);
    return min;
}

static unsigned long spinProbeStmtEvery()
{
    static const unsigned long every = hotFuncEnvUL("SPINPROBE_STMT_EVERY", 1000);
    return every == 0 ? 1000 : every;
}

static const char* spinProbeStmtKind(const SVFStmt* stmt)
{
    if (SVFUtil::isa<AddrStmt>(stmt)) return "addr";
    if (SVFUtil::isa<BinaryOPStmt>(stmt)) return "binary";
    if (SVFUtil::isa<CmpStmt>(stmt)) return "cmp";
    if (SVFUtil::isa<UnaryOPStmt>(stmt)) return "unary";
    if (SVFUtil::isa<BranchStmt>(stmt)) return "branch";
    if (SVFUtil::isa<LoadStmt>(stmt)) return "load";
    if (SVFUtil::isa<StoreStmt>(stmt)) return "store";
    if (SVFUtil::isa<CopyStmt>(stmt)) return "copy";
    if (SVFUtil::isa<GepStmt>(stmt)) return "gep";
    if (SVFUtil::isa<SelectStmt>(stmt)) return "select";
    if (SVFUtil::isa<PhiStmt>(stmt)) return "phi";
    if (SVFUtil::isa<CallPE>(stmt)) return "callpe";
    if (SVFUtil::isa<RetPE>(stmt)) return "retpe";
    return "unknown";
}

static bool aeSkipExtMemStmtsEnabled()
{
    static const bool enabled = (std::getenv("AE_SKIP_EXT_MEM_STMTS") != nullptr);
    return enabled;
}

static unsigned long aeSkipExtMemStmtMin()
{
    static const unsigned long min = hotFuncEnvUL("AE_SKIP_EXT_MEM_STMT_MIN", 10000);
    return min;
}

static const char* extMemorySummaryKind(const CallICFGNode* callNode)
{
    if (!callNode)
        return nullptr;
    const FunObjVar* fun = callNode->getCalledFunction();
    if (!fun || !SVFUtil::isExtCall(fun))
        return nullptr;

    for (const std::string& annotation :
            ExtAPI::getExtAPI()->getExtFuncAnnotations(fun))
    {
        if (annotation.find("MEMCPY") != std::string::npos)
            return "memcpy";
        if (annotation.find("MEMSET") != std::string::npos)
            return "memset";
    }
    return nullptr;
}

struct ExtMemStmtSkipStats
{
    unsigned long nodes = 0;
    unsigned long stmts = 0;
    Map<std::string, unsigned long> nodesByFunction;
    Map<std::string, unsigned long> stmtsByFunction;
};

static ExtMemStmtSkipStats extMemStmtSkipStats;

static void dumpExtMemStmtSkipStats(const char* reason)
{
    std::ofstream of(aeTmpPath("svf_ext_mem_stmt_skip.tsv"));
    of << "# reason=" << reason
       << "\tenabled=" << (aeSkipExtMemStmtsEnabled() ? 1 : 0)
       << "\tmin_stmt=" << aeSkipExtMemStmtMin()
       << "\ttotal_nodes=" << extMemStmtSkipStats.nodes
       << "\ttotal_stmts=" << extMemStmtSkipStats.stmts
       << "\n";
    of << "function\tskipped_nodes\tskipped_stmts\n";
    for (const auto& kv : extMemStmtSkipStats.nodesByFunction)
    {
        const std::string& name = kv.first;
        of << name
           << "\t" << kv.second
           << "\t" << extMemStmtSkipStats.stmtsByFunction[name]
           << "\n";
    }
}

static void recordExtMemStmtSkip(const ICFGNode* node,
                                 unsigned long stmtTotal,
                                 const char* kind)
{
    (void)kind;
    const std::string name = node->getFun()->getName();
    ++extMemStmtSkipStats.nodes;
    extMemStmtSkipStats.stmts += stmtTotal;
    ++extMemStmtSkipStats.nodesByFunction[name];
    extMemStmtSkipStats.stmtsByFunction[name] += stmtTotal;
    dumpExtMemStmtSkipStats("live");
}

static bool aeHotCyclePhiTopEnabled()
{
    static const bool enabled = (std::getenv("AE_HOT_CYCLE_PHI_TOP") != nullptr);
    return enabled;
}

struct HotCyclePhiTopStats
{
    unsigned long hits = 0;
    Map<std::string, unsigned long> hitsByFunction;
};

static HotCyclePhiTopStats hotCyclePhiTopStats;

static void recordHotCyclePhiTop(const ICFGNode* node)
{
    ++hotCyclePhiTopStats.hits;
    ++hotCyclePhiTopStats.hitsByFunction[node->getFun()->getName()];

    std::ofstream of(aeTmpPath("svf_hotcycle_phi_top.tsv"));
    of << "# enabled=" << (aeHotCyclePhiTopEnabled() ? 1 : 0)
       << "\ttotal_hits=" << hotCyclePhiTopStats.hits
       << "\n";
    of << "function\thits\n";
    for (const auto& kv : hotCyclePhiTopStats.hitsByFunction)
        of << kv.first << "\t" << kv.second << "\n";
}

static bool aeHotFunctionTopEnabled()
{
    static const bool enabled = (std::getenv("AE_HOT_FUNC_TOP") != nullptr);
    return enabled;
}

static unsigned long aeHotFunctionTopThreshold()
{
    static const unsigned long threshold = hotFuncEnvUL("AE_HOT_FUNC_TOP_THRESHOLD", 8);
    return threshold == 0 ? 1 : threshold;
}

static unsigned long aeHotFunctionTopMinStmts()
{
    static const unsigned long minimum =
        hotFuncEnvUL("AE_HOT_FUNC_TOP_MIN_STMTS", 0);
    return minimum;
}

static bool aeForceTopFunctionName(const std::string& functionName)
{
    const char* env = std::getenv("AE_FORCE_TOP_FUNS");
    if (env == nullptr || *env == '\0')
        return false;

    const std::string specs(env);
    size_t start = 0;
    while (start <= specs.size())
    {
        size_t end = specs.find(',', start);
        if (end == std::string::npos)
            end = specs.size();

        size_t first = start;
        while (first < end && std::isspace(static_cast<unsigned char>(specs[first])))
            ++first;
        size_t last = end;
        while (last > first && std::isspace(static_cast<unsigned char>(specs[last - 1])))
            --last;

        if (last > first)
        {
            const std::string token = specs.substr(first, last - first);
            if (functionName.find(token) != std::string::npos)
                return true;
        }

        if (end == specs.size())
            break;
        start = end + 1;
    }
    return false;
}

struct HotFunctionTopStats
{
    unsigned long hits = 0;
    unsigned long varTops = 0;
    unsigned long locTops = 0;
    Map<std::string, unsigned long> hitsByFunction;
    Map<std::string, unsigned long> varTopsByFunction;
    Map<std::string, unsigned long> locTopsByFunction;
};

static HotFunctionTopStats hotFunctionTopStats;

template <typename ComponentRange>
static void collectWTONodes(const ComponentRange& comps,
                            std::vector<const ICFGNode*>& nodes)
{
    for (const ICFGWTOComp* comp : comps)
    {
        if (const ICFGSingletonWTO* singleton =
                SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            nodes.push_back(singleton->getICFGNode());
        }
        else if (const ICFGCycleWTO* cycle =
                     SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            nodes.push_back(cycle->head()->getICFGNode());
            collectWTONodes(cycle->getWTOComponents(), nodes);
        }
    }
}

static void recordHotFunctionTop(const ICFGNode* funEntry,
                                 unsigned long varTops,
                                 unsigned long locTops)
{
    const std::string name = funEntry->getFun()->getName();
    ++hotFunctionTopStats.hits;
    hotFunctionTopStats.varTops += varTops;
    hotFunctionTopStats.locTops += locTops;
    ++hotFunctionTopStats.hitsByFunction[name];
    hotFunctionTopStats.varTopsByFunction[name] += varTops;
    hotFunctionTopStats.locTopsByFunction[name] += locTops;

    std::ofstream of(aeTmpPath("svf_hotfunc_top.tsv"));
    of << "# enabled=" << (aeHotFunctionTopEnabled() ? 1 : 0)
       << "\tthreshold=" << aeHotFunctionTopThreshold()
       << "\tmin_stmts=" << aeHotFunctionTopMinStmts()
       << "\ttotal_hits=" << hotFunctionTopStats.hits
       << "\ttotal_var_tops=" << hotFunctionTopStats.varTops
       << "\ttotal_loc_tops=" << hotFunctionTopStats.locTops
       << "\n";
    of << "function\thits\tvar_tops\tloc_tops\n";
    for (const auto& kv : hotFunctionTopStats.hitsByFunction)
    {
        const std::string& fun = kv.first;
        of << fun << "\t" << kv.second
           << "\t" << hotFunctionTopStats.varTopsByFunction[fun]
           << "\t" << hotFunctionTopStats.locTopsByFunction[fun]
           << "\n";
    }
}

static bool spinProbeStmtProfileEnabled()
{
    static const bool enabled = (std::getenv("SPINPROBE_STMT_PROFILE") != nullptr);
    return enabled;
}

static void dumpSpinProbeStmtProfile(const ICFGNode* node)
{
    if (!spinProbeStmtProfileEnabled())
        return;

    const unsigned long stmtTotal = node->getSVFStmts().size();
    if (spinProbeStmtNode() != 0 && node->getId() != spinProbeStmtNode())
        return;
    if (spinProbeStmtNode() == 0 && stmtTotal < spinProbeStmtMin())
        return;

    static Set<NodeID> dumpedNodes;
    if (dumpedNodes.find(node->getId()) != dumpedNodes.end())
        return;
    dumpedNodes.insert(node->getId());

    Map<std::string, unsigned long> counts;
    std::vector<std::pair<unsigned long, std::string>> firstKinds;
    std::vector<std::pair<unsigned long, std::string>> lastKinds;
    unsigned long stmtIndex = 0;
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        ++stmtIndex;
        const std::string kind = spinProbeStmtKind(stmt);
        ++counts[kind];

        if (firstKinds.size() < 16)
            firstKinds.push_back(std::make_pair(stmtIndex, kind));

        if (lastKinds.size() == 16)
            lastKinds.erase(lastKinds.begin());
        lastKinds.push_back(std::make_pair(stmtIndex, kind));
    }

    std::ofstream of(aeTmpPath("svf_stmt_profile.tsv"), std::ios::app);
    of << "# node_fun=" << node->getFun()->getName()
       << "\tnode_id=" << node->getId()
       << "\tnode_kind=" << spinProbeNodeKind(node)
       << "\tstmt_total=" << stmtTotal
       << "\n";
    of << "section\tindex\tkind\tcount\n";

    const char* orderedKinds[] = {
        "addr", "binary", "cmp", "unary", "branch", "load", "store",
        "copy", "gep", "select", "phi", "callpe", "retpe", "unknown"
    };
    for (const char* kind : orderedKinds)
    {
        auto it = counts.find(kind);
        if (it != counts.end() && it->second != 0)
            of << "count\t0\t" << kind << "\t" << it->second << "\n";
    }

    for (const auto& item : firstKinds)
        of << "first\t" << item.first << "\t" << item.second << "\t0\n";
    for (const auto& item : lastKinds)
        of << "last\t" << item.first << "\t" << item.second << "\t0\n";
}
}


bool AbstractInterpretation::isExtMemHeavyNode(const ICFGNode* node) const
{
    return extMemHeavyNodes.find(node) != extMemHeavyNodes.end();
}

const AbstractInterpretation::ExtMemHeavyInfo*
AbstractInterpretation::getExtMemHeavyInfo(const ICFGNode* node) const
{
    auto it = extMemHeavyInfos.find(node);
    return it == extMemHeavyInfos.end() ? nullptr : &it->second;
}

void AbstractInterpretation::preAnalyzeExtMemHeavyNodes()
{
    extMemHeavyNodes.clear();
    extMemHeavyInfos.clear();

    if (!aeSkipExtMemStmtsEnabled() || icfg == nullptr)
    {
        dumpExtMemHeavyPreAnalysisStats("disabled");
        return;
    }

    for (ICFG::const_iterator it = icfg->begin(), eit = icfg->end();
         it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const unsigned long stmtTotal = node->getSVFStmts().size();
        if (stmtTotal < aeSkipExtMemStmtMin())
            continue;

        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        const char* summaryKind = extMemorySummaryKind(callNode);
        if (!summaryKind)
            continue;

        ExtMemHeavyInfo info;
        info.stmtTotal = stmtTotal;
        info.summaryKind = summaryKind;
        info.functionName = node->getFun() ? node->getFun()->getName() : "<unknown>";
        const FunObjVar* callee = callNode->getCalledFunction();
        info.calleeName = callee ? callee->getName() : "<unknown>";

        extMemHeavyNodes.insert(node);
        extMemHeavyInfos[node] = info;
    }

    dumpExtMemHeavyPreAnalysisStats("pre");
}

void AbstractInterpretation::dumpExtMemHeavyPreAnalysisStats(const char* reason) const
{
    std::ofstream of(aeTmpPath("svf_ext_mem_heavy_pre.tsv"));

    unsigned long totalStmts = 0;
    for (const auto& kv : extMemHeavyInfos)
        totalStmts += kv.second.stmtTotal;

    of << "# reason=" << reason
       << "\tenabled=" << (aeSkipExtMemStmtsEnabled() ? 1 : 0)
       << "\tmin_stmt=" << aeSkipExtMemStmtMin()
       << "\ttotal_nodes=" << extMemHeavyInfos.size()
       << "\ttotal_stmts=" << totalStmts
       << "\n";
    of << "node_id\tfunction\tcallee\tsummary_kind\tstmt_total\n";

    std::vector<const ICFGNode*> nodes;
    nodes.reserve(extMemHeavyInfos.size());
    for (const auto& kv : extMemHeavyInfos)
        nodes.push_back(kv.first);
    std::sort(nodes.begin(), nodes.end(),
              [](const ICFGNode* lhs, const ICFGNode* rhs) {
                  return lhs->getId() < rhs->getId();
              });

    for (const ICFGNode* node : nodes)
    {
        const ExtMemHeavyInfo& info = extMemHeavyInfos.find(node)->second;
        of << node->getId()
           << "\t" << info.functionName
           << "\t" << info.calleeName
           << "\t" << info.summaryKind
           << "\t" << info.stmtTotal
           << "\n";
    }
}

void AbstractInterpretation::runOnModule()
{
    stat->startClk();
    utils = new AbsExtAPI(this);
    /// collect checkpoint
    utils->collectCheckPoint();
    preAnalyzeExtMemHeavyNodes();

    analyse();
    SVFUtil::errs() << "[LOOP-MEMO] skipped=" << loopMemoHits << "/"
                    << loopMemoTotal << " cycle invocations\n";
    SVFUtil::errs() << "[FUNC-MEMO] skipped=" << funcMemoHits << "/"
                    << funcMemoTotal << " function invocations\n";
    valueProbeDumpSummary();
    dumpHotFunctionStats();
    dumpSpinProbe("final");
    dumpHotCycleThrottleStats("final");
    dumpExtMemStmtSkipStats("final");
    utils->checkPointAllSet();
    stat->endClk();
    stat->finializeStat();
    if (Options::PStat())
        stat->performStat();
    for (auto& detector: detectors)
        detector->reportBug();
}

void AbstractInterpretation::dumpHotFunctionStats() const
{
    auto getCount = [](const Map<const ICFGNode*, unsigned long>& m,
                       const ICFGNode* n) -> unsigned long
    {
        auto it = m.find(n);
        return it == m.end() ? 0 : it->second;
    };

    std::vector<const ICFGNode*> funcs;
    funcs.reserve(funcCallCount.size());
    for (const auto& kv : funcCallCount)
        funcs.push_back(kv.first);

    std::sort(funcs.begin(), funcs.end(),
              [&](const ICFGNode* lhs, const ICFGNode* rhs)
              {
                  const unsigned long lhsBody = getCount(funcBodyExecCount, lhs);
                  const unsigned long rhsBody = getCount(funcBodyExecCount, rhs);
                  if (lhsBody != rhsBody)
                      return lhsBody > rhsBody;

                  const unsigned long lhsEnter = getCount(funcCallCount, lhs);
                  const unsigned long rhsEnter = getCount(funcCallCount, rhs);
                  if (lhsEnter != rhsEnter)
                      return lhsEnter > rhsEnter;

                  return lhs->getFun()->getName() < rhs->getFun()->getName();
              });

    {
        std::ofstream of(aeTmpPath("svf_funccount.txt"));
        of << "total_calls=" << funcCallTotal
           << " total_body_exec=" << funcBodyExecTotal
           << " distinct_funcs=" << funcCallCount.size()
           << " mode=" << hotFuncMode()
           << " hot_threshold=" << hotFuncThreshold()
           << "\n";
        for (size_t i = 0; i < funcs.size() && i < 60; ++i)
        {
            const ICFGNode* f = funcs[i];
            of << getCount(funcCallCount, f) << "\t"
               << getCount(funcBodyExecCount, f) << "\t"
               << getCount(funcFastHitCount, f) << "\t"
               << f->getFun()->getName() << "\n";
        }
    }

    std::ofstream tsv(aeTmpPath("svf_hotfunc_counts.tsv"));
    tsv << "# mode=" << hotFuncMode()
        << "\thot_threshold=" << hotFuncThreshold()
        << "\ttotal_enter=" << funcCallTotal
        << "\ttotal_body_exec=" << funcBodyExecTotal
        << "\ttotal_fast_hit=" << funcMemoHits
        << "\n";
    tsv << "function\tenter\tbody_exec\tfast_hit\thot_attempt"
        << "\tfallback_no_cache\tfallback_state_diff"
        << "\tfallback_overlay_stale\tlast_overlay_readset"
        << "\tstatic_stmt_count\tsmall_body_top_bypass\n";

    for (const ICFGNode* f : funcs)
    {
        unsigned long readSetSize = 0;
        auto rvit = funcReadVersions.find(f);
        if (rvit != funcReadVersions.end())
            readSetSize = rvit->second.size();

        tsv << f->getFun()->getName()
            << "\t" << getCount(funcCallCount, f)
            << "\t" << getCount(funcBodyExecCount, f)
            << "\t" << getCount(funcFastHitCount, f)
            << "\t" << getCount(funcHotAttemptCount, f)
            << "\t" << getCount(funcHotNoCacheCount, f)
            << "\t" << getCount(funcHotStateDiffCount, f)
            << "\t" << getCount(funcHotOverlayStaleCount, f)
            << "\t" << readSetSize
            << "\t" << getCount(funcStaticStmtCount, f)
            << "\t" << getCount(funcSmallBodyTopBypassCount, f)
            << "\n";
    }
}

void AbstractInterpretation::dumpSpinProbe(const char* reason) const
{
    std::ofstream cur(aeTmpPath("svf_spinprobe_current.txt"));
    cur << "reason=" << reason
        << "\ttotal_enter=" << funcCallTotal
        << "\ttotal_body_exec=" << funcBodyExecTotal
        << "\ttotal_fast_hit=" << funcMemoHits
        << "\tglobal_loop_iters=" << spinGlobalLoopIters
        << "\tglobal_node_execs=" << spinGlobalNodeExecs
        << "\n";

    if (spinActiveCycle)
    {
        const ICFGNode* head = spinActiveCycle->head()->getICFGNode();
        cur << "active_cycle_fun=" << head->getFun()->getName()
            << "\tactive_cycle_head_id=" << head->getId()
            << "\tactive_iter=" << spinActiveIter
            << "\tcycle_iters=" << (spinCycleIterations.find(spinActiveCycle) == spinCycleIterations.end() ? 0 : spinCycleIterations.find(spinActiveCycle)->second)
            << "\n";
    }
    else
    {
        cur << "active_cycle_fun=\tactive_cycle_head_id=\tactive_iter=0\tcycle_iters=0\n";
    }

    if (spinActiveNode)
    {
        cur << "active_node_fun=" << spinActiveNode->getFun()->getName()
            << "\tactive_node_id=" << spinActiveNode->getId()
            << "\n";
    }
    else
    {
        cur << "active_node_fun=\tactive_node_id=\n";
    }

    std::vector<const ICFGCycleWTO*> cycles;
    cycles.reserve(spinCycleIterations.size());
    for (const auto& kv : spinCycleIterations)
        cycles.push_back(kv.first);

    auto getCount = [](const Map<const ICFGCycleWTO*, unsigned long>& m,
                       const ICFGCycleWTO* c) -> unsigned long
    {
        auto it = m.find(c);
        return it == m.end() ? 0 : it->second;
    };

    std::sort(cycles.begin(), cycles.end(),
              [&](const ICFGCycleWTO* lhs, const ICFGCycleWTO* rhs)
              {
                  const unsigned long li = getCount(spinCycleIterations, lhs);
                  const unsigned long ri = getCount(spinCycleIterations, rhs);
                  if (li != ri)
                      return li > ri;
                  return lhs->head()->getICFGNode()->getId() < rhs->head()->getICFGNode()->getId();
              });

    std::ofstream tsv(aeTmpPath("svf_spinprobe.tsv"));
    tsv << "# reason=" << reason
        << "\ttotal_enter=" << funcCallTotal
        << "\ttotal_body_exec=" << funcBodyExecTotal
        << "\ttotal_fast_hit=" << funcMemoHits
        << "\tglobal_loop_iters=" << spinGlobalLoopIters
        << "\tglobal_node_execs=" << spinGlobalNodeExecs
        << "\n";
    tsv << "function\thead_node_id\tinvocations\titerations\thead_execs"
        << "\tbody_node_execs\tsubcycle_calls\twiden_fixpoints\tnarrow_fixpoints\n";

    for (const ICFGCycleWTO* c : cycles)
    {
        const ICFGNode* head = c->head()->getICFGNode();
        tsv << head->getFun()->getName()
            << "\t" << head->getId()
            << "\t" << getCount(spinCycleInvocations, c)
            << "\t" << getCount(spinCycleIterations, c)
            << "\t" << getCount(spinCycleHeadExecs, c)
            << "\t" << getCount(spinCycleBodyNodeExecs, c)
            << "\t" << getCount(spinCycleSubcycleCalls, c)
            << "\t" << getCount(spinCycleWidenFixpoints, c)
            << "\t" << getCount(spinCycleNarrowFixpoints, c)
            << "\n";
    }
}

void AbstractInterpretation::dumpSpinProbeLive(const ICFGNode* node, const char* reason) const
{
    const std::string tmpPath = aeTmpPath("svf_spinprobe_live.tmp");
    const std::string finalPath = aeTmpPath("svf_spinprobe_live.txt");
    std::ofstream live(tmpPath);
    live << "reason=" << reason
         << "\ttotal_enter=" << funcCallTotal
         << "\ttotal_body_exec=" << funcBodyExecTotal
         << "\ttotal_fast_hit=" << funcMemoHits
         << "\tglobal_loop_iters=" << spinGlobalLoopIters
         << "\tglobal_node_execs=" << spinGlobalNodeExecs
         << "\n";

    if (spinActiveCycle)
    {
        const ICFGNode* head = spinActiveCycle->head()->getICFGNode();
        live << "active_cycle_fun=" << head->getFun()->getName()
             << "\tactive_cycle_head_id=" << head->getId()
             << "\tactive_iter=" << spinActiveIter
             << "\n";
    }
    else
    {
        live << "active_cycle_fun=\tactive_cycle_head_id=\tactive_iter=0\n";
    }

    if (node)
    {
        live << "node_fun=" << node->getFun()->getName()
             << "\tnode_id=" << node->getId()
             << "\tnode_kind=" << spinProbeNodeKind(node)
             << "\tstmt_count=" << node->getSVFStmts().size()
             << "\n";
    }
    else
    {
        live << "node_fun=\tnode_id=\tnode_kind=\tstmt_count=0\n";
    }
    live.close();
    std::rename(tmpPath.c_str(), finalPath.c_str());
}

void AbstractInterpretation::dumpSpinProbeStmtLive(const ICFGNode* node,
                                                   unsigned long stmtIndex,
                                                   unsigned long stmtTotal,
                                                   const char* stmtKind) const
{
    const std::string tmpPath = aeTmpPath("svf_spinprobe_stmt.tmp");
    const std::string finalPath = aeTmpPath("svf_spinprobe_stmt.txt");
    std::ofstream live(tmpPath);
    live << "total_enter=" << funcCallTotal
         << "\ttotal_body_exec=" << funcBodyExecTotal
         << "\ttotal_fast_hit=" << funcMemoHits
         << "\tglobal_loop_iters=" << spinGlobalLoopIters
         << "\tglobal_node_execs=" << spinGlobalNodeExecs
         << "\n";

    if (spinActiveCycle)
    {
        const ICFGNode* head = spinActiveCycle->head()->getICFGNode();
        live << "active_cycle_fun=" << head->getFun()->getName()
             << "\tactive_cycle_head_id=" << head->getId()
             << "\tactive_iter=" << spinActiveIter
             << "\n";
    }
    else
    {
        live << "active_cycle_fun=\tactive_cycle_head_id=\tactive_iter=0\n";
    }

    live << "node_fun=" << node->getFun()->getName()
         << "\tnode_id=" << node->getId()
         << "\tnode_kind=" << spinProbeNodeKind(node)
         << "\tstmt_index=" << stmtIndex
         << "\tstmt_total=" << stmtTotal
         << "\tstmt_kind=" << stmtKind
         << "\n";
    live.close();
    std::rename(tmpPath.c_str(), finalPath.c_str());
}

AbstractInterpretation::AbstractInterpretation()
{
    stat = new AEStat(this);
    // Run Andersen's pointer analysis and build WTO
    svfir = PAG::getPAG();
    icfg = svfir->getICFG();
    preAnalysis = new AEWTO(svfir, icfg);
    callGraph = preAnalysis->getCallGraph();
    icfg->updateCallGraph(callGraph);
    preAnalysis->initWTO();
}

/// Factory: first call allocates the concrete subclass based on
/// Options::AESparsity(); all subsequent calls return the same instance.
/// Must only be called after the option parser has populated AESparsity.
AbstractInterpretation& AbstractInterpretation::getAEInstance()
{
    // Leak the singleton on purpose.  AbstractInterpretation owns a
    // Map<std::string, std::function<void(const CallICFGNode*)>> func_map
    // whose lambda closures back-reference state owned by other globals
    // (preAnalysis's WTO, the call graph, ...).  Letting the static
    // unique_ptr's atexit-time destructor run hits a static-destruction-
    // order issue: the func_map hashtable's destructor calls into
    // std::function destroyers whose closures touch already-destroyed
    // state, and ~_Hashtable() segfaults during normal program shutdown.
    //
    // Reliably reproducible from any downstream tool that drives a full
    // AE analysis to completion and then exits normally:
    //   - SSA's ass3 binary (Software-Security-Analysis/Assignment-3)
    //   - pysvf via Python interpreter shutdown
    //
    // A process-lifetime singleton has no observable lifecycle past
    // program exit, so leaking is benign and avoids the use-after-destroy.
    static AbstractInterpretation* instance = []() -> AbstractInterpretation*
    {
        switch (Options::AESparsity())
        {
        case AESparsity::SemiSparse:
            return new SemiSparseAbstractInterpretation();
        case AESparsity::Sparse:
            return new FullSparseAbstractInterpretation();
        case AESparsity::Dense:
        default:
            return new AbstractInterpretation();
        }
    }();
    return *instance;
}


/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete utils;
    delete stat;
    delete preAnalysis;
}

/// Collect entry point functions for analysis.
/// In main mode, entry is main/svf.main. In no-main mode,
/// entries are SCCs with no external caller in the Andersen-resolved CallGraph.
FIFOWorkList<const FunObjVar*> AbstractInterpretation::collectProgEntryFuns()
{
    FIFOWorkList<const FunObjVar*> entryFunctions;
    const bool mainEntry = Options::AEFunEntry() == AEFunEntryMode::MAIN;
    Set<NodeID> visitedEntrySCCs;
    auto* callGraphSCC = preAnalysis->getCallGraphSCC();

    for (auto it = callGraph->begin(); it != callGraph->end(); ++it)
    {
        const CallGraphNode* cgNode = it->second;
        const FunObjVar* fun = cgNode->getFunction();

        // Skip declarations
        if (fun->isDeclaration())
            continue;

        if (mainEntry)
        {
            if (SVFUtil::isProgEntryFunction(fun))
            {
                entryFunctions.push(fun);
                break;
            }
        }
        else
        {
            NodeID repNodeId = callGraphSCC->repNode(cgNode->getId());
            if (visitedEntrySCCs.count(repNodeId))
                continue;

            const NodeBS& cgSCCNodes = callGraphSCC->subNodes(repNodeId);
            bool hasExternalCaller = false;
            for (NodeID nodeId : cgSCCNodes)
            {
                const CallGraphNode* sccNode = callGraph->getGNode(nodeId);
                for (auto inEdge : sccNode->getInEdges())
                {
                    if (!cgSCCNodes.test(inEdge->getSrcID()))
                    {
                        hasExternalCaller = true;
                        break;
                    }
                }
                if (hasExternalCaller)
                    break;
            }

            if (hasExternalCaller)
                continue;

            visitedEntrySCCs.insert(repNodeId);
            const FunObjVar* entryFun = fun;
            for (NodeID nodeId : cgSCCNodes)
            {
                const FunObjVar* sccFun = callGraph->getGNode(nodeId)->getFunction();
                if (SVFUtil::isProgEntryFunction(sccFun))
                {
                    entryFun = sccFun;
                    break;
                }
            }
            entryFunctions.push(entryFun);
        }
    }

    if (mainEntry && entryFunctions.empty())
    {
        SVFUtil::errs() << SVFUtil::errMsg(
                            "AE -ae-fun-entry=main requires a program entry function, but main/svf.main was not found.\n");
        assert(false && "No program entry function found for -ae-fun-entry=main");
        abort();
    }

    return entryFunctions;
}


/// Program entry - entry policy is selected by -ae-fun-entry.
void AbstractInterpretation::analyse()
{
    analyzeFromAllProgEntries();
}

/// Analyze the entry functions selected by collectProgEntryFuns().
/// Abstract state is shared across entry points so that functions analyzed from
/// earlier entries are not re-analyzed from scratch.
void AbstractInterpretation::analyzeFromAllProgEntries()
{
    // Collect all entry point functions
    FIFOWorkList<const FunObjVar*> entryFunctions = collectProgEntryFuns();

    if (entryFunctions.empty())
    {
        assert(false && "No entry functions found for analysis");
        return;
    }
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    const ICFGNode* globalNode = icfg->getGlobalICFGNode();
    const unsigned long entryLimit = hotFuncEnvUL("AE_ENTRY_LIMIT", 0);
    const unsigned long entryDumpEvery = hotFuncEnvUL("AE_ENTRY_DUMP_EVERY", 100);
    const bool resetStatePerEntry = aeEnvBool("AE_ENTRY_STATE_RESET", false);
    if (resetStatePerEntry)
        SVFUtil::errs() << "[AE-ENTRY-STATE] reset_per_entry=1\n";
    if (aeEntryNodeBudget() != 0)
        SVFUtil::errs() << "[AE-ENTRY-BUDGET] node_budget=" << aeEntryNodeBudget()
                        << " after=" << aeEntryBudgetAfter() << "\n";
    unsigned long analyzedEntries = 0;
    while (!entryFunctions.empty())
    {
        if (entryLimit != 0 && analyzedEntries >= entryLimit)
        {
            SVFUtil::errs() << "[AE-ENTRY-LIMIT] analyzed=" << analyzedEntries
                            << " remaining=" << entryFunctions.size() << "\n";
            break;
        }
        if (resetStatePerEntry && analyzedEntries != 0)
        {
            resetEntryTransientState();
        }
        const FunObjVar* entryFun = entryFunctions.pop();
        ++analyzedEntries;
        currentEntryIndex = analyzedEntries;
        currentEntryStartNodeExecs = spinGlobalNodeExecs;
        const ICFGNode* funEntry = icfg->getFunEntryICFGNode(entryFun);
        updateAbsState(funEntry, getAbsState(globalNode));
        handleFunction(funEntry, nullptr);
        if (entryDumpEvery != 0 &&
                (analyzedEntries == 1 ||
                 analyzedEntries % entryDumpEvery == 0 ||
                 entryFunctions.empty()))
        {
            SVFUtil::errs() << "[AE-ENTRY] analyzed=" << analyzedEntries
                            << " remaining=" << entryFunctions.size()
                            << " trace_nodes=" << abstractTrace.size()
                            << " all_nodes=" << allAnalyzedNodes.size()
                            << "\n";
        }
    }
}

void AbstractInterpretation::resetEntryTransientState()
{
    abstractTrace.clear();
    cycleInputCache.clear();
    cycleInputVer.clear();
    funcInputCache.clear();
    funcReadVersions.clear();
    gepReadStack.clear();
    gepOverlayVersion = 0;
    gepFieldVersion.clear();

    spinActiveCycle = nullptr;
    spinActiveNode = nullptr;
    spinActiveIter = 0;

    reportCallStack.clear();
    for (auto& detector : detectors)
        detector->resetTransientState();
    aeTrimHeap();
    handleGlobalNode();
}

/// handle global node
/// Initializes the abstract state for the global ICFG node and processes all global statements.
/// This includes setting up the null pointer and black hole pointer (blkPtr).
/// BlkPtr is initialized to point to the BlackHole object, representing
/// an unknown memory location that cannot be statically resolved.
void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    // Global init is one of the few legitimate direct-mutation sites:
    // updateAbsState filters out ValVars in semi-sparse mode, but NullPtr/
    // BlkPtr have no SVFVar so we cannot route them through updateAbsValue.
    // Use the manager's operator[] (auto-creates the entry if absent).
    AbstractState& init = abstractTrace[node];
    init = AbstractState();
    // TODO: we cannot find right SVFVar for NullPtr, so we use init[NullPtr]
    // directly. Same for BlkPtr below.
    init[IRGraph::NullPtr] = AddressValue();

    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }

    // BlkPtr is the canonical unknown value.  Keep its address-domain meaning
    // for pointer uses, and also give it numeric top so external-input stores
    // can flow through ordinary store/load state as [-inf, +inf].
    AbstractValue blkPtrValue(IntervalValue::top());
    blkPtrValue.getAddrs().insert(BlackHoleObjAddr);
    abstractTrace[node][PAG::getPAG()->getBlkPtr()] = blkPtrValue;
}

/// Pull-based state merge: for each predecessor that has an abstract state,
/// copy its state, apply branch refinement for conditional IntraCFGEdges,
/// and join all feasible states into getAbsState(node).
/// The join is dispatched through the manager so semi-sparse can skip
/// ValVar merging.
/// Returns true if at least one predecessor contributed state.
bool AbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode* node)
{
    // Collect all feasible predecessor states, then merge at the end.
    AbstractState merged;
    bool hasFeasiblePred = false;

    for (auto& edge : node->getInEdges())
    {
        const ICFGNode* pred = edge->getSrcNode();
        if (!hasAbsState(pred))
            continue;

        if (const IntraCFGEdge* intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
        {
            if (intraCfgEdge->getCondition())
            {
                AbstractState predState = getAbsState(pred);
                if (isBranchEdgeFeasible(intraCfgEdge, predState))
                {
                    if (!aeConservativeBranch())
                        collectBranchRefinement(intraCfgEdge, predState);
                    joinStates(merged, predState);
                    hasFeasiblePred = true;
                }
            }
            else
            {
                joinStates(merged, getAbsState(pred));
                hasFeasiblePred = true;
            }
        }
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            // Under bounded callsite sensitivity, a function invocation inherits
            // memory only from the caller currently being interpreted.  Joining
            // every incoming call edge here would immediately destroy the
            // CallPE precision by reintroducing unrelated caller states.
            if (aeCallsiteSensitivePE() && !reportCallStack.empty() &&
                    pred != reportCallStack.back())
                continue;
            joinStates(merged, getAbsState(pred));
            hasFeasiblePred = true;
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            switch (Options::HandleRecur())
            {
            case TOP:
                joinStates(merged, getAbsState(pred));
                hasFeasiblePred = true;
                break;
            case WIDEN_ONLY:
            case WIDEN_NARROW:
            {
                const RetICFGNode* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
                const CallICFGNode* callSite = returnSite->getCallICFGNode();
                if (hasAbsState(callSite))
                {
                    joinStates(merged, getAbsState(pred));
                    hasFeasiblePred = true;
                }
                break;
            }
            }
        }
    }

    if (!hasFeasiblePred)
        return false;

    updateAbsState(node, merged);

    return true;
}

/// Given a cmp operand, walk its SSA def edge to find the LoadStmt that
/// produced it. This lets us trace back to the ObjVar in memory so that
/// branch narrowing can refine the stored value.
///
/// Example: for `%cmp = icmp sgt %a, 5` where `%a = load i32, ptr %p`,
/// calling findBackingLoad(%a) returns the LoadStmt, and we can then
/// narrow the ObjVar behind %p.
///
/// Follows one level of CopyStmt (e.g., zext/sext) if the load is not
/// directly on the cmp operand. Returns nullptr if no load is found.
static const LoadStmt* findBackingLoad(const SVFVar* var)
{
    if (var->getInEdges().empty())
        return nullptr;
    SVFStmt* inStmt = *var->getInEdges().begin();
    if (const LoadStmt* ls = SVFUtil::dyn_cast<LoadStmt>(inStmt))
        return ls;
    if (const CopyStmt* cs = SVFUtil::dyn_cast<CopyStmt>(inStmt))
    {
        const SVFVar* src = cs->getRHSVar();
        if (!src->getInEdges().empty())
            return SVFUtil::dyn_cast<LoadStmt>(*src->getInEdges().begin());
    }
    return nullptr;
}

static bool aeVarDependsOnRetPE(const SVFVar* var, unsigned depth = 0)
{
    if (var == nullptr || depth > 3)
        return false;

    for (SVFStmt* inStmt : var->getInEdges())
    {
        if (SVFUtil::isa<RetPE>(inStmt))
            return true;

        if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(inStmt))
        {
            if (aeVarDependsOnRetPE(copy->getRHSVar(), depth + 1))
                return true;
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(inStmt))
        {
            for (u32_t i = 0; i < phi->getOpVarNum(); ++i)
                if (aeVarDependsOnRetPE(phi->getOpVar(i), depth + 1))
                    return true;
        }
    }

    return false;
}

static bool aeCmpDependsOnRetPE(const CmpStmt* cmpStmt)
{
    return aeVarDependsOnRetPE(cmpStmt->getOpVar(0)) ||
           aeVarDependsOnRetPE(cmpStmt->getOpVar(1));
}

/// Compute the interval constraint on one cmp operand given the predicate,
/// branch direction (succ), which side it is on, and the other operand's
/// interval. Returns top if no useful narrowing is possible.
///
/// Called from collectBranchRefinement for each non-constant operand that has a
/// backing load. Given a branch condition like:
///
///   %cmp = icmp sgt %a, 5       ;  a > 5
///   br i1 %cmp, label %T, %F
///
/// On the true branch (succ=1), operand %a (isLHS=true) is constrained to
/// [6, +inf). On the false branch (succ=0), %a is constrained to (-inf, 5].
/// The result is used to narrow the ObjVar behind %a's load.
static IntervalValue computeCmpConstraint(s32_t predicate, s64_t succ,
        bool isLHS, const IntervalValue& self,
        const IntervalValue& other)
{
    // Normalize: always reason from the LHS perspective.
    // If we are the RHS operand, swap the predicate direction.
    if (!isLHS)
    {
        // a > b from b's perspective: b < a
        static const Map<s32_t, s32_t> swapPred =
        {
            {CmpStmt::ICMP_EQ,  CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_NE,  CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGT},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGE},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UEQ},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UNE},
        };
        auto it = swapPred.find(predicate);
        if (it == swapPred.end()) return IntervalValue::top();
        predicate = it->second;
    }

    // If false branch, negate the predicate.
    if (succ == 0)
    {
        static const Map<s32_t, s32_t> negPred =
        {
            {CmpStmt::ICMP_EQ,  CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_NE,  CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGE},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGT},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UNE},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UEQ},
        };
        auto it = negPred.find(predicate);
        if (it == negPred.end()) return IntervalValue::top();
        predicate = it->second;
    }

    // Now compute the constraint on LHS given: LHS <predicate> other
    IntervalValue result = self;
    switch (predicate)
    {
    case CmpStmt::ICMP_EQ:
    case CmpStmt::FCMP_OEQ:
    case CmpStmt::FCMP_UEQ:
        result.meet_with(other);
        break;
    case CmpStmt::ICMP_NE:
    case CmpStmt::FCMP_ONE:
    case CmpStmt::FCMP_UNE:
    case CmpStmt::FCMP_FALSE:
    case CmpStmt::FCMP_TRUE:
        return IntervalValue::top(); // no useful narrowing
    case CmpStmt::ICMP_UGT:
    case CmpStmt::ICMP_SGT:
    case CmpStmt::FCMP_OGT:
    case CmpStmt::FCMP_UGT:
        result.meet_with(IntervalValue(other.lb() + 1, IntervalValue::plus_infinity()));
        break;
    case CmpStmt::ICMP_UGE:
    case CmpStmt::ICMP_SGE:
    case CmpStmt::FCMP_OGE:
    case CmpStmt::FCMP_UGE:
        result.meet_with(IntervalValue(other.lb(), IntervalValue::plus_infinity()));
        break;
    case CmpStmt::ICMP_ULT:
    case CmpStmt::ICMP_SLT:
    case CmpStmt::FCMP_OLT:
    case CmpStmt::FCMP_ULT:
        result.meet_with(IntervalValue(IntervalValue::minus_infinity(), other.ub() - 1));
        break;
    case CmpStmt::ICMP_ULE:
    case CmpStmt::ICMP_SLE:
    case CmpStmt::FCMP_OLE:
    case CmpStmt::FCMP_ULE:
        result.meet_with(IntervalValue(IntervalValue::minus_infinity(), other.ub()));
        break;
    default:
        return IntervalValue::top();
    }
    return result;
}

bool AbstractInterpretation::isCmpBranchEdgeFeasible(const IntraCFGEdge* edge,
        AbstractState& as)
{
    if (aeConservativeBranch())
        return true;

    const ICFGNode* pred = edge->getSrcNode();
    s64_t succ = edge->getSuccessorCondValue();
    const CmpStmt* cmpStmt = SVFUtil::cast<CmpStmt>(
                                 *edge->getCondition()->getInEdges().begin());

    if (cmpStmt->getOpVarID(0) == IRGraph::NullPtr ||
            cmpStmt->getOpVarID(1) == IRGraph::NullPtr)
        return true;

    AbstractValue opVal[2] =
    {
        getAbsValue(cmpStmt->getOpVar(0), pred),
        getAbsValue(cmpStmt->getOpVar(1), pred)
    };

    const bool hasIntervalCmp = opVal[0].isInterval() && opVal[1].isInterval();
    if (!hasIntervalCmp && (opVal[0].isAddr() || opVal[1].isAddr()))
        return true;

    if (aeCmpDependsOnRetPE(cmpStmt))
    {
        if (aeBranchPruneTrace())
        {
            SVFUtil::errs() << "[AE-BRANCH-KEEP] reason=retpe"
                            << " succ=" << succ
                            << " pred_node=" << pred->getId()
                            << " dst_node=" << edge->getDstNode()->getId()
                            << " pred_fun=" << (pred->getFun() ? pred->getFun()->getName() : "")
                            << " dst_fun=" << (edge->getDstNode()->getFun() ?
                                                edge->getDstNode()->getFun()->getName() : "")
                            << " pred_loc=" << pred->getSourceLoc()
                            << " dst_loc=" << edge->getDstNode()->getSourceLoc()
                            << " stmt=" << cmpStmt->toString() << "\n";
        }
        return true;
    }

    // Feasibility check: cmp result must be compatible with branch successor
    IntervalValue resVal = getAbsValue(cmpStmt->getRes(), pred).getInterval();
    resVal.meet_with(IntervalValue((s64_t)succ, succ));
    if (resVal.isBottom())
    {
        if (aeBranchPruneTrace())
        {
            const AbstractValue op0Val = getAbsValue(cmpStmt->getOpVar(0), pred);
            const AbstractValue op1Val = getAbsValue(cmpStmt->getOpVar(1), pred);
            const AbstractValue cmpVal = getAbsValue(cmpStmt->getRes(), pred);
            SVFUtil::errs() << "[AE-BRANCH-PRUNE] kind=cmp"
                            << " succ=" << succ
                            << " pred_node=" << pred->getId()
                            << " dst_node=" << edge->getDstNode()->getId()
                            << " pred_fun=" << (pred->getFun() ? pred->getFun()->getName() : "")
                            << " dst_fun=" << (edge->getDstNode()->getFun() ?
                                                edge->getDstNode()->getFun()->getName() : "")
                            << " pred_loc=" << pred->getSourceLoc()
                            << " dst_loc=" << edge->getDstNode()->getSourceLoc()
                            << " cmp_val=" << cmpVal.toString()
                            << " op0=" << op0Val.toString()
                            << " op1=" << op1Val.toString()
                            << " stmt=" << cmpStmt->toString() << "\n";
        }
        return false;
    }

    return true;
}

bool AbstractInterpretation::isSwitchBranchEdgeFeasible(
    const IntraCFGEdge* edge, AbstractState& as)
{
    if (aeConservativeBranch())
        return true;

    const ICFGNode* pred = edge->getSrcNode();
    s64_t succ = edge->getSuccessorCondValue();
    const SVFVar* var = edge->getCondition();

    AbstractValue condVal = getAbsValue(var, pred);
    IntervalValue switch_cond = condVal.getInterval();
    switch_cond.meet_with(IntervalValue(succ, succ));
    if (switch_cond.isBottom())
    {
        if (aeBranchPruneTrace())
        {
            SVFUtil::errs() << "[AE-BRANCH-PRUNE] kind=switch"
                            << " succ=" << succ
                            << " pred_node=" << pred->getId()
                            << " dst_node=" << edge->getDstNode()->getId()
                            << " pred_fun=" << (pred->getFun() ? pred->getFun()->getName() : "")
                            << " dst_fun=" << (edge->getDstNode()->getFun() ?
                                                edge->getDstNode()->getFun()->getName() : "")
                            << " pred_loc=" << pred->getSourceLoc()
                            << " dst_loc=" << edge->getDstNode()->getSourceLoc()
                            << " cond_val=" << condVal.toString() << "\n";
        }
        return false;
    }
    return true;
}

void AbstractInterpretation::collectBranchRefinement(const IntraCFGEdge* edge,
        AbstractState& as)
{
    if (aeConservativeBranch())
        return;

    const SVFVar* cond = edge->getCondition();
    const ICFGNode* pred = edge->getSrcNode();
    const ICFGNode* succNode = edge->getDstNode();
    s64_t succ = edge->getSuccessorCondValue();

    // A branch condition with no defining edge gives us nothing to refine on;
    // skip refinement rather than aborting (or dereferencing an empty list).
    if (cond->getInEdges().empty())
        return;
    const SVFStmt* condDef = *cond->getInEdges().begin();

    if (const CmpStmt* cmpStmt = SVFUtil::dyn_cast<CmpStmt>(condDef))
    {
        s32_t predicate = cmpStmt->getPredicate();

        if (cmpStmt->getOpVarID(0) == IRGraph::NullPtr ||
                cmpStmt->getOpVarID(1) == IRGraph::NullPtr)
        {
            // p == NULL / p != NULL: no interval obj to refine.
        }
        else
        {
            AbstractValue opVal[2] = {getAbsValue(cmpStmt->getOpVar(0), pred),
                                      getAbsValue(cmpStmt->getOpVar(1), pred)
                                     };

            const bool hasIntervalCmp =
                opVal[0].isInterval() && opVal[1].isInterval();
            if (!hasIntervalCmp && (opVal[0].isAddr() || opVal[1].isAddr()))
            {
                // Pointer-valued cmp: branch feasibility only.
            }
            else
            {
                for (int i = 0; i < 2; i++)
                {
                    const int other = 1 - i;
                    const LoadStmt* load =
                        findBackingLoad(cmpStmt->getOpVar(i));

                    if (opVal[i].getInterval().is_numeral())
                    {
                        // Example: in x < 5, operand 5 is not refined.
                    }
                    else if (!opVal[other].getInterval().is_numeral())
                    {
                        // Example: x < y, neither side has a fixed bound.
                    }
                    else if (!load)
                    {
                        // Example: cmp uses a computed temporary, not load p.
                    }
                    else
                    {
                        IntervalValue narrowed = computeCmpConstraint(
                                                     predicate, succ, i == 0, opVal[i].getInterval(),
                                                     opVal[other].getInterval());

                        if (narrowed.isTop())
                        {
                            // != and unsupported predicates reach here.
                        }
                        else
                        {
                            const ICFGNode* loadIcfg = load->getICFGNode();
                            const AbstractValue& ptrVal =
                                getAbsValue(load->getRHSVar(), loadIcfg);
                            if (!ptrVal.isAddr())
                            {
                                // Cannot map load p back to concrete ObjVars.
                            }
                            else
                            {
                                for (const auto& addr : ptrVal.getAddrs())
                                {
                                    NodeID objId = as.getIDFromAddr(addr);
                                    recordBranchRefinement(objId, narrowed, as,
                                                           loadIcfg, succNode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        const SVFVar* var = cond;

        AbstractValue condVal = getAbsValue(var, pred);
        IntervalValue switch_cond = condVal.getInterval();
        switch_cond.meet_with(IntervalValue(succ, succ));
        if (switch_cond.isBottom())
        {
            // This case label is not reachable from cond's interval.
        }
        else
        {
            as[var->getId()] = AbstractValue(switch_cond);

            FIFOWorkList<const SVFStmt*> stmtList;
            for (SVFStmt* stmt : var->getInEdges())
                stmtList.push(stmt);
            while (!stmtList.empty())
            {
                const SVFStmt* stmt = stmtList.pop();
                const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt);
                if (!load)
                {
                    // Skip non-load definitions of the switch condition.
                }
                else
                {
                    const ICFGNode* loadIcfg = load->getICFGNode();
                    const AbstractValue& ptrVal =
                        getAbsValue(load->getRHSVar(), loadIcfg);
                    if (!ptrVal.isAddr())
                    {
                        // Cannot map load p back to concrete ObjVars.
                    }
                    else
                    {
                        for (const auto& addr : ptrVal.getAddrs())
                        {
                            NodeID objId = as.getIDFromAddr(addr);
                            recordBranchRefinement(objId, switch_cond, as,
                                                   loadIcfg, succNode);
                        }
                    }
                }
            }
        }
    }
}

void AbstractInterpretation::recordBranchRefinement(
    NodeID objId, const IntervalValue& narrowed, AbstractState& as,
    const ICFGNode* loadIcfg, const ICFGNode* /*succ*/)
{
    // Default (dense / semi-sparse): MEET narrowed onto obj's current
    // value, store back into the local `as`.  Caller's joinStates
    // propagates `as` into `merged`, then `updateAbsState(succ, merged)`
    // commits it to trace[succ].
    //
    // We can't go through the polymorphic updateAbsValue here: `as` is
    // a transient per-edge predState copy that lives outside
    // abstractTrace, so it has no node id.  Writing via `updateAbsValue`
    // with `succ` as the node would land in trace[succ] but get
    // clobbered by the subsequent `updateAbsState(succ, merged)`; with
    // `loadIcfg` it would corrupt the obj's authoritative value at its
    // load site.  AbstractState::store on the transient `as` is the
    // only sound primitive — and recordBranchRefinement itself is the
    // virtual customisation point (FullSparse routes to
    // refinementTrace instead of touching `as`).
    const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objId));
    if (objVar && hasAbsValue(objVar, loadIcfg))
    {
        AbstractValue cur = getAbsValue(objVar, loadIcfg);
        if (cur.isInterval())
        {
            IntervalValue itv = cur.getInterval();
            itv.meet_with(narrowed);
            u32_t addr = AbstractState::getVirtualMemAddress(objId);
            as.store(addr, AbstractValue(itv));
        }
    }
}

bool AbstractInterpretation::isBranchEdgeFeasible(const IntraCFGEdge* edge,
        AbstractState& as)
{
    const SVFVar* cmpVar = edge->getCondition();
    // A branch condition with no defining edge (e.g. an undef/constant/argument
    // value never produced by a Cmp or switch statement) gives us nothing to
    // refine on. Conservatively treat the edge as feasible rather than aborting
    // (and rather than dereferencing an empty in-edge list): this keeps the
    // analysis sound by never pruning a potentially-reachable successor.
    if (cmpVar->getInEdges().empty())
        return true;
    if (SVFUtil::isa<CmpStmt>(*cmpVar->getInEdges().begin()))
        return isCmpBranchEdgeFeasible(edge, as);
    return isSwitchBranchEdgeFeasible(edge, as);
}

/**
 * Handle an ICFG node: execute statements on the current abstract state.
 * The node's pre-state must already be in getAbsState(node) (set by
 * mergeStatesFromPredecessors, or by handleGlobalNode for the global node).
 * Returns true if the abstract state has changed, false if fixpoint reached or unreachable.
 */
bool AbstractInterpretation::handleICFGNode(const ICFGNode* node)
{
    // Check reachability: pre-state must have been propagated by predecessors
    bool isFunEntry = SVFUtil::isa<FunEntryICFGNode>(node);
    if (!hasAbsState(node))
    {
        if (isFunEntry)
        {
            // Entry point with no callers: inherit from global node
            const ICFGNode* globalNode = icfg->getGlobalICFGNode();
            if (hasAbsState(globalNode))
                updateAbsState(node, getAbsState(globalNode));
            else
                updateAbsState(node, AbstractState());
        }
        else
        {
            return false;  // unreachable node
        }
    }

    spinActiveNode = node;
    ++spinGlobalNodeExecs;
    if (spinProbeLiveEnabled() && spinGlobalNodeExecs % spinProbeLiveEvery() == 0)
        dumpSpinProbeLive(node, "node-enter");
    if (spinGlobalNodeExecs % hotFuncEnvUL("SPINPROBE_NODE_DUMP_EVERY", 200000) == 0)
        dumpSpinProbe("node");

    const unsigned long stmtTotal = node->getSVFStmts().size();
    const SVFStmt* onlyStmt = nullptr;
    if (stmtTotal == 1)
        onlyStmt = *node->getSVFStmts().begin();
    const bool hotPhiOnlyNode =
        aeHotCyclePhiTopEnabled() &&
        spinActiveCycle != nullptr &&
        onlyStmt != nullptr &&
        SVFUtil::isa<PhiStmt>(onlyStmt) &&
        hotCycleThrottleFuns.find(node->getFun()) != hotCycleThrottleFuns.end();

    // The return value is not used by the current WTO driver; cycle fixpoint
    // checks happen at widen/narrow boundaries.  On large programs, avoiding
    // this full-state copy removes a major transient RSS spike.
    AbstractState prevState;
    const bool trackPrevState = !hotPhiOnlyNode && !aeSkipNodePrevState();
    if (trackPrevState)
        prevState = getAbsState(node);

    stat->getBlockTrace()++;
    stat->getICFGNodeTrace()++;

    const bool stmtProbe =
        spinProbeStmtEnabled() &&
        ((spinProbeStmtNode() != 0 && node->getId() == spinProbeStmtNode()) ||
         (spinProbeStmtNode() == 0 && stmtTotal >= spinProbeStmtMin()));
    dumpSpinProbeStmtProfile(node);

    const ExtMemHeavyInfo* extMemHeavyInfo = getExtMemHeavyInfo(node);

    if (extMemHeavyInfo)
    {
        recordExtMemStmtSkip(node, extMemHeavyInfo->stmtTotal,
                             extMemHeavyInfo->summaryKind.c_str());
    }
    else
    {
        // Handle SVF statements
        unsigned long stmtIndex = 0;
        for (const SVFStmt *stmt: node->getSVFStmts())
        {
            ++stmtIndex;
            if (stmtProbe &&
                (stmtIndex == 1 || stmtIndex == stmtTotal ||
                 stmtIndex % spinProbeStmtEvery() == 0))
                dumpSpinProbeStmtLive(node, stmtIndex, stmtTotal, spinProbeStmtKind(stmt));
            handleSVFStatement(stmt);
        }
    }

    if (hotPhiOnlyNode)
    {
        allAnalyzedNodes.insert(node);
        return true;
    }

    // Handle call sites
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callNode);
    }

    // Run detectors
    for (auto& detector: detectors)
        detector->detect(node);
    stat->countStateSize();

    // Track this node as analyzed (for coverage statistics across all entry points)
    allAnalyzedNodes.insert(node);

    if (trackPrevState && getAbsState(node) == prevState)
        return false;

    return true;
}

bool AbstractInterpretation::applyHotFunctionTop(const ICFGNode* funEntry,
        const CallICFGNode* caller)
{
    const bool forceTop = aeForceTopFunctionName(funEntry->getFun()->getName());
    if (!aeHotFunctionTopEnabled() && !forceTop)
        return false;
    if (!forceTop && funcBodyExecCount[funEntry] < aeHotFunctionTopThreshold())
        return false;

    const unsigned long minStmts = aeHotFunctionTopMinStmts();
    if (!forceTop && minStmts != 0)
    {
        auto costIt = funcStaticStmtCount.find(funEntry);
        if (costIt == funcStaticStmtCount.end())
        {
            unsigned long stmtCount = 0;
            for (const SVFBasicBlock* bb : funEntry->getFun()->getReachableBBs())
                for (const ICFGNode* node : bb->getICFGNodeList())
                    stmtCount += node->getSVFStmts().size();
            costIt = funcStaticStmtCount.emplace(funEntry, stmtCount).first;
        }
        if (costIt->second < minStmts)
        {
            ++funcSmallBodyTopBypassCount[funEntry];
            return false;
        }
    }

    if (caller != nullptr)
    {
        skipRecursionWithTop(caller);
        recordHotFunctionTop(funEntry, 0, 0);
        return true;
    }

    auto it = preAnalysis->getFuncToWTO().find(funEntry->getFun());
    if (it == preAnalysis->getFuncToWTO().end())
        return false;

    std::vector<const ICFGNode*> nodes;
    nodes.push_back(funEntry);
    collectWTONodes(it->second->getWTOComponents(), nodes);
    std::sort(nodes.begin(), nodes.end());
    nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());

    unsigned long varTops = 0;
    unsigned long locTops = 0;
    for (const ICFGNode* node : nodes)
    {
        auto stateIt = abstractTrace.find(node);
        if (stateIt == abstractTrace.end())
            continue;

        AbstractState& state = stateIt->second;
        std::vector<std::pair<u32_t, bool>> varIds;
        for (const auto& kv : state.getVarToVal())
        {
            if (kv.second.isInterval() || kv.second.isAddr())
                varIds.push_back(std::make_pair(kv.first, kv.second.isAddr()));
        }
        for (const auto& idAndAddr : varIds)
        {
            AbstractValue topValue(IntervalValue::top());
            if (idAndAddr.second)
                topValue.getAddrs().insert(BlackHoleObjAddr);
            state[idAndAddr.first] = topValue;
            ++varTops;
        }

        std::vector<std::pair<u32_t, bool>> locIds;
        for (const auto& kv : state.getLocToVal())
        {
            if (kv.second.isInterval() || kv.second.isAddr())
                locIds.push_back(std::make_pair(kv.first, kv.second.isAddr()));
        }
        for (const auto& idAndAddr : locIds)
        {
            AbstractValue topValue(IntervalValue::top());
            if (idAndAddr.second)
                topValue.getAddrs().insert(BlackHoleObjAddr);
            state.store(AbstractState::getVirtualMemAddress(idAndAddr.first),
                        topValue);
            ++locTops;
        }
    }

    recordHotFunctionTop(funEntry, varTops, locTops);
    return true;
}

bool AbstractInterpretation::entryNodeBudgetExceeded() const
{
    const unsigned long budget = aeEntryNodeBudget();
    if (budget == 0)
        return false;
    if (currentEntryIndex <= aeEntryBudgetAfter())
        return false;
    return spinGlobalNodeExecs - currentEntryStartNodeExecs >= budget;
}

void AbstractInterpretation::dumpEntryBudgetStats(const char* reason) const
{
    const unsigned long totalHits = entryBudgetFunctionTopHits + entryBudgetCycleTopHits;
    std::ofstream of(aeTmpPath("svf_entry_budget_top.tsv"));
    of << "# reason=" << reason
       << "\tbudget=" << aeEntryNodeBudget()
       << "\tafter=" << aeEntryBudgetAfter()
       << "\tcurrent_entry=" << currentEntryIndex
       << "\tentry_node_execs=" << (spinGlobalNodeExecs - currentEntryStartNodeExecs)
       << "\tfunction_top_hits=" << entryBudgetFunctionTopHits
       << "\tcycle_top_hits=" << entryBudgetCycleTopHits
       << "\ttotal_hits=" << totalHits
       << "\n";
    of << "kind\tfunction\thits\n";
    for (const auto& kv : entryBudgetFunctionTopByFunction)
        of << "function\t" << kv.first << "\t" << kv.second << "\n";
    for (const auto& kv : entryBudgetCycleTopByFunction)
        of << "cycle\t" << kv.first << "\t" << kv.second << "\n";
}

bool AbstractInterpretation::applyEntryBudgetFunctionTop(const ICFGNode* funEntry,
        const CallICFGNode* caller)
{
    if (caller == nullptr || !entryNodeBudgetExceeded())
        return false;

    skipRecursionWithTop(caller);
    ++entryBudgetFunctionTopHits;
    ++entryBudgetFunctionTopByFunction[funEntry->getFun()->getName()];
    const unsigned long every = aeEntryBudgetDumpEvery();
    if (entryBudgetFunctionTopHits == 1 || entryBudgetFunctionTopHits % every == 0)
        dumpEntryBudgetStats("function");
    return true;
}

bool AbstractInterpretation::applyEntryBudgetCycleTop(const ICFGCycleWTO* cycle)
{
    if (!entryNodeBudgetExceeded())
        return false;
    if (!applyHotCycleThrottle(cycle, true))
        return false;

    const ICFGNode* head = cycle->head()->getICFGNode();
    ++entryBudgetCycleTopHits;
    ++entryBudgetCycleTopByFunction[head->getFun()->getName()];
    const unsigned long every = aeEntryBudgetDumpEvery();
    if (entryBudgetCycleTopHits == 1 || entryBudgetCycleTopHits % every == 0)
        dumpEntryBudgetStats("cycle");
    return true;
}

/**
 * Handle a function using worklist algorithm guided by WTO order.
 * All top-level WTO components are pushed into the worklist upfront,
 * so the traversal order is exactly the WTO order — each node is
 * visited once, and cycles are handled as whole components.
 */
void AbstractInterpretation::handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller)
{
    static const bool memoOff = (std::getenv("MEMO_OFF") != nullptr);
    ++funcCallTotal;
    ++funcCallCount[funEntry];
    if (funcCallTotal == 1 || funcCallTotal % hotFuncDumpEvery() == 0)
        dumpHotFunctionStats();
    auto it = preAnalysis->getFuncToWTO().find(funEntry->getFun());
    if (it == preAnalysis->getFuncToWTO().end())
    {
        if (!aeAllowMissingState())
            assert(false && "Missing WTO for function");

        static unsigned long missingWTO = 0;
        ++missingWTO;
        if (missingWTO <= 20 || missingWTO % 1000 == 0)
        {
            SVFUtil::errs() << "[AE-MISSING-WTO] skip function count=" << missingWTO
                            << " fun=" << funEntry->getFun()->getName()
                            << (caller ? " from_call=1" : " from_call=0")
                            << "\n";
        }

        if (caller != nullptr && hasAbsState(caller))
        {
            AbstractState callerState = getAbsState(caller);

            std::vector<std::pair<u32_t, bool>> varIds;
            for (const auto& kv : callerState.getVarToVal())
                varIds.push_back(std::make_pair(kv.first, kv.second.isAddr()));
            for (const auto& idAndAddr : varIds)
            {
                AbstractValue topValue(IntervalValue::top());
                if (idAndAddr.second)
                    topValue.getAddrs().insert(BlackHoleObjAddr);
                callerState[idAndAddr.first] = topValue;
            }

            std::vector<std::pair<u32_t, bool>> locIds;
            for (const auto& kv : callerState.getLocToVal())
                locIds.push_back(std::make_pair(kv.first, kv.second.isAddr()));
            for (const auto& idAndAddr : locIds)
            {
                AbstractValue topValue(IntervalValue::top());
                if (idAndAddr.second)
                    topValue.getAddrs().insert(BlackHoleObjAddr);
                callerState.store(AbstractState::getVirtualMemAddress(idAndAddr.first),
                                  topValue);
            }

            updateAbsState(caller, callerState);
            if (const RetICFGNode* retNode = caller->getRetICFGNode())
                updateAbsState(retNode, callerState);
        }
        return;
    }

    if (applyEntryBudgetFunctionTop(funEntry, caller))
        return;

    if (applyHotFunctionTop(funEntry, caller))
    {
        ++funcMemoHits;
        ++funcFastHitCount[funEntry];
        return;
    }

    // Semi-sparse ValVars live at their def-sites, so the formal parameters at
    // funEntry still contain the previous invocation until CallPE executes.
    // Materialize the active caller and its actual arguments before taking the
    // memoization snapshot; otherwise two different calls can share a stale key
    // and incorrectly skip the callee body.
    if (aeCallsiteSensitivePE() && caller != nullptr && hasAbsState(caller))
    {
        updateAbsState(funEntry, getAbsState(caller));
        for (const SVFStmt* stmt : funEntry->getSVFStmts())
            if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
                updateStateOnCall(callPE);
    }

    // EXPERIMENT (function memoization, cheap full-state key): if this function's
    // entry state is identical to its previous invocation, re-analyzing the body
    // would reproduce the same result, so skip it (shared/persistent trace[] from
    // last time still holds the body effect). Keyed by FunEntry node.
    ++funcMemoTotal;
    AbstractState fSnap;
    if (hasAbsState(funEntry))
        fSnap = getAbsState(funEntry);
    auto fmit = funcInputCache.find(funEntry);

    if (valueProbeEnabled())
    {
        const std::string& funName = funEntry->getFun()->getName();
        if (valueProbeHotFunction(funName))
        {
            ValueProbeStats& s = valueProbeStats[funName];
            ++s.calls;
            s.lastVarN = fSnap.getVarToVal().size();
            s.lastLocN = fSnap.getLocToVal().size();

            bool hadPrev = false;
            bool fullSame = false;
            unsigned long varChanged = 0, varAddedRemoved = 0;
            unsigned long locChanged = 0, locAddedRemoved = 0;
            auto pit = valueProbePrevInput.find(funEntry);
            if (pit != valueProbePrevInput.end())
            {
                hadPrev = true;
                ++s.compared;
                fullSame = (pit->second == fSnap);
                if (fullSame)
                    ++s.fullSame;

                const bool shapeSame =
                    pit->second.getVarToVal().size() == fSnap.getVarToVal().size() &&
                    pit->second.getLocToVal().size() == fSnap.getLocToVal().size();
                if (shapeSame)
                    ++s.shapeSame;

                valueProbeDiffMap(pit->second.getVarToVal(), fSnap.getVarToVal(),
                                  varChanged, varAddedRemoved);
                valueProbeDiffMap(pit->second.getLocToVal(), fSnap.getLocToVal(),
                                  locChanged, locAddedRemoved);
                s.varValueChanged += varChanged;
                s.varAddedRemoved += varAddedRemoved;
                s.locValueChanged += locChanged;
                s.locAddedRemoved += locAddedRemoved;
                if (shapeSame && !fullSame)
                    ++s.shapeSameButValueChanged;
            }

            unsigned long readSetSize = 0;
            unsigned long staleFields = 0;
            auto rvit = funcReadVersions.find(funEntry);
            if (rvit != funcReadVersions.end())
            {
                readSetSize = rvit->second.size();
                for (const auto& kv : rvit->second)
                    if (gepFieldVersion[kv.first] != kv.second)
                        ++staleFields;
                s.overlayReadSetMax = std::max(s.overlayReadSetMax, readSetSize);
                if (staleFields)
                {
                    ++s.overlayStaleCalls;
                    s.overlayStaleFields += staleFields;
                    if (hadPrev && fullSame)
                        ++s.sameStateButOverlayStale;
                }
            }

            valueProbePrevInput[funEntry] = fSnap;
            ++valueProbeHotCalls;
            if (s.calls <= 20 || s.calls % 500 == 0)
                valueProbeAppendDetail(funName, funcCallTotal, s.calls, hadPrev,
                                       fullSame, s.lastVarN, s.lastLocN,
                                       varChanged, varAddedRemoved,
                                       locChanged, locAddedRemoved,
                                       readSetSize, staleFields);
            if (valueProbeHotCalls % 1000UL == 0)
                valueProbeDumpSummary();
        }
    }

    const bool legacyExact = !memoOff && hotFuncLegacyMode();
    const bool hotExact = !memoOff && hotFuncExactMode();
    const bool isHot = funcBodyExecCount[funEntry] >= hotFuncThreshold();
    const bool mayReplay = legacyExact || (hotExact && isHot);

    if (hotExact && isHot)
        ++funcHotAttemptCount[funEntry];

    if (mayReplay && fmit != funcInputCache.end())
    {
        if (fmit->second == fSnap)
        {
            // Sound per-field gate: reuse only if every gepOverlay field this
            // function read last time is unchanged (dynamic-dependency replay).
            bool fresh = true;
            const auto& rv = funcReadVersions[funEntry];
            for (const auto& kv : rv)
                if (gepFieldVersion[kv.first] != kv.second) { fresh = false; break; }
            if (fresh)
            {
                ++funcMemoHits;
                ++funcFastHitCount[funEntry];
                if (!gepReadStack.empty())
                    for (const auto& kv : rv)
                        gepReadStack.back().insert(kv.first);
                return;
            }
            if (hotExact && isHot)
                ++funcHotOverlayStaleCount[funEntry];
        }
        else if (hotExact && isHot)
            ++funcHotStateDiffCount[funEntry];
    }
    else if (hotExact && isHot)
        ++funcHotNoCacheCount[funEntry];

    ++funcBodyExecTotal;
    ++funcBodyExecCount[funEntry];
    funcInputCache[funEntry] = fSnap;
    gepReadStack.push_back(Set<NodeID>());

    // Push all top-level WTO components into the worklist in WTO order
    FIFOWorkList<const ICFGWTOComp*> worklist(it->second->getWTOComponents());

    while (!worklist.empty())
    {
        const ICFGWTOComp* comp = worklist.pop();

        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            const ICFGNode* node = singleton->getICFGNode();
            if (mergeStatesFromPredecessors(node))
                handleICFGNode(node);
        }
        else if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            if (mergeStatesFromPredecessors(cycle->head()->getICFGNode()))
                handleLoopOrRecursion(cycle, caller);
        }
    }

    // record this function's gepOverlay read-set and each read field's version
    Set<NodeID> myReads = gepReadStack.back();
    gepReadStack.pop_back();
    Map<NodeID, unsigned long>& rv = funcReadVersions[funEntry];
    rv.clear();
    for (NodeID id : myReads)
        rv[id] = gepFieldVersion[id];
    if (!gepReadStack.empty())
        for (NodeID id : myReads)
            gepReadStack.back().insert(id);
}


void AbstractInterpretation::handleCallSite(const ICFGNode* node)
{
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        if (isExtCall(callNode))
        {
            handleExtCall(callNode);
        }
        else
        {
            // Handle both direct and indirect calls uniformly
            handleFunCall(callNode);
        }
    }
    else
        assert (false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode *callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::handleExtCall(const CallICFGNode *callNode)
{
    utils->handleExtAPI(callNode);
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode);
    }
}

/// Get callee function: directly for direct calls, via pointer analysis for indirect calls
const FunObjVar* AbstractInterpretation::getCallee(const CallICFGNode* callNode)
{
    // Direct call: get callee directly from call node
    if (const FunObjVar* callee = callNode->getCalledFunction())
        return callee;

    // Indirect call: resolve callee through pointer analysis
    const auto callsiteMaps = svfir->getIndirectCallsites();
    auto it = callsiteMaps.find(callNode);
    if (it == callsiteMaps.end())
        return nullptr;

    NodeID call_id = it->second;
    if (!hasAbsState(callNode))
        return nullptr;

    const AbstractValue& Addrs = getAbsValue(svfir->getSVFVar(call_id), callNode);
    if (!Addrs.isAddr() || Addrs.getAddrs().empty())
        return nullptr;

    NodeID addr = *Addrs.getAddrs().begin();
    const SVFVar* func_var = getSVFVar(getAbsState(callNode).getIDFromAddr(addr));
    return SVFUtil::dyn_cast<FunObjVar>(func_var);
}

/// Handle direct or indirect call: get callee(s), process function body, set return state.
///
/// For direct calls, the callee is known statically.
/// For indirect calls, the previous implementation resolved callees from the abstract
/// state's address domain, which only picked the first address and missed other targets.
/// Since the abstract state's address domain is not an over-approximation for function
/// pointers (it may be uninitialized or incomplete), we now use Andersen's pointer
/// analysis results from the pre-computed call graph, which soundly resolves all
/// possible indirect call targets.
void AbstractInterpretation::handleFunCall(const CallICFGNode *callNode)
{
    if (skipRecursiveCall(callNode))
        return;

    // Direct call: callee is known
    if (const FunObjVar* callee = callNode->getCalledFunction())
    {
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        reportCallStack.push_back(callNode);
        handleFunction(calleeEntry, callNode);
        reportCallStack.pop_back();
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        updateAbsState(retNode, getAbsState(callNode));
        return;
    }

    // Indirect call: use Andersen's call graph to get all resolved callees.
    const RetICFGNode* retNode = callNode->getRetICFGNode();
    if (callGraph->hasIndCSCallees(callNode))
    {
        const auto& callees = callGraph->getIndCSCallees(callNode);
        for (const FunObjVar* callee : callees)
        {
            if (callee->isDeclaration())
                continue;
            const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
            reportCallStack.push_back(callNode);
            handleFunction(calleeEntry, callNode);
            reportCallStack.pop_back();
        }
    }
    // Resume return node from caller's state (context-insensitive)
    updateAbsState(retNode, getAbsState(callNode));
}

// Loop / recursion handling (handleLoopOrRecursion + cycle helpers +
// recursion utilities) lives in AELoopRecursion.cpp.

void AbstractInterpretation::handleSVFStatement(const SVFStmt *stmt)
{
    if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
    {
        updateStateOnAddr(addr);
    }
    else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
    {
        updateStateOnBinary(binary);
    }
    else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
    {
        updateStateOnCmp(cmp);
    }
    else if (SVFUtil::isa<UnaryOPStmt>(stmt))
    {
    }
    else if (SVFUtil::isa<BranchStmt>(stmt))
    {
        // branch stmt is handled in hasBranchES
    }
    else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
    {
        updateStateOnLoad(load);
    }
    else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        updateStateOnStore(store);
    }
    else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
    {
        updateStateOnCopy(copy);
    }
    else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        updateStateOnGep(gep);
    }
    else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt))
    {
        updateStateOnSelect(select);
    }
    else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
    {
        updateStateOnPhi(phi);
    }
    else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
    {
        // To handle Call Edge
        updateStateOnCall(callPE);
    }
    else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
    {
        updateStateOnRet(retPE);
    }
    else
        assert(false && "implement this part");
    // NullPtr should not be changed by any statement. If the entry is missing
    // (not yet auto-inserted) we treat that as "unchanged" — only check the
    // entry if it actually exists.
    {
        const auto& vmap = getAbsState(stmt->getICFGNode()).getVarToVal();
        auto it = vmap.find(IRGraph::NullPtr);
        (void)it; // Suppress warning of unused variable under release build
        if (!aeAllowMissingState())
        {
            assert(it == vmap.end() ||
                   (!it->second.isInterval() && !it->second.isAddr()));
        }
    }
}

void AbstractInterpretation::updateStateOnGep(const GepStmt *gep)
{
    const ICFGNode* node = gep->getICFGNode();
    IntervalValue offsetPair = getGepElementIndex(gep);
    AddressValue gepAddrs = getGepObjAddrs(SVFUtil::cast<ValVar>(gep->getRHSVar()), offsetPair);
    updateAbsValue(gep->getLHSVar(), gepAddrs, node);
}

void AbstractInterpretation::updateStateOnSelect(const SelectStmt *select)
{
    const ICFGNode* node = select->getICFGNode();
    const AbstractValue& condVal = getAbsValue(select->getCondition(), node);
    const AbstractValue& tVal = getAbsValue(select->getTrueValue(), node);
    const AbstractValue& fVal = getAbsValue(select->getFalseValue(), node);
    AbstractValue resVal;
    if (condVal.getInterval().is_numeral())
    {
        resVal = condVal.getInterval().is_zero() ? fVal : tVal;
    }
    else
    {
        resVal = tVal;
        resVal.join_with(fVal);
    }
    updateAbsValue(select->getRes(), resVal, node);
}

void AbstractInterpretation::updateStateOnPhi(const PhiStmt *phi)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    if (aeHotCyclePhiTopEnabled() && spinActiveCycle != nullptr &&
            hotCycleThrottleFuns.find(icfgNode->getFun()) != hotCycleThrottleFuns.end())
    {
        const SVFVar* res = phi->getRes();
        AbstractValue topValue(IntervalValue::top());
        if (res->isPointer())
            topValue.getAddrs().insert(BlackHoleObjAddr);
        updateAbsValue(res, topValue, icfgNode);
        recordHotCyclePhiTop(icfgNode);
        return;
    }

    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        if (hasAbsState(opICFGNode))
        {
            AbstractState tmpState = getAbsState(opICFGNode);
            const AbstractValue& opVal = getAbsValue(phi->getOpVar(i), opICFGNode);
            const ICFGEdge* edge = icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);
            if (edge)
            {
                const IntraCFGEdge* intraEdge = SVFUtil::cast<IntraCFGEdge>(edge);
                if (intraEdge->getCondition())
                {
                    if (isBranchEdgeFeasible(intraEdge, tmpState))
                        rhs.join_with(opVal);
                }
                else
                    rhs.join_with(opVal);
            }
            else
            {
                rhs.join_with(opVal);
            }
        }
    }
    updateAbsValue(phi->getRes(), rhs, icfgNode);
}


/// Handle CallPE: phi-like merging of actual parameters from all call sites
/// into the formal parameter at FunEntryICFGNode (e.g., formal = join(actual1@cs1, actual2@cs2, ...))
void AbstractInterpretation::updateStateOnCall(const CallPE *callPE)
{
    const ICFGNode* node = callPE->getICFGNode();
    const SVFVar* res = callPE->getRes();
    AbstractValue rhs;

    const CallICFGNode* activeCall = nullptr;
    if (aeCallsiteSensitivePE() && !reportCallStack.empty())
        activeCall = reportCallStack.back();

    bool matchedActiveCall = false;
    for (u32_t i = 0; i < callPE->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = callPE->getOpCallICFGNode(i);
        if (activeCall && opICFGNode != activeCall)
            continue;
        if (hasAbsState(opICFGNode))
        {
            const AbstractValue& opVal = getAbsValue(callPE->getOpVar(i), opICFGNode);
            rhs.join_with(opVal);
            matchedActiveCall = true;
        }
    }

    // An indirect/thread edge may not have an operand tagged with the dynamic
    // callsite. Fall back to the context-insensitive join instead of dropping
    // a possible actual argument.
    if (activeCall && !matchedActiveCall)
    {
        for (u32_t i = 0; i < callPE->getOpVarNum(); i++)
        {
            const ICFGNode* opICFGNode = callPE->getOpCallICFGNode(i);
            if (hasAbsState(opICFGNode))
                rhs.join_with(getAbsValue(callPE->getOpVar(i), opICFGNode));
        }
    }
    updateAbsValue(res, rhs, node);
}

void AbstractInterpretation::updateStateOnRet(const RetPE *retPE)
{
    // RetPE is already attached to the RetICFGNode of one concrete callsite.
    // The dynamic call has been popped by the time that node executes, so
    // filtering against reportCallStack would compare with the outer caller and
    // could drop a real return value.
    const ICFGNode* node = retPE->getICFGNode();
    const AbstractValue& rhsVal = getAbsValue(retPE->getRHSVar(), node);
    updateAbsValue(retPE->getLHSVar(), rhsVal, node);
}


void AbstractInterpretation::updateStateOnAddr(const AddrStmt *addr)
{
    const ICFGNode* node = addr->getICFGNode();
    // initObjVar mutates _varToAbsVal/_addrToAbsVal directly, so we need
    // mutable access; route via the manager.
    AbstractState& as = getAbsState(node);
    as.initObjVar(SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    (void)getAllocaInstByteSizeInterval(addr);
    // AddrStmt: lhs(ValVar) = &rhs(ObjVar).
    // as[rhsId] stores the ObjVar's virtual address in _varToVal,
    // NOT the object contents. So we must use as[] directly for ObjVar.
    u32_t rhsId = addr->getRHSVarID();
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[rhsId].getInterval().meet_with(utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    // LHS is a ValVar (pointer), write through the API
    updateAbsValue(addr->getLHSVar(), as[rhsId], node);
}


void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt *binary)
{
    const ICFGNode* node = binary->getICFGNode();
    // Treat bottom (uninitialized) operands as top for soundness
    const AbstractValue& op0Val = getAbsValue(binary->getOpVar(0), node);
    const AbstractValue& op1Val = getAbsValue(binary->getOpVar(1), node);
    IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval();
    IntervalValue rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
    IntervalValue resVal;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        resVal = (lhs + rhs);
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        resVal = (lhs - rhs);
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        resVal = (lhs * rhs);
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        resVal = (lhs / rhs);
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        resVal = (lhs % rhs);
        break;
    case BinaryOPStmt::Xor:
        resVal = (lhs ^ rhs);
        break;
    case BinaryOPStmt::And:
        resVal = (lhs & rhs);
        break;
    case BinaryOPStmt::Or:
        resVal = (lhs | rhs);
        break;
    case BinaryOPStmt::AShr:
        resVal = (lhs >> rhs);
        break;
    case BinaryOPStmt::Shl:
        resVal = (lhs << rhs);
        break;
    case BinaryOPStmt::LShr:
        resVal = (lhs >> rhs);
        break;
    default:
        assert(false && "undefined binary: ");
    }
    updateAbsValue(binary->getRes(), resVal, node);
}

static bool aeCmpPredicateIsEq(u32_t predicate)
{
    return predicate == CmpStmt::ICMP_EQ ||
           predicate == CmpStmt::FCMP_OEQ ||
           predicate == CmpStmt::FCMP_UEQ;
}

static bool aeCmpPredicateIsNe(u32_t predicate)
{
    return predicate == CmpStmt::ICMP_NE ||
           predicate == CmpStmt::FCMP_ONE ||
           predicate == CmpStmt::FCMP_UNE;
}

static bool aeCmpPredicateIsAlwaysFalse(u32_t predicate)
{
    return predicate == CmpStmt::FCMP_FALSE;
}

static bool aeCmpPredicateIsAlwaysTrue(u32_t predicate)
{
    return predicate == CmpStmt::FCMP_TRUE;
}

static bool aeIntervalIsZero(const AbstractValue& val)
{
    return val.isInterval() &&
           val.getInterval().equals(IntervalValue((s64_t)0, (s64_t)0));
}

static bool aeOperandIsNullPtr(u32_t varId, const AbstractValue& val)
{
    if (varId == IRGraph::NullPtr || aeIntervalIsZero(val))
        return true;

    if (!val.isAddr())
        return false;

    const AddressValue addrs = val.getAddrs();
    return addrs.size() == 1 && addrs.contains(NullMemAddr);
}

static bool aeAddressSetHasUnknownOrNull(const AddressValue& addrs)
{
    return addrs.empty() || addrs.contains(BlackHoleObjAddr) ||
           addrs.contains(NullMemAddr);
}

static bool aeOperandDefinitelyNonNullAddr(const AbstractValue& val)
{
    return val.isAddr() && !aeAddressSetHasUnknownOrNull(val.getAddrs());
}

static bool aeKnownSingletonSameAddr(const AddressValue& lhs,
                                     const AddressValue& rhs)
{
    if (aeAddressSetHasUnknownOrNull(lhs) || aeAddressSetHasUnknownOrNull(rhs))
        return false;
    return lhs.size() == 1 && rhs.size() == 1 && lhs.equals(rhs);
}

static bool aeKnownDisjointAddrs(const AddressValue& lhs,
                                 const AddressValue& rhs)
{
    if (aeAddressSetHasUnknownOrNull(lhs) || aeAddressSetHasUnknownOrNull(rhs))
        return false;
    return !lhs.hasIntersect(rhs);
}

static IntervalValue aeConservativePointerCmpResult(
    u32_t predicate, u32_t op0, const AbstractValue& op0Val,
    u32_t op1, const AbstractValue& op1Val)
{
    if (aeCmpPredicateIsAlwaysFalse(predicate))
        return IntervalValue((s64_t)0, (s64_t)0);
    if (aeCmpPredicateIsAlwaysTrue(predicate))
        return IntervalValue((s64_t)1, (s64_t)1);

    if (!aeCmpPredicateIsEq(predicate) && !aeCmpPredicateIsNe(predicate))
        return IntervalValue((s64_t)0, (s64_t)1);

    const bool isEq = aeCmpPredicateIsEq(predicate);
    const bool lhsNull = aeOperandIsNullPtr(op0, op0Val);
    const bool rhsNull = aeOperandIsNullPtr(op1, op1Val);

    if (lhsNull && rhsNull)
        return isEq ? IntervalValue((s64_t)1, (s64_t)1) :
               IntervalValue((s64_t)0, (s64_t)0);

    if (lhsNull || rhsNull)
    {
        const AbstractValue& other = lhsNull ? op1Val : op0Val;
        if (aeOperandDefinitelyNonNullAddr(other))
            return isEq ? IntervalValue((s64_t)0, (s64_t)0) :
                   IntervalValue((s64_t)1, (s64_t)1);
        return IntervalValue((s64_t)0, (s64_t)1);
    }

    if (op0Val.isAddr() && op1Val.isAddr())
    {
        const AddressValue lhs = op0Val.getAddrs();
        const AddressValue rhs = op1Val.getAddrs();
        if (aeKnownSingletonSameAddr(lhs, rhs))
            return isEq ? IntervalValue((s64_t)1, (s64_t)1) :
                   IntervalValue((s64_t)0, (s64_t)0);
        if (aeKnownDisjointAddrs(lhs, rhs))
            return isEq ? IntervalValue((s64_t)0, (s64_t)0) :
                   IntervalValue((s64_t)1, (s64_t)1);
    }

    return IntervalValue((s64_t)0, (s64_t)1);
}

void AbstractInterpretation::updateStateOnCmp(const CmpStmt *cmp)
{
    const ICFGNode* node = cmp->getICFGNode();
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    const AbstractValue& op0Val = getAbsValue(cmp->getOpVar(0), node);
    const AbstractValue& op1Val = getAbsValue(cmp->getOpVar(1), node);

    const bool pointerLikeCmp = op0 == IRGraph::NullPtr ||
                                op1 == IRGraph::NullPtr ||
                                op0Val.isAddr() || op1Val.isAddr();
    if (pointerLikeCmp)
    {
        IntervalValue resVal = aeConservativePointerCmpResult(
                                   cmp->getPredicate(), op0, op0Val, op1, op1Val);
        updateAbsValue(cmp->getRes(), resVal, node);
        return;
    }

    IntervalValue resVal;
    if (op0Val.isInterval() && op1Val.isInterval())
    {
        // Treat bottom (uninitialized) operands as top for soundness.
        IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval();
        IntervalValue rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            resVal = (lhs == rhs);
            break;
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
            resVal = (lhs != rhs);
            break;
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
            resVal = (lhs > rhs);
            break;
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
            resVal = (lhs >= rhs);
            break;
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
            resVal = (lhs < rhs);
            break;
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
            resVal = (lhs <= rhs);
            break;
        case CmpStmt::FCMP_FALSE:
            resVal = IntervalValue((s64_t)0, (s64_t)0);
            break;
        case CmpStmt::FCMP_TRUE:
            resVal = IntervalValue((s64_t)1, (s64_t)1);
            break;
        case CmpStmt::FCMP_ORD:
        case CmpStmt::FCMP_UNO:
            // FCMP_ORD/UNO depends on NaN, which this domain does not track.
            resVal = IntervalValue((s64_t)0, (s64_t)1);
            break;
        default:
            assert(false && "undefined compare: ");
        }
        updateAbsValue(cmp->getRes(), resVal, node);
    }
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt *load)
{
    const ICFGNode* node = load->getICFGNode();
    AbstractValue loaded =
        loadValue(SVFUtil::cast<ValVar>(load->getRHSVar()), node);
    updateAbsValue(load->getLHSVar(), loaded, node);
}

void AbstractInterpretation::updateStateOnStore(const StoreStmt *store)
{
    const ICFGNode* node = store->getICFGNode();
    AbstractValue val = getAbsValue(store->getRHSVar(), node);
    storeValue(SVFUtil::cast<ValVar>(store->getLHSVar()), val, node);
}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt *copy)
{
    const ICFGNode* node = copy->getICFGNode();
    const SVFVar* lhsVar = copy->getLHSVar();
    const SVFVar* rhsVar = copy->getRHSVar();

    auto getZExtValue = [&](const SVFVar* var)
    {
        const SVFType* type = var->getType();
        if (SVFUtil::isa<SVFIntegerType>(type))
        {
            u32_t bits = type->getByteSize() * 8;
            const AbstractValue& val = getAbsValue(var, node);
            if (val.getInterval().is_numeral())
            {
                if (bits == 8)
                {
                    int8_t signed_i8_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<uint8_t>(signed_i8_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 16)
                {
                    s16_t signed_i16_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u16_t>(signed_i16_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 32)
                {
                    s32_t signed_i32_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u32_t>(signed_i32_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 64)
                {
                    s64_t signed_i64_value = val.getInterval().getIntNumeral();
                    return IntervalValue((s64_t)signed_i64_value, (s64_t)signed_i64_value);
                }
                else
                {
                    return IntervalValue::top();
                }
            }
            else
            {
                return IntervalValue::top();
            }
        }
        return IntervalValue::top();
    };

    auto getTruncValue = [&](const SVFVar* var, const SVFType* dstType)
    {
        const IntervalValue& itv = getAbsValue(var, node).getInterval();
        if(itv.isBottom()) return itv;
        s64_t int_lb = itv.lb().getIntNumeral();
        s64_t int_ub = itv.ub().getIntNumeral();
        u32_t dst_bits = dstType->getByteSize() * 8;
        if (dst_bits == 8)
        {
            int8_t s8_lb = static_cast<int8_t>(int_lb);
            int8_t s8_ub = static_cast<int8_t>(int_ub);
            if (s8_lb > s8_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s8_lb, s8_ub);
        }
        else if (dst_bits == 16)
        {
            s16_t s16_lb = static_cast<s16_t>(int_lb);
            s16_t s16_ub = static_cast<s16_t>(int_ub);
            if (s16_lb > s16_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s16_lb, s16_ub);
        }
        else if (dst_bits == 32)
        {
            s32_t s32_lb = static_cast<s32_t>(int_lb);
            s32_t s32_ub = static_cast<s32_t>(int_ub);
            if (s32_lb > s32_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s32_lb, s32_ub);
        }
        else if (dst_bits == 64)
        {
            return IntervalValue(int_lb, int_ub);
        }
        else
        {
            return utils->getRangeLimitFromType(dstType);
        }
    };

    const AbstractValue& rhsVal = getAbsValue(rhsVar, node);

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        updateAbsValue(lhsVar, rhsVal, node);
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        updateAbsValue(lhsVar, getZExtValue(rhsVar), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        updateAbsValue(lhsVar, getTruncValue(rhsVar, lhsVar->getType()), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        //insert nullptr
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        updateAbsValue(lhsVar, IntervalValue::top(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (rhsVal.isAddr())
            updateAbsValue(lhsVar, rhsVal, node);
    }
    else
        assert(false && "undefined copy kind");
}
