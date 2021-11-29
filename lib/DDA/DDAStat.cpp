/*
 * DDAStat.cpp
 *
 *  Created on: Sep 15, 2014
 *      Author: Yulei Sui
 */

#include "DDA/DDAStat.h"
#include "DDA/FlowDDA.h"
#include "DDA/ContextDDA.h"
#include "Graphs/SVFGStat.h"
#include "MemoryModel/PointsTo.h"

#include <iomanip>

using namespace SVF;
using namespace SVFUtil;

DDAStat::DDAStat(FlowDDA* pta) : PTAStat(pta), flowDDA(pta), contextDDA(nullptr)
{
    initDefault();
}
DDAStat::DDAStat(ContextDDA* pta) : PTAStat(pta), flowDDA(nullptr), contextDDA(pta)
{
    initDefault();
}

void DDAStat::initDefault()
{
    _TotalNumOfQuery = 0;
    _TotalNumOfOutOfBudgetQuery = 0;
    _TotalNumOfDPM = 0;
    _TotalNumOfStrongUpdates = 0;
    _TotalNumOfMustAliases = 0;
    _TotalNumOfInfeasiblePath = 0;
    _TotalNumOfStep = 0;
    _TotalNumOfStepInCycle = 0;

    _NumOfIndCallEdgeSolved = 0;
    _MaxCPtsSize = _MaxPtsSize = 0;
    _TotalCPtsSize = _TotalPtsSize = 0;
    _NumOfNullPtr = 0;
    _NumOfConstantPtr = 0;
    _NumOfBlackholePtr = 0;
    _AvgNumOfDPMAtSVFGNode = 0;
    _MaxNumOfDPMAtSVFGNode = 0;
    _TotalTimeOfQueries = 0;
    _AnaTimePerQuery = 0;
    _AnaTimeCyclePerQuery = 0;


    _NumOfDPM = 0;
    _NumOfStrongUpdates = 0;
    _NumOfMustAliases = 0;
    _NumOfInfeasiblePath = 0;

    _NumOfStep = 0;
    _NumOfStepInCycle = 0;
    _AnaTimePerQuery = 0;
    _AnaTimeCyclePerQuery = 0;
    _TotalTimeOfQueries = 0;

    _vmrssUsageBefore = _vmrssUsageAfter = 0;
    _vmsizeUsageBefore = _vmsizeUsageAfter = 0;
}

SVFG* DDAStat::getSVFG() const
{
    if(flowDDA)
        return flowDDA->getSVFG();
    else
        return contextDDA->getSVFG();

}

PointerAnalysis* DDAStat::getPTA() const
{
    if(flowDDA)
        return flowDDA;
    else
        return contextDDA;
}

