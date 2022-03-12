//===- Andersen.h -- Field-sensitive Andersen's pointer analysis-------------//
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
 * Andersen.h
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#ifndef ANDERSENPASS_H_
#define ANDERSENPASS_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAStat.h"
#include "WPA/WPASolver.h"
#include "MemoryModel/SVFIR.h"
#include "Graphs/ConsG.h"
#include "Graphs/OfflineConsG.h"

namespace SVF
{

class SVFModule;

/*!
 * Abstract class of inclusion-based Pointer Analysis
 */
typedef WPASolver<ConstraintGraph*> WPAConstraintSolver;

class AndersenBase:  public WPAConstraintSolver, public BVDataPTAImpl
{
public:

    /// Constructor
    AndersenBase(SVFIR* _pag, PTATY type = Andersen_BASE, bool alias_check = true)
        :  BVDataPTAImpl(_pag, type, alias_check), consCG(nullptr)
    {
        iterationForPrintStat = OnTheFlyIterBudgetForStat;
    }

    /// Destructor
    ~AndersenBase() override;

    /// Andersen analysis
    virtual void analyze() override;

    /// Initialize analysis
    virtual void initialize() override;

    /// Finalize analysis
    virtual void finalize() override;

    /// Implement it in child class to update call graph
    virtual inline bool updateCallGraph(const CallSiteToFunPtrMap&) override
    {
        return false;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AndersenBase *)
    {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return ( pta->getAnalysisTy() == Andersen_BASE
				|| pta->getAnalysisTy() == Andersen_WPA
                || pta->getAnalysisTy() == AndersenLCD_WPA
                || pta->getAnalysisTy() == AndersenHCD_WPA
                || pta->getAnalysisTy() == AndersenHLCD_WPA
                || pta->getAnalysisTy() == AndersenWaveDiff_WPA
                || pta->getAnalysisTy() == AndersenWaveDiffWithType_WPA
                || pta->getAnalysisTy() == AndersenSCD_WPA
                || pta->getAnalysisTy() == AndersenSFR_WPA
				|| pta->getAnalysisTy() == TypeCPP_WPA
				|| pta->getAnalysisTy() == Steensgaard_WPA);
    }
    //@}

    /// Get constraint graph
    ConstraintGraph* getConstraintGraph()
    {
        return consCG;
    }

    /// dump statistics
    inline void printStat()
    {
        PointerAnalysis::dumpStat();
    }

    virtual void normalizePointsTo() override;

    /// remove redundant gepnodes in constraint graph
    void cleanConsCG(NodeID id);

    NodeBS redundantGepNodes;

    /// Statistics
    //@{
    static u32_t numOfProcessedAddr;   /// Number of processed Addr edge
    static u32_t numOfProcessedCopy;   /// Number of processed Copy edge
    static u32_t numOfProcessedGep;    /// Number of processed Gep edge
    static u32_t numOfProcessedLoad;   /// Number of processed Load edge
    static u32_t numOfProcessedStore;  /// Number of processed Store edge
    static u32_t numOfSfrs;
    static u32_t numOfFieldExpand;

    static u32_t numOfSCCDetection;
    static double timeOfSCCDetection;
    static double timeOfSCCMerges;
    static double timeOfCollapse;
    static u32_t AveragePointsToSetSize;
    static u32_t MaxPointsToSetSize;
    static double timeOfProcessCopyGep;
    static double timeOfProcessLoadStore;
    static double timeOfUpdateCallGraph;
    //@}

protected:
    /// Constraint Graph
    ConstraintGraph* consCG;
};

/*!
 * Inclusion-based Pointer Analysis
 */
class Andersen:  public AndersenBase
{


public:
    typedef SCCDetection<ConstraintGraph*> CGSCC;
    typedef OrderedMap<CallSite, NodeID> CallSite2DummyValPN;

    /// Constructor
    Andersen(SVFIR* _pag, PTATY type = Andersen_WPA, bool alias_check = true)
        :  AndersenBase(_pag, type, alias_check), pwcOpt(false), diffOpt(true)
    {
    }

    /// Destructor
    virtual ~Andersen()
    {

    }

    /// Initialize analysis
    virtual void initialize();

    /// Finalize analysis
    virtual void finalize();

