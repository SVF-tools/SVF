//===- NodeIDAllocator.cpp -- Allocates node IDs on request ------------------------//

#include "Util/NodeIDAllocator.h"

namespace SVF
{
    NodeID NodeIDAllocator::numObjects = 4;
    NodeID NodeIDAllocator::numValues = 4;
    NodeID NodeIDAllocator::numSymbols = 4;
    NodeID NodeIDAllocator::numNodes = 4;

    enum NodeIDAllocator::Strategy NodeIDAllocator::strategy = NodeIDAllocator::Strategy::NONE;

    const std::string NodeIDAllocator::userStrategyDense = "dense";
    const std::string NodeIDAllocator::userStrategyDebug = "debug";

    const NodeID NodeIDAllocator::blackHoleObjectId = 0;
    const NodeID NodeIDAllocator::constantObjectId = 1;
    const NodeID NodeIDAllocator::blackHolePointerId = 2;
    const NodeID NodeIDAllocator::nullPointerId = 3;

    static llvm::cl::opt<std::string> nodeAllocStrat(
        "node-alloc-strat", llvm::cl::init(SVF::NodeIDAllocator::userStrategyDense),
        llvm::cl::desc("Method of allocating (LLVM) values to node IDs [dense, debug]"));

    NodeID NodeIDAllocator::allocateObjectId(void)
    {
        if (strategy == Strategy::NONE) setStrategy(nodeAllocStrat);

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
        if (strategy == Strategy::NONE) setStrategy(nodeAllocStrat);

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
        if (strategy == Strategy::NONE) setStrategy(nodeAllocStrat);

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

    void NodeIDAllocator::setStrategy(std::string userStrategy)
    {
        if (userStrategy == userStrategyDebug) strategy = Strategy::DEBUG;
        else if (userStrategy == userStrategyDense) strategy = Strategy::DENSE;
        else assert(false && "Unknown node allocation strategy specified; expected 'dense' or 'debug'");
    }

    void NodeIDAllocator::endSymbolAllocation(void)
    {
        numSymbols = numNodes;
    }

};  // namespace SVF.
