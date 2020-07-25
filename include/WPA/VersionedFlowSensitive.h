//===- VersionedFlowSensitive.h -- Flow-sensitive pointer analysis---------------------//

/*
 * VersionedFlowSensitiveAnalysis.h
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#ifndef VFS_H_
#define VFS_H_

#include "Graphs/SVFGOPT.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/FlowSensitive.h"
#include "WPA/WPAFSSolver.h"
class AndersenWaveDiff;
class SVFModule;

/*!
 * Flow sensitive whole program pointer analysis
 */
class VersionedFlowSensitive : public FlowSensitive
{
    friend class VersionedFlowSensitiveStat;
public:
    typedef BVDataPTAImpl::VDFPTDataTy::ObjToVersionMap ObjToVersionMap;
    typedef DenseMap<NodeID, MeldVersion> ObjToMeldVersionMap;

    typedef BVDataPTAImpl::VDFPTDataTy::LocVersionMap LocVersionMap;
    /// Maps locations to all versions it sees (through objects).
    typedef DenseMap<NodeID, ObjToMeldVersionMap> LocMeldVersionMap;

    enum VersionType {
        CONSUME,
        YIELD,
    };

    /// Constructor
    VersionedFlowSensitive(PAG *_pag, PTATY type = VFS_WPA);

    /// Flow sensitive analysis
    // virtual void analyze(SVFModule* svfModule) override;

    /// Initialize analysis
    virtual void initialize() override;

    /// Finalize analysis
    virtual void finalize() override;

    /// Get PTA name
    virtual const std::string PTAName() const override
    {
        return "VersionedFlowSensitive";
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast
    //@{
    static inline bool classof(const VersionedFlowSensitive *)
    {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return pta->getAnalysisTy() == VFS_WPA;
    }
    //@}

protected:
    virtual bool processLoad(const LoadSVFGNode* load) override;
    virtual bool processStore(const StoreSVFGNode* store) override;
    virtual void processNode(NodeID n) override;
    virtual void updateConnectedNodes(const SVFGEdgeSetTy& newEdges);

    /// Override to do nothing. Instead, we will use propagateVersion when necessary.
    virtual bool propAlongIndirectEdge(const IndirectSVFGEdge* edge) override { return false; }

private:
    /// Precolour the split SVFG.
    void precolour(void);
    /// Colour the precoloured split SVFG.
    void colour(void);
    /// Melds v2 into v1 (in place), returns whether a change occurred.
    bool meld(MeldVersion &mv1, MeldVersion &mv2);

    /// Moves meldConsume/Yield to consume/yield.
    void mapMeldVersions(LocMeldVersionMap &from, LocVersionMap &to);

    /// Returns whether l is a delta node.
    bool delta(NodeID l) const;

    /// Returns a new MeldVersion for o.
    MeldVersion newMeldVersion(NodeID o);
    /// Whether l has a consume/yield version for o.
    bool hasVersion(NodeID l, NodeID o, enum VersionType v) const;
    /// Whether l has a consume/yield MeldVersion for o.
    bool hasMeldVersion(NodeID l, NodeID o, enum VersionType v) const;

    /// Determine which versions rely on which versions, and which statements
    /// rely on which versions.
    void determineReliance(void);

    /// Propagates version v of o to any version of o which relies on v.
    /// Recursively applies to reliant versions till no new changes are made.
    /// Adds any statements which rely on any changes made to the worklist.
    void propagateVersion(NodeID o, Version v);

    /// Dumps versionReliance and stmtReliance.
    void dumpReliances(void) const;

    /// SVFG node (label) x object -> version to consume.
    /// Used during colouring.
    LocMeldVersionMap meldConsume;
    /// SVFG node (label) x object -> version to yield.
    /// Used during colouring.
    LocMeldVersionMap meldYield;
    /// Object -> MeldVersion counter.
    DenseMap<NodeID, unsigned> meldVersions;

    /// Actual consume map.
    LocVersionMap consume;
    /// Actual yield map.
    LocVersionMap yield;

    /// o -> (version -> versions which rely on it).
    DenseMap<NodeID, DenseMap<Version, DenseSet<Version>>> versionReliance;
    /// o x version -> statement nodes which rely on that o/version.
    DenseMap<NodeID, DenseMap<Version, DenseNodeSet>> stmtReliance;

    /// Worklist for performing meld colouring, takes SVFG node l.
    FIFOWorkList<NodeID> vWorklist;

    /// Points-to DS for working with versions.
    BVDataPTAImpl::VDFPTDataTy *vPtD;

    /// Additional statistics.
    //@{
    Size_t numPrecolouredNodes;  ///< Number of precoloured nodes.
    Size_t numPrecolourVersions; ///< Number of versions created during precolouring.

    double relianceTime;     ///< Time to determine version and statement reliance.
    double precolouringTime; ///< Time to precolour SVFG.
    double colouringTime;    ///< Time to colour SVFG.
    double meldMappingTime;  ///< Time to map MeldVersions to Versions.
    double versionPropTime;  ///< Time to propagate versions to versions which rely on them.
    //@}
};

#endif /* VFS_H_ */
