//===- FlowSensitiveTBHC.cpp -- flow-sensitive type filter ------------//

/*
 * FlowSensitiveTBHC.cpp
 *
 *  Created on: Oct 08, 2019
 *      Author: Mohamad Barbar
 */

#include "Util/Options.h"
#include "SVF-FE/DCHG.h"
#include "SVF-FE/CPPUtil.h"
#include "WPA/FlowSensitiveTBHC.h"
#include "WPA/WPAStat.h"
#include "WPA/Andersen.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;

FlowSensitiveTBHC::FlowSensitiveTBHC(PAG* _pag, PTATY type) : FlowSensitive(_pag, type), TypeBasedHeapCloning(this)
{
    // Using `this` as the argument for TypeBasedHeapCloning is okay. As PointerAnalysis, it's
    // already constructed. TypeBasedHeapCloning also doesn't use pta in the constructor so it
    // just needs to be allocated, which it is.
    allReuse = Options::TBHCAllReuse;
    storeReuse = allReuse || Options::TBHCStoreReuse;
}

void FlowSensitiveTBHC::analyze()
{
    FlowSensitive::analyze();
}

void FlowSensitiveTBHC::initialize()
{
    PointerAnalysis::initialize();
    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());
    svfg = memSSA.buildFullSVFG(ander);
    setGraph(svfg);
    stat = new FlowSensitiveStat(this);

    DCHGraph *dchg = SVFUtil::dyn_cast<DCHGraph>(getCHGraph());
    assert(dchg != nullptr && "FSTBHC: DCHGraph required!");

    TypeBasedHeapCloning::setDCHG(dchg);
    TypeBasedHeapCloning::setPAG(pag);

    // Populates loadGeps.
    determineWhichGepsAreLoads();
}

void FlowSensitiveTBHC::finalize(void)
{
    FlowSensitive::finalize();
    // ^ Will print call graph and alias stats.

    if(print_stat)
        dumpStats();
    // getDFPTDataTy()->dumpPTData();

    validateTBHCTests(svfMod);
}

void FlowSensitiveTBHC::backPropagate(NodeID clone)
{
    PAGNode *cloneObj = pag->getPAGNode(clone);
    assert(cloneObj && "FSTBHC: clone does not exist in PAG?");
    PAGNode *originalObj = pag->getPAGNode(getOriginalObj(clone));
    assert(cloneObj && "FSTBHC: original object does not exist in PAG?");
    // Check the original object too because when reuse of a gep occurs, the new object
    // is an FI object.
    if (SVFUtil::isa<CloneGepObjPN>(cloneObj) || SVFUtil::isa<GepObjPN>(originalObj))
    {
        // Since getGepObjClones is updated, some GEP nodes need to be redone.
        const NodeBS &retrievers = gepToSVFGRetrievers[getOriginalObj(clone)];
        for (NodeID r : retrievers)
        {
            pushIntoWorklist(r);
        }
    }
    else if (SVFUtil::isa<CloneFIObjPN>(cloneObj) || SVFUtil::isa<CloneDummyObjPN>(cloneObj))
    {
        pushIntoWorklist(getAllocationSite(getOriginalObj(clone)));
    }
    else
    {
        assert(false && "FSTBHC: unexpected object type?");
    }
}

