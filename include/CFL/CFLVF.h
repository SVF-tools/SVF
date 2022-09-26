//===----- CFLVF.h -- CFL Value-Flow Client--------------//
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
 * CFLVF.h
 *
 *  Created on: September 5, 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFLVF_H_
#define INCLUDE_CFL_CFLVF_H_

#include "CFL/CFLSolver.h"
#include "CFL/CFGNormalizer.h"
#include "CFL/GrammarBuilder.h"
#include "CFL/CFLGraphBuilder.h"
#include "CFL/CFLGramGraphChecker.h"
#include "MemoryModel/PointerAnalysis.h"
#include "Graphs/ConsG.h"
#include "Util/Options.h"
#include "SABER/SaberSVFGBuilder.h"

namespace SVF
{

class CFLVF : public BVDataPTAImpl
{

public:
    typedef OrderedMap<CallSite, NodeID> CallSite2DummyValPN;

    CFLVF(SVFIR* ir) : BVDataPTAImpl(ir, PointerAnalysis::CFLFSCS_WPA, false), svfir(ir), graph(nullptr), grammar(nullptr), solver(nullptr)
    {
    }

    /// Destructor
    virtual ~CFLVF()
    {
        delete solver;
    }

    /// Start Analysis here (main part of pointer analysis).
    virtual void analyze();

private:
    CallSite2DummyValPN callsite2DummyValPN;        ///< Map an instruction to a dummy obj which created at an indirect callsite, which invokes a heap allocator
    SVFIR* svfir;
    CFLGraph* graph;
    CFLGrammar* grammar;
    CFLSolver *solver;
    SaberSVFGBuilder memSSA;
    SVFG* svfg;
};

} // End namespace SVF

#endif /* INCLUDE_CFL_CFLVF_H_*/
