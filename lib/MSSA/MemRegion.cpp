//===- MemRegion.cpp -- Memory region-----------------------------------------//
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
 * MemRegion.cpp
 *
 *  Created on: Dec 14, 2013
 *      Author: Yulei Sui
 */

#include "Util/SVFModule.h"
#include "MSSA/MemRegion.h"
#include "MSSA/MSSAMuChi.h"
#include "SVF-FE/LLVMUtil.h"

using namespace SVF;
using namespace SVFUtil;

Size_t MemRegion::totalMRNum = 0;
Size_t MRVer::totalVERNum = 0;

static llvm::cl::opt<bool> IgnoreDeadFun("mssa-ignoreDeadFun", llvm::cl::init(false),
        llvm::cl::desc("Don't construct memory SSA for deadfunction"));

/*!
 * Clean up memory
 */
void MRGenerator::destroy()
{

    for (const auto *it : memRegSet)
    {
        delete it;
    }

    delete callGraphSCC;
    callGraphSCC = nullptr;
    callGraph = nullptr;
    pta = nullptr;
}

/*!
 * Generate a memory region and put in into functions which use it
 */
void MRGenerator::createMR(const SVFFunction* fun, const PointsTo& cpts)
{
    const PointsTo& repCPts = getRepPointsTo(cpts);
    MemRegion mr(repCPts);
    auto mit = memRegSet.find(&mr);
    if(mit!=memRegSet.end())
    {
        const MemRegion* mr = *mit;
        MRSet& mrs = funToMRsMap[fun];
        if(mrs.find(mr)==mrs.end())
            mrs.insert(mr);
    }
    else
    {
        auto* m = new MemRegion(repCPts);
        memRegSet.insert(m);
        funToMRsMap[fun].insert(m);
    }
}

/*!
 * Generate a memory region and put in into functions which use it
 */
const MemRegion* MRGenerator::getMR(const PointsTo& cpts) const
{
    MemRegion mr(getRepPointsTo(cpts));
    auto mit = memRegSet.find(&mr);
    assert(mit!=memRegSet.end() && "memory region not found!!");
    return *mit;
}


/*!
 * Collect globals for escape analysis
 */
void MRGenerator::collectGlobals()
{
    PAG* pag = pta->getPAG();
    for (auto & nIter : *pag)
    {
        if(auto* obj = SVFUtil::dyn_cast<ObjPN>(nIter.second))
        {
            if (obj->getMemObj()->isGlobalObj())
            {
                allGlobals.set(nIter.first);
                allGlobals |= CollectPtsChain(nIter.first);
            }
        }
    }
}

/*!
 * Generate memory regions according to pointer analysis results
 * Attach regions on loads/stores
 */
void MRGenerator::generateMRs()
{

    DBOUT(DGENERAL, outs() << pasMsg("Generate Memory Regions \n"));

    collectGlobals();

    callGraphSCC->find();

    DBOUT(DGENERAL, outs() << pasMsg("\tCollect ModRef For Load/Store \n"));

    /// collect mod-ref for loads/stores
    collectModRefForLoadStore();

    DBOUT(DGENERAL, outs() << pasMsg("\tCollect ModRef For const CallBlockNode*\n"));

    /// collect mod-ref for calls
    collectModRefForCall();

    DBOUT(DGENERAL, outs() << pasMsg("\tPartition Memory Regions \n"));
    /// Partition memory regions
    partitionMRs();
    /// attach memory regions for loads/stores/calls
    updateAliasMRs();
}

/*!
 * Generate memory regions for loads/stores
 */
