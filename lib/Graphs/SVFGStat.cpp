//===- SVFGStat.cpp -- SVFG statistics---------------------------------------//
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
 * SVFGStat.cpp
 *
 *  Created on: 28/11/2013
 *      Author: yesen
 */

#include "Graphs/SVFG.h"
#include "Graphs/SVFGStat.h"
#include "Graphs/PTACallGraph.h"

using namespace SVF;

const char* MemSSAStat::TotalTimeOfConstructMemSSA = "TotalMSSATime";	///< Total time for constructing memory SSA
const char* MemSSAStat::TimeOfGeneratingMemRegions  = "GenRegionTime";	///< Time for allocating regions
const char* MemSSAStat::TimeOfCreateMUCHI  = "GenMUCHITime";	///< Time for generating mu/chi for load/store/calls
const char* MemSSAStat::TimeOfInsertingPHI = "InsertPHITime";	///< Time for inserting phis
const char* MemSSAStat::TimeOfSSARenaming = "SSARenameTime";	///< Time for SSA rename

const char* MemSSAStat::NumOfMaxRegion = "MaxRegSize";	///< Number of max points-to set in region.
const char* MemSSAStat::NumOfAveragePtsInRegion = "AverageRegSize";	///< Number of average points-to set in region.
const char* MemSSAStat::NumOfMemRegions = "MemRegions";	///< Number of memory regions
const char* MemSSAStat::NumOfEntryChi = "FunEntryChi";	///< Number of function entry chi
const char* MemSSAStat::NumOfRetMu = "FunRetMu";	///< Number of function return mu
const char* MemSSAStat::NumOfCSChi = "CSChiNode";	///< Number of call site chi
const char* MemSSAStat::NumOfCSMu = "CSMuNode";	///< Number of call site mu
const char* MemSSAStat::NumOfLoadMu = "LoadMuNode";	///< Number of load mu
const char* MemSSAStat::NumOfStoreChi = "StoreChiNode";	///< Number of store chi
const char* MemSSAStat::NumOfMSSAPhi = "MSSAPhi";	///< Number of non-top level ptr phi

const char* MemSSAStat::NumOfFunHasEntryChi = "FunHasEntryChi";	///< Number of functions which have entry chi
const char* MemSSAStat::NumOfFunHasRetMu = "FunHasRetMu";	///< Number of functions which have return mu
const char* MemSSAStat::NumOfCSHasChi = "CSHasChi";	///< Number of call sites which have chi
const char* MemSSAStat::NumOfCSHasMu = "CSHasMu";	///< Number of call sites which have mu
const char* MemSSAStat::NumOfLoadHasMu = "LoadHasMu";	///< Number of loads which have mu
const char* MemSSAStat::NumOfStoreHasChi = "StoreHasChi";	///< Number of stores which have chi
const char* MemSSAStat::NumOfBBHasMSSAPhi = "BBHasMSSAPhi";	///< Number of basic blocks which have mssa phi

/*!
 * Constructor
 */
MemSSAStat::MemSSAStat(MemSSA* memSSA) : PTAStat(nullptr)
{
    mssa = memSSA;
    startClk();
}

/*!
 * Start stating
 */
