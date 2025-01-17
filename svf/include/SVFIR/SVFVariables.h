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
#include "SVFIR/SymbolTableInfo.h"
#include "SVFIR/SVFStatements.h"

namespace SVF
{

class SVFVar;
/*
 * SVFIR program variables (PAGNodes)
 */
typedef GenericNode<SVFVar, SVFStmt> GenericPAGNodeTy;
class SVFVar : public GenericPAGNodeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class IRGraph;
    friend class SVFIR;
    friend class VFG;

public:
    /// Nine kinds of SVFIR variables
    /// ValNode: llvm pointer value
    /// ObjNode: memory object
    /// RetNode: unique return node
    /// Vararg: unique node for vararg parameter
    /// GepValNode: temporary gep value node for field sensitivity
    /// GepValNode: temporary gep obj node for field sensitivity
    /// BaseObjNode: for field insensitive analysis
    /// DummyValNode and DummyObjNode: for non-llvm-value node
    typedef GNodeK PNODEK;
    typedef s64_t GEdgeKind;

protected:
    const SVFValue* value; ///< value of this SVFIR node
    SVFStmt::KindToSVFStmtMapTy InEdgeKindToSetMap;
    SVFStmt::KindToSVFStmtMapTy OutEdgeKindToSetMap;
    bool isPtr;	/// whether it is a pointer (top-level or address-taken)
    const SVFFunction* func; /// function containing this variable

    /// Constructor to create an empty object (for deserialization)
    SVFVar(NodeID i, PNODEK k) : GenericPAGNodeTy(i, k), value{} {}

public:
    /// Constructor
    SVFVar(const SVFValue* val, NodeID i, PNODEK k);
    /// Destructor
    virtual ~SVFVar() {}

    ///  Get/has methods of the components
    //@{
    inline const SVFValue* getValue() const
    {
        assert(this->getNodeKind() != DummyValNode &&
               this->getNodeKind() != DummyObjNode &&
               "dummy node do not have value!");
        assert(!SymbolTableInfo::isBlkObjOrConstantObj(this->getId()) &&
               "blackhole and constant obj do not have value");
        assert(value && "value is null (GepObjNode whose basenode is a DummyObj?)");
        return value;
    }

    /// Return type of the value
    inline virtual const SVFType* getType() const
    {
        return value ? value->getType() : nullptr;
    }

    inline bool hasValue() const
    {
        return value != nullptr;
    }
    /// Whether it is a pointer
    virtual inline bool isPointer() const
    {
        return isPtr;
    }
    /// Whether it is constant data, i.e., "0", "1.001", "str"
    /// or llvm's metadata, i.e., metadata !4087
    bool isConstDataOrAggDataButNotNullPtr() const;

    /// Whether this is an isolated node on the SVFIR graph
    virtual bool isIsolatedNode() const;

    /// Get name of the LLVM value
    // TODO: (Optimization) Should it return const reference instead of value?
    virtual const std::string getValueName() const = 0;

    /// Return the function containing this SVFVar
    /// @return The SVFFunction containing this variable, or nullptr if it's a global/constant expression
    virtual inline const SVFFunction* getFunction() const
    {
        // Return cached function if available
        if(func) return func;

        // If we have an associated LLVM value, check its parent function
        if (value)
        {
            // For instructions, return the function containing the parent basic block
            if (auto inst = SVFUtil::dyn_cast<SVFInstruction>(value))
            {
                return inst->getParent()->getParent();
            }
        }

        // Return nullptr for globals/constants with no parent function
        return nullptr;
    }

