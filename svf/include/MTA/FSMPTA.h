//===- FSMPTA.h -- Flow-sensitive analysis of multithreaded programs-------------//
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
 * FSMPTA.h
 *
 *  Created on: Jul 29, 2015
 *      Author: Yulei Sui, Peng Di
 *
 * The implementation is based on
 * Yulei Sui, Peng Di, and Jingling Xue. "Sparse Flow-Sensitive Pointer Analysis for Multithreaded Programs".
 * 2016 International Symposium on Code Generation and Optimization (CGO'16)
 */

#ifndef FSPTANALYSIS_H_
#define FSPTANALYSIS_H_


#include "WPA/FlowSensitive.h"
#include "MSSA/SVFGBuilder.h"
#include "MTA/LockAnalysis.h"
#include "MemoryModel/PointsTo.h"
#include "MTA/MHP.h"

namespace SVF
{

class MHP;
class LockAnalysis;


class SVFGNodeLockSpan
{
public:
    SVFGNodeLockSpan(const StmtSVFGNode* SVFGnode, LockAnalysis::LockSpan lockspan) :
        SVFGNode(SVFGnode), lockSpan(lockspan) {}
    virtual ~SVFGNodeLockSpan() {}

    inline bool operator< (const SVFGNodeLockSpan& rhs) const
    {
        if (SVFGNode != rhs.getSVFGNode())
            return SVFGNode < rhs.getSVFGNode();
        return lockSpan.size() < rhs.getLockSpan().size();
    }
    inline SVFGNodeLockSpan& operator= (const SVFGNodeLockSpan& rhs)
    {
        if(*this != rhs)
        {
            SVFGNode = rhs.getSVFGNode();
            lockSpan = rhs.getLockSpan();
        }
        return *this;
    }
    inline bool operator== (const SVFGNodeLockSpan& rhs) const
    {
        return (SVFGNode == rhs.getSVFGNode() && lockSpan == rhs.getLockSpan());
    }
    inline bool operator!= (const SVFGNodeLockSpan& rhs) const
    {
        return !(*this == rhs);
    }
    inline const StmtSVFGNode* getSVFGNode() const
    {
        return SVFGNode;
    }
    inline const LockAnalysis::LockSpan getLockSpan() const
    {
        return lockSpan;
    }
private:
    const StmtSVFGNode* SVFGNode;
    LockAnalysis::LockSpan lockSpan;
};

/*!
 * SVFG builder for DDA
 */
class MTASVFGBuilder : public SVFGBuilder
{

public:
    typedef PointerAnalysis::CallSiteSet CallSiteSet;
    typedef PointerAnalysis::CallEdgeMap CallEdgeMap;
    typedef PointerAnalysis::FunctionSet FunctionSet;
    typedef Set<const SVFGNode*> SVFGNodeSet;
    typedef std::vector<const SVFGNode*> SVFGNodeVec;
    typedef NodeBS SVFGNodeIDSet;
    typedef Set<const SVFInstruction*> InstSet;
    typedef std::pair<NodeID,NodeID> NodeIDPair;
    typedef Map<SVFGNodeLockSpan, bool> PairToBoolMap;

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
    FSMPTA(MHP* m, LockAnalysis* la) : FlowSensitive(m->getTCT()->getPTA()->getPAG()), mhp(m), lockana(la)
    {
    }

    /// Destructor
    ~FSMPTA()
    {
    }

    /// Initialize analysis
    void initialize(SVFModule* module);

    inline SVFIR* getPAG()
    {
        return mhp->getTCT()->getPTA()->getPAG();
    }

    /// Create signle instance of flow-sensitive pointer analysis
    static FSMPTA* createFSMPTA(SVFModule* module, MHP* m, LockAnalysis* la)
    {
        if (mfspta == nullptr)
        {
            mfspta = new FSMPTA(m,la);
            mfspta->analyze();
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
    using FlowSensitive::initialize;
};

} // End namespace SVF

template <> struct std::hash<SVF::SVFGNodeLockSpan>
{
    size_t operator()(const SVF::SVFGNodeLockSpan &cs) const
    {
        std::hash<SVF::StmtSVFGNode* >h;
        SVF::StmtSVFGNode* node = const_cast<SVF::StmtSVFGNode* > (cs.getSVFGNode());
        return h(node);
    }
};

#endif /* FSPTANALYSIS_H_ */
