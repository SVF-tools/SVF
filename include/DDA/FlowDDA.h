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
#include "SVF-FE/DataFlowUtil.h"

namespace SVF
{

class DDAClient;
typedef StmtDPItem<SVFGNode> LocDPItem;

/*!
 * Flow sensitive demand-driven analysis on value-flow graph
 */
class FlowDDA : public BVDataPTAImpl, public DDAVFSolver<NodeID,PointsTo,LocDPItem>
{

public:
    typedef BVDataPTAImpl::CallSiteSet CallSiteSet;
    typedef BVDataPTAImpl::CallEdgeMap	CallEdgeMap;
    typedef BVDataPTAImpl::FunctionSet	FunctionSet;
    /// Constructor
    FlowDDA(SVFIR* _pag, DDAClient* client): BVDataPTAImpl(_pag, PointerAnalysis::FlowS_DDA),
        DDAVFSolver<NodeID,PointsTo,LocDPItem>(),
        _client(client)
    {
    }
    /// Destructor
    inline virtual ~FlowDDA()
    {
    }
    /// dummy analyze method
    virtual void analyze() override {}

    /// Compute points-to set for all top variable
    void computeDDAPts(NodeID id) override;

    /// Handle out-of-budget dpm
    void handleOutOfBudgetDpm(const LocDPItem& dpm);

    /// Handle condition for flow analysis (backward analysis)
    virtual bool handleBKCondition(LocDPItem& dpm, const SVFGEdge* edge) override;

    /// refine indirect call edge
    bool testIndCallReachability(LocDPItem& dpm, const SVFFunction* callee, CallSiteID csId);

    /// Initialization of the analysis
    inline virtual void initialize() override
    {
        BVDataPTAImpl::initialize();
        buildSVFG(pag);
        setCallGraph(getPTACallGraph());
        setCallGraphSCC(getCallGraphSCC());
        stat = setDDAStat(new DDAStat(this));
    }

    /// Finalize analysis
    inline virtual void finalize() override
    {
        BVDataPTAImpl::finalize();
    }

    /// we exclude concrete heap here following the conditions:
    /// (1) local allocated heap and
    /// (2) not escaped to the scope outside the current function
    /// (3) not inside loop
    /// (4) not involved in recursion
    virtual bool isHeapCondMemObj(const NodeID& var, const StoreSVFGNode* store) override;

    /// Override parent method
    virtual inline PointsTo getConservativeCPts(const LocDPItem& dpm) override
    {
        return getAndersenAnalysis()->getPts(dpm.getCurNodeID());
    }
    /// Override parent method
    virtual inline NodeID getPtrNodeID(const NodeID& var) const override
    {
        return var;
    }
    /// Handle Address SVFGNode to add proper points-to
    virtual inline void handleAddr(PointsTo& pts,const LocDPItem& dpm,const AddrSVFGNode* addr) override
    {
        NodeID srcID = addr->getPAGSrcNodeID();
        /// whether this object is set field-insensitive during pre-analysis
        if (isFieldInsensitive(srcID))
            srcID = getFIObjVar(srcID);

        addDDAPts(pts,srcID);
        DBOUT(DDDA, SVFUtil::outs() << "\t add points-to target " << srcID << " to dpm ");
        DBOUT(DDDA, dpm.dump());
    }
    /// processGep node
    virtual PointsTo processGepPts(const GepSVFGNode* gep, const PointsTo& srcPts) override;

    /// Update call graph.
    //@{
    virtual void updateCallGraphAndSVFG(const LocDPItem& dpm,const CallICFGNode* cs,SVFGEdgeSet& svfgEdges) override
    {
        CallEdgeMap newEdges;
        resolveIndCalls(cs, getCachedPointsTo(dpm), newEdges);
        for (CallEdgeMap::const_iterator iter = newEdges.begin(),eiter = newEdges.end(); iter != eiter; iter++)
        {
            const CallICFGNode* newcs = iter->first;
            const FunctionSet & functions = iter->second;
            for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
            {
                const SVFFunction* func = *func_iter;
                getSVFG()->connectCallerAndCallee(newcs, func, svfgEdges);
            }
        }
    }
    //@}

    /// Override parent class functions to get/add cached points-to directly via PAGNode ID
    //@{
    virtual inline const PointsTo& getCachedTLPointsTo(const LocDPItem& dpm) override
    {
        return getPts(dpm.getCurNodeID());
    }
    //@}

    /// Union pts
    virtual bool unionDDAPts(LocDPItem dpm, const PointsTo& targetPts) override
    {
        if (isTopLevelPtrStmt(dpm.getLoc())) return unionPts(dpm.getCurNodeID(), targetPts);
        else return dpmToADCPtSetMap[dpm] |= targetPts;
    }

    virtual const std::string PTAName() const override
    {
        return "FlowSensitive DDA";
    }

private:
    DDAClient* _client;				///< DDA client
    PTACFInfoBuilder loopInfoBuilder; ///< LoopInfo
};

} // End namespace SVF

#endif /* FlowDDA_H_ */
