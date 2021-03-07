/*
 * MTAAnnotator.cpp
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "MTA/MTAAnnotator.h"
#include "MTA/LockAnalysis.h"
#include "SVF-FE/LLVMUtil.h"
#include <sstream>

using namespace SVF;
using namespace SVFUtil;

static llvm::cl::opt<u32_t> AnnoFlag("anno", llvm::cl::init(0), llvm::cl::desc("prune annotated instructions: 0001 Thread Local; 0002 Alias; 0004 MHP."));

void MTAAnnotator::annotateDRCheck(Instruction* inst)
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << DR_CHECK;

    /// memcpy and memset is not annotated
    if (auto* st = SVFUtil::dyn_cast<StoreInst>(inst))
    {
        numOfAnnotatedSt++;
        addMDTag(inst, st->getPointerOperand(), rawstr.str());
    }
    else if (auto* ld = SVFUtil::dyn_cast<LoadInst>(inst))
    {
        numOfAnnotatedLd++;
        addMDTag(inst, ld->getPointerOperand(), rawstr.str());
    }
}

void MTAAnnotator::collectLoadStoreInst(SVFModule* mod)
{

    for (auto & F : *mod)
    {
        const Function* fun = F;
        if (SVFUtil::isExtCall(fun))
            continue;
        for (inst_iterator II = inst_begin(F), E = inst_end(F); II != E; ++II)
        {
            const Instruction *inst = &*II;
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

    numOfAllSt = storeset.size();
    numOfAllLd = loadset.size();
}

const Value* MTAAnnotator::getStoreOperand(const Instruction* inst)
{
    if (const auto* st = SVFUtil::dyn_cast<StoreInst>(inst))
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
    if (const auto* ld = SVFUtil::dyn_cast<LoadInst>(inst))
    {
        return ld->getPointerOperand();
    }

    if (isMemcpy(inst))
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
    if (!AnnoFlag)
        return;
    collectLoadStoreInst(mhp->getTCT()->getPTA()->getModule());
}

void MTAAnnotator::pruneThreadLocal(PointerAnalysis* pta)
{
    bool AnnoLocal = AnnoFlag & ANNO_LOCAL;
    if (!AnnoLocal)
        return;

    DBOUT(DGENERAL, outs() << pasMsg("Run annotator prune thread local pairs\n"));
    PAG* pag = pta->getPAG();
    PointsTo nonlocalobjs;
    PointsTo worklist;

    /// find fork arguments' objects
    const PAGEdge::PAGEdgeSetTy& forkedges = pag->getPTAEdgeSet(PAGEdge::ThreadFork);
    for (auto *edge : forkedges)
    {
         worklist |= pta->getPts(edge->getDstID());
        worklist |= pta->getPts(edge->getSrcID());
    }

    /// find global pointer-to objects
    const PAG::PAGEdgeSet& globaledges = pag->getGlobalPAGEdgeSet();
    for (const auto *edge : globaledges)
    {
         if (edge->getEdgeKind() == PAGEdge::Addr)
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
        for (const auto& pt : pts)
        {
            if (!nonlocalobjs.test(pt))
                worklist.set(pt);
        }
        NodeBS fields = pag->getAllFieldsObjNode(obj);
        for (const auto& field : fields)
        {
            if (!nonlocalobjs.test(field))
                worklist.set(field);
        }
    }

    /// compute all store and load instructions that may operate a non-local object.
    InstSet needannost;
    InstSet needannold;
    for (const auto *it : storeset)
    {
        PointsTo pts = pta->getPts(pag->getValueNode(getStoreOperand(it)));
        for (const auto& pt : pts)
        {
            if (nonlocalobjs.test(pt))
            {
                needannost.insert(it);
                break;
            }
        }
    }

    for (const auto *it : loadset)
    {
        PointsTo pts = pta->getPts(pag->getValueNode(getLoadOperand(it)));
        for (const auto& pt : pts)
        {
            if (nonlocalobjs.test(pt))
            {
                needannold.insert(it);
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

    bool AnnoMHP = AnnoFlag & ANNO_MHP;
    bool AnnoAlias = AnnoFlag & ANNO_ALIAS;

    if (!AnnoMHP && !AnnoAlias)
        return;

    DBOUT(DGENERAL, outs() << pasMsg("Run annotator prune Alias or MHP pairs\n"));
    InstSet needannost;
    InstSet needannold;
    for (auto it1 = storeset.begin(), eit1 = storeset.end(); it1 != eit1; ++it1)
    {
        for (auto it2 = it1, eit2 = storeset.end(); it2 != eit2; ++it2)
        {
            if(!pta->alias(getStoreOperand(*it1), getStoreOperand(*it2)))
                continue;

            if (AnnoMHP)
            {
                if (mhp->mayHappenInParallel(*it1, *it2) && !lsa->isProtectedByCommonLock(*it1, *it2))
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
        for (const auto *it2 : loadset)
        {
            if(!pta->alias(getStoreOperand(*it1), getLoadOperand(it2)))
                continue;

            if (AnnoMHP)
            {
                if (mhp->mayHappenInParallel(*it1, it2) && !lsa->isProtectedByCommonLock(*it1, it2))
                {
                    needannost.insert(*it1);
                    needannold.insert(it2);
                }
            }
            else
            {
                needannost.insert(*it1);
                needannold.insert(it2);
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
    if (!AnnoFlag)
        return;
    for (const auto *it : storeset)
    {
        annotateDRCheck(const_cast<Instruction*>(it));
    }
    for (const auto *it : loadset)
    {
        annotateDRCheck(const_cast<Instruction*>(it));
    }
}
