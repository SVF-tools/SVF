//===- Andersen.h -- Field-sensitive Andersen's pointer analysis-------------//
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
 * TypeAnalysis.cpp
 *
 *  Created on: 7 Sep. 2018
 *      Author: Yulei Sui
 */

#include "SVF-FE/CPPUtil.h"
#include "SVF-FE/ICFGBuilder.h"
#include "SVF-FE/CHG.h"
#include "WPA/TypeAnalysis.h"
#include "MemoryModel/PTAStat.h"
#include "Graphs/ICFGStat.h"
#include "Graphs/VFG.h"

using namespace SVF;
using namespace SVFUtil;
using namespace cppUtil;
using namespace std;

llvm::cl::opt<bool> genICFG("genicfg", llvm::cl::init(true), llvm::cl::desc("Generate ICFG graph"));

/// Initialize analysis
void TypeAnalysis::initialize()
{
	AndersenBase::initialize();
    if (genICFG)
    {
        icfg = PAG::getPAG()->getICFG();
        icfg->dump("icfg_initial");
        icfg->dump("vfg_initial");
        if (print_stat)
        {
            ICFGStat stat(icfg);
            stat.performStat();
        }
    }
}

/// Finalize analysis
void TypeAnalysis::finalize()
{
    AndersenBase::finalize();
    if (print_stat)
        dumpCHAStats();
}

void TypeAnalysis::analyze()
{
    initialize();
    CallEdgeMap newEdges;
    callGraphSolveBasedOnCHA(getIndirectCallsites(), newEdges);
    finalize();
}

void TypeAnalysis::callGraphSolveBasedOnCHA(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges)
{
    for(auto callsite : callsites)
    {
        const CallBlockNode* cbn = callsite.first;
        CallSite cs = SVFUtil::getLLVMCallSite(cbn->getCallSite());
        if (isVirtualCallSite(cs))
        {
            virtualCallSites.insert(cs);
            const Value *vtbl = getVCallVtblPtr(cs);
            assert(pag->hasValueNode(vtbl));
            VFunSet vfns;
            getVFnsFromCHA(cbn, vfns);
            connectVCallToVFns(cbn, vfns, newEdges);
        }
    }
}


void TypeAnalysis::dumpCHAStats()
{

    const CHGraph *chgraph = SVFUtil::dyn_cast<CHGraph>(getCHGraph());
    if (chgraph == nullptr)
    {
        SVFUtil::errs() << "dumpCHAStats only implemented for standard CHGraph.\n";
        return;
    }

    s32_t pure_abstract_class_num = 0;
    s32_t multi_inheritance_class_num = 0;
    for (auto it : *chgraph)
    {
        CHNode *node = it.second;
        outs() << "class " << node->getName() << "\n";
        if (node->isPureAbstract())
            pure_abstract_class_num++;
        if (node->isMultiInheritance())
            multi_inheritance_class_num++;
    }
    outs() << "class_num:\t" << chgraph->getTotalNodeNum() << '\n';
    outs() << "pure_abstract_class_num:\t" << pure_abstract_class_num << '\n';
    outs() << "multi_inheritance_class_num:\t" << multi_inheritance_class_num << '\n';

    /*
     * count the following info:
     * vtblnum
     * total vfunction
     * vtbl max vfunction
     * pure abstract class
     */
    s32_t vtblnum = 0;
    s32_t vfunc_total = 0;
    s32_t vtbl_max = 0;
    s32_t pure_abstract = 0;
    set<const SVFFunction*> allVirtualFunctions;
    for (auto it : *chgraph)
    {
        CHNode *node = it.second;
        if (node->isPureAbstract())
            pure_abstract++;

        s32_t vfuncs_size = 0;
        const vector<CHNode::FuncVector>& vecs = node->getVirtualFunctionVectors();
        for (const auto & vec : vecs)
        {
            vfuncs_size += vec.size();
            for (const auto *func : vec)
            {
                 allVirtualFunctions.insert(func);
            }
        }
        if (vfuncs_size > 0)
        {
            vtblnum++;
            if (vfuncs_size > vtbl_max)
            {
                vtbl_max = vfuncs_size;
            }
        }
    }
    vfunc_total = allVirtualFunctions.size();

    outs() << "vtblnum:\t" << vtblnum << '\n';
    outs() << "vtbl_average:\t" << (double)(vfunc_total)/vtblnum << '\n';
    outs() << "vtbl_max:\t" << vtbl_max << '\n';
}