void MRGenerator::collectModRefForLoadStore()
{

    SVFModule* svfModule = pta->getModule();
    for (const auto *fi : *svfModule)
    {
        const SVFFunction& fun = *fi;

        /// if this function does not have any caller, then we do not care its MSSA
        if (IgnoreDeadFun && isDeadFunction(fun.getLLVMFun()))
            continue;

        for (Function::const_iterator iter = fun.getLLVMFun()->begin(), eiter = fun.getLLVMFun()->end();
                iter != eiter; ++iter)
        {
            const BasicBlock& bb = *iter;
            for (const auto & inst : bb)
            {
                 PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(&inst);
                for (const auto *inst : pagEdgeList)
                {
                     pagEdgeToFunMap[inst] = &fun;
                    if (const auto *st = SVFUtil::dyn_cast<StorePE>(inst))
                    {
                        PointsTo cpts(pta->getPts(st->getDstID()));
                        // TODO: change this assertion check later when we have conditional points-to set
                        if (cpts.empty())
                            continue;
                        assert(!cpts.empty() && "null pointer!!");
                        addCPtsToStore(cpts, st, &fun);
                    }

                    else if (const auto *ld = SVFUtil::dyn_cast<LoadPE>(inst))
                    {
                        PointsTo cpts(pta->getPts(ld->getSrcID()));
                        // TODO: change this assertion check later when we have conditional points-to set
                        if (cpts.empty())
                            continue;
                        assert(!cpts.empty() && "null pointer!!");
                        addCPtsToLoad(cpts, ld, &fun);
                    }
                }
            }
        }
    }
}


/*!
 * Generate memory regions for calls
 */
void MRGenerator::collectModRefForCall()
{

    DBOUT(DGENERAL, outs() << pasMsg("\t\tCollect Callsite PointsTo \n"));

    /// collect points-to information for callsites
    for(const auto *it : pta->getPAG()->getCallSiteSet())
    {
        collectCallSitePts(it);
    }

    DBOUT(DGENERAL, outs() << pasMsg("\t\tPerform Callsite Mod-Ref \n"));

    WorkList worklist;
    getCallGraphSCCRevTopoOrder(worklist);

    while(!worklist.empty())
    {
        NodeID callGraphNodeID = worklist.pop();
        /// handle all sub scc nodes of this rep node
        const NodeBS& subNodes = callGraphSCC->subNodes(callGraphNodeID);
        for(const auto &subNode : subNodes)
        {
            PTACallGraphNode* subCallGraphNode = callGraph->getCallGraphNode(subNode);
            /// Get mod-ref of all callsites calling callGraphNode
            modRefAnalysis(subCallGraphNode,worklist);
        }
    }

    DBOUT(DGENERAL, outs() << pasMsg("\t\tAdd PointsTo to Callsites \n"));

    for (CallSite cs : SymbolTableInfo::SymbolInfo()->getCallSiteSet())
    {
        const CallBlockNode* callBlockNode = pta->getPAG()->getICFG()->getCallBlockNode(cs.getInstruction());
        if(hasRefSideEffectOfCallSite(callBlockNode))
        {
            NodeBS refs = getRefSideEffectOfCallSite(callBlockNode);
            addCPtsToCallSiteRefs(refs,callBlockNode);
        }
        if(hasModSideEffectOfCallSite(callBlockNode))
        {
            NodeBS mods = getModSideEffectOfCallSite(callBlockNode);
            /// mods are treated as both def and use of memory objects
            addCPtsToCallSiteMods(mods,callBlockNode);
            addCPtsToCallSiteRefs(mods,callBlockNode);
        }
    }
}

/*!
 * Given a condition pts, insert into cptsToRepCPtsMap
 * Always map it to its superset(rep) cpts according to existing items
 * 1) map cpts to its superset(rep) which exists in the map, otherwise its superset is itself
 * 2) adjust existing items in the map if their supersets are cpts
 */
void MRGenerator::sortPointsTo(const PointsTo& cpts)
{

    if(cptsToRepCPtsMap.find(cpts)!=cptsToRepCPtsMap.end())
        return;

    PointsToList subSetList;
    PointsTo repCPts = cpts;
    for(auto & it : cptsToRepCPtsMap)
    {
        PointsTo& existCPts = it.second;
        if(cpts.contains(existCPts))
        {
            subSetList.insert(it.first);
        }
        else if(existCPts.contains(cpts))
        {
            repCPts = existCPts;
        }
    }

    for(const auto & it : subSetList)
    {
        cptsToRepCPtsMap[it] = cpts;
    }

    cptsToRepCPtsMap[cpts] = repCPts;
}

