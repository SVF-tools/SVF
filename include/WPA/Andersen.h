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

#include "MemoryModel/PointerAnalysis.h"
#include "WPA/WPAStat.h"
#include "WPA/WPASolver.h"
#include "MemoryModel/ConsG.h"
#include <llvm/PassAnalysisSupport.h>	// analysis usage
#include <llvm/Support/Debug.h>		// DEBUG TYPE

class PTAType;
class SVFModule;
/*!
 * Inclusion-based Pointer Analysis
 */
typedef WPASolver<ConstraintGraph*> WPAConstraintSolver;

class Andersen:  public WPAConstraintSolver, public BVDataPTAImpl {


public:
    typedef std::set<const ConstraintEdge*> EdgeSet;
    typedef SCCDetection<ConstraintGraph*> CGSCC;

    /// Pass ID
    static char ID;

    /// Statistics
    //@{
    static Size_t numOfProcessedAddr;	/// Number of processed Addr edge
    static Size_t numOfProcessedCopy;	/// Number of processed Copy edge
    static Size_t numOfProcessedGep;	/// Number of processed Gep edge
    static Size_t numOfProcessedLoad;	/// Number of processed Load edge
    static Size_t numOfProcessedStore;	/// Number of processed Store edge

    static Size_t numOfSCCDetection;
    static double timeOfSCCDetection;
    static double timeOfSCCMerges;
    static double timeOfCollapse;
    static Size_t AveragePointsToSetSize;
    static Size_t MaxPointsToSetSize;
    static double timeOfProcessCopyGep;
    static double timeOfProcessLoadStore;
    static double timeOfUpdateCallGraph;
    //@}

    /// Constructor
    Andersen(PTATY type = Andersen_WPA)
        :  BVDataPTAImpl(type), consCG(NULL)
    {
        reanalyze = false;
    }

    /// Destructor
    virtual ~Andersen() {
        if (consCG != NULL)
            delete consCG;
        consCG = NULL;
    }

    /// Andersen analysis
    void analyze(SVFModule svfModule);

    /// Initialize analysis
    virtual inline void initialize(SVFModule svfModule) {
        resetData();
        /// Build PAG
        PointerAnalysis::initialize(svfModule);
        /// Build Constraint Graph
        consCG = new ConstraintGraph(pag);
        setGraph(consCG);
        /// Create statistic class
        stat = new AndersenStat(this);

    }

    //}

    /// Finalize analysis
    virtual inline void finalize() {
        /// dump constraint graph if PAGDotGraph flag is enabled
        consCG->dump();
        /// sanitize field insensitive obj
        /// TODO: Fields has been collapsed during Andersen::collapseField().
        //	sanitizePts();

        PointerAnalysis::finalize();
    }

    /// Reset data
    inline void resetData() {
        AveragePointsToSetSize = 0;
        MaxPointsToSetSize = 0;
        timeOfProcessCopyGep = 0;
        timeOfProcessLoadStore = 0;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const Andersen *) {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta) {
        return (pta->getAnalysisTy() == Andersen_WPA
                || pta->getAnalysisTy() == AndersenLCD_WPA
                || pta->getAnalysisTy() == AndersenWave_WPA
                || pta->getAnalysisTy() == AndersenWaveDiff_WPA
                || pta->getAnalysisTy() == AndersenWaveDiffWithType_WPA);
    }
    //@}

    /// SCC methods
    //@{
    inline NodeID sccRepNode(NodeID id) const {
        return consCG->sccRepNode(id);
    }
    inline NodeBS& sccSubNodes(NodeID repId) {
        return consCG->sccSubNodes(repId);
    }
    //@}

    /// Get points-to set
    virtual inline PointsTo& getPts(NodeID id) {
        return getPTDataTy()->getPts(sccRepNode(id));
    }

    /// Get constraint graph
    ConstraintGraph* getConstraintGraph() {
        return consCG;
    }

protected:
    /// Reanalyze if any constraint value changed
    bool reanalyze;

    /// Override WPASolver function in order to use the default solver
    virtual void processNode(NodeID nodeId);

    /// handling various constraints
    //@{
    void processAllAddr();

    virtual bool processLoad(NodeID node, const ConstraintEdge* load);

    virtual bool processStore(NodeID node, const ConstraintEdge* load);

    virtual bool processCopy(NodeID node, const ConstraintEdge* edge);

    virtual void processAddr(const AddrCGEdge* addr);

    virtual void processGep(NodeID node, const GepCGEdge* edge);

    virtual void processGepPts(PointsTo& pts, const GepCGEdge* edge);
    //@}

    /// Add copy edge on constraint graph
    virtual inline bool addCopyEdge(NodeID src, NodeID dst) {
        return consCG->addCopyCGEdge(src, dst);
    }

