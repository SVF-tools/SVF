//===- FlowSensitiveTBHC.h -- flow-sensitive type filter ----------------//

/*
 * FlowSensitiveTBHC.h
 *
 *  Created on: Oct 08, 2019
 *      Author: Mohamad Barbar
 */

#ifndef FLOWSENSITIVETYPEFILTER_H_
#define FLOWSENSITIVETYPEFILTER_H_

#include "SVF-FE/DCHG.h"
#include "Graphs/SVFGOPT.h"
#include "MSSA/SVFGBuilder.h"
#include "Util/TypeBasedHeapCloning.h"
#include "WPA/FlowSensitive.h"

namespace SVF
{

class SVFModule;

/*!
 * Flow sensitive whole program pointer analysis with type-based heap cloning.
 */
class FlowSensitiveTBHC : public FlowSensitive, public TypeBasedHeapCloning
{
public:
    /// Returns raw ctir metadata of the instruction behind a SVFG node.
    /// Wraps getRawCTirMetadata(const Value *). Returns null if it doesn't exist.
    static const MDNode *getRawCTirMetadata(const SVFGNode *);

    /// Constructor
    FlowSensitiveTBHC(PAG* _pag, PTATY type = FSTBHC_WPA);

    /// Destructor
    virtual ~FlowSensitiveTBHC() { };

    /// Flow sensitive analysis with FSTBHC.
    virtual void analyze() override;
    /// Initialize analysis.
    virtual void initialize() override;
    /// Finalize analysis.
    virtual void finalize() override;

    /// Get PTA name
    virtual const std::string PTAName() const override
    {
        return "FSTBHC";
    }

    /// For LLVM RTTI.
    static inline bool classof(const FlowSensitiveTBHC *)
    {
        return true;
    }

    /// For LLVM RTTI.
    static inline bool classof(const PointerAnalysis *pta)
    {
        return pta->getAnalysisTy() == FSTBHC_WPA;
    }

    virtual bool propAlongIndirectEdge(const IndirectSVFGEdge* edge) override;
    virtual bool propAlongDirectEdge(const DirectSVFGEdge* edge) override;

    virtual bool processAddr(const AddrSVFGNode* addr) override;
    virtual bool processGep(const GepSVFGNode* gep) override;
    virtual bool processLoad(const LoadSVFGNode* load) override;
    virtual bool processStore(const StoreSVFGNode* store) override;
    virtual bool processPhi(const PHISVFGNode* phi) override;
    virtual bool processCopy(const CopySVFGNode* copy) override;

    virtual inline const NodeBS& getAllFieldsObjNode(NodeID id) override;

    virtual inline bool updateInFromIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar) override;
    virtual inline bool updateInFromOut(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar) override;

    virtual inline bool unionPtsFromIn(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar) override;
    virtual inline bool unionPtsFromTop(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar) override;

    virtual inline bool propDFOutToIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar) override;
    virtual inline bool propDFInToIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar) override;

    virtual void expandFIObjs(const PointsTo& pts, PointsTo& expandedPts) override;

    /// Extracts the value from SVFGNode (if it exists), and calls
    /// getTypeFromCTirMetadata(const Value *).
    /// If no ctir type exists, returns null (void).
    const DIType *getTypeFromCTirMetadata(const SVFGNode *);

protected:
    virtual void backPropagate(NodeID clone) override;

    virtual void countAliases(Set<std::pair<NodeID, NodeID>> cmp, unsigned *mayAliases, unsigned *noAliases) override;

private:
    /// Determines whether each GEP is for a load or not. Builds gepIsLoad map.
    /// This is a quick heuristic; if all destination nodes are loads, it's a load.
    void determineWhichGepsAreLoads(void);

    /// Returns true if the given GEP is for loads, false otherwise. If the node ID
    /// is not for a GEP SVFG node, returns false.
    bool gepIsLoad(NodeID gep);

    /// Whether to allow for reuse at stores.
    bool storeReuse;
    /// Whether to allow reuse at all instructions (load/store/field).
    /// allReuse => storeReuse.
    bool allReuse;

    /// Maps GEP objects to the SVFG nodes that retrieved them with getGepObjClones.
    Map<NodeID, NodeBS> gepToSVFGRetrievers;
    /// Maps whether a (SVFG) GEP node is a load or not.
    NodeBS loadGeps;
};

} // End namespace SVF

#endif /* FLOWSENSITIVETYPEFILTER_H_ */