/*!
 * Partition memory regions
 */
void MRGenerator::partitionMRs()
{

    /// Compute all superset of all condition points-to sets
    /// TODO: we may need some refined region partitioning algorithm here
    /// For now, we just collapse all refs/mods objects at callsites into one region
    /// Consider modularly partition memory regions to speed up analysis (only partition regions within function scope)
    for(auto & it : getFunToPointsToList())
    {
        for(const auto & cit : it.second)
        {
            sortPointsTo(cit);
        }
    }
    /// Generate memory regions according to condition pts after computing superset
    for(auto & it : getFunToPointsToList())
    {
        const SVFFunction* fun = it.first;
        for(const auto & cit : it.second)
        {
            createMR(fun,cit);
        }
    }

}

/*!
 * Update aliased regions for loads/stores/callsites
 */
void MRGenerator::updateAliasMRs()
{

    /// update stores with its aliased regions
    for(const auto & it : storesToPointsToMap)
    {
        MRSet aliasMRs;
        const SVFFunction* fun = getFunction(it.first);
        const PointsTo& storeCPts = it.second;
        getAliasMemRegions(aliasMRs,storeCPts,fun);
        for(const auto *aliasMR : aliasMRs)
        {
            storesToMRsMap[it.first].insert(aliasMR);
        }
    }

    for(const auto & it : loadsToPointsToMap)
    {
        MRSet aliasMRs;
        const SVFFunction* fun = getFunction(it.first);
        const PointsTo& loadCPts = it.second;
        getMRsForLoad(aliasMRs, loadCPts, fun);
        for(const auto *aliasMR : aliasMRs)
        {
            loadsToMRsMap[it.first].insert(aliasMR);
        }
    }

    /// update callsites with its aliased regions
    for(const auto & it : callsiteToModPointsToMap)
    {
        const SVFFunction* fun = it.first->getCaller();
        MRSet aliasMRs;
        const PointsTo& callsiteModCPts = it.second;
        getAliasMemRegions(aliasMRs,callsiteModCPts,fun);
        for(const auto *aliasMR : aliasMRs)
        {
            callsiteToModMRsMap[it.first].insert(aliasMR);
        }
    }
    for(const auto & it : callsiteToRefPointsToMap)
    {
        const SVFFunction* fun = it.first->getCaller();
        MRSet aliasMRs;
        const PointsTo& callsiteRefCPts = it.second;
        getMRsForCallSiteRef(aliasMRs, callsiteRefCPts, fun);
        for(const auto *aliasMR : aliasMRs)
        {
            callsiteToRefMRsMap[it.first].insert(aliasMR);
        }
    }
}


/*!
 * Add indirect uses an memory object in the function
 */
void MRGenerator::addRefSideEffectOfFunction(const SVFFunction* fun, const NodeBS& refs)
{
    for(const auto& ref : refs)
    {
        if(isNonLocalObject(ref,fun))
            funToRefsMap[fun].set(ref);
    }
}

/*!
 * Add indirect def an memory object in the function
 */
void MRGenerator::addModSideEffectOfFunction(const SVFFunction* fun, const NodeBS& mods)
{
    for(const auto& mod : mods)
    {
        if(isNonLocalObject(mod,fun))
            funToModsMap[fun].set(mod);
    }
}

/*!
 * Add indirect uses an memory object in the function
 */
bool MRGenerator::addRefSideEffectOfCallSite(const CallBlockNode* cs, const NodeBS& refs)
{
    if(!refs.empty())
    {
        NodeBS refset = refs;
        refset &= getCallSiteArgsPts(cs);
        getEscapObjviaGlobals(refset,refs);
        addRefSideEffectOfFunction(cs->getCaller(),refset);
        return csToRefsMap[cs] |= refset;
    }
    return false;
}

