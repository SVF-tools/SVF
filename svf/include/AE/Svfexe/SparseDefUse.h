//===- SparseDefUse.h -- Sparse Def-Use Table for Abstract Interpretation-//
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
 * SparseDefUse.h
 *
 *  Created on: Feb 9, 2026
 *      Author: Jiawei Wang
 *
 * Simple def-use table for address-taken objects.
 * Maps each ObjVar to the ICFG nodes that store to it (via StoreStmt).
 * Top-level variables don't need this table since ValVar::getICFGNode()
 * gives their single definition point in SSA form.
 */

#ifndef INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_
#define INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_

#include "SVFIR/SVFIR.h"
#include "Graphs/ICFG.h"
#include "MemoryModel/PointerAnalysis.h"

namespace SVF
{

class SparseDefUse
{
public:
    SparseDefUse(PointerAnalysis* pta)
        : pta(pta) {}

    ~SparseDefUse() = default;

    /// Build the table by scanning all ICFG nodes for StoreStmts
    void build(ICFG* icfg);

    /// Get the ICFG nodes that may define an ObjVar (via store)
    const std::vector<const ICFGNode*>& getDefICFGNodes(NodeID objId) const
    {
        auto it = objToDefNodes.find(objId);
        if (it != objToDefNodes.end())
            return it->second;
        return emptyVec;
    }

private:
    PointerAnalysis* pta;
    Map<NodeID, std::vector<const ICFGNode*>> objToDefNodes;
    static const std::vector<const ICFGNode*> emptyVec;
};

} // End namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_ */
