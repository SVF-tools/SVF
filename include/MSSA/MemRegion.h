//===- MemRegion.h -- Memory region -----------------------------------------//
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
 * MemoryRegion.h
 *
 *  Created on: Jul 14, 2013
 *      Author: Yulei Sui
 */

#ifndef MEMORYREGION_H_
#define MEMORYREGION_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/PTACallGraph.h"
#include "Util/WorkList.h"
#include "Graphs/ICFG.h"

#include <set>

namespace SVF
{

typedef NodeID MRID;
typedef NodeID MRVERID;
typedef NodeID MRVERSION;

/// Memory Region class
class MemRegion
{

public:
    typedef DdNode* Condition;
private:
    /// region ID 0 is reserved
    static Size_t totalMRNum;
    MRID rid;
    const PointsTo cptsSet;

public:
    /// Constructor
    MemRegion(const PointsTo& cp) :
        rid(++totalMRNum), cptsSet(cp)
    {
    }
    /// Destructor
    ~MemRegion()
    {
    }

    /// Return memory region ID
    inline MRID getMRID() const
    {
        return rid;
    }
    /// Return points-to
    inline const PointsTo &getPointsTo() const
    {
        return cptsSet;
    }
    /// Operator== overriding
    inline bool operator==(const MemRegion* rhs) const
    {
        return this->getPointsTo() == rhs->getPointsTo();
    }
    /// Dump string
    inline std::string dumpStr() const
    {
        std::string str;
        str += "pts{";
        for (PointsTo::iterator ii = cptsSet.begin(), ie = cptsSet.end();
                ii != ie; ii++)
        {
            char int2str[16];
            sprintf(int2str, "%d", *ii);
            str += int2str;
            str += " ";
        }
        str += "}";
        return str;
    }

    /// add the hash function here to sort elements and remove
    /// and remove duplicated element in the set (binary tree comparision)
    //@{
    typedef struct
    {
        bool operator()(const MemRegion* lhs, const MemRegion* rhs) const
        {
            return SVFUtil::cmpPts(lhs->getPointsTo(), rhs->getPointsTo());
        }
    } equalMemRegion;

    typedef struct
    {
        bool operator()(const PointsTo& lhs, const PointsTo& rhs) const
        {
            return SVFUtil::cmpPts(lhs, rhs);
        }
    } equalPointsTo;
    //@}

    /// Return memory object number inside a region
    inline u32_t getRegionSize() const
    {
        return cptsSet.count();
    }
};

/*!
 * Memory Region Partitioning
 */
class MRGenerator
{

public:
    typedef FIFOWorkList<NodeID> WorkList;

    /// Get typedef from Pointer Analysis
    //@{
    //@}
    ///Define mem region set
    typedef OrderedSet<const MemRegion*, MemRegion::equalMemRegion> MRSet;
    typedef Map<const PAGEdge*, const SVFFunction*> PAGEdgeToFunMap;
    typedef OrderedSet<PointsTo, MemRegion::equalPointsTo> PointsToList;
    typedef Map<const SVFFunction*, PointsToList > FunToPointsToMap;
    typedef OrderedMap<PointsTo, PointsTo, MemRegion::equalPointsTo > PtsToRepPtsSetMap;

    /// Map a function to its region set
    typedef Map<const SVFFunction*, MRSet> FunToMRsMap;
    /// Map loads/stores to its mem regions,
    /// TODO:visitAtomicCmpXchgInst, visitAtomicRMWInst??
    //@{
    typedef Map<const LoadPE*, MRSet> LoadsToMRsMap;
    typedef Map<const StorePE*, MRSet> StoresToMRsMap;
    typedef Map<const CallBlockNode*, MRSet> CallSiteToMRsMap;
    //@}

    /// Map loads/stores/callsites to their cpts set
    //@{
    typedef Map<const LoadPE*, PointsTo> LoadsToPointsToMap;
    typedef Map<const StorePE*, PointsTo> StoresToPointsToMap;
    typedef Map<const CallBlockNode*, PointsTo> CallSiteToPointsToMap;
    //@}

    /// Maps Mod-Ref analysis
    //@{
    /// Map a function to its indirect refs/mods of memory objects
    typedef Map<const SVFFunction*, NodeBS> FunToNodeBSMap;
    /// Map a callsite to its indirect refs/mods of memory objects
    typedef Map<const CallBlockNode*, NodeBS> CallSiteToNodeBSMap;
    //@}