void MemSSAStat::performStat()
{

    endClk();

    MRGenerator* mrGenerator = mssa->getMRGenerator();
    u32_t regionNumber = mrGenerator->getMRNum();

    u32_t maxRegionSize = 0;
    u32_t totalRegionPtsNum = 0;
    MRGenerator::MRSet & mrSet = mrGenerator->getMRSet();
    MRGenerator::MRSet::const_iterator it = mrSet.begin();
    MRGenerator::MRSet::const_iterator eit = mrSet.end();
    for (; it != eit; it++)
    {
        const MemRegion* region = *it;
        u32_t regionSize = region->getRegionSize();
        if (regionSize > maxRegionSize)
            maxRegionSize = regionSize;
        totalRegionPtsNum += regionSize;
    }

    timeStatMap[TotalTimeOfConstructMemSSA] = (endTime - startTime)/TIMEINTERVAL;
    timeStatMap[TimeOfGeneratingMemRegions] = MemSSA::timeOfGeneratingMemRegions;
    timeStatMap[TimeOfCreateMUCHI] =  MemSSA::timeOfCreateMUCHI;
    timeStatMap[TimeOfInsertingPHI] =  MemSSA::timeOfInsertingPHI;
    timeStatMap[TimeOfSSARenaming] =  MemSSA::timeOfSSARenaming;

    PTNumStatMap[NumOfMaxRegion] = maxRegionSize;
    timeStatMap[NumOfAveragePtsInRegion] = (regionNumber == 0) ? 0 : ((double)totalRegionPtsNum / regionNumber);
    PTNumStatMap[NumOfMemRegions] = regionNumber;
    PTNumStatMap[NumOfEntryChi] = mssa->getFunEntryChiNum();
    PTNumStatMap[NumOfRetMu] = mssa->getFunRetMuNum();
    PTNumStatMap[NumOfCSChi] = mssa->getCallSiteChiNum();
    PTNumStatMap[NumOfCSMu] = mssa->getCallSiteMuNum();
    PTNumStatMap[NumOfLoadMu] = mssa->getLoadMuNum();
    PTNumStatMap[NumOfStoreChi] = mssa->getStoreChiNum();
    PTNumStatMap[NumOfMSSAPhi] = mssa->getBBPhiNum();

    PTNumStatMap[NumOfFunHasEntryChi] = mssa->getFunToEntryChiSetMap().size();
    PTNumStatMap[NumOfFunHasRetMu] = mssa->getFunToRetMuSetMap().size();
    PTNumStatMap[NumOfCSHasChi] = mssa->getCallSiteToChiSetMap().size();
    PTNumStatMap[NumOfCSHasMu] = mssa->getCallSiteToMuSetMap().size();
    PTNumStatMap[NumOfLoadHasMu] = mssa->getLoadToMUSetMap().size();
    PTNumStatMap[NumOfStoreHasChi] = mssa->getStoreToChiSetMap().size();
    PTNumStatMap[NumOfBBHasMSSAPhi] = mssa->getBBToPhiSetMap().size();

    printStat();

}

/*!
 * Print statistics
 */
void MemSSAStat::printStat()
{

    std::cout << "\n****Memory SSA Statistics****\n";
    PTAStat::printStat();
}

/*!
 * Constructor
 */
SVFGStat::SVFGStat(SVFG* g) : PTAStat(nullptr)
{
    graph = g;
    clear();
    startClk();
    connectDirSVFGEdgeTimeStart = connectDirSVFGEdgeTimeEnd = 0;
    connectIndSVFGEdgeTimeStart = connectIndSVFGEdgeTimeEnd = 0;
    addTopLevelNodeTimeStart = addTopLevelNodeTimeEnd = 0;
    addAddrTakenNodeTimeStart = addAddrTakenNodeTimeEnd = 0;
    svfgOptTimeStart = svfgOptTimeEnd = 0;
}

void SVFGStat::clear()
{
    numOfNodes = 0;
    numOfFormalIn = numOfFormalOut = numOfFormalParam = numOfFormalRet = 0;
    numOfActualIn = numOfActualOut = numOfActualParam = numOfActualRet = 0;
    numOfLoad = numOfStore = numOfCopy = numOfGep = numOfAddr = 0;
    numOfMSSAPhi = numOfPhi = 0;
    avgWeight = 0;
    avgInDegree = avgOutDegree = 0;
    maxInDegree = maxOutDegree = 0;
    avgIndInDegree = avgIndOutDegree = 0;
    maxIndInDegree = maxIndOutDegree = 0;

    totalInEdge = totalOutEdge = 0;
    totalIndInEdge = totalIndOutEdge = 0;
    totalIndEdgeLabels = 0;

    totalIndCallEdge = totalIndRetEdge = 0;
    totalDirCallEdge = totalDirRetEdge = 0;
}

NodeID SVFGStat::getSCCRep(SVFGSCC* scc, NodeID id) const
{
    return scc->repNode(id);
}
NodeID SVFGStat::nodeInCycle(SVFGSCC* scc, NodeID id) const
{
    return scc->isInCycle(id);
}

