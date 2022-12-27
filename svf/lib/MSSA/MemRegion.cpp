//===- MemRegion.cpp -- Memory region-----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * MemRegion.cpp
 *
 *  Created on: Dec 14, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVFIR/SVFModule.h"
#include "MSSA/MemRegion.h"
#include "MSSA/MSSAMuChi.h"

using namespace SVF;
using namespace SVFUtil;

u32_t MemRegion::totalMRNum = 0;
u32_t MRVer::totalVERNum = 0;

MRGenerator::MRGenerator(BVDataPTAImpl* p, bool ptrOnly) :
    pta(p), ptrOnlyMSSA(ptrOnly)
{
    callGraph = pta->getPTACallGraph();
    callGraphSCC = new SCC(callGraph);
}

/*!
 * Clean up memory
 */
void MRGenerator::destroy()
{

    for (MRSet::iterator it = memRegSet.begin(), eit = memRegSet.end();
            it != eit; ++it)
    {
        delete *it;
    }

    delete callGraphSCC;
    callGraphSCC = nullptr;
    callGraph = nullptr;
    pta = nullptr;
}

/*!
 * Generate a memory region and put in into functions which use it
 */
void MRGenerator::createMR(const SVFFunction* fun, const NodeBS& cpts)
{
    const NodeBS& repCPts = getRepPointsTo(cpts);
    MemRegion mr(repCPts);
    MRSet::const_iterator mit = memRegSet.find(&mr);
    if(mit!=memRegSet.end())
    {
        const MemRegion* mr = *mit;
        MRSet& mrs = funToMRsMap[fun];
        if(mrs.find(mr)==mrs.end())
            mrs.insert(mr);
    }
    else
    {
        MemRegion* m = new MemRegion(repCPts);
        memRegSet.insert(m);
        funToMRsMap[fun].insert(m);
    }
}

/*!
 * Generate a memory region and put in into functions which use it
 */
const MemRegion* MRGenerator::getMR(const NodeBS& cpts) const
{
    MemRegion mr(getRepPointsTo(cpts));
    MRSet::iterator mit = memRegSet.find(&mr);
    assert(mit!=memRegSet.end() && "memory region not found!!");
    return *mit;
}


/*!
 * Collect globals for escape analysis
 */