void DDAStat::performStatPerQuery(NodeID ptr)
{

    u32_t NumOfDPM = 0;
    u32_t NumOfLoc = 0;
    u32_t maxNumOfDPMPerLoc = 0;
    u32_t cptsSize = 0;
    PointsTo pts;
    if(flowDDA)
    {
        for(FlowDDA::LocToDPMVecMap::const_iterator it =  flowDDA->getLocToDPMVecMap().begin(),
                eit = flowDDA->getLocToDPMVecMap().end(); it!=eit; ++it)
        {
            NumOfLoc++;
            u32_t num = it->second.size();
            NumOfDPM += num;
            if(num > maxNumOfDPMPerLoc)
                maxNumOfDPMPerLoc = num;
        }
        cptsSize = flowDDA->getPts(ptr).count();
        pts = flowDDA->getPts(ptr);
    }
    else if(contextDDA)
    {
        for(ContextDDA::LocToDPMVecMap::const_iterator it =  contextDDA->getLocToDPMVecMap().begin(),
                eit = contextDDA->getLocToDPMVecMap().end(); it!=eit; ++it)
        {
            NumOfLoc++;
            u32_t num = it->second.size();
            NumOfDPM += num;
            if(num > maxNumOfDPMPerLoc)
                maxNumOfDPMPerLoc = num;
        }
        ContextCond cxt;
        CxtVar var(cxt,ptr);
        cptsSize = contextDDA->getPts(var).count();
        pts = contextDDA->getBVPointsTo(contextDDA->getPts(var));
    }
    u32_t ptsSize = pts.count();

    double avgDPMAtLoc = NumOfLoc!=0 ? (double)NumOfDPM/NumOfLoc : 0;
    _AvgNumOfDPMAtSVFGNode += avgDPMAtLoc;
    if(maxNumOfDPMPerLoc > _MaxNumOfDPMAtSVFGNode)
        _MaxNumOfDPMAtSVFGNode = maxNumOfDPMPerLoc;

    _TotalCPtsSize += cptsSize;
    if (_MaxCPtsSize < cptsSize)
        _MaxCPtsSize = cptsSize;

    _TotalPtsSize += ptsSize;
    if(_MaxPtsSize < ptsSize)
        _MaxPtsSize = ptsSize;

    if(cptsSize == 0)
        _NumOfNullPtr++;

    if(getPTA()->containBlackHoleNode(pts))
    {
        _NumOfConstantPtr++;
    }
    if(getPTA()->containConstantNode(pts))
    {
        _NumOfBlackholePtr++;
    }

    _TotalNumOfQuery++;
    _TotalNumOfDPM += _NumOfDPM;
    _TotalNumOfStrongUpdates += _NumOfStrongUpdates;
    _TotalNumOfMustAliases += _NumOfMustAliases;
    _TotalNumOfInfeasiblePath += _NumOfInfeasiblePath;

    _TotalNumOfStep += _NumOfStep;
    _TotalNumOfStepInCycle += _NumOfStepInCycle;

    timeStatMap.clear();
    NumPerQueryStatMap.clear();

    timeStatMap["TimePerQuery"] = _AnaTimePerQuery/TIMEINTERVAL;
    timeStatMap["CyleTimePerQuery"] = _AnaTimeCyclePerQuery/TIMEINTERVAL;

    NumPerQueryStatMap["CPtsSize"] = cptsSize;
    NumPerQueryStatMap["PtsSize"] = ptsSize;
    NumPerQueryStatMap["NumOfStep"] = _NumOfStep;
    NumPerQueryStatMap["NumOfStepInCycle"] = _NumOfStepInCycle;
    NumPerQueryStatMap["NumOfDPM"] = _NumOfDPM;
    NumPerQueryStatMap["NumOfSU"] = _NumOfStrongUpdates;
    NumPerQueryStatMap["IndEdgeResolved"] = getPTA()->getNumOfResolvedIndCallEdge() - _NumOfIndCallEdgeSolved;
    NumPerQueryStatMap["AvgDPMAtLoc"] = avgDPMAtLoc;
    NumPerQueryStatMap["MaxDPMAtLoc"] = maxNumOfDPMPerLoc;
    NumPerQueryStatMap["MaxPathPerQuery"] = ContextCond::maximumPath;
    NumPerQueryStatMap["MaxCxtPerQuery"] = ContextCond::maximumCxt;
    NumPerQueryStatMap["NumOfMustAA"] = _NumOfMustAliases;
    NumPerQueryStatMap["NumOfInfePath"] = _NumOfInfeasiblePath;

    /// reset numbers for next query
    _NumOfStep = 0;
    _NumOfStepInCycle = 0;
    _NumOfDPM = 0;
    _NumOfStrongUpdates = 0;
    _NumOfMustAliases = 0;
    _NumOfInfeasiblePath = 0;
    _AnaTimeCyclePerQuery = 0;
    _NumOfIndCallEdgeSolved = getPTA()->getNumOfResolvedIndCallEdge();
}

void DDAStat::getNumOfOOBQuery()
{
    if (flowDDA)
        _TotalNumOfOutOfBudgetQuery = flowDDA->outOfBudgetDpms.size();
    else if (contextDDA)
        _TotalNumOfOutOfBudgetQuery = contextDDA->outOfBudgetDpms.size();
}

