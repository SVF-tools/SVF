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


#include "Util/SVFModule.h"
#include "MemoryModel/PointerAnalysis.h"
#include "WPA/WPAPass.h"
#include "WPA/Andersen.h"
#include "WPA/AndersenSFR.h"
#include "WPA/FlowSensitive.h"
#include "WPA/TypeAnalysis.h"

char WPAPass::ID = 0;

static llvm::RegisterPass<WPAPass> WHOLEPROGRAMPA("wpa",
        "Whole Program Pointer Analysis Pass");

/// register this into alias analysis group
///static RegisterAnalysisGroup<AliasAnalysis> AA_GROUP(WHOLEPROGRAMPA);

static llvm::cl::bits<PointerAnalysis::PTATY> PASelected(llvm::cl::desc("Select pointer analysis"),
		llvm::cl::values(
            clEnumValN(PointerAnalysis::Andersen_WPA, "nander", "Standard inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenLCD_WPA, "lander", "Lazy cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenHCD_WPA, "hander", "Hybrid cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenHLCD_WPA, "hlander", "Hybrid lazy cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenSCD_WPA, "sander", "Selective cycle detection inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenSFR_WPA, "sfrander", "Stride-based field representation includion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenWaveDiff_WPA, "wander", "Wave propagation inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenWaveDiff_WPA, "ander", "Diff wave propagation inclusion-based analysis"),
            clEnumValN(PointerAnalysis::AndersenWaveDiffWithType_WPA, "andertype", "Diff wave propagation with type inclusion-based analysis"),
            clEnumValN(PointerAnalysis::FSSPARSE_WPA, "fspta", "Sparse flow sensitive pointer analysis"),
			clEnumValN(PointerAnalysis::TypeCPP_WPA, "type", "Type-based fast analysis for Callgraph, PAG and CHA")
        ));


static llvm::cl::bits<WPAPass::AliasCheckRule> AliasRule(llvm::cl::desc("Select alias check rule"),
		llvm::cl::values(
            clEnumValN(WPAPass::Conservative, "conservative", "return MayAlias if any pta says alias"),
            clEnumValN(WPAPass::Veto, "veto", "return NoAlias if any pta says no alias")
        ));

static llvm::cl::opt<bool> anderSVFG("svfg", llvm::cl::init(false),
                        llvm::cl::desc("Generate SVFG after Andersen's Analysis"));

static llvm::cl::opt<bool> printAliases("print-aliases", llvm::cl::init(false),
                        llvm::cl::desc("Print results for all pair aliases"));


/*!
 * Destructor
 */
WPAPass::~WPAPass() {
    PTAVector::const_iterator it = ptaVector.begin();
    PTAVector::const_iterator eit = ptaVector.end();
    for (; it != eit; ++it) {
        PointerAnalysis* pta = *it;
        delete pta;
    }
    ptaVector.clear();
}

/*!
 * We start from here
 */
void WPAPass::runOnModule(SVFModule svfModule) {
    for (u32_t i = 0; i<= PointerAnalysis::Default_PTA; i++) {
        if (PASelected.isSet(i))
            runPointerAnalysis(svfModule, i);
    }
    assert(!ptaVector.empty() && "No pointer analysis is specified.\n");
}


/*!
 * Create pointer analysis according to a specified kind and then analyze the module.
 */
void WPAPass::runPointerAnalysis(SVFModule svfModule, u32_t kind)
{
    /// Initialize pointer analysis.
    switch (kind) {
        case PointerAnalysis::Andersen_WPA:
            _pta = new Andersen();
            break;
        case PointerAnalysis::AndersenLCD_WPA:
            _pta = new AndersenLCD();
            break;
        case PointerAnalysis::AndersenHCD_WPA:
            _pta = new AndersenHCD();
            break;
        case PointerAnalysis::AndersenHLCD_WPA:
            _pta = new AndersenHLCD();
            break;
        case PointerAnalysis::AndersenSCD_WPA:
            _pta = new AndersenSCD();
            break;
        case PointerAnalysis::AndersenSFR_WPA:
            _pta = new AndersenSFR();
            break;
        case PointerAnalysis::AndersenWaveDiff_WPA:
            _pta = new AndersenWaveDiff();
            break;
        case PointerAnalysis::AndersenWaveDiffWithType_WPA:
            _pta = new AndersenWaveDiffWithType();
            break;
        case PointerAnalysis::FSSPARSE_WPA:
            _pta = new FlowSensitive();
            break;
        case PointerAnalysis::TypeCPP_WPA:
            _pta = new TypeAnalysis();
            break;
        default:
            assert(false && "This pointer analysis has not been implemented yet.\n");
            return;
    }

    ptaVector.push_back(_pta);
    _pta->analyze(svfModule);
    if (anderSVFG) {
        SVFGBuilder memSSA(true);
        assert(SVFUtil::isa<Andersen>(_pta) && "supports only andersen for pre-computed SVFG");
        SVFG *svfg = memSSA.buildFullSVFG((BVDataPTAImpl*)_pta);
        svfg->dump("ander_svfg");
    }

	if (printAliases)
		PrintAliasPairs(_pta);
}

void WPAPass::PrintAliasPairs(PointerAnalysis* pta) {
	PAG* pag = pta->getPAG();
	for (PAG::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit) {
		PAGNode* node1 = lit->second;
		PAGNode* node2 = node1;
		for (PAG::iterator rit = lit, erit = pag->end(); rit != erit; ++rit) {
			node2 = rit->second;
			if(node1==node2)
				continue;
			const Function* fun1 = node1->getFunction();
			const Function* fun2 = node2->getFunction();
			AliasResult result = pta->alias(node1->getId(), node2->getId());
			SVFUtil::outs()	<< (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")
					<< " var" << node1->getId() << "[" << node1->getValueName()
					<< "@" << (fun1==NULL?"":fun1->getName()) << "] --"
					<< " var" << node2->getId() << "[" << node2->getValueName()
					<< "@" << (fun2==NULL?"":fun2->getName()) << "]\n";
		}
	}
}

/*!
 * Return alias results based on our points-to/alias analysis
 * TODO: Need to handle PartialAlias and MustAlias here.
 */
AliasResult WPAPass::alias(const Value* V1, const Value* V2) {

    AliasResult result = llvm::MayAlias;

    PAG* pag = _pta->getPAG();

    /// TODO: When this method is invoked during compiler optimizations, the IR
    ///       used for pointer analysis may been changed, so some Values may not
    ///       find corresponding PAG node. In this case, we only check alias
    ///       between two Values if they both have PAG nodes. Otherwise, MayAlias
    ///       will be returned.
    if (pag->hasValueNode(V1) && pag->hasValueNode(V2)) {
        /// Veto is used by default
        if (AliasRule.getBits() == 0 || AliasRule.isSet(Veto)) {
            /// Return NoAlias if any PTA gives NoAlias result
            result = llvm::MayAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it) {
                if ((*it)->alias(V1, V2) == llvm::NoAlias)
                    result = llvm::NoAlias;
            }
        }
        else if (AliasRule.isSet(Conservative)) {
            /// Return MayAlias if any PTA gives MayAlias result
            result = llvm::NoAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it) {
                if ((*it)->alias(V1, V2) == llvm::MayAlias)
                    result = llvm::MayAlias;
            }
        }
    }

    return result;
}
