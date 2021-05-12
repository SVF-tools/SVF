//===- Options.cpp -- Command line options ------------------------//

#include <llvm/Support/CommandLine.h>
#include "Util/Options.h"

namespace SVF
{
    const llvm::cl::opt<bool> Options::MarkedClocksOnly(
        "marked-clocks-only",
        llvm::cl::init(false),
        llvm::cl::desc("Only measure times where explicitly marked"));

    const llvm::cl::opt<NodeIDAllocator::Strategy> Options::NodeAllocStrat(
        "node-alloc-strat",
        llvm::cl::init(NodeIDAllocator::Strategy::DEBUG),
        llvm::cl::desc("Method of allocating (LLVM) values and memory objects as node IDs"),
        llvm::cl::values(
            clEnumValN(NodeIDAllocator::Strategy::DENSE, "dense", "allocate objects together and values together, separately (default)"),
            clEnumValN(NodeIDAllocator::Strategy::SEQ, "seq", "allocate values and objects sequentially, intermixed"),
            clEnumValN(NodeIDAllocator::Strategy::DEBUG, "debug", "allocate value and objects sequentially, intermixed, except GEP objects as offsets")));

    const llvm::cl::opt<unsigned> Options::MaxFieldLimit(
        "field-limit",
        llvm::cl::init(512),
        llvm::cl::desc("Maximum number of fields for field sensitive analysis"));


    // ContextDDA.cpp
    const llvm::cl::opt<unsigned long long> Options::CxtBudget(
        "cxt-bg", 
        llvm::cl::init(10000),
        llvm::cl::desc("Maximum step budget of context-sensitive traversing")
    );


    // DDAClient.cpp
    const llvm::cl::opt<bool> Options::SingleLoad(
        "single-load", 
        llvm::cl::init(true),
        llvm::cl::desc("Count load pointer with same source operand as one query")
    );

    const llvm::cl::opt<bool> Options::DumpFree(
        "dump-free", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump use after free locations")
    );

    const llvm::cl::opt<bool> Options::DumpUninitVar(
        "dump-uninit-var", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump uninitialised variables")
    );

    const llvm::cl::opt<bool> Options::DumpUninitPtr(
        "dump-uninit-ptr", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump uninitialised pointers")
    );

    const llvm::cl::opt<bool> Options::DumpSUPts(
        "dump-su-pts", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump strong updates store")
    );

    const llvm::cl::opt<bool> Options::DumpSUStore(
        "dump-su-store", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump strong updates store")
    );

    const llvm::cl::opt<bool> Options::MallocOnly(
        "malloc-only", 
        llvm::cl::init(true),
        llvm::cl::desc("Only add tainted objects for malloc")
    );

    const llvm::cl::opt<bool> Options::TaintUninitHeap(
        "uninit-heap", 
        llvm::cl::init(true),
        llvm::cl::desc("detect uninitialized heap variables")
    );

    const llvm::cl::opt<bool> Options::TaintUninitStack(
        "uninit-stack", 
        llvm::cl::init(true),
        llvm::cl::desc("detect uninitialized stack variables")
    );

    // DDAPass.cpp
    const llvm::cl::opt<unsigned> Options::MaxPathLen(
        "max-path",  
        llvm::cl::init(100000),
        llvm::cl::desc("Maximum path limit for DDA")
    );

    const llvm::cl::opt<unsigned> Options::MaxContextLen(
        "max-cxt",  
        llvm::cl::init(3),
        llvm::cl::desc("Maximum context limit for DDA")
    );

    const llvm::cl::opt<std::string> Options::UserInputQuery(
        "query",  
        llvm::cl::init("all"),
        llvm::cl::desc("Please specify queries by inputing their pointer ids")
    );

    const llvm::cl::opt<bool> Options::InsenRecur(
        "in-recur", 
        llvm::cl::init(false),
        llvm::cl::desc("Mark context insensitive SVFG edges due to function recursions")
    );

