#include "WPA/LoopInfoConsolidatorPass.h"

using namespace llvm;

bool LoopInfoConsolidatorPass::runOnModule(Module& M) {
	errs() << "I saw a module called " << M.getName() << "!\n";
	
    for (Function& func: M.getFunctionList()) {
        if (func.isDeclaration()) {
            continue;
        }
        LoopInfoWrapperPass& lInfoPass = getAnalysis<LoopInfoWrapperPass>(func);
        LoopInfo& lInfo = lInfoPass.getLoopInfo();
        for (Loop* loop: lInfo) {
            for (BasicBlock* bb: loop->getBlocksVector()) {
                bbInLoops.insert(bb);
            }
        }
    }
    
    /*
    errs() << "loop bbs:\n";
    for (BasicBlock* bb: bbInLoops) {
        bb->dump();
    }
    */
	return false;
}


char LoopInfoConsolidatorPass::ID = 0;
