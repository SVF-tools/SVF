//===- PTAStat.cpp -- Base class for statistics in SVF-----------------------//
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
 * PTAStat.cpp
 *
 *  Created on: Oct 13, 2013
 *      Author: Yulei Sui
 */

#include <iomanip>
#include "Graphs/PTACallGraph.h"
#include "MemoryModel/PTAStat.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/PAG.h"
#include "Util/Options.h"

using namespace SVF;

const char* PTAStat:: TotalAnalysisTime = "TotalTime";	///< PAG value nodes
const char* PTAStat:: SCCDetectionTime = "SCCDetectTime"; ///< Total SCC detection time
const char* PTAStat:: SCCMergeTime = "SCCMergeTime"; ///< Total SCC merge time

const char* PTAStat:: ProcessLoadStoreTime = "LoadStoreTime";	///< process load and store time
const char* PTAStat:: ProcessCopyGepTime = "CopyGepTime";	///< process copy and gep time
const char* PTAStat:: UpdateCallGraphTime = "UpdateCGTime";	///< process copy and gep time

const char* PTAStat:: TotalNumOfPointers = "TotalPointers";	///< PAG value nodes
const char* PTAStat:: TotalNumOfObjects = "TotalObjects";	///< Total PAG object node
const char* PTAStat:: TotalNumOfFieldObjects = "TotalFieldObjects";	///< Total PAG field object node
const char* PTAStat:: MaxStructSize = "MaxStructSize";	///< Max struct size (bytes)
const char* PTAStat:: TotalNumOfEdges = "TotalPAGEdges";	///< Total PAG edge number

const char* PTAStat:: NumOfFunctionObjs = "FunctionObjs";	///< function numbers
const char* PTAStat:: NumOfGlobalObjs = "GlobalObjs";	///< PAG global object node
const char* PTAStat:: NumOfHeapObjs  = "HeapObjs";	///< PAG heap object node
const char* PTAStat:: NumOfStackObjs = "StackObjs";	///< PAG stack object node

const char* PTAStat::NumberOfFieldInsensitiveObj = "FIObjNum";
const char* PTAStat::NumberOfFieldSensitiveObj = "FSObjNum";

const char* PTAStat:: NumOfObjsHasVarStruct = "VarStructObj";	///< PAG object node has struct (maybe nested with array)
const char* PTAStat:: NumOfObjsHasVarArray = "VarArrayObj";	///< PAG object node has array (maybe nested with struct)
const char* PTAStat:: NumOfObjsHasConstStruct = "ConstStructObj";	///< PAG object node has const struct (maybe nested with array)
const char* PTAStat:: NumOfObjsHasConstArray = "ConstArrayObj";	///< PAG object node has const array (maybe nested with struct)
const char* PTAStat:: NumOfNonPtrObjs = "NonPtrObj";	///< PAG object node which is non pointer type object (do not have pts)
const char* PTAStat:: NumOfConstantObjs = "ConstantObj";	///< PAG object node which is purely scalar

const char* PTAStat:: NumOfAddrs = "AddrsNum";		///< PAG addr edge
const char* PTAStat:: NumOfLoads = "LoadsNum";		///< PAG load edge
const char* PTAStat:: NumOfStores = "StoresNum";		///< PAG store edge
const char* PTAStat:: NumOfCopys = "CopysNum";		///< PAG copy edge
const char* PTAStat:: NumOfGeps = "GepsNum";		///< PAG gep edge
const char* PTAStat:: NumOfCalls = "CallsNum";		///< PAG call edge
const char* PTAStat:: NumOfReturns = "ReturnsNum";	///< PAG return edge

const char* PTAStat:: NumOfProcessedAddrs = "AddrProcessed";		///< PAG addr processed edge
const char* PTAStat:: NumOfProcessedLoads = "LoadProcessed";		///< PAG load processed edge
const char* PTAStat:: NumOfProcessedStores = "StoreProcessed";		///< PAG store processed edge
const char* PTAStat:: NumOfProcessedCopys = "CopyProcessed";		///< PAG copy processed edge
const char* PTAStat:: NumOfProcessedGeps = "GepProcessed";		///< PAG gep processed edge