bool FlowSensitiveTBHC::propAlongIndirectEdge(const IndirectSVFGEdge* edge)
{
    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();

    bool changed = false;

    // Get points-to targets may be used by next SVFG node.
    // Propagate points-to set for node used in dst.
    const NodeBS & pts = edge->getPointsTo();

    // Since the base Andersen's analysis does NOT perform type-based heap cloning,
    // it uses only the base objects; we want to account for clones too.
    NodeBS edgePtsAndClones;

    // TODO: the conditional bool may be unnecessary.
    // Adding all clones is redundant, and introduces too many calls to propVarPts...
    // This introduces performance and precision penalties.
    // We should filter out according to src.
    bool isStore = false;
    const DIType *tildet = nullptr;
    PointsTo storePts;
    if (const StoreSVFGNode *store = SVFUtil::dyn_cast<StoreSVFGNode>(src))
    {
        tildet = getTypeFromCTirMetadata(store);
        isStore = true;
        storePts = getPts(store->getPAGDstNodeID());
    }

    const PointsTo &filterSet = getFilterSet(src->getId());
    for (NodeID o : pts)
    {
        if (!filterSet.test(o))
        {
            edgePtsAndClones.set(o);
        }

        for (NodeID c : getClones(o))
        {
            if (!isStore)
            {
                if (!filterSet.test(c))
                {
                    edgePtsAndClones.set(c);
                }
            }
            else
            {
                if (storePts.test(c) && !filterSet.test(c))
                {
                    edgePtsAndClones.set(c);
                }
            }
        }

        if (GepObjPN *gep = SVFUtil::dyn_cast<GepObjPN>(pag->getPAGNode(o)))
        {
            // Want the geps which are at the same "level" as this one (same mem obj, same offset).
            const NodeBS &geps = getGepObjsFromMemObj(gep->getMemObj(), gep->getLocationSet().getOffset());
            for (NodeID g : geps)
            {
                const DIType *gepType = getType(g);
                if (!isStore || gepType || isBase(tildet, gepType))
                {
                    if (!filterSet.test(g))
                    {
                        edgePtsAndClones.set(g);
                    }
                }
            }
        }
    }

    for (NodeID o : edgePtsAndClones)
    {
        if (propVarPtsFromSrcToDst(o, src, dst))
            changed = true;

        if (isFIObjNode(o))
        {
            /// If this is a field-insensitive obj, propagate all field node's pts
            const NodeBS &allFields = getAllFieldsObjNode(o);
            for (NodeID f : allFields)
            {
                if (propVarPtsFromSrcToDst(f, src, dst))
                    changed = true;
            }
        }
    }

    return changed;
}

