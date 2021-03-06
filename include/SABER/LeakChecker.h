//===- LeakChecker.h -- Detecting memory leaks--------------------------------//
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
 * LeakChecker.h
 *
 *  Created on: Apr 1, 2014
 *      Author: rockysui
 */

#ifndef LEAKCHECKER_H_
#define LEAKCHECKER_H_

#include "SABER/SrcSnkDDA.h"
#include "SABER/SaberCheckerAPI.h"

namespace SVF
{

/*!
 * Static Memory Leak Detector
 */
class LeakChecker : public SrcSnkDDA
{

public:
    using SVFGNodeToCSIDMap = Map<const SVFGNode *, const CallBlockNode *>;
    using CSWorkList = FIFOWorkList<const CallBlockNode *>;
    using WorkList = ProgSlice::VFWorkList;
    using SVFGNodeBS = NodeBS;
    enum LEAK_TYPE
    {
        NEVER_FREE_LEAK,
        CONTEXT_LEAK,
        PATH_LEAK,
        GLOBAL_LEAK
    };

    /// Constructor
    LeakChecker()
    {
    }
    /// Destructor
    virtual ~LeakChecker()
    {
    }

    /// We start from here
    virtual bool runOnModule(SVFModule* module)
    {
        /// start analysis
        analyze(module);
        return false;
    }

    /// Initialize sources and sinks
    //@{
    /// Initialize sources and sinks
    void initSrcs() override;
    void initSnks() override;
    /// Whether the function is a heap allocator/reallocator (allocate memory)
    inline bool isSourceLikeFun(const SVFFunction* fun) override
    {
        return SaberCheckerAPI::getCheckerAPI()->isMemAlloc(fun);
    }
    /// Whether the function is a heap deallocator (free/release memory)
    inline bool isSinkLikeFun(const SVFFunction* fun) override
    {
        return SaberCheckerAPI::getCheckerAPI()->isMemDealloc(fun);
    }
    /// A SVFG node is source if it is an actualRet at malloc site
    inline bool isSource(const SVFGNode* node) override
    {
        return getSources().find(node)!=getSources().end();
    }
    /// A SVFG node is source if it is an actual parameter at dealloca site
    inline bool isSink(const SVFGNode* node) override
    {
        return getSinks().find(node)!=getSinks().end();
    }
    //@}

protected:
    /// Report leaks
    //@{
    void reportBug(ProgSlice* slice) override;
    void reportNeverFree(const SVFGNode* src);
    void reportPartialLeak(const SVFGNode* src);
    //@}

    /// Validate test cases for regression test purpose
    void testsValidation(const ProgSlice* slice);
    void validateSuccessTests(const SVFGNode* source, const SVFFunction* fun);
    void validateExpectedFailureTests(const SVFGNode* source, const SVFFunction* fun);

    /// Record a source to its callsite
    //@{
    inline void addSrcToCSID(const SVFGNode* src, const CallBlockNode* cs)
    {
        srcToCSIDMap[src] = cs;
    }
    inline const CallBlockNode* getSrcCSID(const SVFGNode* src)
    {
        auto it =srcToCSIDMap.find(src);
        assert(it!=srcToCSIDMap.end() && "source node not at a callsite??");
        return it->second;
    }
    //@}
private:
    SVFGNodeToCSIDMap srcToCSIDMap;
};

} // End namespace SVF

#endif /* LEAKCHECKER_H_ */
