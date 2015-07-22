//===- SaberAnnotator.cpp -- Annotation---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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

using namespace llvm;

/*!
 *
 */
void SaberAnnotator::annotateSource() {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_SLICESOURCE ; //<< _curSlice->getSource()->getId();
    if(const Instruction* sourceinst = dyn_cast<Instruction>(_curSlice->getLLVMValue(_curSlice->getSource()))) {
        addMDTag(const_cast<Instruction*>(sourceinst),rawstr.str());
    }
    else
        assert(false && "instruction of source node not found");

}

/*!
 *
 */
void SaberAnnotator::annotateSinks() {
    for(ProgSlice::SVFGNodeSet::iterator it = _curSlice->getSinks().begin(),
            eit = _curSlice->getSinks().end(); it!=eit; ++it) {
        if(const ActualParmSVFGNode* ap = dyn_cast<ActualParmSVFGNode>(*it)) {
            const Instruction* sinkinst = ap->getCallSite().getInstruction();
            assert(isa<CallInst>(sinkinst) && "not a call instruction?");
            const CallInst* sink = cast<CallInst>(sinkinst);
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
void SaberAnnotator::annotateFeasibleBranch(const BranchInst *brInst, u32_t succPos) {

    assert((succPos == 0 || succPos == 1) && "branch instruction should only have two successors");

    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_FESIBLE << _curSlice->getSource()->getId();
    BranchInst* br = const_cast<BranchInst*>(brInst);
    addMDTag(br,br->getCondition(),rawstr.str());
}

/*!
 * Annotate branch instruction and its corresponding infeasible path
 */
void SaberAnnotator::annotateInfeasibleBranch(const BranchInst *brInst, u32_t succPos) {

    assert((succPos == 0 || succPos == 1) && "branch instruction should only have two successors");

    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << SB_INFESIBLE << _curSlice->getSource()->getId();
    BranchInst* br = const_cast<BranchInst*>(brInst);
    addMDTag(br,br->getCondition(),rawstr.str());
}


/*!
 * Annotate switch instruction and its corresponding feasible path
 */
void SaberAnnotator::annotateSwitch(SwitchInst *switchInst, u32_t succPos) {
    assert(succPos < switchInst->getNumSuccessors() && "successor position not correct!");
}