bool FlowSensitiveTBHC::propAlongDirectEdge(const DirectSVFGEdge* edge)
{
    double start = stat->getClk();
    bool changed = false;

    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();
    // If this is an actual-param or formal-ret, top-level pointer's pts must be
    // propagated from src to dst.
    if (ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(src))
    {
        if (!ap->getParam()->isPointer()) return false;
        changed = propagateFromAPToFP(ap, dst);
    }
    else if (FormalRetSVFGNode* fp = SVFUtil::dyn_cast<FormalRetSVFGNode>(src))
    {
        if (!fp->getRet()->isPointer()) return false;
        changed = propagateFromFRToAR(fp, dst);
    }
    else
    {
        // Direct SVFG edge links between def and use of a top-level pointer.
        // There's no points-to information propagated along direct edge.
        // Since the top-level pointer's value has been changed at src node,
        // return TRUE to put dst node into the work list.
        changed = true;
    }

    double end = stat->getClk();
    directPropaTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool FlowSensitiveTBHC::processAddr(const AddrSVFGNode* addr)
{
    double start = stat->getClk();

    NodeID srcID = addr->getPAGSrcNodeID();
    NodeID dstID = addr->getPAGDstNodeID();

    double end = stat->getClk();
    addrTime += (end - start) / TIMEINTERVAL;

    if (!addr->getPAGEdge()->isPTAEdge()) return false;

    bool changed = FlowSensitive::processAddr(addr);

    start = stat->getClk();

    const DIType *objType;
    if (isHeapMemObj(srcID))
    {
        objType = undefType;
    }
    else if (pag->isConstantObj(srcID))
    {
        // Probably constants that have been merged into one.
        // We make it undefined even though it's technically a global
        // to keep in line with SVF's design.
        // This will end up splitting into one for each type of constant.
        objType = undefType;
    }
    else
    {
        // Stack/global.
        objType = getTypeFromCTirMetadata(addr);
    }

    setType(srcID, objType);
    setAllocationSite(srcID, addr->getId());

    // All the typed versions of srcID. This handles back-propagation.
    const NodeBS &clones = getClones(srcID);
    for (NodeID c : clones)
    {
        changed = addPts(dstID, c) || changed;
        // No need for typing these are all clones; they are all typed.
    }

    end = stat->getClk();
    addrTime += (end - start) / TIMEINTERVAL;

    return changed;
}

bool FlowSensitiveTBHC::processGep(const GepSVFGNode* gep)
{
    // Copy of that in FlowSensitive.cpp + some changes.
    double start = stat->getClk();
    bool changed = false;

    NodeID q = gep->getPAGSrcNodeID();

    const DIType *tildet = getTypeFromCTirMetadata(gep);
    if (tildet != undefType)
    {
        bool reuse = Options::TBHCAllReuse || (Options::TBHCStoreReuse && !gepIsLoad(gep->getId()));
        changed = init(gep->getId(), q, tildet, reuse, true);
    }

    if (!gep->getPAGEdge()->isPTAEdge())
    {
        return changed;
    }

    const PointsTo& qPts = getPts(q);
    PointsTo &filterSet = getFilterSet(gep->getId());
    PointsTo tmpDstPts;
    for (NodeID oq : qPts)
    {
        if (filterSet.test(oq)) continue;

        if (TypeBasedHeapCloning::isBlkObjOrConstantObj(oq))
        {
            tmpDstPts.set(oq);
        }
        else
        {
            if (SVFUtil::isa<VariantGepPE>(gep->getPAGEdge()))
            {
                setObjFieldInsensitive(oq);
                tmpDstPts.set(oq);
                const DIType *t = getType(oq);
                if (t && (t->getTag() == dwarf::DW_TAG_array_type || t->getTag() == dwarf::DW_TAG_pointer_type))
                {
                    const NodeBS fieldClones = getGepObjClones(oq, 1);
                    for (NodeID fc : fieldClones)
                    {
                        gepToSVFGRetrievers[getOriginalObj(fc)].set(gep->getId());
                        tmpDstPts.set(fc);
                    }
                }
            }
            else if (const NormalGepPE* normalGep = SVFUtil::dyn_cast<NormalGepPE>(gep->getPAGEdge()))
            {
                const DIType *baseType = getType(oq);

                // Drop down to field insensitive.
                if (baseType == nullptr)
                {
                    setObjFieldInsensitive(oq);
                    NodeID fiObj = oq;
                    tmpDstPts.set(fiObj);
                    continue;
                }

                if (DCHGraph::isAgg(baseType) && baseType->getTag() != dwarf::DW_TAG_array_type
                        && normalGep->getLocationSet().getOffset() >= dchg->getNumFields(baseType))
                {
                    // If the field offset is too high for this object, it is killed. It seems that a
                    // clone was made on this GEP but this is not the base (e.g. base->f1->f2), and SVF
                    // expects to operate on the base (hence the large offset). The base will have been
                    // cloned at another GEP and back-propagated, thus it'll reach here safe and sound.
                    // We ignore arrays/pointers because those are array accesses/pointer arithmetic we
                    // assume are correct.
                    // Obviously, non-aggregates cannot have their fields taken so they are spurious.
                    filterSet.set(oq);
                }
                else
                {
                    // Operate on the field and all its clones.
                    const NodeBS fieldClones = getGepObjClones(oq, normalGep->getLocationSet().getOffset());
                    for (NodeID fc : fieldClones)
                    {
                        gepToSVFGRetrievers[getOriginalObj(fc)].set(gep->getId());
                        tmpDstPts.set(fc);
                    }
                }
            }
            else
            {
                assert(false && "FSTBHC: new gep edge?");
            }
        }
    }

    double end = stat->getClk();
    gepTime += (end - start) / TIMEINTERVAL;

    changed = unionPts(gep->getPAGDstNodeID(), tmpDstPts) || changed;
    return changed;
}

bool FlowSensitiveTBHC::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();

    bool changed = false;
    const DIType *tildet = getTypeFromCTirMetadata(load);
    if (tildet != undefType)
    {
        changed = init(load->getId(), load->getPAGSrcNodeID(), tildet, Options::TBHCAllReuse);
    }

    // We want to perform the initialisation for non-pointer nodes but not process the load.
    if (!load->getPAGEdge()->isPTAEdge())
    {
        return changed;
    }

    NodeID dstVar = load->getPAGDstNodeID();

    const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());
    const PointsTo &filterSet = getFilterSet(load->getId());
    // unionPtsFromIn is going to call getOriginalObj on ptd anyway.
    // This results in fewer loop iterations. o_t, o_s --> o.
    PointsTo srcOriginalObjs;
    for (NodeID s : srcPts)
    {
        if (filterSet.test(s)) continue;
        if (pag->isConstantObj(s) || pag->isNonPointerObj(s)) continue;
        srcOriginalObjs.set(getOriginalObj(s));
    }

    for (NodeID ptd : srcOriginalObjs)
    {
        // filterSet tests happened while building srcOriginalObjs.
        if (unionPtsFromIn(load, ptd, dstVar))
            changed = true;

        if (isFIObjNode(ptd))
        {
            /// If the ptd is a field-insensitive node, we should also get all field nodes'
            /// points-to sets and pass them to pagDst.
            const NodeBS &allFields = getAllFieldsObjNode(ptd);
            for (NodeID f : allFields)
            {
                if (unionPtsFromIn(load, f, dstVar))
                    changed = true;
            }
        }
    }

    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool FlowSensitiveTBHC::processStore(const StoreSVFGNode* store)
{
    double start = stat->getClk();

    bool changed = false;
    const DIType *tildet = getTypeFromCTirMetadata(store);
    if (tildet != undefType)
    {
        changed = init(store->getId(), store->getPAGDstNodeID(), tildet, Options::TBHCAllReuse || Options::TBHCStoreReuse);
    }

    // Like processLoad: we want to perform initialisation for non-pointers but not the store.
    if (!store->getPAGEdge()->isPTAEdge())
    {
        // Pass through and return because there may be some pointer nodes
        // relying on this node's parents.
        changed = getDFPTDataTy()->updateAllDFOutFromIn(store->getId(), 0, false);
        return changed;
    }

    const PointsTo & dstPts = getPts(store->getPAGDstNodeID());

    /// STORE statement can only be processed if the pointer on the LHS
    /// points to something. If we handle STORE with an empty points-to
    /// set, the OUT set will be updated from IN set. Then if LHS pointer
    /// points-to one target and it has been identified as a strong
    /// update, we can't remove those points-to information computed
    /// before this strong update from the OUT set.
    if (dstPts.empty())
    {
        return changed;
    }

    changed = false;
    const PointsTo &filterSet = getFilterSet(store->getId());
    if(getPts(store->getPAGSrcNodeID()).empty() == false)
    {
        for (NodeID ptd : dstPts)
        {
            if (filterSet.test(ptd)) continue;

            if (pag->isConstantObj(ptd) || pag->isNonPointerObj(ptd))
                continue;

            if (unionPtsFromTop(store, store->getPAGSrcNodeID(), ptd))
                changed = true;
        }
    }

    double end = stat->getClk();
    storeTime += (end - start) / TIMEINTERVAL;

    double updateStart = stat->getClk();
    // also merge the DFInSet to DFOutSet.
    /// check if this is a strong updates store
    NodeID singleton;
    bool isSU = isStrongUpdate(store, singleton);
    if (isSU)
    {
        svfgHasSU.set(store->getId());
        if (strongUpdateOutFromIn(store, singleton))
            changed = true;
    }
    else
    {
        svfgHasSU.reset(store->getId());
        if (weakUpdateOutFromIn(store))
            changed = true;
    }
    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    return changed;
}

