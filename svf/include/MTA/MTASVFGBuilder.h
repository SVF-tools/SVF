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
 */

#ifndef INCLUDE_MTA_MTASVFGBUILDER_H_
#define INCLUDE_MTA_MTASVFGBUILDER_H_

#include "MSSA/SVFGBuilder.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGEdge.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MemoryModel/PointsTo.h"
#include <utility>
#include <vector>

namespace SVF
{

class SlicedSVFGView;

class MTASVFGBuilder : public SVFGBuilder
{
public:
    typedef Set<const StmtSVFGNode*> SVFGNodeSet;
    typedef NodeBS SVFGNodeIDSet;

    enum class InterferenceEdgeMode
    {
        Analysis,
        SlicingOnly
    };

    /// Constructor: driven by the interleaving (MHP) and lock analyses.
    MTASVFGBuilder(MHP* mhp, LockAnalysis* lockAnalysis,
                   InterferenceEdgeMode edgeMode = InterferenceEdgeMode::Analysis)
        : SVFGBuilder(),
          labelInterferenceEdges(edgeMode == InterferenceEdgeMode::Analysis),
          mhp(mhp), lockAnalysis(lockAnalysis) {}
    ~MTASVFGBuilder() override = default;

    /// Configure the builder for VFG_pre (pre-analysis) slicing, which is only
    /// *sliced*, never *solved*: the data-dependence slice traverses the
    /// interference edges for connectivity only (it reads getSrcNode/getInEdges,
    /// never the per-edge points-to label). So drop the interference-edge
    /// points-to labels here -- large programs can carry very large labels, yet
    /// those labels have no consumer in the slice. The edge set
    /// is unchanged (edges are added on the MHP + lock tests, not on points-to),
    /// so the slice -- and the preserved race set -- are identical.
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
    ///
    /// The value stores only the additional lock-span witnesses; the endpoint
    /// ICFG nodes are implicit in the key and consumers must add them back.
    using ThreadVFQueryMap = Map<ThreadVFEdge, Set<const ICFGNode*>>;
    const ThreadVFQueryMap& getThreadVFQueryMap() const
    { return threadVFQueryMap; }

    using ThreadVFCandidate = std::pair<NodeID, NodeID>;
    using ThreadVFCandidateList = std::vector<ThreadVFCandidate>;

    class ThreadVFBuildConfig
    {
    public:
        static ThreadVFBuildConfig mainPhase(
            const SlicedSVFGView* scope,
            const ThreadVFCandidateList* candidates = nullptr)
        {
            return ThreadVFBuildConfig(scope, candidates);
        }

        static ThreadVFBuildConfig wholeProgram()
        {
            return ThreadVFBuildConfig(nullptr, nullptr);
        }

    private:
        friend class MTASVFGBuilder;
        ThreadVFBuildConfig(const SlicedSVFGView* scope,
                            const ThreadVFCandidateList* candidates)
            : scope(scope), candidates(candidates) {}

        const SlicedSVFGView* scope = nullptr; ///< null means the whole base SVFG
        /// Optional conservative candidate universe selected from VFG_pre.
        /// Main MHP/lock facts still decide every emitted edge; this only avoids
        /// re-querying alias pairs that context-insensitive pre MHP rejected or
        /// whose endpoints do not survive VFG'_pre.
        const ThreadVFCandidateList* candidates = nullptr;
    };

    /// Replace only the ILA-dependent thread-aware overlay. The underlying
    /// MemorySSA, stock SVFG, and fork/join value flow remain unchanged.
    void replaceThreadAwareOverlay(MHP* mhp, LockAnalysis* lockAnalysis,
                                   const ThreadVFBuildConfig& config);

    /// Remove all currently attached thread-aware interference edges.
    void clearThreadAwareOverlay();

    size_t getThreadAwareEdgeCount() const { return threadAwareEdges.size(); }

protected:
    /// Rewrite the SVFG build hook: build the stock SVFG, then add MHP edges.
    void buildSVFG() override;

    /// Inject a thread-aware MRGenerator so the MemSSA mod-ref carries the FSAM
    /// fork/join side effects (relocated here from core MemRegion).
    std::unique_ptr<MRGenerator> createMRGenerator(BVDataPTAImpl* pta, bool ptrOnlyMSSA) override;

private:
    /// Active overlay configuration; defaults suit VFG_pre.
    const SlicedSVFGView* overlayScope = nullptr; ///< null = whole base SVFG
    const ThreadVFCandidateList* overlayCandidates = nullptr;
    bool recordThreadVF = true;                ///< false = skip [THREAD-VF] recording
    bool labelInterferenceEdges = true;        ///< false = VFG_pre (sliced-only): omit edge points-to labels

    /// Collect the store/load SVFG nodes to pair for interference edges (all of
    /// them, or -- when a slice is set -- only the kept ones).
    void collectLoadStoreSVFGNodes();
    bool isInOverlayScope(const SVFGNode* node) const;

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
    SVFGEdge* addTDEdge(NodeID srcId, NodeID dstId, const PointsTo& pts);

    /// Lock-span head/tail tests (non-interference lock-pair pruning).
    //@{
    SVFGNodeIDSet getPrevNodes(const StmtSVFGNode* n);
    SVFGNodeIDSet getSuccNodes(const StmtSVFGNode* n);
    bool isHeadOfSpan(const StmtSVFGNode* n);
    bool isTailOfSpan(const StmtSVFGNode* n);
    //@}

    SVFGNodeSet storeNodes;
    SVFGNodeSet loadNodes;

    /// [THREAD-VF] per-edge query map (see getThreadVFQueryMap).
    ThreadVFQueryMap threadVFQueryMap;
    MHP* mhp;
    LockAnalysis* lockAnalysis;

    Map<const StmtSVFGNode*, SVFGNodeIDSet> predecessorCache;
    Map<const StmtSVFGNode*, SVFGNodeIDSet> successorCache;
    Map<const StmtSVFGNode*, bool> spanHeadCache;
    Map<const StmtSVFGNode*, bool> spanTailCache;
    SVFGEdgeSet threadAwareEdges;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_MTASVFGBUILDER_H_ */
