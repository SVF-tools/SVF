//===- ICFGSimplification.h -- Simplify ICFG----------------------------------//
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
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)
//===----------------------------------------------------------------------===//


//
// Created by Jiawei Wang on 2024/2/25.
//
#include "AE/Svfexe/SVFIR2AbsState.h"
#include "Graphs/ICFG.h"

namespace SVF
{

class ICFGSimplification
{
public:
    ICFGSimplification() = default;

    virtual ~ICFGSimplification() = default;

    static void mergeAdjacentNodes(ICFG* icfg);
};
}