void MRGenerator::collectGlobals()
{
    SVFIR* pag = pta->getPAG();
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
        if(ObjVar* obj = SVFUtil::dyn_cast<ObjVar>(nIter->second))
        {
            if (obj->getMemObj()->isGlobalObj())
            {
                allGlobals.set(nIter->first);
                allGlobals |= CollectPtsChain(nIter->first);
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

    DBOUT(DGENERAL, outs() << pasMsg("\tCollect ModRef For const CallICFGNode*\n"));

    /// collect mod-ref for calls
    collectModRefForCall();

    DBOUT(DGENERAL, outs() << pasMsg("\tPartition Memory Regions \n"));
    /// Partition memory regions
    partitionMRs();
    /// attach memory regions for loads/stores/calls
    updateAliasMRs();
}

bool MRGenerator::hasSVFStmtList(const SVFInstruction* inst)
{
    SVFIR* pag = pta->getPAG();
    if (ptrOnlyMSSA)
        return pag->hasPTASVFStmtList(pag->getICFG()->getICFGNode(inst));
    else
        return pag->hasSVFStmtList(pag->getICFG()->getICFGNode(inst));
}

SVFIR::SVFStmtList& MRGenerator::getPAGEdgesFromInst(const SVFInstruction* inst)
{
    SVFIR* pag = pta->getPAG();
    if (ptrOnlyMSSA)
        return pag->getPTASVFStmtList(pag->getICFG()->getICFGNode(inst));
    else
        return pag->getSVFStmtList(pag->getICFG()->getICFGNode(inst));
}

/*!
 * Generate memory regions for loads/stores
 */
void MRGenerator::collectModRefForLoadStore()
{

    SVFModule* svfModule = pta->getModule();
    for (SVFModule::const_iterator fi = svfModule->begin(), efi = svfModule->end(); fi != efi;
            ++fi)
    {
        const SVFFunction& fun = **fi;

        /// if this function does not have any caller, then we do not care its MSSA
        if (Options::IgnoreDeadFun() && fun.isUncalledFunction())
            continue;

        for (SVFFunction::const_iterator iter = fun.begin(), eiter = fun.end();
                iter != eiter; ++iter)
        {
            const SVFBasicBlock* bb = *iter;
            for (SVFBasicBlock::const_iterator bit = bb->begin(), ebit = bb->end();
                    bit != ebit; ++bit)
            {
                const SVFInstruction* svfInst = *bit;
                SVFStmtList& pagEdgeList = getPAGEdgesFromInst(svfInst);
                for (SVFStmtList::iterator bit = pagEdgeList.begin(), ebit =
                            pagEdgeList.end(); bit != ebit; ++bit)
                {
                    const PAGEdge* inst = *bit;
                    pagEdgeToFunMap[inst] = &fun;
                    if (const StoreStmt *st = SVFUtil::dyn_cast<StoreStmt>(inst))
                    {
                        NodeBS cpts(pta->getPts(st->getLHSVarID()).toNodeBS());
                        // TODO: change this assertion check later when we have conditional points-to set
                        if (cpts.empty())
                            continue;
                        assert(!cpts.empty() && "null pointer!!");
                        addCPtsToStore(cpts, st, &fun);
                    }

                    else if (const LoadStmt *ld = SVFUtil::dyn_cast<LoadStmt>(inst))
                    {
                        NodeBS cpts(pta->getPts(ld->getRHSVarID()).toNodeBS());
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
    for(SVFIR::CallSiteSet::const_iterator it =  pta->getPAG()->getCallSiteSet().begin(),
            eit = pta->getPAG()->getCallSiteSet().end(); it!=eit; ++it)
    {
        collectCallSitePts((*it));
    }

    DBOUT(DGENERAL, outs() << pasMsg("\t\tPerform Callsite Mod-Ref \n"));

    WorkList worklist;
    getCallGraphSCCRevTopoOrder(worklist);

    while(!worklist.empty())
    {
        NodeID callGraphNodeID = worklist.pop();
        /// handle all sub scc nodes of this rep node
        const NodeBS& subNodes = callGraphSCC->subNodes(callGraphNodeID);
        for(NodeBS::iterator it = subNodes.begin(), eit = subNodes.end(); it!=eit; ++it)
        {
            PTACallGraphNode* subCallGraphNode = callGraph->getCallGraphNode(*it);
            /// Get mod-ref of all callsites calling callGraphNode
            modRefAnalysis(subCallGraphNode,worklist);
        }
    }

    DBOUT(DGENERAL, outs() << pasMsg("\t\tAdd PointsTo to Callsites \n"));

    for (const CallICFGNode* callBlockNode : pta->getPAG()->getCallSiteSet())
    {
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
void MRGenerator::sortPointsTo(const NodeBS& cpts)
{

    if(cptsToRepCPtsMap.find(cpts)!=cptsToRepCPtsMap.end())
        return;

    PointsToList subSetList;
    NodeBS repCPts = cpts;
    for(PtsToRepPtsSetMap::iterator it = cptsToRepCPtsMap.begin(),
            eit = cptsToRepCPtsMap.end(); it!=eit; ++it)
    {
        NodeBS& existCPts = it->second;
        if(cpts.contains(existCPts))
        {
            subSetList.insert(it->first);
        }
        else if(existCPts.contains(cpts))
        {
            repCPts = existCPts;
        }
    }

    for(PointsToList::iterator it = subSetList.begin(), eit = subSetList.end(); it!=eit; ++it)
    {
        cptsToRepCPtsMap[*it] = cpts;
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
    for(FunToPointsTosMap::iterator it = getFunToPointsToList().begin(), eit = getFunToPointsToList().end();
            it!=eit; ++it)
    {
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            sortPointsTo(*cit);
        }
    }
    /// Generate memory regions according to condition pts after computing superset
    for(FunToPointsTosMap::iterator it = getFunToPointsToList().begin(), eit = getFunToPointsToList().end();
            it!=eit; ++it)
    {
        const SVFFunction* fun = it->first;
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            createMR(fun,*cit);
        }
    }

}

/*!
 * Update aliased regions for loads/stores/callsites
 */
void MRGenerator::updateAliasMRs()
{

    /// update stores with its aliased regions
    for(StoresToPointsToMap::const_iterator it = storesToPointsToMap.begin(), eit = storesToPointsToMap.end(); it!=eit; ++it)
    {
        MRSet aliasMRs;
        const SVFFunction* fun = getFunction(it->first);
        const NodeBS& storeCPts = it->second;
        getAliasMemRegions(aliasMRs,storeCPts,fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait)
        {
            storesToMRsMap[it->first].insert(*ait);
        }
    }

    for(LoadsToPointsToMap::const_iterator it = loadsToPointsToMap.begin(), eit = loadsToPointsToMap.end(); it!=eit; ++it)
    {
        MRSet aliasMRs;
        const SVFFunction* fun = getFunction(it->first);
        const NodeBS& loadCPts = it->second;
        getMRsForLoad(aliasMRs, loadCPts, fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait)
        {
            loadsToMRsMap[it->first].insert(*ait);
        }
    }

    /// update callsites with its aliased regions
    for(CallSiteToPointsToMap::const_iterator it =  callsiteToModPointsToMap.begin(),
            eit = callsiteToModPointsToMap.end(); it!=eit; ++it)
    {
        const SVFFunction* fun = it->first->getCaller();
        MRSet aliasMRs;
        const NodeBS& callsiteModCPts = it->second;
        getAliasMemRegions(aliasMRs,callsiteModCPts,fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait)
        {
            callsiteToModMRsMap[it->first].insert(*ait);
        }
    }
    for(CallSiteToPointsToMap::const_iterator it =  callsiteToRefPointsToMap.begin(),
            eit = callsiteToRefPointsToMap.end(); it!=eit; ++it)
    {
        const SVFFunction* fun = it->first->getCaller();
        MRSet aliasMRs;
        const NodeBS& callsiteRefCPts = it->second;
        getMRsForCallSiteRef(aliasMRs, callsiteRefCPts, fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait)
        {
            callsiteToRefMRsMap[it->first].insert(*ait);
        }
    }
}


/*!
 * Add indirect uses an memory object in the function
 */
void MRGenerator::addRefSideEffectOfFunction(const SVFFunction* fun, const NodeBS& refs)
{
    for(NodeBS::iterator it = refs.begin(), eit = refs.end(); it!=eit; ++it)
    {
        if(isNonLocalObject(*it,fun))
            funToRefsMap[fun].set(*it);
    }
}

/*!
 * Add indirect def an memory object in the function
 */
void MRGenerator::addModSideEffectOfFunction(const SVFFunction* fun, const NodeBS& mods)
{
    for(NodeBS::iterator it = mods.begin(), eit = mods.end(); it!=eit; ++it)
    {
        if(isNonLocalObject(*it,fun))
            funToModsMap[fun].set(*it);
    }
}

/*!
 * Add indirect uses an memory object in the function
 */
bool MRGenerator::addRefSideEffectOfCallSite(const CallICFGNode* cs, const NodeBS& refs)
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
bool MRGenerator::addModSideEffectOfCallSite(const CallICFGNode* cs, const NodeBS& mods)
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
void MRGenerator::collectCallSitePts(const CallICFGNode* cs)
{
    /// collect the pts chain of the callsite arguments
    NodeBS& argsPts = csToCallSiteArgsPtsMap[cs];
    SVFIR* pag = pta->getPAG();
    CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(cs->getCallSite());
    RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(cs->getCallSite());

    WorkList worklist;
    if (pag->hasCallSiteArgsMap(callBlockNode))
    {
        const SVFIR::SVFVarList& args = pta->getPAG()->getCallSiteArgsList(callBlockNode);
        for(SVFIR::SVFVarList::const_iterator itA = args.begin(), ieA = args.end(); itA!=ieA; ++itA)
        {
            const PAGNode* node = *itA;
            if(node->isPointer())
                worklist.push(node->getId());
        }
    }

    while(!worklist.empty())
    {
        NodeID nodeId = worklist.pop();
        const NodeBS& tmp = pta->getPts(nodeId).toNodeBS();
        for(NodeBS::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
            argsPts |= CollectPtsChain(*it);
    }

    /// collect the pts chain of the return argument
    NodeBS& retPts = csToCallSiteRetPtsMap[cs];

    if (pta->getPAG()->callsiteHasRet(retBlockNode))
    {
        const PAGNode* node = pta->getPAG()->getCallSiteRet(retBlockNode);
        if(node->isPointer())
        {
            const NodeBS& tmp = pta->getPts(node->getId()).toNodeBS();
            for(NodeBS::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
                retPts |= CollectPtsChain(*it);
        }
    }

}


/*!
 * Recurisively collect all points-to of the whole struct fields
 */
NodeBS& MRGenerator::CollectPtsChain(NodeID id)
{
    NodeID baseId = pta->getPAG()->getBaseObjVar(id);
    NodeToPTSSMap::iterator it = cachedPtsChainMap.find(baseId);
    if(it!=cachedPtsChainMap.end())
        return it->second;
    else
    {
        NodeBS& pts = cachedPtsChainMap[baseId];
        pts |= pta->getPAG()->getFieldsAfterCollapse(baseId);

        WorkList worklist;
        for(NodeBS::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
            worklist.push(*it);

        while(!worklist.empty())
        {
            NodeID nodeId = worklist.pop();
            const NodeBS& tmp = pta->getPts(nodeId).toNodeBS();
            for(NodeBS::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
            {
                pts |= CollectPtsChain(*it);
            }
        }
        return pts;
    }

}

/*!
 * Get all the objects in callee's modref escaped via global objects (the chain pts of globals)
 * Otherwise, the object in callee's modref would not escape through globals
 */

void MRGenerator::getEscapObjviaGlobals(NodeBS& globs, const NodeBS& calleeModRef)
{
    for(NodeBS::iterator it = calleeModRef.begin(), eit = calleeModRef.end(); it!=eit; ++it)
    {
        const MemObj* obj = pta->getPAG()->getObject(*it);
        (void)obj; // Suppress warning of unused variable under release build
        assert(obj && "object not found!!");
        if(allGlobals.test(*it))
            globs.set(*it);
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
        if(const SVFFunction* svffun = pta->getPAG()->getGNode(id)->getFunction())
        {
            if(svffun!=curFun)
                return true;
            else
                return callGraphSCC->isInCycle(callGraph->getCallGraphNode(svffun)->getId());
        }
    }

    return false;
}

/*!
 * Get Mod-Ref of a callee function
 */
bool MRGenerator::handleCallsiteModRef(NodeBS& mod, NodeBS& ref, const CallICFGNode* cs, const SVFFunction* callee)
{
    /// if a callee is a heap allocator function, then its mod set of this callsite is the heap object.
    if(isHeapAllocExtCall(cs->getCallSite()))
    {
        SVFStmtList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        for (SVFStmtList::const_iterator bit = pagEdgeList.begin(),
                ebit = pagEdgeList.end(); bit != ebit; ++bit)
        {
            const PAGEdge* edge = *bit;
            if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(edge))
                mod.set(addr->getRHSVarID());
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
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        PTACallGraphEdge* edge = *it;

        /// handle direct callsites
        for(PTACallGraphEdge::CallInstSet::iterator cit = edge->getDirectCalls().begin(),
                ecit = edge->getDirectCalls().end(); cit!=ecit; ++cit)
        {
            NodeBS mod, ref;
            const CallICFGNode* cs = (*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
        /// handle indirect callsites
        for(PTACallGraphEdge::CallInstSet::iterator cit = edge->getIndirectCalls().begin(),
                ecit = edge->getIndirectCalls().end(); cit!=ecit; ++cit)
        {
            NodeBS mod, ref;
            const CallICFGNode* cs = (*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
    }
}

/*!
 * Obtain the mod sets for a call, used for external ModRefInfo queries
 */
NodeBS MRGenerator::getModInfoForCall(const CallICFGNode* cs)
{
    if (isExtCall(cs->getCallSite()) && !isHeapAllocExtCall(cs->getCallSite()))
    {
        SVFStmtList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        NodeBS mods;
        for (SVFStmtList::const_iterator bit = pagEdgeList.begin(), ebit =
                    pagEdgeList.end(); bit != ebit; ++bit)
        {
            const PAGEdge* edge = *bit;
            if (const StoreStmt* st = SVFUtil::dyn_cast<StoreStmt>(edge))
                mods |= pta->getPts(st->getLHSVarID()).toNodeBS();
        }
        return mods;
    }
    else
    {
        return getModSideEffectOfCallSite(cs);
    }
}

/*!
 * Obtain the ref sets for a call, used for external ModRefInfo queries
 */
NodeBS MRGenerator::getRefInfoForCall(const CallICFGNode* cs)
{
    if (isExtCall(cs->getCallSite()) && !isHeapAllocExtCall(cs->getCallSite()))
    {
        SVFStmtList& pagEdgeList = getPAGEdgesFromInst(cs->getCallSite());
        NodeBS refs;
        for (SVFStmtList::const_iterator bit = pagEdgeList.begin(), ebit =
                    pagEdgeList.end(); bit != ebit; ++bit)
        {
            const PAGEdge* edge = *bit;
            if (const LoadStmt* ld = SVFUtil::dyn_cast<LoadStmt>(edge))
                refs |= pta->getPts(ld->getRHSVarID()).toNodeBS();
        }
        return refs;
    }
    else
    {
        return getRefSideEffectOfCallSite(cs);
    }
}

/*!
 * Determine whether a CallSite instruction can mod or ref
 * any memory location
 */
ModRefInfo MRGenerator::getModRefInfo(const CallICFGNode* cs)
{
    bool ref = !getRefInfoForCall(cs).empty();
    bool mod = !getModInfoForCall(cs).empty();

    if (mod && ref)
        return ModRefInfo::ModRef;
    else if (ref)
        return ModRefInfo::Ref;
    else if (mod)
        return ModRefInfo::Mod;
    else
        return ModRefInfo::NoModRef;
}

/*!
 * Determine whether a const CallICFGNode* instruction can mod or ref
 * a specific memory location pointed by V
 */
ModRefInfo MRGenerator::getModRefInfo(const CallICFGNode* cs, const SVFValue* V)
{
    bool ref = false;
    bool mod = false;

    if (pta->getPAG()->hasValueNode(V))
    {
        const NodeBS pts(pta->getPts(pta->getPAG()->getValueNode(V)).toNodeBS());
        const NodeBS csRef = getRefInfoForCall(cs);
        const NodeBS csMod = getModInfoForCall(cs);
        NodeBS ptsExpanded, csRefExpanded, csModExpanded;
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
    else if (ref)
        return ModRefInfo::Ref;
    else if (mod)
        return ModRefInfo::Mod;
    else
        return ModRefInfo::NoModRef;
}

/*!
 * Determine mod-ref relations between two const CallICFGNode* instructions
 */
ModRefInfo MRGenerator::getModRefInfo(const CallICFGNode* cs1, const CallICFGNode* cs2)
{
    bool ref = false;
    bool mod = false;

    /// return NoModRef neither two callsites ref or mod any memory
    if (getModRefInfo(cs1) == ModRefInfo::NoModRef || getModRefInfo(cs2) == ModRefInfo::NoModRef)
        return ModRefInfo::NoModRef;

    const NodeBS cs1Ref = getRefInfoForCall(cs1);
    const NodeBS cs1Mod = getModInfoForCall(cs1);
    const NodeBS cs2Ref = getRefInfoForCall(cs2);
    const NodeBS cs2Mod = getModInfoForCall(cs2);
    NodeBS cs1RefExpanded, cs1ModExpanded, cs2RefExpanded, cs2ModExpanded;
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
    else if (ref)
        return ModRefInfo::Ref;
    else if (mod)
        return ModRefInfo::Mod;
    else
        return ModRefInfo::NoModRef;
}

std::ostream& SVF::operator<<(std::ostream &o, const MRVer& mrver)
{
    o << "MRVERID: " << mrver.getID() <<" MemRegion: " << mrver.getMR()->dumpStr() << " MRVERSION: " << mrver.getSSAVersion() << " MSSADef: " << mrver.getDef()->getType() << ", "
      << mrver.getDef()->getMR()->dumpStr() ;
    return o;
}
