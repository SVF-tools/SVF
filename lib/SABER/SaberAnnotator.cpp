//===- SaberAnnotator.cpp -- Annotation---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SaberAnnotator.cpp
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui
 */

#include "SABER/SaberAnnotator.h"
#include "SABER/ProgSlice.h"

using namespace SVF;

/*!
 *
 */
void SaberAnnotator::annotateSource()
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_SLICESOURCE ; //<< _curSlice->getSource()->getId();
    if(const Instruction* sourceinst = SVFUtil::dyn_cast<Instruction>(_curSlice->getSource()->getValue()))
    {
        addMDTag(const_cast<Instruction*>(sourceinst),rawstr.str());
    }
    else
        assert(false && "instruction of source node not found");

}

/*!
 *
 */
void SaberAnnotator::annotateSinks()
{
    for(ProgSlice::SVFGNodeSet::const_iterator it = _curSlice->getSinks().begin(),
            eit = _curSlice->getSinks().end(); it!=eit; ++it)
    {
        if(const ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(*it))
        {
            const Instruction* sinkinst = ap->getCallSite()->getCallSite();
            assert(SVFUtil::isa<CallInst>(sinkinst) && "not a call instruction?");
            const CallInst* sink = SVFUtil::cast<CallInst>(sinkinst);
            std::string str;
            raw_string_ostream rawstr(str);
            rawstr << SB_SLICESINK << _curSlice->getSource()->getId();
            addMDTag(const_cast<CallInst*>(sink),sink->getArgOperand(0),rawstr.str());
        }
        else
            assert(false && "sink node is not a actual parameter?");
    }
}

/*!
 * Annotate branch instruction and its corresponding feasible path
 */
void SaberAnnotator::annotateFeasibleBranch(const BranchStmt *branchStmt, u32_t succPos)
{

    assert((succPos == 0 || succPos == 1) && "branch instruction should only have two successors");

    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_FESIBLE << _curSlice->getSource()->getId();
    addMDTag(const_cast<Instruction *>(branchStmt->getInst()),
             const_cast<Value *>(branchStmt->getCondition()->getValue()), rawstr.str());
}

/*!
 * Annotate branch instruction and its corresponding infeasible path
 */
void SaberAnnotator::annotateInfeasibleBranch(const BranchStmt *branchStmt, u32_t succPos)
{

    assert((succPos == 0 || succPos == 1) && "branch instruction should only have two successors");

    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_INFESIBLE << _curSlice->getSource()->getId();
    addMDTag(const_cast<Instruction *>(branchStmt->getInst()),
             const_cast<Value *>(branchStmt->getCondition()->getValue()), rawstr.str());
}


/*!
 * Annotate switch instruction and its corresponding feasible path
 */
void SaberAnnotator::annotateSwitch(const BranchStmt *swithStmt, u32_t succPos)
{
    assert(succPos < swithStmt->getNumSuccessors() && "successor position not correct!");
}




