//===- SVFBaseNode.h -- Base Node of SVF Graphs ---------------------------------------//
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
 * SVFBaseNode.h
 *
 *  Created on: Jan 18, 2024
 *      Author: Xiao Cheng
 */
#ifndef SVF_SVFBASENODE_H
#define SVF_SVFBASENODE_H


#include "SVFIR/SVFType.h"

namespace SVF{


class SVFBaseNode
{

public:

    enum GNodeK
    {
        // ┌── ICFGNode: Classes of inter-procedural and intra-procedural control flow graph nodes
        ICFGNodeStart,    // * Start of ICFGNode
        IntraBlock,       // ├──Represents a node within a single procedure
        GlobalBlock,      // ├──Represents a global-level block
        // │   └─ InterICFGNode: Classes of inter-procedural control flow graph nodes
        InterBlockStart,  // ** Start of InterICFGNode
        FunEntryBlock,    // ├──Entry point of a function
        FunExitBlock,     // ├──Exit point of a function
        FunCallBlock,     // ├──Call site in the function
        FunRetBlock,      // ├──Return site in the function
        InterBlockEnd,    // ** End of InterICFGNode
        ICFGNodeEnd,      // * End of ICFGNode
        // └────────

        // ┌── SVFVar: Classes of top-level variables (ValVar) and address-taken variables (ObjVar)
        // │   └── ValVar: Classes of top-level variable nodes
        ValNode,                 // ├──Represents a standard value variable
        ArgNode,                 // ├──Represents an argument value variable
        FunValNode,              // ├──Represents a Function value variable
        GepValNode,              // ├──Represents a GEP value variable
        RetNode,                 // ├──Represents a return value node
        VarargNode,              // ├──Represents a variadic argument node
        GlobalValNode,           // ├──Represents a global variable node
        ConstantDataValNode,     // ├──Represents a constant data variable
        BlackHoleNode,           // ├──Represents a black hole node
        ConstantFPValNode,       // ├──Represents a constant float-point value node
        ConstantIntValNode,      // ├── Represents a constant integer value node
        ConstantNullptrValNode,  // ├── Represents a constant nullptr value node
        DummyValNode,            // ├──Dummy node for uninitialized values
        // │   └── ObjVar: Classes of object variable nodes
        ObjNode,                 // ├──Represents an object variable
        GepObjNode,              // ├──Represents a GEP object variable
        // │        └── BaseObjVar: Classes of base object nodes
        BaseObjNode,             // ├──Represents a base object node
        FunObjNode,              // ├──Types of function object
        HeapObjNode,             // ├──Types of heap object
        StackObjNode,            // ├──Types of stack object
        GlobalObjNode,           // ├──Types of global object
        ConstantDataObjNode,     // ├──Types of constant data object
        ConstantFPObjNode,       // ├──Types of constant float-point object
        ConstantIntObjNode,      // ├──Types of constant integer object
        ConstantNullptrObjNode,  // ├──Types of constant nullptr object
        DummyObjNode,            // ├──Dummy node for uninitialized objects
        // └────────

        // ┌── VFGNode: Classes of Value Flow Graph (VFG) node kinds with operations
        Cmp,              // ├──Represents a comparison operation
        BinaryOp,         // ├──Represents a binary operation
        UnaryOp,          // ├──Represents a unary operation
        Branch,           // ├──Represents a branch operation
        DummyVProp,       // ├──Dummy node for value propagation
        NPtr,             // ├──Represents a null pointer operation
        // │   └── ArgumentVFGNode: Classes of argument nodes in VFG
        FRet,             // ├──Represents a function return value
        ARet,             // ├──Represents an argument return value
        AParm,            // ├──Represents an argument parameter
        FParm,            // ├──Represents a function parameter
        // │   └── StmtVFGNode: Classes of statement nodes in VFG
        Addr,             // ├──Represents an address operation
        Copy,             // ├──Represents a copy operation
        Gep,              // ├──Represents a GEP operation
        Store,            // ├──Represents a store operation
        Load,             // ├──Represents a load operation
        // │   └── PHIVFGNode: Classes of PHI nodes in VFG
        TPhi,             // ├──Represents a type-based PHI node
        TIntraPhi,        // ├──Represents an intra-procedural PHI node
        TInterPhi,        // ├──Represents an inter-procedural PHI node
        // │   └── MRSVFGNode: Classes of Memory-related SVFG nodes
        FPIN,             // ├──Function parameter input
        FPOUT,            // ├──Function parameter output
        APIN,             // ├──Argument parameter input
        APOUT,            // ├──Argument parameter output
        // │        └── MSSAPHISVFGNode: Classes of Mem SSA PHI nodes for SVFG
        MPhi,             // ├──Memory PHI node
        MIntraPhi,        // ├──Intra-procedural memory PHI node
        MInterPhi,        // ├──Inter-procedural memory PHI node
        // └────────