    /// Get incoming SVFIR statements (edges)
    inline SVFStmt::SVFStmtSetTy& getIncomingEdges(SVFStmt::PEDGEK kind)
    {
        return InEdgeKindToSetMap[kind];
    }
    /// Get outgoing SVFIR statements (edges)
    inline SVFStmt::SVFStmtSetTy& getOutgoingEdges(SVFStmt::PEDGEK kind)
    {
        return OutEdgeKindToSetMap[kind];
    }
    /// Has incoming SVFIR statements (edges)
    inline bool hasIncomingEdges(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        if (it != InEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }
    /// Has outgoing SVFIR statements (edges)
    inline bool hasOutgoingEdges(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        if (it != OutEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Get incoming SVFStmt iterator
    inline SVFStmt::SVFStmtSetTy::iterator getIncomingEdgesBegin(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get incoming SVFStmt iterator
    inline SVFStmt::SVFStmtSetTy::iterator getIncomingEdgesEnd(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }

    /// Get outgoing SVFStmt iterator
    inline SVFStmt::SVFStmtSetTy::iterator getOutgoingEdgesBegin(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get outgoing SVFStmt iterator
    inline SVFStmt::SVFStmtSetTy::iterator getOutgoingEdgesEnd(SVFStmt::PEDGEK kind) const
    {
        SVFStmt::KindToSVFStmtMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }
    //@}

    static inline bool classof(const SVFVar *)
    {
        return true;
    }

    static inline bool classof(const GenericPAGNodeTy * node)
    {
        return isSVFVarKind(node->getNodeKind());
    }

    static inline bool classof(const SVFBaseNode* node)
    {
        return isSVFVarKind(node->getNodeKind());
    }

private:
    ///  add methods of the components
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
    /// Has incoming VariantGepEdges
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
    virtual const std::string toString() const;

    /// Dump to console for debugging
    void dump() const;

    //@}
    /// Overloading operator << for dumping SVFVar value
    //@{
    friend OutStream& operator<< (OutStream &o, const SVFVar &node)
    {
        o << node.toString();
        return o;
    }
    //@}
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
    static inline bool classof(const SVFBaseNode* node)
    {
        return isValVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    ValVar(const SVFValue* val, NodeID i, PNODEK ty = ValNode, const ICFGNode* node = nullptr)
        : SVFVar(val, i, ty), icfgNode(node)
    {
    }
    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value)
            return value->getName();
        return "";
    }

    const ICFGNode* getICFGNode() const
    {
        return icfgNode;
    }

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
    ObjVar(const SVFValue* val, NodeID i, PNODEK ty = ObjNode) :
        SVFVar(val, i, ty)
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
    static inline bool classof(const SVFBaseNode* node)
    {
        return isObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Return name of a LLVM value
    virtual const std::string getValueName() const
    {
        if (value)
            return value->getName();
        return "";
    }
    /// Return type of the value
    inline virtual const SVFType* getType() const
    {
        return value->getType();
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
    const CallGraphNode* cgNode;
    u32_t argNo;
    bool uncalled;

protected:
    /// Constructor to create function argument (for SVFIRReader/deserialization)
    ArgValVar(NodeID i, PNODEK ty = ArgNode) : ValVar(i, ty) {}

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ArgValVar*)
    {
        return true;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ArgNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ArgNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ArgNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == ArgNode;
    }
    //@}

    /// Constructor
    ArgValVar(NodeID i, u32_t argNo, const ICFGNode* icn, const CallGraphNode* callGraphNode,
              bool isUncalled = false, PNODEK ty = ArgNode);

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value)
            return value->getName() + " (argument valvar)";
        return " (argument valvar)";
    }

    virtual const SVFFunction* getFunction() const;

    const SVFFunction* getParent() const;

    ///  Return the index of this formal argument in its containing function.
    /// For example in "void foo(int a, float b)" a is 0 and b is 1.
    inline u32_t getArgNo() const
    {
        return argNo;
    }

    inline bool isArgOfUncalledFunction() const
    {
        return uncalled;
    }

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
    NodeID base;	// base node id
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
    //@}

    /// Constructor
    GepValVar(NodeID baseID, const SVFValue* val, NodeID i, const AccessPath& ap,
              const SVFType* ty)
        : ValVar(val, i, GepValNode), ap(ap), base(baseID), gepValType(ty)
    {
    }

    /// offset of the base value variable
    inline APOffset getConstantFieldIdx() const
    {
        return ap.getConstantStructFldIdx();
    }

    /// Return the base object from which this GEP node came from.
    inline NodeID getBaseNode(void) const
    {
        return base;
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value)
            return value->getName() + "_" +
                   std::to_string(getConstantFieldIdx());
        return "offset_" + std::to_string(getConstantFieldIdx());
    }

    inline const SVFType* getType() const
    {
        return gepValType;
    }

    virtual const std::string toString() const;
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
    BaseObjVar(NodeID i, PNODEK ty = BaseObjNode) : ObjVar(i, ty) {}

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return isBaseObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    BaseObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti, PNODEK ty = BaseObjNode)
        :  ObjVar(val, i, ty), typeInfo(ti)
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
        if (value)
            return value->getName() + " (base object)";
        return " (base object)";
    }

