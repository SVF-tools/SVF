//===- WPAPass.cpp -- Whole program analysis pass------------------------------//
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
//===-----------------------------------------------------------------------===//

/*
 * @file: WPA.cpp
 * @author: yesen
 * @date: 10/06/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#include "Util/Options.h"
#include "SVFIR/SVFModule.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAPass.h"
#include "WPA/Andersen.h"
#include "WPA/AndersenPWC.h"
#include "WPA/FlowSensitive.h"
#include "WPA/VersionedFlowSensitive.h"
#include "WPA/TypeAnalysis.h"
#include "WPA/Steensgaard.h"

using namespace SVF;

char WPAPass::ID = 0;

/*!
 * Destructor
 */
WPAPass::~WPAPass()
{
    PTAVector::const_iterator it = ptaVector.begin();
    PTAVector::const_iterator eit = ptaVector.end();
    for (; it != eit; ++it)
    {
        PointerAnalysis* pta = *it;
        delete pta;
    }
    ptaVector.clear();
}

/*!
 * We start from here
 */
void WPAPass::runOnModule(SVFIR* pag)
{
    for (u32_t i = 0; i<= PointerAnalysis::Default_PTA; i++)
    {
        PointerAnalysis::PTATY iPtaTy = static_cast<PointerAnalysis::PTATY>(i);
        if (Options::PASelected(iPtaTy))
            runPointerAnalysis(pag, i);
    }
    assert(!ptaVector.empty() && "No pointer analysis is specified.\n");
}

/*!
 * Create pointer analysis according to a specified kind and then analyze the module.
 */
void WPAPass::runPointerAnalysis(SVFIR* pag, u32_t kind)
{
    /// Initialize pointer analysis.
    switch (kind)
    {
    case PointerAnalysis::Andersen_WPA:
        _pta = new Andersen(pag);
        break;
    case PointerAnalysis::AndersenSCD_WPA:
        _pta = new AndersenSCD(pag);
        break;
    case PointerAnalysis::AndersenSFR_WPA:
        _pta = new AndersenSFR(pag);
        break;
    case PointerAnalysis::AndersenWaveDiff_WPA:
        _pta = new AndersenWaveDiff(pag);
        break;
    case PointerAnalysis::Steensgaard_WPA:
        _pta = new Steensgaard(pag);
        break;
    case PointerAnalysis::FSSPARSE_WPA:
        _pta = new FlowSensitive(pag);
        break;
    case PointerAnalysis::VFS_WPA:
        _pta = new VersionedFlowSensitive(pag);
        break;
    case PointerAnalysis::TypeCPP_WPA:
        _pta = new TypeAnalysis(pag);
        break;
    default:
        assert(false && "This pointer analysis has not been implemented yet.\n");
        return;
    }

    ptaVector.push_back(_pta);
    _pta->analyze();
    if (Options::AnderSVFG())
    {
        SVFGBuilder memSSA(true);
        assert(SVFUtil::isa<AndersenBase>(_pta) && "supports only andersen/steensgaard for pre-computed SVFG");
        SVFG *svfg = memSSA.buildFullSVFG((BVDataPTAImpl*)_pta);
        /// support mod-ref queries only for -ander
        if (Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA))
            _svfg = svfg;
    }

    if (Options::PrintAliases())
        PrintAliasPairs(_pta);
}

void WPAPass::PrintAliasPairs(PointerAnalysis* pta)
{
    SVFIR* pag = pta->getPAG();
    for (SVFIR::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit)
    {
        PAGNode* node1 = lit->second;
        PAGNode* node2 = node1;
        for (SVFIR::iterator rit = lit, erit = pag->end(); rit != erit; ++rit)
        {
            node2 = rit->second;
            if(node1==node2)
                continue;
            const SVFFunction* fun1 = node1->getFunction();
            const SVFFunction* fun2 = node2->getFunction();
            AliasResult result = pta->alias(node1->getId(), node2->getId());
            SVFUtil::outs()	<< (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")
                            << " var" << node1->getId() << "[" << node1->getValueName()
                            << "@" << (fun1==nullptr?"":fun1->getName()) << "] --"
                            << " var" << node2->getId() << "[" << node2->getValueName()
                            << "@" << (fun2==nullptr?"":fun2->getName()) << "]\n";
        }
    }
}

const PointsTo& WPAPass::getPts(const SVFValue* value)
{
    assert(_pta && "initialize a pointer analysis first");
    SVFIR* pag = _pta->getPAG();
    return getPts(pag->getValueNode(value));
}

const PointsTo& WPAPass::getPts(NodeID var)
{
    assert(_pta && "initialize a pointer analysis first");
    return _pta->getPts(var);
}

/*!
 * Return alias results based on our points-to/alias analysis
 * TODO: Need to handle PartialAlias and MustAlias here.
 */
AliasResult WPAPass::alias(const SVFValue* V1, const SVFValue* V2)
{

    AliasResult result = AliasResult::MayAlias;

    SVFIR* pag = _pta->getPAG();

    /// TODO: When this method is invoked during compiler optimizations, the IR
    ///       used for pointer analysis may been changed, so some Values may not
    ///       find corresponding SVFIR node. In this case, we only check alias
    ///       between two Values if they both have SVFIR nodes. Otherwise, MayAlias
    ///       will be returned.
    if (pag->hasValueNode(V1) && pag->hasValueNode(V2))
    {
        /// Veto is used by default
        if (Options::AliasRule.nothingSet() || Options::AliasRule(Veto))
        {
            /// Return NoAlias if any PTA gives NoAlias result
            result = AliasResult::MayAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == AliasResult::NoAlias)
                    result = AliasResult::NoAlias;
            }
        }
        else if (Options::AliasRule(Conservative))
        {
            /// Return MayAlias if any PTA gives MayAlias result
            result = AliasResult::NoAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == AliasResult::MayAlias)
                    result = AliasResult::MayAlias;
            }
        }
    }

    return result;
}

/*!
 * Return mod-ref result of a Callsite
 */
ModRefInfo WPAPass::getModRefInfo(const CallSite callInst)
{
    assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallICFGNode* cbn = icfg->getCallICFGNode(callInst.getInstruction());
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn);
}

/*!
 * Return mod-ref results of a Callsite to a specific memory location
 */
ModRefInfo WPAPass::getModRefInfo(const CallSite callInst, const SVFValue* V)
{
    assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallICFGNode* cbn = icfg->getCallICFGNode(callInst.getInstruction());
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn, V);
}

/*!
 * Return mod-ref result between two CallInsts
 */
ModRefInfo WPAPass::getModRefInfo(const CallSite callInst1, const CallSite callInst2)
{
    assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallICFGNode* cbn1 = icfg->getCallICFGNode(callInst1.getInstruction());
    const CallICFGNode* cbn2 = icfg->getCallICFGNode(callInst2.getInstruction());
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn1, cbn2);
}