const char* PTAStat::NumOfSfr = "NumOfSFRs";                    ///< number of field representatives
const char* PTAStat::NumOfFieldExpand = "NumOfFieldExpand";

const char* PTAStat:: NumOfPointers = "Pointers";	///< PAG value node, each of them maps to a llvm value
const char* PTAStat:: NumOfGepFieldPointers = "DYFieldPtrs";	///< PAG gep value node (field value, dynamically created dummy node)

const char* PTAStat:: NumOfMemObjects = "MemObjects";	///< PAG object node, each of them maps to a llvm value
const char* PTAStat:: NumOfGepFieldObjects = "DYFieldObjs";	///< PAG gep object node (field obj, dynamically created dummy node)

const char* PTAStat:: AveragePointsToSetSize = "AvgPtsSetSize";		///< Average Points-to set size
const char* PTAStat:: AverageTopLevPointsToSetSize = "AvgTopLvlPtsSize";		///< Average Points-to set size of top level pointers
const char* PTAStat:: MaxPointsToSetSize = "MaxPtsSetSize";		///< Max Points-to set size

const char* PTAStat:: NumOfIterations = "Iterations";	///< Number of iterations during resolution

const char* PTAStat:: NumOfIndirectCallSites = "IndCallSites";	///< Number of indirect callsites
const char* PTAStat:: NumOfIndirectEdgeSolved = "IndEdgeSolved";	///< Number of indirect calledge resolved

const char* PTAStat:: NumOfSCCDetection = "NumOfSCCDetect";   ///< Number of scc detection performed
const char* PTAStat:: NumOfCycles = "TotalCycleNum";   ///< Number of scc cycles detected
const char* PTAStat:: NumOfPWCCycles = "TotalPWCCycleNum";   ///< Number of pwc scc cycles detected
const char* PTAStat:: NumOfNodesInCycles = "NodesInCycles"; ///< Number of nodes in cycles detected
const char* PTAStat:: MaxNumOfNodesInSCC = "MaxNodesInSCC";	///< max Number of nodes in one scc cycle

const char* PTAStat:: NumOfNullPointer = "NullPointer";	///< Number of pointers points-to null

PTAStat::PTAStat(PointerAnalysis* p) : startTime(0), endTime(0), pta(p)
{

}

double PTAStat::getClk(bool mark) {
    if (Options::MarkedClocksOnly && !mark) return 0.0;
    return CLOCK_IN_MS();
}

