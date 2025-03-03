//===- SVFSymbols.h -- SVF Variables------------------------//
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
 * SVFVariables.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SVFVARIABLE_H_
#define INCLUDE_SVFIR_SVFVARIABLE_H_

#include "Graphs/GenericGraph.h"
#include "SVFIR/ObjTypeInfo.h"
#include "SVFIR/SVFStatements.h"

namespace SVF
{

class SVFVar;
/*
 * Program variables in SVFIR (based on PAG nodes)
 * These represent variables in the program analysis graph
 */
typedef GenericNode<SVFVar, SVFStmt> GenericPAGNodeTy;
class SVFVar : public GenericPAGNodeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
    friend class IRGraph;
    friend class SVFIR;
    friend class VFG;

public:
    /// Node kinds for SVFIR variables:
    /// ValNode - LLVM pointer value
    /// ObjNode - Memory object
    /// RetValNode - Function return value
    /// VarargNode - Variable argument parameter
    /// GepValNode - Temporary value for field-sensitive analysis
    /// GepObjNode - Temporary object for field-sensitive analysis
    /// BaseObjNode - Base object for field-insensitive analysis
    /// DummyValNode/DummyObjNode - Nodes for non-LLVM values
    typedef GNodeK PNODEK;
    typedef s64_t GEdgeKind;

protected:
    /// Maps tracking incoming and outgoing edges by kind
    SVFStmt::KindToSVFStmtMapTy InEdgeKindToSetMap;
    SVFStmt::KindToSVFStmtMapTy OutEdgeKindToSetMap;

    /// Empty constructor for deserialization
    SVFVar(NodeID i, PNODEK k) : GenericPAGNodeTy(i, k) {}


public:
    /// Standard constructor with ID, type and kind
    SVFVar(NodeID i, const SVFType* svfType, PNODEK k);

    /// Virtual destructor
    virtual ~SVFVar() {}

    /// Check if this variable represents a pointer
    virtual inline bool isPointer() const
    {
        assert(type && "type is null?");
        return type->isPointerTy();
    }

    /// Check if this variable represents constant data/metadata but not null pointer
    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return false;
    }

    /// Check if this node is isolated (no edges) in the SVFIR graph
    virtual bool isIsolatedNode() const;

    /// Get string name of the represented LLVM value
    virtual const std::string getValueName() const = 0;

    /// Get containing function, or null for globals/constants
    virtual inline const FunObjVar* getFunction() const
    {
        return nullptr;
    }

    /// Edge accessors and checkers
    //@{
    inline SVFStmt::SVFStmtSetTy& getIncomingEdges(SVFStmt::PEDGEK kind)
    {
        return InEdgeKindToSetMap[kind];
    }

    inline SVFStmt::SVFStmtSetTy& getOutgoingEdges(SVFStmt::PEDGEK kind)
    {
        return OutEdgeKindToSetMap[kind];
    }

    inline bool hasIncomingEdges(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        if (it != InEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    inline bool hasOutgoingEdges(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        if (it != OutEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Edge iterators
    inline SVFStmt::SVFStmtSetTy::iterator getIncomingEdgesBegin(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "Edge kind not found");
        return it->second.begin();
    }

    inline SVFStmt::SVFStmtSetTy::iterator getIncomingEdgesEnd(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "Edge kind not found");
        return it->second.end();
    }

    inline SVFStmt::SVFStmtSetTy::iterator getOutgoingEdgesBegin(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "Edge kind not found");
        return it->second.begin();
    }

    inline SVFStmt::SVFStmtSetTy::iterator getOutgoingEdgesEnd(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "Edge kind not found");
        return it->second.end();
    }
    //@}

    /// Type checking support for LLVM-style RTTI
    static inline bool classof(const SVFVar *)
    {
        return true;
    }

    static inline bool classof(const GenericPAGNodeTy * node)
    {
        return isSVFVarKind(node->getNodeKind());
    }

    static inline bool classof(const SVFValue* node)
    {
        return isSVFVarKind(node->getNodeKind());
    }

    /// Check if this pointer is in an uncalled function
    virtual bool ptrInUncalledFunction() const;

    /// Check if this variable represents constant/aggregate data
    virtual bool isConstDataOrAggData() const
    {
        return false;
    }