    /// Reset data
    inline void resetData()
    {
        AveragePointsToSetSize = 0;
        MaxPointsToSetSize = 0;
        timeOfProcessCopyGep = 0;
        timeOfProcessLoadStore = 0;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const Andersen *)
    {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return (pta->getAnalysisTy() == Andersen_WPA
				|| pta->getAnalysisTy() == AndersenLCD_WPA
                || pta->getAnalysisTy() == AndersenHCD_WPA
                || pta->getAnalysisTy() == AndersenHLCD_WPA
                || pta->getAnalysisTy() == AndersenWaveDiff_WPA
                || pta->getAnalysisTy() == AndersenWaveDiffWithType_WPA
                || pta->getAnalysisTy() == AndersenSCD_WPA
                || pta->getAnalysisTy() == AndersenSFR_WPA);
    }
    //@}

    /// SCC methods
    //@{
    inline NodeID sccRepNode(NodeID id) const
    {
        return consCG->sccRepNode(id);
    }
    inline NodeBS& sccSubNodes(NodeID repId)
    {
        return consCG->sccSubNodes(repId);
    }
    //@}

    /// Operation of points-to set
    virtual inline const PointsTo& getPts(NodeID id)
    {
        return getPTDataTy()->getPts(sccRepNode(id));
    }
    virtual inline bool unionPts(NodeID id, const PointsTo& target)
    {
        id = sccRepNode(id);
        return getPTDataTy()->unionPts(id, target);
    }
    virtual inline bool unionPts(NodeID id, NodeID ptd)
    {
        id = sccRepNode(id);
        ptd = sccRepNode(ptd);
        return getPTDataTy()->unionPts(id,ptd);
    }


    void dumpTopLevelPtsTo();

    void setPWCOpt(bool flag)
    {
        pwcOpt = flag;
        if (pwcOpt)
            setSCCEdgeFlag(ConstraintNode::Direct);
        else
            setSCCEdgeFlag(ConstraintNode::Copy);
    }

    bool mergePWC() const
    {
        return pwcOpt;
    }

    void setDiffOpt(bool flag)
    {
        diffOpt = flag;
    }

    bool enableDiff() const
    {
        return diffOpt;
    }

protected:

    CallSite2DummyValPN callsite2DummyValPN;        ///< Map an instruction to a dummy obj which created at an indirect callsite, which invokes a heap allocator
    void heapAllocatorViaIndCall(CallSite cs,NodePairSet &cpySrcNodes);

    bool pwcOpt;
    bool diffOpt;

    /// Handle diff points-to set.
    virtual inline void computeDiffPts(NodeID id)
    {
        if (enableDiff())
        {
            NodeID rep = sccRepNode(id);
            getDiffPTDataTy()->computeDiffPts(rep, getDiffPTDataTy()->getPts(rep));
        }
    }
    virtual inline const PointsTo& getDiffPts(NodeID id)
    {
        NodeID rep = sccRepNode(id);
        if (enableDiff())
            return getDiffPTDataTy()->getDiffPts(rep);
        else
            return getPTDataTy()->getPts(rep);
    }

    /// Handle propagated points-to set.
    inline void updatePropaPts(NodeID dstId, NodeID srcId)
    {
        if (!enableDiff())
            return;
        NodeID srcRep = sccRepNode(srcId);
        NodeID dstRep = sccRepNode(dstId);
        getDiffPTDataTy()->updatePropaPtsMap(srcRep, dstRep);
    }
    inline void clearPropaPts(NodeID src)
    {
        if (enableDiff())
        {
            NodeID rep = sccRepNode(src);
            getDiffPTDataTy()->clearPropaPts(rep);
        }
    }

    virtual void initWorklist() {}

    virtual void setSCCEdgeFlag(ConstraintNode::SCCEdgeFlag f)
    {
        ConstraintNode::sccEdgeFlag = f;
    }

    /// Override WPASolver function in order to use the default solver
    virtual void processNode(NodeID nodeId);

    /// handling various constraints
    //@{
    void processAllAddr();

    virtual bool processLoad(NodeID node, const ConstraintEdge* load);
    virtual bool processStore(NodeID node, const ConstraintEdge* load);
    virtual bool processCopy(NodeID node, const ConstraintEdge* edge);
    virtual bool processGep(NodeID node, const GepCGEdge* edge);
    virtual void handleCopyGep(ConstraintNode* node);
    virtual void handleLoadStore(ConstraintNode* node);
    virtual void processAddr(const AddrCGEdge* addr);
    virtual bool processGepPts(const PointsTo& pts, const GepCGEdge* edge);
    //@}

    /// Add copy edge on constraint graph
    virtual inline bool addCopyEdge(NodeID src, NodeID dst)
    {
        if (consCG->addCopyCGEdge(src, dst))
        {
            updatePropaPts(src, dst);
            return true;
        }
        return false;
    }

    /// Update call graph for the input indirect callsites
    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);

