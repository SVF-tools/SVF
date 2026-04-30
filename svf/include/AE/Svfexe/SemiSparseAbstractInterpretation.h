//===- SemiSparseAbstractInterpretation.h -- Semi-Sparse Mode-----------//
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

#ifndef INCLUDE_AE_SVFEXE_SEMISPARSEABSTRACTINTERPRETATION_H_
#define INCLUDE_AE_SVFEXE_SEMISPARSEABSTRACTINTERPRETATION_H_

#include "AE/Svfexe/SparseAbstractInterpretation.h"

namespace SVF
{

/// Abstract Interpretation for `Options::AESparsity::SemiSparse`.
///
/// In semi-sparse mode ValVars live at their SVFG def-sites while
/// ObjVars are still propagated through `trace[node]`.  The cycle
/// helpers in `SparseAbstractInterpretation` already implement exactly
/// this behaviour (pull cycle ValVars from def-sites, scatter widened
/// values back).  This class therefore inherits the helpers as-is and
/// only exists as the concrete type instantiated by the factory when
/// `Options::AESparsity() == SemiSparse`, mirroring the
/// `FullSparseAbstractInterpretation` sibling.
class SemiSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    SemiSparseAbstractInterpretation() = default;
    ~SemiSparseAbstractInterpretation() override;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SEMISPARSEABSTRACTINTERPRETATION_H_ */