    typedef Map<NodeID, NodeBS> NodeToPTSSMap;

    /// PAG edge list
    typedef PAG::PAGEdgeList PAGEdgeList;
    /// Call Graph SCC
    typedef SCCDetection<PTACallGraph*> SCC;

    MRSet& getMRSet()
    {
        return memRegSet;
    }

    /// Get superset cpts set
    inline const PointsTo& getRepPointsTo(const PointsTo& cpts) const
    {
        PtsToRepPtsSetMap::const_iterator it = cptsToRepCPtsMap.find(cpts);
        assert(it!=cptsToRepCPtsMap.end() && "can not find superset of cpts??");
        return it->second;
    }
    /// Get a memory region according to cpts
    const MemRegion* getMR(const PointsTo& cpts) const;

private:

    BVDataPTAImpl* pta;
    SCC* callGraphSCC;
    PTACallGraph* callGraph;
    bool ptrOnlyMSSA;

    /// Map a function to all its memory regions
    FunToMRsMap funToMRsMap;
    /// Map a load PAG Edge to its memory regions sets in order for inserting mus in Memory SSA
    LoadsToMRsMap loadsToMRsMap;
    /// Map a store PAG Edge to its memory regions sets in order for inserting chis in Memory SSA
    StoresToMRsMap storesToMRsMap;
    /// Map a callsite to its refs regions
    CallSiteToMRsMap callsiteToRefMRsMap;
    /// Map a callsite to its mods regions
    CallSiteToMRsMap callsiteToModMRsMap;
    /// Map a load PAG Edge to its CPts set map
    LoadsToPointsToMap loadsToPointsToMap;
    /// Map a store PAG Edge to its CPts set map
    StoresToPointsToMap	storesToPointsToMap;
    /// Map a callsite to it refs cpts set
    CallSiteToPointsToMap callsiteToRefPointsToMap;
    /// Map a callsite to it mods cpts set
    CallSiteToPointsToMap callsiteToModPointsToMap;

    /// Map a function to all of its conditional points-to sets
    FunToPointsToMap funToPointsToMap;
    /// Map a PAGEdge to its fun
    PAGEdgeToFunMap pagEdgeToFunMap;

    /// Map a function to its indirect uses of memory objects
    FunToNodeBSMap funToRefsMap;
    /// Map a function to its indirect defs of memory objects
    FunToNodeBSMap funToModsMap;
    /// Map a callsite to its indirect uses of memory objects
    CallSiteToNodeBSMap csToRefsMap;
    /// Map a callsite to its indirect defs of memory objects
    CallSiteToNodeBSMap csToModsMap;
    /// Map a callsite to all its object might pass into its callees
    CallSiteToNodeBSMap csToCallSiteArgsPtsMap;
    /// Map a callsite to all its object might return from its callees
    CallSiteToNodeBSMap csToCallSiteRetPtsMap;

    /// Map a pointer to its cached points-to chain;
    NodeToPTSSMap cachedPtsChainMap;

    /// All global variable PAG node ids
    NodeBS allGlobals;

    /// Clean up memory
    void destroy();

    //Get all objects might pass into callee from a callsite
    void collectCallSitePts(const CallBlockNode* cs);

    //Recursive collect points-to chain
    NodeBS& CollectPtsChain(NodeID id);

    /// Return the pts chain of all callsite arguments
    inline NodeBS& getCallSiteArgsPts(const CallBlockNode* cs)
    {
        return csToCallSiteArgsPtsMap[cs];
    }
    /// Return the pts chain of the return parameter of the callsite
    inline NodeBS& getCallSiteRetPts(const CallBlockNode* cs)
    {
        return csToCallSiteRetPtsMap[cs];
    }
    /// Whether the object node is a non-local object
    /// including global, heap, and stack variable in recursions
    bool isNonLocalObject(NodeID id, const SVFFunction* curFun) const;

    /// Get all the objects in callee's modref escaped via global objects (the chain pts of globals)
    void getEscapObjviaGlobals(NodeBS& globs, const NodeBS& pts);

    /// Get reverse topo call graph scc
    void getCallGraphSCCRevTopoOrder(WorkList& worklist);

protected:
    MRGenerator(BVDataPTAImpl* p, bool ptrOnly) :
        pta(p), ptrOnlyMSSA(ptrOnly)
    {
        callGraph = pta->getPTACallGraph();
        callGraphSCC = new SCC(callGraph);
    }