void PTAStat::performStat()
{

    callgraphStat();

    PAG* pag = PAG::getPAG();
    u32_t numOfFunction = 0;
    u32_t numOfGlobal = 0;
    u32_t numOfStack = 0;
    u32_t numOfHeap = 0;
    u32_t numOfHasVarArray = 0;
    u32_t numOfHasVarStruct = 0;
    u32_t numOfHasConstArray = 0;
    u32_t numOfHasConstStruct = 0;
    u32_t numOfScalar = 0;
    u32_t numOfConstant = 0;
    u32_t fiObjNumber = 0;
    u32_t fsObjNumber = 0;
    Set<SymID> memObjSet;
    for(PAG::iterator it = pag->begin(), eit = pag->end(); it!=eit; ++it)
    {
        PAGNode* node = it->second;
        if(ObjPN* obj = SVFUtil::dyn_cast<ObjPN>(node))
        {
            const MemObj* mem = obj->getMemObj();
            if (memObjSet.insert(mem->getSymId()).second == false)
                continue;
            if(mem->isBlackHoleObj())
                continue;
            if(mem->isFunction())
                numOfFunction++;
            if(mem->isGlobalObj())
                numOfGlobal++;
            if(mem->isStack())
                numOfStack++;
            if(mem->isHeap())
                numOfHeap++;
            if(mem->isVarArray())
                numOfHasVarArray++;
            if(mem->isVarStruct())
                numOfHasVarStruct++;
            if(mem->isConstArray())
                numOfHasConstArray++;
            if(mem->isConstStruct())
                numOfHasConstStruct++;
            if(mem->hasPtrObj() == false)
                numOfScalar++;
            if(mem->isConstant())
                numOfConstant++;

            if (mem->isFieldInsensitive())
                fiObjNumber++;
            else
                fsObjNumber++;

            if(pta->isLocalVarInRecursiveFun(node->getId()))
            {
                localVarInRecursion.set(node->getId());
            }
        }
    }



    generalNumMap[TotalNumOfPointers] = pag->getValueNodeNum() + pag->getFieldValNodeNum();
    generalNumMap[TotalNumOfObjects] = pag->getObjectNodeNum();
    generalNumMap[TotalNumOfFieldObjects] = pag->getFieldObjNodeNum();
    generalNumMap[MaxStructSize] = SymbolTableInfo::SymbolInfo()->getMaxStructSize();
    generalNumMap[TotalNumOfEdges] = pag->getPAGEdgeNum();
    generalNumMap["TotalPTAPAGEdges"] = pag->totalPTAPAGEdge;
    generalNumMap[NumberOfFieldInsensitiveObj] = fiObjNumber;
    generalNumMap[NumberOfFieldSensitiveObj] = fsObjNumber;

    generalNumMap[NumOfAddrs] = pag->getEdgeSet(PAGEdge::Addr).size();
    generalNumMap[NumOfLoads] = pag->getEdgeSet(PAGEdge::Load).size();
    generalNumMap[NumOfStores] = pag->getEdgeSet(PAGEdge::Store).size();
    generalNumMap[NumOfCopys] =  pag->getEdgeSet(PAGEdge::Copy).size();
    generalNumMap[NumOfGeps] =  pag->getEdgeSet(PAGEdge::NormalGep).size() + pag->getEdgeSet(PAGEdge::VariantGep).size();
    generalNumMap[NumOfCalls] = pag->getEdgeSet(PAGEdge::Call).size();
    generalNumMap[NumOfReturns] = pag->getEdgeSet(PAGEdge::Ret).size();

    generalNumMap[NumOfFunctionObjs] = numOfFunction;
    generalNumMap[NumOfGlobalObjs] = numOfGlobal;
    generalNumMap[NumOfHeapObjs]  = numOfHeap;
    generalNumMap[NumOfStackObjs] = numOfStack;

    generalNumMap[NumOfObjsHasVarStruct] = numOfHasVarStruct;
    generalNumMap[NumOfObjsHasVarArray] = numOfHasVarArray;
    generalNumMap[NumOfObjsHasConstStruct] = numOfHasConstStruct;
    generalNumMap[NumOfObjsHasConstArray] = numOfHasConstArray;
    generalNumMap[NumOfNonPtrObjs] = numOfScalar;

    generalNumMap[NumOfIndirectCallSites] = pag->getIndirectCallsites().size();
    generalNumMap["TotalCallSite"] = pag->getCallSiteSet().size();
    generalNumMap["LocalVarInRecur"] = localVarInRecursion.count();
    bitcastInstStat();
    branchStat();

    printStat("General Stats");

}

