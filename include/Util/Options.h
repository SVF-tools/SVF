//===- Options.h -- Command line options ------------------------//

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "Util/NodeIDAllocator.h"
#include <sstream>
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAPass.h"

namespace SVF
{

/// Carries around command line options.
class Options
{
public:
    Options(void) = delete;

    /// If set, only return the clock when getClk is called as getClk(true).
    /// Retrieving the clock is slow but it should be fine for a few calls.
    /// This is good for benchmarking when we don't need to know how long processLoad
    /// takes, for example (many calls), but want to know things like total solve time.
    /// Should be used only to affect getClk, not CLOCK_IN_MS.
    static const llvm::cl::opt<bool> MarkedClocksOnly;

    /// Allocation strategy to be used by the node ID allocator.
    /// Currently dense, seq, or debug.
    static const llvm::cl::opt<SVF::NodeIDAllocator::Strategy> NodeAllocStrat;

    /// Maximum number of field derivations for an object.
    static const llvm::cl::opt<unsigned> MaxFieldLimit;

    // ContextDDA.cpp
    static const llvm::cl::opt<unsigned long long> CxtBudget;

    // DDAClient.cpp
    static const llvm::cl::opt<bool> SingleLoad;
    static const llvm::cl::opt<bool> DumpFree;
    static const llvm::cl::opt<bool> DumpUninitVar;
    static const llvm::cl::opt<bool> DumpUninitPtr;
    static const llvm::cl::opt<bool> DumpSUPts;
    static const llvm::cl::opt<bool> DumpSUStore;
    static const llvm::cl::opt<bool> MallocOnly;
    static const llvm::cl::opt<bool> TaintUninitHeap;
    static const llvm::cl::opt<bool> TaintUninitStack;

    // DDAPass.cpp
    static const llvm::cl::opt<unsigned> MaxPathLen;
    static const llvm::cl::opt<unsigned> MaxContextLen;
    static const llvm::cl::opt<std :: string> UserInputQuery;
    static const llvm::cl::opt<bool> InsenRecur;
    static const llvm::cl::opt<bool> InsenCycle;
    static const llvm::cl::opt<bool> PrintCPts;
    static const llvm::cl::opt<bool> PrintQueryPts;
    static const llvm::cl::opt<bool> WPANum;
    static llvm::cl::bits<PointerAnalysis::PTATY> DDASelected;

    // FlowDDA.cpp
    static const llvm::cl::opt<unsigned long long> FlowBudget;

    // Offline constraint graph (OfflineConsG.cpp)
    static const llvm::cl::opt<bool> OCGDotGraph;

    // Program Assignment Graph for pointer analysis (PAG.cpp)
    static llvm::cl::opt<bool> HandBlackHole;
    static const llvm::cl::opt<bool> FirstFieldEqBase;

    // SVFG optimizer (SVFGOPT.cpp)
    static const llvm::cl::opt<bool> ContextInsensitive;
    static const llvm::cl::opt<bool> KeepAOFI;
    static const llvm::cl::opt<std::string> SelfCycle;

    // Sparse value-flow graph (VFG.cpp)
    static const llvm::cl::opt<bool> DumpVFG;

     // Location set for modeling abstract memory object (LocationSet.cpp)
    static const llvm::cl::opt<bool> SingleStride;

    // Base class of pointer analyses (PointerAnalysis.cpp)
    static const llvm::cl::opt<bool> TypePrint;
    static const llvm::cl::opt<bool> FuncPointerPrint;
    static const llvm::cl::opt<bool> PTSPrint;
    static const llvm::cl::opt<bool> PTSAllPrint;
    static const llvm::cl::opt<bool> PStat;
    static const llvm::cl::opt<unsigned> StatBudget;
    static const llvm::cl::opt<bool> PAGDotGraph;
    static const llvm::cl::opt<bool> DumpICFG;
    static const llvm::cl::opt<bool> CallGraphDotGraph;
    static const llvm::cl::opt<bool> PAGPrint;
    static const llvm::cl::opt<unsigned> IndirectCallLimit;
    static const llvm::cl::opt<bool> UsePreCompFieldSensitive;
    static const llvm::cl::opt<bool> EnableAliasCheck;
    static const llvm::cl::opt<bool> EnableThreadCallGraph;
    static const llvm::cl::opt<bool> ConnectVCallOnCHA;

