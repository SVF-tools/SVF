//===- PreAnalysis.h -- Pre-Analysis for Abstract Interpretation----------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * PreAnalysis.h
 *
 *  Created on: Feb 25, 2026
 *      Author: Jiawei Wang
 *
 * This file provides a pre-analysis phase for Abstract Interpretation.
 * It runs Andersen's pointer analysis and builds WTO (Weak Topological Order)
 * for each function before the main abstract interpretation.
 */

#ifndef INCLUDE_AE_SVFEXE_PREANALYSIS_H_
#define INCLUDE_AE_SVFEXE_PREANALYSIS_H_

#include "SVFIR/SVFIR.h"
#include "Graphs/ICFG.h"
#include "Graphs/CallGraph.h"
#include "Graphs/SCC.h"
#include "AE/Core/ICFGWTO.h"
#include "WPA/Andersen.h"

namespace SVF
{

class PreAnalysis
{
public:
    typedef SCCDetection<CallGraph*> CallGraphSCC;

    PreAnalysis(SVFIR* pag, ICFG* icfg);
    virtual ~PreAnalysis();

    /// Accessors for Andersen's results
    AndersenWaveDiff* getPointerAnalysis() const
    {
        return pta;
    }
    CallGraph* getCallGraph() const
    {
        return callGraph;
    }
    CallGraphSCC* getCallGraphSCC() const
    {
        return callGraphSCC;
    }
    /// Build WTO for each function using call graph SCC
    void initWTO();

    /// Accessors for WTO data
    const Map<const FunObjVar*, const ICFGWTO*>& getFuncToWTO() const
    {
        return funcToWTO;
    }

    /// Walk every function's WTO and populate cycleToValVars bottom-up.
    /// Called once right after initWTO(). No-op in dense mode.
    void initCycleValVars();

    /// Look up the ValVar id set of a WTO cycle. Returns nullptr if the
    /// cycle is unknown (e.g. dense mode, where the map is never built).
    const Set<const ValVar*> getCycleValVars(const ICFGCycleWTO* cycle) const
    {
        auto it = cycleToValVars.find(cycle);
        return it == cycleToValVars.end() ? Set<const ValVar*>() : it->second;
    }

private:
    SVFIR* svfir;
    ICFG* icfg;
    AndersenWaveDiff* pta;
    CallGraph* callGraph;
    CallGraphSCC* callGraphSCC;

    Map<const FunObjVar*, const ICFGWTO*> funcToWTO;

    /// Pre-computed (semi-sparse only) map from a WTO cycle to the IDs of
    /// every ValVar whose def-site is inside that cycle, including all
    /// nested sub-cycles. Empty in dense mode.
    Map<const ICFGCycleWTO*, Set<const ValVar*>> cycleToValVars;
};

} // End namespace SVF

#endif /* INCLUDE_AE_SVFEXE_PREANALYSIS_H_ */
