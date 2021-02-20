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


};  // namespace SVF.
