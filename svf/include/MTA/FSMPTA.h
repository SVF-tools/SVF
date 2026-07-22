//===- FSMPTA.h -- Flow-sensitive multithreaded pointer analysis (FSAM) -===//
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
 * FSMPTA.h
 *
 *      Author: Jiawei Yang
 *
 * Sparse flow-sensitive pointer analysis for multithreaded programs (FSAM,
 * Sui/Di/Xue CGO'16). It runs the sparse flow-sensitive solver
 * (`FlowSensitive`) over a *thread-aware* SVFG built by `MTASVFGBuilder`,
 * i.e. the stock thread-oblivious value flow augmented with inter-thread
 * (interference) edges derived from the MHP and lock analyses.
 */

#ifndef INCLUDE_MTA_FSMPTA_H_
#define INCLUDE_MTA_FSMPTA_H_

#include "WPA/FlowSensitive.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/MTASlicer.h"
#include <unordered_set>

namespace SVF
{

/// One solver for the whole and the sliced SVFG: SVFGGraph is SVFG* (whole;
/// every node processed) or const SlicedSVFGView* (sliced; nodes outside the
/// view are propagation barriers). Restriction is answered by
/// GenericGraphTraits<SVFGGraph>::containsNode, resolved at compile time; the
/// stock FlowSensitive transfer semantics are untouched.
template<class SVFGGraph>
class FSMPTA : public FlowSensitive
{
public:
    /// Constructor.
    ///  - graph: gates the per-node transfer (the solver runs over the whole
    ///    thread-aware SVFG). For SVFGGraph == SVFG* it may be null (every node
    ///    is processed). A SlicedSVFGView needs only its ICFG view for membership,
    ///    so it can be created before the SVFG itself is built here.
    FSMPTA(MHP* m, LockAnalysis* la, SVFGGraph graph)
        : FlowSensitive(m->getTCT()->getPTA()->getPAG()),
          mhp(m), mtaSVFGBuilder(m, la), graph(graph)
    {
    }

    ~FSMPTA() override = default;

    /// Initialise: build the thread-aware SVFG, then solve sparsely on it.
    void initialize() override;

    /// Restrict the solve to the graph's nodes (whole: no restriction).
    void processNode(NodeID nodeId) override;

    inline MHP* getMHP() const
    {
        return mhp;
    }

private:
    MHP* mhp;
    /// Owns the thread-aware SVFG used by the FS solver (must outlive `svfg`).
    MTASVFGBuilder mtaSVFGBuilder;
    /// The graph the solve is restricted to (see the constructor).
    SVFGGraph graph;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_FSMPTA_H_ */
