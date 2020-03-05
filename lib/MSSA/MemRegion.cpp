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

#include "MSSA/MemRegion.h"
#include "MSSA/MSSAMuChi.h"
#include "Util/SVFUtil.h"
#include "Util/SVFModule.h"

using namespace SVFUtil;

Size_t MemRegion::totalMRNum = 0;
Size_t MRVer::totalVERNum = 0;

static llvm::cl::opt<bool> IgnoreDeadFun("mssa-ignoreDeadFun", llvm::cl::init(false),
                                   llvm::cl::desc("Don't construct memory SSA for deadfunction"));

/*!
 * Clean up memory
 */
void MRGenerator::destroy() {

    for (MRSet::iterator it = memRegSet.begin(), eit = memRegSet.end();
            it != eit; ++it) {
        delete *it;
    }

    delete callGraphSCC;
    callGraphSCC = NULL;
    callGraph = NULL;
    pta = NULL;
}

/*!
 * Generate a memory region and put in into functions which use it
 */
void MRGenerator::createMR(const Function* fun, const PointsTo& cpts) {
    const PointsTo& repCPts = getRepPointsTo(cpts);
    MemRegion mr(repCPts);
    MRSet::const_iterator mit = memRegSet.find(&mr);
    if(mit!=memRegSet.end()) {
        const MemRegion* mr = *mit;
        MRSet& mrs = funToMRsMap[fun];
        if(mrs.find(mr)==mrs.end())
            mrs.insert(mr);
    }
    else {
        MemRegion* m = new MemRegion(repCPts);
        memRegSet.insert(m);
        funToMRsMap[fun].insert(m);
    }
}

/*!
 * Generate a memory region and put in into functions which use it
 */
const MemRegion* MRGenerator::getMR(const PointsTo& cpts) const {
    MemRegion mr(getRepPointsTo(cpts));
    MRSet::iterator mit = memRegSet.find(&mr);
    assert(mit!=memRegSet.end() && "memory region not found!!");
    return *mit;
}


/*!
 * Collect globals for escape analysis
 */
void MRGenerator::collectGlobals() {
    PAG* pag = pta->getPAG();
    for (PAG::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter) {
        if(ObjPN* obj = SVFUtil::dyn_cast<ObjPN>(nIter->second)) {
            if (obj->getMemObj()->isGlobalObj()) {
                allGlobals.set(nIter->getFirst());
                allGlobals |= CollectPtsChain(nIter->getFirst());
            }
        }
    }
}

/*!
 * Generate memory regions according to pointer analysis results
 * Attach regions on loads/stores
 */