void SVFGStat::performStat()
{
    endClk();

    clear();

    processGraph();

    timeStatMap["TotalTime"] = (endTime - startTime)/TIMEINTERVAL;

    timeStatMap["ConnDirEdgeTime"] = (connectDirSVFGEdgeTimeEnd - connectDirSVFGEdgeTimeStart)/TIMEINTERVAL;

    timeStatMap["ConnIndEdgeTime"] = (connectIndSVFGEdgeTimeEnd - connectIndSVFGEdgeTimeStart)/TIMEINTERVAL;

    timeStatMap["TLNodeTime"] = (addTopLevelNodeTimeEnd - addTopLevelNodeTimeStart)/TIMEINTERVAL;

    timeStatMap["ATNodeTime"] = (addAddrTakenNodeTimeEnd - addAddrTakenNodeTimeStart)/TIMEINTERVAL;

    timeStatMap["OptTime"] = (svfgOptTimeEnd - svfgOptTimeStart)/TIMEINTERVAL;

    PTNumStatMap["TotalNode"] = numOfNodes;

    PTNumStatMap["FormalIn"] = numOfFormalIn;
    PTNumStatMap["FormalOut"] = numOfFormalOut;
    PTNumStatMap["FormalParam"] = numOfFormalParam;
    PTNumStatMap["FormalRet"] = numOfFormalRet;

    PTNumStatMap["ActualIn"] = numOfActualIn;
    PTNumStatMap["ActualOut"] = numOfActualOut;
    PTNumStatMap["ActualParam"] = numOfActualParam;
    PTNumStatMap["ActualRet"] = numOfActualRet;

    PTNumStatMap["Addr"] = numOfAddr;
    PTNumStatMap["Copy"] = numOfCopy;
    PTNumStatMap["Gep"] = numOfGep;
    PTNumStatMap["Store"] = numOfStore;
    PTNumStatMap["Load"] = numOfLoad;

    PTNumStatMap["PHI"] = numOfPhi;
    PTNumStatMap["MSSAPhi"] = numOfMSSAPhi;

    timeStatMap["AvgWeight"] = (totalIndInEdge == 0) ? 0 : ((double)avgWeight / totalIndInEdge);

    PTNumStatMap["TotalEdge"] = totalInEdge;
    PTNumStatMap["DirectEdge"] = totalInEdge - totalIndInEdge;
    PTNumStatMap["IndirectEdge"] = totalIndInEdge;
    PTNumStatMap["IndirectEdgeLabels"] = totalIndEdgeLabels;

    PTNumStatMap["IndCallEdge"] = totalIndCallEdge;
    PTNumStatMap["IndRetEdge"] = totalIndRetEdge;
    PTNumStatMap["DirectCallEdge"] = totalDirCallEdge;
    PTNumStatMap["DirectRetEdge"] = totalDirRetEdge;

    PTNumStatMap["AvgInDegree"] = avgInDegree;
    PTNumStatMap["AvgOutDegree"] = avgOutDegree;
    PTNumStatMap["MaxInDegree"] = maxInDegree;
    PTNumStatMap["MaxOutDegree"] = maxOutDegree;

    PTNumStatMap["AvgIndInDeg"] = avgIndInDegree;
    PTNumStatMap["AvgIndOutDeg"] = avgIndOutDegree;
    PTNumStatMap["MaxIndInDeg"] = maxIndInDegree;
    PTNumStatMap["MaxIndOutDeg"] = maxIndOutDegree;

    printStat();
}

void SVFGStat::processGraph()
{
    NodeSet nodeHasIndInEdge;
    NodeSet nodeHasIndOutEdge;

    SVFG::SVFGNodeIDToNodeMapTy::iterator it = graph->begin();
    SVFG::SVFGNodeIDToNodeMapTy::iterator eit = graph->end();
    for (; it != eit; ++it)
    {
        numOfNodes++;
        if (SVFUtil::isa<FormalINSVFGNode>(it->second))
            numOfFormalIn++;
        else if (SVFUtil::isa<FormalOUTSVFGNode>(it->second))
            numOfFormalOut++;
        else if (SVFUtil::isa<FormalParmSVFGNode>(it->second))
            numOfFormalParam++;
        else if (SVFUtil::isa<FormalRetSVFGNode>(it->second))
            numOfFormalRet++;
        else if (SVFUtil::isa<ActualINSVFGNode>(it->second))
            numOfActualIn++;
        else if (SVFUtil::isa<ActualOUTSVFGNode>(it->second))
            numOfActualOut++;
        else if (SVFUtil::isa<ActualParmSVFGNode>(it->second))
            numOfActualParam++;
        else if (SVFUtil::isa<ActualRetSVFGNode>(it->second))
            numOfActualRet++;
        else if (SVFUtil::isa<AddrSVFGNode>(it->second))
            numOfAddr++;
        else if (SVFUtil::isa<CopySVFGNode>(it->second))
            numOfCopy++;
        else if (SVFUtil::isa<GepSVFGNode>(it->second))
            numOfGep++;
        else if (SVFUtil::isa<LoadSVFGNode>(it->second))
            numOfLoad++;
        else if (SVFUtil::isa<StoreSVFGNode>(it->second))
            numOfStore++;
        else if (SVFUtil::isa<PHISVFGNode>(it->second))
            numOfPhi++;
        else if (SVFUtil::isa<MSSAPHISVFGNode>(it->second))
            numOfMSSAPhi++;

        SVFGNode* node = it->second;
        calculateNodeDegrees(node, nodeHasIndInEdge, nodeHasIndOutEdge);
    }

    if (numOfNodes > 0)
    {
        avgInDegree = totalInEdge / numOfNodes;
        avgOutDegree = totalOutEdge / numOfNodes;
    }

    if (!nodeHasIndInEdge.empty())
        avgIndInDegree = totalIndInEdge / nodeHasIndInEdge.size();

    if (!nodeHasIndOutEdge.empty())
        avgIndOutDegree = totalIndOutEdge / nodeHasIndOutEdge.size();
}

