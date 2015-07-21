/* SVF - Static Value-Flow Analysis Framework
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

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
