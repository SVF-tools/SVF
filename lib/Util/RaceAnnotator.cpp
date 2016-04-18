/*
 * RaceAnnotator.cpp
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/RaceAnnotator.h"
#include <sstream>
#include <llvm/Support/CommandLine.h>	// for llvm command line options

using namespace llvm;

void RaceAnnotator::annotateDRCheck(Instruction* inst) {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << DR_CHECK;

    /// memcpy and memset is not annotated
    if (StoreInst* st = dyn_cast<StoreInst>(inst)) {
        addMDTag(inst, st->getPointerOperand(), rawstr.str());
    } else if (LoadInst* ld = dyn_cast<LoadInst>(inst)) {
        addMDTag(inst, ld->getPointerOperand(), rawstr.str());
    }
}
