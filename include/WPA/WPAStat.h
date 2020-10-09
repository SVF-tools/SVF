//===- WPAStat.h -- WPA statistics--------------------------------------------//
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
 * FlowSensitiveStat.h
 *
 *  Created on: 27/11/2013
 *      Author: yesen
 */

#ifndef FLOWSENSITIVESTAT_H_
#define FLOWSENSITIVESTAT_H_

#include "MemoryModel/PTAStat.h"
#include "WPA/FlowSensitive.h"
#include "WPA/VersionedFlowSensitive.h"

namespace SVF
{

class AndersenBase;
class PAG;
class ConstraintGraph;
class PAGNode;

/*!
 * Statistics of Andersen's analysis
 */
class AndersenStat : public PTAStat
{

private:
    AndersenBase* pta;

public:
    static const char* CollapseTime;

    static u32_t _MaxPtsSize;
    static u32_t _NumOfCycles;
    static u32_t _NumOfPWCCycles;
    static u32_t _NumOfNodesInCycles;
    static u32_t _MaxNumOfNodesInSCC;
    u32_t _NumOfNullPtr;
    u32_t _NumOfConstantPtr;
    u32_t _NumOfBlackholePtr;

    AndersenStat(AndersenBase* p);

    virtual ~AndersenStat()
    {

    }

    virtual void performStat();

    void collectCycleInfo(ConstraintGraph* consCG);

    void statNullPtr();

    void constraintGraphStat();
};

/*!
 * Statistics of flow-sensitive analysis
 */
class FlowSensitiveStat : public PTAStat
{
public:
    typedef FlowSensitive::DFInOutMap DFInOutMap;
    typedef FlowSensitive::PtsMap PtsMap;

    FlowSensitive * fspta;

    FlowSensitiveStat(FlowSensitive* pta): PTAStat(pta)
    {
        fspta = pta;
        clearStat();
        startClk();
    }

    virtual ~FlowSensitiveStat() {}

    virtual void performStat();

private:
    enum ENUM_INOUT
    {
        IN,
        OUT
    };

    void clearStat();

    void statNullPtr();

    void statPtsSize();

    void statAddrVarPtsSize();

    void calculateAddrVarPts(NodeID pointer, const SVFGNode* node);

    void statInOutPtsSize(const DFInOutMap& data, ENUM_INOUT inOrOut);

    u32_t _NumOfNullPtr;
    u32_t _NumOfConstantPtr;
    u32_t _NumOfBlackholePtr;

    /// number of SVFG nodes which have IN/OUT set.
    u32_t _NumOfSVFGNodesHaveInOut[2];
    u32_t _NumOfFormalInSVFGNodesHaveInOut[2];
    u32_t _NumOfFormalOutSVFGNodesHaveInOut[2];
    u32_t _NumOfActualInSVFGNodesHaveInOut[2];
    u32_t _NumOfActualOutSVFGNodesHaveInOut[2];
    u32_t _NumOfLoadSVFGNodesHaveInOut[2];
    u32_t _NumOfStoreSVFGNodesHaveInOut[2];
    u32_t _NumOfMSSAPhiSVFGNodesHaveInOut[2];

    /// number of pag nodes which have points-to set in IN/OUT set.
    u32_t _NumOfVarHaveINOUTPts[2];
    u32_t _NumOfVarHaveEmptyINOUTPts[2];
    u32_t _NumOfVarHaveINOUTPtsInFormalIn[2];
    u32_t _NumOfVarHaveINOUTPtsInFormalOut[2];
    u32_t _NumOfVarHaveINOUTPtsInActualIn[2];
    u32_t _NumOfVarHaveINOUTPtsInActualOut[2];
    u32_t _NumOfVarHaveINOUTPtsInLoad[2];
    u32_t _NumOfVarHaveINOUTPtsInStore[2];
    u32_t _NumOfVarHaveINOUTPtsInMSSAPhi[2];
    u32_t _PotentialNumOfVarHaveINOUTPts[2];

    /// sizes of points-to set
    u32_t _MaxPtsSize;	///< max points-to set size.
    u32_t _MaxTopLvlPtsSize;	///< max points-to set size in top-level pointers.
    u32_t _MaxInOutPtsSize[2];	///< max points-to set size in IN/OUT set.

    u32_t _TotalPtsSize;	///< total points-to set size.

    double _AvgPtsSize;	///< average points-to set size.
    double _AvgTopLvlPtsSize;	///< average points-to set size in top-level pointers.
    double _AvgInOutPtsSize[2];	///< average points-to set size in IN set.
    double _AvgAddrTakenVarPtsSize;	///< average points-to set size of addr-taken variables.

    u32_t _MaxAddrTakenVarPts;	///< max points-to set size of addr-taken variables.
    u32_t _NumOfAddrTakeVar;	///< number of occurrences of addr-taken variables in load/store.
};

class VersionedFlowSensitiveStat : public PTAStat
{
public:
    VersionedFlowSensitive *vfspta;

    VersionedFlowSensitiveStat(VersionedFlowSensitive* pta): PTAStat(pta)
    {
        vfspta = pta;
        clearStat();
        startClk();
    }

    virtual ~VersionedFlowSensitiveStat() { }

    virtual void performStat();

private:
    void clearStat();

    /// For all version-related statistics.
    void versionStat(void);

    /// For all PTS size related statistics not handled by versionStat.
    void ptsSizeStat(void);

    /// Total number of versions across all objects.
    u32_t _NumVersions;
    /// Most versions for a single object.
    u32_t _MaxVersions;
    /// Number of version PTSs actually used (sum of next two fields).
    u32_t _NumUsedVersions;
    /// Number of versions with non-empty points-to sets (since versioning is over-approximate).
    u32_t _NumNonEmptyVersions;
    /// Number of versions with empty points-to sets (actually empty, not never-accessed).
    u32_t _NumEmptyVersions;
    /// Number of objects which have a single version.
    u32_t _NumSingleVersion;

    /// Largest PTS size.
    u32_t _MaxPtsSize;
    /// Max points-to set size in top-level pointers.
    u32_t _MaxTopLvlPtsSize;
    /// Max address-taken points-to set size.
    u32_t _MaxVersionPtsSize;

    /// Total of points-to set sizes for calculating averages.
    u32_t _TotalPtsSize;

    /// Average size across all points-to sets.
    double _AvgPtsSize;
    /// Average points-to set size for top-level pointers.
    double _AvgTopLvlPtsSize;
    /// Average points-to set size for address-taken objects.
    double _AvgVersionPtsSize;
};
} // End namespace SVF

#endif /* FLOWSENSITIVESTAT_H_ */
