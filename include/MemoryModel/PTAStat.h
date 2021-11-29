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

    static const char* TotalNumOfPointers;	///< Total PAG value node
    static const char* TotalNumOfObjects;	///< Total PAG object node
    static const char* TotalNumOfFieldObjects;	///< Total PAG field object node
    static const char* MaxStructSize;	///< Max struct size
    static const char* TotalNumOfEdges;	///< Total PAG edge number

    static const char* NumOfAddrs;		///< PAG addr edge
    static const char* NumOfLoads;		///< PAG load edge
    static const char* NumOfStores;		///< PAG store edge
    static const char* NumOfCopys;		///< PAG copy edge
    static const char* NumOfGeps;		///< PAG gep edge
    static const char* NumOfCalls;		///< PAG call edge
    static const char* NumOfReturns;	///< PAG return edge

    static const char* NumOfProcessedAddrs;		///< PAG processed addr edge
    static const char* NumOfProcessedLoads;		///< PAG processed load edge
    static const char* NumOfProcessedStores;	///< PAG processed store edge
    static const char* NumOfProcessedCopys;		///< PAG processed copy edge
    static const char* NumOfProcessedGeps;		///< PAG processed gep edge

    static const char* NumOfSfr;                ///< num of field representatives
    static const char* NumOfFieldExpand;

    static const char* NumOfFunctionObjs;	///< Function numbers
    static const char* NumOfGlobalObjs;	///< PAG global object node
    static const char* NumOfHeapObjs;	///< PAG heap object node
    static const char* NumOfStackObjs;	///< PAG stack object node

    static const char* NumOfObjsHasVarStruct;	///< PAG object node has var struct (maybe nested with array)
    static const char* NumOfObjsHasVarArray;	///< PAG object node has var array (maybe nested with struct)
    static const char* NumOfObjsHasConstStruct;	///< PAG object node has const struct (maybe nested with array)
    static const char* NumOfObjsHasConstArray;	///< PAG object node has const array (maybe nested with struct)
    static const char* NumOfNonPtrObjs;	///< PAG object node which is non pointer type object (do not have pts)
    static const char* NumOfConstantObjs;	///< PAG object node which is purely constant

    static const char* NumberOfFieldInsensitiveObj;
    static const char* NumberOfFieldSensitiveObj;

    static const char* NumOfPointers;	///< PAG value node, each of them maps to a llvm value
    static const char* NumOfGepFieldPointers;	///< PAG gep value node (field value, dynamically created dummy node)

    static const char* NumOfMemObjects;	///< PAG object node, each of them maps to a llvm value
    static const char* NumOfGepFieldObjects;	///< PAG gep object node (field obj, dynamically created dummy node)

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

    PTAStat(PointerAnalysis* p);
    virtual ~PTAStat() {}

    virtual inline void startClk()
    {
        startTime = CLOCK_IN_MS();
    }
    virtual inline void endClk()
    {
        endTime = CLOCK_IN_MS();
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
