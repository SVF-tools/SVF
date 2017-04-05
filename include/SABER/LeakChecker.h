//===- LeakChecker.h -- Detecting memory leaks--------------------------------//
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
 * LeakChecker.h
 *
 *  Created on: Apr 1, 2014
 *      Author: rockysui
 */

#ifndef LEAKCHECKER_H_
#define LEAKCHECKER_H_

#include "SABER/SrcSnkDDA.h"
#include "SABER/SaberCheckerAPI.h"

/*!
 * Static Memory Leak Detector
 */
class LeakChecker : public SrcSnkDDA, public llvm::ModulePass {

public:
    typedef std::map<const SVFGNode*,llvm::CallSite> SVFGNodeToCSIDMap;
    typedef FIFOWorkList<llvm::CallSite> CSWorkList;
    typedef ProgSlice::VFWorkList WorkList;
    typedef NodeBS SVFGNodeBS;
    typedef PAG::CallSiteSet CallSiteSet;
    enum LEAK_TYPE {
        NEVER_FREE_LEAK,
        CONTEXT_LEAK,
        PATH_LEAK,
        GLOBAL_LEAK
    };

    /// Pass ID
    static char ID;

    /// Constructor
    LeakChecker(char id = ID): ModulePass(ID) {
    }
    /// Destructor
    virtual ~LeakChecker() {
    }

    /// We start from here
    virtual bool runOnModule(llvm::Module& module) {
        /// start analysis
        analyze(module);
        return false;
    }

    /// Get pass name
    virtual llvm::StringRef getPassName() const {
        return "Static Memory Leak Analysis";
    }

    /// Pass dependence
    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const {
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
    }

    /// Initialize sources and sinks
    //@{
    /// Initialize sources and sinks
    virtual void initSrcs();
    virtual void initSnks();
    /// Whether the function is a heap allocator/reallocator (allocate memory)
    virtual inline bool isSourceLikeFun(const llvm::Function* fun) {
        return SaberCheckerAPI::getCheckerAPI()->isMemAlloc(fun);
    }
    /// Whether the function is a heap deallocator (free/release memory)
    virtual inline bool isSinkLikeFun(const llvm::Function* fun) {
        return SaberCheckerAPI::getCheckerAPI()->isMemDealloc(fun);
    }
    /// Identify allocation wrappers
    bool isInAWrapper(const SVFGNode* src, CallSiteSet& csIdSet);
    /// A SVFG node is source if it is an actualRet at malloc site
    inline bool isSouce(const SVFGNode* node) {
        return getSources().find(node)!=getSources().end();
    }
    /// A SVFG node is source if it is an actual parameter at dealloca site
    inline bool isSink(const SVFGNode* node) {
        return getSinks().find(node)!=getSinks().end();
    }
    //@}

protected:
    /// Get PAG
    PAG* getPAG() const {
        return PAG::getPAG();
    }
    /// Report leaks
    //@{
    virtual void reportBug(ProgSlice* slice);
    void reportNeverFree(const SVFGNode* src);
    void reportPartialLeak(const SVFGNode* src);
    //@}

    /// Validate test cases for regression test purpose
    void testsValidation(const ProgSlice* slice);
    void validateSuccessTests(const SVFGNode* source, const llvm::Function* fun);
    void validateExpectedFailureTests(const SVFGNode* source, const llvm::Function* fun);

    /// Record a source to its callsite
    //@{
    inline void addSrcToCSID(const SVFGNode* src, llvm::CallSite cs) {
        srcToCSIDMap[src] = cs;
    }
    inline llvm::CallSite getSrcCSID(const SVFGNode* src) {
        SVFGNodeToCSIDMap::iterator it =srcToCSIDMap.find(src);
        assert(it!=srcToCSIDMap.end() && "source node not at a callsite??");
        return it->second;
    }
    //@}
private:
    SVFGNodeToCSIDMap srcToCSIDMap;
};

#endif /* LEAKCHECKER_H_ */