    /// Update call graph for the input indirect callsites
    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);

    /// Merge sub node to its rep
    virtual void mergeNodeToRep(NodeID nodeId,NodeID newRepId);

    /// Merge sub node in a SCC cycle to their rep node
    //@{
    void mergeSccNodes(NodeID repNodeId, NodeBS & chanegdRepNodes);
    void mergeSccCycle();
    //@}
    /// Collapse a field object into its base for field insensitive anlaysis
    //@{
    bool collapseNodePts(NodeID nodeId);
    bool collapseField(NodeID nodeId);
    //@}

    /// Updates subnodes of its rep, and rep node of its subs
    void updateNodeRepAndSubs(NodeID nodeId);

    /// SCC detection
    virtual NodeStack& SCCDetect();

    /// Constraint Graph
    ConstraintGraph* consCG;

    /// Sanitize pts for field insensitive objects
    void sanitizePts() {
        for(ConstraintGraph::iterator it = consCG->begin(), eit = consCG->end(); it!=eit; ++it) {
            PointsTo& pts = getPts(it->first);
            NodeBS fldInsenObjs;
            for(NodeBS::iterator pit = pts.begin(), epit = pts.end(); pit!=epit; ++pit) {
                if(consCG->isFieldInsensitiveObj(*pit))
                    fldInsenObjs.set(*pit);
            }
            for(NodeBS::iterator pit = fldInsenObjs.begin(), epit = fldInsenObjs.end(); pit!=epit; ++pit) {
                unionPts(it->first,consCG->getAllFieldsObjNode(*pit));
            }
        }
    }

    /// Get PTA name
    virtual const std::string PTAName() const {
        return "AndersenWPA";
    }

    /// match types for Gep Edges
    virtual bool matchType(NodeID ptrid, NodeID objid, const NormalGepCGEdge *normalGepEdge) {
        return true;
    }
    /// add type for newly created GepObjNode
    virtual void addTypeForGepObjNode(NodeID id, const NormalGepCGEdge* normalGepEdge) {
        return;
    }
};

/*
 * Wave propagation based Andersen Analysis
 */
class AndersenWave : public Andersen {

private:
    static AndersenWave* waveAndersen; // static instance

public:
    AndersenWave(PTATY type = AndersenWave_WPA) : Andersen(type) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenWave* createAndersenWave(SVFModule svfModule) {
        if(waveAndersen==NULL) {
            waveAndersen = new AndersenWave();
            waveAndersen->analyze(svfModule);
            return waveAndersen;
        }
        return waveAndersen;
    }
    static void releaseAndersenWave() {
        if (waveAndersen)
            delete waveAndersen;
        waveAndersen = NULL;
    }

    virtual void processNode(NodeID nodeId);
    virtual void postProcessNode(NodeID nodeId);

    virtual void handleCopyGep(ConstraintNode* node);

    virtual bool handleLoad(NodeID id, const ConstraintEdge* load);
    virtual bool handleStore(NodeID id, const ConstraintEdge* store);
};



/**
 * Wave propagation with diff points-to set.
 */
class AndersenWaveDiff : public AndersenWave {

private:

    static AndersenWaveDiff* diffWave; // static instance

    PointsTo & getCachePts(const ConstraintEdge* edge) {
        EdgeID edgeId = edge->getEdgeID();
        return getDiffPTDataTy()->getCachePts(edgeId);
    }

    /// Handle diff points-to set.
    //@{
    virtual inline void computeDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        getDiffPTDataTy()->computeDiffPts(rep, getDiffPTDataTy()->getPts(rep));
    }
    virtual inline PointsTo& getDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        return getDiffPTDataTy()->getDiffPts(rep);
    }
    //@}

    /// Handle propagated points-to set.
    //@{
    inline void updatePropaPts(NodeID dst, NodeID src) {
        NodeID srcRep = sccRepNode(src);
        NodeID dstRep = sccRepNode(dst);
        getDiffPTDataTy()->updatePropaPtsMap(srcRep, dstRep);
    }
    inline void clearPropaPts(NodeID src) {
        NodeID rep = sccRepNode(src);
        getDiffPTDataTy()->clearPropaPts(rep);
    }
    //@}

public:
    AndersenWaveDiff(PTATY type = AndersenWaveDiff_WPA): AndersenWave(type) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenWaveDiff* createAndersenWaveDiff(SVFModule svfModule) {
        if(diffWave==NULL) {
            diffWave = new AndersenWaveDiff();
            diffWave->analyze(svfModule);
            return diffWave;
        }
        return diffWave;
    }
    static void releaseAndersenWaveDiff() {
        if (diffWave)
            delete diffWave;
        diffWave = NULL;
    }

    virtual void handleCopyGep(ConstraintNode* node);
    virtual bool processCopy(NodeID node, const ConstraintEdge* edge);
    virtual void processGep(NodeID node, const GepCGEdge* edge);

    virtual bool handleLoad(NodeID id, const ConstraintEdge* load);
    virtual bool handleStore(NodeID id, const ConstraintEdge* store);

    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);

