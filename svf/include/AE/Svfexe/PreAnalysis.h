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

class SparseDefUse;

class PreAnalysis
{
public:
    typedef SCCDetection<CallGraph*> CallGraphSCC;

    PreAnalysis(SVFIR* pag, ICFG* icfg);
    virtual ~PreAnalysis();

    /// Accessors for Andersen's results
    AndersenWaveDiff* getPointerAnalysis() const { return pta; }
    CallGraph* getCallGraph() const { return callGraph; }
    CallGraphSCC* getCallGraphSCC() const { return callGraphSCC; }

    /// Build WTO for each function using call graph SCC
    void initWTO();

    /// Build the sparse def-use table for sparse state propagation
    void buildDefUseTable(ICFG* icfg);

    /// Get the sparse def-use table (nullptr if not built)
    SparseDefUse* getDefUseTable() const { return defUseTable; }

    /// Get points-to set for a variable from Andersen's analysis
    const PointsTo& getPts(NodeID id) const;

    /// Accessors for WTO data
    const Map<const FunObjVar*, const ICFGWTO*>& getFuncToWTO() const
    {
        return funcToWTO;
    }

private:
    SVFIR* svfir;
    ICFG* icfg;
    AndersenWaveDiff* pta;
    CallGraph* callGraph;
    CallGraphSCC* callGraphSCC;

    Map<const FunObjVar*, const ICFGWTO*> funcToWTO;
    SparseDefUse* defUseTable{nullptr};
};

} // End namespace SVF

#endif /* INCLUDE_AE_SVFEXE_PREANALYSIS_H_ */
