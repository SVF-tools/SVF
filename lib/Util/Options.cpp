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
            clEnumValN(NodeIDAllocator::Strategy::DENSE, "dense", "allocate objects together and values together, separately (default)"),
            clEnumValN(NodeIDAllocator::Strategy::SEQ,     "seq", "allocate values and objects sequentially, intermixed"),
            clEnumValN(NodeIDAllocator::Strategy::DEBUG, "debug", "allocate value and objects sequentially, intermixed, except GEP objects as offsets")
        )
    );


    const llvm::cl::opt<unsigned> Options::MaxFieldLimit(
        "fieldlimit",
        llvm::cl::init(512),
        llvm::cl::desc("Maximum number of fields for field sensitive analysis")
    );

    const llvm::cl::opt<bool> Options::ClusterAnder(
        "cluster-ander",
        llvm::cl::init(false),
        llvm::cl::desc("Stage Andersen's with Steensgard's and cluster based on that")
    );

    const llvm::cl::opt<bool> Options::ClusterFs(
        "cluster-fs",
        llvm::cl::init(false),
        llvm::cl::desc("Cluster for FS/VFS with auxiliary Andersen's")
    );

    const llvm::cl::opt<PointsTo::Type> Options::StagedPtType(
        "staged-pt-type",
        llvm::cl::init(PointsTo::Type::SBV),
        llvm::cl::desc("points-to set data structure to use in the main phase of a staged analysis"),
        llvm::cl::values(
            clEnumValN(PointsTo::Type::SBV, "sbv", "sparse bit vector"),
            clEnumValN(PointsTo::Type::CBV, "cbv", "core bit vector (dynamic bit vector without leading and trailing 0s)")
        )
    );

};  // namespace SVF.