private:
    /// Edge management methods
    //@{
    inline void addInEdge(SVFStmt* inEdge)
    {
        GEdgeKind kind = inEdge->getEdgeKind();
        InEdgeKindToSetMap[kind].insert(inEdge);
        addIncomingEdge(inEdge);
    }

    inline void addOutEdge(SVFStmt* outEdge)
    {
        GEdgeKind kind = outEdge->getEdgeKind();
        OutEdgeKindToSetMap[kind].insert(outEdge);
        addOutgoingEdge(outEdge);
    }

    /// Check for incoming variable field GEP edges
    inline bool hasIncomingVariantGepEdge() const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(SVFStmt::Gep);
        if (it != InEdgeKindToSetMap.end())
        {
            for(auto gep : it->second)
            {
                if(SVFUtil::cast<GepStmt>(gep)->isVariantFieldGep())
                    return true;
            }
        }
        return false;
    }

public:
    /// Get string representation
    virtual const std::string toString() const;

    /// Debug dump to console
    void dump() const;

    /// Stream operator overload for output
    friend OutStream& operator<< (OutStream &o, const SVFVar &node)
    {
        o << node.toString();
        return o;
    }
};



/*
 * Value (Pointer) variable
 */
class ValVar: public SVFVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const ICFGNode* icfgNode; // icfgnode related to valvar
protected:
    /// Constructor to create an empty ValVar (for SVFIRReader/deserialization)
    ValVar(NodeID i, PNODEK ty = ValNode) : SVFVar(i, ty), icfgNode(nullptr) {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ValVar*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return isValVarKinds(node->getNodeKind());
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return isValVarKinds(node->getNodeKind());
    }
    static inline bool classof(const SVFValue* node)
    {
        return isValVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    ValVar(NodeID i, const SVFType* svfType, const ICFGNode* node, PNODEK ty = ValNode)
        : SVFVar(i, svfType, ty), icfgNode(node)
    {
    }
    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return getName();
    }

    const ICFGNode* getICFGNode() const
    {
        return icfgNode;
    }

    virtual const FunObjVar* getFunction() const;

    virtual const std::string toString() const;
};

/*
 * Memory Object variable
 */
class ObjVar: public SVFVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

protected:
    /// Constructor to create an empty ObjVar (for SVFIRReader/deserialization)
    ObjVar(NodeID i, PNODEK ty = ObjNode) : SVFVar(i, ty) {}
    /// Constructor
    ObjVar(NodeID i, const SVFType* svfType, PNODEK ty = ObjNode) :
        SVFVar(i, svfType, ty)
    {
    }
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ObjVar*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return isObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return isObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const SVFValue* node)
    {
        return isObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Return name of a LLVM value
    virtual const std::string getValueName() const
    {
        return getName();
    }

    virtual const std::string toString() const;
};


/**
 * @brief Class representing a function argument variable in the SVFIR
 *
 * This class models function argument in the program analysis. It extends ValVar
 * to specifically handle function argument.
 */
class ArgValVar: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const FunObjVar* cgNode;
    u32_t argNo;

protected:
    /// Constructor to create function argument (for SVFIRReader/deserialization)
    ArgValVar(NodeID i, PNODEK ty = ArgValNode) : ValVar(i, ty) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ArgValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ArgValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ArgValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ArgValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ArgValNode;
    }
    //@}

    /// Constructor
    ArgValVar(NodeID i, u32_t argNo, const ICFGNode* icn, const FunObjVar* callGraphNode,
              const SVFType* svfType);

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return getName() + " (argument valvar)";
    }

    virtual const FunObjVar* getFunction() const;

    const FunObjVar* getParent() const;

    ///  Return the index of this formal argument in its containing function.
    /// For example in "void foo(int a, float b)" a is 0 and b is 1.
    inline u32_t getArgNo() const
    {
        return argNo;
    }

    bool isArgOfUncalledFunction() const;

    virtual bool isPointer() const;

    virtual const std::string toString() const;
};


/*
 * Gep Value (Pointer) variable, this variable can be dynamic generated for field sensitive analysis
 * e.g. memcpy, temp gep value variable needs to be created
 * Each Gep Value variable is connected to base value variable via gep edge
 */
