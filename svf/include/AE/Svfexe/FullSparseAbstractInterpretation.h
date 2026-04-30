//===- FullSparseAbstractInterpretation.h -- Full-Sparse Mode (stub)----//
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
//===---------------------------------------------------------------------===//

#ifndef INCLUDE_AE_SVFEXE_FULLSPARSEABSTRACTINTERPRETATION_H_
#define INCLUDE_AE_SVFEXE_FULLSPARSEABSTRACTINTERPRETATION_H_

#include "AE/Svfexe/SparseAbstractInterpretation.h"

namespace SVF
{

/// Abstract Interpretation for `Options::AESparsity::Sparse` (full-sparse).
///
/// In full-sparse mode both ValVars and ObjVars live at their SVFG
/// def-sites; reads query the SVFG for the reaching-def site, writes
/// happen at def-sites.  This is a TODO: the planned implementation
/// extends the cycle helpers to also pull/scatter ObjVars and routes
/// obj reads through SVFG (see `doc/plan-full-sparse.md`).  For now
/// this class is a compile-only stub that inherits its semi-sparse
/// behaviour from `SparseAbstractInterpretation`; it will not produce
/// correct full-sparse results until the overrides land.
class FullSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation() = default;
    ~FullSparseAbstractInterpretation() override;

    // TODO(full-sparse): override getFullCycleHeadState / widenCycleState /
    //                    narrowCycleState to also handle ObjVars via SVFG
    //                    def-site queries (see doc/plan-full-sparse.md).
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_FULLSPARSEABSTRACTINTERPRETATION_H_ */
