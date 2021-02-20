//===- NodeIDAllocator.cpp -- Allocates node IDs on request ------------------------//

#include <iomanip>
#include <iostream>

#include "FastCluster/fastcluster.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "MemoryModel/PTAStat.h"
#include "Util/NodeIDAllocator.h"
#include "Util/BasicTypes.h"
#include "Util/SVFBasicTypes.h"
#include "Util/SVFUtil.h"
#include "Util/PointsTo.h"
#include "Util/Options.h"

namespace SVF
{
    const NodeID NodeIDAllocator::blackHoleObjectId = 0;
    const NodeID NodeIDAllocator::constantObjectId = 1;
    const NodeID NodeIDAllocator::blackHolePointerId = 2;
    const NodeID NodeIDAllocator::nullPointerId = 3;

    NodeIDAllocator *NodeIDAllocator::allocator = nullptr;

    NodeIDAllocator *NodeIDAllocator::get(void)
    {
        if (allocator == nullptr)
        {
            allocator = new NodeIDAllocator();
        }

        return allocator;
    }

    void NodeIDAllocator::unset(void)
    {
        if (allocator != nullptr)
        {
            delete allocator;
        }
    }

    // Initialise counts to 4 because that's how many special nodes we have.
    NodeIDAllocator::NodeIDAllocator(void)
        : numNodes(4), numObjects(4), numValues(4), numSymbols(4), strategy(Options::NodeAllocStrat)
    { }

    NodeID NodeIDAllocator::allocateObjectId(void)
    {
        NodeID id = 0;
        if (strategy == Strategy::DENSE)
        {
            // We allocate objects from 0(-ish, considering the special nodes) to # of objects.
            id = numObjects;
        }
        else if (strategy == Strategy::SEQ)
        {
            // Everything is sequential and intermixed.
            id = numNodes;
        }
        else if (strategy == Strategy::DEBUG)
        {
            // Non-GEPs just grab the next available ID.
            // We may have "holes" because GEPs increment the total
            // but allocate far away. This is not a problem because
            // we don't care about the relative distances between nodes.
            id = numNodes;
        }
        else
        {
            assert(false && "NodeIDAllocator::allocateObjectId: unimplemented node allocation strategy.");
        }

        ++numObjects;
        ++numNodes;

        assert(id != 0 && "NodeIDAllocator::allocateObjectId: ID not allocated");
        return id;
    }

    NodeID NodeIDAllocator::allocateGepObjectId(NodeID base, u32_t offset, u32_t maxFieldLimit)
    {
        NodeID id = 0;
        if (strategy == Strategy::DENSE)
        {
            // Nothing different to the other case.
            id =  numObjects;
        }
        else if (strategy == Strategy::SEQ)
        {
            // Everything is sequential and intermixed.
            id = numNodes;
        }
        else if (strategy == Strategy::DEBUG)
        {
            // For a gep id, base id is set at lower bits, and offset is set at higher bits
            // e.g., 1100050 denotes base=50 and offset=10
            // The offset is 10, not 11, because we add 1 to the offset to ensure that the
            // high bits are never 0. For example, we do not want the gep id to be 50 when
            // the base is 50 and the offset is 0.
            NodeID gepMultiplier = pow(10, ceil(log10(
                                                    numSymbols > maxFieldLimit ?
                                                    numSymbols : maxFieldLimit
                                                )));
            id = (offset + 1) * gepMultiplier + base;
            assert(id > numNodes && "NodeIDAllocator::allocateGepObjectId: GEP allocation clashing with other nodes");
        }
        else
        {
            assert(false && "NodeIDAllocator::allocateGepObjectId: unimplemented node allocation strategy");
        }

        ++numObjects;
        ++numNodes;

        assert(id != 0 && "NodeIDAllocator::allocateGepObjectId: ID not allocated");
        return id;
    }