class GepValVar: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    AccessPath ap;	// AccessPath
    const ValVar* base;	// base node
    const SVFType* gepValType;

    /// Constructor to create empty GeValVar (for SVFIRReader/deserialization)
    GepValVar(NodeID i) : ValVar(i, GepValNode), gepValType{} {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepValVar *)
    {
        return true;
    }
    static inline bool classof(const ValVar * node)
    {
        return node->getNodeKind() == SVFVar::GepValNode;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::GepValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::GepValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::GepValNode;
    }
    //@}

    /// Constructor
    GepValVar(const ValVar* baseNode, NodeID i, const AccessPath& ap,
              const SVFType* ty, const ICFGNode* node);

    /// offset of the base value variable
    inline APOffset getConstantFieldIdx() const
    {
        return ap.getConstantStructFldIdx();
    }

    /// Return the base object from which this GEP node came from.
    inline const ValVar* getBaseNode(void) const
    {
        return base;
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return getName() + "_" +
               std::to_string(getConstantFieldIdx());
    }

    virtual bool isPointer() const
    {
        return base->isPointer();
    }

    inline const SVFType* getType() const
    {
        return gepValType;
    }

    virtual const FunObjVar* getFunction() const
    {
        return base->getFunction();
    }

    virtual const std::string toString() const;

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return base->isConstDataOrAggDataButNotNullPtr();
    }
    virtual inline bool ptrInUncalledFunction() const
    {
        return base->ptrInUncalledFunction();
    }

    virtual inline bool isConstDataOrAggData() const
    {
        return base->isConstDataOrAggData();
    }
};

/*
 * Base memory object variable (address-taken variables in LLVM-based languages)
 */
class BaseObjVar : public ObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
private:
    ObjTypeInfo* typeInfo;

    const ICFGNode* icfgNode; /// ICFGNode related to the creation of this object

protected:
    /// Constructor to create empty ObjVar (for SVFIRReader/deserialization)
    BaseObjVar(NodeID i, const ICFGNode* node, PNODEK ty = BaseObjNode) : ObjVar(i, ty), icfgNode(node) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BaseObjVar*)
    {
        return true;
    }
    static inline bool classof(const ObjVar* node)
    {
        return isBaseObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const SVFVar* node)
    {
        return isBaseObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return isBaseObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const SVFValue* node)
    {
        return isBaseObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    BaseObjVar(NodeID i, ObjTypeInfo* ti,
               const SVFType* svfType, const ICFGNode* node, PNODEK ty = BaseObjNode)
        : ObjVar(i, svfType, ty), typeInfo(ti), icfgNode(node)
    {
    }

    virtual const BaseObjVar* getBaseMemObj() const
    {
        return this;
    }

    /// Get the ICFGNode related to the creation of this object
    inline const ICFGNode* getICFGNode() const
    {
        return icfgNode;
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return getName() + " (base object)";
    }

    virtual const std::string toString() const;

    /// Get the memory object id
    inline NodeID getId() const
    {
        return id;
    }

    /// Get obj type
    const SVFType* getType() const
    {
        return typeInfo->getType();
    }

    /// Get the number of elements of this object
    u32_t getNumOfElements() const
    {
        return typeInfo->getNumOfElements();
    }

    /// Set the number of elements of this object
    void setNumOfElements(u32_t num)
    {
        return typeInfo->setNumOfElements(num);
    }

    /// Get max field offset limit
    u32_t getMaxFieldOffsetLimit() const
    {
        return typeInfo->getMaxFieldOffsetLimit();
    }


    /// Return true if its field limit is 0
    bool isFieldInsensitive() const
    {
        return getMaxFieldOffsetLimit() == 0;
    }

    /// Set the memory object to be field insensitive
    void setFieldInsensitive()
    {
        typeInfo->setMaxFieldOffsetLimit(0);
    }


    /// Set the memory object to be field sensitive (up to max field limit)
    void setFieldSensitive()
    {
        typeInfo->setMaxFieldOffsetLimit(typeInfo->getNumOfElements());
    }

    /// Whether it is a black hole object
    bool isBlackHoleObj() const;

    /// Get the byte size of this object
    u32_t getByteSizeOfObj() const
    {
        return typeInfo->getByteSizeOfObj();
    }

    /// Check if byte size is a const value
    bool isConstantByteSize() const
    {
        return typeInfo->isConstantByteSize();
    }


    /// object attributes methods
    //@{
    bool isFunction() const
    {
        return typeInfo->isFunction();
    }
    bool isGlobalObj() const
    {
        return typeInfo->isGlobalObj();
    }
    bool isStaticObj() const
    {
        return typeInfo->isStaticObj();
    }
    bool isStack() const
    {
        return typeInfo->isStack();
    }
    bool isHeap() const
    {
        return typeInfo->isHeap();
    }
    bool isStruct() const
    {
        return typeInfo->isStruct();
    }
    bool isArray() const
    {
        return typeInfo->isArray();
    }
    bool isVarStruct() const
    {
        return typeInfo->isVarStruct();
    }
    bool isVarArray() const
    {
        return typeInfo->isVarArray();
    }
    bool isConstantStruct() const
    {
        return typeInfo->isConstantStruct();
    }
    bool isConstantArray() const
    {
        return typeInfo->isConstantArray();
    }
    bool isConstDataOrConstGlobal() const
    {
        return typeInfo->isConstDataOrConstGlobal();
    }
    virtual inline bool isConstDataOrAggData() const
    {
        return typeInfo->isConstDataOrAggData();
    }
    //@}

    /// Clean up memory
    void destroy()
    {
        delete typeInfo;
        typeInfo = nullptr;
    }

    virtual const FunObjVar* getFunction() const;

};