    /// A set of All memory regions
    MRSet memRegSet;
    /// Map a condition pts to its rep conditional pts (super set points-to)
    PtsToRepPtsSetMap cptsToRepCPtsMap;

    /// Generate a memory region and put in into functions which use it
    void createMR(const SVFFunction* fun, const PointsTo& cpts);

    /// Collect all global variables for later escape analysis
    void collectGlobals();

    /// Generate regions for loads/stores
    virtual void collectModRefForLoadStore();

    /// Generate regions for calls/rets
    virtual void collectModRefForCall();

    /// Partition regions
    virtual void partitionMRs();

    /// Update aliased regions for loads/stores/callsites
    virtual void updateAliasMRs();

    /// Given a condition pts, insert into cptsToRepCPtsMap for region generation
    virtual void sortPointsTo(const PointsTo& cpts);

    /// Whether a region is aliased with a conditional points-to
    virtual inline bool isAliasedMR(const PointsTo& cpts, const MemRegion* mr)
    {
        return mr->getPointsTo().intersects(cpts);
    }
    /// Get all aliased mem regions from function fun according to cpts
    virtual inline void getAliasMemRegions(MRSet& aliasMRs, const PointsTo& cpts, const SVFFunction* fun)
    {
        for(MRSet::const_iterator it = funToMRsMap[fun].begin(), eit = funToMRsMap[fun].end(); it!=eit; ++it)
        {
            if(isAliasedMR(cpts,*it))
                aliasMRs.insert(*it);
        }
    }

    /// Get memory regions for a load statement according to cpts.
    virtual inline void getMRsForLoad(MRSet& aliasMRs, const PointsTo& cpts, const SVFFunction*)
    {
        const MemRegion* mr = getMR(cpts);
        aliasMRs.insert(mr);
    }

    /// Get memory regions for call site ref according to cpts.
    virtual inline void getMRsForCallSiteRef(MRSet& aliasMRs, const PointsTo& cpts, const SVFFunction*)
    {
        const MemRegion* mr = getMR(cpts);
        aliasMRs.insert(mr);
    }

    /// Mod-Ref analysis for callsite invoking this callGraphNode
    virtual void modRefAnalysis(PTACallGraphNode* callGraphNode, WorkList& worklist);

    /// Get Mod-Ref of a callee function
    virtual bool handleCallsiteModRef(NodeBS& mod, NodeBS& ref, const CallBlockNode* cs, const SVFFunction* fun);


    /// Add cpts to store/load
    //@{
    inline void addCPtsToStore(PointsTo& cpts, const StorePE *st, const SVFFunction* fun)
    {
        storesToPointsToMap[st] = cpts;
        funToPointsToMap[fun].insert(cpts);
        addModSideEffectOfFunction(fun,cpts);
    }
    inline void addCPtsToLoad(PointsTo& cpts, const LoadPE *ld, const SVFFunction* fun)
    {
        loadsToPointsToMap[ld] = cpts;
        funToPointsToMap[fun].insert(cpts);
        addRefSideEffectOfFunction(fun,cpts);
    }
    inline void addCPtsToCallSiteRefs(PointsTo& cpts, const CallBlockNode* cs)
    {
        callsiteToRefPointsToMap[cs] |= cpts;
        funToPointsToMap[cs->getCaller()].insert(cpts);
    }
    inline void addCPtsToCallSiteMods(PointsTo& cpts, const CallBlockNode* cs)
    {
        callsiteToModPointsToMap[cs] |= cpts;
        funToPointsToMap[cs->getCaller()].insert(cpts);
    }
    inline bool hasCPtsList(const SVFFunction* fun) const
    {
        return funToPointsToMap.find(fun)!=funToPointsToMap.end();
    }
    inline PointsToList& getPointsToList(const SVFFunction* fun)
    {
        return funToPointsToMap[fun];
    }
    inline FunToPointsToMap& getFunToPointsToList()
    {
        return funToPointsToMap;
    }
    //@}
    /// Add/Get methods for side-effect of functions and callsites
    //@{
    /// Add indirect uses an memory object in the function
    void addRefSideEffectOfFunction(const SVFFunction* fun, const NodeBS& refs);
    /// Add indirect def an memory object in the function
    void addModSideEffectOfFunction(const SVFFunction* fun, const NodeBS& mods);
    /// Add indirect uses an memory object in the function
    bool addRefSideEffectOfCallSite(const CallBlockNode* cs, const NodeBS& refs);
    /// Add indirect def an memory object in the function
    bool addModSideEffectOfCallSite(const CallBlockNode* cs, const NodeBS& mods);

