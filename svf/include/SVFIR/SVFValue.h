//===- SVFValue.h -- Basic types used in SVF-------------------------------//
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
 * SVFValue.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 *  Refactored on: Feb 10, 2025
 *      Author: Xiao Cheng, Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SVFVALUE_H_
#define INCLUDE_SVFIR_SVFVALUE_H_

#include "SVFIR/SVFType.h"
#include "Graphs/GraphPrinter.h"


namespace SVF
{


class SVFValue
{

public:

    enum GNodeK
    {
        // ┌─────────────────────────────────────────────────────────────────────────┐
        // │ ICFGNode: Classes of inter-procedural and intra-procedural control flow │
        // │ graph nodes (Parent class: ICFGNode)                                   │
        // └─────────────────────────────────────────────────────────────────────────┘
        IntraBlock,       // ├── Represents a node within a single procedure
        GlobalBlock,      // ├── Represents a global-level block
        // │   └─ Subclass: InterICFGNode
        FunEntryBlock,    // │   ├── Entry point of a function
        FunExitBlock,     // │   ├── Exit point of a function
        FunCallBlock,     // │   ├── Call site in the function
        FunRetBlock,      // │   └── Return site in the function

        // ┌─────────────────────────────────────────────────────────────────────────┐
        // │ SVFVar: Classes of variable nodes (Parent class: SVFVar)               │
        // │   Includes two main subclasses: ValVar and ObjVar                      │
        // └─────────────────────────────────────────────────────────────────────────┘
        // └─ Subclass: ValVar (Top-level variable nodes)
        ValNode,                 // ├── Represents a standard value variable
        ArgValNode,              // ├── Represents an argument value variable
        FunValNode,              // ├── Represents a function value variable
        GepValNode,              // ├── Represents a GEP value variable
        RetValNode,              // ├── Represents a return value node
        VarargValNode,           // ├── Represents a variadic argument node
        GlobalValNode,           // ├── Represents a global variable node
        ConstAggValNode,         // ├── Represents a constant aggregate value node
        // │   └─ Subclass: ConstDataValVar
        ConstDataValNode,        // │   ├── Represents a constant data variable
        BlackHoleValNode,        // │   ├── Represents a black hole node
        ConstFPValNode,          // │   ├── Represents a constant floating-point value node
        ConstIntValNode,         // │   ├── Represents a constant integer value node
        ConstNullptrValNode,     // │   └── Represents a constant nullptr value node
        // │   └─ Subclass: DummyValVar
        DummyValNode,            // │   └── Dummy node for uninitialized values

        // └─ Subclass: ObjVar (Object variable nodes)
        ObjNode,                 // ├── Represents an object variable
        // │   └─ Subclass: GepObjVar
        GepObjNode,              // │   ├── Represents a GEP object variable
        // │   └─ Subclass: BaseObjVar
        BaseObjNode,             // │   ├── Represents a base object node
        FunObjNode,              // │   ├── Represents a function object
        HeapObjNode,             // │   ├── Represents a heap object
        StackObjNode,            // │   ├── Represents a stack object
        GlobalObjNode,           // │   ├── Represents a global object
        ConstAggObjNode,         // │   ├── Represents a constant aggregate object
        // │   └─ Subclass: ConstDataObjVar
        ConstDataObjNode,        // │   ├── Represents a constant data object
        ConstFPObjNode,          // │   ├── Represents a constant floating-point object
        ConstIntObjNode,         // │   ├── Represents a constant integer object
        ConstNullptrObjNode,     // │   └── Represents a constant nullptr object
        // │   └─ Subclass: DummyObjVar
        DummyObjNode,            // │   └── Dummy node for uninitialized objects

