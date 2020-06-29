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

typedef unsigned Version;

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
    FlowSensitivePlaceholder(PTATY type = FSPH_WPA) : FlowSensitive()
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

private:
    /// Distribute yielded/consumed versions at each SVFG node.
    void distributeVersions(void);
    /// Set yielded/consumed versions at a single node.
    void setVersions(const SVFGNode *s);
    /// Returns a new version for o.
    Version newVersion(NodeID o);
    /// Whether l has a consume/yield version for o. fsph-TODO: const.
    bool hasVersion(NodeID l, NodeID o, enum VersionType v) const;

    /// SVFG node (label) x object -> version to consume.
    DenseMap<NodeID, DenseMap<NodeID, Version>> consume;
    /// SVFG node (label) x object -> version to yield.
    DenseMap<NodeID, DenseMap<NodeID, Version>> yield;
    /// Object -> version (counter).
    DenseMap<NodeID, Version> versions;
};

#endif /* FSPH_H_ */
