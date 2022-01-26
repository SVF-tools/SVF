//===- SVFGBuilder.h -- Building SVFG-----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "MemoryModel/PointerAnalysis.h"
#include "Graphs/SVFGOPT.h"

namespace SVF
{

/*!
 * Dominator frontier used in MSSA
 */
class MemSSADF : public DominanceFrontier
{
public:
    MemSSADF() : DominanceFrontier()
    {}

    bool runOnDT(DominatorTree& dt)
    {
        releaseMemory();
        analyze(dt);
        return false;
    }
};

/*!
 * SVFG Builder
 */
class SVFGBuilder
{

public:
    typedef PointerAnalysis::CallSiteSet CallSiteSet;
    typedef PointerAnalysis::CallEdgeMap CallEdgeMap;
    typedef PointerAnalysis::FunctionSet FunctionSet;
    typedef SVFG::SVFGEdgeSetTy SVFGEdgeSet;

    /// Constructor
    SVFGBuilder(bool _SVFGWithIndCall = false): svfg(nullptr), SVFGWithIndCall(_SVFGWithIndCall) {}

    /// Destructor
    virtual ~SVFGBuilder() {}

    static SVFG* globalSvfg;

    SVFG* buildPTROnlySVFG(BVDataPTAImpl* pta);
    SVFG* buildFullSVFG(BVDataPTAImpl* pta);

    /// Clean up
    static void releaseSVFG()
    {
        if (globalSvfg)
            delete globalSvfg;
        globalSvfg = nullptr;
    }
    /// Get SVFG instance
    inline SVFG* getSVFG() const
    {
        return svfg;
    }

    /// Mark feasible VF edge by removing it from set vfEdgesAtIndCallSite
    inline void markValidVFEdge(SVFGEdgeSet& edges)
    {
        for(SVFGEdgeSet::iterator it = edges.begin(), eit = edges.end(); it!=eit; ++it)
            vfEdgesAtIndCallSite.erase(*it);
    }
    /// Return true if this is an VF Edge pre-connected by Andersen's analysis
    inline bool isSpuriousVFEdgeAtIndCallSite(const SVFGEdge* edge)
    {
        return vfEdgesAtIndCallSite.find(const_cast<SVFGEdge*>(edge))!=vfEdgesAtIndCallSite.end();
    }

    /// Build Memory SSA
    virtual MemSSA* buildMSSA(BVDataPTAImpl* pta, bool ptrOnlyMSSA);

protected:
    /// Create a DDA SVFG. By default actualOut and FormalIN are removed, unless withAOFI is set true.
    SVFG* build(BVDataPTAImpl* pta, VFG::VFGK kind);
    /// Can be rewritten by subclasses
    virtual void buildSVFG();
    /// Release global SVFG
    virtual void releaseMemory();

    /// SVFG Edges connected at indirect call/ret sites
    SVFGEdgeSet vfEdgesAtIndCallSite;
    SVFG* svfg;
    /// SVFG with precomputed indirect call edges
    bool SVFGWithIndCall;
};

} // End namespace SVF

#endif /* ANDERSENMEMSSA_H_ */