void PTAStat::callgraphStat()
{

    PTACallGraph* graph = pta->getPTACallGraph();
    PointerAnalysis::CallGraphSCC* callgraphSCC = new PointerAnalysis::CallGraphSCC(graph);
    callgraphSCC->find();

    unsigned totalNode = 0;
    unsigned totalCycle = 0;
    unsigned nodeInCycle = 0;
    unsigned maxNodeInCycle = 0;
    unsigned totalEdge = 0;
    unsigned edgeInCycle = 0;

    NodeSet sccRepNodeSet;
    PTACallGraph::iterator it = graph->begin();
    PTACallGraph::iterator eit = graph->end();
    for (; it != eit; ++it)
    {
        totalNode++;
        if(callgraphSCC->isInCycle(it->first))
        {
            sccRepNodeSet.insert(callgraphSCC->repNode(it->first));
            nodeInCycle++;
            const NodeBS& subNodes = callgraphSCC->subNodes(it->first);
            if(subNodes.count() > maxNodeInCycle)
                maxNodeInCycle = subNodes.count();
        }

        PTACallGraphNode::const_iterator edgeIt = it->second->InEdgeBegin();
        PTACallGraphNode::const_iterator edgeEit = it->second->InEdgeEnd();
        for (; edgeIt != edgeEit; ++edgeIt)
        {
            PTACallGraphEdge *edge = *edgeIt;
            totalEdge+= edge->getDirectCalls().size() + edge->getIndirectCalls().size();
            if(callgraphSCC->repNode(edge->getSrcID()) == callgraphSCC->repNode(edge->getDstID()))
            {
                edgeInCycle+=edge->getDirectCalls().size() + edge->getIndirectCalls().size();
            }
        }
    }

    totalCycle = sccRepNodeSet.size();

    PTNumStatMap["TotalNode"] = totalNode;
    PTNumStatMap["TotalCycle"] = totalCycle;
    PTNumStatMap["NodeInCycle"] = nodeInCycle;
    PTNumStatMap["MaxNodeInCycle"] = maxNodeInCycle;
    PTNumStatMap["TotalEdge"] = totalEdge;
    PTNumStatMap["CalRetPairInCycle"] = edgeInCycle;

    PTAStat::printStat("CallGraph Stats");

    delete callgraphSCC;
}

void PTAStat::printStat(string statname)
{

    StringRef fullName(SymbolTableInfo::SymbolInfo()->getModule()->getModuleIdentifier());
    StringRef name = fullName.split('/').second;
    moduleName = name.split('.').first.str();

    std::cout << "\n*********" << statname << "***************\n";
    std::cout << "################ (program : " << moduleName << ")###############\n";
    std::cout.flags(std::ios::left);
    unsigned field_width = 20;
    for(NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    std::cout << "-------------------------------------------------------\n";
    for(TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    for(NUMStatMap::iterator it = PTNumStatMap.begin(), eit = PTNumStatMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }

    std::cout << "#######################################################" << std::endl;
    std::cout.flush();
    generalNumMap.clear();
    PTNumStatMap.clear();
    timeStatMap.clear();
}


void PTAStat::bitcastInstStat()
{
    SVFModule* module = pta->getModule();
    u32_t numberOfBitCast = 0;
    for (SVFModule::llvm_const_iterator funIter = module->llvmFunBegin(), funEiter = module->llvmFunEnd();
            funIter != funEiter; ++funIter)
    {
        const Function* func = *funIter;
        for (Function::const_iterator bbIt = func->begin(), bbEit = func->end();
                bbIt != bbEit; ++bbIt)
        {
            const BasicBlock& bb = *bbIt;
            for (BasicBlock::const_iterator instIt = bb.begin(), instEit = bb.end();
                    instIt != instEit; ++instIt)
            {
                const Instruction& inst = *instIt;
                if (const BitCastInst* bitcast = SVFUtil::dyn_cast<BitCastInst>(&inst))
                {
                    if (SVFUtil::isa<PointerType>(bitcast->getSrcTy()))
                        numberOfBitCast++;
                }
            }
        }
    }

    generalNumMap["BitCastNumber"] = numberOfBitCast;
}

void PTAStat::branchStat()
{
    SVFModule* module = pta->getModule();
    u32_t numOfBB_2Succ = 0;
    u32_t numOfBB_3Succ = 0;
    for (SVFModule::llvm_const_iterator funIter = module->llvmFunBegin(), funEiter = module->llvmFunEnd();
            funIter != funEiter; ++funIter)
    {
        const Function* func = *funIter;
        for (Function::const_iterator bbIt = func->begin(), bbEit = func->end();
                bbIt != bbEit; ++bbIt)
        {
            const BasicBlock& bb = *bbIt;
            u32_t numOfSucc = bb.getTerminator()->getNumSuccessors();
            if (numOfSucc == 2)
                numOfBB_2Succ++;
            else if (numOfSucc > 2)
                numOfBB_3Succ++;
        }
    }

    generalNumMap["BBWith2Succ"] = numOfBB_2Succ;
    generalNumMap["BBWith3Succ"] = numOfBB_3Succ;
}