    const llvm::cl::opt<bool> Options::InsenCycle(
        "in-cycle", 
        llvm::cl::init(false),
        llvm::cl::desc("Mark context insensitive SVFG edges due to value-flow cycles")
    );

    const llvm::cl::opt<bool> Options::PrintCPts(
        "cpts", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump conditional points-to set ")
    );

    const llvm::cl::opt<bool> Options::PrintQueryPts(
        "print-query-pts",
        llvm::cl::init(false),
        llvm::cl::desc("Dump queries' conditional points-to set ")
    );

    const llvm::cl::opt<bool> Options::WPANum(
        "wpa-num", 
        llvm::cl::init(false),
        llvm::cl::desc("collect WPA FS number only ")
    );

    /// register this into alias analysis group
    //static RegisterAnalysisGroup<AliasAnalysis> AA_GROUP(DDAPA);
    llvm::cl::bits<PointerAnalysis::PTATY> Options::DDASelected(
        llvm::cl::desc("Select pointer analysis"),
        llvm::cl::values(
            clEnumValN(PointerAnalysis::FlowS_DDA, "dfs", "Demand-driven flow sensitive analysis"),
            clEnumValN(PointerAnalysis::Cxt_DDA, "cxt", "Demand-driven context- flow- sensitive analysis")
    ));

    // FlowDDA.cpp
    const llvm::cl::opt<unsigned long long> Options::FlowBudget(
        "flow-bg",  
        llvm::cl::init(10000),
        llvm::cl::desc("Maximum step budget of flow-sensitive traversing")
    );


    // Offline constraint graph (OfflineConsG.cpp)
    const llvm::cl::opt<bool> Options::OCGDotGraph(
        "dump-ocg", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of Offline Constraint Graph")
    );

    
    // Program Assignment Graph for pointer analysis (PAG.cpp)
    llvm::cl::opt<bool> Options::HandBlackHole(
        "blk", 
        llvm::cl::init(false),
        llvm::cl::desc("Hanle blackhole edge")
    );

    const llvm::cl::opt<bool> Options::FirstFieldEqBase(
        "ff-eq-base", 
        llvm::cl::init(true),
        llvm::cl::desc("Treat base objects as their first fields")
    );


    // SVFG optimizer (SVFGOPT.cpp)
    const llvm::cl::opt<bool> Options::ContextInsensitive(
        "ci-svfg", 
        llvm::cl::init(false),
        llvm::cl::desc("Reduce SVFG into a context-insensitive one")
    );

    const llvm::cl::opt<bool> Options::KeepAOFI(
        "keep-aofi", 
        llvm::cl::init(false),
        llvm::cl::desc("Keep formal-in and actual-out parameters")
    );

    const llvm::cl::opt<std::string> Options::SelfCycle(
        "keep-self-cycle", 
        llvm::cl::value_desc("keep self cycle"),
        llvm::cl::desc("How to handle self cycle edges: all, context, none")
    );


    // Sparse value-flow graph (VFG.cpp)
    const llvm::cl::opt<bool> Options::DumpVFG(
        "dump-vfg",
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of VFG")
    );


    // Location set for modeling abstract memory object (LocationSet.cpp)
    const llvm::cl::opt<bool> Options::SingleStride(
        "stride-only", 
        llvm::cl::init(false),
        llvm::cl::desc("Only use single stride in LocMemoryModel")
    );

    
    // Base class of pointer analyses (PointerAnalysis.cpp)
    const llvm::cl::opt<bool> Options::TypePrint(
        "print-type", 
        llvm::cl::init(false),
        llvm::cl::desc("Print type")
    );

    const llvm::cl::opt<bool> Options::FuncPointerPrint(
        "print-fp", 
        llvm::cl::init(false),
        llvm::cl::desc("Print targets of indirect call site")
    );

    const llvm::cl::opt<bool> Options::PTSPrint(
        "print-pts", 
        llvm::cl::init(false),
        llvm::cl::desc("Print points-to set of top-level pointers")
    );

