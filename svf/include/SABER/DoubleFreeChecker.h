//===- DoubleFreeChecker.h -- Checking double-free errors---------------------//
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
 * DoubleFreeChecker.h
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */

#ifndef DOUBLEFREECHECKER_H_
#define DOUBLEFREECHECKER_H_

#include "SABER/LeakChecker.h"

namespace SVF
{

/*!
 * Double free checker to check deallocations of memory
 */

class DoubleFreeChecker : public LeakChecker
{

public:
    /// Constructor
    DoubleFreeChecker(): LeakChecker()
    {
    }

    /// Destructor
    virtual ~DoubleFreeChecker()
    {
    }

    /// We start from here
    virtual bool runOnModule(SVFIR* pag) override
    {
        /// start analysis
        analyze(pag->getModule());
        return false;
    }

    /// Report file/close bugs
    void reportBug(ProgSlice* slice) override;


    /// Validate test cases for regression test purpose
    void testsValidation(ProgSlice* slice);
    void validateSuccessTests(ProgSlice* slice, const SVFFunction* fun);
    void validateExpectedFailureTests(ProgSlice* slice, const SVFFunction* fun);
};

} // End namespace SVF

#endif /* DOUBLEFREECHECKER_H_ */
