//===- Options.cpp -- Command line options ------------------------//

#include <llvm/Support/CommandLine.h>

#include "Util/Options.h"

namespace SVF
{
    const llvm::cl::opt<bool> Options::MarkedClocksOnly(
        "marked-clocks-only",
        llvm::cl::init(false),
        llvm::cl::desc("Only measure times where explicitly marked")
    );

    const llvm::cl::opt<NodeIDAllocator::Strategy> Options::NodeAllocStrat(
        "node-alloc-strat",
        llvm::cl::init(NodeIDAllocator::Strategy::DENSE),
        llvm::cl::desc("Method of allocating (LLVM) values and memory objects as node IDs"),
        llvm::cl::values(
            clEnumValN(NodeIDAllocator::Strategy::DENSE, "dense", "allocate objects together and values together, separately"),
            clEnumValN(NodeIDAllocator::Strategy::SEQ,     "seq", "allocate values and objects sequentially, intermixed"),
            clEnumValN(NodeIDAllocator::Strategy::DEBUG, "debug", "allocate value and objects sequentially, intermixed, except GEP objects as offsets")
        )
    );

};  // namespace SVF.
