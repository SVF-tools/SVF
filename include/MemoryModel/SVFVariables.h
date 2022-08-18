//===- SVFSymbols.h -- SVF Variables------------------------//
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
 * SVFVariables.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef OBJECTANDSYMBOL_H_
#define OBJECTANDSYMBOL_H_

#include "Graphs/GenericGraph.h"
#include "MemoryModel/SymbolTableInfo.h"
#include "MemoryModel/SVFStatements.h"

namespace SVF
{

class SVFVar;
/*
 * SVFIR program variables (PAGNodes)
 */
typedef GenericNode<SVFVar,SVFStmt> GenericPAGNodeTy;
class SVFVar : public GenericPAGNodeTy
{
    friend class IRGraph;
    friend class SVFIR;
    friend class VFG;

public:
    /// Nine kinds of SVFIR variables
    /// ValNode: llvm pointer value
    /// ObjNode: memory object
    /// RetNode: unique return node
    /// Vararg: unique node for vararg parameter
    /// GepValNode: tempory gep value node for field sensitivity
    /// GepValNode: tempory gep obj node for field sensitivity
    /// FIObjNode: for field insensitive analysis
    /// DummyValNode and DummyObjNode: for non-llvm-value node
    enum PNODEK
    {
        ValNode,
        ObjNode,
        RetNode,
        VarargNode,
        GepValNode,
        GepObjNode,
        FIObjNode,
        DummyValNode,
        DummyObjNode,
    };


protected:
    const Value* value; ///< value of this SVFIR node
    SVFStmt::KindToSVFStmtMapTy InEdgeKindToSetMap;
    SVFStmt::KindToSVFStmtMapTy OutEdgeKindToSetMap;
    bool isPtr;	/// whether it is a pointer (top-level or address-taken)

public:
    /// Constructor
    SVFVar(const Value* val, NodeID i, PNODEK k);
    /// Destructor
    virtual ~SVFVar()
    {
    }

    ///  Get/has methods of the components
    //@{
    inline const Value* getValue() const
    {
        assert((this->getNodeKind() != DummyValNode && this->getNodeKind() != DummyObjNode) && "dummy node do not have value!");
        assert((SymbolTableInfo::isBlkObjOrConstantObj(this->getId())==false) && "blackhole and constant obj do not have value");
        assert(value && "value is null (GepObjNode whose basenode is a DummyObj?)");
        return value;
    }

    /// Return type of the value
    inline virtual const Type* getType() const
    {
        if (value)
            return value->getType();
        return nullptr;
    }

    inline bool hasValue() const
    {
        return value!=nullptr;
    }
    /// Whether it is a pointer
    virtual inline bool isPointer() const
    {
        return isPtr;
    }
    /// Whether it is constant data, i.e., "0", "1.001", "str"
    /// or llvm's metadata, i.e., metadata !4087
    bool isConstantData() const;

    /// Whether this is an isoloated node on the SVFIR graph
    bool isIsolatedNode() const;

    /// Get name of the LLVM value
    virtual const std::string getValueName() const = 0;

    /// Return the function that this SVFVar resides in. Return nullptr if it is a global or constantexpr node
    virtual inline const Function* getFunction() const
    {
        if(value)
        {
            if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(value))
                return inst->getParent()->getParent();
            else if (const Argument* arg = SVFUtil::dyn_cast<Argument>(value))
                return arg->getParent();
            else if (const Function* fun = SVFUtil::dyn_cast<Function>(value))
                return fun;
        }
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

private:
    ///  add methods of the components
    //@{
    inline void addInEdge(SVFStmt* inEdge)
    {
        GNodeK kind = inEdge->getEdgeKind();
        InEdgeKindToSetMap[kind].insert(inEdge);
        addIncomingEdge(inEdge);
    }

    inline void addOutEdge(SVFStmt* outEdge)
    {
        GNodeK kind = outEdge->getEdgeKind();
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

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ValVar *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::ValNode ||
               node->getNodeKind() == SVFVar::GepValNode ||
               node->getNodeKind() == SVFVar::RetNode ||
               node->getNodeKind() == SVFVar::VarargNode ||
               node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::ValNode ||
               node->getNodeKind() == SVFVar::GepValNode ||
               node->getNodeKind() == SVFVar::RetNode ||
               node->getNodeKind() == SVFVar::VarargNode ||
               node->getNodeKind() == SVFVar::DummyValNode;
    }
    //@}

    /// Constructor
    ValVar(const Value* val, NodeID i, PNODEK ty = ValNode) :
        SVFVar(val, i, ty)
    {
    }
    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str();
        return "";
    }

    virtual const std::string toString() const;
};


/*
 * Memory Object variable
 */