    /// Get indirect refs of a function
    inline const NodeBS& getRefSideEffectOfFunction(const SVFFunction* fun)
    {
        return funToRefsMap[fun];
    }
    /// Get indirect mods of a function
    inline const NodeBS& getModSideEffectOfFunction(const SVFFunction* fun)
    {
        return funToModsMap[fun];
    }
    /// Get indirect refs of a callsite
    inline const NodeBS& getRefSideEffectOfCallSite(const CallBlockNode* cs)
    {
        return csToRefsMap[cs];
    }
    /// Get indirect mods of a callsite
    inline const NodeBS& getModSideEffectOfCallSite(const CallBlockNode* cs)
    {
        return csToModsMap[cs];
    }
    /// Has indirect refs of a callsite
    inline bool hasRefSideEffectOfCallSite(const CallBlockNode* cs)
    {
        return csToRefsMap.find(cs) != csToRefsMap.end();
    }
    /// Has indirect mods of a callsite
    inline bool hasModSideEffectOfCallSite(const CallBlockNode* cs)
    {
        return csToModsMap.find(cs) != csToModsMap.end();
    }
    //@}

public:
    inline Size_t getMRNum() const
    {
        return memRegSet.size();
    }

    /// Destructor
    virtual ~MRGenerator()
    {
        destroy();
    }

    /// Start generating memory regions
    virtual void generateMRs();

    /// Get the function which PAG Edge located
    const SVFFunction* getFunction(const PAGEdge* pagEdge) const
    {
        PAGEdgeToFunMap::const_iterator it = pagEdgeToFunMap.find(pagEdge);
        assert(it!=pagEdgeToFunMap.end() && "can not find its function, it is a global PAG edge");
        return it->second;
    }
    /// Get Memory Region set
    //@{
    inline MRSet& getFunMRSet(const SVFFunction* fun)
    {
        return funToMRsMap[fun];
    }
    inline MRSet& getLoadMRSet(const LoadPE* load)
    {
        return loadsToMRsMap[load];
    }
    inline MRSet& getStoreMRSet(const StorePE* store)
    {
        return storesToMRsMap[store];
    }
    inline bool hasRefMRSet(const CallBlockNode* cs)
    {
        return callsiteToRefMRsMap.find(cs)!=callsiteToRefMRsMap.end();
    }
    inline bool hasModMRSet(const CallBlockNode* cs)
    {
        return callsiteToModMRsMap.find(cs)!=callsiteToModMRsMap.end();
    }
    inline MRSet& getCallSiteRefMRSet(const CallBlockNode* cs)
    {
        return callsiteToRefMRsMap[cs];
    }
    inline MRSet& getCallSiteModMRSet(const CallBlockNode* cs)
    {
        return callsiteToModMRsMap[cs];
    }
    //@}
    /// Whether this instruction has PAG Edge
    inline bool hasPAGEdgeList(const Instruction* inst)
    {
        PAG* pag = pta->getPAG();
        if (ptrOnlyMSSA)
            return pag->hasPTAPAGEdgeList(pag->getICFG()->getBlockICFGNode(inst));
        else
            return pag->hasPAGEdgeList(pag->getICFG()->getBlockICFGNode(inst));
    }
    /// Given an instruction, get all its the PAGEdge (statement) in sequence
    inline PAGEdgeList& getPAGEdgesFromInst(const Instruction* inst)
    {
        PAG* pag = pta->getPAG();
        if (ptrOnlyMSSA)
            return pag->getInstPTAPAGEdgeList(pag->getICFG()->getBlockICFGNode(inst));
        else
            return pag->getInstPAGEdgeList(pag->getICFG()->getBlockICFGNode(inst));
    }

    /// getModRefInfo APIs
    //@{
    /// Collect mod ref for external callsite other than heap alloc external call
    PointsTo getModInfoForCall(const CallBlockNode* cs);
    PointsTo getRefInfoForCall(const CallBlockNode* cs);
    ModRefInfo getModRefInfo(const CallBlockNode* cs);
    ModRefInfo getModRefInfo(const CallBlockNode* cs, const Value* V);
    ModRefInfo getModRefInfo(const CallBlockNode* cs1, const CallBlockNode* cs2);
    //@}

};

} // End namespace SVF

#endif /* MEMORYREGION_H_ */
