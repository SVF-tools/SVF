//===- Options.h -- Command line options ------------------------//

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include "Util/NodeIDAllocator.h"

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
};

};  // namespace SVF

#endif  // ifdef OPTIONS_H_
