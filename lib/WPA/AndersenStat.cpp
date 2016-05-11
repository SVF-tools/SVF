//===- AndersenStat.cpp -- Statistics of Andersen's analysis------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
#include "WPA/WPAStat.h"
#include "WPA/Andersen.h"

using namespace llvm;
using namespace analysisUtil;

u32_t AndersenStat::_MaxPtsSize = 0;
u32_t AndersenStat::_NumOfCycles = 0;
u32_t AndersenStat::_NumOfPWCCycles = 0;
u32_t AndersenStat::_NumOfNodesInCycles = 0;
u32_t AndersenStat::_MaxNumOfNodesInSCC = 0;

const char* AndersenStat::CollapseTime = "CollapseTime";
const char* AndersenStat::NumberOfCGNode = "CGNodeNum";

/*!
 * Constructor
 */
AndersenStat::AndersenStat(Andersen* p): PTAStat(p),pta(p) {
    _NumOfNullPtr = 0;
    _NumOfConstantPtr= 0;
    _NumOfBlackholePtr = 0;
    startClk();
}

/*!
 * Collect cycle information
 */
void AndersenStat::collectCycleInfo(ConstraintGraph* consCG) {
    _NumOfCycles = 0;
    _NumOfPWCCycles = 0;
    _NumOfNodesInCycles = 0;
    std::set<NodeID> repNodes;
    repNodes.clear();
    for(ConstraintGraph::iterator it = consCG->begin(), eit = consCG->end(); it!=eit; ++it) {
        // sub nodes have been removed from the constraint graph, only rep nodes are left.
        NodeID repNode = consCG->sccRepNode(it->first);
        NodeBS& subNodes = pta->sccSubNodes(repNode);
        NodeBS clone = subNodes;
        for (NodeBS::iterator it = subNodes.begin(), eit = subNodes.end(); it != eit; ++it) {
            NodeID nodeId = *it;
            PAGNode* pagNode = pta->getPAG()->getPAGNode(nodeId);
            if (isa<ObjPN>(pagNode) && consCG->isFieldInsensitiveObj(nodeId)) {
                NodeID baseId = consCG->getBaseObjNode(nodeId);
                clone.reset(nodeId);
                clone.set(baseId);
            }
        }
        u32_t num = clone.count();
        if (num > 1) {
            if(repNodes.insert(repNode).second) {
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

/*!
 * Stat null pointers
 */
void AndersenStat::statNullPtr() {

    _NumOfNullPtr = 0;
    for (PAG::iterator iter = pta->getPAG()->begin(), eiter = pta->getPAG()->end();
            iter != eiter; ++iter) {
        NodeID pagNodeId = iter->first;
        PAGNode* pagNode = iter->second;
        PAGEdge::PAGEdgeSetTy& inComingStore = pagNode->getIncomingEdges(PAGEdge::Store);
        PAGEdge::PAGEdgeSetTy& outGoingLoad = pagNode->getOutgoingEdges(PAGEdge::Load);
        if (inComingStore.empty()==false || outGoingLoad.empty()==false) {
            ///TODO: change the condition here to fetch the points-to set
            PointsTo& pts = pta->getPts(pagNodeId);
            if(pta->containBlackHoleNode(pts)) {
                _NumOfConstantPtr++;
            }
            if(pta->containConstantNode(pts)) {
                _NumOfBlackholePtr++;
            }
            if(pts.empty()) {
                std::string str;
                raw_string_ostream rawstr(str);
                if (!isa<DummyValPN>(pagNode) && !isa<DummyObjPN>(pagNode) ) {
                    // if a pointer is in dead function, we do not care
                    if(isPtrInDeadFunction(pagNode->getValue()) == false) {
                        _NumOfNullPtr++;
                        rawstr << "##Null Pointer : (NodeID " << pagNode->getId()
                               << ") PtrName:" << pagNode->getValue()->getName();
                        wrnMsg(rawstr.str());
                        //pagNode->getValue()->dump();
                    }
                }
                else {
                    _NumOfNullPtr++;
                    rawstr << "##Null Pointer : (NodeID " << pagNode->getId() << ")";
                    wrnMsg(rawstr.str());
                }
            }
        }
    }

}

/*!
 * Start here
 */
void AndersenStat::performStat() {

    assert(isa<Andersen>(pta) && "not an andersen pta pass!! what else??");
    endClk();

    PAG* pag = pta->getPAG();
    ConstraintGraph* consCG = pta->getConstraintGraph();

    // collect constraint graph cycles
    collectCycleInfo(consCG);

    // stat null ptr number
    statNullPtr();

    u32_t numOfCopys = 0;
    u32_t numOfGeps = 0;
    // collect copy and gep edges
    for(ConstraintEdge::ConstraintEdgeSetTy::iterator it = consCG->getDirectCGEdges().begin(),
            eit = consCG->getDirectCGEdges().end(); it!=eit; ++it) {
        if(isa<CopyCGEdge>(*it))
            numOfCopys++;
        else if(isa<GepCGEdge>(*it))
            numOfGeps++;
        else
            assert(false && "what else!!");
    }

    u32_t cgNodeNumber = 0;
    for (ConstraintGraph::ConstraintNodeIDToNodeMapTy::iterator nodeIt = consCG->begin(), nodeEit = consCG->end();
            nodeIt != nodeEit; nodeIt++) {
        cgNodeNumber++;
    }

    u32_t totalPointers = 0;
    u32_t totalTopLevPointers = 0;
    u32_t totalPtsSize = 0;
    u32_t totalTopLevPtsSize = 0;
    for (PAG::iterator iter = pta->getPAG()->begin(), eiter = pta->getPAG()->end();
            iter != eiter; ++iter) {
        NodeID node = iter->first;
        PointsTo& pts = pta->getPts(node);
        u32_t size = pts.count();
        totalPointers++;
        totalPtsSize+=size;

        if(pta->getPAG()->isValidTopLevelPtr(pta->getPAG()->getPAGNode(node))) {
            totalTopLevPointers++;
            totalTopLevPtsSize+=size;
        }

        if(size > _MaxPtsSize )
            _MaxPtsSize = size;
    }


    PTAStat::performStat();

    timeStatMap[TotalAnalysisTime] = (endTime - startTime)/TIMEINTERVAL;
    timeStatMap[SCCDetectionTime] = Andersen::timeOfSCCDetection;
    timeStatMap[SCCMergeTime] =  Andersen::timeOfSCCMerges;
    timeStatMap[CollapseTime] =  Andersen::timeOfCollapse;

    timeStatMap[ProcessLoadStoreTime] =  Andersen::timeOfProcessLoadStore;
    timeStatMap[ProcessCopyGepTime] =  Andersen::timeOfProcessCopyGep;
    timeStatMap[UpdateCallGraphTime] =  Andersen::timeOfUpdateCallGraph;

    PTNumStatMap[TotalNumOfPointers] = pag->getValueNodeNum() + pag->getFieldValNodeNum();
    PTNumStatMap[TotalNumOfObjects] = pag->getObjectNodeNum() + pag->getFieldObjNodeNum();
    PTNumStatMap[TotalNumOfEdges] = consCG->getLoadCGEdges().size() + consCG->getStoreCGEdges().size()
                                    + numOfCopys + numOfGeps;

    PTNumStatMap[NumOfAddrs] =  consCG->getAddrCGEdges().size();
    PTNumStatMap[NumOfCopys] = numOfCopys;
    PTNumStatMap[NumOfGeps] =  numOfGeps;
    PTNumStatMap[NumOfLoads] = consCG->getLoadCGEdges().size();
    PTNumStatMap[NumOfStores] = consCG->getStoreCGEdges().size();
    PTNumStatMap["TotalLoadInst"] = pag->loadInstNum;
    PTNumStatMap["TotalStoreInst"] = pag->storeInstNum;

    PTNumStatMap[NumOfProcessedAddrs] = Andersen::numOfProcessedAddr;
    PTNumStatMap[NumOfProcessedCopys] = Andersen::numOfProcessedCopy;
    PTNumStatMap[NumOfProcessedGeps] = Andersen::numOfProcessedGep;
    PTNumStatMap[NumOfProcessedLoads] = Andersen::numOfProcessedLoad;
    PTNumStatMap[NumOfProcessedStores] = Andersen::numOfProcessedStore;

    PTNumStatMap[NumOfPointers] = pag->getValueNodeNum();
    PTNumStatMap[NumOfMemObjects] = pag->getObjectNodeNum();
    PTNumStatMap[NumOfGepFieldPointers] = pag->getFieldValNodeNum();
    PTNumStatMap[NumOfGepFieldObjects] = pag->getFieldObjNodeNum();

    PTNumStatMap[NumberOfCGNode] = cgNodeNumber;

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

    printStat();

}

/*!
 * Print all statistics
 */
void AndersenStat::printStat() {

    std::cout << "\n****Andersen Pointer Analysis Statistics****\n";
    PTAStat::printStat();
}
