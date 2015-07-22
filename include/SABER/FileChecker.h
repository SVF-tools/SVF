//===- FileChecker.h -- Checking incorrect file-open close errors-------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
    virtual const char* getPassName() const {
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
