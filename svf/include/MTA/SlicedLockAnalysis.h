// MSli: LockAnalysis running on a sliced ICFG view.
// Inherits from SVF::LockAnalysis and overrides ICFG traversal methods to use SlicedICFGView.
#pragma once

// Include WorkList.h before TCT.h because TCT.h includes SCC.h which uses FIFOWorkList
#include <Util/WorkList.h>
#include <MTA/LockAnalysis.h>
#include <MTA/TCT.h>

// Forward declarations
class SlicedSVFIRView;
class SlicedICFGView;

#include <memory>
#include <vector>

/**
 * SlicedLockAnalysis
 *
 * Inherits from SVF::LockAnalysis and overrides ICFG traversal methods to use SlicedICFGView.
 *
 * Design goals:
 * - Inherit from SVF::LockAnalysis to reuse existing implementation
 * - Override virtual ICFG traversal methods to use sliced view semantics
 * - Keep using original SVF::ICFGNode pointers as identities
 */
class SlicedLockAnalysis final : public SVF::LockAnalysis {
public:
    struct Stats {
        size_t locksites = 0;
        size_t unlocksites = 0;
        size_t candidateFuncs = 0;
        size_t intraLocks = 0;
        size_t cxtLocks = 0;
        size_t cxtStmtVisited = 0;
    };

    /// If slicedView is provided, it will be used to access sliced ICFG/CallGraph/ThreadCallGraph views.
    /// If slicedView is null, uses full ICFG from SVFIR.
    explicit SlicedLockAnalysis(SVF::TCT* tct, const SlicedSVFIRView* slicedView = nullptr);
    ~SlicedLockAnalysis() = default;

    // Override analyze() to call our own versions of non-virtual methods
    void analyze();

    // Override ICFG traversal methods to use sliced view
protected:
    // Note: These methods are not virtual in base class, but we declare them for internal use
    const SVF::ICFGNode* getFunEntry(const SVF::FunObjVar* fun) const;
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;
    void getPredNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;
    bool acceptsNode(const SVF::ICFGNode* node) const;
    void getInEdgesOfCallGraphNode(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const;

    // Note: These handlers are not virtual in base class, but we declare them for internal use
    void handleIntra(const SVF::CxtStmt& cts);
    void handleCall(const SVF::CxtStmt& cts);
    void handleRet(const SVF::CxtStmt& cts);
    void handleFork(const SVF::CxtStmt& cts);

public:
    const Stats& getStats() const { return stats; }

private:
    using InstSet = SVF::LockAnalysis::InstSet;
    using InstVec = SVF::LockAnalysis::InstVec;
    using FunSet = SVF::LockAnalysis::FunSet;
    using CILockToSpan = SVF::LockAnalysis::CILockToSpan;
    using InstToInstSetMap = SVF::LockAnalysis::InstToInstSetMap;
    using CxtLock = SVF::LockAnalysis::CxtLock;
    using CxtLockSet = SVF::LockAnalysis::CxtLockSet;
    using LockSpan = SVF::LockAnalysis::LockSpan;
    using CxtStmtSet = SVF::LockAnalysis::CxtStmtSet;
    using InstToCxtStmtSet = SVF::LockAnalysis::InstToCxtStmtSet;
    using CxtStmtToCxtLockSet = SVF::LockAnalysis::CxtStmtToCxtLockSet;
    using CxtLockToSpan = SVF::LockAnalysis::CxtLockToSpan;
    using CxtLockProc = SVF::LockAnalysis::CxtLockProc;
    using CxtLockProcVec = SVF::LockAnalysis::CxtLockProcVec;
    using CxtLockProcSet = SVF::LockAnalysis::CxtLockProcSet;
    using CxtStmtWorkList = SVF::LockAnalysis::CxtStmtWorkList;

    void collectLockUnlocksites();
    void buildCandidateFuncSetforLock();
    void analyzeIntraProcedualLock();
    bool intraForwardTraverse(const SVF::ICFGNode* lock, InstSet& unlockset, InstSet& forwardInsts);
    bool intraBackwardTraverse(const InstSet& unlockset, InstSet& backwardInsts);

    void collectCxtLock();
    void analyzeLockSpanCxtStmt();

    // worklist helpers (same semantics as SVF)
    bool pushToCTPWorkList(const CxtLockProc& clp);
    CxtLockProc popFromCTPWorkList();
    bool isVisitedCTPs(const CxtLockProc& clp) const;

    bool pushToCTSWorkList(const SVF::CxtStmt& cs);
    SVF::CxtStmt popFromCTSWorkList();

    // CFG traversal handlers (declared above in protected section, no need to redeclare)
    void handleCallRelation(CxtLockProc& clp, const SVF::CallGraphEdge* cgEdge, const SVF::CallICFGNode* call);

    // Context helpers
    void pushCxt(SVF::CallStrCxt& cxt, const SVF::CallICFGNode* call, const SVF::FunObjVar* callee);
    bool matchCxt(SVF::CallStrCxt& cxt, const SVF::CallICFGNode* call, const SVF::FunObjVar* callee);
    bool isContextSuffix(const SVF::CallStrCxt& lhs, const SVF::CallStrCxt& call) const;

    // Lock helpers
    bool isTDFork(const SVF::ICFGNode* call) const;
    bool isTDAcquire(const SVF::ICFGNode* call) const;
    bool isTDRelease(const SVF::ICFGNode* call) const;
    bool isCallSite(const SVF::ICFGNode* inst) const;
    bool isExtCall(const SVF::ICFGNode* inst) const;
    const SVF::SVFVar* getLockVal(const SVF::ICFGNode* call) const;
    SVF::ThreadCallGraph* getTCG() const;

    bool isAliasedLocks(const SVF::ICFGNode* i1, const SVF::ICFGNode* i2) const;

    // cxt-lock set operations (copied from SVF)
    void addCxtLock(const SVF::CallStrCxt& cxt, const SVF::ICFGNode* inst);
    bool hasCxtLock(const CxtLock& cxtLock) const;

    bool addCxtStmtToSpan(const SVF::CxtStmt& cts, const CxtLock& cl);
    bool removeCxtStmtToSpan(SVF::CxtStmt& cts, const CxtLock& cl);
    void touchCxtStmt(SVF::CxtStmt& cts);

    void markCxtStmtFlag(const SVF::CxtStmt& tgr, const SVF::CxtStmt& src);
    bool intersect(CxtLockSet& tgrlockset, const CxtLockSet& srclockset);

private:
    const SlicedSVFIRView* slicedView; // Optional: for accessing sliced views
    const SlicedICFGView* icfgView; // ICFG view (from slicedView or nullptr for full ICFG)

    // Note: Most member variables should be inherited from base class
    // Only keep additional ones if needed
    Stats stats{};
};