/*!
 * Add indirect def an memory object in the function
 */
bool MRGenerator::addModSideEffectOfCallSite(const CallBlockNode* cs, const NodeBS& mods)
{
    if(!mods.empty())
    {
        NodeBS modset = mods;
        modset &= (getCallSiteArgsPts(cs) | getCallSiteRetPts(cs));
        getEscapObjviaGlobals(modset,mods);
        addModSideEffectOfFunction(cs->getCaller(),modset);
        return csToModsMap[cs] |= modset;
    }
    return false;
}


/*!
 * Get the reverse topo order of scc call graph
 */
void MRGenerator::getCallGraphSCCRevTopoOrder(WorkList& worklist)
{

    NodeStack& topoOrder = callGraphSCC->topoNodeStack();
    while(!topoOrder.empty())
    {
        NodeID callgraphNodeID = topoOrder.top();
        topoOrder.pop();
        worklist.push(callgraphNodeID);
    }
}

/*!
 * Get all objects might pass into and pass out of callee(s) from a callsite
 */
void MRGenerator::collectCallSitePts(const CallBlockNode* cs)
{
    /// collect the pts chain of the callsite arguments
    NodeBS& argsPts = csToCallSiteArgsPtsMap[cs];
    PAG* pag = pta->getPAG();
    CallBlockNode* callBlockNode = pag->getICFG()->getCallBlockNode(cs->getCallSite());
    RetBlockNode* retBlockNode = pag->getICFG()->getRetBlockNode(cs->getCallSite());

    WorkList worklist;
    if (pag->hasCallSiteArgsMap(callBlockNode))
    {
        const PAG::PAGNodeList& args = pta->getPAG()->getCallSiteArgsList(callBlockNode);
        for(const auto *node : args)
        {
             if(node->isPointer())
                worklist.push(node->getId());
        }
    }

    while(!worklist.empty())
    {
        NodeID nodeId = worklist.pop();
        const PointsTo& tmp = pta->getPts(nodeId);
        for(const auto &it : tmp)
            argsPts |= CollectPtsChain(it);
    }

    /// collect the pts chain of the return argument
    NodeBS& retPts = csToCallSiteRetPtsMap[cs];

    if (pta->getPAG()->callsiteHasRet(retBlockNode))
    {
        const PAGNode* node = pta->getPAG()->getCallSiteRet(retBlockNode);
        if(node->isPointer())
        {
            const PointsTo& tmp = pta->getPts(node->getId());
            for(const auto& it : tmp)
                retPts |= CollectPtsChain(it);
        }
    }

}


/*!
 * Recurisively collect all points-to of the whole struct fields
 */
NodeBS& MRGenerator::CollectPtsChain(NodeID id)
{
    NodeID baseId = pta->getPAG()->getBaseObjNode(id);
    auto it = cachedPtsChainMap.find(baseId);
    if(it!=cachedPtsChainMap.end())
        return it->second;

    PointsTo& pts = cachedPtsChainMap[baseId];
    pts |= pta->getPAG()->getFieldsAfterCollapse(baseId);

    WorkList worklist;
    for(const auto& pt : pts)
        worklist.push(pt);

    while(!worklist.empty())
    {
        NodeID nodeId = worklist.pop();
        const PointsTo& tmp = pta->getPts(nodeId);
        for(const auto& it : tmp)
        {
            pts |= CollectPtsChain(it);
        }
    }
    return pts;
}

/*!
 * Get all the objects in callee's modref escaped via global objects (the chain pts of globals)
 * Otherwise, the object in callee's modref would not escape through globals
 */

void MRGenerator::getEscapObjviaGlobals(NodeBS& globs, const NodeBS& calleeModRef)
{
    for(const auto& it : calleeModRef)
    {
        const MemObj* obj = pta->getPAG()->getObject(it);
        assert(obj && "object not found!!");
        if(allGlobals.test(it))
            globs.set(it);
    }
}

