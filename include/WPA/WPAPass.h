//===- WPAPass.h -- Whole program analysis------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * @file: WPA.h
 * @author: yesen
 * @date: 10/06/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#ifndef WPA_H_
#define WPA_H_

#include "MemoryModel/PointerAnalysis.h"
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Pass.h>


/*!
 * Whole program pointer analysis.
 * This class performs various pointer analysis on the given module.
 */
class WPAPass: public llvm::ModulePass, public llvm::AliasAnalysis {
    typedef std::vector<PointerAnalysis*> PTAVector;

public:
    /// Pass ID
    static char ID;

    enum AliasCheckRule {
        Conservative,	///< return MayAlias if any pta says alias
        Veto,			///< return NoAlias if any pta says no alias
        Precise			///< return alias result by the most precise pta
    };

    /// Constructor
    WPAPass() : llvm::ModulePass(ID), llvm::AliasAnalysis() {}
    /// Destructor
    ~WPAPass();

    /// LLVM analysis usage
    virtual inline void getAnalysisUsage(llvm::AnalysisUsage &au) const {
        // declare your dependencies here.
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    /// Get adjusted analysis for alias analysis
    virtual inline void* getAdjustedAnalysisPointer(llvm::AnalysisID id) {
        return this;
    }

    /// Interface expose to users of our pointer analysis, given Location infos
    virtual inline llvm::AliasResult alias(const llvm::MemoryLocation  &LocA, const llvm::MemoryLocation  &LocB) {
        return alias(LocA.Ptr, LocB.Ptr);
    }

    /// Interface expose to users of our pointer analysis, given Value infos
    virtual llvm::AliasResult alias(const llvm::Value* V1,	const llvm::Value* V2);

    /// We start from here
    virtual bool runOnModule(llvm::Module& module);

    /// PTA name
    virtual inline const char* getPassName() const {
        return "WPAPass";
    }

private:
    /// Create pointer analysis according to specified kind and analyze the module.
    void runPointerAnalysis(llvm::Module& module, u32_t kind);

    PTAVector ptaVector;	///< all pointer analysis to be executed.
    PointerAnalysis* _pta;	///<  pointer analysis to be executed.
};


#endif /* WPA_H_ */
