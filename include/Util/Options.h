//===- Options.h -- Command line options ------------------------//

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "Util/NodeIDAllocator.h"
#include "Util/PointsTo.h"
#include "FastCluster/fastcluster.h"

namespace SVF
{

/// Carries around command line options.
class Options
{
public:
    Options(void) = delete;

    /// If set, only return the clock when getClk is called as getClk(true).
    /// Retrieving the clock is slow but it should be fine for a few calls.
    /// This is good for benchmarking when we don't need to know how long processLoad
    /// takes, for example (many calls), but want to know things like total solve time.
    /// Should be used only to affect getClk, not CLOCK_IN_MS.
    static const llvm::cl::opt<bool> MarkedClocksOnly;

    /// Allocation strategy to be used by the node ID allocator.
    /// Currently dense, seq, or debug.
    static const llvm::cl::opt<SVF::NodeIDAllocator::Strategy> NodeAllocStrat;

    /// Maximum number of field derivations for an object.
    static const llvm::cl::opt<unsigned> MaxFieldLimit;

    /// Whether to stage Andersen's with Steensgaard and cluster based on that data.
    static const llvm::cl::opt<bool> ClusterAnder;

    /// Whether to cluster FS or VFS with the auxiliary Andersen's.
    static const llvm::cl::opt<bool> ClusterFs;

    /// Type of points-to set to use for the main phase of a staged analysis (i.e., after
    /// clustering).
    static const llvm::cl::opt<PointsTo::Type> StagedPtType;

    /// Clustering method for ClusterFs/ClusterAnder.
    /// TODO: we can separate it into two options, and make Clusterer::cluster take in a method
    ///       argument rather than plugging Options::ClusterMethod *inside* Clusterer::cluster
    ///       directly, but it seems we will always want single anyway, and this is for testing.
    static const llvm::cl::opt<enum hclust_fast_methods> ClusterMethod;

    /// Cluster partitions separately.
    static const llvm::cl::opt<bool> PartitionedClustering;
};

};  // namespace SVF

#endif  // ifdef OPTIONS_H_
