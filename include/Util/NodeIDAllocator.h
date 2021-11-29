//===- NodeIDAllocator.h -- Allocates node IDs on request ------------------------//

#ifndef NODEIDALLOCATOR_H_
#define NODEIDALLOCATOR_H_

#include "FastCluster/fastcluster.h"
#include "Util/SVFBasicTypes.h"
#include "MemoryModel/PointsTo.h"

namespace SVF
{

// Forward declare for the Clusterer.
class BVDataPTAImpl;

/// Allocates node IDs for objects and values, upon request, according to
/// some strategy which can be user-defined.
/// It is the job of SymbolTableInfo to tell the NodeIDAllocator when
/// all symbols have been allocated through endSymbolAllocation.
class NodeIDAllocator
{
public:
    /// Allocation strategy to use.
    enum Strategy
    {
        /// Allocate objects contiguously, separate from values, and vice versa.
        /// If [****...*****] is the space of unsigned integers, we allocate as,
        /// [ssssooooooo...vvvvvvv] (o = object, v = value, s = special).
        DENSE,
        /// Allocate objects objects and values sequentially, intermixed.
        SEQ,
        /// Allocate values and objects as they come in with a single counter.
        /// GEP objects are allocated as an offset from their base (see implementation
        /// of allocateGepObjectId). The purpose of this allocation strategy
        /// is human readability.
        DEBUG,
    };

    /// These nodes, and any nodes before them are assumed allocated
    /// as objects and values. For simplicity's sake, numObjects and
    /// numVals thus start at 4 (and the other counters are set
    /// appropriately).
    ///@{
    static const NodeID blackHoleObjectId;
    static const NodeID constantObjectId;
    static const NodeID blackHolePointerId;
    static const NodeID nullPointerId;
    ///@}

    /// Return (singleton) allocator.
    static NodeIDAllocator *get(void);

    /// Deletes the (singleton) allocator.
    static void unset(void);

    /// Allocate an object ID as determined by the strategy.
    NodeID allocateObjectId(void);

    /// Allocate a GEP object ID as determined by the strategy.
    /// allocateObjectId is still fine for GEP objects, but
    /// for some strategies (DEBUG, namely), GEP objects can
    /// be allocated differently (more readable, for DEBUG).
    /// Regardless, numObjects is shared; there is no special
    /// numGepObjects.
    NodeID allocateGepObjectId(NodeID base, u32_t offset, u32_t maxFieldLimit);

    /// Allocate a value ID as determined by the strategy.
    NodeID allocateValueId(void);

    /// Notify the allocator that all symbols have had IDs allocated.
    void endSymbolAllocation(void);

    /// Returns the total number of memory objects.
    NodeID getNumObjects(void) const { return numObjects; }

private:
    /// Builds a node ID allocator with the strategy specified on the command line.
    NodeIDAllocator(void);

private:
    /// These are moreso counters than amounts.
    ///@{
    /// Number of memory objects allocated, including specials.
    NodeID numObjects;
    /// Number of values allocated, including specials.
    NodeID numValues;
    /// Number of explicit symbols allocated (e.g., llvm::Values), including specials.
    NodeID numSymbols;
    /// Total number of objects and values allocated.
    NodeID numNodes;
    ///@}

    /// Strategy to allocate with.
    enum Strategy strategy;

    /// Single allocator.
    static NodeIDAllocator *allocator;

public:
    /// Perform clustering given points-to sets with nodes allocated according to the
    /// DENSE strategy.
    class Clusterer
    {
    private:
        /// Maps a pair of nodes to their (minimum) distance and the number of
        /// times that distance occurs in a set of *unique* points-to sets.
        typedef Map<NodePair, std::pair<unsigned, unsigned>> DistOccMap;

