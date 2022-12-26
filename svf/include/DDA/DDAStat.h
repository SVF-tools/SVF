//===- DDAStat.h -- Statistics for demand-driven pass-------------//
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
 * DDAStat.h
 *
 *  Created on: Sep 15, 2014
 *      Author: Yulei Sui
 */

#ifndef DDASTAT_H_
#define DDASTAT_H_

#include "Util/PTAStat.h"
#include "MemoryModel/PointsTo.h"

namespace SVF
{

class FlowDDA;
class ContextDDA;
class SVFG;
class PointerAnalysis;

/*!
 * Statistics of demand-driven analysis
 */
class DDAStat : public PTAStat
{

public:
    DDAStat(FlowDDA* pta);
    DDAStat(ContextDDA* pta);

    u32_t _NumOfDPM;
    u32_t _NumOfStrongUpdates;
    u32_t _NumOfMustAliases;
    u32_t _NumOfInfeasiblePath;

    u64_t _NumOfStep;
    u64_t _NumOfStepInCycle;
    double _AnaTimePerQuery;
    double _AnaTimeCyclePerQuery;
    double _TotalTimeOfQueries;
    double _TotalTimeOfBKCondition;

    NodeBS _StrongUpdateStores;

    void performStatPerQuery(NodeID ptr) override;

    void performStat() override;

    void printStat(std::string str = "") override;

    void printStatPerQuery(NodeID ptr, const PointsTo& pts) override;

    void getNumOfOOBQuery();

    inline void setMemUsageBefore(u32_t vmrss, u32_t vmsize)
    {
        _vmrssUsageBefore = vmrss;
        _vmsizeUsageBefore = vmsize;
    }

    inline void setMemUsageAfter(u32_t vmrss, u32_t vmsize)
    {
        _vmrssUsageAfter = vmrss;
        _vmsizeUsageAfter = vmsize;
    }

private:
    FlowDDA* flowDDA;
    ContextDDA* contextDDA;

    u32_t _TotalNumOfQuery;
    u32_t _TotalNumOfOutOfBudgetQuery;
    u32_t _TotalNumOfDPM;
    u32_t _TotalNumOfStrongUpdates;
    u32_t _TotalNumOfMustAliases;
    u32_t _TotalNumOfInfeasiblePath;

    u32_t _TotalNumOfStep;
    u32_t _TotalNumOfStepInCycle;

    u32_t _NumOfIndCallEdgeSolved;
    u32_t _MaxCPtsSize;
    u32_t _MaxPtsSize;
    u32_t _TotalCPtsSize;
    u32_t _TotalPtsSize;
    u32_t _NumOfNullPtr;
    u32_t _NumOfConstantPtr;
    u32_t _NumOfBlackholePtr;

    u32_t _vmrssUsageBefore;
    u32_t _vmrssUsageAfter;
    u32_t _vmsizeUsageBefore;
    u32_t _vmsizeUsageAfter;

    double _AvgNumOfDPMAtSVFGNode;
    u32_t _MaxNumOfDPMAtSVFGNode;

    NUMStatMap NumPerQueryStatMap;

    void initDefault();

public:
    SVFG* getSVFG() const;

    PointerAnalysis* getPTA() const;

    inline NodeBS& getStrongUpdateStores()
    {
        return _StrongUpdateStores;
    }
};

} // End namespace SVF

#endif /* DDASTAT_H_ */
