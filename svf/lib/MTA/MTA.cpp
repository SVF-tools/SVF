//===- MTA.h -- Analysis of multithreaded programs-------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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

MTA::MTA() : tcg(nullptr), tct(nullptr), mhp(nullptr), lsa(nullptr)
{
    stat = std::make_unique<MTAStat>();
}

MTA::~MTA()
{
    if (tcg)
        delete tcg;
    //if (tct)
    //    delete tct;
    delete mhp;
    delete lsa;
}

/*!
 * Perform data race detection
 */
bool MTA::runOnModule(SVFIR* pag)
{
    mhp = computeMHP(pag->getModule());
    lsa = computeLocksets(mhp->getTCT());



    /*
    if (Options::AndersenAnno()) {
        pta = mhp->getTCT()->getPTA();
        if (pta->printStat())
            stat->performMHPPairStat(mhp,lsa);
        AndersenWaveDiff::releaseAndersenWaveDiff();
    } else if (Options::FSAnno()) {

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
    SVFIR* pag = PAG::getPAG();
    PointerAnalysis* pta = AndersenWaveDiff::createAndersenWaveDiff(pag);
    pta->getPTACallGraph()->dump("ptacg");

    DBOUT(DGENERAL, outs() << pasMsg("Build TCT\n"));
    DBOUT(DMTA, outs() << pasMsg("Build TCT\n"));
    DOTIMESTAT(double tctStart = stat->getClk());
    tct = std::make_unique<TCT>(pta);
    tcg = tct->getThreadCallGraph();
    DOTIMESTAT(double tctEnd = stat->getClk());
    DOTIMESTAT(stat->TCTTime += (tctEnd - tctStart) / TIMEINTERVAL);

    if (pta->printStat())
    {
        stat->performThreadCallGraphStat(tcg);
        stat->performTCTStat(tct.get());
    }

    tcg->dump("tcg");

    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis\n"));

    DOTIMESTAT(double mhpStart = stat->getClk());
    MHP* mhp = new MHP(tct.get());
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
    SVFIR* pag = SVFIR::getPAG();

    Set<const SVFInstruction*> needcheckinst;
    // Add symbols for all of the functions and the instructions in them.
    for (const SVFFunction* F : module->getFunctionSet())
    {
        // collect and create symbols inside the function body
        for (const SVFBasicBlock* svfbb : F->getBasicBlockList())
        {
            for (const SVFInstruction* svfInst : svfbb->getInstructionList())
            {

                for(const SVFStmt* stmt : pag->getSVFStmtList(pag->getICFG()->getICFGNode(svfInst)))
                {
                    if (SVFUtil::isa<LoadStmt>(stmt))
                    {
                        loads.insert(svfInst);
                    }
                    else if (SVFUtil::isa<StoreStmt>(stmt))
                    {
                        stores.insert(svfInst);
                    }
                }
            }
        }
    }

    for (LoadSet::const_iterator lit = loads.begin(), elit = loads.end(); lit != elit; ++lit)
    {
        const SVFInstruction* load = *lit;
        bool loadneedcheck = false;
        for (StoreSet::const_iterator sit = stores.begin(), esit = stores.end(); sit != esit; ++sit)
        {
            const SVFInstruction* store = *sit;

            loadneedcheck = true;
            needcheckinst.insert(store);
        }
        if (loadneedcheck)
            needcheckinst.insert(load);
    }

    outs() << "HP needcheck: " << needcheckinst.size() << "\n";
}

