//===- TypeAnalysis.h -- Fast type-based analysis without pointer analysis---------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * TypeAnalysis.h
 *
 *  Created on: 7 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_WPA_TYPEANALYSIS_H_
#define INCLUDE_WPA_TYPEANALYSIS_H_

#include "WPA/Andersen.h"

namespace SVF
{

class TypeAnalysis:  public AndersenBase
{

public:
    /// Constructor
    TypeAnalysis(SVFIR* pag)
        :  AndersenBase(pag, TypeCPP_WPA)
    {
    }

    /// Destructor
    virtual ~TypeAnalysis()
    {
    }

    /// Type analysis
    void analyze();

    /// Initialize analysis
    void initialize();

    /// Finalize analysis
    virtual inline void finalize();

    /// Resolve callgraph based on CHA
    void callGraphSolveBasedOnCHA(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges);

    /// Statistics of CHA and callgraph
    void dumpCHAStats();

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const TypeAnalysis *)
    {
        return true;
    }
    static inline bool classof(const PointerAnalysis *pta)
    {
        return (pta->getAnalysisTy() == TypeCPP_WPA);
    }
    //@}
};

} // End namespace SVF

#endif /* INCLUDE_WPA_TYPEANALYSIS_H_ */
