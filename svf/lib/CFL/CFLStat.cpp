//===- CFLStat.cpp -- Statistics of CFL Reachability's analysis------------------//
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
 * CFLStat.cpp
 *
 *  Created on: 17 Sep, 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLStat.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;

/*!
 * Constructor
 */
CFLStat::CFLStat(CFLBase* p): PTAStat(p),pta(p)
{
    startClk();
}

/*!
 * Collect CFLGraph information
 */
void  CFLStat::CFLGraphStat()
{
    pta->countSumEdges();
    CFLGraph* CFLGraph = pta->getCFLGraph();

    timeStatMap["BuildingTime"] = pta->timeOfBuildCFLGraph;
    PTNumStatMap["NumOfNodes"] = CFLGraph->getTotalNodeNum();
    PTNumStatMap["NumOfEdges"] = CFLGraph->getCFLEdges().size();

    PTAStat::printStat("CFLGraph Stats");
}

void CFLStat::CFLGrammarStat()
{
    timeStatMap["BuildingTime"] = pta->timeOfBuildCFLGrammar;
    timeStatMap["NormalizationTime"] = pta->timeOfNormalizeGrammar;

    PTAStat::printStat("CFLGrammar Stats");
}

void CFLStat::CFLSolverStat()
{
    timeStatMap["AnalysisTime"] = pta->timeOfSolving;
    PTNumStatMap["numOfChecks"] = pta->numOfChecks;
    PTNumStatMap["numOfIteration"] = pta->numOfIteration;
    PTNumStatMap["SumEdges"] = pta->numOfStartEdges;

    PTAStat::printStat("CFL-reachability Solver Stats");
}

/*!
 * Start here
 */
void CFLStat::performStat()
{
    assert((SVFUtil::isa<CFLAlias, CFLVF>(pta)) && "not an CFLAlias pass!! what else??");
    endClk();

    // Grammar stat
    CFLGrammarStat();

    // CFLGraph stat
    CFLGraphStat();

    // Solver stat
    CFLSolverStat();

    // Stat about Call graph and General stat
    PTAStat::performStat();
}

