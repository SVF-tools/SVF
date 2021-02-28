//===- NodeIDAllocator.cpp -- Allocates node IDs on request ------------------------//

#include <iomanip>
#include <iostream>
#include <queue>

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

    std::vector<NodeID> NodeIDAllocator::Clusterer::cluster(BVDataPTAImpl *pta, const std::vector<std::pair<NodeID, unsigned>> keys, std::string evalSubtitle)
    {
        assert(pta != nullptr && "Clusterer::cluster: given null BVDataPTAImpl");

        Map<std::string, std::string> stats;
        double totalTime = 0.0;
        double fastClusterTime = 0.0;
        double distanceMatrixTime = 0.0;
        double dendogramTraversalTime = 0.0;

        // Pair of nodes to their (minimum) distance and the number of occurrences of that distance.
        Map<std::pair<NodeID, NodeID>, std::pair<unsigned, unsigned>> distances;

        double clkStart = PTAStat::getClk(true);
        // Map points-to sets to occurrences.
        Map<PointsTo, unsigned> pointsToSets;
        // Nodes to some of the objects they share a set with.
        // TODO: describe why "some", use vector not Map.
        Map<NodeID, Set<NodeID>> graph;
        // Number of objects we're reallocating is the largest node (recall, we
        // assume a dense allocation, which goes from 0--modulo specials--to some n).
        // If we "allocate" for specials, it is not a problem except 2 potentially wasted
        // allocations. This is trivial enough to make special handling not worth it.
        size_t numObjects = 0;
        for (const std::pair<NodeID, unsigned> &keyOcc : keys)
        {
            const PointsTo &pts = pta->getPts(keyOcc.first);
            NodeID firstO = !pts.empty() ? *(pts.begin()) : 0;
            for (const NodeID o : pts)
            {
                if (o >= numObjects) numObjects = o + 1;
                if (o != firstO)
                {
                    graph[firstO].insert(o);
                    graph[o].insert(firstO);
                }
            }

            pointsToSets[pts] += keyOcc.second;;
        }

        stats[NumObjects] = std::to_string(numObjects);

        size_t numLabels = 0;
        const std::vector<NodeID> objectLabels = labelObjects(graph, numObjects, numLabels);
        std::vector<Set<NodeID>> labelObjects(numLabels);
        for (NodeID o = 0; o < numObjects; ++o) labelObjects[objectLabels[o]].insert(o);

        size_t numMappings = 0;

        // Label to object mappings which map 0 to n to all the objects part of that
        // label, thus requiring a smaller distance matrix.
        std::vector<std::vector<NodeID>> partitionMappings(numLabels);
        std::vector<Map<NodeID, unsigned>> partitionReverseMappings(numLabels);
        for (unsigned l = 0; l < numLabels; ++l)
        {
            size_t curr = 0;
            for (NodeID o : labelObjects[l])
            {
                partitionMappings[l].push_back(o);
                partitionReverseMappings[l][o] = curr++;
            }

            // Number of native words needed for this partition if we were
            // to start assigning from 0. curr is the number of objects.
            assert(curr != 0);
            numMappings += ((curr - 1) / NATIVE_INT_SIZE + 1) * NATIVE_INT_SIZE;
        }

        // Points-to sets which are relevant to a label.
        // Pair is for occurences.
        std::vector<std::vector<std::pair<const PointsTo *, unsigned>>> labelPointsTos(numLabels);
        for (const Map<PointsTo, unsigned>::value_type &pto : pointsToSets)
        {
            const PointsTo &pt = pto.first;
            const unsigned occ = pto.second;
            if (pt.empty()) continue;
            // Guaranteed that begin() != end() because of the continue above.
            const NodeID o = *(pt.begin());
            unsigned label = objectLabels[o];
            // By definition, all other objects in the points-to set will have the same label.
            labelPointsTos[label].push_back(std::make_pair(&pt, occ));
        }

        // Mapping we'll return.
        std::vector<NodeID> nodeMap(numObjects, UINT_MAX);

        unsigned allocCounter = 0;
        for (unsigned l = 0; l < numLabels; ++l)
        {
            const size_t lNumObjects = labelObjects[l].size();
            // Round up to next Word: ceiling of div. and multiply.
            allocCounter = ((allocCounter + NATIVE_INT_SIZE - 1) / NATIVE_INT_SIZE) * NATIVE_INT_SIZE;

            // If in a group we have less than 64 items, we can just allocate them
            // however.
            if (lNumObjects < NATIVE_INT_SIZE)
            {
                for (NodeID o : labelObjects[l])
                {
                    nodeMap[o] = allocCounter++;
                }

                continue;
            }

            double *distMatrix = getDistanceMatrix(labelPointsTos[l], lNumObjects, partitionReverseMappings[l]);
            double clkEnd = PTAStat::getClk(true);
            double time = (clkEnd - clkStart) / TIMEINTERVAL;
            distanceMatrixTime += time;

            clkStart = PTAStat::getClk(true);
            int *dendogram = new int[2 * (lNumObjects - 1)];
            double *height = new double[lNumObjects - 1];
            // TODO: parameterise method.
            hclust_fast(lNumObjects, distMatrix, Options::ClusterMethod, dendogram, height);
            delete[] distMatrix;
            // We never use the height.
            delete[] height;
            clkEnd = PTAStat::getClk(true);
            time = (clkEnd - clkStart) / TIMEINTERVAL;
            fastClusterTime += time;

            clkStart = PTAStat::getClk(true);
            Set<int> visited;
            traverseDendogram(nodeMap, dendogram, lNumObjects, allocCounter, visited, lNumObjects - 1, partitionMappings[l]);
            delete[] dendogram;
            clkEnd = PTAStat::getClk(true);
            time = (clkEnd - clkStart) / TIMEINTERVAL;
            dendogramTraversalTime += time;

        }

        stats[DistanceMatrixTime] = std::to_string(distanceMatrixTime);
        stats[DendogramTraversalTime] = std::to_string(dendogramTraversalTime);
        stats[FastClusterTime] = std::to_string(fastClusterTime);
        stats[TotalTime] = std::to_string(distanceMatrixTime + dendogramTraversalTime + fastClusterTime);

        if (evalSubtitle != "")
        {
            evaluate(nodeMap, pointsToSets, stats);
            printStats(evalSubtitle, stats);
        }

        return nodeMap;
    }

    std::vector<NodeID> NodeIDAllocator::Clusterer::getReverseNodeMapping(const std::vector<NodeID> &nodeMapping)
    {
        // nodeMapping.size() may not be big enough because we leave some gaps, but it's a start.
        std::vector<NodeID> reverseNodeMapping(nodeMapping.size(), UINT_MAX);
        for (size_t i = 0; i < nodeMapping.size(); ++i)
        {
            const NodeID mapsTo = nodeMapping.at(i);
            if (mapsTo >= reverseNodeMapping.size()) reverseNodeMapping.resize(mapsTo + 1, UINT_MAX);
            reverseNodeMapping.at(mapsTo) = i;
        }

        return reverseNodeMapping;
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

    double *NodeIDAllocator::Clusterer::getDistanceMatrix(const std::vector<std::pair<const PointsTo *, unsigned>> pointsToSets, const size_t numObjects, const Map<NodeID, unsigned> &nodeMap)
    {
        size_t condensedSize = (numObjects * (numObjects - 1)) / 2;
        double *distMatrix = new double[condensedSize];
        for (size_t i = 0; i < condensedSize; ++i) distMatrix[i] = numObjects + 2;

        // TODO: maybe use machine epsilon?
        // For reducing distance due to extra occurrences.
        // Can differentiate ~9999 occurrences.
        double occurrenceEpsilon = 0.0001;

        for (const std::pair<const PointsTo *, unsigned> &ptsOcc : pointsToSets)
        {
            const PointsTo *pts = ptsOcc.first;
            assert(pts != nullptr);
            const unsigned occ = ptsOcc.second;

            // Distance between each element of pts.
            unsigned distance = requiredBits(*pts) / NATIVE_INT_SIZE;

            // Use a vector so we can index into pts.
            std::vector<NodeID> ptsVec;
            for (const NodeID o : *pts) ptsVec.push_back(o);
            for (size_t i = 0; i < ptsVec.size(); ++i)
            {
                const NodeID oi = ptsVec[i];
                const Map<NodeID, unsigned>::const_iterator moi = nodeMap.find(oi);
                assert(moi != nodeMap.end());
                for (size_t j = i + 1; j < ptsVec.size(); ++j)
                {
                    const NodeID oj = ptsVec[j];
                    const Map<NodeID, unsigned>::const_iterator moj = nodeMap.find(oj);
                    assert(moj != nodeMap.end());
                    double &existingDistance = distMatrix[condensedIndex(numObjects, moi->second, moj->second)];

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

    void NodeIDAllocator::Clusterer::traverseDendogram(std::vector<NodeID> &nodeMap, const int *dendogram, const size_t numObjects, unsigned &allocCounter, Set<int> &visited, const int index, const std::vector<NodeID> &partitionNodeMap)
    {
        if (visited.find(index) != visited.end()) return;
        visited.insert(index);

        int left = dendogram[index - 1];
        if (left < 0)
        {
            // Reached a leaf.
            // -1 because the items start from 1 per fastcluster (TODO).
            nodeMap[partitionNodeMap[std::abs(left) - 1]] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, left, partitionNodeMap);
        }

        // Repeat for the right child.
        int right = dendogram[(numObjects - 1) + index - 1];
        if (right < 0)
        {
            nodeMap[partitionNodeMap[std::abs(right) - 1]] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendogram(nodeMap, dendogram, numObjects, allocCounter, visited, right, partitionNodeMap);
        }
    }

    std::vector<NodeID> NodeIDAllocator::Clusterer::labelObjects(const Map<NodeID, Set<NodeID>> &graph, size_t numObjects, size_t &numLabels)
    {
        unsigned label = UINT_MAX;
        std::vector<NodeID> labels(numObjects, UINT_MAX);
        Set<NodeID> labelled;
        for (const Map<NodeID, Set<NodeID>>::value_type &oos : graph)
        {
            const NodeID o = oos.first;
            if (labels[o] != UINT_MAX) continue;
            std::queue<NodeID> bfsQueue;
            bfsQueue.push(o);
            ++label;
            while (!bfsQueue.empty())
            {
                const NodeID o = bfsQueue.front();
                bfsQueue.pop();
                if (labels[o] != UINT_MAX)
                {
                    assert(labels[o] == label);
                    continue;
                }

                labels[o] = label;
                Map<NodeID, Set<NodeID>>::const_iterator neighboursIt = graph.find(o);
                assert(neighboursIt != graph.end());
                for (const NodeID neighbour : neighboursIt->second) bfsQueue.push(neighbour);
            }
        }

        // The remaining objects have no relation with others: they get their own label.
        for (size_t o = 0; o < numObjects; ++o)
        {
            if (labels[o] == UINT_MAX) labels[o] = ++label;
        }

        numLabels = label + 1;

        return labels;
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

    void NodeIDAllocator::Clusterer::printStats(std::string subtitle, Map<std::string, std::string> &stats)
    {
        // When not in order, it is too hard to compare original/new SBV/BV words, so this array forces an order.
        const static std::array<std::string, 10> statKeys =
            { NumObjects, TheoreticalNumWords, OriginalSbvNumWords, OriginalBvNumWords,
              NewSbvNumWords, NewBvNumWords, DistanceMatrixTime, FastClusterTime,
              DendogramTraversalTime, TotalTime };

        const unsigned fieldWidth = 20;
        std::cout.flags(std::ios::left);
        std::cout << "****Clusterer Statistics: " << subtitle << "****\n";
        for (const std::string &statKey : statKeys)
        {
            Map<std::string, std::string>::const_iterator stat = stats.find(statKey);
            if (stat != stats.end())
            {
                std::cout << std::setw(fieldWidth) << statKey << " " << stat->second << "\n";
            }
        }

        std::cout.flush();
    }

};  // namespace SVF.