    virtual const std::string toString() const;

    /// Get the memory object id
    inline SymID getId() const
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
    bool isBlackHoleObj() const
    {
        return SymbolTableInfo::isBlkObj(getId());
    }

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
    bool isConstDataOrAggData() const
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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    //@}

    /// Constructor
    GepObjVar(const BaseObjVar* baseObj, NodeID i,
              const APOffset& apOffset, PNODEK ty = GepObjNode)
        : ObjVar(baseObj->hasValue()? baseObj->getValue(): nullptr, i, ty), apOffset(apOffset), base(baseObj)
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
    inline virtual const SVFType* getType() const
    {
        return SymbolTableInfo::SymbolInfo()->getFlatternedElemType(value->getType(), apOffset);
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value)
            return value->getName() + "_" + std::to_string(apOffset);
        return "offset_" + std::to_string(apOffset);
    }

    virtual const std::string toString() const;
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
    HeapObjVar(NodeID i, PNODEK ty = HeapObjNode) : BaseObjVar(i, ty) {}

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == HeapObjNode;
    }
    //@}

    /// Constructor
    HeapObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti,
               const SVFFunction* f, PNODEK ty = HeapObjNode):
        BaseObjVar(val, i, ti, ty)
    {
        isPtr = val->getType()->isPointerTy();
        func = f;
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
    StackObjVar(NodeID i, PNODEK ty = StackObjNode) : BaseObjVar(i, ty) {}

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == StackObjNode;
    }
    //@}

    /// Constructor
    StackObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti,
                const SVFFunction* fun, PNODEK ty = StackObjNode):
        BaseObjVar(val, i, ti, ty)
    {
        isPtr = val->getType()->isPointerTy();
        func = fun;
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return " (stack base object)";
    }

    virtual const std::string toString() const;
};


class CallGraphNode;

class FunValVar : public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    const CallGraphNode* callGraphNode;

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == FunValNode;
    }
    //@}

    inline const CallGraphNode* getCallGraphNode() const
    {
        return callGraphNode;
    }

    /// Constructor
    FunValVar(NodeID i, const ICFGNode* icn, const CallGraphNode* cgn,
              PNODEK ty = FunValNode);

    virtual const std::string toString() const;
};

class FunObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const CallGraphNode* callGraphNode;

private:
    /// Constructor to create empty ObjVar (for SVFIRReader/deserialization)
    FunObjVar(NodeID i, PNODEK ty = FunObjNode) : BaseObjVar(i, ty) {}

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == FunObjNode;
    }
    //@}

    /// Constructor
    FunObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti, const CallGraphNode* cgNode,
              PNODEK ty = FunObjNode);

    inline const CallGraphNode* getCallGraphNode() const
    {
        return callGraphNode;
    }

    virtual const SVFFunction* getFunction() const;

    virtual bool isIsolatedNode() const;

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == GlobalValNode;
    }
    //@}

    /// Constructor
    GlobalValVar(const SVFValue* val, NodeID i, const ICFGNode* icn,
                 PNODEK ty = GlobalValNode)
        : ValVar(val, i, ty, icn)
    {
    }

    virtual const std::string toString() const;
};

