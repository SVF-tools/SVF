//===- CFBasicBlockGBuilder.h ----------------------------------------------------------------//
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
 * CFBasicBlockGBuilder.h
 *
 *  Created on: 17 Oct. 2023
 *      Author: Xiao, Jiawei
 */

#include "Graphs/CFBasicBlockG.h"

namespace SVF
{

class CFBasicBlockGBuilder
{

private:
    CFBasicBlockGraph* _CFBasicBlockG;

public:
    CFBasicBlockGBuilder() : _CFBasicBlockG() {}

    virtual void build(ICFG* icfg);

    inline CFBasicBlockGraph* getCFBasicBlockGraph()
    {
        return _CFBasicBlockG;
    }
private:
    void initCFBasicBlockGNodes(ICFG *icfg, Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes);

    void addInterBBEdge(ICFG *icfg, Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes);

    void addIntraBBEdge(ICFG *icfg, Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes);

    void addInterProceduralEdge(ICFG *icfg, Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes);
};
}