void SVFGStat::calculateNodeDegrees(SVFGNode* node, NodeSet& nodeHasIndInEdge, NodeSet& nodeHasIndOutEdge)
{
    // Incoming edge
    const SVFGEdge::SVFGEdgeSetTy& inEdges = node->getInEdges();
    // total in edge
    if (inEdges.size() > maxInDegree)
        maxInDegree = inEdges.size();
    totalInEdge += inEdges.size();

    // indirect in edge
    Size_t indInEdges = 0;
    SVFGEdge::SVFGEdgeSetTy::const_iterator edgeIt = inEdges.begin();
    SVFGEdge::SVFGEdgeSetTy::const_iterator edgeEit = inEdges.end();
    for (; edgeIt != edgeEit; ++edgeIt)
    {
        if (IndirectSVFGEdge* edge = SVFUtil::dyn_cast<IndirectSVFGEdge>(*edgeIt))
        {
            indInEdges++;
            nodeHasIndInEdge.insert(node->getId());
            // get edge's weight
            // TODO: try a new method to calculate weight.
            const NodeBS& cpts = edge->getPointsTo();
            avgWeight += cpts.count();
            totalIndEdgeLabels += cpts.count();
        }

        if (SVFUtil::isa<CallDirSVFGEdge>(*edgeIt))
            totalDirCallEdge++;
        else if (SVFUtil::isa<CallIndSVFGEdge>(*edgeIt))
            totalIndCallEdge++;
        else if (SVFUtil::isa<RetDirSVFGEdge>(*edgeIt))
            totalDirRetEdge++;
        else if (SVFUtil::isa<RetIndSVFGEdge>(*edgeIt))
            totalIndRetEdge++;
    }

    if (indInEdges > maxIndInDegree)
        maxIndInDegree = indInEdges;
    totalIndInEdge += indInEdges;

    /*-----------------------------------------------------*/

    // Outgoing edge
    const SVFGEdge::SVFGEdgeSetTy& outEdges = node->getOutEdges();
    // total out edge
    if (outEdges.size() > maxOutDegree)
        maxOutDegree = outEdges.size();
    totalOutEdge += outEdges.size();

    // indirect out edge
    Size_t indOutEdges = 0;
    edgeIt = outEdges.begin();
    edgeEit = outEdges.end();
    for (; edgeIt != edgeEit; ++edgeIt)
    {
        if ((*edgeIt)->isIndirectVFGEdge())
        {
            indOutEdges++;
            nodeHasIndOutEdge.insert(node->getId());
        }
    }

    if (indOutEdges > maxIndOutDegree)
        maxIndOutDegree = indOutEdges;
    totalIndOutEdge += indOutEdges;
}

