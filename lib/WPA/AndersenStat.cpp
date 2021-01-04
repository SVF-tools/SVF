//===- AndersenStat.cpp -- Statistics of Andersen's analysis------------------//
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
 * AndersenStat.cpp
 *
 *  Created on: Oct 12, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/LLVMUtil.h"
#include "WPA/WPAStat.h"
#include "WPA/Andersen.h"

using namespace SVF;
using namespace SVFUtil;

u32_t AndersenStat::_MaxPtsSize = 0;
u32_t AndersenStat::_NumOfCycles = 0;
u32_t AndersenStat::_NumOfPWCCycles = 0;
u32_t AndersenStat::_NumOfNodesInCycles = 0;
u32_t AndersenStat::_MaxNumOfNodesInSCC = 0;

const char* AndersenStat::CollapseTime = "CollapseTime";

/*!
 * Constructor
 */
AndersenStat::AndersenStat(AndersenBase* p): PTAStat(p),pta(p)
{
    _NumOfNullPtr = 0;
    _NumOfConstantPtr= 0;
    _NumOfBlackholePtr = 0;
    startClk();
}

/*!
 * Collect cycle information
 */
void AndersenStat::collectCycleInfo(ConstraintGraph* consCG)
{
    _NumOfCycles = 0;
    _NumOfPWCCycles = 0;
    _NumOfNodesInCycles = 0;
    NodeSet repNodes;
    repNodes.clear();
    for(ConstraintGraph::iterator it = consCG->begin(), eit = consCG->end(); it!=eit; ++it)
    {
        // sub nodes have been removed from the constraint graph, only rep nodes are left.
        NodeID repNode = consCG->sccRepNode(it->first);
        NodeBS& subNodes = consCG->sccSubNodes(repNode);
        NodeBS clone = subNodes;
        for (NodeBS::iterator it = subNodes.begin(), eit = subNodes.end(); it != eit; ++it)
        {
            NodeID nodeId = *it;
            PAGNode* pagNode = pta->getPAG()->getPAGNode(nodeId);
            if (SVFUtil::isa<ObjPN>(pagNode) && pta->isFieldInsensitive(nodeId))
            {
                NodeID baseId = consCG->getBaseObjNode(nodeId);
                clone.reset(nodeId);
                clone.set(baseId);
            }
        }
        u32_t num = clone.count();
        if (num > 1)
        {
            if(repNodes.insert(repNode).second)
            {
                _NumOfNodesInCycles += num;
                if(consCG->isPWCNode(repNode))
                    _NumOfPWCCycles ++;
            }
            if( num > _MaxNumOfNodesInSCC)
                _MaxNumOfNodesInSCC = num;
        }
    }
    _NumOfCycles += repNodes.size();
}