class ConstantDataValVar: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstantDataValVar*)
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
    static inline bool classof(const SVFBaseNode* node)
    {
        return isConstantDataValVar(node->getNodeKind());
    }
    //@}

    /// Constructor
    ConstantDataValVar(const SVFValue* val, NodeID i, const ICFGNode* icn,
                       PNODEK ty = ConstantDataValNode)
        : ValVar(val, i,  ty,  icn)
    {

    }

    virtual const std::string toString() const;
};

class BlackHoleVar: public ConstantDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const BlackHoleVar*)
    {
        return true;
    }
    static inline bool classof(const ConstantDataValVar* node)
    {
        return node->getNodeKind() == BlackHoleNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == BlackHoleNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == BlackHoleNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == BlackHoleNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == BlackHoleNode;
    }
    //@}

    /// Constructor
    BlackHoleVar(NodeID i, PNODEK ty = BlackHoleNode)
        : ConstantDataValVar(nullptr, i,  nullptr, ty)
    {

    }

    virtual const std::string toString() const
    {
        return "BlackHoleVar";
    }
};

class ConstantFPValVar: public ConstantDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    double dval;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstantFPValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstantDataValVar* node)
    {
        return node->getNodeKind() == ConstantFPValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstantFPValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstantFPValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstantFPValNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == ConstantFPValNode;
    }
    //@}

    inline double getFPValue() const
    {
        return dval;
    }

    /// Constructor
    ConstantFPValVar(const SVFValue* val, NodeID i, double dv, const ICFGNode* icn,
                     PNODEK ty = ConstantFPValNode)
        : ConstantDataValVar(val, i,  icn, ty), dval(dv)
    {

    }

    virtual const std::string toString() const;
};

class ConstantIntValVar: public ConstantDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    u64_t zval;
    s64_t sval;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstantIntValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstantDataValVar* node)
    {
        return node->getNodeKind() == ConstantIntValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstantIntValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstantIntValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstantIntValNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == ConstantIntValNode;
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
    ConstantIntValVar(const SVFValue* val, NodeID i, s64_t sv, u64_t zv, const ICFGNode* icn,
                      PNODEK ty = ConstantIntValNode)
        : ConstantDataValVar(val, i,  icn, ty), zval(zv), sval(sv)
    {

    }
    virtual const std::string toString() const;
};

class ConstantNullPtrValVar: public ConstantDataValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ConstantNullPtrValVar*)
    {
        return true;
    }
    static inline bool classof(const ConstantDataValVar* node)
    {
        return node->getNodeKind() == ConstantNullptrValNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == ConstantNullptrValNode;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == ConstantNullptrValNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == ConstantNullptrValNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == ConstantNullptrValNode;
    }
    //@}

    /// Constructor
    ConstantNullPtrValVar(const SVFValue* val, NodeID i, const ICFGNode* icn,
                          PNODEK ty = ConstantNullptrValNode)
        : ConstantDataValVar(val, i,  icn, ty)
    {

    }

    virtual const std::string toString() const;
};

class GlobalObjVar : public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty ObjVar (for SVFIRReader/deserialization)
    GlobalObjVar(NodeID i, PNODEK ty = GlobalObjNode) : BaseObjVar(i, ty) {}

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == GlobalObjNode;
    }
    //@}

    /// Constructor
    GlobalObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti,
                 PNODEK ty = GlobalObjNode): BaseObjVar(val, i, ti, ty)
    {

    }


    virtual const std::string toString() const;
};

class ConstantDataObjVar: public BaseObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

protected:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstantDataObjVar(NodeID i) : BaseObjVar(i, ConstantDataObjNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstantDataObjVar*)
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

    static inline bool classof(const SVFBaseNode* node)
    {
        return isConstantDataObjVarKinds(node->getNodeKind());
    }
    //@}

    /// Constructor
    ConstantDataObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti,  PNODEK ty = ConstantDataObjNode)
        : BaseObjVar(val, i, ti, ty)
    {
    }

    virtual const std::string toString() const;
};

class ConstantFPObjVar: public ConstantDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstantFPObjVar(NodeID i) : ConstantDataObjVar(i) {}

