//===- DoubleFreeChecker.h -- Checking double-free errors---------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * DoubleFreeChecker.h
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */

#ifndef DOUBLEFREECHECKER_H_
#define DOUBLEFREECHECKER_H_

#include "SABER/LeakChecker.h"

/*!
 * Double free checker to check deallocations of memory
 */

class DoubleFreeChecker : public LeakChecker {

public:
    /// Pass ID
    static char ID;

    /// Constructor
    DoubleFreeChecker(char id = ID): LeakChecker(ID) {
    }

    /// Destructor
    virtual ~DoubleFreeChecker() {
    }

    /// We start from here
    virtual bool runOnModule(llvm::Module& module) {
        /// start analysis
        analyze(module);
        return false;
    }

    /// Get pass name
    virtual const char* getPassName() const {
        return "Double Free Analysis";
    }

    /// Pass dependence
    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const {
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    /// Report file/close bugs
    void reportBug(ProgSlice* slice);
};

#endif /* DOUBLEFREECHECKER_H_ */