        // Additional specific graph node types
        CallNodeKd,    // Callgraph node
        CDNodeKd,      // Control dependence graph node
        CFLNodeKd,     // CFL graph node
        CHNodeKd,      // Class hierarchy graph node
        ConstraintNodeKd, // Constraint graph node
        TCTNodeKd,     // Thread creation tree node
        DCHNodeKd,     // DCHG node
        OtherKd        // Other node kind
    };



    SVFBaseNode(NodeID i, GNodeK k, SVFType* ty = nullptr): id(i),nodeKind(k), type(ty)
    {

    }

    /// Get ID
    inline NodeID getId() const
    {
        return id;
    }

    /// Get node kind
    inline GNodeK getNodeKind() const
    {
        return nodeKind;
    }

    virtual const SVFType* getType() const
    {
        return type;
    }

    inline virtual void setSourceLoc(const std::string& sourceCodeInfo)
    {
        sourceLoc = sourceCodeInfo;
    }

    virtual const std::string getSourceLoc() const
    {
        return sourceLoc;
    }

    const std::string valueOnlyToString() const;


protected:
    NodeID id;		///< Node ID
    GNodeK nodeKind;	///< Node kind
    const SVFType* type; ///< SVF type

    std::string sourceLoc;  ///< Source code information of this value

    /// Helper functions to check node kinds
    //{@ Check node kind
    static inline bool isICFGNodeKinds(GNodeK n)
    {
        static_assert(ICFGNodeEnd - ICFGNodeStart == 9,
                      "the number of ICFGNodeKinds has changed, make sure "
                      "the range is correct");
        return n < ICFGNodeEnd && n > ICFGNodeStart;
    }

    static inline bool isInterICFGNodeKind(GNodeK n)
    {
        static_assert(InterBlockEnd - InterBlockStart == 5,
                      "the number of InterICFGNodeKind has changed, make sure "
                      "the range is correct");
        return n < InterBlockEnd && n > InterBlockStart;
    }

    static inline bool isSVFVarKind(GNodeK n)
    {
        static_assert(DummyObjNode - ValNode == 24,
                      "The number of SVFVarKinds has changed, make sure the "
                      "range is correct");

        return n <= DummyObjNode && n >= ValNode;
    }

    static inline bool isValVarKinds(GNodeK n)
    {
        static_assert(DummyValNode - ValNode == 12,
                      "The number of ValVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyValNode && n >= ValNode;
    }


    static inline bool isConstantDataValVar(GNodeK n)
    {
        static_assert(ConstantNullptrValNode - ConstantDataValNode == 4,
                      "The number of ConstantDataValVarKinds has changed, make "
                      "sure the range is correct");
        return n <= ConstantNullptrValNode && n >= ConstantDataValNode;
    }

    static inline bool isObjVarKinds(GNodeK n)
    {
        static_assert(DummyObjNode - ObjNode == 11,
                      "The number of ObjVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyObjNode && n >= ObjNode;
    }

    static inline bool isBaseObjVarKinds(GNodeK n)
    {
        static_assert(DummyObjNode - BaseObjNode == 9,
                      "The number of BaseObjVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyObjNode && n >= BaseObjNode;
    }

    static inline bool isConstantDataObjVarKinds(GNodeK n)
    {
        static_assert(ConstantNullptrObjNode - ConstantDataObjNode == 3,
                      "The number of ConstantDataObjVarKinds has changed, make "
                      "sure the range is correct");
        return n <= ConstantNullptrObjNode && n >= ConstantDataObjNode;
    }

    static inline bool isVFGNodeKinds(GNodeK n)
    {
        static_assert(MInterPhi - Cmp == 24,
                      "The number of VFGNodeKinds has changed, make sure the "
                      "range is correct");
        return n <= MInterPhi && n >= Cmp;
    }

    static inline bool isArgumentVFGNodeKinds(GNodeK n)
    {
        static_assert(FParm - FRet == 3,
                      "The number of ArgumentVFGNodeKinds has changed, make "
                      "sure the range is correct");
        return n <= FParm && n >= FRet;
    }

    static inline bool isStmtVFGNodeKinds(GNodeK n)
    {
        static_assert(Load - Addr == 4,
                      "The number of StmtVFGNodeKinds has changed, make sure "
                      "the range is correct");
        return n <= Load && n >= Addr;
    }

    static inline bool isPHIVFGNodeKinds(GNodeK n)
    {
        static_assert(TInterPhi - TPhi == 2,
                      "The number of PHIVFGNodeKinds has changed, make sure "
                      "the range is correct");
        return n <= TInterPhi && n >= TPhi;
    }

    static inline bool isMRSVFGNodeKinds(GNodeK n)
    {
        static_assert(MInterPhi - FPIN == 6,
                      "The number of MRSVFGNodeKinds has changed, make sure "
                      "the range is correct");
        return n <= MInterPhi && n >= FPIN;
    }

    static inline bool isMSSAPHISVFGNodeKinds(GNodeK n)
    {
        static_assert(MInterPhi - MPhi == 2,
                      "The number of MSSAPHISVFGNodeKinds has changed, make "
                      "sure the range is correct");
        return n <= MInterPhi && n >= MPhi;
    }
    //@}
};
}
#endif // SVF_SVFBASENODE_H
