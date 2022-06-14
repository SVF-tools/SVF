//
// Created by Jiawei Wang on 6/14/22.
//

#ifndef SVF_LLVMLOOPANALYSIS_H
#define SVF_LLVMLOOPANALYSIS_H

#include "MemoryModel/SVFIR.h"
#include "MemoryModel/SVFLoop.h"

namespace SVF {
    class LLVMLoopAnalysis {
    public:
        typedef Map<const ICFGEdge *, const SVFLoop *> ICFGEdgeToSVFLoop;

        /// Constructor
        LLVMLoopAnalysis();

        /// Destructor
        virtual ~LLVMLoopAnalysis() {}

        /// We start the pass here
        virtual bool buildLLVMLoops(SVFModule *mod);

        virtual void build(SVFIR *svfir);

        void buildSVFLoops(ICFG *icfg);

        const ICFGEdgeToSVFLoop &getLoopMap() const {
            return icfgEdgeToSVFLoop;
        }

    protected:
        std::vector<const Loop *> llvmLoops;
        ICFGEdgeToSVFLoop icfgEdgeToSVFLoop;
        Set<std::string> black_lst;
    };
} // end fo SVF

#endif //SVF_LLVMLOOPANALYSIS_H