/*
 * Gep Obj variable, this is dynamic generated for field sensitive analysis
 * Each gep obj variable is one field of a BaseObjVar (base)
 */
class GepObjVar: public ObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    APOffset apOffset = 0;

    const BaseObjVar* base;

    /// Constructor to create empty GepObjVar (for SVFIRReader/deserialization)
    //  only for reading from file when we don't have BaseObjVar*
    GepObjVar(NodeID i, PNODEK ty = GepObjNode) : ObjVar(i, ty), base{} {}

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepObjVar*)
    {
        return true;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    //@}

    /// Constructor
    GepObjVar(const BaseObjVar* baseObj, NodeID i,
              const APOffset& apOffset, PNODEK ty = GepObjNode)
        : ObjVar(i, baseObj->getType(), ty), apOffset(apOffset), base(baseObj)
    {
    }

    /// offset of the mem object
    inline APOffset getConstantFieldIdx() const
    {
        return apOffset;
    }

    /// Return the base object from which this GEP node came from.
    inline NodeID getBaseNode(void) const
    {
        return base->getId();
    }

    inline const BaseObjVar* getBaseObj() const
    {
        return base;
    }

    /// Return the type of this gep object
    inline virtual const SVFType* getType() const;


    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return getName() + "_" + std::to_string(apOffset);
    }

    virtual const FunObjVar* getFunction() const
    {
        return base->getFunction();
    }

    virtual const std::string toString() const;

    virtual inline bool ptrInUncalledFunction() const
    {
        return base->ptrInUncalledFunction();
    }

    virtual inline bool isConstDataOrAggData() const
    {
        return base->isConstDataOrAggData();
    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return base->isConstDataOrAggDataButNotNullPtr();
    }

    virtual bool isPointer() const
    {
        return base->isPointer();
    }
};



/**
 * @brief Class representing a heap object variable in the SVFIR
 *
 * This class models heap-allocated objects in the program analysis. It extends BaseObjVar
 * to specifically handle heap memory locations.
 */
class HeapObjVar: public BaseObjVar
{

    friend class SVFIRWriter;
    friend class SVFIRReader;

protected:
    /// Constructor to create heap object var
    HeapObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i, node, HeapObjNode) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const HeapObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    //@}

    /// Constructor
    HeapObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node):
        BaseObjVar(i, ti, svfType, node, HeapObjNode)
    {
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return " (heap base object)";
    }

    virtual const std::string toString() const;
};


/**
 * @brief Represents a stack-allocated object variable in the SVFIR (SVF Intermediate Representation)
 * @inherits BaseObjVar
 *
 * This class models variables that are allocated on the stack in the program.
 * It provides type checking functionality through LLVM-style RTTI (Runtime Type Information)
 * methods like classof.
 */
class StackObjVar: public BaseObjVar
{

    friend class SVFIRWriter;
    friend class SVFIRReader;

protected:
    /// Constructor to create stack object var
    StackObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i, node, StackObjNode) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StackObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    //@}

    /// Constructor
    StackObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node):
        BaseObjVar(i, ti, svfType, node, StackObjNode)
    {
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return " (stack base object)";
    }

    virtual const std::string toString() const;
};


