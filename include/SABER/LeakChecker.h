//===- LeakChecker.h -- Detecting memory leaks--------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
    virtual const char* getPassName() const {
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
