//===- SlicedLockAnalysis.h -- Lock analysis over a sliced ICFG view ----===//
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
 * SlicedLockAnalysis.h
 *
 *      Author: Jiawei Yang
 *
 * LockAnalysis running on a sliced ICFG view. Inherits SVF::LockAnalysis and
 * overrides the ICFG/CallGraph traversal hooks to walk only the kept nodes/edges.
 */

#ifndef INCLUDE_MTA_SLICEDLOCKANALYSIS_H_
#define INCLUDE_MTA_SLICEDLOCKANALYSIS_H_

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
 * SlicedLockAnalysis -- LockAnalysis whose ICFG traversal hooks are overridden to
 * use a SlicedICFGView (original ICFGNode pointers stay the identities). A null
 * slicedView falls back to the full ICFG.
 */
class SlicedLockAnalysis final : public SVF::LockAnalysis {
public:
    explicit SlicedLockAnalysis(SVF::TCT* tct, const SlicedSVFIRView* slicedView = nullptr);
    ~SlicedLockAnalysis() override = default;

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

#endif /* INCLUDE_MTA_SLICEDLOCKANALYSIS_H_ */