void SVFGStat::performSCCStat(SVFGEdgeSet insensitiveCalRetEdges)
{

    generalNumMap.clear();
    PTNumStatMap.clear();
    timeStatMap.clear();

    unsigned totalNode = 0;
    unsigned totalCycle = 0;
    unsigned nodeInCycle = 0;
    unsigned maxNodeInCycle = 0;
    unsigned totalEdge = 0;
    unsigned edgeInCycle = 0;

    unsigned totalDirectEdge = 0;
    unsigned directEdgeInCycle = 0;
    unsigned totalIndirectEdge = 0;
    unsigned indirectEdgeInCycle = 0;
    unsigned totalCallEdge = 0;
    unsigned callEdgeInCycle = 0;
    unsigned insensitiveCallEdge = 0;
    unsigned totalRetEdge = 0;
    unsigned retEdgeInCycle = 0;
    unsigned insensitiveRetEdge = 0;

    SVFGSCC* svfgSCC = new SVFGSCC(graph);
    svfgSCC->find();

    NodeSet sccRepNodeSet;
    SVFG::SVFGNodeIDToNodeMapTy::iterator it = graph->begin();
    SVFG::SVFGNodeIDToNodeMapTy::iterator eit = graph->end();
    for (; it != eit; ++it)
    {
        totalNode++;
        if(svfgSCC->isInCycle(it->first))
        {
            nodeInCycle++;
            sccRepNodeSet.insert(svfgSCC->repNode(it->first));
            const NodeBS& subNodes = svfgSCC->subNodes(it->first);
            if(subNodes.count() > maxNodeInCycle)
                maxNodeInCycle = subNodes.count();
        }

        SVFGEdge::SVFGEdgeSetTy::const_iterator edgeIt = it->second->InEdgeBegin();
        SVFGEdge::SVFGEdgeSetTy::const_iterator edgeEit = it->second->InEdgeEnd();
        for (; edgeIt != edgeEit; ++edgeIt)
        {

            const SVFGEdge *edge = *edgeIt;
            totalEdge++;
            bool eCycle = false;
            if(getSCCRep(svfgSCC,edge->getSrcID()) == getSCCRep(svfgSCC,edge->getDstID()))
            {
                edgeInCycle++;
                eCycle = true;
            }

            if (edge->isDirectVFGEdge())
            {
                totalDirectEdge++;
                if(eCycle)
                    directEdgeInCycle++;

            }
            if (edge->isIndirectVFGEdge())
            {
                totalIndirectEdge++;
                if(eCycle)
                    indirectEdgeInCycle++;
            }
            if (edge->isCallVFGEdge())
            {
                totalCallEdge++;
                if(eCycle)
                {
                    callEdgeInCycle++;
                }

                if(insensitiveCalRetEdges.find(edge)!=insensitiveCalRetEdges.end())
                {
                    insensitiveCallEdge++;
                }
            }
            if (edge->isRetVFGEdge())
            {
                totalRetEdge++;
                if(eCycle)
                {
                    retEdgeInCycle++;
                }

                if(insensitiveCalRetEdges.find(edge)!=insensitiveCalRetEdges.end())
                {
                    insensitiveRetEdge++;
                }
            }
        }
    }


    totalCycle = sccRepNodeSet.size();


    PTNumStatMap["TotalNode"] = totalNode;
    PTNumStatMap["TotalCycle"] = totalCycle;
    PTNumStatMap["NodeInCycle"] = nodeInCycle;
    PTNumStatMap["MaxNodeInCycle"] = maxNodeInCycle;
    PTNumStatMap["TotalEdge"] = totalEdge;
    PTNumStatMap["EdgeInCycle"] = edgeInCycle;

    PTNumStatMap["TotalDirEdge"] = totalDirectEdge;
    PTNumStatMap["DirEdgeInCycle"] = directEdgeInCycle;
    PTNumStatMap["TotalIndEdge"] = totalIndirectEdge;
    PTNumStatMap["IndEdgeInCycle"] = indirectEdgeInCycle;

    PTNumStatMap["TotalCallEdge"] = totalCallEdge;
    PTNumStatMap["CallEdgeInCycle"] = callEdgeInCycle;
    PTNumStatMap["InsenCallEdge"] = insensitiveCallEdge;

    PTNumStatMap["TotalRetEdge"] = totalRetEdge;
    PTNumStatMap["RetEdgeInCycle"] = retEdgeInCycle;
    PTNumStatMap["InsenRetEdge"] = insensitiveRetEdge;


    std::cout << "\n****SVFG SCC Stat****\n";
    PTAStat::printStat();

    delete svfgSCC;

}

void SVFGStat::printStat()
{
    std::cout << "\n****SVFG Statistics****\n";
    PTAStat::printStat();
}