protected:
    virtual void mergeNodeToRep(NodeID nodeId,NodeID newRepId);

    virtual inline bool addCopyEdge(NodeID src, NodeID dst) {
        if (Andersen::addCopyEdge(src, dst)) {
            if (unionPts(sccRepNode(dst), sccRepNode(src)))
                pushIntoWorklist(sccRepNode(dst));

            return true;
        }
        else
            return false;
    }

    /// process "bitcast" CopyCGEdge
    virtual void processCast(const ConstraintEdge *edge) {
        return;
    }
};



/**
 * Wave propagation with diff points-to set with type filter.
 */
class AndersenWaveDiffWithType : public AndersenWaveDiff {

private:

    typedef std::map<NodeID, std::set<const GepCGEdge*>> TypeMismatchedObjToEdgeTy;

    TypeMismatchedObjToEdgeTy typeMismatchedObjToEdges;

    void recordTypeMismatchedGep(NodeID obj, const GepCGEdge* gepEdge) {
        TypeMismatchedObjToEdgeTy::iterator it = typeMismatchedObjToEdges.find(obj);
        if (it != typeMismatchedObjToEdges.end()) {
            std::set<const GepCGEdge*> &edges = it->second;
            edges.insert(gepEdge);
        } else {
            std::set<const GepCGEdge*> edges;
            edges.insert(gepEdge);
            typeMismatchedObjToEdges[obj] = edges;
        }
    }

    static AndersenWaveDiffWithType* diffWaveWithType; // static instance

    /// Handle diff points-to set.
    //@{
    virtual inline void computeDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        getDiffPTDataTy()->computeDiffPts(rep, getDiffPTDataTy()->getPts(rep));
    }
    virtual inline PointsTo& getDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        return getDiffPTDataTy()->getDiffPts(rep);
    }
    //@}

public:
    AndersenWaveDiffWithType(PTATY type = AndersenWaveDiffWithType_WPA): AndersenWaveDiff(type) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenWaveDiffWithType* createAndersenWaveDiffWithType(SVFModule svfModule) {
        if(diffWaveWithType==NULL) {
            diffWaveWithType = new AndersenWaveDiffWithType();
            diffWaveWithType->analyze(svfModule);
            return diffWaveWithType;
        }
        return diffWaveWithType;
    }
    static void releaseAndersenWaveDiffWithType() {
        if (diffWaveWithType)
            delete diffWaveWithType;
        diffWaveWithType = NULL;
    }

protected:
    /// SCC detection
    virtual NodeStack& SCCDetect();
    /// merge types of nodes in a cycle
    void mergeTypeOfNodes(const NodeBS &nodes);
    /// process "bitcast" CopyCGEdge
    virtual void processCast(const ConstraintEdge *edge);
    /// update type of objects when process "bitcast" CopyCGEdge
    void updateObjType(const llvm::Type *type, PointsTo &objs);
    /// process mismatched gep edges
    void processTypeMismatchedGep(NodeID obj, const llvm::Type *type);
    /// match types for Gep Edges
    virtual bool matchType(NodeID ptrid, NodeID objid, const NormalGepCGEdge *normalGepEdge);
    /// add type for newly created GepObjNode
    virtual void addTypeForGepObjNode(NodeID id, const NormalGepCGEdge* normalGepEdge);
};



/*
 * Lazy Cycle Detection based Andersen Analysis
 * TODO: note that this implementaion is incomplete,
 * and the algorithm may not be consistent with the one in paper
 */
class AndersenLCD : public Andersen {

private:
    static AndersenLCD* lcdAndersen; // static instance

public:
    AndersenLCD(PTATY type = AndersenLCD_WPA): Andersen(type) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenLCD* createAndersenWave(SVFModule svfModule) {
        if(lcdAndersen==NULL) {
            lcdAndersen = new AndersenLCD();
            lcdAndersen->analyze(svfModule);
            return lcdAndersen;
        }
        return lcdAndersen;
    }
    static void releaseAndersenLCD() {
        if (lcdAndersen)
            delete lcdAndersen;
        lcdAndersen = NULL;
    }
    /// Overriding functions of Andersen Pass
    //@{
    void processNode(NodeID nodeId);

    bool processCopy(NodeID node, const ConstraintEdge* edge);
    //@}
private:
    /// Lazy cycle elimination edge, in order to avoid duplicate detection
    EdgeSet lcdEdges;

    /// LCD edge
    bool inline isNewLCDEdge(const ConstraintEdge* edge) {
        if(0==lcdEdges.count(edge)) {
            lcdEdges.insert(edge);
            return true;
        }
        return false;
    }
};

#endif /* ANDERSENPASS_H_ */