    const llvm::cl::opt<bool> Options::PTSAllPrint(
        "print-all-pts", 
        llvm::cl::init(false),
        llvm::cl::desc("Print all points-to set of both top-level and address-taken variables")
    );

    const llvm::cl::opt<bool> Options::PStat(
        "stat", 
        llvm::cl::init(true),
        llvm::cl::desc("Statistic for Pointer analysis")
    );

    const llvm::cl::opt<unsigned> Options::StatBudget(
        "stat-limit", 
        llvm::cl::init(20),
        llvm::cl::desc("Iteration budget for On-the-fly statistics")
    );

    const llvm::cl::opt<bool> Options::PAGDotGraph(
        "dump-pag", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of PAG")
    );

    const llvm::cl::opt<bool> Options::PAGDotGraphShorter(
            "dump-pag-shorter",
            llvm::cl::init(true),
            llvm::cl::desc("If dumping dot graph of PAG, use shorter lines")
    );

    const llvm::cl::opt<bool> Options::DumpICFG(
        "dump-icfg", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of ICFG")
    );

    const llvm::cl::opt<bool> Options::CallGraphDotGraph(
        "dump-callgraph", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of Call Graph")
    );

    const llvm::cl::opt<bool> Options::PAGPrint(
        "print-pag", 
        llvm::cl::init(false),
        llvm::cl::desc("Print PAG to command line")
    );

    const llvm::cl::opt<unsigned> Options::IndirectCallLimit(
        "ind-call-limit",  
        llvm::cl::init(50000),
        llvm::cl::desc("Indirect solved call edge limit")
    );

    const llvm::cl::opt<bool> Options::UsePreCompFieldSensitive(
        "pre-field-sensitive", 
        llvm::cl::init(true),
        llvm::cl::desc("Use pre-computed field-sensitivity for later analysis")
    );

    const llvm::cl::opt<bool> Options::EnableAliasCheck(
        "alias-check", 
        llvm::cl::init(true),
        llvm::cl::desc("Enable alias check functions")
    );

    const llvm::cl::opt<bool> Options::EnableThreadCallGraph(
        "enable-tcg", 
        llvm::cl::init(true),
        llvm::cl::desc("Enable pointer analysis to use thread call graph")
    );

    const llvm::cl::opt<bool> Options::ConnectVCallOnCHA(
        "v-call-cha", 
        llvm::cl::init(false),
        llvm::cl::desc("connect virtual calls using cha")
    );
    

    // PointerAnalysisImpl.cpp
    const llvm::cl::opt<bool> Options::INCDFPTData(
        "inc-data", 
        llvm::cl::init(true),
        llvm::cl::desc("Enable incremental DFPTData for flow-sensitive analysis")
    );

    
    // Memory region (MemRegion.cpp)
    const llvm::cl::opt<bool> Options::IgnoreDeadFun(
        "mssa-ignore-dead-fun", 
        llvm::cl::init(false),
        llvm::cl::desc("Don't construct memory SSA for deadfunction")
    );

    
    // Base class of pointer analyses (MemSSA.cpp)
    const llvm::cl::opt<bool> Options::DumpMSSA(
        "dump-mssa", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump memory SSA")
    );

    const llvm::cl::opt<std::string> Options::MSSAFun(
        "mssa-fun",  
        llvm::cl::init(""),
        llvm::cl::desc("Please specify which function needs to be dumped")
    );

    const llvm::cl::opt<std::string> Options::MemPar(
        "mem-par", 
        llvm::cl::value_desc("memory-partition-type"),
        llvm::cl::desc("memory partition strategy")
    );


    // SVFG builder (SVFGBuilder.cpp)
    const llvm::cl::opt<bool> Options::SVFGWithIndirectCall(
        "svfg-with-ind-call", 
        llvm::cl::init(false),
        llvm::cl::desc("Update Indirect Calls for SVFG using pre-analysis")
    );

