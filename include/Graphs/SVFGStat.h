//===- SVFGStat.h -- Statistics of SVFG---------------------------------------//
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
 * @file: SVFGStat.h
 * @author: yesen
 * @date: 10/12/2013
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#ifndef SVFGSTAT_H_
#define SVFGSTAT_H_

#include "Graphs/SVFG.h"
#include "MemoryModel/PTAStat.h"
#include "Util/SCC.h"

namespace SVF
{

class MemSSA;

class MemSSAStat : public PTAStat
{

public:
    static const char* TotalTimeOfConstructMemSSA;	///< Total time for constructing memory SSA
    static const char* TimeOfGeneratingMemRegions;	///< Time for allocating regions
    static const char* TimeOfCreateMUCHI;	///< Time for generating mu/chi for load/store/calls
    static const char* TimeOfInsertingPHI;	///< Time for inserting phis
    static const char* TimeOfSSARenaming;	///< Time for SSA rename

    static const char* NumOfMaxRegion;	///< Number of max points-to set in region.
    static const char* NumOfAveragePtsInRegion;	///< Number of average points-to set in region.
    static const char* NumOfMemRegions;	///< Number of memory regions
    static const char* NumOfEntryChi;	///< Number of function entry chi
    static const char* NumOfRetMu;	///< Number of function return mu
    static const char* NumOfCSChi;	///< Number of call site chi
    static const char* NumOfCSMu;	///< Number of call site mu
    static const char* NumOfLoadMu;	///< Number of load mu
    static const char* NumOfStoreChi;	///< Number of store chi
    static const char* NumOfMSSAPhi;	///< Number of mssa phi

    static const char* NumOfFunHasEntryChi;	///< Number of functions which have entry chi
    static const char* NumOfFunHasRetMu;	///< Number of functions which have return mu
    static const char* NumOfCSHasChi;	///< Number of call sites which have chi
    static const char* NumOfCSHasMu;	///< Number of call sites which have mu
    static const char* NumOfLoadHasMu;	///< Number of loads which have mu
    static const char* NumOfStoreHasChi;	///< Number of stores which have chi
    static const char* NumOfBBHasMSSAPhi;	///< Number of basic blocks which have mssa phi

    MemSSAStat(MemSSA*);

    virtual ~MemSSAStat()
    {

    }
    virtual void performStat() override;

    virtual void printStat(std::string str = "") override;

private:
    MemSSA* mssa;
};


class SVFGStat : public PTAStat
{
public:
    typedef Set<const SVFGNode*> SVFGNodeSet;
    typedef OrderedSet<const SVFGEdge*> SVFGEdgeSet;
    typedef SCCDetection<SVFG*> SVFGSCC;

    SVFGStat(SVFG* g);

    virtual ~SVFGStat() {}

    virtual void performStat() override;

    virtual void printStat(std::string str = "") override;

    virtual void performSCCStat(SVFGEdgeSet insensitiveCalRetEdges);

    void dirVFEdgeStart()
    {
        connectDirSVFGEdgeTimeStart = PTAStat::getClk(true);
    }

    void dirVFEdgeEnd()
    {
        connectDirSVFGEdgeTimeEnd = PTAStat::getClk(true);
    }

    void indVFEdgeStart()
    {
        connectIndSVFGEdgeTimeStart = PTAStat::getClk(true);
    }

    void indVFEdgeEnd()
    {
        connectIndSVFGEdgeTimeEnd = PTAStat::getClk(true);
    }

    void TLVFNodeStart()
    {
        addTopLevelNodeTimeStart = PTAStat::getClk(true);
    }

    void TLVFNodeEnd()
    {
        addTopLevelNodeTimeEnd = PTAStat::getClk(true);
    }

    void ATVFNodeStart()
    {
        addAddrTakenNodeTimeStart = PTAStat::getClk(true);
    }

    void ATVFNodeEnd()
    {
        addAddrTakenNodeTimeEnd = PTAStat::getClk(true);
    }

    void sfvgOptStart()
    {
        svfgOptTimeStart = PTAStat::getClk(true);
    }