class CallGraphNode;

class FunObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
    friend class LLVMModuleSet;

public:
    typedef SVFLoopAndDomInfo::BBSet BBSet;
    typedef SVFLoopAndDomInfo::BBList BBList;
    typedef SVFLoopAndDomInfo::LoopBBs LoopBBs;

    typedef BasicBlockGraph::IDToNodeMapTy::const_iterator const_bb_iterator;


private:
    bool isDecl;   /// return true if this function does not have a body
    bool intrinsic; /// return true if this function is an intrinsic function (e.g., llvm.dbg), which does not reside in the application code
    bool isAddrTaken; /// return true if this function is address-taken (for indirect call purposes)
    bool isUncalled;    /// return true if this function is never called
    bool isNotRet;   /// return true if this function never returns
    bool supVarArg;    /// return true if this function supports variable arguments
    const SVFFunctionType* funcType; /// FunctionType, which is different from the type (PointerType) of this SVF Function
    SVFLoopAndDomInfo* loopAndDom;  /// the loop and dominate information
    const FunObjVar * realDefFun;  /// the definition of a function across multiple modules
    BasicBlockGraph* bbGraph; /// the basic block graph of this function
    std::vector<const ArgValVar*> allArgs;    /// all formal arguments of this function
    const SVFBasicBlock *exitBlock;             /// a 'single' basic block having no successors and containing return instruction in a function


private:
    /// Constructor to create empty ObjVar (for SVFIRReader/deserialization)
    FunObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i,node, FunObjNode) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    //@}

    /// Constructor
    FunObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node);


    virtual ~FunObjVar()
    {
        delete loopAndDom;
        delete bbGraph;
    }

    void initFunObjVar(bool decl, bool intrinc, bool addr, bool uncalled, bool notret, bool vararg, const SVFFunctionType *ft,
                       SVFLoopAndDomInfo *ld, const FunObjVar *real, BasicBlockGraph *bbg,
                       const std::vector<const ArgValVar *> &allarg, const SVFBasicBlock *exit);

    void setRelDefFun(const FunObjVar *real)
    {
        realDefFun = real;
    }

    virtual const FunObjVar*getFunction() const;

    inline void addArgument(const ArgValVar *arg)
    {
        allArgs.push_back(arg);
    }
    inline bool isDeclaration() const
    {
        return isDecl;
    }

    inline bool isIntrinsic() const
    {
        return intrinsic;
    }

    inline bool hasAddressTaken() const
    {
        return isAddrTaken;
    }

    inline bool isVarArg() const
    {
        return supVarArg;
    }

    inline bool isUncalledFunction() const
    {
        return isUncalled;
    }

    inline bool hasReturn() const
    {
        return  !isNotRet;
    }

    /// Returns the FunctionType
    inline const SVFFunctionType* getFunctionType() const
    {
        return funcType;
    }

    /// Returns the FunctionType
    inline const SVFType* getReturnType() const
    {
        return funcType->getReturnType();
    }

    inline SVFLoopAndDomInfo* getLoopAndDomInfo() const
    {
        return loopAndDom;
    }

    inline const std::vector<const SVFBasicBlock*>& getReachableBBs() const
    {
        return loopAndDom->getReachableBBs();
    }

    inline void getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const
    {
        return loopAndDom->getExitBlocksOfLoop(bb,exitbbs);
    }

    inline bool hasLoopInfo(const SVFBasicBlock* bb) const
    {
        return loopAndDom->hasLoopInfo(bb);
    }

    const LoopBBs& getLoopInfo(const SVFBasicBlock* bb) const
    {
        return loopAndDom->getLoopInfo(bb);
    }

    inline const SVFBasicBlock* getLoopHeader(const BBList& lp) const
    {
        return loopAndDom->getLoopHeader(lp);
    }

    inline bool loopContainsBB(const BBList& lp, const SVFBasicBlock* bb) const
    {
        return loopAndDom->loopContainsBB(lp,bb);
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomTreeMap() const
    {
        return loopAndDom->getDomTreeMap();
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap() const
    {
        return loopAndDom->getDomFrontierMap();
    }

    inline bool isLoopHeader(const SVFBasicBlock* bb) const
    {
        return loopAndDom->isLoopHeader(bb);
    }

    inline bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
    {
        return loopAndDom->dominate(bbKey,bbValue);
    }

    inline bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
    {
        return loopAndDom->postDominate(bbKey,bbValue);
    }

    inline const FunObjVar* getDefFunForMultipleModule() const
    {
        if(realDefFun==nullptr)
            return this;
        return realDefFun;
    }

    void setBasicBlockGraph(BasicBlockGraph* graph)
    {
        this->bbGraph = graph;
    }

    BasicBlockGraph* getBasicBlockGraph()
    {
        return bbGraph;
    }

    const BasicBlockGraph* getBasicBlockGraph() const
    {
        return bbGraph;
    }

    inline bool hasBasicBlock() const
    {
        return bbGraph && bbGraph->begin() != bbGraph->end();
    }

    inline const SVFBasicBlock* getEntryBlock() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        assert(bbGraph->begin()->second->getInEdges().size() == 0 && "the first basic block is not entry block");
        return bbGraph->begin()->second;
    }

    inline const SVFBasicBlock* getExitBB() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        assert(exitBlock && "must have an exitBlock");
        return exitBlock;
    }

    inline void setExitBlock(SVFBasicBlock *bb)
    {
        assert(!exitBlock && "have already set exit Basicblock!");
        exitBlock = bb;
    }


    u32_t inline arg_size() const
    {
        return allArgs.size();
    }

    inline const ArgValVar*  getArg(u32_t idx) const
    {
        assert (idx < allArgs.size() && "getArg() out of range!");
        return allArgs[idx];
    }

    inline const SVFBasicBlock* front() const
    {
        return getEntryBlock();
    }

    inline const SVFBasicBlock* back() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        /// Carefully! 'back' is just the last basic block of function,
        /// but not necessarily a exit basic block
        /// more refer to: https://github.com/SVF-tools/SVF/pull/1262
        return std::prev(bbGraph->end())->second;
    }

    inline const_bb_iterator begin() const
    {
        return bbGraph->begin();
    }

    inline const_bb_iterator end() const
    {
        return bbGraph->end();
    }

    virtual bool isIsolatedNode() const;

    virtual const std::string toString() const;
};
class FunValVar : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    const FunObjVar* funObjVar;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FunValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == FunValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == FunValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == FunValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == FunValNode;
    }
    //@}

    inline virtual const FunObjVar* getFunction() const
    {
        return funObjVar->getFunction();
    }

    /// Constructor
    FunValVar(NodeID i, const ICFGNode* icn, const FunObjVar* cgn, const SVFType* svfType);


    virtual bool isPointer() const
    {
        return true;
    }

    virtual const std::string toString() const;
};