    const llvm::cl::opt<bool> Options::SingleVFG(
        "single-vfg", 
        llvm::cl::init(false),
        llvm::cl::desc("Create a single VFG shared by multiple analysis")
    );

    llvm::cl::opt<bool> Options::OPTSVFG(
        "opt-svfg", 
        llvm::cl::init(true),
        llvm::cl::desc("Optimize SVFG to eliminate formal-in and actual-out")
    );

    
    // FSMPTA.cpp
    const llvm::cl::opt<bool> Options::UsePCG(
        "pcg-td-edge", 
        llvm::cl::init(false),
        llvm::cl::desc("Use PCG lock for non-sparsely adding SVFG edges")
    );

    const llvm::cl::opt<bool> Options::IntraLock(
        "intra-lock-td-edge", 
        llvm::cl::init(true), 
        llvm::cl::desc("Use simple intra-procedual lock for adding SVFG edges")
    );

    const llvm::cl::opt<bool> Options::ReadPrecisionTDEdge(
        "rp-td-edge", 
        llvm::cl::init(false), 
        llvm::cl::desc("perform read precision to refine SVFG edges")
    );

    const llvm::cl::opt<u32_t> Options::AddModelFlag(
        "add-td-edge", 
        llvm::cl::init(0), 
        llvm::cl::desc("Add thread SVFG edges with models: 0 Non Add Edge; 1 NonSparse; 2 All Optimisation; 3 No MHP; 4 No Alias; 5 No Lock; 6 No Read Precision.")
    );


    // LockAnalysis.cpp
    const llvm::cl::opt<bool> Options::PrintLockSpan(
        "print-lock", 
        llvm::cl::init(false), 
        llvm::cl::desc("Print Thread Interleaving Results")
    );


    // MHP.cpp
    const llvm::cl::opt<bool> Options::PrintInterLev(
        "print-interlev", 
        llvm::cl::init(false),
        llvm::cl::desc("Print Thread Interleaving Results")
    );
    
    const llvm::cl::opt<bool> Options::DoLockAnalysis(
        "lock-analysis", 
        llvm::cl::init(true),
        llvm::cl::desc("Run Lock Analysis")
    );


    // MTA.cpp
    const llvm::cl::opt<bool> Options::AndersenAnno(
        "tsan-ander", 
        llvm::cl::init(false), 
        llvm::cl::desc("Add TSan annotation according to Andersen")
    );

    const llvm::cl::opt<bool> Options::FSAnno(
        "tsan-fs", 
        llvm::cl::init(false), 
        llvm::cl::desc("Add TSan annotation according to flow-sensitive analysis")
    );


    // MTAAnnotator.cpp
    const llvm::cl::opt<u32_t> Options::AnnoFlag(
        "anno", 
        llvm::cl::init(0), 
        llvm::cl::desc("prune annotated instructions: 0001 Thread Local; 0002 Alias; 0004 MHP.")
    );

    
    // MTAResultValidator.cpp
    const llvm::cl::opt<bool> Options::PrintValidRes(
        "print-mhp-validation", 
        llvm::cl::init(false), 
        llvm::cl::desc("Print MHP Validation Results")
    );


    // MTAStat.cpp
    const llvm::cl::opt<bool> Options::AllPairMHP(
        "all-pair-mhp", 
        llvm::cl::init(false), 
        llvm::cl::desc("All pair MHP computation")
    );


    // PCG.cpp
    //const llvm::cl::opt<bool> TDPrint(
        // "print-td",
        // llvm::cl::init(true), 
        // llvm::cl::desc("Print Thread Analysis Results"));


    // TCT.cpp
    const llvm::cl::opt<bool> Options::TCTDotGraph(
        "dump-tct", 
        llvm::cl::init(false), 
        llvm::cl::desc("Dump dot graph of Call Graph")
    );

    
    // LeakChecker.cpp
    const llvm::cl::opt<bool> Options::ValidateTests(
        "valid-tests", 
        llvm::cl::init(false),
        llvm::cl::desc("Validate memory leak tests")
    );