    // PointerAnalysisImpl.cpp
    static const llvm::cl::opt<bool> INCDFPTData;

    // Memory region (MemRegion.cpp)
    static const llvm::cl::opt<bool> IgnoreDeadFun;

    // Base class of pointer analyses (MemSSA.cpp)
    static const llvm::cl::opt<bool> DumpMSSA;
    static const llvm::cl::opt<std :: string> MSSAFun;
    // static const llvm::cl::opt<string> MSSAFun;
    static const llvm::cl::opt<std::string> MemPar;

    // SVFG builder (SVFGBuilder.cpp)
    static const llvm::cl::opt<bool> SVFGWithIndirectCall;
    static const llvm::cl::opt<bool> SingleVFG;
    static llvm::cl::opt<bool> OPTSVFG;

    // FSMPTA.cpp
    static const llvm::cl::opt<bool> UsePCG;
    static const llvm::cl::opt<bool> IntraLock;
    static const llvm::cl::opt<bool> ReadPrecisionTDEdge;
    static const llvm::cl::opt<u32_t> AddModelFlag;

    // LockAnalysis.cpp
    static const llvm::cl::opt<bool> PrintLockSpan;

    // MHP.cpp
    static const llvm::cl::opt<bool> PrintInterLev;
    static const llvm::cl::opt<bool> DoLockAnalysis;

    // MTA.cpp
    static const llvm::cl::opt<bool> AndersenAnno;
    static const llvm::cl::opt<bool> FSAnno;

    // MTAAnnotator.cpp
    static const llvm::cl::opt<u32_t> AnnoFlag;

    // MTAResultValidator.cpp
    static const llvm::cl::opt<bool> PrintValidRes;

    //MTAStat.cpp
    static const llvm::cl::opt<bool> AllPairMHP;

    // PCG.cpp
    //const llvm::cl::opt<bool> TDPrint

    // TCT.cpp
    static const llvm::cl::opt<bool> TCTDotGraph;

    // LeakChecker.cpp
    static const llvm::cl::opt<bool> ValidateTests;

    // Source-sink analyzer (SrcSnkDDA.cpp)
    static const llvm::cl::opt<bool> DumpSlice;
    static const llvm::cl::opt<unsigned> CxtLimit;

    // CHG.cpp
    static const llvm::cl::opt<bool> DumpCHA;

    // DCHG.cpp
    static const llvm::cl::opt<bool> PrintDCHG;

    // LLVMModule.cpp
    static const llvm::cl::opt<std::string> Graphtxt;
    static const llvm::cl::opt<bool> SVFMain;

    // SymbolTableInfo.cpp
    static const llvm::cl::opt<bool> LocMemModel;
    static const llvm::cl::opt<bool> ModelConsts;
    static const llvm::cl::opt<bool> SymTabPrint;

    // Conditions.cpp
    static const llvm::cl::opt<unsigned> MaxBddSize;

    // PathCondAllocator.cpp
    static const llvm::cl::opt<bool> PrintPathCond;

    // SVFUtil.cpp
    static const llvm::cl::opt<bool> DisableWarn;

    // Andersen.cpp
    static const llvm::cl::opt<bool> ConsCGDotGraph;
    static const llvm::cl::opt<bool> PrintCGGraph;
    // static const llvm::cl::opt<string> WriteAnder;
    static const llvm::cl::opt<std :: string> WriteAnder;
    // static const llvm::cl::opt<string> ReadAnder;
    static const llvm::cl::opt<std :: string> ReadAnder;
    static const llvm::cl::opt<bool> PtsDiff;
    static const llvm::cl::opt<bool> MergePWC;

    // FlowSensitive.cpp
    static const llvm::cl::opt<bool> CTirAliasEval;

    //FlowSensitiveTBHC.cpp
    static const llvm::cl::opt<bool> TBHCStoreReuse;
    static const llvm::cl::opt<bool> TBHCAllReuse;;

    // TypeAnalysis.cpp
    static const llvm::cl::opt<bool> GenICFG;

    //WPAPass.cpp
    static const llvm::cl::opt<bool> AnderSVFG;
    static const llvm::cl::opt<bool> PrintAliases;
    static llvm::cl::bits<PointerAnalysis::PTATY> PASelected;
    static llvm::cl::bits<WPAPass::AliasCheckRule> AliasRule;

};
};  // namespace SVF

#endif  // ifdef OPTIONS_H_
