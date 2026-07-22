//===- FSMPTA.cpp -- Flow-sensitive multithreaded pointer analysis (FSAM) ===//
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
 * FSMPTA.cpp
 *
 *      Author: Jiawei Yang
 *
 * Implements the flow-sensitive multithreaded pointer analysis (FSAM): build
 * the thread-aware SVFG, then run the sparse flow-sensitive solver over it.
 */

#include "MTA/FSMPTA.h"
#include "Graphs/SlicedGraphs.h"
#include "WPA/Andersen.h"
#include "WPA/WPAStat.h"
#include "Util/Options.h"
#include "Graphs/SVFGNode.h"
#include "Graphs/ThreadCallGraph.h"

using namespace SVF;

/*!
 * Restrict the sparse solve to the graph's nodes: a node outside the (possibly
 * sliced) SVFG gets no transfer and propagates nothing -- a propagation barrier,
 * exactly "don't update the value flows that were sliced away". By the slice's
 * data-dependence closure, no target pointer depends on a sliced-away statement,
 * so the target points-to results are preserved. The whole graph (SVFG*)
 * contains every node, so its instantiation folds the test away.
 */
template<class SVFGGraph>
void FSMPTA<SVFGGraph>::processNode(NodeID nodeId)
{
    if (!GenericGraphTraits<SVFGGraph>::containsNode(graph, svfg->getSVFGNode(nodeId)))
        return; // outside the sliced SVFG -- leave inert
    FlowSensitive::processNode(nodeId);
}

/*!
 * Build the thread-aware SVFG with MTASVFGBuilder, then hand it to the
 * stock sparse flow-sensitive solver. Mirrors FlowSensitive::initialize()
 * but swaps the default SVFG builder for the MTA-aware one.
 */
template<class SVFGGraph>
void FSMPTA<SVFGGraph>::initialize()
{
    PointerAnalysis::initialize();

    stat = new FlowSensitiveStat(this);

    // The artifact runs FSAM with default options; clustering / plain-mapping
    // of the auxiliary Andersen's analysis is not supported here.
    assert(!Options::ClusterAnder() && "FSMPTA: clustering aux. Andersen's unsupported.");
    assert(!Options::ClusterFs() && !Options::PlainMappingFs() &&
           "FSMPTA: cluster-fs / plain-mapping-fs unsupported.");

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

// The two SVFGs the solver runs on; the implementation above is written once.
namespace SVF
{
template class FSMPTA<SVFG*>;
template class FSMPTA<const SlicedSVFGView*>;
} // namespace SVF
