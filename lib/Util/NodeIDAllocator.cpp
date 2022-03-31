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
            allocator = nullptr;
        }
    }

    // Initialise counts to 4 because that's how many special nodes we have.
    NodeIDAllocator::NodeIDAllocator(void)
        : numObjects(4), numValues(4), numSymbols(4), numNodes(4), strategy(Options::NodeAllocStrat)
    { }

    NodeID NodeIDAllocator::allocateObjectId(void)
    {
        NodeID id = 0;
        if (strategy == Strategy::DENSE)
        {
            // We allocate objects from 0(-ish, considering the special nodes) to # of objects.
            id = numObjects;
        }
        else if (strategy == Strategy::REVERSE_DENSE)
        {
            id = UINT_MAX - numObjects;
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
        else if (strategy == Strategy::REVERSE_DENSE)
        {
            id = UINT_MAX - numObjects;
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
            assert(id > numSymbols && "NodeIDAllocator::allocateGepObjectId: GEP allocation clashing with other nodes");
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
        else if (strategy == Strategy::REVERSE_DENSE)
        {
            id = numValues;
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

    NodeID NodeIDAllocator::endSymbolAllocation(void)
    {
        numSymbols = numNodes;
        return numSymbols;
    }

    const std::string NodeIDAllocator::Clusterer::NumObjects = "NumObjects";
    const std::string NodeIDAllocator::Clusterer::RegioningTime = "RegioningTime";
    const std::string NodeIDAllocator::Clusterer::DistanceMatrixTime = "DistanceMatrixTime";
    const std::string NodeIDAllocator::Clusterer::FastClusterTime = "FastClusterTime";
    const std::string NodeIDAllocator::Clusterer::DendrogramTraversalTime = "DendrogramTravTime";
    const std::string NodeIDAllocator::Clusterer::EvalTime = "EvalTime";
    const std::string NodeIDAllocator::Clusterer::TotalTime = "TotalTime";
    const std::string NodeIDAllocator::Clusterer::TheoreticalNumWords = "TheoreticalWords";
    const std::string NodeIDAllocator::Clusterer::OriginalBvNumWords = "OriginalBvWords";
    const std::string NodeIDAllocator::Clusterer::OriginalSbvNumWords = "OriginalSbvWords";
    const std::string NodeIDAllocator::Clusterer::NewBvNumWords = "NewBvWords";
    const std::string NodeIDAllocator::Clusterer::NewSbvNumWords = "NewSbvWords";
    const std::string NodeIDAllocator::Clusterer::NumRegions = "NumRegions";
    const std::string NodeIDAllocator::Clusterer::NumGtIntRegions = "NumGtIntRegions";
    const std::string NodeIDAllocator::Clusterer::LargestRegion = "LargestRegion";
    const std::string NodeIDAllocator::Clusterer::BestCandidate = "BestCandidate";
    const std::string NodeIDAllocator::Clusterer::NumNonTrivialRegionObjects = "NumNonTrivObj";

    std::vector<NodeID> NodeIDAllocator::Clusterer::cluster(BVDataPTAImpl *pta, const std::vector<std::pair<NodeID, unsigned>> keys, std::vector<std::pair<hclust_fast_methods, std::vector<NodeID>>> &candidates, std::string evalSubtitle)
    {
        assert(pta != nullptr && "Clusterer::cluster: given null BVDataPTAImpl");
        assert(Options::NodeAllocStrat == Strategy::DENSE && "Clusterer::cluster: only dense allocation clustering currently supported");

        Map<std::string, std::string> overallStats;
        double fastClusterTime = 0.0;
        double distanceMatrixTime = 0.0;
        double dendrogramTraversalTime = 0.0;
        double regioningTime = 0.0;
        double evalTime = 0.0;

        // Pair of nodes to their (minimum) distance and the number of occurrences of that distance.
        Map<std::pair<NodeID, NodeID>, std::pair<unsigned, unsigned>> distances;

        double clkStart = PTAStat::getClk(true);

        // Map points-to sets to occurrences.
        Map<PointsTo, unsigned> pointsToSets;

        // Objects each object shares at least a points-to set with.
        Map<NodeID, Set<NodeID>> coPointeeGraph;
        for (const std::pair<NodeID, unsigned> &keyOcc : keys)
        {
            const PointsTo &pts = pta->getPts(keyOcc.first);
            const size_t oldSize = pointsToSets.size();
            pointsToSets[pts] += keyOcc.second;;

            // Edges in this graph have no weight or uniqueness, so we only need to
            // do this for each points-to set once.
            if (oldSize != pointsToSets.size())
            {
                NodeID firstO = !pts.empty() ? *(pts.begin()) : 0;
                Set<NodeID> &firstOsNeighbours = coPointeeGraph[firstO];
                for (const NodeID o : pts)
                {
                    if (o != firstO)
                    {
                        firstOsNeighbours.insert(o);
                        coPointeeGraph[o].insert(firstO);
                    }
                }
            }
        }

        size_t numObjects = NodeIDAllocator::get()->numObjects;
        overallStats[NumObjects] = std::to_string(numObjects);

        size_t numRegions = 0;
        std::vector<unsigned> objectsRegion;
        if (Options::RegionedClustering)
        {
            objectsRegion = regionObjects(coPointeeGraph, numObjects, numRegions);
        }
        else
        {
            // Just a single big region (0).
            objectsRegion.insert(objectsRegion.end(), numObjects, 0);
            numRegions = 1;
        }

        // Set needs to be ordered because getDistanceMatrix, in its n^2 iteration, expects
        // sets to be ordered (we are building a condensed matrix, not a full matrix, so it
        // matters). In getDistanceMatrix, doing regionReverseMapping for oi and oj, where
        // oi < oj, and getting a result moi > moj gives incorrect results.
        // In the condensed matrix, [b][a] where b >= a, is incorrect.
        std::vector<OrderedSet<NodeID>> regionsObjects(numRegions);
        for (NodeID o = 0; o < numObjects; ++o) regionsObjects[objectsRegion[o]].insert(o);

        // Size of the return node mapping. It is potentially larger than the number of
        // objects because we align each region to NATIVE_INT_SIZE.
        // size_t numMappings = 0;

        // Maps a region to a mapping which maps 0 to n to all objects
        // in that region.
        std::vector<std::vector<NodeID>> regionMappings(numRegions);
        // The reverse: region to mapping of objects to a 0 to n from above.
        std::vector<Map<NodeID, unsigned>> regionReverseMappings(numRegions);
        // We can thus use 0 to n for each region to create smaller distance matrices.
        for (unsigned region = 0; region < numRegions; ++region)
        {
            size_t curr = 0;
            // With the OrderedSet above, o1 < o2 => map[o1] < map[o2].
            for (NodeID o : regionsObjects[region])
            {
                // push_back here is just like p...[region][curr] = o.
                regionMappings[region].push_back(o);
                regionReverseMappings[region][o] = curr++;
            }

            // curr is the number of objects. A region with no objects makes no sense.
            assert(curr != 0);

            // Number of bits needed for this region if we were
            // to start assigning from 0 rounded up to the fewest needed
            // native ints. This is added to the number of mappings since
            // we align each region to a native int.
            // numMappings += requiredBits(regionsObjects[region].size());
        }

        // Points-to sets which are relevant to a region, i.e., those whose elements
        // belong to that region. Pair is for occurences.
        std::vector<std::vector<std::pair<const PointsTo *, unsigned>>> regionsPointsTos(numRegions);
        for (const Map<PointsTo, unsigned>::value_type &ptocc : pointsToSets)
        {
            const PointsTo &pt = ptocc.first;
            const unsigned occ = ptocc.second;
            if (pt.empty()) continue;
            // Guaranteed that begin() != end() because of the continue above. All objects in pt
            // will be relevant to the same region.
            unsigned region = objectsRegion[*(pt.begin())];
            // In our "graph", objects in the same points-to set have an edge between them,
            // so they are all in the same connected component/region.
            regionsPointsTos[region].push_back(std::make_pair(&pt, occ));
        }

        double clkEnd = PTAStat::getClk(true);
        regioningTime = (clkEnd - clkStart) / TIMEINTERVAL;
        overallStats[RegioningTime] = std::to_string(regioningTime);
        overallStats[NumRegions] = std::to_string(numRegions);

        std::vector<hclust_fast_methods> methods;
        if (Options::ClusterMethod == HCLUST_METHOD_SVF_BEST)
        {
            methods.push_back(HCLUST_METHOD_SINGLE);
            methods.push_back(HCLUST_METHOD_COMPLETE);
            methods.push_back(HCLUST_METHOD_AVERAGE);
        }
        else
        {
            methods.push_back(Options::ClusterMethod);
        }

        for (const hclust_fast_methods method : methods)
        {
            std::vector<NodeID> nodeMap(numObjects, UINT_MAX);

            unsigned numGtIntRegions = 0;
            unsigned largestRegion = 0;
            unsigned nonTrivialRegionObjects = 0;
            unsigned allocCounter = 0;
            for (unsigned region = 0; region < numRegions; ++region)
            {
                const size_t regionNumObjects = regionsObjects[region].size();
                // Round up to next Word: ceiling of current allocation to get how
                // many words and multiply to get the number of bits; if we're aligning.
                if (Options::RegionAlign)
                {
                    allocCounter =
                        ((allocCounter + NATIVE_INT_SIZE - 1) / NATIVE_INT_SIZE) * NATIVE_INT_SIZE;
                }

                if (regionNumObjects > largestRegion) largestRegion = regionNumObjects;

                // For regions with fewer than 64 objects, we can just allocate them
                // however as they will be in the one int regardless..
                if (regionNumObjects < NATIVE_INT_SIZE)
                {
                    for (NodeID o : regionsObjects[region]) nodeMap[o] = allocCounter++;
                    continue;
                }

                ++numGtIntRegions;
                nonTrivialRegionObjects += regionNumObjects;

                double *distMatrix = getDistanceMatrix(regionsPointsTos[region], regionNumObjects,
                                                       regionReverseMappings[region], distanceMatrixTime);

                clkStart = PTAStat::getClk(true);
                int *dendrogram = new int[2 * (regionNumObjects - 1)];
                double *height = new double[regionNumObjects - 1];
                hclust_fast(regionNumObjects, distMatrix, method, dendrogram, height);
                delete[] distMatrix;
                delete[] height;
                clkEnd = PTAStat::getClk(true);
                fastClusterTime += (clkEnd - clkStart) / TIMEINTERVAL;

                clkStart = PTAStat::getClk(true);
                Set<int> visited;
                traverseDendrogram(nodeMap, dendrogram, regionNumObjects, allocCounter,
                                   visited, regionNumObjects - 1, regionMappings[region]);
                delete[] dendrogram;
                clkEnd = PTAStat::getClk(true);
                dendrogramTraversalTime += (clkEnd - clkStart) / TIMEINTERVAL;
            }

            candidates.push_back(std::make_pair(method, nodeMap));

            // Though we "update" these in the loop, they will be the same every iteration.
            overallStats[NumGtIntRegions] = std::to_string(numGtIntRegions);
            overallStats[LargestRegion] = std::to_string(largestRegion);
            overallStats[NumNonTrivialRegionObjects] = std::to_string(nonTrivialRegionObjects);
        }

        // Work out which of the mappings we generated looks best.
        std::pair<hclust_fast_methods, std::vector<NodeID>> bestMapping = determineBestMapping(candidates, pointsToSets,
                                                                                               evalSubtitle, evalTime);

        overallStats[DistanceMatrixTime] = std::to_string(distanceMatrixTime);
        overallStats[DendrogramTraversalTime] = std::to_string(dendrogramTraversalTime);
        overallStats[FastClusterTime] = std::to_string(fastClusterTime);
        overallStats[EvalTime] = std::to_string(evalTime);
        overallStats[TotalTime] = std::to_string(distanceMatrixTime + dendrogramTraversalTime + fastClusterTime + regioningTime + evalTime);

        overallStats[BestCandidate] = SVFUtil::hclustMethodToString(bestMapping.first);
        printStats(evalSubtitle + ": overall", overallStats);

        return bestMapping.second;
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
        return requiredBits(pts.count());
    }

    unsigned NodeIDAllocator::Clusterer::requiredBits(const size_t n)
    {
        if (n == 0) return 0;
        // Ceiling of number of bits amongst each native integer gives needed native ints,
        // so we then multiply again by the number of bits in each native int.
        return ((n - 1) / NATIVE_INT_SIZE + 1) * NATIVE_INT_SIZE;
    }

    double *NodeIDAllocator::Clusterer::getDistanceMatrix(const std::vector<std::pair<const PointsTo *, unsigned>> pointsToSets,
                                                          const size_t numObjects, const Map<NodeID, unsigned> &nodeMap,
                                                          double &distanceMatrixTime)
    {
        const double clkStart = PTAStat::getClk(true);
        size_t condensedSize = (numObjects * (numObjects - 1)) / 2;
        double *distMatrix = new double[condensedSize];
        for (size_t i = 0; i < condensedSize; ++i) distMatrix[i] = numObjects * numObjects;

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

        const double clkEnd = PTAStat::getClk(true);
        distanceMatrixTime += (clkEnd - clkStart) / TIMEINTERVAL;

        return distMatrix;
    }

    void NodeIDAllocator::Clusterer::traverseDendrogram(std::vector<NodeID> &nodeMap, const int *dendrogram, const size_t numObjects, unsigned &allocCounter, Set<int> &visited, const int index, const std::vector<NodeID> &regionNodeMap)
    {
        if (visited.find(index) != visited.end()) return;
        visited.insert(index);

        int left = dendrogram[index - 1];
        if (left < 0)
        {
            // Reached a leaf.
            // -1 because the items start from 1 per fastcluster (TODO).
            nodeMap[regionNodeMap[std::abs(left) - 1]] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendrogram(nodeMap, dendrogram, numObjects, allocCounter, visited, left, regionNodeMap);
        }

        // Repeat for the right child.
        int right = dendrogram[(numObjects - 1) + index - 1];
        if (right < 0)
        {
            nodeMap[regionNodeMap[std::abs(right) - 1]] = allocCounter;
            ++allocCounter;
        }
        else
        {
            traverseDendrogram(nodeMap, dendrogram, numObjects, allocCounter, visited, right, regionNodeMap);
        }
    }

    std::vector<NodeID> NodeIDAllocator::Clusterer::regionObjects(const Map<NodeID, Set<NodeID>> &graph, size_t numObjects, size_t &numLabels)
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

    void NodeIDAllocator::Clusterer::evaluate(const std::vector<NodeID> &nodeMap, const Map<PointsTo, unsigned> pointsToSets, Map<std::string, std::string> &stats, bool accountForOcc)
    {
        u64_t totalTheoretical = 0;
        u64_t totalOriginalSbv = 0;
        u64_t totalOriginalBv = 0;
        u64_t totalNewSbv = 0;
        u64_t totalNewBv = 0;

        for (const Map<PointsTo, unsigned>::value_type &ptsOcc : pointsToSets)
        {
            const PointsTo &pts = ptsOcc.first;
            const unsigned occ = ptsOcc.second;
            if (pts.count() == 0) continue;

            u64_t theoretical = requiredBits(pts) / NATIVE_INT_SIZE;
            if (accountForOcc) theoretical *= occ;

            // Check number of words for original SBV.
            Set<unsigned> words;
            // TODO: nasty hardcoding.
            for (const NodeID o : pts) words.insert(o / 128);
            u64_t originalSbv = words.size() * 2;
            if (accountForOcc) originalSbv *= occ;

            // Check number of words for original BV.
            NodeID min = UINT_MAX;
            NodeID max = 0;
            for (NodeID o : pts)
            {
                if (o < min) min = o;
                if (o > max) max = o;
            }
            words.clear();
            for (NodeID b = min; b <= max; ++b)
            {
                words.insert(b / NATIVE_INT_SIZE);
            }
            u64_t originalBv = words.size();
            if (accountForOcc) originalBv *= occ;

            // Check number of words for new SBV.
            words.clear();
            // TODO: nasty hardcoding.
            for (const NodeID o : pts) words.insert(nodeMap[o] / 128);
            u64_t newSbv = words.size() * 2;
            if (accountForOcc) newSbv *= occ;

            // Check number of words for new BV.
            min = UINT_MAX;
            max = 0;
            for (const NodeID o : pts)
            {
                const NodeID mappedO = nodeMap[o];
                if (mappedO < min) min = mappedO;
                if (mappedO > max) max = mappedO;
            }

            words.clear();
            // No nodeMap[b] because min and max and from nodeMap.
            for (NodeID b = min; b <= max; ++b) words.insert(b / NATIVE_INT_SIZE);
            u64_t newBv = words.size();
            if (accountForOcc) newBv *= occ;

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

        // Work out which of the mappings we generated looks best.
    std::pair<hclust_fast_methods, std::vector<NodeID>> NodeIDAllocator::Clusterer::determineBestMapping(
            const std::vector<std::pair<hclust_fast_methods, std::vector<NodeID>>> &candidates,
            Map<PointsTo, unsigned> pointsToSets, const std::string &evalSubtitle, double &evalTime)
    {
        // In case we're not comparing anything, set to first "candidate".
        std::pair<hclust_fast_methods, std::vector<NodeID>> bestMapping = candidates[0];
        // Number of bits required for the best candidate.
        size_t bestWords = std::numeric_limits<size_t>::max();
        if (evalSubtitle != "" || Options::ClusterMethod == HCLUST_METHOD_SVF_BEST)
        {
            for (const std::pair<hclust_fast_methods, std::vector<NodeID>> &candidate : candidates)
            {
                Map<std::string, std::string> candidateStats;
                hclust_fast_methods candidateMethod = candidate.first;
                std::string candidateMethodName = SVFUtil::hclustMethodToString(candidateMethod);
                std::vector<NodeID> candidateMapping = candidate.second;

                // TODO: parameterise final arg.
                const double clkStart = PTAStat::getClk(true);
                evaluate(candidateMapping, pointsToSets, candidateStats, true);
                const double clkEnd = PTAStat::getClk(true);
                evalTime += (clkEnd - clkStart) / TIMEINTERVAL;
                printStats(evalSubtitle + ": candidate " + candidateMethodName, candidateStats);

                size_t candidateWords = 0;
                if (Options::PtType == PointsTo::SBV) candidateWords = std::stoull(candidateStats[NewSbvNumWords]);
                else if (Options::PtType == PointsTo::CBV) candidateWords = std::stoull(candidateStats[NewBvNumWords]);
                else assert(false && "Clusterer::cluster: unsupported BV type for clustering.");

                if (candidateWords < bestWords)
                {
                    bestWords = candidateWords;
                    bestMapping = candidate;
                }
            }
        }

        return bestMapping;
    }

    void NodeIDAllocator::Clusterer::printStats(std::string subtitle, Map<std::string, std::string> &stats)
    {
        // When not in order, it is too hard to compare original/new SBV/BV words, so this array forces an order.
        const static std::array<std::string, 17> statKeys =
            { NumObjects, TheoreticalNumWords, OriginalSbvNumWords, OriginalBvNumWords,
              NewSbvNumWords, NewBvNumWords, NumRegions, NumGtIntRegions,
              NumNonTrivialRegionObjects, LargestRegion, RegioningTime,
              DistanceMatrixTime, FastClusterTime, DendrogramTraversalTime,
              EvalTime, TotalTime, BestCandidate };

        const unsigned fieldWidth = 20;
        SVFUtil::outs().flags(std::ios::left);
        SVFUtil::outs() << "****Clusterer Statistics: " << subtitle << "****\n";
        for (const std::string &statKey : statKeys)
        {
            Map<std::string, std::string>::const_iterator stat = stats.find(statKey);
            if (stat != stats.end())
            {
                SVFUtil::outs() << std::setw(fieldWidth) << statKey << " " << stat->second << "\n";
            }
        }

        SVFUtil::outs().flush();
    }

};  // namespace SVF.