void DDAStat::performStat()
{

    generalNumMap.clear();
    PTNumStatMap.clear();
    timeStatMap.clear();

    callgraphStat();

    getNumOfOOBQuery();

    for (PAG::const_iterator nodeIt = PAG::getPAG()->begin(), nodeEit = PAG::getPAG()->end(); nodeIt != nodeEit; nodeIt++)
    {
        PAGNode* pagNode = nodeIt->second;
        if(SVFUtil::isa<ObjPN>(pagNode))
        {
            if(getPTA()->isLocalVarInRecursiveFun(nodeIt->first))
            {
                localVarInRecursion.set(nodeIt->first);
            }
        }
    }

    timeStatMap["TotalQueryTime"] =  _TotalTimeOfQueries/TIMEINTERVAL;
    timeStatMap["AvgTimePerQuery"] =  (_TotalTimeOfQueries/TIMEINTERVAL)/_TotalNumOfQuery;
    timeStatMap["TotalBKCondTime"] =  (_TotalTimeOfBKCondition/TIMEINTERVAL);

    PTNumStatMap["NumOfQuery"] = _TotalNumOfQuery;
    PTNumStatMap["NumOfOOBQuery"] = _TotalNumOfOutOfBudgetQuery;
    PTNumStatMap["NumOfDPM"] = _TotalNumOfDPM;
    PTNumStatMap["NumOfSU"] = _TotalNumOfStrongUpdates;
    PTNumStatMap["NumOfStoreSU"] = _StrongUpdateStores.count();
    PTNumStatMap["NumOfStep"] =  _TotalNumOfStep;
    PTNumStatMap["NumOfStepInCycle"] =  _TotalNumOfStepInCycle;
    timeStatMap["AvgDPMAtLoc"] = (double)_AvgNumOfDPMAtSVFGNode/_TotalNumOfQuery;
    PTNumStatMap["MaxDPMAtLoc"] = _MaxNumOfDPMAtSVFGNode;
    PTNumStatMap["MaxPathPerQuery"] = ContextCond::maximumPath;
    PTNumStatMap["MaxCxtPerQuery"] = ContextCond::maximumCxt;
    PTNumStatMap["MaxCPtsSize"] = _MaxCPtsSize;
    PTNumStatMap["MaxPtsSize"] = _MaxPtsSize;
    timeStatMap["AvgCPtsSize"] = (double)_TotalCPtsSize/_TotalNumOfQuery;
    timeStatMap["AvgPtsSize"] = (double)_TotalPtsSize/_TotalNumOfQuery;
    PTNumStatMap["IndEdgeSolved"] = getPTA()->getNumOfResolvedIndCallEdge();
    PTNumStatMap["NumOfNullPtr"] = _NumOfNullPtr;
    PTNumStatMap["PointsToConstPtr"] = _NumOfConstantPtr;
    PTNumStatMap["PointsToBlkPtr"] = _NumOfBlackholePtr;
    PTNumStatMap["NumOfMustAA"] = _TotalNumOfMustAliases;
    PTNumStatMap["NumOfInfePath"] = _TotalNumOfInfeasiblePath;
    PTNumStatMap["NumOfStore"] = PAG::getPAG()->getPTAEdgeSet(PAGEdge::Store).size();
    PTNumStatMap["MemoryUsageVmrss"] = _vmrssUsageAfter - _vmrssUsageBefore;
    PTNumStatMap["MemoryUsageVmsize"] = _vmsizeUsageAfter - _vmsizeUsageBefore;

    printStat();
}

void DDAStat::printStatPerQuery(NodeID ptr, const PointsTo& pts)
{

    if (timeStatMap.empty() == false && NumPerQueryStatMap.empty() == false)
    {
        std::cout.flags(std::ios::left);
        unsigned field_width = 20;
        std::cout << "---------------------Stat Per Query--------------------------------\n";
        for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it)
        {
            // format out put with width 20 space
            std::cout << std::setw(field_width) << it->first << it->second << "\n";
        }
        for (NUMStatMap::iterator it = NumPerQueryStatMap.begin(), eit = NumPerQueryStatMap.end(); it != eit; ++it)
        {
            // format out put with width 20 space
            std::cout << std::setw(field_width) << it->first << it->second << "\n";
        }
    }
    getPTA()->dumpPts(ptr, pts);
}


void DDAStat::printStat()
{

    if(flowDDA)
    {
        FlowDDA::ConstSVFGEdgeSet edgeSet;
        flowDDA->getSVFG()->getStat()->performSCCStat(edgeSet);
    }
    else if(contextDDA)
    {
        contextDDA->getSVFG()->getStat()->performSCCStat(contextDDA->getInsensitiveEdgeSet());
    }

    std::cout << "\n****Demand-Driven Pointer Analysis Statistics****\n";
    PTAStat::printStat();
}