void AndersenStat::constraintGraphStat()
{


    ConstraintGraph* consCG = pta->getConstraintGraph();

    u32_t numOfCopys = 0;
    u32_t numOfGeps = 0;
    // collect copy and gep edges
    for(ConstraintEdge::ConstraintEdgeSetTy::iterator it = consCG->getDirectCGEdges().begin(),
            eit = consCG->getDirectCGEdges().end(); it!=eit; ++it)
    {
        if(SVFUtil::isa<CopyCGEdge>(*it))
            numOfCopys++;
        else if(SVFUtil::isa<GepCGEdge>(*it))
            numOfGeps++;
        else
            assert(false && "what else!!");
    }

    u32_t totalNodeNumber = 0;
    u32_t cgNodeNumber = 0;
    u32_t objNodeNumber = 0;
    u32_t addrtotalIn = 0;
    u32_t addrtotalOut = 0;
    u32_t addrmaxIn = 0;
    u32_t addrmaxOut = 0;
    u32_t copytotalIn = 0;
    u32_t copytotalOut = 0;
    u32_t copymaxIn = 0;
    u32_t copymaxOut = 0;
    u32_t loadtotalIn = 0;
    u32_t loadtotalOut = 0;
    u32_t loadmaxIn = 0;
    u32_t loadmaxOut = 0;
    u32_t storetotalIn = 0;
    u32_t storetotalOut = 0;
    u32_t storemaxIn = 0;
    u32_t storemaxOut = 0;


    for (ConstraintGraph::ConstraintNodeIDToNodeMapTy::iterator nodeIt = consCG->begin(), nodeEit = consCG->end();
            nodeIt != nodeEit; nodeIt++)
    {
        totalNodeNumber++;
        if(nodeIt->second->getInEdges().empty() && nodeIt->second->getOutEdges().empty())
            continue;
        cgNodeNumber++;
        if(SVFUtil::isa<ObjPN>(pta->getPAG()->getPAGNode(nodeIt->first)))
            objNodeNumber++;

        u32_t nCopyIn = nodeIt->second->getDirectInEdges().size();
        if(nCopyIn > copymaxIn)
            copymaxIn = nCopyIn;
        copytotalIn +=nCopyIn;
        u32_t nCopyOut = nodeIt->second->getDirectOutEdges().size();
        if(nCopyOut > copymaxOut)
            copymaxOut = nCopyOut;
        copytotalOut +=nCopyOut;
        u32_t nLoadIn = nodeIt->second->getLoadInEdges().size();
        if(nLoadIn > loadmaxIn)
            loadmaxIn = nLoadIn;
        loadtotalIn +=nLoadIn;
        u32_t nLoadOut = nodeIt->second->getLoadOutEdges().size();
        if(nLoadOut > loadmaxOut)
            loadmaxOut = nLoadOut;
        loadtotalOut +=nLoadOut;
        u32_t nStoreIn = nodeIt->second->getStoreInEdges().size();
        if(nStoreIn > storemaxIn)
            storemaxIn = nStoreIn;
        storetotalIn +=nStoreIn;
        u32_t nStoreOut = nodeIt->second->getStoreOutEdges().size();
        if(nStoreOut > storemaxOut)
            storemaxOut = nStoreOut;
        storetotalOut +=nStoreOut;
        u32_t nAddrIn = nodeIt->second->getAddrInEdges().size();
        if(nAddrIn > addrmaxIn)
            addrmaxIn = nAddrIn;
        addrtotalIn +=nAddrIn;
        u32_t nAddrOut = nodeIt->second->getAddrOutEdges().size();
        if(nAddrOut > addrmaxOut)
            addrmaxOut = nAddrOut;
        addrtotalOut +=nAddrOut;
    }
    double storeavgIn = (double)storetotalIn/cgNodeNumber;
    double loadavgIn = (double)loadtotalIn/cgNodeNumber;
    double copyavgIn = (double)copytotalIn/cgNodeNumber;
    double addravgIn = (double)addrtotalIn/cgNodeNumber;
    double avgIn = (double)(addrtotalIn + copytotalIn + loadtotalIn + storetotalIn)/cgNodeNumber;


    PTNumStatMap["NumOfCGNode"] = totalNodeNumber;
    PTNumStatMap["TotalValidNode"] = cgNodeNumber;
    PTNumStatMap["TotalValidObjNode"] = objNodeNumber;
    PTNumStatMap["NumOfCGEdge"] = consCG->getLoadCGEdges().size() + consCG->getStoreCGEdges().size()
                                  + numOfCopys + numOfGeps;
    PTNumStatMap["NumOfAddrs"] =  consCG->getAddrCGEdges().size();
    PTNumStatMap["NumOfCopys"] = numOfCopys;
    PTNumStatMap["NumOfGeps"] =  numOfGeps;
    PTNumStatMap["NumOfLoads"] = consCG->getLoadCGEdges().size();
    PTNumStatMap["NumOfStores"] = consCG->getStoreCGEdges().size();
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

    PTAStat::printStat("Constraint Graph Stats");
}
/*!
 * Stat null pointers
 */
void AndersenStat::statNullPtr()
{

    _NumOfNullPtr = 0;
    for (PAG::iterator iter = pta->getPAG()->begin(), eiter = pta->getPAG()->end();
            iter != eiter; ++iter)
    {
        NodeID pagNodeId = iter->first;
        PAGNode* pagNode = iter->second;
        if (pagNode->isTopLevelPtr() == false)
            continue;
        PAGEdge::PAGEdgeSetTy& inComingStore = pagNode->getIncomingEdges(PAGEdge::Store);
        PAGEdge::PAGEdgeSetTy& outGoingLoad = pagNode->getOutgoingEdges(PAGEdge::Load);
        if (inComingStore.empty()==false || outGoingLoad.empty()==false)
        {
            ///TODO: change the condition here to fetch the points-to set
            const PointsTo& pts = pta->getPts(pagNodeId);
            if (pta->containBlackHoleNode(pts))
                _NumOfBlackholePtr++;

            if (pta->containConstantNode(pts))
                _NumOfConstantPtr++;

            if(pts.empty())
            {
                std::string str;
                raw_string_ostream rawstr(str);
                if (!SVFUtil::isa<DummyValPN>(pagNode) && !SVFUtil::isa<DummyObjPN>(pagNode) )
                {
                    // if a pointer is in dead function, we do not care
                    if(isPtrInDeadFunction(pagNode->getValue()) == false)
                    {
                        _NumOfNullPtr++;
                        rawstr << "##Null Pointer : (NodeID " << pagNode->getId()
                               << ") PtrName:" << pagNode->getValue()->getName();
                        writeWrnMsg(rawstr.str());
                        //pagNode->getValue()->dump();
                    }
                }
                else
                {
                    _NumOfNullPtr++;
                    rawstr << "##Null Pointer : (NodeID " << pagNode->getId() << ")";
                    writeWrnMsg(rawstr.str());
                }
            }
        }
    }

}