    // Source-sink analyzer (SrcSnkDDA.cpp)
    const llvm::cl::opt<bool> Options::DumpSlice(
        "dump-slice", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of Saber Slices")
    );

    const llvm::cl::opt<unsigned> Options::CxtLimit(
        "cxt-limit",  
        llvm::cl::init(3),
        llvm::cl::desc("Source-Sink Analysis Contexts Limit")
    );

    
    // CHG.cpp
    const llvm::cl::opt<bool> Options::DumpCHA(
        "dump-cha", 
        llvm::cl::init(false), 
        llvm::cl::desc("dump the class hierarchy graph")
    );


    // DCHG.cpp
    const llvm::cl::opt<bool> Options::PrintDCHG(
        "print-dchg", 
        llvm::cl::init(false), 
        llvm::cl::desc("print the DCHG if debug information is available")
    );


    // LLVMModule.cpp
    const llvm::cl::opt<std::string> Options::Graphtxt(
        "graph-txt", 
        llvm::cl::value_desc("filename"),
        llvm::cl::desc("graph txt file to build PAG")
    );

    const llvm::cl::opt<bool> Options::SVFMain(
        "svf-main", 
        llvm::cl::init(false),
        llvm::cl::desc("add svf.main()")
    );

    
    // SymbolTableInfo.cpp
    const llvm::cl::opt<bool> Options::LocMemModel(
        "loc-mm", 
        llvm::cl::init(false),
        llvm::cl::desc("Bytes/bits modeling of memory locations")
    );

    const llvm::cl::opt<bool> Options::ModelConsts(
        "model-consts", 
        llvm::cl::init(false),
        llvm::cl::desc("Modeling individual constant objects")
    );

    const llvm::cl::opt<bool> Options::SymTabPrint(
            "print-symbol-table", llvm::cl::init(false),
            llvm::cl::desc("Print Symbol Table to command line")
    );


    // Conditions.cpp
    const llvm::cl::opt<unsigned> Options::MaxBddSize(
        "max-bdd-size",  
        llvm::cl::init(100000),
        llvm::cl::desc("Maximum context limit for DDA")
    );

    
    // PathCondAllocator.cpp
    const llvm::cl::opt<bool> Options::PrintPathCond(
        "print-pc", 
        llvm::cl::init(false),
        llvm::cl::desc("Print out path condition")
    );


    // SVFUtil.cpp
    const llvm::cl::opt<bool> Options::DisableWarn(
        "dwarn", 
        llvm::cl::init(true),
        llvm::cl::desc("Disable warning")
    );

    
    // Andersen.cpp
    const llvm::cl::opt<bool> Options::ConsCGDotGraph(
        "dump-constraint-graph", 
        llvm::cl::init(false),
        llvm::cl::desc("Dump dot graph of Constraint Graph")
    );

    const llvm::cl::opt<bool> Options::BriefConsCGDotGraph(
            "brief-constraint-graph",
            llvm::cl::init(true),
            llvm::cl::desc("Dump dot graph of Constraint Graph")
    );

    const llvm::cl::opt<bool> Options::PrintCGGraph(
        "print-constraint-graph", 
        llvm::cl::init(false),
        llvm::cl::desc("Print Constraint Graph to Terminal")
    );

    const llvm::cl::opt<std::string> Options::WriteAnder(
        "write-ander",  
        llvm::cl::init(""),
        llvm::cl::desc("Write Andersen's analysis results to a file")
    );

    const llvm::cl::opt<std::string> Options::ReadAnder(
        "read-ander",  
        llvm::cl::init(""),
        llvm::cl::desc("Read Andersen's analysis results from a file")
    );

    const llvm::cl::opt<bool> Options::PtsDiff(
        "diff",  
        llvm::cl::init(true),
        llvm::cl::desc("Disable diff pts propagation")
    );

