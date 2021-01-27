//===- NodeIDAllocator.h -- Allocates node IDs on request ------------------------//

#ifndef NODEIDALLOCATOR_H_
#define NODEIDALLOCATOR_H_

#include "Util/SVFBasicTypes.h"

namespace SVF
{

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

    /// Allocate an object ID as determined by the strategy.
    static NodeID allocateObjectId(void);

    /// Allocate a GEP object ID as determined by the strategy.
    /// allocateObjectId is still fine for GEP objects, but
    /// for some strategies (DEBUG, namely), GEP objects can
    /// be allocated differently (more readable, for DEBUG).
    /// Regardless, numObjects is shared; there is no special
    /// numGepObjects.
    static NodeID allocateGepObjectId(NodeID base, u32_t offset, u32_t maxFieldLimit);

    /// Allocate a value ID as determined by the strategy.
    static NodeID allocateValueId(void);

    /// Notify the allocator that all symbols have had IDs allocated.
    static void endSymbolAllocation(void);

    /// Set the strategy from the user-facing strategy names.
    static void setStrategy(std::string userStrategy);

private:
    /// These are moreso counters than amounts.
    ///@{
    /// Number of memory objects allocated, including specials.
    static NodeID numObjects;
    /// Number of values allocated, including specials.
    static NodeID numValues;
    /// Number of explicit symbols allocated (e.g., llvm::Values), including specials.
    static NodeID numSymbols;
    /// Total number of objects and values allocated.
    static NodeID numNodes;
    ///@}

    /// Strategy to allocate with. Initially NONE.
    static enum Strategy strategy;
};

};  // namespace SVF

#endif  // ifdef NODEIDALLOCATOR_H_