    /// Connect formal and actual parameters for indirect callsites
    void connectCaller2CalleeParams(CallSite cs, const SVFFunction* F, NodePairSet& cpySrcNodes);

    /// Merge sub node to its rep
    virtual void mergeNodeToRep(NodeID nodeId,NodeID newRepId);

    virtual bool mergeSrcToTgt(NodeID srcId,NodeID tgtId);

    /// Merge sub node in a SCC cycle to their rep node
    //@{
    void mergeSccNodes(NodeID repNodeId, const NodeBS& subNodes);
    void mergeSccCycle();
    //@}
    /// Collapse a field object into its base for field insensitive anlaysis
    //@{
    void collapsePWCNode(NodeID nodeId);
    void collapseFields();
    bool collapseNodePts(NodeID nodeId);
    bool collapseField(NodeID nodeId);
    //@}

    /// Updates subnodes of its rep, and rep node of its subs
    void updateNodeRepAndSubs(NodeID nodeId,NodeID newRepId);

    /// SCC detection
    virtual NodeStack& SCCDetect();



    /// Sanitize pts for field insensitive objects
    void sanitizePts()
    {
        for(ConstraintGraph::iterator it = consCG->begin(), eit = consCG->end(); it!=eit; ++it)
        {
            const PointsTo& pts = getPts(it->first);
            NodeBS fldInsenObjs;

            for (NodeID o : pts)
            {
                if(isFieldInsensitive(o))
                    fldInsenObjs.set(o);
            }

            for (NodeID o : fldInsenObjs)
            {
                const NodeBS &allFields = consCG->getAllFieldsObjVars(o);
                for (NodeID f : allFields) addPts(it->first, f);
            }
        }
    }

    /// Get PTA name
    virtual const std::string PTAName() const
    {
        return "AndersenWPA";
    }

    /// match types for Gep Edges
    virtual bool matchType(NodeID, NodeID, const NormalGepCGEdge*)
    {
        return true;
    }
    /// add type for newly created GepObjNode
    virtual void addTypeForGepObjNode(NodeID, const NormalGepCGEdge*)
    {
        return;
    }

    /// Runs a Steensgaard analysis and performs clustering based on those
    /// results set the global best mapping.
    virtual void cluster(void) const;
};



/**
 * Wave propagation with diff points-to set.
 */
class AndersenWaveDiff : public Andersen
{

private:

    static AndersenWaveDiff* diffWave; // static instance

public:
    AndersenWaveDiff(SVFIR* _pag, PTATY type = AndersenWaveDiff_WPA, bool alias_check = true): Andersen(_pag, type, alias_check) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenWaveDiff* createAndersenWaveDiff(SVFIR* _pag)
    {
        if(diffWave==nullptr)
        {
            diffWave = new AndersenWaveDiff(_pag, AndersenWaveDiff_WPA, false);
            diffWave->analyze();
            return diffWave;
        }
        return diffWave;
    }
    static void releaseAndersenWaveDiff()
    {
        if (diffWave)
            delete diffWave;
        diffWave = nullptr;
    }

    virtual void solveWorklist();
    virtual void processNode(NodeID nodeId);
    virtual void postProcessNode(NodeID nodeId);
    virtual void handleCopyGep(ConstraintNode* node);
    virtual bool handleLoad(NodeID id, const ConstraintEdge* load);
    virtual bool handleStore(NodeID id, const ConstraintEdge* store);
    virtual bool processCopy(NodeID node, const ConstraintEdge* edge);

protected:
    virtual void mergeNodeToRep(NodeID nodeId,NodeID newRepId);

    /// process "bitcast" CopyCGEdge
    virtual void processCast(const ConstraintEdge*)
    {
        return;
    }
};


/*
 * Lazy Cycle Detection Based Andersen Analysis
 */
