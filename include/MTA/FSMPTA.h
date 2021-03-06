/*
 * FSMPTA.h
 *
 *  Created on: Jul 29, 2015
 *      Author: Yulei Sui, Peng Di
 */

#ifndef FSPTANALYSIS_H_
#define FSPTANALYSIS_H_


#include "WPA/FlowSensitive.h"
#include "MSSA/SVFGBuilder.h"
#include "MTA/LockAnalysis.h"

namespace SVF
{

class MHP;
class LockAnalysis;

/*!
 * SVFG builder for DDA
 */
class MTASVFGBuilder : public SVFGBuilder
{

public:
    using CallSiteSet = PointerAnalysis::CallSiteSet;
    using CallEdgeMap = PointerAnalysis::CallEdgeMap;
    using FunctionSet = PointerAnalysis::FunctionSet;
    using SVFGNodeSet = Set<const SVFGNode*>;
    using SVFGNodeVec = std::vector<const SVFGNode*>;
    using SVFGNodeIDSet = NodeBS;
    using InstSet Set<const Instruction*>;
    using NodeIDPair = std::pair<NodeID,NodeID>;

    using SVFGNodeLockSpanPair = std::pair<const StmtSVFGNode*, LockAnalysis::LockSpan>;
    using PairToBoolMap = Map<SVFGNodeLockSpanPair, bool>;
    /// Constructor
    MTASVFGBuilder(MHP* m, LockAnalysis* la) : SVFGBuilder(), mhp(m), lockana(la)
    {
    }

    /// Destructor
    virtual ~MTASVFGBuilder() {}

    /// Number of newly added SVFG edges
    static u32_t numOfNewSVFGEdges;
    static u32_t numOfRemovedSVFGEdges;
    static u32_t numOfRemovedPTS;

protected:
    /// Re-write create SVFG method
    virtual void buildSVFG();

private:
    /// Record edges
    bool recordEdge(NodeID id1, NodeID id2, PointsTo pts);
    bool recordAddingEdge(NodeID id1, NodeID id2, PointsTo pts);
    bool recordRemovingEdge(NodeID id1, NodeID id2, PointsTo pts);
    /// perform adding/removing MHP Edges in value flow graph
    void performAddingMHPEdges();
    void performRemovingMHPEdges();
    SVFGEdge* addTDEdges(NodeID srcId, NodeID dstId, PointsTo& pts);
    /// Connect MHP indirect value-flow edges for two nodes that may-happen-in-parallel
    void connectMHPEdges(PointerAnalysis* pta);

    void handleStoreLoadNonSparse(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);
    void handleStoreStoreNonSparse(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);

    void handleStoreLoad(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);
    void handleStoreStore(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);

    void handleStoreLoadWithLockPrecisely(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);
    void handleStoreStoreWithLockPrecisely(const StmtSVFGNode* n1,const StmtSVFGNode* n2, PointerAnalysis* pta);

    void mergeSpan(NodeBS comlocks, InstSet& res);
    void readPrecision();

    SVFGNodeIDSet getPrevNodes(const StmtSVFGNode* n);
    SVFGNodeIDSet getSuccNodes(const StmtSVFGNode* n);
    SVFGNodeIDSet getSuccNodes(const StmtSVFGNode* n, NodeID o);

    bool isHeadofSpan(const StmtSVFGNode* n, LockAnalysis::LockSpan lspan);
    bool isTailofSpan(const StmtSVFGNode* n, LockAnalysis::LockSpan lspan);
    bool isHeadofSpan(const StmtSVFGNode* n, InstSet mergespan);
    bool isTailofSpan(const StmtSVFGNode* n, InstSet mergespan);
    bool isHeadofSpan(const StmtSVFGNode* n);
    bool isTailofSpan(const StmtSVFGNode* n);
    /// Collect all loads/stores SVFGNodes
    void collectLoadStoreSVFGNodes();

    /// all stores/loads SVFGNodes
    SVFGNodeSet stnodeSet;
    SVFGNodeSet ldnodeSet;

    /// MHP class
    MHP* mhp;
    LockAnalysis* lockana;

    Set<NodeIDPair> recordedges;
    Map<NodeIDPair, PointsTo> edge2pts;


    Map<const StmtSVFGNode*, SVFGNodeIDSet> prevset;
    Map<const StmtSVFGNode*, SVFGNodeIDSet> succset;

    Map<const StmtSVFGNode*, bool> headmap;
    Map<const StmtSVFGNode*, bool> tailmap;

    PairToBoolMap pairheadmap;
    PairToBoolMap pairtailmap;


    static const u32_t ADDEDGE_NOEDGE= 0;
    static const u32_t ADDEDGE_NONSPARSE= 1;
    static const u32_t ADDEDGE_ALLOPT= 2;
    static const u32_t ADDEDGE_NOMHP= 3;
    static const u32_t ADDEDGE_NOALIAS= 4;
    static const u32_t ADDEDGE_NOLOCK= 5;
    static const u32_t ADDEDGE_NORP= 6;
//    static const u32_t ADDEDGE_PRECISELOCK= 5;

};


/*!
 * Flow-sensitive pointer analysis for multithreaded programs
 */
class FSMPTA : public FlowSensitive
{


public:

    /// Constructor
    FSMPTA(MHP* m, LockAnalysis* la) : FlowSensitive(), mhp(m), lockana(la)
    {
    }

    /// Destructor
    ~FSMPTA()
    {
    }

    /// Initialize analysis
    void initialize(SVFModule* module);

    /// Create signle instance of flow-sensitive pointer analysis
    static FSMPTA* createFSMPTA(SVFModule* module, MHP* m, LockAnalysis* la)
    {
        if (mfspta == nullptr)
        {
            mfspta = new FSMPTA(m,la);
            mfspta->analyze(module);
        }
        return mfspta;
    }

    /// Release flow-sensitive pointer analysis
    static void releaseFSMPTA()
    {
        if (mfspta)
            delete mfspta;
        mfspta = nullptr;
    }

    /// Get MHP
    inline MHP* getMHP() const
    {
        return mhp;
    }

private:
    static FSMPTA* mfspta;
    MHP* mhp;
    LockAnalysis* lockana;
};

} // End namespace SVF

#endif /* FSPTANALYSIS_H_ */
