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
 *
 * This is the engine the MSli paper calls the main FSMPTA phase.
 */

#ifndef INCLUDE_MTA_FSMPTA_H_
#define INCLUDE_MTA_FSMPTA_H_

#include "WPA/FlowSensitive.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/SlicedView.h"
#include <unordered_set>

namespace SVF
{

class FSMPTA : public FlowSensitive
{
public:
    /// Constructor.
    ///  - view != nullptr  => sliced (Layer 2): the FS solve skips memory ops not
    ///    in the slice ("don't update the value flows sliced away").
    ///  - preBuilt != nullptr => reuse an already-built thread-aware SVFG instead
    ///    of building a fresh one (build the SVFG exactly once across slicing +
    ///    main solve). Ownership stays with the caller.
    FSMPTA(MHP* m, LockAnalysis* la,
             const SlicedSVFIRView* view = nullptr, SVFG* preBuilt = nullptr)
        : FlowSensitive(m->getTCT()->getPTA()->getPAG()),
          mhp(m), mtaSVFGBuilder(m, la),
          slicedView(view), preBuiltSVFG(preBuilt)
    {
        if (slicedView)
            for (const ICFGNode* n : slicedView->getICFG()->getKeptNodes())
                keptNodes.insert(n);
    }

    ~FSMPTA() override = default;

    /// Initialise: build the thread-aware SVFG, then solve sparsely on it.
    void initialize() override;

    /// Skip flow-sensitive processing of sliced-away load/store nodes.
    void processNode(NodeID nodeId) override;

    inline MHP* getMHP() const
    {
        return mhp;
    }

private:
    MHP* mhp;
    /// Owns the thread-aware SVFG used by the FS solver (must outlive `svfg`).
    MTASVFGBuilder mtaSVFGBuilder;
    /// Non-null in sliced (Layer 2) mode.
    const SlicedSVFIRView* slicedView;
    /// Non-null when reusing a pre-built thread-aware SVFG (not owned).
    SVFG* preBuiltSVFG;
    std::unordered_set<const ICFGNode*> keptNodes;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_FSMPTA_H_ */