class AndersenLCD : virtual public Andersen
{

private:
    static AndersenLCD* lcdAndersen;
    EdgeSet metEdges;
    NodeSet lcdCandidates;

public:
    AndersenLCD(SVFIR* _pag, PTATY type = AndersenLCD_WPA) :
        Andersen(_pag, type), metEdges({}), lcdCandidates( {})
    {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenLCD* createAndersenLCD(SVFIR* _pag)
    {
        if (lcdAndersen == nullptr)
        {
            lcdAndersen = new AndersenLCD(_pag);
            lcdAndersen->analyze();
            return lcdAndersen;
        }
        return lcdAndersen;
    }

    static void releaseAndersenLCD()
    {
        if (lcdAndersen)
            delete lcdAndersen;
        lcdAndersen = nullptr;
    }

protected:
    // 'lcdCandidates' is used to collect nodes need to be visited by SCC detector
    //@{
    inline bool hasLCDCandidate () const
    {
        return !lcdCandidates.empty();
    };
    inline void cleanLCDCandidate()
    {
        lcdCandidates.clear();
    };
    inline void addLCDCandidate(NodeID nodeId)
    {
        lcdCandidates.insert(nodeId);
    };
    //@}

    // 'metEdges' is used to collect edges met by AndersenLCD, to avoid redundant visit
    //@{
    bool isMetEdge (ConstraintEdge* edge) const
    {
        EdgeSet::const_iterator it = metEdges.find(edge->getEdgeID());
        return it != metEdges.end();
    };
    void addMetEdge(ConstraintEdge* edge)
    {
        metEdges.insert(edge->getEdgeID());
    };
    //@}

    //AndersenLCD worklist processer
    void solveWorklist();
    // Solve constraints of each nodes
    virtual void handleCopyGep(ConstraintNode* node);
    // Collapse nodes and fields based on 'lcdCandidates'
    void mergeSCC();
    // AndersenLCD specified SCC detector, need to input a nodeStack 'lcdCandidate'
    NodeStack& SCCDetect();
    bool mergeSrcToTgt(NodeID nodeId, NodeID newRepId);
};



/*!
 * Hybrid Cycle Detection Based Andersen Analysis
 */
class AndersenHCD : virtual public Andersen
{

public:
    typedef SCCDetection<OfflineConsG*> OSCC;

private:
    static AndersenHCD* hcdAndersen;
    NodeSet mergedNodes;
    OfflineConsG* oCG;

public:
    AndersenHCD(SVFIR* _pag, PTATY type = AndersenHCD_WPA) :
        Andersen(_pag, type), oCG(nullptr)
    {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenHCD *createAndersenHCD(SVFIR* _pag)
    {
        if (hcdAndersen == nullptr)
        {
            hcdAndersen = new AndersenHCD(_pag);
            hcdAndersen->analyze();
            return hcdAndersen;
        }
        return hcdAndersen;
    }

    static void releaseAndersenHCD()
    {
        if (hcdAndersen)
            delete hcdAndersen;
        hcdAndersen = nullptr;
    }

protected:
    virtual void initialize();

    // Get offline rep node from offline constraint graph
    //@{
    inline bool hasOfflineRep(NodeID nodeId) const
    {
        return oCG->hasOCGRep(nodeId);
    }
    inline NodeID getOfflineRep(NodeID nodeId)
    {
        return oCG->getOCGRep(nodeId);
    }
    //@}

    // The set 'mergedNodes' is used to record the merged node, therefore avoiding re-merge nodes
    //@{
    inline bool isaMergedNode(NodeID node) const
    {
        NodeSet::const_iterator it = mergedNodes.find(node);
        return it != mergedNodes.end();
    };
    inline void setMergedNode(NodeID node)
    {
        if (!isaMergedNode(node))
            mergedNodes.insert(node);
    };
    //@}

    virtual void solveWorklist();
    virtual void mergeSCC(NodeID nodeId);
    void mergeNodeAndPts(NodeID node, NodeID tgt);

};



/*!
 * Hybrid Lazy Cycle Detection Based Andersen Analysis
 */
class AndersenHLCD : public AndersenHCD, public AndersenLCD
{

private:
    static AndersenHLCD* hlcdAndersen;

public:
    AndersenHLCD(SVFIR* _pag, PTATY type = AndersenHLCD_WPA) :
        Andersen(_pag, type), AndersenHCD(_pag, type), AndersenLCD(_pag, type)
    {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenHLCD *createAndersenHLCD(SVFIR* _pag)
    {
        if (hlcdAndersen == nullptr)
        {
            hlcdAndersen = new AndersenHLCD(_pag);
            hlcdAndersen->analyze();
            return hlcdAndersen;
        }
        return hlcdAndersen;
    }

    static void releaseAndersenHLCD()
    {
        if (hlcdAndersen)
            delete hlcdAndersen;
        hlcdAndersen = nullptr;
    }

protected:
    void initialize()
    {
        AndersenHCD::initialize();
    }
    void solveWorklist()
    {
        AndersenHCD::solveWorklist();
    }
    void handleCopyGep(ConstraintNode* node)
    {
        AndersenLCD::handleCopyGep(node);
    }
    void mergeSCC(NodeID nodeId);
    bool mergeSrcToTgt(NodeID nodeId, NodeID newRepId)
    {
        return AndersenLCD::mergeSrcToTgt(nodeId, newRepId);
    }

};

} // End namespace SVF

#endif /* ANDERSENPASS_H_ */
