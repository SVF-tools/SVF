//===- VersionedFlowSensitive.h -- Versioned flow-sensitive pointer analysis --------//

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
#include "MemoryModel/PointsTo.h"

namespace SVF
{

class AndersenWaveDiff;
class SVFModule;

/*!
 * Versioned flow sensitive whole program pointer analysis
 */
class VersionedFlowSensitive : public FlowSensitive
{
    friend class VersionedFlowSensitiveStat;

private:
    typedef llvm::SparseBitVector<> MeldVersion;

public:
    typedef Map<NodeID, Version> ObjToVersionMap;
    typedef Map<NodeID, MeldVersion> ObjToMeldVersionMap;
    typedef Map<VersionedVar, const DummyVersionPropSVFGNode *> VarToPropNodeMap;

    typedef Map<NodeID, ObjToVersionMap> LocVersionMap;
    /// Maps locations to all versions it sees (through objects).
    typedef Map<NodeID, ObjToMeldVersionMap> LocMeldVersionMap;
    /// (o -> (v -> versions with rely on o:v).
    typedef Map<NodeID, Map<Version, std::vector<Version>>> VersionRelianceMap;

    /// For caching the first step in LocVersionMaps.
    typedef struct VersionCache
    {
        /// SVFG node ID.
        NodeID l;
        /// Nested map, i.e. consume[l] or yield[l].
        ObjToVersionMap *ovm;
        /// Whether l and ovm can be accessed.
        bool valid;
    } VersionCache;

    /// If this version appears, there has been an error.
    static const Version invalidVersion;

    /// Return key into vPtD for address-taken var of a specific version.
    static VersionedVar atKey(NodeID, Version);

    /// Constructor
    VersionedFlowSensitive(PAG *_pag, PTATY type = VFS_WPA);

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

    /// Create single instance of versioned flow-sensitive points-to analysis.
    static VersionedFlowSensitive *createVFSWPA(PAG *_pag)
    {
        if (vfspta == nullptr)
        {
            vfspta = new VersionedFlowSensitive(_pag);
            vfspta->analyze();
        }

        return vfspta;
    }

    /// Release flow-sensitive pointer analysis
    static void releaseVFSWPA()
    {
        if (vfspta) delete vfspta;
        vfspta = nullptr;
    }

protected:
    virtual bool processLoad(const LoadSVFGNode* load) override;
    virtual bool processStore(const StoreSVFGNode* store) override;
    virtual void processNode(NodeID n) override;
    virtual void updateConnectedNodes(const SVFGEdgeSetTy& newEdges) override;

    /// Override to do nothing. Instead, we will use propagateVersion when necessary.
    virtual bool propAlongIndirectEdge(const IndirectSVFGEdge*) override { return false; }

    /// Override since we want to assign different weights based on versioning.
    virtual void cluster(void) override;

private:
    /// Prelabel the SVFG: set y(o) for stores and c(o) for delta nodes to a new version.
    void prelabel(void);
    /// Meld label the prelabeled SVFG.
    void meldLabel(void);
    /// Melds v2 into v1 (in place), returns whether a change occurred.
    static bool meld(MeldVersion &mv1, const MeldVersion &mv2);

    /// Moves meldConsume/Yield to consume/yield.
    void mapMeldVersions();

    /// Returns a new MeldVersion for o during the prelabeling phase.
    MeldVersion newMeldVersion(NodeID o);

    /// Determine which versions rely on which versions (e.g. c_l'(o) relies on y_l(o)
    /// given l-o->l' and y_l(o) = a, c_l'(o) = b), and which statements rely on which
    /// versions (e.g. node l relies on c_l(o)).
    void determineReliance(void);

    /// Propagates version v of o to any version of o which relies on v when o/v is changed.
    /// Recursively applies to reliant versions till no new changes are made.
    /// Adds any statements which rely on any changes made to the worklist.
    void propagateVersion(NodeID o, Version v);

    /// Propagates version v of o to version vp of o. time indicates whether it should record time
    /// taken itself.
    void propagateVersion(const NodeID o, const Version v, const Version vp, bool time=true);

