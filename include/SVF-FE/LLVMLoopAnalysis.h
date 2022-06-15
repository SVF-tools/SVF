//===- LLVMLoopAnalysis.h -- LoopAnalysis of SVF --------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
 * LLVMLoopAnalysis.h
 *
 *  Created on: 14, 06, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 */

#ifndef SVF_LLVMLOOPANALYSIS_H
#define SVF_LLVMLOOPANALYSIS_H

#include "MemoryModel/SVFIR.h"
#include "MemoryModel/SVFLoop.h"

namespace SVF
{
class LLVMLoopAnalysis
{
public:

    /// Constructor
    LLVMLoopAnalysis() = default;;

    /// Destructor
    virtual ~LLVMLoopAnalysis() = default;

    /// Build llvm loops based on LoopInfo analysis
    virtual void buildLLVMLoops(SVFModule *mod, std::vector<const Loop *> &llvmLoops);

    /// Start from here
    virtual void build(ICFG *icfg);

    /// Build SVF loops based on llvm loops
    virtual void buildSVFLoops(ICFG *icfg, std::vector<const Loop *> &llvmLoops);
};
} // end fo SVF

#endif //SVF_LLVMLOOPANALYSIS_H
