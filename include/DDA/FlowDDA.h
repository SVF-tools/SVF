/*
 * FlowDDA.h
 *
 *  Created on: Jun 30, 2014
 *      Author: Yulei Sui, Sen Ye
 */

#ifndef FlowDDA_H_
#define FlowDDA_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "Util/DPItem.h"
#include "DDA/DDAVFSolver.h"

class DDAClient;
typedef StmtDPItem<SVFGNode> LocDPItem;

/*!
 * Flow sensitive demand-driven analysis on value-flow graph
 */
class FlowDDA : public BVDataPTAImpl, public DDAVFSolver<NodeID,PointsTo,LocDPItem> {

public:
    typedef BVDataPTAImpl::CallSiteSet CallSiteSet;
    typedef BVDataPTAImpl::CallEdgeMap	CallEdgeMap;
    typedef BVDataPTAImpl::FunctionSet	FunctionSet;
    /// Constructor
    FlowDDA(SVFModule* m, DDAClient* client): BVDataPTAImpl(PointerAnalysis::FlowS_DDA),
        DDAVFSolver<NodeID,PointsTo,LocDPItem>(),
        _client(client) {
    }
    /// Destructor
    inline virtual ~FlowDDA() {
    }
    /// dummy analyze method
    virtual void analyze(SVFModule* mod) {}

    /// Compute points-to set for all top variable
    void computeDDAPts(NodeID id);

    /// Handle out-of-budget dpm
    void handleOutOfBudgetDpm(const LocDPItem& dpm);

    /// Handle condition for flow analysis (backward analysis)
    virtual bool handleBKCondition(LocDPItem& dpm, const SVFGEdge* edge);

    /// refine indirect call edge
    bool testIndCallReachability(LocDPItem& dpm, const Function* callee, CallSiteID csId);

    /// Initialization of the analysis
    inline virtual void initialize(SVFModule* module) {
        BVDataPTAImpl::initialize(module);
        buildSVFG(module);
        setCallGraph(getPTACallGraph());
        setCallGraphSCC(getCallGraphSCC());
        stat = setDDAStat(new DDAStat(this));
    }

    /// Finalize analysis
    inline virtual void finalize() {
        BVDataPTAImpl::finalize();
    }

    /// we exclude concrete heap here following the conditions:
    /// (1) local allocated heap and
    /// (2) not escaped to the scope outside the current function
    /// (3) not inside loop
    /// (4) not involved in recursion
    bool isHeapCondMemObj(const NodeID& var, const StoreSVFGNode* store);

    /// Override parent method
    inline PointsTo getConservativeCPts(const LocDPItem& dpm) {
        return getAndersenAnalysis()->getPts(dpm.getCurNodeID());
    }
    /// Override parent method
    virtual inline NodeID getPtrNodeID(const NodeID& var) const {
        return var;
    }
    /// Handle Address SVFGNode to add proper points-to
    inline void handleAddr(PointsTo& pts,const LocDPItem& dpm,const AddrSVFGNode* addr) {
        NodeID srcID = addr->getPAGSrcNodeID();
        /// whether this object is set field-insensitive during pre-analysis
        if (isFieldInsensitive(srcID))
            srcID = getFIObjNode(srcID);

        addDDAPts(pts,srcID);
        DBOUT(DDDA, SVFUtil::outs() << "\t add points-to target " << srcID << " to dpm ");
        DBOUT(DDDA, dpm.dump());
    }
    /// processGep node
    PointsTo processGepPts(const GepSVFGNode* gep, const PointsTo& srcPts);

    /// Update call graph.
    //@{
    void updateCallGraphAndSVFG(const LocDPItem& dpm,const CallBlockNode* cs,SVFGEdgeSet& svfgEdges)
    {
        CallEdgeMap newEdges;
        resolveIndCalls(cs, getCachedPointsTo(dpm), newEdges);
        for (CallEdgeMap::const_iterator iter = newEdges.begin(),eiter = newEdges.end(); iter != eiter; iter++) {
            const CallBlockNode* newcs = iter->first;
            const FunctionSet & functions = iter->second;
            for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++) {
                const Function * func = *func_iter;
                getSVFG()->connectCallerAndCallee(newcs, func, svfgEdges);
            }
        }
    }
    //@}

    virtual const std::string PTAName() const {
        return "FlowSensitive DDA";
    }

private:

    /// Override parent class functions to get/add cached points-to directly via PAGNode ID
    //@{
    inline PointsTo& getCachedTLPointsTo(const LocDPItem& dpm) {
        return getPts(dpm.getCurNodeID());
    }
    //@}

    DDAClient* _client;				///< DDA client
    PTACFInfoBuilder loopInfoBuilder; ///< LoopInfo
};

#endif /* FlowDDA_H_ */
