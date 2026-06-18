//===- FSPTA.h -- Flow-sensitive multithreaded pointer analysis (FSAM) --===//
//
// Sparse flow-sensitive pointer analysis for multithreaded programs (FSAM,
// Sui/Di/Xue CGO'16). It runs SVF 3.2's sparse flow-sensitive solver
// (`FlowSensitive`) over a *thread-aware* SVFG built by `MTASVFGBuilder`,
// i.e. the stock thread-oblivious value flow augmented with inter-thread
// (interference) edges derived from the MHP and lock analyses.
//
// This is the engine the MSli paper calls the main FSPTA phase.
//
// Port of SVF-2.9 `FSMPTA` (removed from SVF 3.x), adapted to the SVF 3.2
// FlowSensitive/SVFGBuilder API.
//===----------------------------------------------------------------------===//
// Author: Jiawei Yang

#pragma once

#include "WPA/FlowSensitive.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/SlicedView.h"
#include <unordered_set>

namespace SVF
{

class FSPTA : public FlowSensitive
{
public:
    /// Constructor.
    ///  - view != nullptr  => sliced (Layer 2): the FS solve skips memory ops not
    ///    in the slice ("don't update the value flows sliced away").
    ///  - preBuilt != nullptr => reuse an already-built thread-aware SVFG instead
    ///    of building a fresh one (build the SVFG exactly once across slicing +
    ///    main solve). Ownership stays with the caller.
    FSPTA(MHP* m, LockAnalysis* la,
             const SlicedSVFIRView* view = nullptr, SVFG* preBuilt = nullptr)
        : FlowSensitive(m->getTCT()->getPTA()->getPAG()),
          mhp(m), lockana(la), mtaSVFGBuilder(m, la),
          slicedView(view), preBuiltSVFG(preBuilt)
    {
        if (slicedView)
            for (const ICFGNode* n : slicedView->getICFG()->getKeptNodes())
                keptNodes.insert(n);
    }

    ~FSPTA() override = default;

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
    LockAnalysis* lockana;
    /// Owns the thread-aware SVFG used by the FS solver (must outlive `svfg`).
    MTASVFGBuilder mtaSVFGBuilder;
    /// Non-null in sliced (Layer 2) mode.
    const SlicedSVFIRView* slicedView;
    /// Non-null when reusing a pre-built thread-aware SVFG (not owned).
    SVFG* preBuiltSVFG;
    std::unordered_set<const ICFGNode*> keptNodes;
};

} // End namespace SVF
