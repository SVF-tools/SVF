//===- CFLStat.cpp -- Statistics of CFL Reachability's analysis------------------//
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
void  CFLStat::collectCFLInfo(CFLGraph* CFLGraph)
{
    timeStatMap["timeOfBuildCFLGraph"] = pta->timeOfBuildCFLGraph;
    PTNumStatMap["NumOfNodes"] = CFLGraph->getTotalNodeNum();
    PTNumStatMap["NumOfEdges"] = CFLGraph->getCFLEdges().size();

    PTAStat::printStat("CFLGraph Stats");
}

void CFLStat::constraintGraphStat()
{


    u32_t numOfCopys = 0;
    u32_t numOfGeps = 0;

    u32_t totalNodeNumber = 0;
    u32_t cgNodeNumber = 0;
    u32_t objNodeNumber = 0;
    u32_t addrtotalIn = 0;
    u32_t addrmaxIn = 0;
    u32_t addrmaxOut = 0;
    u32_t copytotalIn = 0;
    u32_t copymaxIn = 0;
    u32_t copymaxOut = 0;
    u32_t loadtotalIn = 0;
    u32_t loadmaxIn = 0;
    u32_t loadmaxOut = 0;
    u32_t storetotalIn = 0;
    u32_t storemaxIn = 0;
    u32_t storemaxOut = 0;

    double storeavgIn = (double)storetotalIn/cgNodeNumber;
    double loadavgIn = (double)loadtotalIn/cgNodeNumber;
    double copyavgIn = (double)copytotalIn/cgNodeNumber;
    double addravgIn = (double)addrtotalIn/cgNodeNumber;
    double avgIn = (double)(addrtotalIn + copytotalIn + loadtotalIn + storetotalIn)/cgNodeNumber;


    PTNumStatMap["NumOfCGNode"] = totalNodeNumber;
    PTNumStatMap["TotalValidNode"] = cgNodeNumber;
    PTNumStatMap["TotalValidObjNode"] = objNodeNumber;
    PTNumStatMap["NumOfCopys"] = numOfCopys;
    PTNumStatMap["NumOfGeps"] =  numOfGeps;
    PTNumStatMap["MaxInCopyEdge"] = copymaxIn;
    PTNumStatMap["MaxOutCopyEdge"] = copymaxOut;
    PTNumStatMap["MaxInLoadEdge"] = loadmaxIn;
    PTNumStatMap["MaxOutLoadEdge"] = loadmaxOut;
    PTNumStatMap["MaxInStoreEdge"] = storemaxIn;
    PTNumStatMap["MaxOutStoreEdge"] = storemaxOut;
    PTNumStatMap["AvgIn/OutStoreEdge"] = storeavgIn;
    PTNumStatMap["MaxInAddrEdge"] = addrmaxIn;
    PTNumStatMap["MaxOutAddrEdge"] = addrmaxOut;
    timeStatMap["AvgIn/OutCopyEdge"] = copyavgIn;
    timeStatMap["AvgIn/OutLoadEdge"] = loadavgIn;
    timeStatMap["AvgIn/OutAddrEdge"] = addravgIn;
    timeStatMap["AvgIn/OutEdge"] = avgIn;

    PTAStat::printStat("CFL Graph Stats");
}

void CFLStat::CFLGrammarStat()
{
    timeStatMap["timeOfBuildCFLGrammar"] = pta->timeOfBuildCFLGrammar;
    timeStatMap["timeOfNormalizeGrammar"] = pta->timeOfNormalizeGrammar;
    PTAStat::printStat("CFLGrammar Stats");
}

/*!
 * Start here
 */
void CFLStat::performStat()
{
    assert((SVFUtil::isa<CFLAlias>(pta)||SVFUtil::isa<CFLVF>(pta)) && "not an CFLAlias pass!! what else??");
    endClk();

    pta->countSumEdges();
    
    // CFLGraph stat
    CFLGraph* CFLGraph = pta->getCFLGraph();
    collectCFLInfo(CFLGraph);

    // Solver stat
    timeStatMap["AnalysisTime"] = pta->timeOfSolving;
    PTNumStatMap["SumEdges"] = pta->numOfStartEdges;
    PTAStat::printStat("CFL-reachability analysis Stats");
    
    // Grammar stat
    CFLGrammarStat();

    PTAStat::performStat();

    // ConstraintGraph stat
    constraintGraphStat();
}

