//===- FSPTA.cpp -- Flow-sensitive multithreaded pointer analysis -------===//
//
// Port of SVF-2.9 FSMPTA::initialize onto SVF 3.2 FlowSensitive.
//===----------------------------------------------------------------------===//

#include "MTA/FSPTA.h"
#include "WPA/Andersen.h"
#include "WPA/WPAStat.h"
#include "Util/Options.h"
#include "Graphs/SVFGNode.h"
#include "Graphs/ThreadCallGraph.h"

using namespace SVF;

/*!
 * Sliced (Layer 2) mode: the thread-aware SVFG is built whole, but we do not
 * propagate flow facts through load/store statements that the FSPTA slice
 * removed -- exactly "don't update the value flows that were sliced away".
 * By the slice's data-dependence closure, no target pointer depends on a
 * sliced-away statement, so the target points-to results are preserved.
 * Non-statement nodes (Addr, Phi, call/return boundaries) are always processed
 * to keep the inter-procedural value flow intact.
 */
void FSPTA::processNode(NodeID nodeId)
{
    if (slicedView)
    {
        const SVFGNode* node = svfg->getSVFGNode(nodeId);
        if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(node))
        {
            const StmtSVFGNode* stmt = SVFUtil::cast<StmtSVFGNode>(node);
            const ICFGNode* icfg = stmt->getICFGNode();
            if (icfg && keptNodes.find(icfg) == keptNodes.end())
                return; // sliced away -- leave inert
        }
    }
    FlowSensitive::processNode(nodeId);
}

/*!
 * Build the thread-aware SVFG with MTASVFGBuilder, then hand it to the
 * stock sparse flow-sensitive solver. Mirrors FlowSensitive::initialize()
 * but swaps the default SVFG builder for the MTA-aware one.
 */
void FSPTA::initialize()
{
    PointerAnalysis::initialize();

    stat = new FlowSensitiveStat(this);

    // The artifact runs FSAM with default options; clustering / plain-mapping
    // of the auxiliary Andersen's analysis is not supported here.
    assert(!Options::ClusterAnder() && "FSPTA: clustering aux. Andersen's unsupported.");
    assert(!Options::ClusterFs() && !Options::PlainMappingFs() &&
           "FSPTA: cluster-fs / plain-mapping-fs unsupported.");

    ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());

    if (preBuiltSVFG != nullptr)
    {
        // Reuse the thread-aware SVFG built once in pre-analysis (build once):
        // its fork/join call-graph edges and interference edges are already in
        // place. The sliced solve is handled by processNode().
        svfg = preBuiltSVFG;
    }
    else
    {
        // FSAM's thread-oblivious value flow treats a thread fork as an ordinary
        // call: the spawner's memory state must flow into the spawnee. The stock
        // Andersen call graph does not carry the pthread fork/join edges, so we
        // add them here (they are CallGraphEdges) before building MemSSA/SVFG.
        if (ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(ander->getCallGraph()))
        {
            tcg->updateCallGraph(ander);   // fork edges (spawner -> spawnee, forward)
            tcg->updateJoinEdge(ander);    // join edges (for join-related def-use)
        }
        // Build the thread-aware SVFG (stock value flow + MHP interference edges).
        svfg = mtaSVFGBuilder.buildPTROnlySVFG(ander);
    }

    setGraph(svfg);
}
