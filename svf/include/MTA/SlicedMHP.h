// MSli: MHP running on a sliced ICFG view.
// Inherits from SVF::MHP and overrides ICFG traversal methods to use SlicedICFGView.
#pragma once

// Include WorkList.h before TCT.h because TCT.h includes SCC.h which uses FIFOWorkList
// Include SVFVariables.h before MHP.h because MHP.h uses ValVar which needs complete definition
#include <Util/WorkList.h>
#include <SVFIR/SVFVariables.h>
#include <Util/SVFUtil.h>
#include <MTA/MHP.h>
#include <MTA/TCT.h>

// Forward declarations
class SlicedSVFIRView;
class SlicedICFGView;

#include <memory>
#include <unordered_set>
#include <vector>

/**
 * SlicedMHP
 *
 * Inherits from SVF::MHP and overrides ICFG traversal methods to use SlicedICFGView.
 *
 * Design goals:
 * - Inherit from SVF::MHP to reuse existing implementation
 * - Override virtual ICFG traversal methods to use sliced view semantics
 * - Keep using original SVF::ICFGNode pointers as identities
 */
class SlicedMHP final : public SVF::MHP {
public:
    struct Stats {
        size_t worklistPops = 0;
        size_t uniqueVisitedICFGNodes = 0;
        size_t seedEntryNotKept = 0;

        size_t handleIntraCalls = 0;
        size_t handleRetCalls = 0;
        size_t handleCallCalls = 0;
        size_t handleForkCalls = 0;
        size_t handleJoinCalls = 0;
        size_t handleNonCandidateCalls = 0;
    };

    /// If slicedView is provided, it will be used to access sliced ICFG/CallGraph/ThreadCallGraph views.
    /// If slicedView is null, uses full ICFG from SVFIR.
    explicit SlicedMHP(SVF::TCT* tct, const SlicedSVFIRView* slicedView = nullptr);
    ~SlicedMHP() override;

    // Override ICFG traversal methods to use sliced view
protected:
    // Not in base class, but we can use it internally
    const SVF::ICFGNode* getFunEntry(const SVF::FunObjVar* fun) const;
    void getFunICFGNodes(const SVF::FunObjVar* fun, std::vector<const SVF::ICFGNode*>& out) const;
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;
    bool acceptsNode(const SVF::ICFGNode* node) const;
    void getInEdgesOfCallGraphNode(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const;

    // Optionally override handlers if needed
    void handleIntra(const SVF::CxtThreadStmt& cts) override;
    void handleCall(const SVF::CxtThreadStmt& cts, SVF::NodeID rootTid) override;
    void handleRet(const SVF::CxtThreadStmt& cts) override;
    void handleFork(const SVF::CxtThreadStmt& cts, SVF::NodeID rootTid) override;
    void handleJoin(const SVF::CxtThreadStmt& cts, SVF::NodeID rootTid) override;

public:

    const Stats& getStats() const { return stats; }

private:
    const SlicedSVFIRView* slicedView; // Optional: for accessing sliced views
    const SlicedICFGView* icfgView; // ICFG view (from slicedView or nullptr for full ICFG)

    std::unique_ptr<class SlicedForkJoinAnalysis> fja;

    // Additional helper methods (if needed beyond base class)
protected:
    void handleNonCandidateFun(const SVF::CxtThreadStmt& cts);
    void updateNonCandidateFunInterleaving();
    
    // Override analyzeInterleaving to track statistics
    void analyzeInterleaving() override;
    
    // Override pushCxt to use SlicedTCT's pushCxt if available
    // This hides the base class inline pushCxt method
    void pushCxt(SVF::CallStrCxt& cxt, const SVF::CallICFGNode* call, const SVF::FunObjVar* callee);

    // debug stats
    Stats stats{};
    std::unordered_set<const SVF::ICFGNode*> visitedNodes;
};

// Forward declaration
class SlicedICFGView;
class SlicedMHP;

/**
 * SlicedForkJoinAnalysis
 *
 * Inherits from SVF::MHP::ForkJoinAnalysis and overrides ICFG traversal methods
 * to use SlicedICFGView.
 */
class SlicedForkJoinAnalysis : public SVF::ForkJoinAnalysis {
public:
    SlicedForkJoinAnalysis(SVF::TCT* tct, const SlicedICFGView* icfgView, const SlicedMHP* parent = nullptr);

    // Note: analyzeForkJoinPair is not virtual in base class
    // We'll need to make it virtual in SVF, or override handle methods instead

protected:
    // Note: getFunEntry is not in base class, but we can use it internally
    const SVF::ICFGNode* getFunEntry(const SVF::FunObjVar* fun) const;
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;
    bool acceptsNode(const SVF::ICFGNode* node) const;
    
    // Override handlers to use sliced ICFG traversal methods
    // Note: These are virtual in base class, but they access private members
    // We'll provide minimal overrides that call base class
    void handleFork(const SVF::CxtStmt& cts, SVF::NodeID rootTid) override;
    void handleJoin(const SVF::CxtStmt& cts, SVF::NodeID rootTid) override;
    void handleCall(const SVF::CxtStmt& cts, SVF::NodeID rootTid) override;
    void handleRet(const SVF::CxtStmt& cts) override;
    void handleIntra(const SVF::CxtStmt& cts) override;

private:
    const SlicedICFGView* icfgView; // For accessing sliced ICFG view
    const SlicedMHP* parent; // For accessing parent's sliced view methods
};