class ObjVar: public SVFVar
{

protected:
    const MemObj* mem;	///< memory object
    /// Constructor
    ObjVar(const Value* val, NodeID i, const MemObj* m, PNODEK ty = ObjNode) :
        SVFVar(val, i, ty), mem(m)
    {
    }
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ObjVar *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::ObjNode ||
               node->getNodeKind() == SVFVar::GepObjNode ||
               node->getNodeKind() == SVFVar::FIObjNode ||
               node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::ObjNode ||
               node->getNodeKind() == SVFVar::GepObjNode ||
               node->getNodeKind() == SVFVar::FIObjNode ||
               node->getNodeKind() == SVFVar::DummyObjNode;
    }
    //@}

    /// Return memory object
    const MemObj* getMemObj() const
    {
        return mem;
    }

    /// Return name of a LLVM value
    virtual const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str();
        return "";
    }
    /// Return type of the value
    inline virtual const llvm::Type* getType() const
    {
        return mem->getType();
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

private:
    LocationSet ls;	// LocationSet
    const Type *gepValType;

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
    GepValVar(const Value* val, NodeID i, const LocationSet& l, const Type *ty) :
        ValVar(val, i, GepValNode), ls(l), gepValType(ty)
    {
    }

    /// offset of the base value variable
    inline s32_t getConstantFieldIdx() const
    {
        return ls.accumulateConstantFieldIdx();
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str() + "_" + std::to_string(getConstantFieldIdx());
        return "offset_" + std::to_string(getConstantFieldIdx());
    }

    inline const Type* getType() const
    {
        return gepValType;
    }

    virtual const std::string toString() const;
};


/*
 * Gep Obj variable, this is dynamic generated for field sensitive analysis
 * Each gep obj variable is one field of a MemObj (base)
 */
class GepObjVar: public ObjVar
{
private:
    LocationSet ls;
    NodeID base = 0;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepObjVar *)
    {
        return true;
    }
    static inline bool classof(const ObjVar * node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::GepObjNode;
    }
    //@}

    /// Constructor
    GepObjVar(const MemObj* mem, NodeID i, const LocationSet& l, PNODEK ty = GepObjNode) :
        ObjVar(mem->getValue(), i, mem, ty), ls(l)
    {
        base = mem->getId();
    }

    /// offset of the mem object
    inline const LocationSet& getLocationSet() const
    {
        return ls;
    }

    /// offset of the mem object
    inline s32_t getConstantFieldIdx() const
    {
        return ls.accumulateConstantFieldIdx();
    }

    /// Set the base object from which this GEP node came from.
    inline void setBaseNode(NodeID base)
    {
        this->base = base;
    }

    /// Return the base object from which this GEP node came from.
    inline NodeID getBaseNode(void) const
    {
        return base;
    }

    /// Return the type of this gep object
    inline virtual const Type* getType() const
    {
        return SymbolTableInfo::SymbolInfo()->getFlatternedElemType(mem->getType(), ls.accumulateConstantFieldIdx());
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str() + "_" + std::to_string(ls.accumulateConstantFieldIdx());
        return "offset_" + std::to_string(ls.accumulateConstantFieldIdx());
    }

    virtual const std::string toString() const;
};

/*
 * Field-insensitive Gep Obj variable, this is dynamic generated for field sensitive analysis
 * Each field-insensitive gep obj node represents all fields of a MemObj (base)
 */
class FIObjVar: public ObjVar
{
public:

    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FIObjVar *)
    {
        return true;
    }
    static inline bool classof(const ObjVar * node)
    {
        return node->getNodeKind() == SVFVar::FIObjNode;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::FIObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::FIObjNode;
    }
    //@}

    /// Constructor
    FIObjVar(const Value* val, NodeID i, const MemObj* mem, PNODEK ty = FIObjNode) :
        ObjVar(val, i, mem, ty)
    {
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str() + " (base object)";
        return " (base object)";
    }

    virtual const std::string toString() const;
};

/*
 * Unique Return node of a procedure
 */
class RetPN: public ValVar
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetPN *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    static inline bool classof(const ValVar *node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::RetNode;
    }
    //@}

    /// Constructor
    RetPN(const SVFFunction* val, NodeID i) :
        ValVar(val->getLLVMFun(), i, RetNode)
    {
    }

    /// Return name of a LLVM value
    const std::string getValueName() const
    {
        return value->getName().str() + "_ret";
    }

    virtual const std::string toString() const;
};


/*
 * Unique vararg node of a procedure
 */
class VarArgPN: public ValVar
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const VarArgPN *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    static inline bool classof(const ValVar *node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::VarargNode;
    }
    //@}

    /// Constructor
    VarArgPN(const SVFFunction* val, NodeID i) :
        ValVar(val->getLLVMFun(), i, VarargNode)
    {
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        return value->getName().str() + "_vararg";
    }

    virtual const std::string toString() const;
};




/*
 * Dummy variable without any LLVM value
 */
class DummyValVar: public ValVar
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyValVar *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const ValVar *node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::DummyValNode;
    }
    //@}

    /// Constructor
    DummyValVar(NodeID i) : ValVar(nullptr, i, DummyValNode)
    {
    }


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
class DummyObjVar: public ObjVar
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyObjVar *)
    {
        return true;
    }
    static inline bool classof(const SVFVar *node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const ObjVar *node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == SVFVar::DummyObjNode;
    }
    //@}

    /// Constructor
    DummyObjVar(NodeID i,const MemObj* m, PNODEK ty = DummyObjNode)
        : ObjVar(nullptr, i, m, ty)
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

#endif /* OBJECTANDSYMBOL_H_ */