    /// Returns true if l is a delta node, i.e., may have new incoming edges due to
    /// on-the-fly call graph resolution. approxCallGraph is the over-approximate
    /// call graph built by the pre-analysis.
    virtual bool delta(NodeID l) const;

    /// Shared code for getConsume and getYield. They wrap this function.
    Version getVersion(const NodeID l, const NodeID o, VersionCache &cache, LocVersionMap &lvm);

    /// Returns the consumed version of o at l. If no such version exists, returns invalidVersion.
    Version getConsume(const NodeID l, const NodeID o);

    /// Returns the yielded version of o at l. If no such version exists, returns invalidVersion.
    Version getYield(const NodeID l, const NodeID o);

    /// Shared code for setConsume and setYield. They wrap this function.
    void setVersion(const NodeID l, const NodeID o, const Version v, VersionCache &cache, LocVersionMap &lvm);

    /// Sets the consumed version of o at l to v.
    void setConsume(const NodeID l, const NodeID o, const Version v);

    /// Sets the yielded version of o at l to v.
    void setYield(const NodeID l, const NodeID o, const Version v);

    /// Invalidates yieldCache.
    void invalidateYieldCache(void);

    /// Invalidates consumeCache.
    void invalidateConsumeCache(void);

    /// Dumps versionReliance and stmtReliance.
    void dumpReliances(void) const;

    /// Dumps maps consume and yield.
    void dumpLocVersionMaps(void) const;

    /// Dumps a MeldVersion to stdout.
    static void dumpMeldVersion(MeldVersion &v);

    /// SVFG node (label) x object -> version to consume.
    /// Used during meld labeling. We use MeldVersions and Versions for performance.
    /// MeldVersions are currently SparseBitVectors which are necessary for the meld operator,
    /// but when meld labeling is complete, we don't want to carry around SBVs and use them; integers
    /// are better.
    LocMeldVersionMap meldConsume;
    /// SVFG node (label) x object -> version to yield.
    /// Used during meld labeling.
    /// For non-stores, yield == consume, so meldYield only has entries for stores.
    LocMeldVersionMap meldYield;
    /// Object -> MeldVersion counter. Used in the prelabeling phase to generate a
    /// new MeldVersion.
    Map<NodeID, unsigned> meldVersions;

    /// Like meldConsume but with Versions, not MeldVersions.
    /// Created after meld labeling from meldConsume and used during the analysis.
    /// When modifying consume itself (not a value) outside of setConsume, invalidateConsumeCache
    /// should be called. E.g. if a call like consume[l] is an insertion.
    LocVersionMap consume;
    /// Actual yield map. Yield analogue to consume.
    /// When modifying yield itself (not a value) outside of setYield, invalidateYieldCache
    /// should be called.
    LocVersionMap yield;

    /// Cache for the nested map in consume.
    VersionCache consumeCache;
    /// Cache for the nested map in yield.
    VersionCache yieldCache;

    /// o -> (version -> versions which rely on it).
    VersionRelianceMap versionReliance;
    /// o x version -> statement nodes which rely on that o/version.
    Map<NodeID, Map<Version, NodeBS>> stmtReliance;

    /// Maps an <object, version> pair to the SVFG node indicating that pair
    /// needs to be propagated.
    VarToPropNodeMap versionedVarToPropNode;

    /// Worklist for performing meld labeling, takes SVFG node l.
    /// Nodes are added when the version they yield is changed.
    FIFOWorkList<NodeID> vWorklist;

    /// Points-to DS for working with versions.
    BVDataPTAImpl::VersionedPTDataTy *vPtD;

    /// Additional statistics.
    //@{
    Size_t numPrelabeledNodes;  ///< Number of prelabeled nodes.
    Size_t numPrelabelVersions; ///< Number of versions created during prelabeling.

    double relianceTime;     ///< Time to determine version and statement reliance.
    double prelabelingTime;  ///< Time to prelabel SVFG.
    double meldLabelingTime; ///< Time to meld label SVFG.
    double meldMappingTime;  ///< Time to map MeldVersions to Versions.
    double versionPropTime;  ///< Time to propagate versions to versions which rely on them.
    //@}

    static VersionedFlowSensitive *vfspta;
};

} // End namespace SVF

#endif /* VFS_H_ */
