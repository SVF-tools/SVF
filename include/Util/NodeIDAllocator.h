//===- NodeIDAllocator.h -- Allocates node IDs on request ------------------------//

#ifndef NODEIDALLOCATOR_H_
#define NODEIDALLOCATOR_H_

#include "Util/SVFBasicTypes.h"

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
        /// Used to initialise from llvm::cl::opt.
        NONE,
        /// Allocate objects contiguously, separate from values, and vice versa.
        /// If [****...*****] is the space of unsigned integers, we allocate as,
        /// [ssssooooooo...vvvvvvv] (o = object, v = value, s = special).
        DENSE,
        /// Allocate values and objects as they come in with a single counter.
        /// GEP objects are allocated as an offset from their base (see implementation
        /// of allocateGepObjectId). The purpose of this allocation strategy
        /// is human readability.
        DEBUG,
    };

    /// Option strings as written by the user.
    ///@{
    static const std::string userStrategyDense;
    static const std::string userStrategyDebug;
    ///@}

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

    /// Strategy to allocate with. Initially NONE.
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
        static const std::string DistanceMatrixTime;
        static const std::string FastClusterTime;
        static const std::string DendogramTraversalTime;
        static const std::string TotalTime;
        static const std::string TheoreticalNumWords;
        static const std::string OriginalBvNumWords;
        static const std::string OriginalSbvNumWords;
        static const std::string NewBvNumWords;
        static const std::string NewSbvNumWords;
        ///@}

    public:
        /// Returns vector mapping previously allocated node IDs to a smarter allocation
        /// based on the points-to sets in pta accessed through keys.
        /// TODO: kind of sucks pta can't be const here because getPts isn't.
        static std::vector<NodeID> cluster(BVDataPTAImpl *pta, const std::vector<NodeID> keys, bool eval=false);

    private:
        /// Returns an index into a condensed matrix (upper triangle, excluding diagonals) corresponding
        /// to an nxn matrix.
        static inline size_t condensedIndex(size_t n, size_t i, size_t j);

        /// Returns the minimum number of bits required to represent pts in a perfect world.
        static inline unsigned requiredBits(const PointsTo &pts);

        /// Builds the upper triangle of the distance matrix, as an array of length
        /// (numObjects * (numObjects - 1)) / 2, as required by fastcluster.
        /// Responsibility of caller to `delete`.
        static inline double *getDistanceMatrix(const Set<PointsTo> pointsToSets, const unsigned numObjects);

        /// Traverses the dendogram produced by fastcluster, making node o, where o is the nth leaf (per
        /// recursive DFS) map to n. index is the dendogram node to work off. The traversal should start
        /// at the top, which is the "last" (consider that it is 2D) element of the dendogram, numObjects - 1.
        static inline void traverseDendogram(std::vector<NodeID> &nodeMap, const int *dendogram, const unsigned numObjects, unsigned &allocCounter, Set<int> &visited, const int index);

        /// Fills in *NumWords statistics in stats..
        static inline void evaluate(const std::vector<NodeID> &nodeMap, const Set<PointsTo> pointsToSets, Map<std::string, std::string> &stats);

        /// Prints statistics to SVFUtil::outs().
        /// TODO: make stats const.
        static inline void printStats(Map<std::string, std::string> &stats);
    };
};

};  // namespace SVF

#endif  // ifdef NODEIDALLOCATOR_H_
