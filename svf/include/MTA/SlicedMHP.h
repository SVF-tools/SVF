//===- SlicedMHP.h -- MHP analysis over a sliced ICFG view --------------===//
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
 * SlicedMHP.h
 *
 *      Author: Jiawei Yang
 *
 * MHP running on a sliced ICFG view. Inherits SVF::MHP and overrides the
 * ICFG/CallGraph traversal hooks to walk only the kept (sliced) nodes/edges.
 */

#pragma once

// Include WorkList.h before TCT.h because TCT.h includes SCC.h which uses FIFOWorkList
// Include SVFVariables.h before MHP.h because MHP.h uses ValVar which needs complete definition
#include <Util/WorkList.h>
#include <SVFIR/SVFVariables.h>
#include <Util/SVFUtil.h>
#include <MTA/MHP.h>
#include <MTA/TCT.h>

#include <memory>
#include <vector>

namespace SVF
{

// Forward declarations
class SlicedSVFIRView;
class SlicedICFGView;

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

} // namespace SVF

