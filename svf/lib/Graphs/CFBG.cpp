//===- CFBG.cpp ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * CFBG.cpp
 *
 *  Created on: 24 Dec. 2022
 *      Author: Xiao, Jiawei
 */

#include "Graphs/CFBG.h"
#include "SVFIR/SVFIR.h"

namespace SVF{
CFBGNode::CFBGNode(u32_t id, const SVFBasicBlock *svfBasicBlock) : GenericCFBGNodeTy(id, 0),
_svfBasicBlock(svfBasicBlock) {
        for (auto it = svfBasicBlock->begin(); it != svfBasicBlock->end(); ++it) {
            const SVFInstruction *ins = *it;
            ICFGNode *icfgNode = PAG::getPAG()->getICFG()->getICFGNode(ins);
            _icfgNodes.push_back(icfgNode);
        }
}


inline void CFBGNode::setSVFBasicBlock(const SVFBasicBlock *svfBasicBlock) {
    _icfgNodes.clear();
    _svfBasicBlock = svfBasicBlock;
    for (auto it = svfBasicBlock->begin(); it != svfBasicBlock->end(); ++it) {
        const SVFInstruction *ins = *it;
        ICFGNode *icfgNode = PAG::getPAG()->getICFG()->getICFGNode(ins);
        _icfgNodes.push_back(icfgNode);
    }
}
}

