/*
 * MTA.cpp
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/MTA.h"
#include "MTA/MHP.h"
#include "MTA/TCT.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTAStat.h"
#include "WPA/Andersen.h"
#include "MTA/FSMPTA.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

static llvm::RegisterPass<MTA> RACEDETECOR("pmhp", "May-Happen-in-Parallel Analysis");


char MTA::ID = 0;
ModulePass* MTA::modulePass = nullptr;
MTA::FunToSEMap MTA::func2ScevMap;
MTA::FunToLoopInfoMap MTA::func2LoopInfoMap;

MTA::MTA() :
    ModulePass(ID), tcg(nullptr), tct(nullptr)
{
    stat = new MTAStat();
}

MTA::~MTA()
{
    if (tcg)
        delete tcg;
    //if (tct)
    //    delete tct;
}

bool MTA::runOnModule(Module& module)
{
    SVFModule* m(module);
    return runOnModule(m);
}

/*!
 * Perform data race detection
 */
bool MTA::runOnModule(SVFModule* module)
{


    modulePass = this;

    MHP* mhp = computeMHP(module);
    LockAnalysis* lsa = computeLocksets(mhp->getTCT());



    /*
    if (Options::AndersenAnno) {
        pta = mhp->getTCT()->getPTA();
        if (pta->printStat())
            stat->performMHPPairStat(mhp,lsa);
        AndersenWaveDiff::releaseAndersenWaveDiff();
    } else if (Options::FSAnno) {

        reportMemoryUsageKB("Mem before analysis");
        DBOUT(DGENERAL, outs() << pasMsg("FSMPTA analysis\n"));
        DBOUT(DMTA, outs() << pasMsg("FSMPTA analysis\n"));

        DOTIMESTAT(double ptStart = stat->getClk());
        pta = FSMPTA::createFSMPTA(module, mhp,lsa);
        DOTIMESTAT(double ptEnd = stat->getClk());
        DOTIMESTAT(stat->FSMPTATime += (ptEnd - ptStart) / TIMEINTERVAL);

        reportMemoryUsageKB("Mem after analysis");

        if (pta->printStat())
            stat->performMHPPairStat(mhp,lsa);

        FSMPTA::releaseFSMPTA();
    }

    if (DoInstrumentation) {
        DBOUT(DGENERAL, outs() << pasMsg("ThreadSanitizer Instrumentation\n"));
        DBOUT(DMTA, outs() << pasMsg("ThreadSanitizer Instrumentation\n"));
        TSan tsan;
        tsan.doInitialization(*pta->getModule());
        for (Module::iterator it = pta->getModule()->begin(), eit = pta->getModule()->end(); it != eit; ++it) {
            tsan.runOnFunction(*it);
        }
        if (pta->printStat())
            PrintStatistics();
    }
    */

    delete mhp;
    delete lsa;

    return false;
}

/*!
 * Compute lock sets
 */
LockAnalysis* MTA::computeLocksets(TCT* tct)
{
    LockAnalysis* lsa = new LockAnalysis(tct);
    lsa->analyze();
    return lsa;
}

MHP* MTA::computeMHP(SVFModule* module)
{

    DBOUT(DGENERAL, outs() << pasMsg("MTA analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MTA analysis\n"));
    PointerAnalysis* pta = AndersenWaveDiff::createAndersenWaveDiff(module);
    pta->getPTACallGraph()->dump("ptacg");

    DBOUT(DGENERAL, outs() << pasMsg("Build TCT\n"));
    DBOUT(DMTA, outs() << pasMsg("Build TCT\n"));
    DOTIMESTAT(double tctStart = stat->getClk());
    tct = new TCT(pta);
    tcg = tct->getThreadCallGraph();
    DOTIMESTAT(double tctEnd = stat->getClk());
    DOTIMESTAT(stat->TCTTime += (tctEnd - tctStart) / TIMEINTERVAL);

    if (pta->printStat())
    {
        stat->performThreadCallGraphStat(tcg);
        stat->performTCTStat(tct);
    }

    tcg->dump("tcg");

    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis\n"));

    DOTIMESTAT(double mhpStart = stat->getClk());
    MHP* mhp = new MHP(tct);
    mhp->analyze();
    DOTIMESTAT(double mhpEnd = stat->getClk());
    DOTIMESTAT(stat->MHPTime += (mhpEnd - mhpStart) / TIMEINTERVAL);

    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis finish\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis finish\n"));
    return mhp;
}

///*!
// * Check   (1) write-write race
// * 		 (2) write-read race
// * 		 (3) read-read race
// * when two memory access may-happen in parallel and are not protected by the same lock
// * (excluding global constraints because they are initialized before running the main function)
// */
void MTA::detect(SVFModule* module)
{

    DBOUT(DGENERAL, outs() << pasMsg("Starting Race Detection\n"));

    LoadSet loads;
    StoreSet stores;

    Set<const Instruction*> needcheckinst;
    // Add symbols for all of the functions and the instructions in them.
    for (SVFModule::iterator F = module->begin(), E = module->end(); F != E; ++F)
    {
        // collect and create symbols inside the function body
        for (inst_iterator II = inst_begin(*F), E = inst_end(*F); II != E; ++II)
        {
            const Instruction *inst = &*II;
            if (const LoadInst* load = SVFUtil::dyn_cast<LoadInst>(inst))
            {
                loads.insert(load);
            }
            else if (const StoreInst* store = SVFUtil::dyn_cast<StoreInst>(inst))
            {
                stores.insert(store);
            }
        }
    }

    for (LoadSet::const_iterator lit = loads.begin(), elit = loads.end(); lit != elit; ++lit)
    {
        const LoadInst* load = *lit;
        bool loadneedcheck = false;
        for (StoreSet::const_iterator sit = stores.begin(), esit = stores.end(); sit != esit; ++sit)
        {
            const StoreInst* store = *sit;

            loadneedcheck = true;
            needcheckinst.insert(store);
        }
        if (loadneedcheck)
            needcheckinst.insert(load);
    }

    outs() << "HP needcheck: " << needcheckinst.size() << "\n";
}

