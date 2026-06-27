//===- MTASVFGBuilder.h -- Thread-aware SVFG builder for FSAM -----------===//
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

/*
 * MTASVFGBuilder.h
 *
 *      Author: Jiawei Yang
 *
 * Builds a *thread-aware* Sparse Value-Flow Graph (SVFG) for the FSAM
 * flow-sensitive multithreaded pointer analysis (Sui, Di, Xue, CGO'16).
 *
 * On top of the stock thread-oblivious SVFG, it adds inter-thread (interference)
 * indirect value-flow edges between store/load and store/store statements that
 *   (1) may-happen-in-parallel (MHP), and
 *   (2) may-alias on the address-taken object, and
 *   (3) are not excluded by a common lock (non-interference lock-pair pruning).
 *
 * Only the default thread-aware edge construction is kept; the
 * precision-experiment variants (non-sparse, PCG, read-precision edge removal)
 * are intentionally omitted.
 */

#ifndef INCLUDE_MTA_MTASVFGBUILDER_H_
#define INCLUDE_MTA_MTASVFGBUILDER_H_

#include "MSSA/SVFGBuilder.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGEdge.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MemoryModel/PointsTo.h"
#include <map>
#include <set>
#include <utility>

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

    /// A candidate thread-aware value-flow edge s --o--> s' (src store, dst
    /// load/store), keyed by its endpoint SVFG nodes.
    typedef std::pair<const StmtSVFGNode*, const StmtSVFGNode*> ThreadVFEdge;

    /// [THREAD-VF] per-edge query map (MSli §4.2, Fig. 6 rule [THREAD-VF]).
    ///
    /// While building VFG_pre we record, for every candidate thread-aware
    /// value-flow edge (s,s') the construction evaluates, its Query(s --o--> s')
    /// set: the endpoints {s,s'} plus — under a common lock — the in-span
    /// witnesses Succ_spl(s) / Pred_spl'(s') that decide TL/HD membership, i.e.
    /// whether the edge survives the non-interference test (Def. 2). The query is
    /// kept *per edge* (not pre-unioned) so ILA slicing can restrict the sources
    /// to the edges that survive the FSPTA slice — ThreadVF(VFG'_pre) — rather
    /// than every candidate pair. Feeding the retained edges' queries into ILA
    /// slicing makes the sliced MHP/lock reproduce the same value-flow decisions
    /// the main phase makes, while keeping the slice minimal.
    const std::map<ThreadVFEdge, std::set<const ICFGNode*>>& getThreadVFQueryMap() const
    { return threadVFQueryMap; }

protected:
    /// Rewrite the SVFG build hook: build the stock SVFG, then add MHP edges.
    void buildSVFG() override;

    /// Inject a thread-aware MRGenerator so the MemSSA mod-ref carries the FSAM
    /// fork/join side effects (relocated here from core MemRegion).
    std::unique_ptr<MRGenerator> createMRGenerator(BVDataPTAImpl* pta, bool ptrOnlyMSSA) override;

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

    /// [THREAD-VF] per-edge query map (see getThreadVFQueryMap).
    std::map<ThreadVFEdge, std::set<const ICFGNode*>> threadVFQueryMap;

    MHP* mhp;
    LockAnalysis* lockana;

    Map<const StmtSVFGNode*, SVFGNodeIDSet> prevset;
    Map<const StmtSVFGNode*, SVFGNodeIDSet> succset;
    Map<const StmtSVFGNode*, bool> headmap;
    Map<const StmtSVFGNode*, bool> tailmap;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_MTASVFGBUILDER_H_ */
