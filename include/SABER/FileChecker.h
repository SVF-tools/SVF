//===- FileChecker.h -- Checking incorrect file-open close errors-------------//
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
 * FileChecker.h
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */

#ifndef FILECHECK_H_
#define FILECHECK_H_

#include "SABER/LeakChecker.h"

/*!
 * File open/close checker to check consistency of file operations
 */

class FileChecker : public LeakChecker {

public:

    /// Pass ID
    static char ID;

    /// Constructor
    FileChecker(char id = ID): LeakChecker(ID) {
    }

    /// Destructor
    virtual ~FileChecker() {
    }

    /// We start from here
    virtual bool runOnModule(llvm::Module& module) {
        /// start analysis
        analyze(module);
        return false;
    }

    /// Get pass name
    virtual llvm::StringRef getPassName() const {
        return "File Open/Close Analysis";
    }

    /// Pass dependence
    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const {
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    inline bool isSourceLikeFun(const llvm::Function* fun) {
        return SaberCheckerAPI::getCheckerAPI()->isFOpen(fun);
    }
    /// Whether the function is a heap deallocator (free/release memory)
    inline bool isSinkLikeFun(const llvm::Function* fun) {
        return SaberCheckerAPI::getCheckerAPI()->isFClose(fun);
    }
    /// Report file/close bugs
    void reportBug(ProgSlice* slice);
    void reportNeverClose(const SVFGNode* src);
    void reportPartialClose(const SVFGNode* src);
};


#endif /* FILECHECK_H_ */
