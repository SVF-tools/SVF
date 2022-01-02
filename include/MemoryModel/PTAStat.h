//===- PTAStat.h -- Base class for statistics---------------------------------//
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
 * AndersenStat.h
 *
 *  Created on: Oct 12, 2013
 *      Author: Yulei Sui
 */

#ifndef ANDERSENSTAT_H_
#define ANDERSENSTAT_H_

#include "Util/BasicTypes.h"
#include "MemoryModel/PointsTo.h"
#include <iostream>
#include <map>
#include <string>

using namespace std;

namespace SVF
{

class PointerAnalysis;

/*!
 * Pointer Analysis Statistics
 */
class PTAStat
{
public:
    static const char* TotalAnalysisTime; ///< Total analysis time
    static const char* SCCDetectionTime; ///< Total SCC detection time
    static const char* SCCMergeTime; ///< Total SCC merge time

    static const char* ProcessLoadStoreTime;///< time of processing loads and stores
    static const char* ProcessCopyGepTime;	///< time of processing copys and geps
    static const char* UpdateCallGraphTime;	///< time of updating call graph

    static const char* TotalNumOfPointers;	///< Total SVFIR value node
    static const char* TotalNumOfObjects;	///< Total SVFIR object node
    static const char* TotalNumOfFieldObjects;	///< Total SVFIR field object node
    static const char* MaxStructSize;	///< Max struct size
    static const char* TotalNumOfEdges;	///< Total SVFIR edge number

    static const char* NumOfAddrs;		///< SVFIR addr edge
    static const char* NumOfLoads;		///< SVFIR load edge
    static const char* NumOfStores;		///< SVFIR store edge
    static const char* NumOfCopys;		///< SVFIR copy edge
    static const char* NumOfGeps;		///< SVFIR gep edge
    static const char* NumOfCalls;		///< SVFIR call edge
    static const char* NumOfReturns;	///< SVFIR return edge

    static const char* NumOfProcessedAddrs;		///< SVFIR processed addr edge
    static const char* NumOfProcessedLoads;		///< SVFIR processed load edge
    static const char* NumOfProcessedStores;	///< SVFIR processed store edge
    static const char* NumOfProcessedCopys;		///< SVFIR processed copy edge
    static const char* NumOfProcessedGeps;		///< SVFIR processed gep edge

    static const char* NumOfSfr;                ///< num of field representatives
    static const char* NumOfFieldExpand;

    static const char* NumOfFunctionObjs;	///< Function numbers
    static const char* NumOfGlobalObjs;	///< SVFIR global object node
    static const char* NumOfHeapObjs;	///< SVFIR heap object node
    static const char* NumOfStackObjs;	///< SVFIR stack object node

    static const char* NumOfObjsHasVarStruct;	///< SVFIR object node has var struct (maybe nested with array)
    static const char* NumOfObjsHasVarArray;	///< SVFIR object node has var array (maybe nested with struct)
    static const char* NumOfObjsHasConstStruct;	///< SVFIR object node has const struct (maybe nested with array)
    static const char* NumOfObjsHasConstArray;	///< SVFIR object node has const array (maybe nested with struct)
    static const char* NumOfNonPtrObjs;	///< SVFIR object node which is non pointer type object (do not have pts)
    static const char* NumOfConstantObjs;	///< SVFIR object node which is purely constant

    static const char* NumberOfFieldInsensitiveObj;
    static const char* NumberOfFieldSensitiveObj;

    static const char* NumOfPointers;	///< SVFIR value node, each of them maps to a llvm value
    static const char* NumOfGepFieldPointers;	///< SVFIR gep value node (field value, dynamically created dummy node)

    static const char* NumOfMemObjects;	///< SVFIR object node, each of them maps to a llvm value
    static const char* NumOfGepFieldObjects;	///< SVFIR gep object node (field obj, dynamically created dummy node)

    static const char* AveragePointsToSetSize;		///< Average points-to size of all variables
    static const char* AverageTopLevPointsToSetSize; ///< Average points-to size of top-level variables
    static const char* MaxPointsToSetSize;			///< Max points-to size

    static const char* NumOfIterations;	///< Number of iterations during resolution

    static const char* NumOfIndirectCallSites;	///< Number of indirect callsites
    static const char* NumOfIndirectEdgeSolved;	///< Number of indirect calledge resolved

    static const char* NumOfSCCDetection; ///< Number of scc detection performed
    static const char* NumOfCycles;   ///< Number of scc cycles detected
    static const char* NumOfPWCCycles;   ///< Number of scc cycles detected
    static const char* NumOfNodesInCycles; ///< Number of nodes in cycles detected
    static const char* MaxNumOfNodesInSCC;	///< max Number of nodes in one scc

    static const char* NumOfNullPointer;	///< Number of pointers points-to null

    typedef Map<const char*,u32_t> NUMStatMap;

    typedef Map<const char*,double> TIMEStatMap;

    enum ClockType
    {
        Wall,
        CPU,
    };

    PTAStat(PointerAnalysis* p);
    virtual ~PTAStat() {}

    virtual inline void startClk()
    {
        startTime = getClk(true);
    }
    virtual inline void endClk()
    {
        endTime = getClk(true);
    }
    /// When mark is true, real clock is always returned. When mark is false, it is
    /// only returned when Options::MarkedClocksOnly is not set.
    /// Default call for getClk is unmarked, while MarkedClocksOnly is false by default.
    static double getClk(bool mark=false);

    NUMStatMap generalNumMap;
    NUMStatMap PTNumStatMap;
    TIMEStatMap timeStatMap;
    NodeBS localVarInRecursion;

    double startTime;
    double endTime;

    virtual void performStat();

    virtual void printStat(string str = "");

    virtual void performStatPerQuery(NodeID) {}

    virtual void printStatPerQuery(NodeID, const PointsTo&) {}

    virtual void callgraphStat();
private:
    void bitcastInstStat();
    void branchStat();

    PointerAnalysis* pta;
    std::string moduleName;
};

} // End namespace SVF

#endif /* ANDERSENSTAT_H_ */