    const llvm::cl::opt<bool> Options::MergePWC(
        "merge-pwc",  
        llvm::cl::init(true),
        llvm::cl::desc("Enable PWC in graph solving")
    );

    
    // FlowSensitive.cpp
    const llvm::cl::opt<bool> Options::CTirAliasEval(
        "ctir-alias-eval", 
        llvm::cl::init(false), 
        llvm::cl::desc("Prints alias evaluation of ctir instructions in FS analyses")
    );

    
    // FlowSensitiveTBHC.cpp
    /// Whether we allow reuse for TBHC.
    const llvm::cl::opt<bool> Options::TBHCStoreReuse(
        "tbhc-store-reuse", 
        llvm::cl::init(false), 
        llvm::cl::desc("Allow for object reuse in at stores in FSTBHC")
    );

    const llvm::cl::opt<bool> Options::TBHCAllReuse(
        "tbhc-all-reuse", 
        llvm::cl::init(false), 
        llvm::cl::desc("Allow for object reuse everywhere in FSTBHC")
    );

    
    // TypeAnalysis.cpp
    const llvm::cl::opt<bool> Options::GenICFG(
        "gen-icfg", 
        llvm::cl::init(true), 
        llvm::cl::desc("Generate ICFG graph")
    );


    //WPAPass.cpp
    const llvm::cl::opt<bool> Options::AnderSVFG(
        "svfg", 
        llvm::cl::init(false),
        llvm::cl::desc("Generate SVFG after Andersen's Analysis")
    );

    const llvm::cl::opt<bool> Options::WPAOPTSVFG(
            "wpa-opt-svfg",
            llvm::cl::init(false),
            llvm::cl::desc("When using WPA pass, optimize SVFG to eliminate formal-in and actual-out (default false)")
    );

    const llvm::cl::opt<bool> Options::PrintAliases(
        "print-aliases", 
        llvm::cl::init(false),
        llvm::cl::desc("Print results for all pair aliases")
    );

    llvm::cl::bits<PointerAnalysis::PTATY> Options::PASelected(
        llvm::cl::desc("Select pointer analysis"),
        llvm::cl::values(
            clEnumValN(PointerAnalysis::Andersen_WPA, "nander", "Standard inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenLCD_WPA, "lander", "Lazy cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenHCD_WPA, "hander", "Hybrid cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenHLCD_WPA, "hlander", "Hybrid lazy cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenSCD_WPA, "sander", "Selective cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenSFR_WPA, "sfrander", "Stride-based field representation includion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenWaveDiff_WPA, "wander", "Wave propagation inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenWaveDiff_WPA, "ander", "Diff wave propagation inclusion-based analysis"),
            clEnumValN(PointerAnalysis::Steensgaard_WPA, "steens", "Steensgaard's pointer analysis"),
            // Disabled till further work is done.
            // clEnumValN(PointerAnalysis::AndersenWaveDiffWithType_WPA, "andertype", "Diff wave propagation with type inclusion-based analysis"),
            clEnumValN(PointerAnalysis::FSSPARSE_WPA, "fspta", "Sparse flow sensitive pointer analysis"),
            clEnumValN(PointerAnalysis::FSTBHC_WPA, "fstbhc", "Sparse flow-sensitive type-based heap cloning pointer analysis"),
            clEnumValN(PointerAnalysis::VFS_WPA, "vfspta", "Versioned sparse flow-sensitive points-to analysis"),
            clEnumValN(PointerAnalysis::TypeCPP_WPA, "type", "Type-based fast analysis for Callgraph, PAG and CHA")
        ));


    llvm::cl::bits<WPAPass::AliasCheckRule> Options::AliasRule(llvm::cl::desc("Select alias check rule"),
        llvm::cl::values(
            clEnumValN(WPAPass::Conservative, "conservative", "return MayAlias if any pta says alias"),
            clEnumValN(WPAPass::Veto, "veto", "return NoAlias if any pta says no alias")
        ));

    
}; // namespace SVF.