/*!
 * Start here
 */
void AndersenStat::performStat()
{

    assert(SVFUtil::isa<AndersenBase>(pta) && "not an andersen pta pass!! what else??");
    endClk();

    PAG* pag = pta->getPAG();
    ConstraintGraph* consCG = pta->getConstraintGraph();

    // collect constraint graph cycles
    collectCycleInfo(consCG);

    // stat null ptr number
    statNullPtr();

    u32_t totalPointers = 0;
    u32_t totalTopLevPointers = 0;
    u32_t totalPtsSize = 0;
    u32_t totalTopLevPtsSize = 0;
    for (PAG::iterator iter = pta->getPAG()->begin(), eiter = pta->getPAG()->end();
            iter != eiter; ++iter)
    {
        NodeID node = iter->first;
        const PointsTo& pts = pta->getPts(node);
        u32_t size = pts.count();
        totalPointers++;
        totalPtsSize+=size;

        if(pta->getPAG()->isValidTopLevelPtr(pta->getPAG()->getPAGNode(node)))
        {
            totalTopLevPointers++;
            totalTopLevPtsSize+=size;
        }

        if(size > _MaxPtsSize )
            _MaxPtsSize = size;
    }


    PTAStat::performStat();

    constraintGraphStat();

    timeStatMap[TotalAnalysisTime] = (endTime - startTime)/TIMEINTERVAL;
    timeStatMap[SCCDetectionTime] = Andersen::timeOfSCCDetection;
    timeStatMap[SCCMergeTime] =  Andersen::timeOfSCCMerges;
    timeStatMap[CollapseTime] =  Andersen::timeOfCollapse;

    timeStatMap[ProcessLoadStoreTime] =  Andersen::timeOfProcessLoadStore;
    timeStatMap[ProcessCopyGepTime] =  Andersen::timeOfProcessCopyGep;
    timeStatMap[UpdateCallGraphTime] =  Andersen::timeOfUpdateCallGraph;

    PTNumStatMap[TotalNumOfPointers] = pag->getValueNodeNum() + pag->getFieldValNodeNum();
    PTNumStatMap[TotalNumOfObjects] = pag->getObjectNodeNum() + pag->getFieldObjNodeNum();


    PTNumStatMap[NumOfProcessedAddrs] = Andersen::numOfProcessedAddr;
    PTNumStatMap[NumOfProcessedCopys] = Andersen::numOfProcessedCopy;
    PTNumStatMap[NumOfProcessedGeps] = Andersen::numOfProcessedGep;
    PTNumStatMap[NumOfProcessedLoads] = Andersen::numOfProcessedLoad;
    PTNumStatMap[NumOfProcessedStores] = Andersen::numOfProcessedStore;

    PTNumStatMap[NumOfSfr] = Andersen::numOfSfrs;
    PTNumStatMap[NumOfFieldExpand] = Andersen::numOfFieldExpand;

    PTNumStatMap[NumOfPointers] = pag->getValueNodeNum();
    PTNumStatMap[NumOfMemObjects] = pag->getObjectNodeNum();
    PTNumStatMap[NumOfGepFieldPointers] = pag->getFieldValNodeNum();
    PTNumStatMap[NumOfGepFieldObjects] = pag->getFieldObjNodeNum();

    timeStatMap[AveragePointsToSetSize] = (double)totalPtsSize/totalPointers;;
    timeStatMap[AverageTopLevPointsToSetSize] = (double)totalTopLevPtsSize/totalTopLevPointers;;

    PTNumStatMap[MaxPointsToSetSize] = _MaxPtsSize;

    PTNumStatMap[NumOfIterations] = pta->numOfIteration;

    PTNumStatMap[NumOfIndirectCallSites] = consCG->getIndirectCallsites().size();
    PTNumStatMap[NumOfIndirectEdgeSolved] = pta->getNumOfResolvedIndCallEdge();

    PTNumStatMap[NumOfSCCDetection] = Andersen::numOfSCCDetection;
    PTNumStatMap[NumOfCycles] = _NumOfCycles;
    PTNumStatMap[NumOfPWCCycles] = _NumOfPWCCycles;
    PTNumStatMap[NumOfNodesInCycles] = _NumOfNodesInCycles;
    PTNumStatMap[MaxNumOfNodesInSCC] = _MaxNumOfNodesInSCC;
    PTNumStatMap[NumOfNullPointer] = _NumOfNullPtr;
    PTNumStatMap["PointsToConstPtr"] = _NumOfConstantPtr;
    PTNumStatMap["PointsToBlkPtr"] = _NumOfBlackholePtr;

    PTAStat::printStat("Andersen Pointer Analysis Stats");
}

