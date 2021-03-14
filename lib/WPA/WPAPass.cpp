//===- WPAPass.cpp -- Whole program analysis pass------------------------------//
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
#include "Util/SVFModule.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAPass.h"
#include "WPA/Andersen.h"
#include "WPA/AndersenSFR.h"
#include "WPA/FlowSensitive.h"
#include "WPA/FlowSensitiveTBHC.h"
#include "WPA/VersionedFlowSensitive.h"
#include "WPA/TypeAnalysis.h"
#include "WPA/Steensgaard.h"
#include "SVF-FE/PAGBuilder.h"

using namespace SVF;

char WPAPass::ID = 0;

static llvm::RegisterPass<WPAPass> WHOLEPROGRAMPA("wpa",
        "Whole Program Pointer AnalysWPAis Pass");


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
void WPAPass::runOnModule(SVFModule* svfModule)
{
    for (u32_t i = 0; i<= PointerAnalysis::Default_PTA; i++)
    {
        if (Options::PASelected.isSet(i))
            runPointerAnalysis(svfModule, i);
    }
    assert(!ptaVector.empty() && "No pointer analysis is specified.\n");
}

/*!
 * We start from here
 */
bool WPAPass::runOnModule(Module& module)
{
    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(module);
    runOnModule(svfModule);
    return false;
}

/*!
 * Create pointer analysis according to a specified kind and then analyze the module.
 */
void WPAPass::runPointerAnalysis(SVFModule* svfModule, u32_t kind)
{
	/// Build PAG
	PAGBuilder builder;
	PAG* pag = builder.build(svfModule);
    /// Initialize pointer analysis.
    switch (kind)
    {
    case PointerAnalysis::Andersen_WPA:
        _pta = new Andersen(pag);
        break;
    case PointerAnalysis::AndersenLCD_WPA:
        _pta = new AndersenLCD(pag);
        break;
    case PointerAnalysis::AndersenHCD_WPA:
        _pta = new AndersenHCD(pag);
        break;
    case PointerAnalysis::AndersenHLCD_WPA:
        _pta = new AndersenHLCD(pag);
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
    case PointerAnalysis::AndersenWaveDiffWithType_WPA:
        _pta = new AndersenWaveDiffWithType(pag);
        break;
    case PointerAnalysis::Steensgaard_WPA:
        _pta = new Steensgaard(pag);
        break;
    case PointerAnalysis::FSSPARSE_WPA:
        _pta = new FlowSensitive(pag);
        break;
    case PointerAnalysis::FSTBHC_WPA:
        _pta = new FlowSensitiveTBHC(pag);
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
    if (Options::AnderSVFG)
    {
        SVFGBuilder memSSA(true);
        assert(SVFUtil::isa<AndersenBase>(_pta) && "supports only andersen/steensgaard for pre-computed SVFG");
        SVFG *svfg = memSSA.buildFullSVFGWithoutOPT((BVDataPTAImpl*)_pta);
        /// support mod-ref queries only for -ander
        if (Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA))
            _svfg = svfg;
    }

    if (Options::PrintAliases)
        PrintAliasPairs(_pta);
}

void WPAPass::PrintAliasPairs(PointerAnalysis* pta)
{
    PAG* pag = pta->getPAG();
    for (PAG::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit)
    {
        PAGNode* node1 = lit->second;
        PAGNode* node2 = node1;
        for (PAG::iterator rit = lit, erit = pag->end(); rit != erit; ++rit)
        {
            node2 = rit->second;
            if(node1==node2)
                continue;
            const Function* fun1 = node1->getFunction();
            const Function* fun2 = node2->getFunction();
            AliasResult result = pta->alias(node1->getId(), node2->getId());
            SVFUtil::outs()	<< (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")
                            << " var" << node1->getId() << "[" << node1->getValueName()
                            << "@" << (fun1==nullptr?"":fun1->getName()) << "] --"
                            << " var" << node2->getId() << "[" << node2->getValueName()
                            << "@" << (fun2==nullptr?"":fun2->getName()) << "]\n";
        }
    }
}

/*!
 * Return alias results based on our points-to/alias analysis
 * TODO: Need to handle PartialAlias and MustAlias here.
 */
AliasResult WPAPass::alias(const Value* V1, const Value* V2)
{

    AliasResult result = llvm::MayAlias;

    PAG* pag = _pta->getPAG();

    /// TODO: When this method is invoked during compiler optimizations, the IR
    ///       used for pointer analysis may been changed, so some Values may not
    ///       find corresponding PAG node. In this case, we only check alias
    ///       between two Values if they both have PAG nodes. Otherwise, MayAlias
    ///       will be returned.
    if (pag->hasValueNode(V1) && pag->hasValueNode(V2))
    {
        /// Veto is used by default
        if (Options::AliasRule.getBits() == 0 || Options::AliasRule.isSet(Veto))
        {
            /// Return NoAlias if any PTA gives NoAlias result
            result = llvm::MayAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == llvm::NoAlias)
                    result = llvm::NoAlias;
            }
        }
        else if (Options::AliasRule.isSet(Conservative))
        {
            /// Return MayAlias if any PTA gives MayAlias result
            result = llvm::NoAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == llvm::MayAlias)
                    result = llvm::MayAlias;
            }
        }
    }

    return result;
}

/*!
 * Return mod-ref result of a CallInst
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn = icfg->getCallBlockNode(callInst);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn);
}

/*!
 * Return mod-ref results of a CallInst to a specific memory location
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst, const Value* V)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn = icfg->getCallBlockNode(callInst);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn, V);
}

/*!
 * Return mod-ref result between two CallInsts
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst1, const CallInst* callInst2)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn1 = icfg->getCallBlockNode(callInst1);
    const CallBlockNode* cbn2 = icfg->getCallBlockNode(callInst2);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn1, cbn2);
}
