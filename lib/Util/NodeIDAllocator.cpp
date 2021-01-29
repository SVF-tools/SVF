//===- NodeIDAllocator.cpp -- Allocates node IDs on request ------------------------//

#include "FastCluster/fastcluster.h"
#include "Util/NodeIDAllocator.h"
#include "Util/BasicTypes.h"
#include "Util/SVFBasicTypes.h"
#include "Util/SVFUtil.h"

namespace SVF
{
    const std::string NodeIDAllocator::userStrategyDense = "dense";
    const std::string NodeIDAllocator::userStrategyDebug = "debug";

    const NodeID NodeIDAllocator::blackHoleObjectId = 0;
    const NodeID NodeIDAllocator::constantObjectId = 1;
    const NodeID NodeIDAllocator::blackHolePointerId = 2;
    const NodeID NodeIDAllocator::nullPointerId = 3;

    NodeIDAllocator *NodeIDAllocator::allocator = nullptr;

    static llvm::cl::opt<std::string> nodeAllocStrat(
        "node-alloc-strat", llvm::cl::init(SVF::NodeIDAllocator::userStrategyDense),
        llvm::cl::desc("Method of allocating (LLVM) values to node IDs [dense, debug]"));

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
        : numNodes(4), numObjects(4), numValues(4), numSymbols(4)
    {
        if (nodeAllocStrat == userStrategyDebug) strategy = Strategy::DEBUG;
        else if (nodeAllocStrat == userStrategyDense) strategy = Strategy::DENSE;
        else assert(false && "Unknown node allocation strategy specified; expected 'dense' or 'debug'");
    }

    NodeID NodeIDAllocator::allocateObjectId(void)
    {
        NodeID id = 0;
        if (strategy == Strategy::DENSE)
        {
            // We allocate objects from 0(-ish, considering the special nodes) to # of objects.
            id = numObjects;
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

    std::vector<NodeID> NodeIDAllocator::Clusterer::cluster(PTData<NodeID, NodeID, PointsTo> *ptd, const std::vector<NodeID> keys, bool eval)
    {
        assert(ptd != nullptr && "Clusterer::cluster: given null ptd");

        // Pair of nodes to their (minimum) distance and the number of occurrences of that distance.
        Map<std::pair<NodeID, NodeID>, std::pair<unsigned, unsigned>> distances;

        Set<PointsTo> pointsToSets;
        // Number of objects we're reallocating is the largest node (recall, we
        // assume a dense allocation, which goes from 0--modulo specials--to some n).
        // If we "allocate" for specials, it is not a problem except 2 potentially wasted
        // allocations. This is trivial enough to make special handling not worth it.
        unsigned numObjects = 0;
        for (const NodeID key : keys)
        {
            const PointsTo &pts = ptd->getPts(key);
            for (const NodeID o : pts) if (o >= numObjects) numObjects = o + 1;
            pointsToSets.insert(pts);
        }

        // Mapping we'll return.
        std::vector<NodeID> nodeMap(numObjects, UINT_MAX);

        DistOccMap distOcc = getDistancesAndOccurences(pointsToSets);

        double *distMatrix = getDistanceMatrix(distOcc, numObjects);

        int *dendogram = new int[2 * (numObjects - 1)];
        double *height = new double[numObjects - 1];
        // TODO: parameterise method.
        hclust_fast(numObjects, distMatrix, HCLUST_METHOD_SINGLE, dendogram, height);

        unsigned allocCounter = 0;
        Set<int> visited;
        traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, numObjects - 1);

        if (eval) evaluate(nodeMap, pointsToSets);

        return nodeMap;
    }

    unsigned NodeIDAllocator::Clusterer::requiredBits(const PointsTo &pts)
    {
        if (pts.count() == 0) return 0;

        // Ceiling of number of bits amongst each native integer gives needed native ints,
        // so we then multiple again by the number of bits in each native int.
        return ((pts.count() - 1) / NATIVE_INT_SIZE + 1) * NATIVE_INT_SIZE;
    }

    NodeIDAllocator::Clusterer::DistOccMap NodeIDAllocator::Clusterer::getDistancesAndOccurences(const Set<PointsTo> pointsToSets)
    {
        DistOccMap distOcc;
        for (const PointsTo &pts : pointsToSets)
        {
            // Distance between each element of pts.
            unsigned distance = requiredBits(pts);

            // Use a vector so we can index into pts.
            std::vector<NodeID> ptsVec;
            for (const NodeID o : pts) ptsVec.push_back(o);
            for (size_t i = 0; i < ptsVec.size(); ++i)
            {
                const NodeID oi = ptsVec[i];
                for (size_t j = i + 1; j < ptsVec.size(); ++j)
                {
                    const NodeID oj = ptsVec[j];
                    std::pair<unsigned, unsigned> &distOccPair = distOcc[std::make_pair(oi, oj)];
                    // Three cases:
                    //   We have some record of the same distance as the points-to set, in which
                    //     case simply increment how often it occurs for this node pair.
                    //   The distance in this points-to set is less than the recorded distance,
                    //     so we record that distance instead.
                    //   The distance in this points-to set is more, so we ignore it.
                    if (distance == distOccPair.first)
                    {
                        distOccPair.second += 1;
                    }
                    else if (distance < distOccPair.first || distOccPair.first == 0)
                    {
                        distOccPair.first = distance;
                        distOccPair.second = 1;
                    }
                }
            }
        }

        return distOcc;
    }

    double *NodeIDAllocator::Clusterer::getDistanceMatrix(const DistOccMap distOcc, const unsigned numObjects)
    {
        double *distMatrix = new double[(numObjects * (numObjects - 1)) / 2];

        // Index into distMatrix.
        unsigned m = 0;
        for (size_t i = 0; i < numObjects; ++i) {
            for (size_t j = i + 1; j < numObjects; ++j) {
                // numObjects is the maximum distance.
                unsigned distance = numObjects + 1;
                DistOccMap::const_iterator distOccIt = distOcc.find(std::make_pair(i, j));
                // Update from the max, if it is smaller (i.e., it exists).
                if (distOccIt != distOcc.end()) distance = distOccIt->second.first;
                // TODO: account for occ.
                distMatrix[m] = distance;
                ++m;
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

    void NodeIDAllocator::Clusterer::evaluate(const std::vector<NodeID> &nodeMap, const Set<PointsTo> pointsToSets)
    {
        unsigned totalTheoretical = 0;
        unsigned totalOriginalSbv = 0;
        unsigned totalOriginalBv = 0;
        unsigned totalNewSbv = 0;
        unsigned totalNewBv = 0;

        for (const PointsTo &pts : pointsToSets)
        {
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
            for (NodeID b = *minMax.first; b != *minMax.second; ++b)
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

        SVFUtil::outs() << "Clusterer::evaluate:\n"
                        << "  Total theoretical: " << totalTheoretical << "\n"
                        << "  Total original SBV: " << totalOriginalSbv << "\n"
                        << "  Total original BV: " << totalOriginalBv << "\n"
                        << "  Total new SBV: " << totalNewSbv << "\n"
                        << "  Total new BV: " << totalNewBv << "\n";
    }
};  // namespace SVF.