class GlobalValVar : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GlobalValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == GlobalValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == GlobalValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == GlobalValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == GlobalValNode;
    }
    //@}

    /// Constructor
    GlobalValVar(NodeID i, const ICFGNode* icn, const SVFType* svfType)
        : ValVar(i, svfType, icn, GlobalValNode)
    {
        type = svfType;
    }


    virtual const std::string toString() const;
};

class ConstAggValVar: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstAggValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstAggValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstAggValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstAggValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ConstAggValNode;
    }
    //@}

    /// Constructor
    ConstAggValVar(NodeID i, const ICFGNode* icn, const SVFType* svfTy)
        : ValVar(i, svfTy, icn, ConstAggValNode)
    {
        type = svfTy;
    }


    virtual bool isConstDataOrAggData() const
    {
        return true;
    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return true;
    }

    virtual const std::string toString() const;
};


class ConstDataValVar : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstDataValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return isConstantDataValVar(node->getNodeKind());
    }
    static inline bool classof(const SVFVar* node)
    {
        return isConstantDataValVar(node->getNodeKind());
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return isConstantDataValVar(node->getNodeKind());
    }
    static inline bool classof(const SVFValue* node)
    {
        return isConstantDataValVar(node->getNodeKind());
    }
    //@}

    /// Constructor
    ConstDataValVar(NodeID i, const ICFGNode* icn, const SVFType* svfType,
                    PNODEK ty = ConstDataValNode)
        : ValVar(i, svfType, icn, ty)
    {

    }

    virtual bool isConstDataOrAggData() const
    {
        return true;
    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return true;
    }

    virtual const std::string toString() const;
};