    NodeID NodeIDAllocator::allocateValueId(void)
    {
        NodeID id = 0;
        if (strategy == Strategy::DENSE)
        {
            // We allocate values from UINT_MAX to UINT_MAX - # of values.
            // TODO: UINT_MAX does not allow for an easily changeable type
            //       of NodeID (though it is already in use elsewhere).
            id = UINT_MAX - numValues;
        }
        else if (strategy == Strategy::SEQ)
        {
            // Everything is sequential and intermixed.
            id = numNodes;
        }
        else if (strategy == Strategy::DEBUG)
        {
            id = numNodes;
        }
        else
        {
            assert(false && "NodeIDAllocator::allocateValueId: unimplemented node allocation strategy");
        }

        ++numValues;
        ++numNodes;

        assert(id != 0 && "NodeIDAllocator::allocateValueId: ID not allocated");
        return id;
    }

    void NodeIDAllocator::endSymbolAllocation(void)
    {
        numSymbols = numNodes;
    }

    const std::string NodeIDAllocator::Clusterer::NumObjects = "NumObjects";
    const std::string NodeIDAllocator::Clusterer::DistanceMatrixTime = "DistanceMatrixTime";
    const std::string NodeIDAllocator::Clusterer::FastClusterTime = "FastClusterTime";
    const std::string NodeIDAllocator::Clusterer::DendogramTraversalTime = "DendogramTravTime";
    const std::string NodeIDAllocator::Clusterer::TotalTime = "TotalTime";
    const std::string NodeIDAllocator::Clusterer::TheoreticalNumWords = "TheoreticalWords";
    const std::string NodeIDAllocator::Clusterer::OriginalBvNumWords = "OriginalBvWords";
    const std::string NodeIDAllocator::Clusterer::OriginalSbvNumWords = "OriginalSbvWords";
    const std::string NodeIDAllocator::Clusterer::NewBvNumWords = "NewBvWords";
    const std::string NodeIDAllocator::Clusterer::NewSbvNumWords = "NewSbvWords";

    std::vector<NodeID> NodeIDAllocator::Clusterer::cluster(BVDataPTAImpl *pta, const std::vector<std::pair<NodeID, unsigned>> keys, bool eval)
    {
        assert(pta != nullptr && "Clusterer::cluster: given null BVDataPTAImpl");

        Map<std::string, std::string> stats;
        double totalTime = 0;

        // Pair of nodes to their (minimum) distance and the number of occurrences of that distance.
        Map<std::pair<NodeID, NodeID>, std::pair<unsigned, unsigned>> distances;

        double clkStart = PTAStat::getClk(true);
        // Map points-to sets to occurrences.
        Map<PointsTo, unsigned> pointsToSets;
        // Number of objects we're reallocating is the largest node (recall, we
        // assume a dense allocation, which goes from 0--modulo specials--to some n).
        // If we "allocate" for specials, it is not a problem except 2 potentially wasted
        // allocations. This is trivial enough to make special handling not worth it.
        unsigned numObjects = 0;
        for (const std::pair<NodeID, unsigned> &keyOcc : keys)
        {
            const PointsTo &pts = pta->getPts(keyOcc.first);
            for (const NodeID o : pts) if (o >= numObjects) numObjects = o + 1;
            pointsToSets[pts] += keyOcc.second;;
        }

        stats[NumObjects] = std::to_string(numObjects);

        // Mapping we'll return.
        std::vector<NodeID> nodeMap(numObjects, UINT_MAX);

        double *distMatrix = getDistanceMatrix(pointsToSets, numObjects);
        double clkEnd = PTAStat::getClk(true);
        double time = (clkEnd - clkStart) / TIMEINTERVAL;
        totalTime += time;
        stats[DistanceMatrixTime] = std::to_string(time);

        clkStart = PTAStat::getClk(true);
        int *dendogram = new int[2 * (numObjects - 1)];
        double *height = new double[numObjects - 1];
        // TODO: parameterise method.
        hclust_fast(numObjects, distMatrix, HCLUST_METHOD_SINGLE, dendogram, height);
        delete[] distMatrix;
        // We never use the height.
        delete[] height;
        clkEnd = PTAStat::getClk(true);
        time = (clkEnd - clkStart) / TIMEINTERVAL;
        totalTime += time;
        stats[FastClusterTime] = std::to_string(time);

        clkStart = PTAStat::getClk(true);
        unsigned allocCounter = 0;
        Set<int> visited;
        traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, numObjects - 1);
        delete[] dendogram;
        clkEnd = PTAStat::getClk(true);
        time = (clkEnd - clkStart) / TIMEINTERVAL;
        totalTime += time;
        stats[DendogramTraversalTime] = std::to_string(time);

