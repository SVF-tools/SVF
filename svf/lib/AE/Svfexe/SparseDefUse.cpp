//===- SparseDefUse.cpp -- Sparse Def-Use Table Implementation------------//
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
 * SparseDefUse.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: Jiawei Wang
 */

#include "AE/Svfexe/SparseDefUse.h"

using namespace SVF;

const std::vector<const ICFGNode*> SparseDefUse::emptyVec;

void SparseDefUse::build(ICFG* icfg)
{
    // Single pass: scan every ICFG node for StoreStmts.
    // For each store, get pts(LHS pointer) and add node to each object's def list.
    for (auto it = icfg->begin(); it != icfg->end(); ++it)
    {
        const ICFGNode* node = it->second;
        for (const SVFStmt* stmt : node->getSVFStmts())
        {
            if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
            {
                NodeID ptrId = store->getLHSVarID();
                const PointsTo& pts = pta->getPts(ptrId);
                for (NodeID objId : pts)
                {
                    objToDefNodes[objId].push_back(node);
                }
            }
        }
    }
}
