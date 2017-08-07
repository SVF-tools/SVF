
#include "SABER/ArgvFlow.h"
#include "Util/AnalysisUtil.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace analysisUtil;

char ArgvFlow::ID = 0;

static RegisterPass<ArgvFlow> ARGVFLOW("argv-flow", "argv to open reach");

void ArgvFlow::reportBug(ProgSlice* slice) {
	if (isAllPathReachable() || isSomePathReachable()) {
		std::cout << "Reachable.\n";
	}
}
