//===- GenericGraph.cpp -- Generic graph ---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * GenericGraph.cpp
 *
 *  Created on: Sep 24, 2024
 *      Author: Xiao Cheng
 */
#include "Graphs/GenericGraph.h"

using namespace SVF;


// ┌── InterICFGNodeKinds: Types of inter-procedural control flow graph nodes
// │   ├── FunEntryBlock
// │   ├── FunExitBlock
// │   ├── FunCallBlock
// │   └── FunRetBlock
SVFBaseNode::GNodeKSet SVFBaseNode::InterICFGNodeKinds = {
        FunEntryBlock,
        FunExitBlock,
        FunCallBlock,
        FunRetBlock
};

// ┌── ICFGNodeKinds: Combines InterICFGNodeKinds with intra-procedural nodes
// │   ├── (inherits from InterICFGNodeKinds)
// │   ├── IntraBlock
// │   └── GlobalBlock
SVFBaseNode::GNodeKSet SVFBaseNode::ICFGNodeKinds = []() {
    GNodeKSet result;
    result.insert(InterICFGNodeKinds.begin(), InterICFGNodeKinds.end());
    result.insert(IntraBlock);
    result.insert(GlobalBlock);
    assert(result.size() == 6 && "6 kinds of ICFGNodes");
    return result;
}();

// ┌── ValVarKinds: Types of value variable nodes
// │   ├── ValNode
// │   ├── GepValNode
// │   ├── RetNode
// │   ├── VarargNode
// │   └── DummyValNode
SVFBaseNode::GNodeKSet SVFBaseNode::ValVarKinds = {
        ValNode,
        GepValNode,
        RetNode,
        VarargNode,
        DummyValNode
};

// ┌── ObjVarKinds: Types of object variable nodes
// │   ├── ObjNode
// │   ├── GepObjNode
// │   ├── FIObjNode
// │   └── DummyObjNode
SVFBaseNode::GNodeKSet SVFBaseNode::ObjVarKinds = {
        ObjNode,
        GepObjNode,
        FIObjNode,
        DummyObjNode
};

// ┌── SVFVarKinds: Combines ValVarKinds and ObjVarKinds
// │   ├── (inherits from ValVarKinds)
// │   └── (inherits from ObjVarKinds)
SVFBaseNode::GNodeKSet SVFBaseNode::SVFVarKinds = []() {
    GNodeKSet result;
    result.insert(ValVarKinds.begin(), ValVarKinds.end());
    result.insert(ObjVarKinds.begin(), ObjVarKinds.end());
    assert(result.size() == 9 && "9 kinds of SVFVar");
    return result;
}();

// ┌── MSSAPHISVFGNodeKinds: Types of SSA PHI nodes for SVFG
// │   ├── MPhi
// │   ├── MIntraPhi
// │   └── MInterPhi
SVFBaseNode::GNodeKSet SVFBaseNode::MSSAPHISVFGNodeKinds = {
        MPhi, MIntraPhi, MInterPhi
};

// ┌── MRSVFGNodeKinds: Combines MSSAPHISVFGNodeKinds with additional nodes
// │   ├── (inherits from MSSAPHISVFGNodeKinds)
// │   ├── FPIN
// │   ├── FPOUT
// │   ├── APIN
// │   └── APOUT
SVFBaseNode::GNodeKSet SVFBaseNode::MRSVFGNodeKinds = []() {
    GNodeKSet result;
    result.insert(MSSAPHISVFGNodeKinds.begin(), MSSAPHISVFGNodeKinds.end());
    result.insert(FPIN);
    result.insert(FPOUT);
    result.insert(APIN);
    result.insert(APOUT);
    return result;
}();

// ┌── StmtVFGNodeKinds: Types of statement nodes in VFG
// │   ├── Addr
// │   ├── Copy
// │   ├── Gep
// │   ├── Store
// │   └── Load
SVFBaseNode::GNodeKSet SVFBaseNode::StmtVFGNodeKinds = {
        Addr, Copy, Gep, Store, Load
};

// ┌── PHIVFGNodeKinds: Types of PHI nodes in VFG
// │   ├── TPhi
// │   ├── TIntraPhi
// │   └── TInterPhi
SVFBaseNode::GNodeKSet SVFBaseNode::PHIVFGNodeKinds = {
        TPhi, TIntraPhi, TInterPhi
};

// ┌── ArgumentVFGNodeKinds: Types of argument nodes in VFG
// │   ├── FRet
// │   ├── ARet
// │   ├── AParm
// │   └── FParm
SVFBaseNode::GNodeKSet SVFBaseNode::ArgumentVFGNodeKinds = {
        FRet, ARet, AParm, FParm
};

// ┌── VFGNodeKinds: Combines various VFG node kinds with additional operations
// │   ├── (inherits from MRSVFGNodeKinds)
// │   ├── (inherits from StmtVFGNodeKinds)
// │   ├── (inherits from PHIVFGNodeKinds)
// │   ├── (inherits from ArgumentVFGNodeKinds)
// │   ├── Cmp
// │   ├── BinaryOp
// │   ├── UnaryOp
// │   ├── Branch
// │   └── NPtr
SVFBaseNode::GNodeKSet SVFBaseNode::VFGNodeKinds = []() {
    GNodeKSet result;
    result.insert(MRSVFGNodeKinds.begin(), MRSVFGNodeKinds.end());
    result.insert(StmtVFGNodeKinds.begin(), StmtVFGNodeKinds.end());
    result.insert(PHIVFGNodeKinds.begin(), PHIVFGNodeKinds.end());
    result.insert(ArgumentVFGNodeKinds.begin(), ArgumentVFGNodeKinds.end());
    result.insert(Cmp);
    result.insert(BinaryOp);
    result.insert(UnaryOp);
    result.insert(Branch);
    result.insert(NPtr);
    result.insert(DummyVProp);
    assert(result.size() == 25 && "25 kinds of VFGNode");
    return result;
}();