        /// Statistics strings.
        ///@{
        static const std::string NumObjects;
        static const std::string RegioningTime;
        static const std::string DistanceMatrixTime;
        static const std::string FastClusterTime;
        static const std::string DendrogramTraversalTime;
        static const std::string EvalTime;
        static const std::string TotalTime;
        static const std::string TheoreticalNumWords;
        static const std::string OriginalBvNumWords;
        static const std::string OriginalSbvNumWords;
        static const std::string NewBvNumWords;
        static const std::string NewSbvNumWords;
        static const std::string NumRegions;
        static const std::string NumGtIntRegions;
        static const std::string LargestRegion;
        static const std::string BestCandidate;
        static const std::string NumNonTrivialRegionObjects;
        ///@}

    public:
        /// Returns vector mapping previously allocated node IDs to a smarter allocation
        /// based on the points-to sets in pta accessed through keys.
        /// The second part of the keys pairs are the number of (potential) occurrences of that points-to set
        /// or a subset, depending on the client's wish.
        /// TODO: interfaces are getting unwieldy, an initialised object may be better. 
        /// TODO: kind of sucks pta can't be const here because getPts isn't.
        static std::vector<NodeID> cluster(BVDataPTAImpl *pta, const std::vector<std::pair<NodeID, unsigned>> keys, std::vector<std::pair<hclust_fast_methods, std::vector<NodeID>>> &candidates, std::string evalSubtitle="");

        // Returns a reverse node mapping for mapping generated by cluster().
        static std::vector<NodeID> getReverseNodeMapping(const std::vector<NodeID> &nodeMapping);

        /// Fills in *NumWords statistics in stats..
        static void evaluate(const std::vector<NodeID> &nodeMap, const Map<PointsTo, unsigned> pointsToSets, Map<std::string, std::string> &stats, bool accountForOcc);

        /// Prints statistics to SVFUtil::outs().
        /// TODO: make stats const.
        static void printStats(std::string title, Map<std::string, std::string> &stats);

    private:
        /// Returns an index into a condensed matrix (upper triangle, excluding diagonals) corresponding
        /// to an nxn matrix.
        static inline size_t condensedIndex(size_t n, size_t i, size_t j);

        /// Returns the minimum number of bits required to represent pts in a perfect world.
        static inline unsigned requiredBits(const PointsTo &pts);

        /// Returns the minimum number of bits required to represent n items in a perfect world.
        static inline unsigned requiredBits(const size_t n);

        /// Builds the upper triangle of the distance matrix, as an array of length
        /// (numObjects * (numObjects - 1)) / 2, as required by fastcluster.
        /// Responsibility of caller to `delete`.
        static inline double *getDistanceMatrix(const std::vector<std::pair<const PointsTo *, unsigned>> pointsToSets,
                                                const size_t numObjects, const Map<NodeID, unsigned> &nodeMap,
                                                double &distanceMatrixTime);

        /// Traverses the dendrogram produced by fastcluster, making node o, where o is the nth leaf (per
        /// recursive DFS) map to n. index is the dendrogram node to work off. The traversal should start
        /// at the top, which is the "last" (consider that it is 2D) element of the dendrogram, numObjects - 1.
        static inline void traverseDendrogram(std::vector<NodeID> &nodeMap, const int *dendrogram, const size_t numObjects, unsigned &allocCounter, Set<int> &visited, const int index, const std::vector<NodeID> &regionNodeMap);

        /// Returns a vector mapping object IDs to a label such that if two objects appear
        /// in the same points-to set, they have the same label. The "appear in the same
        /// points-to set" is encoded by graph which is an adjacency list ensuring that
        /// x in pt(p) and y in pt(p) -> x is reachable from y.
        static inline std::vector<unsigned> regionObjects(const Map<NodeID, Set<NodeID>> &graph, size_t numObjects, size_t &numLabels);

        // From all the candidates, returns the best mapping for pointsToSets (points-to set -> # occurences).
        static inline std::pair<hclust_fast_methods, std::vector<NodeID>> determineBestMapping(
            const std::vector<std::pair<hclust_fast_methods, std::vector<NodeID>>> &candidates,
            Map<PointsTo, unsigned> pointsToSets, const std::string &evalSubtitle, double &evalTime);

    };
};

}  // namespace SVF

#endif  // ifdef NODEIDALLOCATOR_H_