class BlackHoleValVar : public ConstDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BlackHoleValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstDataValVar* node)
    {
        return node->getNodeKind() == BlackHoleValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == BlackHoleValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == BlackHoleValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == BlackHoleValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == BlackHoleValNode;
    }
    //@}

    /// Constructor
    BlackHoleValVar(NodeID i, const SVFType* svfType, PNODEK ty = BlackHoleValNode)
        : ConstDataValVar(i,  nullptr, svfType, ty)
    {

    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return false;
    }

    virtual const std::string toString() const
    {
        return "BlackHoleValVar";
    }
};

class ConstFPValVar : public ConstDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    double dval;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstFPValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstDataValVar* node)
    {
        return node->getNodeKind() == ConstFPValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstFPValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstFPValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstFPValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ConstFPValNode;
    }
    //@}

    inline double getFPValue() const
    {
        return dval;
    }

    /// Constructor
    ConstFPValVar(NodeID i, double dv,
                  const ICFGNode* icn, const SVFType* svfType)
        : ConstDataValVar(i, icn, svfType, ConstFPValNode), dval(dv)
    {
    }

    virtual const std::string toString() const;
};

class ConstIntValVar : public ConstDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    u64_t zval;
    s64_t sval;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstIntValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstDataValVar* node)
    {
        return node->getNodeKind() == ConstIntValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstIntValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstIntValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstIntValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ConstIntValNode;
    }
    //@}

    s64_t getSExtValue() const
    {
        return sval;
    }


    u64_t getZExtValue() const
    {
        return zval;
    }

    /// Constructor
    ConstIntValVar(NodeID i, s64_t sv, u64_t zv, const ICFGNode* icn, const SVFType* svfType)
        : ConstDataValVar(i,  icn, svfType, ConstIntValNode), zval(zv), sval(sv)
    {

    }
    virtual const std::string toString() const;
};

class ConstNullPtrValVar : public ConstDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstNullPtrValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstDataValVar* node)
    {
        return node->getNodeKind() == ConstNullptrValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstNullptrValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstNullptrValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstNullptrValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ConstNullptrValNode;
    }
    //@}

    /// Constructor
    ConstNullPtrValVar(NodeID i, const ICFGNode* icn, const SVFType* svfType)
        : ConstDataValVar(i,  icn, svfType, ConstNullptrValNode)
    {

    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return false;
    }

    virtual const std::string toString() const;
};

class GlobalObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty ObjVar (for SVFIRReader/deserialization)
    GlobalObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i, node, GlobalObjNode) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GlobalObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    //@}

    /// Constructor
    GlobalObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node,
                 PNODEK ty = GlobalObjNode): BaseObjVar(i, ti, svfType, node, ty)
    {

    }


    virtual const std::string toString() const;
};

class ConstAggObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstAggObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == ConstAggObjNode;
    }

    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == ConstAggObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstAggObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstAggObjNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == ConstAggObjNode;
    }
    //@}

    /// Constructor
    ConstAggObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node)
        : BaseObjVar(i,  ti, svfType, node, ConstAggObjNode)
    {

    }

    virtual bool isConstDataOrAggData() const
    {
        return true;
    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return true;
    }

    virtual const std::string toString() const;
};

class ConstDataObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

protected:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstDataObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i, node, ConstDataObjNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstDataObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const SVFVar* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const ObjVar* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }

    static inline bool classof(const SVFValue* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    ConstDataObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node, PNODEK ty = ConstDataObjNode)
        : BaseObjVar(i, ti, svfType, node, ty)
    {
    }

    virtual bool isConstDataOrAggData() const
    {
        return true;
    }

    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return true;
    }

    virtual const std::string toString() const;
};

class ConstFPObjVar : public ConstDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstFPObjVar(NodeID i, const ICFGNode* node) : ConstDataObjVar(i, node) {}

private:
    float dval;

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstFPObjVar*)
    {
        return true;
    }
    static inline bool classof(const ConstDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }

    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }

    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::ConstFPObjNode;
    }
    //@}

    /// Constructor
    ConstFPObjVar(NodeID i, double dv, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node)
        : ConstDataObjVar(i, ti, svfType, node, ConstFPObjNode), dval(dv)
    {
    }

    inline double getFPValue() const
    {
        return dval;
    }


    virtual const std::string toString() const;
};