private:
    float dval;

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstantFPObjVar*)
    {
        return true;
    }
    static inline bool classof(const ConstantDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }
    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }

    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }

    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }

    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::ConstantFPObjNode;
    }
    //@}

    /// Constructor
    ConstantFPObjVar(const SVFValue* val, NodeID i, double dv, ObjTypeInfo* ti, PNODEK ty = ConstantFPObjNode)
        : ConstantDataObjVar(val, i, ti, ty), dval(dv)
    {
    }

    inline double getFPValue() const
    {
        return dval;
    }


    virtual const std::string toString() const;
};

class ConstantIntObjVar: public ConstantDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstantIntObjVar(NodeID i) : ConstantDataObjVar(i) {}

private:
    u64_t zval;
    s64_t sval;

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstantIntObjVar*)
    {
        return true;
    }

    static inline bool classof(const ConstantDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
    }

    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
    }

    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::ConstantIntObjNode;
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
    ConstantIntObjVar(const SVFValue* val, NodeID i, s64_t sv, u64_t zv, ObjTypeInfo* ti, PNODEK ty = ConstantIntObjNode)
        : ConstantDataObjVar(val, i, ti, ty), zval(zv), sval(sv)
    {
    }


    virtual const std::string toString() const;
};

class ConstantNullPtrObjVar: public ConstantDataObjVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Constructor to create empty DummyObjVar (for SVFIRReader/deserialization)
    ConstantNullPtrObjVar(NodeID i) : ConstantDataObjVar(i) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ConstantNullPtrObjVar*)
    {
        return true;
    }

    static inline bool classof(const ConstantDataObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }

    static inline bool classof(const BaseObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }

    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }
    static inline bool classof(const ObjVar* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }

    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::ConstantNullptrObjNode;
    }
    //@}

    /// Constructor
    ConstantNullPtrObjVar(const SVFValue* val, NodeID i, ObjTypeInfo* ti, PNODEK ty = ConstantNullptrObjNode)
        : ConstantDataObjVar(val, i, ti, ty)
    {
    }


    virtual const std::string toString() const;
};
/*
 * Unique Return node of a procedure
 */
class RetPN: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const CallGraphNode* callGraphNode;
private:
    /// Constructor to create empty RetPN (for SVFIRReader/deserialization)
    RetPN(NodeID i) : ValVar(i, RetNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetPN*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    //@}


    /// Constructor
    RetPN(NodeID i, const CallGraphNode* node);

    inline const CallGraphNode* getCallGraphNode() const
    {
        return callGraphNode;
    }

    virtual const SVFFunction* getFunction() const;

    /// Return name of a LLVM value
    const std::string getValueName() const;

    virtual const std::string toString() const;
};

/*
 * Unique vararg node of a procedure
 */
class VarArgPN: public ValVar
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    const CallGraphNode* callGraphNode;

private:
    /// Constructor to create empty VarArgPN (for SVFIRReader/deserialization)
    VarArgPN(NodeID i) : ValVar(i, VarargNode) {}

public:
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const VarArgPN*)
    {
        return true;
    }
    static inline bool classof(const SVFVar* node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    static inline bool classof(const ValVar* node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    static inline bool classof(const GenericPAGNodeTy* node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    //@}

    /// Constructor
    VarArgPN(NodeID i, const CallGraphNode* node) : ValVar(nullptr, i, VarargNode), callGraphNode(node) {}

    virtual const SVFFunction* getFunction() const;

    /// Return name of a LLVM value
    const std::string getValueName() const;

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
    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    //@}

    /// Constructor
    DummyValVar(NodeID i) : ValVar(nullptr, i, DummyValNode) {}

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "dummyVal";
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
    DummyObjVar(NodeID i) : BaseObjVar(i, DummyObjNode) {}

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

    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    //@}

    /// Constructor
    DummyObjVar(NodeID i, ObjTypeInfo* ti, PNODEK ty = DummyObjNode)
        : BaseObjVar(nullptr, i, ti, ty)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "dummyObj";
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* INCLUDE_SVFIR_SVFVARIABLE_H_ */