void MRGenerator::generateMRs() {

    DBOUT(DGENERAL, outs() << pasMsg("Generate Memory Regions \n"));

    collectGlobals();

    callGraphSCC->find();

    DBOUT(DGENERAL, outs() << pasMsg("\tCollect ModRef For Load/Store \n"));

    /// collect mod-ref for loads/stores
    collectModRefForLoadStore();

    DBOUT(DGENERAL, outs() << pasMsg("\tCollect ModRef For CallSite\n"));

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
void MRGenerator::collectModRefForLoadStore() {

    SVFModule svfModule = pta->getModule();
    for (SVFModule::iterator fi = svfModule.begin(), efi = svfModule.end(); fi != efi;
            ++fi) {
        const Function& fun = **fi;

        /// if this function does not have any caller, then we do not care its MSSA
        if (IgnoreDeadFun && isDeadFunction(&fun))
            continue;

        for (Function::const_iterator iter = fun.begin(), eiter = fun.end();
                iter != eiter; ++iter) {
            const BasicBlock& bb = *iter;
            for (BasicBlock::const_iterator bit = bb.begin(), ebit = bb.end();
                    bit != ebit; ++bit) {
                const Instruction& inst = *bit;
                PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(&inst);
                for (PAGEdgeList::iterator bit = pagEdgeList.begin(), ebit =
                            pagEdgeList.end(); bit != ebit; ++bit) {
                    const PAGEdge* inst = *bit;
                    pagEdgeToFunMap[inst] = &fun;
                    if (const StorePE *st = SVFUtil::dyn_cast<StorePE>(inst)) {
                        PointsTo cpts(pta->getPts(st->getDstID()));
                        // TODO: change this assertion check later when we have conditional points-to set
                        if (cpts.empty())
                            continue;
                        assert(!cpts.empty() && "null pointer!!");
                        addCPtsToStore(cpts, st, &fun);
                    }

                    else if (const LoadPE *ld = SVFUtil::dyn_cast<LoadPE>(inst)) {
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
void MRGenerator::collectModRefForCall() {

    DBOUT(DGENERAL, outs() << pasMsg("\t\tCollect Callsite PointsTo \n"));

    /// collect points-to information for callsites
    for(PAG::CallSiteSet::const_iterator it =  pta->getPAG()->getCallSiteSet().begin(),
            eit = pta->getPAG()->getCallSiteSet().end(); it!=eit; ++it)
        collectCallSitePts(*it);

    DBOUT(DGENERAL, outs() << pasMsg("\t\tPerform Callsite Mod-Ref \n"));

    WorkList worklist;
    getCallGraphSCCRevTopoOrder(worklist);

    while(!worklist.empty()) {
        NodeID callGraphNodeID = worklist.pop();
        /// handle all sub scc nodes of this rep node
        const NodeBS& subNodes = callGraphSCC->subNodes(callGraphNodeID);
        for(NodeBS::iterator it = subNodes.begin(), eit = subNodes.end(); it!=eit; ++it) {
            PTACallGraphNode* subCallGraphNode = callGraph->getCallGraphNode(*it);
            /// Get mod-ref of all callsites calling callGraphNode
            modRefAnalysis(subCallGraphNode,worklist);
        }
    }

    DBOUT(DGENERAL, outs() << pasMsg("\t\tAdd PointsTo to Callsites \n"));

    for(PAG::CallSiteSet::const_iterator it =  pta->getPAG()->getCallSiteSet().begin(),
            eit = pta->getPAG()->getCallSiteSet().end(); it!=eit; ++it) {
        if(hasRefSideEffectOfCallSite(*it)) {
            NodeBS refs = getRefSideEffectOfCallSite(*it);
            addCPtsToCallSiteRefs(refs,*it);
        }
        if(hasModSideEffectOfCallSite(*it)) {
            NodeBS mods = getModSideEffectOfCallSite(*it);
            /// mods are treated as both def and use of memory objects
            addCPtsToCallSiteMods(mods,*it);
            addCPtsToCallSiteRefs(mods,*it);
        }
    }
}

/*!
 * Given a condition pts, insert into cptsToRepCPtsMap
 * Always map it to its superset(rep) cpts according to existing items
 * 1) map cpts to its superset(rep) which exists in the map, otherwise its superset is itself
 * 2) adjust existing items in the map if their supersets are cpts
 */
void MRGenerator::sortPointsTo(const PointsTo& cpts) {

    if(cptsToRepCPtsMap.find(cpts)!=cptsToRepCPtsMap.end())
        return;

    PointsToList subSetList;
    PointsTo repCPts = cpts;
    for(PtsToRepPtsSetMap::iterator it = cptsToRepCPtsMap.begin(),
            eit = cptsToRepCPtsMap.end(); it!=eit; ++it) {
        PointsTo& existCPts = it->second;
        if(cpts.contains(existCPts)) {
            subSetList.insert(it->first);
        }
        else if(existCPts.contains(cpts)) {
            repCPts = existCPts;
        }
    }

    for(PointsToList::iterator it = subSetList.begin(), eit = subSetList.end(); it!=eit; ++it) {
        cptsToRepCPtsMap[*it] = cpts;
    }

    cptsToRepCPtsMap[cpts] = repCPts;
}

/*!
 * Partition memory regions
 */
void MRGenerator::partitionMRs() {

    /// Compute all superset of all condition points-to sets
    /// TODO: we may need some refined region partitioning algorithm here
    /// For now, we just collapse all refs/mods objects at callsites into one region
    /// Consider modularly partition memory regions to speed up analysis (only partition regions within function scope)
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(), eit = getFunToPointsToList().end();
            it!=eit; ++it) {
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit) {
            sortPointsTo(*cit);
        }
    }
    /// Generate memory regions according to condition pts after computing superset
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(), eit = getFunToPointsToList().end();
            it!=eit; ++it) {
        const Function* fun = it->first;
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit) {
            createMR(fun,*cit);
        }
    }

}

/*!
 * Update aliased regions for loads/stores/callsites
 */
void MRGenerator::updateAliasMRs() {

    /// update stores with its aliased regions
    for(StoresToPointsToMap::const_iterator it = storesToPointsToMap.begin(), eit = storesToPointsToMap.end(); it!=eit; ++it) {
        MRSet aliasMRs;
        const Function* fun = getFunction(it->first);
        const PointsTo& storeCPts = it->second;
        getAliasMemRegions(aliasMRs,storeCPts,fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait) {
            storesToMRsMap[it->first].insert(*ait);
        }
    }

    for(LoadsToPointsToMap::const_iterator it = loadsToPointsToMap.begin(), eit = loadsToPointsToMap.end(); it!=eit; ++it) {
        MRSet aliasMRs;
        const Function* fun = getFunction(it->first);
        const PointsTo& loadCPts = it->second;
        getMRsForLoad(aliasMRs, loadCPts, fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait) {
            loadsToMRsMap[it->first].insert(*ait);
        }
    }

    /// update callsites with its aliased regions
    for(CallSiteToPointsToMap::const_iterator it =  callsiteToModPointsToMap.begin(),
            eit = callsiteToModPointsToMap.end(); it!=eit; ++it) {
        const Function* fun = it->first.getCaller();
        MRSet aliasMRs;
        const PointsTo& callsiteModCPts = it->second;
        getAliasMemRegions(aliasMRs,callsiteModCPts,fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait) {
            callsiteToModMRsMap[it->first].insert(*ait);
        }
    }
    for(CallSiteToPointsToMap::const_iterator it =  callsiteToRefPointsToMap.begin(),
            eit = callsiteToRefPointsToMap.end(); it!=eit; ++it) {
        const Function* fun = it->first.getCaller();
        MRSet aliasMRs;
        const PointsTo& callsiteRefCPts = it->second;
        getMRsForCallSiteRef(aliasMRs, callsiteRefCPts, fun);
        for(MRSet::iterator ait = aliasMRs.begin(), eait = aliasMRs.end(); ait!=eait; ++ait) {
            callsiteToRefMRsMap[it->first].insert(*ait);
        }
    }
}


/*!
 * Add indirect uses an memory object in the function
 */
void MRGenerator::addRefSideEffectOfFunction(const Function* fun, const NodeBS& refs) {
    for(NodeBS::iterator it = refs.begin(), eit = refs.end(); it!=eit; ++it) {
        if(isNonLocalObject(*it,fun))
            funToRefsMap[fun].set(*it);
    }
}

/*!
 * Add indirect def an memory object in the function
 */
void MRGenerator::addModSideEffectOfFunction(const Function* fun, const NodeBS& mods) {
    for(NodeBS::iterator it = mods.begin(), eit = mods.end(); it!=eit; ++it) {
        if(isNonLocalObject(*it,fun))
            funToModsMap[fun].set(*it);
    }
}

/*!
 * Add indirect uses an memory object in the function
 */
bool MRGenerator::addRefSideEffectOfCallSite(CallSite cs, const NodeBS& refs) {
    if(!refs.empty()) {
        NodeBS refset = refs;
        refset &= getCallSiteArgsPts(cs);
        getEscapObjviaGlobals(refset,refs);
        addRefSideEffectOfFunction(cs.getCaller(),refset);
        return csToRefsMap[cs] |= refset;
    }
    return false;
}

/*!
 * Add indirect def an memory object in the function
 */
bool MRGenerator::addModSideEffectOfCallSite(CallSite cs, const NodeBS& mods) {
    if(!mods.empty()) {
        NodeBS modset = mods;
        modset &= (getCallSiteArgsPts(cs) | getCallSiteRetPts(cs));
        getEscapObjviaGlobals(modset,mods);
        addModSideEffectOfFunction(cs.getCaller(),modset);
        return csToModsMap[cs] |= modset;
    }
    return false;
}


/*!
 * Get the reverse topo order of scc call graph
 */
void MRGenerator::getCallGraphSCCRevTopoOrder(WorkList& worklist) {

    NodeStack& topoOrder = callGraphSCC->topoNodeStack();
    while(!topoOrder.empty()) {
        NodeID callgraphNodeID = topoOrder.top();
        topoOrder.pop();
        worklist.push(callgraphNodeID);
    }
}

/*!
 * Get all objects might pass into and pass out of callee(s) from a callsite
 */
void MRGenerator::collectCallSitePts(CallSite cs) {
    /// collect the pts chain of the callsite arguments
    NodeBS& argsPts = csToCallSiteArgsPtsMap[cs];

    WorkList worklist;
    if (pta->getPAG()->hasCallSiteArgsMap(cs)) {
        const PAG::PAGNodeList& args = pta->getPAG()->getCallSiteArgsList(cs);
        for(PAG::PAGNodeList::const_iterator itA = args.begin(), ieA = args.end(); itA!=ieA; ++itA) {
            const PAGNode* node = *itA;
            if(node->isPointer())
                worklist.push(node->getId());
        }
    }

    while(!worklist.empty()) {
        NodeID nodeId = worklist.pop();
        PointsTo& tmp = pta->getPts(nodeId);
        for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
            argsPts |= CollectPtsChain(*it);
    }

    /// collect the pts chain of the return argument
    NodeBS& retPts = csToCallSiteRetPtsMap[cs];

    if (pta->getPAG()->callsiteHasRet(cs)) {
        const PAGNode* node = pta->getPAG()->getCallSiteRet(cs);
        if(node->isPointer()){
            PointsTo& tmp = pta->getPts(node->getId());
            for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it)
                retPts |= CollectPtsChain(*it);
        }
    }

}

/*!
 * Recurisively collect all points-to of the whole struct fields
 */
NodeBS& MRGenerator::CollectPtsChain(NodeID id) {
    NodeID baseId = pta->getPAG()->getBaseObjNode(id);
    NodeToPTSSMap::iterator it = cachedPtsChainMap.find(baseId);
    if(it!=cachedPtsChainMap.end())
        return it->second;
    else {
        PointsTo& pts = cachedPtsChainMap[baseId];
        pts |= pta->getPAG()->getFieldsAfterCollapse(baseId);

        WorkList worklist;
        for(PointsTo::iterator it = pts.begin(), eit = pts.end(); it!=eit; ++it)
            worklist.push(*it);

        while(!worklist.empty()) {
            NodeID nodeId = worklist.pop();
            PointsTo& tmp = pta->getPts(nodeId);
            for(PointsTo::iterator it = tmp.begin(), eit = tmp.end(); it!=eit; ++it) {
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

void MRGenerator::getEscapObjviaGlobals(NodeBS& globs, const NodeBS& calleeModRef) {
    for(NodeBS::iterator it = calleeModRef.begin(), eit = calleeModRef.end(); it!=eit; ++it) {
        const MemObj* obj = pta->getPAG()->getObject(*it);
        assert(obj && "object not found!!");
        if(allGlobals.test(*it))
            globs.set(*it);
    }
}

/*!
 * Whether the object node is a non-local object
 * including global, heap, and stack variable in recursions
 */
bool MRGenerator::isNonLocalObject(NodeID id, const Function* curFun) const {
    const MemObj* obj = pta->getPAG()->getObject(id);
    assert(obj && "object not found!!");
    /// if the object is heap or global
    if(obj->isGlobalObj() || obj->isHeap())
        return true;
    /// or if the local variable of its callers
    /// or a local variable is in function recursion cycles
    else if(obj->isStack()) {
        if(const AllocaInst* local = SVFUtil::dyn_cast<AllocaInst>(obj->getRefVal())) {
            const Function* fun = local->getParent()->getParent();
            if(fun!=curFun)
                return true;
            else
                return callGraphSCC->isInCycle(callGraph->getCallGraphNode(fun)->getId());
        }
    }

    return false;
}

/*!
 * Get Mod-Ref of a callee function
 */
bool MRGenerator::handleCallsiteModRef(NodeBS& mod, NodeBS& ref, CallSite cs, const Function* callee){
    /// if a callee is a heap allocator function, then its mod set of this callsite is the heap object.
    if(isHeapAllocExtCall(cs)){
        PAGEdgeList& pagEdgeList = getPAGEdgesFromInst(cs.getInstruction());
        for (PAGEdgeList::const_iterator bit = pagEdgeList.begin(),
                ebit = pagEdgeList.end(); bit != ebit; ++bit) {
            const PAGEdge* edge = *bit;
            if (const AddrPE* addr = SVFUtil::dyn_cast<AddrPE>(edge))
                mod.set(addr->getSrcID());
        }
    }
    /// otherwise, we find the mod/ref sets from the callee function
    else{
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
void MRGenerator::modRefAnalysis(PTACallGraphNode* callGraphNode, WorkList& worklist) {

    /// add ref/mod set of callee to its invocation callsites at caller
    for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it) {
        PTACallGraphEdge* edge = *it;

        /// handle direct callsites
        for(PTACallGraphEdge::CallInstSet::iterator cit = edge->getDirectCalls().begin(),
                ecit = edge->getDirectCalls().end(); cit!=ecit; ++cit) {
            NodeBS mod, ref;
            CallSite cs = SVFUtil::getLLVMCallSite(*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
        /// handle indirect callsites
        for(PTACallGraphEdge::CallInstSet::iterator cit = edge->getIndirectCalls().begin(),
                ecit = edge->getIndirectCalls().end(); cit!=ecit; ++cit) {
            NodeBS mod, ref;
            CallSite cs = SVFUtil::getLLVMCallSite(*cit);
            bool modrefchanged = handleCallsiteModRef(mod, ref, cs, callGraphNode->getFunction());
            if(modrefchanged)
                worklist.push(edge->getSrcID());
        }
    }
}

/*!
 * Determine whether a CallSite instruction can mod or ref
 * any memory location
 */
ModRefInfo MRGenerator::getModRefInfo(CallSite cs) {
    bool ref = hasRefSideEffectOfCallSite(cs);
    bool mod = hasModSideEffectOfCallSite(cs);

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
 * Determine whether a CallSite instruction can mod or ref
 * a specific memory location pointed by V
 */
ModRefInfo MRGenerator::getModRefInfo(CallSite cs, const Value* V) {
    bool ref = false;
    bool mod = false;

    if (pta->getPAG()->hasValueNode(V)) {
        const PointsTo& pts(pta->getPts(pta->getPAG()->getValueNode(V)));
        if (hasRefSideEffectOfCallSite(cs) && getRefSideEffectOfCallSite(cs).intersects(pts))
            ref = true;
        if (hasModSideEffectOfCallSite(cs) && getModSideEffectOfCallSite(cs).intersects(pts))
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
 * Determine mod-ref relations between two CallSite instructions
 */
ModRefInfo MRGenerator::getModRefInfo(CallSite cs1, CallSite cs2) {
    bool ref = false;
    bool mod = false;

    /// return NoModRef neither two callsites ref or mod any memory
    if (getModRefInfo(cs1) == ModRefInfo::NoModRef || getModRefInfo(cs2) == ModRefInfo::NoModRef)
        return ModRefInfo::NoModRef;

    const PointsTo& cs1Ref = getRefSideEffectOfCallSite(cs1);
    const PointsTo& cs1Mod = getModSideEffectOfCallSite(cs1);
    const PointsTo& cs2Ref = getRefSideEffectOfCallSite(cs2);
    const PointsTo& cs2Mod = getModSideEffectOfCallSite(cs2);

    /// Ref: cs1 ref memory mod by cs2
    if (cs1Ref.intersects(cs2Mod))
        ref = true;
    /// Mod: cs1 mod memory ref or mod by cs2
    if (cs1Mod.intersects(cs2Ref) || cs1Mod.intersects(cs2Mod))
        mod = true;
    /// ModRef: cs1 ref and mod memory mod by cs2
    if (cs1Ref.intersects(cs2Mod) && cs1Mod.intersects(cs2Mod))
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
