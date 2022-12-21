/*
 * MTAAnnotator.cpp
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTAAnnotator.h"
#include "MTA/LockAnalysis.h"
#include "MemoryModel/PointsTo.h"
#include <sstream>

using namespace SVF;
using namespace SVFUtil;


void MTAAnnotator::annotateDRCheck(Instruction* inst)
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << DR_CHECK;

    /// memcpy and memset is not annotated
    if (StoreInst* st = SVFUtil::dyn_cast<StoreInst>(inst))
    {
        numOfAnnotatedSt++;
        addMDTag(inst, st->getPointerOperand(), rawstr.str());
    }
    else if (LoadInst* ld = SVFUtil::dyn_cast<LoadInst>(inst))
    {
        numOfAnnotatedLd++;
        addMDTag(inst, ld->getPointerOperand(), rawstr.str());
    }
}

void MTAAnnotator::collectLoadStoreInst(SVFModule* mod)
{

    for (Module& M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            if (SVFUtil::isExtCall(LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(&*F)))
                continue;
            for (const_inst_iterator II = inst_begin(*F), E = inst_end(*F); II != E; ++II)
            {
                const Instruction* inst = &*II;
                if (SVFUtil::isa<LoadInst>(inst))
                {
                    loadset.insert(inst);
                }
                else if (SVFUtil::isa<StoreInst>(inst))
                {
                    storeset.insert(inst);
                }
                else if (isMemset(inst))
                {
                    storeset.insert(inst);
                }
                else if (isMemcpy(inst))
                {
                    storeset.insert(inst);
                    loadset.insert(inst);
                }
            }
        }
    }

    numOfAllSt = storeset.size();
    numOfAllLd = loadset.size();
}

const Value* MTAAnnotator::getStoreOperand(const Instruction* inst)
{
    if (const StoreInst* st = SVFUtil::dyn_cast<StoreInst>(inst))
    {
        return st->getPointerOperand();
    }
    else if (isMemset(inst))
    {
        return inst->getOperand(0);
    }
    else if (isMemcpy(inst))
    {
        return inst->getOperand(0);
    }

    assert(false);
    return nullptr;
}
const Value* MTAAnnotator::getLoadOperand(const Instruction* inst)
{
    if (const LoadInst* ld = SVFUtil::dyn_cast<LoadInst>(inst))
    {
        return ld->getPointerOperand();
    }
    else if (isMemcpy(inst))
    {
        return inst->getOperand(1);
    }

    assert(false);
    return nullptr;
}

void MTAAnnotator::initialize(MHP* m, LockAnalysis* la)
{
    mhp = m;
    lsa = la;
    if (!Options::AnnoFlag())
        return;
    collectLoadStoreInst(mhp->getTCT()->getPTA()->getModule());
}

void MTAAnnotator::pruneThreadLocal(PointerAnalysis* pta)
{
    bool AnnoLocal = Options::AnnoFlag() & ANNO_LOCAL;
    if (!AnnoLocal)
        return;

    DBOUT(DGENERAL, outs() << pasMsg("Run annotator prune thread local pairs\n"));
    SVFIR* pag = pta->getPAG();
    PointsTo nonlocalobjs;
    PointsTo worklist;

    /// find fork arguments' objects
    const SVFStmt::SVFStmtSetTy& forkedges = pag->getPTASVFStmtSet(SVFStmt::ThreadFork);
    for (SVFStmt::SVFStmtSetTy::const_iterator it = forkedges.begin(), eit = forkedges.end(); it != eit; ++it)
    {
        PAGEdge* edge = *it;
        worklist |= pta->getPts(edge->getDstID());
        worklist |= pta->getPts(edge->getSrcID());
    }

    /// find global pointer-to objects
    const SVFIR::SVFStmtSet& globaledges = pag->getGlobalSVFStmtSet();
    for (SVFIR::SVFStmtSet::const_iterator it = globaledges.begin(), eit = globaledges.end(); it != eit; ++it)
    {
        const PAGEdge* edge = *it;
        if (edge->getEdgeKind() == SVFStmt::Addr)
        {
            worklist.set(edge->getSrcID());
        }
    }

    /// find all non-local objects that are transitively pointed by global and fork arguments.
    while (!worklist.empty())
    {
        NodeID obj = worklist.find_first();
        nonlocalobjs.set(obj);
        worklist.reset(obj);
        PointsTo pts = pta->getPts(obj);
        for (PointsTo::iterator pit = pts.begin(), epit = pts.end(); pit != epit; ++pit)
        {
            if (!nonlocalobjs.test(*pit))
                worklist.set(*pit);
        }
        NodeBS fields = pag->getAllFieldsObjVars(obj);
        for (NodeBS::iterator pit = fields.begin(), epit = fields.end(); pit != epit; ++pit)
        {
            if (!nonlocalobjs.test(*pit))
                worklist.set(*pit);
        }
    }

    /// compute all store and load instructions that may operate a non-local object.
    InstSet needannost;
    InstSet needannold;
    for (InstSet::iterator it = storeset.begin(), eit = storeset.end(); it != eit; ++it)
    {
        PointsTo pts = pta->getPts(pag->getValueNode(LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getStoreOperand(*it))));
        for (PointsTo::iterator pit = pts.begin(), epit = pts.end(); pit != epit; ++pit)
        {
            if (nonlocalobjs.test(*pit))
            {
                needannost.insert(*it);
                break;
            }
        }
    }

    for (InstSet::iterator it = loadset.begin(), eit = loadset.end(); it != eit; ++it)
    {
        PointsTo pts = pta->getPts(pag->getValueNode(LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getLoadOperand(*it))));
        for (PointsTo::iterator pit = pts.begin(), epit = pts.end(); pit != epit; ++pit)
        {
            if (nonlocalobjs.test(*pit))
            {
                needannold.insert(*it);
                break;
            }
        }
    }

    storeset = needannost;
    loadset = needannold;

    numOfNonLocalSt = storeset.size();
    numOfNonLocalLd = loadset.size();
}
void MTAAnnotator::pruneAliasMHP(PointerAnalysis* pta)
{

    bool AnnoMHP = Options::AnnoFlag() & ANNO_MHP;
    bool AnnoAlias = Options::AnnoFlag() & ANNO_ALIAS;

    if (!AnnoMHP && !AnnoAlias)
        return;

    DBOUT(DGENERAL, outs() << pasMsg("Run annotator prune Alias or MHP pairs\n"));
    InstSet needannost;
    InstSet needannold;
    for (InstSet::iterator it1 = storeset.begin(), eit1 = storeset.end(); it1 != eit1; ++it1)
    {
        const SVFInstruction* inst1 = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(*it1);
        for (InstSet::iterator it2 = it1, eit2 = storeset.end(); it2 != eit2; ++it2)
        {
            const SVFInstruction* inst2 = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(*it2);
            const SVFValue* v1 = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getStoreOperand(*it1));
            const SVFValue* v2 = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getStoreOperand(*it2));

            if(!pta->alias(v1, v2))
                continue;

            if (AnnoMHP)
            {
                if (mhp->mayHappenInParallel(inst1, inst2) && !lsa->isProtectedByCommonLock(inst1, inst2))
                {
                    needannost.insert(*it1);
                    needannost.insert(*it2);
                }
            }
            else
            {
                /// if it1 == it2, mhp analysis will annotate it1 that locates in loop or recursion.
                /// but alias analysis fails to determine whether it1 is in loop or recursion, that means
                /// all store instructions will be annotated by alias analysis to guarantee sound.
                needannost.insert(*it1);
                needannost.insert(*it2);
            }
        }
        for (InstSet::iterator it2 = loadset.begin(), eit2 = loadset.end(); it2 != eit2; ++it2)
        {
            const SVFInstruction* inst2 = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(*it2);
            const SVFValue* v1 = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getStoreOperand(*it1));
            const SVFValue* v2 = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(getLoadOperand(*it2));

            if(!pta->alias(v1,v2))
                continue;

            if (AnnoMHP)
            {
                if (mhp->mayHappenInParallel(inst1, inst2) && !lsa->isProtectedByCommonLock(inst1, inst2))
                {
                    needannost.insert(*it1);
                    needannold.insert(*it2);
                }
            }
            else
            {
                needannost.insert(*it1);
                needannold.insert(*it2);
            }
        }
    }
    storeset = needannost;
    loadset = needannold;

    if (AnnoMHP)
    {
        numOfMHPSt = storeset.size();
        numOfMHPLd = loadset.size();
    }
    else if (AnnoAlias)
    {
        numOfAliasSt = storeset.size();
        numOfAliasLd = loadset.size();
    }
}
void MTAAnnotator::performAnnotate()
{
    if (!Options::AnnoFlag())
        return;
    for (InstSet::iterator it = storeset.begin(), eit = storeset.end(); it != eit; ++it)
    {
        annotateDRCheck(const_cast<Instruction*>(*it));
    }
    for (InstSet::iterator it = loadset.begin(), eit = loadset.end(); it != eit; ++it)
    {
        annotateDRCheck(const_cast<Instruction*>(*it));
    }
}