/*!
 * Whether the object node is a non-local object
 * including global, heap, and stack variable in recursions
 */
bool MRGenerator::isNonLocalObject(NodeID id, const SVFFunction* curFun) const
{
    const MemObj* obj = pta->getPAG()->getObject(id);
    assert(obj && "object not found!!");
    /// if the object is heap or global
    if(obj->isGlobalObj() || obj->isHeap())
        return true;
    /// or if the local variable of its callers
    /// or a local variable is in function recursion cycles
    else if(obj->isStack())
    {
        if(const auto* local = SVFUtil::dyn_cast<AllocaInst>(obj->getRefVal()))
        {
            const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(local->getFunction());
            if(fun!=curFun)
                return true;

            return callGraphSCC->isInCycle(callGraph->getCallGraphNode(fun)->getId());
        }
    }

    return false;
}

/*!
 * Get Mod-Ref of a callee function
 */
bool MRGenerator::handleCallsiteModRef(NodeBS& mod, NodeBS& ref, const CallBlockNode* cs, const SVFFunction* callee)
{
    /// if a callee is a heap allocator function, then its mod set of this callsite is the heap object.
    if(isHeapAllocExtCall(cs->getCallSite()))
    {
        PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        for (const auto *edge : pagEdgeList)
        {
             if (const auto* addr = SVFUtil::dyn_cast<AddrPE>(edge))
                mod.set(addr->getSrcID());
        }
    }
    /// otherwise, we find the mod/ref sets from the callee function, who has definition and been processed
    else
    {
        mod = getModSideEffectOfFunction(callee);
        ref = getRefSideEffectOfFunction(callee);
    }
    // add ref set
    bool refchanged = addRefSideEffectOfCallSite(cs, ref);
    // add mod set
    bool modchanged = addModSideEffectOfCallSite(cs, mod);

    return refchanged || modchanged;
}

/*!
 * Call site mod-ref analysis
 * Compute mod-ref of all callsites invoking this call graph node
 */
