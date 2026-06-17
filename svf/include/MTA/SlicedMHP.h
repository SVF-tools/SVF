// MSli: MHP running on a sliced ICFG view.
// Inherits from SVF::MHP and overrides ICFG traversal methods to use SlicedICFGView.
#pragma once

// Include WorkList.h before TCT.h because TCT.h includes SCC.h which uses FIFOWorkList
// Include SVFVariables.h before MHP.h because MHP.h uses ValVar which needs complete definition
#include <Util/WorkList.h>
#include <SVFIR/SVFVariables.h>
#include <Util/SVFUtil.h>
#include <MTA/MHP.h>
#include <MTA/TCT.h>

// Forward declarations
class SlicedSVFIRView;
class SlicedICFGView;

#include <memory>
#include <vector>

/**
 * SlicedMHP
 *
 * Inherits SVF::MHP and overrides only the ICFG/CallGraph traversal hooks so the
 * inherited interleaving algorithm walks the sliced view. Fork/join relationships
 * are program-global, so handleJoin reuses the base ForkJoinAnalysis unchanged.
 */
class SlicedMHP final : public SVF::MHP {
public:
    /// If slicedView is provided, it is used to access the sliced ICFG/ThreadCallGraph
    /// views; if null, the full ICFG from SVFIR is used.
    explicit SlicedMHP(SVF::TCT* tct, const SlicedSVFIRView* slicedView = nullptr);
    ~SlicedMHP() override;

protected:
    // Override the base ICFG/CallGraph traversal hooks to walk only the slice;
    // every inherited MHP handler reaches the slice through these.
    const SVF::ICFGNode* getFunEntry(const SVF::FunObjVar* fun) const override;
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const override;
    void getInEdgesOfCallGraphNode(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const override;

    // Kept ICFG nodes of a function (not a base hook; used by updateNonCandidateFunInterleaving).
    void getFunICFGNodes(const SVF::FunObjVar* fun, std::vector<const SVF::ICFGNode*>& out) const;

    // Non-candidate interleaving iterates the sliced CallGraph, so it stays overridden.
    void updateNonCandidateFunInterleaving() override;

private:
    const SlicedSVFIRView* slicedView; // Optional: for accessing sliced views
    const SlicedICFGView* icfgView; // ICFG view (from slicedView or nullptr for full ICFG)
};

