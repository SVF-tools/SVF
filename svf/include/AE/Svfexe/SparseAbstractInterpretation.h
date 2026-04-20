//===- SparseAbstractInterpretation.h -- Sparse Abstract Execution------//
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

#ifndef INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_
#define INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_

#include "AE/Svfexe/AbstractInterpretation.h"

namespace SVF
{

/// Abstract Interpretation driver for sparse modes (currently semi-sparse).
///
/// ValVars whose def-site is inside a cycle but NOT at cycle_head do not
/// flow through cycle_head's merge in semi-sparse mode, so the around-merge
/// widening cannot observe them.  This subclass overrides the three cycle
/// helpers in AbstractInterpretation:
///
///   * getFullCycleHeadState   — in addition to the base's trace read,
///                               pulls cycle ValVars from their def-sites
///                               into the transient AbstractState.
///   * widenCycleState         — after the base widens and writes trace,
///                               scatters the widened ValVars back to their
///                               def-sites so body nodes observe them.
///   * narrowCycleState        — same scatter on non-fixpoint narrowing.
///
/// Selected once, at first call to AbstractInterpretation::getAEInstance(),
/// when Options::AESparsity() == SemiSparse.
class SparseAbstractInterpretation : public AbstractInterpretation
{
public:
    SparseAbstractInterpretation() = default;
    ~SparseAbstractInterpretation() override = default;

protected:
    AbstractState getFullCycleHeadState(const ICFGCycleWTO* cycle) override;

    bool widenCycleState(const AbstractState& prev,
                         const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;

    bool narrowCycleState(const AbstractState& prev,
                          const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
