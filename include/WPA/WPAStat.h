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

#include "Util/PTAStat.h"
#include "WPA/FlowSensitive.h"

class Andersen;
class PAG;
class ConstraintGraph;
class PAGNode;

/*!
 * Statistics of Andersen's analysis
 */
class AndersenStat : public PTAStat {

private:
    Andersen* pta;

public:
    static const char* CollapseTime;

    static const char* NumberOfCGNode;

    static u32_t _MaxPtsSize;
    static u32_t _NumOfCycles;
    static u32_t _NumOfPWCCycles;
    static u32_t _NumOfNodesInCycles;
    static u32_t _MaxNumOfNodesInSCC;
    u32_t _NumOfNullPtr;
    u32_t _NumOfConstantPtr;
    u32_t _NumOfBlackholePtr;

    AndersenStat(Andersen* p);

    virtual ~AndersenStat() {

    }

    virtual void performStat();

    virtual void printStat();

    void collectCycleInfo(ConstraintGraph* consCG);

    void statNullPtr();
};

/*!
 * Statistics of flow-sensitive analysis
 */
class FlowSensitiveStat : public PTAStat {
public:
    typedef FlowSensitive::DFInOutMap DFInOutMap;
    typedef FlowSensitive::PtsMap PtsMap;

    FlowSensitive * fspta;

    FlowSensitiveStat(FlowSensitive* pta): PTAStat(pta) {
        fspta = pta;
        clearStat();
        startClk();
    }

    virtual ~FlowSensitiveStat() {}

    virtual void performStat();

    virtual void printStat();
private:
    enum ENUM_INOUT {
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
    u32_t _NumOfVarHaveINOUTPtsInFormalIn[2];
    u32_t _NumOfVarHaveINOUTPtsInFormalOut[2];
    u32_t _NumOfVarHaveINOUTPtsInActualIn[2];
    u32_t _NumOfVarHaveINOUTPtsInActualOut[2];
    u32_t _NumOfVarHaveINOUTPtsInLoad[2];
    u32_t _NumOfVarHaveINOUTPtsInStore[2];
    u32_t _NumOfVarHaveINOUTPtsInMSSAPhi[2];

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

#endif /* FLOWSENSITIVESTAT_H_ */