bool FlowSensitiveTBHC::processPhi(const PHISVFGNode* phi)
{
    if (!phi->isPTANode()) return false;
    return FlowSensitive::processPhi(phi);
}

/// Returns whether this instruction initialisates an object's
/// vtable (metadata: ctir.vt.init). Returns the object's type,
/// otherwise, nullptr.
static const DIType *getVTInitType(const CopySVFGNode *copy, DCHGraph *dchg)
{
    if (copy->getInst() == nullptr) return nullptr;
    const Instruction *inst = copy->getInst();

    const MDNode *mdNode = inst->getMetadata(cppUtil::ctir::vtInitMDName);
    if (mdNode == nullptr) return nullptr;

    const DIType *type = SVFUtil::dyn_cast<DIType>(mdNode);
    assert(type != nullptr && "TBHC: bad ctir.vt.init metadata");
    return dchg->getCanonicalType(type);
}

bool FlowSensitiveTBHC::processCopy(const CopySVFGNode* copy)
{
    const DIType *vtInitType = getVTInitType(copy, dchg);
    bool changed = false;
    if (vtInitType != nullptr)
    {
        // Setting the virtual table pointer.
        changed = init(copy->getId(), copy->getPAGSrcNodeID(), vtInitType, true);
    }

    return FlowSensitive::processCopy(copy) || changed;
}

const NodeBS& FlowSensitiveTBHC::getAllFieldsObjNode(NodeID id)
{
    return getGepObjs(id);
}

bool FlowSensitiveTBHC::updateInFromIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
{
    // IN sets are only based on the original object.
    return getDFPTDataTy()->updateDFInFromIn(srcStmt->getId(), getOriginalObj(srcVar),
            dstStmt->getId(), getOriginalObj(dstVar));
}

