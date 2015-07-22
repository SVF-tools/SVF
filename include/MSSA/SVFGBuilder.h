//===- SVFGBuilder.h -- Building SVFG-----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * AndersenMemSSA.h
 *
 *  Created on: Oct 27, 2013
 *      Author: Yulei Sui
 */

#ifndef ANDERSENMEMSSA_H_
#define ANDERSENMEMSSA_H_

#include <llvm/Analysis/DominanceFrontier.h>

class SVFG;
class MemSSA;
class BVDataPTAImpl;

/*!
 * Dominator frontier used in MSSA
 */
class MemSSADF : public llvm::DominanceFrontier {
public:
    MemSSADF() : llvm::DominanceFrontier()
    {}

    bool runOnDT(llvm::DominatorTree& dt) {
        releaseMemory();
        getBase().analyze(dt);
        return false;
    }
};

/*!
 * SVFG Builder
 */
class SVFGBuilder {

public:
    /// Constructor
    SVFGBuilder(): svfg(NULL) {}

    /// Destructor
    virtual ~SVFGBuilder() {}

    /// We start from here
    virtual bool build(SVFG* graph,BVDataPTAImpl* pta);

    inline SVFG* getSVFG() const {
        return svfg;
    }

protected:
    virtual void createSVFG(MemSSA* mssa, SVFG* graph);
    virtual void releaseMemory(SVFG* graph);

    SVFG* svfg;
};

#endif /* ANDERSENMEMSSA_H_ */
