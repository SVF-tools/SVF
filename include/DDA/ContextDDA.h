/*
 * ContextDDA.h
 *
 *  Created on: Aug 17, 2014
 *      Author: Yulei Sui
 */

#ifndef ContextDDA_H_
#define ContextDDA_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "DDA/DDAVFSolver.h"
#include "Util/DPItem.h"

class FlowDDA;
class DDAClient;
typedef CxtStmtDPItem<SVFGNode> CxtLocDPItem;

/*!
 * Context-, Flow- Sensitive Demand-driven Analysis
 */
class ContextDDA : public CondPTAImpl<ContextCond>, public DDAVFSolver<CxtVar,CxtPtSet,CxtLocDPItem> {

public:
    /// Constructor
    ContextDDA(SVFModule* mod, DDAClient* client);

    /// Destructor
    virtual ~ContextDDA();

    /// Initialization of the analysis
    virtual void initialize(SVFModule* module);

    /// Finalize analysis
    virtual inline void finalize() {
        CondPTAImpl<ContextCond>::finalize();
    }

    /// dummy analyze method
    virtual void analyze(SVFModule* mod) {}

    /// Compute points-to set for an unconditional pointer
    void computeDDAPts(NodeID id);

    /// Compute points-to set for a context-sensitive pointer
    const CxtPtSet& computeDDAPts(const CxtVar& cxtVar);

    /// Handle out-of-budget dpm
    void handleOutOfBudgetDpm(const CxtLocDPItem& dpm);

    /// Override parent method
    CxtPtSet getConservativeCPts(const CxtLocDPItem& dpm) {
        const PointsTo& pts =  getAndersenAnalysis()->getPts(dpm.getCurNodeID());
        CxtPtSet tmpCPts;
        ContextCond cxt;
        for (PointsTo::iterator piter = pts.begin(); piter != pts.end(); ++piter) {
            CxtVar var(cxt,*piter);
            tmpCPts.set(var);
        }
        return tmpCPts;
    }

    /// Override parent method
    virtual inline NodeID getPtrNodeID(const CxtVar& var) const {
        return var.get_id();
    }
    /// Handle condition for context or path analysis (backward analysis)
    virtual bool handleBKCondition(CxtLocDPItem& dpm, const SVFGEdge* edge);

    /// we exclude concrete heap given the following conditions:
    /// (1) concrete calling context (not involved in recursion and not exceed the maximum context limit)
    /// (2) not inside loop
    bool isHeapCondMemObj(const CxtVar& var, const StoreSVFGNode* store);

    /// refine indirect call edge
    bool testIndCallReachability(CxtLocDPItem& dpm, const Function* callee, const CallBlockNode* cs);

    /// get callsite id from call, return 0 if it is a spurious call edge
    CallSiteID getCSIDAtCall(CxtLocDPItem& dpm, const SVFGEdge* edge);

    /// get callsite id from return, return 0 if it is a spurious return edge
    CallSiteID getCSIDAtRet(CxtLocDPItem& dpm, const SVFGEdge* edge);


    /// Pop recursive callsites
    inline virtual void popRecursiveCallSites(CxtLocDPItem& dpm) {
        ContextCond& cxtCond = dpm.getCond();
        cxtCond.setNonConcreteCxt();
        CallStrCxt& cxt = cxtCond.getContexts();
        while(!cxt.empty() && isEdgeInRecursion(cxt.back())) {
            cxt.pop_back();
        }
    }
    /// Whether call/return inside recursion
    inline virtual bool isEdgeInRecursion(CallSiteID csId) {
        const Function* caller = getPTACallGraph()->getCallerOfCallSite(csId);
        const Function* callee = getPTACallGraph()->getCalleeOfCallSite(csId);
        return inSameCallGraphSCC(caller, callee);
    }
    /// Update call graph.
    //@{
    void updateCallGraphAndSVFG(const CxtLocDPItem& dpm,const CallBlockNode* cs,SVFGEdgeSet& svfgEdges)
    {
        CallEdgeMap newEdges;
        resolveIndCalls(cs, getBVPointsTo(getCachedPointsTo(dpm)), newEdges);
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

    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    inline bool edgeInCallGraphSCC(const SVFGEdge* edge) {
        const Function* srcfun = edge->getSrcNode()->getFun();
        const Function* dstfun = edge->getDstNode()->getFun();

        if(srcfun && dstfun)
            return inSameCallGraphSCC(srcfun,dstfun);

        assert(edge->isRetVFGEdge() == false && "should not be an inter-procedural return edge" );

        return false;
    }

    /// processGep node
    CxtPtSet processGepPts(const GepSVFGNode* gep, const CxtPtSet& srcPts);

    /// Handle Address SVFGNode to add proper conditional points-to
    void handleAddr(CxtPtSet& pts,const CxtLocDPItem& dpm,const AddrSVFGNode* addr) {
        NodeID srcID = addr->getPAGSrcNodeID();
        /// whether this object is set field-insensitive during pre-analysis
        if (isFieldInsensitive(srcID))
            srcID = getFIObjNode(srcID);

        CxtVar var(dpm.getCond(),srcID);
        addDDAPts(pts,var);
        DBOUT(DDDA, SVFUtil::outs() << "\t add points-to target " << var << " to dpm ");
        DBOUT(DDDA, dpm.dump());
    }

    /// Propagate along indirect value-flow if two objects of load and store are same
    virtual inline bool propagateViaObj(const CxtVar& storeObj, const CxtVar& loadObj) {
        return isSameVar(storeObj,loadObj);
    }

    /// Whether two call string contexts are compatible which may represent the same memory object
    /// compare with call strings from last few callsite ids (most recent ids to objects):
    /// compatible : (e.g., 123 == 123, 123 == 23). not compatible (e.g., 123 != 423)
    inline bool isCondCompatible(const ContextCond& cxt1, const ContextCond& cxt2, bool singleton) const;

    /// Whether this edge is treated context-insensitively
    bool isInsensitiveCallRet(const SVFGEdge* edge) {
        return insensitveEdges.find(edge) != insensitveEdges.end();
    }
    /// Return insensitive edge set
    inline ConstSVFGEdgeSet& getInsensitiveEdgeSet() {
        return insensitveEdges;
    }
    /// dump context call strings
    virtual inline void dumpContexts(const ContextCond& cxts) {
        SVFUtil::outs() << cxts.toString() << "\n";
    }

    virtual const std::string PTAName() const {
        return "Context Sensitive DDA";
    }

private:
    ConstSVFGEdgeSet insensitveEdges;///< insensitive call-return edges
    FlowDDA* flowDDA;			///< downgrade to flowDDA if out-of-budget
    DDAClient* _client;			///< DDA client
    PTACFInfoBuilder loopInfoBuilder; ///< LoopInfo
};


#endif /* ContextDDA_H_ */
