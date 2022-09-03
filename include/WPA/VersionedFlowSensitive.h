//===- VersionedFlowSensitive.h -- Versioned flow-sensitive pointer analysis --------//

/*
 * VersionedFlowSensitiveAnalysis.h
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 *
 * The implementation is based on
 * Mohamad Barbar, Yulei Sui and Shiping Chen. "Object Versioning for Flow-Sensitive Pointer Analysis".
 * International Symposium on Code Generation and Optimization (CGO'21)
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
    typedef CoreBitVector MeldVersion;

public:
    typedef Map<NodeID, Version> ObjToVersionMap;
    typedef Map<VersionedVar, const DummyVersionPropSVFGNode *> VarToPropNodeMap;

    typedef std::vector<ObjToVersionMap> LocVersionMap;
    /// (o -> (v -> versions with rely on o:v).
    typedef Map<NodeID, Map<Version, std::vector<Version>>> VersionRelianceMap;

    /// If this version appears, there has been an error.
    static const Version invalidVersion;

    /// Return key into vPtD for address-taken var of a specific version.
    static VersionedVar atKey(NodeID, Version);

    /// Constructor
    VersionedFlowSensitive(SVFIR *_pag, PTATY type = VFS_WPA);

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
    static VersionedFlowSensitive *createVFSWPA(SVFIR *_pag)
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
    virtual bool propAlongIndirectEdge(const IndirectSVFGEdge*) override
    {
        return false;
    }

    /// Override since we want to assign different weights based on versioning.
    virtual void cluster(void) override;

private:
    /// Prelabel the SVFG: set y(o) for stores and c(o) for delta nodes to a new version.
    void prelabel(void);
    /// Meld label the prelabeled SVFG.
    void meldLabel(void);
    /// Melds v2 into v1 (in place), returns whether a change occurred.
    static bool meld(MeldVersion &mv1, const MeldVersion &mv2);

    /// Removes all indirect edges in the SVFG.
    void removeAllIndirectSVFGEdges(void);

    /// Propagates version v of o to any version of o which relies on v when o/v is changed.
    /// Recursively applies to reliant versions till no new changes are made.
    /// Adds any statements which rely on any changes made to the worklist.
    void propagateVersion(NodeID o, Version v);

    /// Propagates version v of o to version vp of o. time indicates whether it should record time
    /// taken itself.
    void propagateVersion(const NodeID o, const Version v, const Version vp, bool time=true);

    /// Fills in isStoreMap and isLoadMap.
    virtual void buildIsStoreLoadMaps(void);

    /// Returns true if l is a store node.
    virtual bool isStore(const NodeID l) const;

    /// Returns true if l is a load node.
    virtual bool isLoad(const NodeID l) const;

    /// Fills in deltaMap and deltaSourceMap for the SVFG.
    virtual void buildDeltaMaps(void);

    /// Returns true if l is a delta node, i.e., may get a new incoming indirect
    /// edge due to on-the-fly callgraph construction.
    virtual bool delta(const NodeID l) const;

    /// Returns true if l is a delta-source node, i.e., may get a new outgoing indirect
    /// edge to a delta node due to on-the-fly callgraph construction.
    virtual bool deltaSource(const NodeID l) const;

    /// Shared code for getConsume and getYield. They wrap this function.
    Version getVersion(const NodeID l, const NodeID o, const LocVersionMap &lvm) const;

    /// Returns the consumed version of o at l. If no such version exists, returns invalidVersion.
    Version getConsume(const NodeID l, const NodeID o) const;

    /// Returns the yielded version of o at l. If no such version exists, returns invalidVersion.
    Version getYield(const NodeID l, const NodeID o) const;

    /// Shared code for setConsume and setYield. They wrap this function.
    void setVersion(const NodeID l, const NodeID o, const Version v, LocVersionMap &lvm);

    /// Sets the consumed version of o at l to v.
    void setConsume(const NodeID l, const NodeID o, const Version v);

    /// Sets the yielded version of o at l to v.
    void setYield(const NodeID l, const NodeID o, const Version v);

    /// Returns the versions of o which rely on o:v.
    std::vector<Version> &getReliantVersions(const NodeID o, const Version v);

    /// Returns the statements which rely on o:v.
    NodeBS &getStmtReliance(const NodeID o, const Version v);

    /// Dumps versionReliance and stmtReliance.
    void dumpReliances(void) const;

    /// Dumps maps consume and yield.
    void dumpLocVersionMaps(void) const;

    /// Dumps a MeldVersion to stdout.
    static void dumpMeldVersion(MeldVersion &v);

    /// Maps locations to objects to a version. The object version is what is
    /// consumed at that location.
    LocVersionMap consume;
    /// Actual yield map. Yield analogue to consume.
    LocVersionMap yield;

    /// o -> (version -> versions which rely on it).
    VersionRelianceMap versionReliance;
    /// o x version -> statement nodes which rely on that o/version.
    Map<NodeID, Map<Version, NodeBS>> stmtReliance;

    /// Maps an <object, version> pair to the SVFG node indicating that pair
    /// needs to be propagated.
    VarToPropNodeMap versionedVarToPropNode;

    // Maps an object o to o' if o is equivalent to o' with respect to
    // versioning. Thus, we don't need to store the versions of o and look
    // up those for o' instead.
    Map<NodeID, NodeID> equivalentObject;

    /// Worklist for performing meld labeling, takes SVFG node l.
    /// Nodes are added when the version they yield is changed.
    FIFOWorkList<NodeID> vWorklist;

    Set<NodeID> prelabeledObjects;

    /// Points-to DS for working with versions.
    BVDataPTAImpl::VersionedPTDataTy *vPtD;

    /// deltaMap[l] means SVFG node l is a delta node, i.e., may get new
    /// incoming edges due to OTF callgraph construction.
    std::vector<bool> deltaMap;

    /// deltaSourceMap[l] means SVFG node l *may* be a source to a delta node
    /// through an dge added as a result of on-the-fly callgraph
    /// construction.
    std::vector<bool> deltaSourceMap;

    /// isStoreMap[l] means SVFG node l is a store node.
    std::vector<bool> isStoreMap;

    /// isLoadMap[l] means SVFG node l is a load node.
    std::vector<bool> isLoadMap;

    /// Additional statistics.
    //@{
    u32_t numPrelabeledNodes;  ///< Number of prelabeled nodes.
    u32_t numPrelabelVersions; ///< Number of versions created during prelabeling.

    double prelabelingTime;  ///< Time to prelabel SVFG.
    double meldLabelingTime; ///< Time to meld label SVFG.
    double versionPropTime;  ///< Time to propagate versions to versions which rely on them.
    //@}

    static VersionedFlowSensitive *vfspta;

    class SCC
    {
    private:
        typedef struct NodeData
        {
            int index;
            int lowlink;
            bool onStack;
        } NodeData;

    public:
        /// Determines the strongly connected components of svfg following only
        /// edges labelled with object. partOf[n] = scc means nodes n is part of
        /// SCC scc. startingNodes contains the nodes to begin the search from.
        /// After completion, footprint will contain all edges which object
        /// appears on (as reached through the algorithm described above) sorted.
        ///
        /// This is not a general SCC detection but specifically for versioning,
        /// so edges to delta nodes are skipped as they are prelabelled. Edges
        /// to stores are also skipped to as they yield a new version (they
        /// cannot be part of an SCC containing more than themselves).
        /// Skipped edges still form part of the footprint.
        static unsigned detectSCCs(VersionedFlowSensitive *vfs,
                                   const SVFG *svfg, const NodeID object,
                                   const std::vector<const SVFGNode *> &startingNodes,
                                   std::vector<int> &partOf,
                                   std::vector<const IndirectSVFGEdge *> &footprint);

    private:
        /// Called by detectSCCs then called recursively.
        static void visit(VersionedFlowSensitive *vfs,
                          const NodeID object,
                          std::vector<int> &partOf,
                          std::vector<const IndirectSVFGEdge *> &footprint,
                          std::vector<NodeData> &nodeData,
                          std::stack<const SVFGNode *> &stack,
                          int &index,
                          int &currentSCC,
                          const SVFGNode *v);
    };
};

} // End namespace SVF

#endif /* VFS_H_ */
