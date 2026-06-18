// MSli: LockAnalysis running on a sliced ICFG view.
// Inherits from SVF::LockAnalysis and overrides ICFG traversal methods to use SlicedICFGView.
// Author: Jiawei Yang
#pragma once

// Include WorkList.h before TCT.h because TCT.h includes SCC.h which uses FIFOWorkList
#include <Util/WorkList.h>
#include <MTA/LockAnalysis.h>
#include <MTA/TCT.h>

#include <memory>
#include <vector>

namespace SVF
{

// Forward declarations
class SlicedSVFIRView;
class SlicedICFGView;

/**
 * SlicedLockAnalysis
 *
 * Inherits from SVF::LockAnalysis and overrides ICFG traversal methods to use SlicedICFGView.
 *
 * Design goals:
 * - Inherit from SVF::LockAnalysis to reuse existing implementation
 * - Override virtual ICFG traversal methods to use sliced view semantics
 * - Keep using original SVF::ICFGNode pointers as identities
 */
class SlicedLockAnalysis final : public SVF::LockAnalysis {
public:
    /// If slicedView is provided, it will be used to access sliced ICFG/CallGraph/ThreadCallGraph views.
    /// If slicedView is null, uses full ICFG from SVFIR.
    explicit SlicedLockAnalysis(SVF::TCT* tct, const SlicedSVFIRView* slicedView = nullptr);
    ~SlicedLockAnalysis() = default;

protected:
    // Override the base ICFG/CallGraph traversal hooks to walk only the slice;
    // every inherited LockAnalysis routine reaches the slice through these.
    const SVF::ICFGNode* getFunEntry(const SVF::FunObjVar* fun) const override;
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const override;
    void getPredNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const override;
    bool acceptsNode(const SVF::ICFGNode* node) const override;
    void getInEdgesOfCallGraphNode(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const override;
    const SVF::CallGraph* getAnalysisCallGraph() const override;

private:
    const SlicedSVFIRView* slicedView; // Optional: for accessing sliced views
    const SlicedICFGView* icfgView; // ICFG view (from slicedView or nullptr for full ICFG)
};

} // namespace SVF

