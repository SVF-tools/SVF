//===----- CHGBuiler.h -- Class hierarchy graph builder ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CHGBuiler.h
 *
 *  Created on: Jun 4, 2021
 *      Author: Yulei Sui
 */

#include "Graphs/CHG.h"

namespace SVF{

class CHGBuilder{

private:
    CHGraph* chg;

public:
    typedef CHGraph::CHNodeSetTy CHNodeSetTy;
    typedef CHGraph::WorkList WorkList;

    CHGBuilder(CHGraph* c): chg(c)
    {

    }
    void buildCHG();
    void buildCHGNodes(const GlobalValue *V);
    void buildCHGNodes(const SVFFunction* F);
    void buildCHGEdges(const SVFFunction* F);
    void buildInternalMaps();
    void readInheritanceMetadataFromModule(const Module &M);

    CHNode *createNode(const std::string name);

    void connectInheritEdgeViaCall(const SVFFunction* caller, CallSite cs);
    void connectInheritEdgeViaStore(const SVFFunction* caller, const StoreInst* store);

    void buildClassNameToAncestorsDescendantsMap();
    const CHGraph::CHNodeSetTy& getInstancesAndDescendants(const std::string className);

    void analyzeVTables(const Module &M);
    void buildVirtualFunctionToIDMap();
    void buildCSToCHAVtblsAndVfnsMap();
    const CHNodeSetTy& getCSClasses(CallSite cs);
    void addFuncToFuncVector(CHNode::FuncVector &v, const SVFFunction *f);
};

}