        stats[TotalTime] = std::to_string(totalTime);

        if (eval) evaluate(nodeMap, pointsToSets, stats);
        printStats(stats);

        return nodeMap;
    }

    size_t NodeIDAllocator::Clusterer::condensedIndex(size_t n, size_t i, size_t j)
    {
        // From https://stackoverflow.com/a/14839010
        return n*(n-1)/2 - (n-i)*(n-i-1)/2 + j - i - 1;
    }

    unsigned NodeIDAllocator::Clusterer::requiredBits(const PointsTo &pts)
    {
        if (pts.count() == 0) return 0;

        // Ceiling of number of bits amongst each native integer gives needed native ints,
        // so we then multiple again by the number of bits in each native int.
        return ((pts.count() - 1) / NATIVE_INT_SIZE + 1) * NATIVE_INT_SIZE;
    }

    double *NodeIDAllocator::Clusterer::getDistanceMatrix(const Map<PointsTo, unsigned> pointsToSets, const unsigned numObjects)
    {
        size_t condensedSize = (numObjects * (numObjects - 1)) / 2;
        double *distMatrix = new double[condensedSize];
        for (size_t i = 0; i < condensedSize; ++i) distMatrix[i] = numObjects + 2;

        // TODO: maybe use machine epsilon?
        // For reducing distance due to extra occurrences.
        // Can differentiate ~9999 occurrences.
        double occurrenceEpsilon = 0.0001;

        for (const Map<PointsTo, unsigned>::value_type &ptsOcc : pointsToSets)
        {
            const PointsTo &pts = ptsOcc.first;
            const unsigned &occ = ptsOcc.second;

            // Distance between each element of pts.
            unsigned distance = requiredBits(pts) / NATIVE_INT_SIZE;

            // Use a vector so we can index into pts.
            std::vector<NodeID> ptsVec;
            for (const NodeID o : pts) ptsVec.push_back(o);
            for (size_t i = 0; i < ptsVec.size(); ++i)
            {
                const NodeID oi = ptsVec[i];
                for (size_t j = i + 1; j < ptsVec.size(); ++j)
                {
                    const NodeID oj = ptsVec[j];
                    // TODO: handle occurrences.
                    double &existingDistance = distMatrix[condensedIndex(numObjects, oi, oj)];

                    // Subtract extra occurrenceEpsilon to make upcoming logic simpler.
                    // When existingDistance is never whole, it is always between two distances.
                    if (distance < existingDistance) existingDistance = distance - occurrenceEpsilon;

                    if (distance == std::ceil(existingDistance))
                    {
                        // We have something like distance == x, existingDistance == x - e, for some e < 1
                        // (potentially even set during this iteration).
                        // So, the new distance is an occurrence the existingDistance being tracked, it just
                        // had some reductions because of multiple occurences.
                        // If there is not room within this distance to reduce more (increase priority),
                        // just ignore it. TODO: maybe warn?
                        if (existingDistance - occ * occurrenceEpsilon > std::floor(existingDistance))
                        {
                            existingDistance -= occ * occurrenceEpsilon;
                        }
                        else
                        {
                            // Reached minimum.
                            existingDistance = std::floor(existingDistance) + occurrenceEpsilon;
                        }
                    }
                }
            }

        }

        return distMatrix;
    }

    void NodeIDAllocator::Clusterer::traverseDendogram(std::vector<NodeID> &nodeMap, const int *dendogram, const unsigned numObjects, unsigned &allocCounter, Set<int> &visited, const int index)
    {
        if (visited.find(index) != visited.end()) return;
        visited.insert(index);

        int left = dendogram[index - 1];
        if (left < 0)
        {
            // Reached a leaf.
            // -1 because the items start from 1 per fastcluster (TODO).
            nodeMap[std::abs(left) - 1] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, left);
        }

        // Repeat for the right child.
        int right = dendogram[(numObjects - 1) + index - 1];
        if (right < 0)
        {
            nodeMap[std::abs(right) - 1] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, right);
        }
    }

    void NodeIDAllocator::Clusterer::evaluate(const std::vector<NodeID> &nodeMap, const Map<PointsTo, unsigned> pointsToSets, Map<std::string, std::string> &stats)
    {
        unsigned totalTheoretical = 0;
        unsigned totalOriginalSbv = 0;
        unsigned totalOriginalBv = 0;
        unsigned totalNewSbv = 0;
        unsigned totalNewBv = 0;

        for (const Map<PointsTo, unsigned>::value_type &ptsOcc : pointsToSets)
        {
            const PointsTo &pts = ptsOcc.first;
            if (pts.count() == 0) continue;

            unsigned theoretical = requiredBits(pts) / NATIVE_INT_SIZE;

            // Check number of words for original SBV.
            Set<unsigned> words;
            for (const NodeID o : pts) words.insert(o / NATIVE_INT_SIZE);
            unsigned originalSbv = words.size();

            // Check number of words for original BV.
            const std::pair<PointsTo::iterator, PointsTo::iterator> minMax =
                std::minmax_element(pts.begin(), pts.end());
            words.clear();
            for (NodeID b = *minMax.first; b <= *minMax.second; ++b)
            {
                words.insert(b / NATIVE_INT_SIZE);
            }
            unsigned originalBv = words.size();

            // Check number of words for new SBV.
            words.clear();
            for (const NodeID o : pts) words.insert(nodeMap[o] / NATIVE_INT_SIZE);
            unsigned newSbv = words.size();

            // Check number of words for new BV.
            NodeID min = UINT_MAX, max = 0;
            for (const NodeID o : pts)
            {
                const NodeID mappedO = nodeMap[o];
                if (mappedO < min) min = mappedO;
                if (mappedO > max) max = mappedO;
            }

            words.clear();
            // No nodeMap[b] because min and max and from nodeMap.
            for (NodeID b = min; b <= max; ++b) words.insert(b / NATIVE_INT_SIZE);
            unsigned newBv = words.size();

            totalTheoretical += theoretical;
            totalOriginalSbv += originalSbv;
            totalOriginalBv += originalBv;
            totalNewSbv += newSbv;
            totalNewBv += newBv;
        }

        stats[TheoreticalNumWords] = std::to_string(totalTheoretical);
        stats[OriginalSbvNumWords] = std::to_string(totalOriginalSbv);
        stats[OriginalBvNumWords] = std::to_string(totalOriginalBv);
        stats[NewSbvNumWords] = std::to_string(totalNewSbv);
        stats[NewBvNumWords] = std::to_string(totalNewBv);
    }

    void NodeIDAllocator::Clusterer::printStats(Map<std::string, std::string> &stats)
    {
        std::cout.flags(std::ios::left);
        SVFUtil::outs() << "****Clusterer Statistics****\n";
        unsigned fieldWidth = 20;
        // Explicit, rather than loop, so we control the order, for readability.
        std::cout << std::setw(fieldWidth) << NumObjects             << " " << stats[NumObjects]             << "\n";
        std::cout << std::setw(fieldWidth) << TheoreticalNumWords    << " " << stats[TheoreticalNumWords]    << "\n";
        std::cout << std::setw(fieldWidth) << OriginalSbvNumWords    << " " << stats[OriginalSbvNumWords]    << "\n";
        std::cout << std::setw(fieldWidth) << OriginalBvNumWords     << " " << stats[OriginalBvNumWords]     << "\n";
        std::cout << std::setw(fieldWidth) << NewSbvNumWords         << " " << stats[NewSbvNumWords]         << "\n";
        std::cout << std::setw(fieldWidth) << NewBvNumWords          << " " << stats[NewBvNumWords]          << "\n";
        std::cout << std::setw(fieldWidth) << DistanceMatrixTime     << " " << stats[DistanceMatrixTime]     << "\n";
        std::cout << std::setw(fieldWidth) << FastClusterTime        << " " << stats[FastClusterTime]        << "\n";
        std::cout << std::setw(fieldWidth) << DendogramTraversalTime << " " << stats[DendogramTraversalTime] << "\n";
        std::cout << std::setw(fieldWidth) << TotalTime              << " " << stats[TotalTime]              << "\n";
    }

};  // namespace SVF.
