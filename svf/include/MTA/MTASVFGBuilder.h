//===- MTASVFGBuilder.h -- Thread-aware SVFG builder for FSAM -------------===//
//
// Builds a *thread-aware* Sparse Value-Flow Graph (SVFG) for the FSAM
// flow-sensitive multithreaded pointer analysis (Sui, Di, Xue, CGO'16).
//
// On top of the stock thread-oblivious SVFG, it adds inter-thread (interference)
// indirect value-flow edges between store/load and store/store statements that
//   (1) may-happen-in-parallel (MHP), and
//   (2) may-alias on the address-taken object, and
//   (3) are not excluded by a common lock (non-interference lock-pair pruning).
//
// This is a port of the SVF-2.9 `MTASVFGBuilder` (removed from SVF 3.x),
// adapted to the SVF 3.2 SVFG/MemSSA API and the artifact's ICFGNode-based
// MHP/LockAnalysis query interface. The precision-experiment variants of the
// original (non-sparse, PCG, read-precision edge removal) are intentionally
// dropped; only the default thread-aware edge construction is kept.
//===----------------------------------------------------------------------===//

#pragma once

#include "MSSA/SVFGBuilder.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGEdge.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MemoryModel/PointsTo.h"
#include <set>

namespace SVF
{

class MTASVFGBuilder : public SVFGBuilder
{
public:
    typedef Set<const StmtSVFGNode*> SVFGNodeSet;
    typedef NodeBS SVFGNodeIDSet;

    /// Constructor: driven by the interleaving (MHP) and lock analyses.
    MTASVFGBuilder(MHP* m, LockAnalysis* la) : SVFGBuilder(), mhp(m), lockana(la) {}
    ~MTASVFGBuilder() override = default;

    /// Number of thread-aware (interference) SVFG edges added.
    static u32_t numOfNewSVFGEdges;

    /// [THREAD-VF] slicing-source set (MSli §4.2, Fig. 6 rule [THREAD-VF]).
    ///
    /// While building VFG_pre we record, for every candidate thread-aware
    /// value-flow pair (s,s') the construction evaluates, the statements whose
    /// ILA (MHP / lock-span) results are queried to decide the [THREAD-VF] rule
    /// in the *main* phase. Per the paper's Query(s --o--> s') this is the
    /// endpoints {s,s'} plus — under a common lock — the in-span witnesses
    /// Succ_spl(s) / Pred_spl'(s') that determine TL/HD membership, i.e. whether
    /// the edge survives the non-interference test (Def. 2). Feeding this set
    /// into ILA slicing makes the sliced MHP/lock reproduce the same
    /// value-flow-construction decisions as the whole-program ILA.
    const std::set<const ICFGNode*>& getThreadVFSrcNodes() const { return threadVFSrcNodes; }

protected:
    /// Rewrite the SVFG build hook: build the stock SVFG, then add MHP edges.
    void buildSVFG() override;

    /// Inject a thread-aware MRGenerator so the MemSSA mod-ref carries the FSAM
    /// fork/join side effects (relocated here from core MemRegion).
    MRGenerator* createMRGenerator(BVDataPTAImpl* pta, bool ptrOnlyMSSA) override;

private:
    /// Collect all store/load SVFG nodes.
    void collectLoadStoreSVFGNodes();

    /// FSAM join-related thread-oblivious value flow (the "return" half of
    /// treating a join as a call without a forward): connect each start
    /// routine's exit defs (FormalOUT) to the ActualOUT at every site that joins
    /// it (FormalOUT -> ActualOUT ret edge). Done here as a post-pass over the
    /// stock SVFG, so core SVFG.cpp stays unmodified.
    void connectThreadJoinEdges();

    /// Add a FormalOUT -> ActualOUT inter-procedural indirect ret edge for a
    /// join, mirroring SVFG::addInterIndirectVFRetEdge using the public SVFG API
    /// (points-to intersection + dedup via hasInterVFGEdge + addSVFGEdge).
    void addJoinRetEdge(const FormalOUTSVFGNode* formalOut,
                        const ActualOUTSVFGNode* actualOut, CallSiteID csId);

    /// Connect inter-thread (interference) value-flow edges for MHP pairs.
    void connectMHPEdges(PointerAnalysis* pta);

    void handleStoreLoad(const StmtSVFGNode* n1, const StmtSVFGNode* n2, PointerAnalysis* pta);
    void handleStoreStore(const StmtSVFGNode* n1, const StmtSVFGNode* n2, PointerAnalysis* pta);

    /// Record the [THREAD-VF] slicing sources for one candidate pair s --o--> s'
    /// (s = src store, sp = dst load/store). Adds the endpoints, and — when the
    /// pair is protected by a common lock — the in-span successor/predecessor
    /// witnesses needed to re-decide the non-interference (tail/head) test.
    void recordThreadVFSource(const StmtSVFGNode* s, const StmtSVFGNode* sp, bool commonLock);

    /// Add a thread-MHP indirect value-flow edge srcId -> dstId carrying pts.
    SVFGEdge* addTDEdges(NodeID srcId, NodeID dstId, const PointsTo& pts);

    /// Lock-span head/tail tests (non-interference lock-pair pruning).
    //@{
    SVFGNodeIDSet getPrevNodes(const StmtSVFGNode* n);
    SVFGNodeIDSet getSuccNodes(const StmtSVFGNode* n);
    bool isHeadofSpan(const StmtSVFGNode* n);
    bool isTailofSpan(const StmtSVFGNode* n);
    //@}

    SVFGNodeSet stnodeSet;  ///< all store SVFG nodes
    SVFGNodeSet ldnodeSet;  ///< all load SVFG nodes

    std::set<const ICFGNode*> threadVFSrcNodes;  ///< [THREAD-VF] ILA slicing sources

    MHP* mhp;
    LockAnalysis* lockana;

    Map<const StmtSVFGNode*, SVFGNodeIDSet> prevset;
    Map<const StmtSVFGNode*, SVFGNodeIDSet> succset;
    Map<const StmtSVFGNode*, bool> headmap;
    Map<const StmtSVFGNode*, bool> tailmap;
};

} // End namespace SVF
