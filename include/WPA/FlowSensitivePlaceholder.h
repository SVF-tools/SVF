//===- FlowSensitivePlaceholder.h -- Flow-sensitive pointer analysis---------------------//

/*
 * FlowSensitiveAnalysisPlaceholder.h
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#ifndef FSPH_H_
#define FSPH_H_

#include "Graphs/SVFGOPT.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/FlowSensitive.h"
#include "WPA/WPAFSSolver.h"
class AndersenWaveDiff;
class SVFModule;

/*!
 * Flow sensitive whole program pointer analysis
 */
class FlowSensitivePlaceholder : public FlowSensitive
{
public:
    enum VersionType {
        CONSUME,
        YIELD,
    };

    /// Constructor
    FlowSensitivePlaceholder(PTATY type = FSPH_WPA) : FlowSensitive(type)
    {
    }

    /// Flow sensitive analysis
    // virtual void analyze(SVFModule* svfModule) override;

    /// Initialize analysis
    virtual void initialize(SVFModule* svfModule) override;

    /// Finalize analysis
    // virtual void finalize() override;

    /// Get PTA name
    virtual const std::string PTAName() const override
    {
        return "FlowSensitivePlaceholder";
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast
    //@{
    static inline bool classof(const FlowSensitivePlaceholder *)
    {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return pta->getAnalysisTy() == FSPH_WPA;
    }
    //@}

protected:
    virtual bool processLoad(const LoadSVFGNode* load) override;
    virtual bool processStore(const StoreSVFGNode* store) override;

private:
    /// Precolour the split SVFG.
    void precolour(void);
    /// Colour the precoloured split SVFG.
    void colour(void);
    /// Melds v2 into v1 (in place), returns whether a change occurred.
    bool meld(Version &v1, Version &v2);

    /// Returns whether l is a delta node.
    bool delta(NodeID l) const;

    /// Returns a new version for o.
    Version newVersion(NodeID o);
    /// Whether l has a consume/yield version for o. fsph-TODO: const.
    bool hasVersion(NodeID l, NodeID o, enum VersionType v) const;

    /// Determine which versions rely on which versions, and which statements
    /// rely on which versions.
    void determineReliance(void);

    /// SVFG node (label) x object -> version to consume.
    DenseMap<NodeID, DenseMap<NodeID, Version>> consume;
    /// SVFG node (label) x object -> version to yield.
    DenseMap<NodeID, DenseMap<NodeID, Version>> yield;
    /// Object -> version counter.
    DenseMap<NodeID, unsigned> versions;
    /// o -> (version -> versions which rely on it).
    DenseMap<NodeID, DenseMap<Version, DenseSet<Version>>> versionReliance;
    /// o x version -> statement nodes which rely on that o/version.
    DenseMap<NodeID, DenseMap<Version, DenseSet<NodeID>>> stmtReliance;

    /// Worklist for performing meld colouring, takes SVFG node l.
    FIFOWorkList<NodeID> vWorklist;

    /// Points-to DS for working with versions.
    BVDataPTAImpl::VDFPTDataTy *vPtD;
};

#endif /* FSPH_H_ */