    void sfvgOptEnd()
    {
        svfgOptTimeEnd = PTAStat::getClk(true);
    }

private:
    void clear();

    void processGraph();

    void calculateNodeDegrees(SVFGNode* node, NodeSet& nodeHasIndInEdge, NodeSet& nodeHasIndOutEdge);

    NodeID getSCCRep(SVFGSCC* scc, NodeID id) const;

    NodeID nodeInCycle(SVFGSCC* scc, NodeID id) const;

    SVFG* graph;

    int numOfNodes;	///< number of svfg nodes.

    int numOfFormalIn;	///< number of formal in svfg nodes.
    int numOfFormalOut;	///< number of formal out svfg nodes.
    int numOfFormalParam;
    int numOfFormalRet;

    int numOfActualIn;	///< number of actual in svfg nodes.
    int numOfActualOut;	///< number of actual out svfg nodes.
    int numOfActualParam;
    int numOfActualRet;

    int numOfLoad;	///< number of load svfg nodes.
    int numOfStore;	///< number of store svfg nodes.
    int numOfCopy;
    int numOfGep;
    int numOfAddr;

    int numOfMSSAPhi;	///< number of mssa phi svfg nodes.
    int numOfPhi;

    int totalInEdge;	///< Total number of incoming SVFG edges
    int totalOutEdge;	///< Total number of outgoing SVFG edges
    int totalIndInEdge;	///< Total number of indirect SVFG edges
    int totalIndOutEdge;
    int totalIndEdgeLabels; ///< Total number of l --o--> lp

    int totalIndCallEdge;
    int totalIndRetEdge;
    int totalDirCallEdge;
    int totalDirRetEdge;

    int avgWeight;	///< average weight.

    int avgInDegree;	///< average in degrees of SVFG nodes.
    int avgOutDegree;	///< average out degrees of SVFG nodes.
    u32_t maxInDegree;	///< max in degrees of SVFG nodes.
    u32_t maxOutDegree;	///< max out degrees of SVFG nodes.

    int avgIndInDegree;	///< average indirect in degrees of SVFG nodes.
    int avgIndOutDegree;	///< average indirect out degrees of SVFG nodes.
    u32_t maxIndInDegree;	///< max indirect in degrees of SVFG nodes.
    u32_t maxIndOutDegree;	///< max indirect out degrees of SVFG nodes.

    double addTopLevelNodeTimeStart;
    double addTopLevelNodeTimeEnd;

    double addAddrTakenNodeTimeStart;
    double addAddrTakenNodeTimeEnd;

    double connectDirSVFGEdgeTimeStart;
    double connectDirSVFGEdgeTimeEnd;

    double connectIndSVFGEdgeTimeStart;
    double connectIndSVFGEdgeTimeEnd;

    double svfgOptTimeStart;
    double svfgOptTimeEnd;

    SVFGNodeSet forwardSlice;
    SVFGNodeSet backwardSlice;
    SVFGNodeSet	sources;
    SVFGNodeSet	sinks;

public:
    inline void addToSources(const SVFGNode* node)
    {
        sources.insert(node);
    }
    inline void addToSinks(const SVFGNode* node)
    {
        sinks.insert(node);
    }
    inline void addToForwardSlice(const SVFGNode* node)
    {
        forwardSlice.insert(node);
    }
    inline void addToBackwardSlice(const SVFGNode* node)
    {
        backwardSlice.insert(node);
    }
    inline bool inForwardSlice(const SVFGNode* node) const
    {
        return forwardSlice.find(node)!=forwardSlice.end();
    }
    inline bool inBackwardSlice(const SVFGNode* node) const
    {
        return backwardSlice.find(node)!=backwardSlice.end();
    }
    inline bool isSource(const SVFGNode* node) const
    {
        return sources.find(node)!=sources.end();
    }
    inline bool isSink(const SVFGNode* node) const
    {
        return sinks.find(node)!=sinks.end();
    }
};

} // End namespace SVF

#endif /* SVFGSTAT_H_ */
