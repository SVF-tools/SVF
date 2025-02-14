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
    mhp = computeMHP();
    lsa = computeLocksets(mhp->getTCT());

    if(Options::RaceCheck())
        detect();

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

MHP* MTA::computeMHP()
{

    DBOUT(DGENERAL, outs() << pasMsg("MTA analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MTA analysis\n"));
    SVFIR* pag = PAG::getPAG();
    PointerAnalysis* pta = AndersenWaveDiff::createAndersenWaveDiff(pag);
    pta->getCallGraph()->dump("ptacg");

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
// * Check   (1) write-read race
// * 		 (2) write-write race (optional)
// * 		 (3) read-read race (optional)
// * when two memory access may-happen in parallel and are not protected by the same lock
// * (excluding global constraints because they are initialized before running the main function)
// */
void MTA::detect()
{

    DBOUT(DGENERAL, outs() << pasMsg("Starting Race Detection\n"));

    Set<const LoadStmt*> loads;
    Set<const StoreStmt*> stores;
    SVFIR* pag = SVFIR::getPAG();
    PointerAnalysis* pta = AndersenWaveDiff::createAndersenWaveDiff(pag);

    // Add symbols for all of the functions and the instructions in them.
    for (const auto& item : *PAG::getPAG()->getCallGraph())
    {
        const FunObjVar* F = item.second->getFunction();
        // collect and create symbols inside the function body
        for (auto it : *F)
        {
            const SVFBasicBlock* svfbb = it.second;
            for (const ICFGNode* icfgNode : svfbb->getICFGNodeList())
            {
                for(const SVFStmt* stmt : pag->getSVFStmtList(icfgNode))
                {
                    if (const LoadStmt* l = SVFUtil::dyn_cast<LoadStmt>(stmt))
                    {
                        loads.insert(l);
                    }
                    else if (const StoreStmt* s = SVFUtil::dyn_cast<StoreStmt>(stmt))
                    {
                        stores.insert(s);
                    }
                }
            }
        }
    }

    for (Set<const LoadStmt*>::const_iterator lit = loads.begin(), elit = loads.end(); lit != elit; ++lit)
    {
        const LoadStmt* load = *lit;
        for (Set<const StoreStmt*>::const_iterator sit = stores.begin(), esit = stores.end(); sit != esit; ++sit)
        {
            const StoreStmt* store = *sit;
            if(SVFUtil::isa<GlobalICFGNode>(load->getICFGNode()) || SVFUtil::isa<GlobalICFGNode>(store->getICFGNode()))
                continue;
            if(mhp->mayHappenInParallelInst(load->getICFGNode(),store->getICFGNode()) && pta->alias(load->getRHSVarID(),store->getLHSVarID()))
                if(lsa->isProtectedByCommonLock(load->getICFGNode(),store->getICFGNode()) == false)
                    outs() << SVFUtil::bugMsg1("race pair(") << " store: " << store->toString() << ", load: " << load->toString() << SVFUtil::bugMsg1(")") << "\n";
        }
    }
}