void MRGenerator::modRefAnalysis(PTACallGraphNode* callGraphNode, WorkList& worklist)
{

    /// add ref/mod set of callee to its invocation callsites at caller
    for(auto it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        PTACallGraphEdge* edge = *it;

        /// handle direct callsites
        for(auto cit = edge->getDirectCalls().begin(),
                ecit = edge->getDirectCalls().end(); cit!=ecit; ++cit)
        {
            NodeBS mod;
            NodeBS ref;
            const CallBlockNode* cs = (*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
        /// handle indirect callsites
        for(auto cit = edge->getIndirectCalls().begin(),
                ecit = edge->getIndirectCalls().end(); cit!=ecit; ++cit)
        {
            NodeBS mod;
            NodeBS ref;
            const CallBlockNode* cs = (*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
    }
}

/*!
 * Obtain the mod sets for a call, used for external ModRefInfo queries
 */
PointsTo MRGenerator::getModInfoForCall(const CallBlockNode* cs)
{
    if (isExtCall(cs->getCallSite()) && !isHeapAllocExtCall(cs->getCallSite()))
    {
        PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        PointsTo mods;
        for (const auto *edge : pagEdgeList)
        {
             if (const auto* st = SVFUtil::dyn_cast<StorePE>(edge))
                mods |= pta->getPts(st->getDstID());
        }
        return mods;
    }
    return getModSideEffectOfCallSite(cs);
}

/*!
 * Obtain the ref sets for a call, used for external ModRefInfo queries
 */
PointsTo MRGenerator::getRefInfoForCall(const CallBlockNode* cs)
{
    if (isExtCall(cs->getCallSite()) && !isHeapAllocExtCall(cs->getCallSite()))
    {
        PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        PointsTo refs;
        for (const auto *edge : pagEdgeList)
        {
             if (const auto* ld = SVFUtil::dyn_cast<LoadPE>(edge))
                 refs |= pta->getPts(ld->getSrcID());
        }
        return refs;
    }
    return getRefSideEffectOfCallSite(cs);
}

/*!
 * Determine whether a CallSite instruction can mod or ref
 * any memory location
 */
ModRefInfo MRGenerator::getModRefInfo(const CallBlockNode* cs)
{
    bool ref = !getRefInfoForCall(cs).empty();
    bool mod = !getModInfoForCall(cs).empty();

    if (mod && ref)
        return ModRefInfo::ModRef;

    if (ref)
        return ModRefInfo::Ref;

    if (mod)
        return ModRefInfo::Mod;

    return ModRefInfo::NoModRef;
}

/*!
 * Determine whether a const CallBlockNode* instruction can mod or ref
 * a specific memory location pointed by V
 */
ModRefInfo MRGenerator::getModRefInfo(const CallBlockNode* cs, const Value* V)
{
    bool ref = false;
    bool mod = false;

    if (pta->getPAG()->hasValueNode(V))
    {
        const PointsTo pts(pta->getPts(pta->getPAG()->getValueNode(V)));
        const PointsTo csRef = getRefInfoForCall(cs);
        const PointsTo csMod = getModInfoForCall(cs);
        PointsTo ptsExpanded;
        PointsTo csRefExpanded;
        PointsTo csModExpanded;
        pta->expandFIObjs(pts, ptsExpanded);
        pta->expandFIObjs(csRef, csRefExpanded);
        pta->expandFIObjs(csMod, csModExpanded);

        if (csRefExpanded.intersects(ptsExpanded))
            ref = true;
        if (csModExpanded.intersects(ptsExpanded))
            mod = true;
    }

    if (mod && ref)
        return ModRefInfo::ModRef;
    if (ref)
        return ModRefInfo::Ref;
    if (mod)
        return ModRefInfo::Mod;

    return ModRefInfo::NoModRef;
}

/*!
 * Determine mod-ref relations between two const CallBlockNode* instructions
 */
ModRefInfo MRGenerator::getModRefInfo(const CallBlockNode* cs1, const CallBlockNode* cs2)
{
    bool ref = false;
    bool mod = false;

    /// return NoModRef neither two callsites ref or mod any memory
    if (getModRefInfo(cs1) == ModRefInfo::NoModRef || getModRefInfo(cs2) == ModRefInfo::NoModRef)
        return ModRefInfo::NoModRef;

    const PointsTo cs1Ref = getRefInfoForCall(cs1);
    const PointsTo cs1Mod = getModInfoForCall(cs1);
    const PointsTo cs2Ref = getRefInfoForCall(cs2);
    const PointsTo cs2Mod = getModInfoForCall(cs2);
    PointsTo cs1RefExpanded;
    PointsTo cs1ModExpanded;
    PointsTo cs2RefExpanded;
    PointsTo cs2ModExpanded;
    pta->expandFIObjs(cs1Ref, cs1RefExpanded);
    pta->expandFIObjs(cs1Mod, cs1ModExpanded);
    pta->expandFIObjs(cs2Ref, cs2RefExpanded);
    pta->expandFIObjs(cs2Mod, cs2ModExpanded);

    /// Ref: cs1 ref memory mod by cs2
    if (cs1RefExpanded.intersects(cs2ModExpanded))
        ref = true;
    /// Mod: cs1 mod memory ref or mod by cs2
    if (cs1ModExpanded.intersects(cs2RefExpanded) || cs1ModExpanded.intersects(cs2ModExpanded))
        mod = true;
    /// ModRef: cs1 ref and mod memory mod by cs2
    if (cs1RefExpanded.intersects(cs2ModExpanded) && cs1ModExpanded.intersects(cs2ModExpanded))
        ref = mod = true;

    if (ref && mod)
        return ModRefInfo::ModRef;

    if (ref)
        return ModRefInfo::Ref;

    if (mod)
        return ModRefInfo::Mod;

    return ModRefInfo::NoModRef;
}