        // ┌─────────────────────────────────────────────────────────────────────────┐
        // │ VFGNode: Classes of Value Flow Graph (VFG) node kinds (Parent class:   │
        // │ VFGNode)                                                               │
        // │   Includes operation nodes and specialized subclasses                  │
        // └─────────────────────────────────────────────────────────────────────────┘
        Cmp,              // ├── Represents a comparison operation
        BinaryOp,         // ├── Represents a binary operation
        UnaryOp,          // ├── Represents a unary operation
        Branch,           // ├── Represents a branch operation
        DummyVProp,       // ├── Dummy node for value propagation
        NPtr,             // ├── Represents a null pointer operation
        // │   └─ Subclass: ArgumentVFGNode
        FRet,             // │   ├── Represents a function return value
        ARet,             // │   ├── Represents an argument return value
        AParm,            // │   ├── Represents an argument parameter
        FParm,            // │   └── Represents a function parameter
        // │   └─ Subclass: StmtVFGNode
        Addr,             // │   ├── Represents an address operation
        Copy,             // │   ├── Represents a copy operation
        Gep,              // │   ├── Represents a GEP operation
        Store,            // │   ├── Represents a store operation
        Load,             // │   └── Represents a load operation
        // │   └─ Subclass: PHIVFGNode
        TPhi,             // │   ├── Represents a type-based PHI node
        TIntraPhi,        // │   ├── Represents an intra-procedural PHI node
        TInterPhi,        // │   └── Represents an inter-procedural PHI node
        // │   └─ Subclass: MRSVFGNode
        FPIN,             // │   ├── Function parameter input
        FPOUT,            // │   ├── Function parameter output
        APIN,             // │   ├── Argument parameter input
        APOUT,            // │   └── Argument parameter output
        // │       └─ Subclass: MSSAPHISVFGNode
        MPhi,             // │       ├── Memory PHI node
        MIntraPhi,        // │       ├── Intra-procedural memory PHI node
        MInterPhi,        // │       └── Inter-procedural memory PHI node

        // ┌─────────────────────────────────────────────────────────────────────────┐
        // │ Additional specific graph node types                                   │
        // └─────────────────────────────────────────────────────────────────────────┘
        CallNodeKd,       // Callgraph node
        CDNodeKd,         // Control dependence graph node
        CFLNodeKd,        // CFL graph node
        CHNodeKd,         // Class hierarchy graph node
        ConstraintNodeKd, // Constraint graph node
        TCTNodeKd,        // Thread creation tree node
        DCHNodeKd,        // DCHG node
        BasicBlockKd,     // Basic block node
        OtherKd           // Other node kind
    };


    SVFValue(NodeID i, GNodeK k, const SVFType* ty = nullptr): id(i),nodeKind(k), type(ty)
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

    inline virtual void setName(const std::string& nameInfo)
    {
        name = nameInfo;
    }

    inline virtual void setName(std::string&& nameInfo)
    {
        name = std::move(nameInfo);
    }

    virtual const std::string& getName() const
    {
        return name;
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

    std::string name;
    std::string sourceLoc;  ///< Source code information of this value

    /// Helper functions to check node kinds
    //{@ Check node kind
    static inline bool isICFGNodeKinds(GNodeK n)
    {
        static_assert(FunRetBlock - IntraBlock == 5,
                      "the number of ICFGNodeKinds has changed, make sure "
                      "the range is correct");
        return n <= FunRetBlock && n >= IntraBlock;
    }

    static inline bool isInterICFGNodeKind(GNodeK n)
    {
        static_assert(FunRetBlock - FunEntryBlock == 3,
                      "the number of InterICFGNodeKind has changed, make sure "
                      "the range is correct");
        return n <= FunRetBlock && n >= FunEntryBlock;
    }

    static inline bool isSVFVarKind(GNodeK n)
    {
        static_assert(DummyObjNode - ValNode == 26,
                      "The number of SVFVarKinds has changed, make sure the "
                      "range is correct");

        return n <= DummyObjNode && n >= ValNode;
    }

    static inline bool isValVarKinds(GNodeK n)
    {
        static_assert(DummyValNode - ValNode == 13,
                      "The number of ValVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyValNode && n >= ValNode;
    }


    static inline bool isConstantDataValVar(GNodeK n)
    {
        static_assert(ConstNullptrValNode - ConstDataValNode == 4,
                      "The number of ConstantDataValVarKinds has changed, make "
                      "sure the range is correct");
        return n <= ConstNullptrValNode && n >= ConstDataValNode;
    }

    static inline bool isObjVarKinds(GNodeK n)
    {
        static_assert(DummyObjNode - ObjNode == 12,
                      "The number of ObjVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyObjNode && n >= ObjNode;
    }

    static inline bool isBaseObjVarKinds(GNodeK n)
    {
        static_assert(DummyObjNode - BaseObjNode == 10,
                      "The number of BaseObjVarKinds has changed, make sure the "
                      "range is correct");
        return n <= DummyObjNode && n >= BaseObjNode;
    }

    static inline bool isConstantDataObjVarKinds(GNodeK n)
    {
        static_assert(ConstNullptrObjNode - ConstDataObjNode == 3,
                      "The number of ConstantDataObjVarKinds has changed, make "
                      "sure the range is correct");
        return n <= ConstNullptrObjNode && n >= ConstDataObjNode;
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


template <typename F, typename S>
OutStream& operator<< (OutStream &o, const std::pair<F, S> &var)
{
    o << "<" << var.first << ", " << var.second << ">";
    return o;
}

}

#endif /* INCLUDE_SVFIR_SVFVALUE_H_ */
