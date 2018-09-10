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

#include "WPA/TypeAnalysis.h"
#include "MemoryModel/CHA.h"
#include "Util/CPPUtil.h"
#include "Util/PTAStat.h"

using namespace llvm;
using namespace cppUtil;
using namespace std;

/// Initialize analysis
void TypeAnalysis::initialize(SVFModule svfModule) {
    PointerAnalysis::initialize(svfModule);
    stat = new PTAStat(this);
}

/// Finalize analysis
void TypeAnalysis::finalize() {
    PointerAnalysis::finalize();
    dumpCHAStats();
}

void TypeAnalysis::analyze(SVFModule svfModule){
	initialize(svfModule);
    CallEdgeMap newEdges;
	callGraphSolveBasedOnCHA(getIndirectCallsites(), newEdges);
	finalize();
}

void TypeAnalysis::callGraphSolveBasedOnCHA(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges) {
    for(CallSiteToFunPtrMap::const_iterator iter = callsites.begin(), eiter = callsites.end(); iter!=eiter; ++iter) {
        CallSite cs = iter->first;
        if (isVirtualCallSite(cs)) {
        		virtualCallSites.insert(cs);
            const Value *vtbl = getVCallVtblPtr(cs);
            assert(pag->hasValueNode(vtbl));
            NodeID vtblId = pag->getValueNode(vtbl);
            VFunSet vfns;
            getVFnsFromCHA(cs, vfns);
            connectVCallToVFns(cs, vfns, newEdges);
        }
    }
}


void TypeAnalysis::dumpCHAStats() {

	const CHGraph *chgraph = getCHGraph();

    s32_t pure_abstract_class_num = 0,
          multi_inheritance_class_num = 0;
    for (CHGraph::const_iterator it = chgraph->begin(), eit = chgraph->end();
            it != eit; ++it) {
        CHNode *node = it->second;
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
    s32_t vtblnum = 0,
          vfunc_total = 0,
          vtbl_max = 0,
          pure_abstract = 0;
    set<const Function*> allVirtualFunctions;
    for (CHGraph::const_iterator it = chgraph->begin(), eit = chgraph->end();
            it != eit; ++it) {
        CHNode *node = it->second;
        if (node->isPureAbstract())
            pure_abstract++;

        s32_t vfuncs_size = 0;
        const vector<CHNode::FuncVector>& vecs = node->getVirtualFunctionVectors();
        for (vector<CHNode::FuncVector>::const_iterator vit = vecs.begin(),
                veit = vecs.end(); vit != veit; ++vit) {
            vfuncs_size += (*vit).size();
            for (vector<const Function*>::const_iterator fit = (*vit).begin(),
                    feit = (*vit).end(); fit != feit; ++fit) {
                const Function *func = *fit;
                allVirtualFunctions.insert(func);
            }
        }
        if (vfuncs_size > 0) {
            vtblnum++;
            if (vfuncs_size > vtbl_max) {
                vtbl_max = vfuncs_size;
            }
        }
    }
    vfunc_total = allVirtualFunctions.size();

    outs() << "vtblnum:\t" << vtblnum << '\n';
    outs() << "vtbl_average:\t" << (double)(vfunc_total)/vtblnum << '\n';
    outs() << "vtbl_max:\t" << vtbl_max << '\n';
}