class ConstIntObjVar : public ConstDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstIntObjVar(NodeID i, const ICFGNode* node) : ConstDataObjVar(i, node) {}

private:
    u64_t zval;
    s64_t sval;

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstIntObjVar*)
    {
        return true;
    }

    static inline bool classof(const ConstDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }

    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::ConstIntObjNode;
    }

    s64_t getSExtValue() const
    {
        return sval;
    }


    u64_t getZExtValue() const
    {
        return zval;
    }
    //@}

    /// Constructor
    ConstIntObjVar(NodeID i, s64_t sv, u64_t zv, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node)
        : ConstDataObjVar(i, ti, svfType, node, ConstIntObjNode), zval(zv), sval(sv)
    {
    }


    virtual const std::string toString() const;
};

class ConstNullPtrObjVar : public ConstDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstNullPtrObjVar(NodeID i, const ICFGNode* node) : ConstDataObjVar(i, node) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstNullPtrObjVar*)
    {
        return true;
    }

    static inline bool classof(const ConstDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }

    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::ConstNullptrObjNode;
    }
    //@}

    /// Constructor
    ConstNullPtrObjVar(NodeID i, ObjTypeInfo* ti, const SVFType* svfType, const ICFGNode* node)
        : ConstDataObjVar(i, ti, svfType, node, ConstNullptrObjNode)
    {
    }
    virtual bool isConstDataOrAggDataButNotNullPtr() const
    {
        return false;
    }
    virtual const std::string toString() const;
};
/*
 * Unique Return node of a procedure
 */
class RetValPN : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const FunObjVar* callGraphNode;
private:
    /// Constructor to create empty RetValPN (for SVFIRReader/deserialization)
    RetValPN(NodeID i) : ValVar(i, RetValNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetValPN*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::RetValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == SVFVar::RetValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::RetValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::RetValNode;
    }
    //@}


    /// Constructor
    RetValPN(NodeID i, const FunObjVar* node, const SVFType* svfType, const ICFGNode* icn);

    inline const FunObjVar* getCallGraphNode() const
    {
        return callGraphNode;
    }

    virtual const FunObjVar* getFunction() const;

    virtual bool isPointer() const;

    /// Return name of a LLVM value
    const std::string getValueName() const;

    virtual const std::string toString() const;
};

/*
 * Unique vararg node of a procedure
 */
class VarArgValPN : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    const FunObjVar* callGraphNode;

private:
    /// Constructor to create empty VarArgValPN (for SVFIRReader/deserialization)
    VarArgValPN(NodeID i) : ValVar(i, VarargValNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const VarArgValPN*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::VarargValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == SVFVar::VarargValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::VarargValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::VarargValNode;
    }
    //@}

    /// Constructor
    VarArgValPN(NodeID i, const FunObjVar* node, const SVFType* svfType, const ICFGNode* icn)
        : ValVar(i, svfType, icn, VarargValNode), callGraphNode(node)
    {
    }

    virtual const FunObjVar* getFunction() const;

    /// Return name of a LLVM value
    const std::string getValueName() const;

    virtual bool isPointer() const
    {
        return true;
    }
    virtual const std::string toString() const;
};

/*
 * Dummy variable without any LLVM value
 */
class DummyValVar: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyValVar*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    //@}

    /// Constructor
    DummyValVar(NodeID i, const ICFGNode* node, const SVFType* svfType = SVFType::getSVFPtrType())
        : ValVar(i, svfType, node, DummyValNode)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "dummyVal";
    }

    virtual bool isPointer() const
    {
        return true;
    }

    virtual const std::string toString() const;
};

/*
 * Dummy object variable
 */
class DummyObjVar: public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    DummyObjVar(NodeID i, const ICFGNode* node) : BaseObjVar(i, node, DummyObjNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyObjVar*)
    {
        return true;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }

    static inline bool classof(const SVFValue* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    //@}

    /// Constructor
    DummyObjVar(NodeID i, ObjTypeInfo* ti, const ICFGNode* node, const SVFType* svfType = SVFType::getSVFPtrType())
        : BaseObjVar(i, ti, svfType, node, DummyObjNode)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "dummyObj";
    }

    virtual bool isPointer() const
    {
        return true;
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* INCLUDE_SVFIR_SVFVARIABLE_H_ */