bool FlowSensitiveTBHC::updateInFromOut(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
{
    // OUT/IN sets only have original objects.
    return getDFPTDataTy()->updateDFInFromOut(srcStmt->getId(), getOriginalObj(srcVar),
            dstStmt->getId(), getOriginalObj(dstVar));
}

bool FlowSensitiveTBHC::unionPtsFromIn(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar)
{
    // IN sets only have original objects.
    return getDFPTDataTy()->updateTLVPts(stmt->getId(), getOriginalObj(srcVar), dstVar);
}

bool FlowSensitiveTBHC::unionPtsFromTop(const SVFGNode* stmt, NodeID srcVar, NodeID dstVar)
{
    // OUT sets only have original objects.
    return getDFPTDataTy()->updateATVPts(srcVar, stmt->getId(), getOriginalObj(dstVar));
}

bool FlowSensitiveTBHC::propDFInToIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
{
    // IN sets are only based on the original object.
    return getDFPTDataTy()->updateAllDFInFromIn(srcStmt->getId(), getOriginalObj(srcVar),
            dstStmt->getId(), getOriginalObj(dstVar));
}

bool FlowSensitiveTBHC::propDFOutToIn(const SVFGNode* srcStmt, NodeID srcVar, const SVFGNode* dstStmt, NodeID dstVar)
{
    // OUT/IN sets only have original objects.
    return getDFPTDataTy()->updateAllDFInFromOut(srcStmt->getId(), getOriginalObj(srcVar),
            dstStmt->getId(), getOriginalObj(dstVar));
}

void FlowSensitiveTBHC::determineWhichGepsAreLoads(void)
{
    for (SVFG::iterator nI = svfg->begin(); nI != svfg->end(); ++nI)
    {
        SVFGNode *svfgNode = nI->second;
        if (const StmtSVFGNode *gep = SVFUtil::dyn_cast<GepSVFGNode>(svfgNode))
        {
            // Only care about ctir nodes - they have the reuse problem.
            if (getTypeFromCTirMetadata(gep))
            {
                bool isLoad = true;
                for (const SVFGEdge *e : gep->getOutEdges())
                {
                    SVFGNode *dst = e->getDstNode();

                    // Loop on itself - don't care.
                    if (gep == dst) continue;

                    if (!SVFUtil::isa<LoadSVFGNode>(dst))
                    {
                        isLoad = false;
                        break;
                    }
                }

                if (isLoad)
                {
                    loadGeps.set(gep->getId());
                }
            }
        }
    }
}

bool FlowSensitiveTBHC::gepIsLoad(NodeID gep)
{
    // Handles when gep is not even a GEP; loadGeps only contains GEPs.
    return loadGeps.test(gep);
}

const MDNode *FlowSensitiveTBHC::getRawCTirMetadata(const SVFGNode *s)
{
    if (const StmtSVFGNode *stmt = SVFUtil::dyn_cast<StmtSVFGNode>(s))
    {
        const Value *v = stmt->getInst() ? stmt->getInst() : stmt->getPAGEdge()->getValue();
        if (v != nullptr)
        {
            return TypeBasedHeapCloning::getRawCTirMetadata(v);
        }
    }

    return nullptr;
}

const DIType *FlowSensitiveTBHC::getTypeFromCTirMetadata(const SVFGNode *s)
{
    if (const StmtSVFGNode *stmt = SVFUtil::dyn_cast<StmtSVFGNode>(s))
    {
        const Value *v = stmt->getInst();
        if (v != nullptr)
        {
            return TypeBasedHeapCloning::getTypeFromCTirMetadata(v);
        }
    }

    return nullptr;
}

void FlowSensitiveTBHC::expandFIObjs(const PointsTo& pts, PointsTo& expandedPts)
{
    expandedPts = pts;
    for (NodeID o : pts)
    {
        expandedPts |= getAllFieldsObjNode(o);
        while (const GepObjPN *gepObj = SVFUtil::dyn_cast<GepObjPN>(pag->getPAGNode(o)))
        {
            expandedPts |= getAllFieldsObjNode(o);
            o = gepObj->getBaseNode();
        }
    }
}

void FlowSensitiveTBHC::countAliases(Set<std::pair<NodeID, NodeID>> cmp, unsigned *mayAliases, unsigned *noAliases)
{
    Map<std::pair<NodeID, NodeID>, PointsTo> filteredPts;
    for (std::pair<NodeID, NodeID> locP : cmp)
    {
        const PointsTo &filterSet = getFilterSet(locP.first);
        const PointsTo &pts = getPts(locP.second);
        PointsTo &ptsFiltered = filteredPts[locP];

        for (NodeID o : pts)
        {
            if (filterSet.test(o)) continue;
            ptsFiltered.set(o);
        }
    }

    for (std::pair<NodeID, NodeID> locPA : cmp)
    {
        const PointsTo &aPts = filteredPts[locPA];
        for (std::pair<NodeID, NodeID> locPB : cmp)
        {
            if (locPB == locPA) continue;
            const PointsTo &bPts = filteredPts[locPB];

            switch (alias(aPts, bPts))
            {
            case llvm::AliasResult::NoAlias:
                ++(*noAliases);
                break;
            case llvm::AliasResult::MayAlias:
                ++(*mayAliases);
                break;
            default:
                assert("Not May/NoAlias?");
            }
        }
    }

}
