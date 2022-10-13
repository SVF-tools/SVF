//===----- CFLBase.h -- CFL Analysis Client Base--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFLAlias.h
 *
 *  Created on: Oct 12, 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFLBASE_H_
#define INCLUDE_CFL_CFLBASE_H_

#include "CFL/CFLSolver.h"
#include "CFL/CFGNormalizer.h"
#include "CFL/GrammarBuilder.h"
#include "CFL/CFLGraphBuilder.h"
#include "CFL/CFLGramGraphChecker.h"
#include "MemoryModel/PointerAnalysis.h"
#include "Graphs/ConsG.h"
#include "Util/Options.h"
#include "Util/SVFBasicTypes.h"

namespace SVF
{

class CFLStat;

class CFLBase : public BVDataPTAImpl
{

public:
    CFLBase(SVFIR* ir, PointerAnalysis::PTATY pty) : BVDataPTAImpl(ir, pty), svfir(ir), graph(nullptr), grammar(nullptr), solver(nullptr)
    {
    }

    /// Destructor
    virtual ~CFLBase()
    {
        delete solver;
    }

    /// Initialize the grammar, graph, solver
    virtual void initialize() = 0;

    /// Print grammar and graph
    virtual void finalize() = 0;

    /// Start Analysis here (main part of pointer analysis).
    virtual void analyze() = 0;

    /// Get CFL graph
    CFLGraph* getCFLGraph()
    {
        return graph;
    }

    virtual void countSumEdges() = 0;

    /// Statistics
    //@{
    static u32_t numOfProcessedAddr;   /// Number of processed Addr edge
    static u32_t numOfProcessedCopy;   /// Number of processed Copy edge
    static u32_t numOfProcessedGep;    /// Number of processed Gep edge
    static u32_t numOfProcessedLoad;   /// Number of processed Load edge
    static u32_t numOfProcessedStore;  /// Number of processed Store edge
    static u32_t numOfSfrs;
    static u32_t numOfFieldExpand;

    static u32_t numOfSCCDetection;
    static double timeOfSCCDetection;
    static double timeOfSCCMerges;
    static double timeOfCollapse;
    static u32_t AveragePointsToSetSize;
    static u32_t MaxPointsToSetSize;
    static double timeOfProcessCopyGep;
    static double timeOfProcessLoadStore;
    static double timeOfUpdateCallGraph;
    static double timeOfSolving;
    static double numOfSumEdges;
    //@}

protected:
    SVFIR* svfir;
    CFLGraph* graph;
    CFLGrammar* grammar;
    CFLSolver *solver;
};

} // End namespace SVF

#endif /* INCLUDE_CFL_CFLBASE_H_*/
