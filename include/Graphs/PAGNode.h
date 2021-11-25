//===- PAGNode.h -- PAG node class-------------------------------------------//
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
 * PAGNode.h
 *
 *  Created on: Nov 10, 2013
 *      Author: Yulei Sui
 */

#ifndef PAGNODE_H_
#define PAGNODE_H_

#include "Graphs/GenericGraph.h"
#include "MemoryModel/MemModel.h"
#include "SVF-FE/SymbolTableInfo.h"
#include "SVF-FE/LLVMUtil.h"
#include "Graphs/PAGEdge.h"

namespace SVF
{

/*
 * PAG node
 */
typedef GenericNode<PAGNode,PAGEdge> GenericPAGNodeTy;
class PAGNode : public GenericPAGNodeTy
{

public:
    /// Nine kinds of PAG nodes
    /// ValNode: llvm pointer value
    /// ObjNode: memory object
    /// RetNode: unique return node
    /// Vararg: unique node for vararg parameter
    /// GepValNode: tempory gep value node for field sensitivity
    /// GepValNode: tempory gep obj node for field sensitivity
    /// FIObjNode: for field insensitive analysis
    /// DummyValNode and DummyObjNode: for non-llvm-value node
    /// Clone*Node: objects created by TBHC.
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
        CloneGepObjNode,   // NOTE: only used for TBHC.
        CloneFIObjNode,    // NOTE: only used for TBHC.
        CloneDummyObjNode  // NOTE: only used for TBHC.
    };


protected:
    const Value* value; ///< value of this PAG node
    PAGEdge::PAGKindToEdgeSetMapTy InEdgeKindToSetMap;
    PAGEdge::PAGKindToEdgeSetMapTy OutEdgeKindToSetMap;
    bool isTLPointer;	/// top-level pointer
    bool isATPointer;	/// address-taken pointer

public:
    /// Constructor
    PAGNode(const Value* val, NodeID i, PNODEK k);
    /// Destructor
    virtual ~PAGNode()
    {
    }

    ///  Get/has methods of the components
    //@{
    inline const Value* getValue() const
    {
        assert((this->getNodeKind() != DummyValNode && this->getNodeKind() != DummyObjNode) && "dummy node do not have value!");
        assert((this->getId()!=SYMTYPE::BlackHole && this->getId() != SYMTYPE::ConstantObj) && "blackhole and constant obj do not have value");
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
        return isTopLevelPtr() || isAddressTakenPtr();
    }
    /// Whether it is a top-level pointer
    inline bool isTopLevelPtr() const
    {
        return isTLPointer;
    }
    /// Whether it is an address-taken pointer
    inline bool isAddressTakenPtr() const
    {
        return isATPointer;
    }
    /// Whether it is constant data, i.e., "0", "1.001", "str"
    /// or llvm's metadata, i.e., metadata !4087
    inline bool isConstantData() const
    {
        if (hasValue())
            return SVFUtil::isConstantData(value);
        else
            return false;
    }

    /// Whether this is an isoloated node on the PAG graph
    bool isIsolatedNode() const;

    /// Get name of the LLVM value
    virtual const std::string getValueName() const = 0;

    /// Return the function that this PAGNode resides in. Return nullptr if it is a global or constantexpr node
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

    /// Get incoming PAG edges
    inline PAGEdge::PAGEdgeSetTy& getIncomingEdges(PAGEdge::PEDGEK kind)
    {
        return InEdgeKindToSetMap[kind];
    }

    /// Get outgoing PAG edges
    inline PAGEdge::PAGEdgeSetTy& getOutgoingEdges(PAGEdge::PEDGEK kind)
    {
        return OutEdgeKindToSetMap[kind];
    }

    /// Has incoming PAG edges
    inline bool hasIncomingEdges(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        if (it != InEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Has incoming VariantGepEdges
    inline bool hasIncomingVariantGepEdge() const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(PAGEdge::VariantGep);
        if (it != InEdgeKindToSetMap.end())
        {
            return (!it->second.empty());
        }
        return false;
    }

    /// Get incoming PAGEdge iterator
    inline PAGEdge::PAGEdgeSetTy::iterator getIncomingEdgesBegin(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get incoming PAGEdge iterator
    inline PAGEdge::PAGEdgeSetTy::iterator getIncomingEdgesEnd(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = InEdgeKindToSetMap.find(kind);
        assert(it!=InEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }

    /// Has outgoing PAG edges
    inline bool hasOutgoingEdges(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        if (it != OutEdgeKindToSetMap.end())
            return (!it->second.empty());
        else
            return false;
    }

    /// Get outgoing PAGEdge iterator
    inline PAGEdge::PAGEdgeSetTy::iterator getOutgoingEdgesBegin(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.begin();
    }

    /// Get outgoing PAGEdge iterator
    inline PAGEdge::PAGEdgeSetTy::iterator getOutgoingEdgesEnd(PAGEdge::PEDGEK kind) const
    {
        PAGEdge::PAGKindToEdgeSetMapTy::const_iterator it = OutEdgeKindToSetMap.find(kind);
        assert(it!=OutEdgeKindToSetMap.end() && "The node does not have such kind of edge");
        return it->second.end();
    }
    //@}

    ///  add methods of the components
    //@{
    inline void addInEdge(PAGEdge* inEdge)
    {
        GNodeK kind = inEdge->getEdgeKind();
        InEdgeKindToSetMap[kind].insert(inEdge);
        addIncomingEdge(inEdge);
    }

    inline void addOutEdge(PAGEdge* outEdge)
    {
        GNodeK kind = outEdge->getEdgeKind();
        OutEdgeKindToSetMap[kind].insert(outEdge);
        addOutgoingEdge(outEdge);
    }

    virtual const std::string toString() const;

    /// Get shape and/or color of node for .dot display.
    virtual const std::string getNodeAttrForDotDisplay() const;

    /// Dump to console for debugging
    void dump() const;

    //@}
    /// Overloading operator << for dumping PAGNode value
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const PAGNode &node)
    {
        o << node.toString();
        return o;
    }
    //@}
};



/*
 * Value(Pointer) node
 */
class ValPN: public PAGNode
{

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ValPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::ValNode ||
               node->getNodeKind() == PAGNode::GepValNode ||
               node->getNodeKind() == PAGNode::DummyValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::ValNode ||
               node->getNodeKind() == PAGNode::GepValNode ||
               node->getNodeKind() == PAGNode::DummyValNode;
    }
    //@}

    /// Constructor
    ValPN(const Value* val, NodeID i, PNODEK ty = ValNode) :
        PAGNode(val, i, ty)
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
 * Memory Object node
 */
class ObjPN: public PAGNode
{

protected:
    const MemObj* mem;	///< memory object
    /// Constructor
    ObjPN(const Value* val, NodeID i, const MemObj* m, PNODEK ty = ObjNode) :
        PAGNode(val, i, ty), mem(m)
    {
    }
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const ObjPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::ObjNode ||
               node->getNodeKind() == PAGNode::GepObjNode ||
               node->getNodeKind() == PAGNode::FIObjNode ||
               node->getNodeKind() == PAGNode::DummyObjNode ||
               node->getNodeKind() == PAGNode::CloneGepObjNode ||
               node->getNodeKind() == PAGNode::CloneFIObjNode ||
               node->getNodeKind() == PAGNode::CloneDummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::ObjNode ||
               node->getNodeKind() == PAGNode::GepObjNode ||
               node->getNodeKind() == PAGNode::FIObjNode ||
               node->getNodeKind() == PAGNode::DummyObjNode ||
               node->getNodeKind() == PAGNode::CloneGepObjNode ||
               node->getNodeKind() == PAGNode::CloneFIObjNode ||
               node->getNodeKind() == PAGNode::CloneDummyObjNode;
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
 * Gep Value (Pointer) node, this node can be dynamic generated for field sensitive analysis
 * e.g. memcpy, temp gep value node needs to be created
 * Each Gep Value node is connected to base value node via gep edge
 */
class GepValPN: public ValPN
{

private:
    LocationSet ls;	// LocationSet
    const Type *gepValType;
    u32_t fieldIdx;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepValPN *)
    {
        return true;
    }
    static inline bool classof(const ValPN * node)
    {
        return node->getNodeKind() == PAGNode::GepValNode;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::GepValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::GepValNode;
    }
    //@}

    /// Constructor
    GepValPN(const Value* val, NodeID i, const LocationSet& l, const Type *ty, u32_t idx) :
        ValPN(val, i, GepValNode), ls(l), gepValType(ty), fieldIdx(idx)
    {
    }

    /// offset of the base value node
    inline u32_t getOffset() const
    {
        return ls.getOffset();
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str() + "_" + llvm::utostr(getOffset());
        return "offset_" + llvm::utostr(getOffset());
    }

    inline const Type* getType() const
    {
        return gepValType;
    }

    u32_t getFieldIdx() const
    {
        return fieldIdx;
    }

    virtual const std::string toString() const;
};


/*
 * Gep Obj node, this is dynamic generated for field sensitive analysis
 * Each gep obj node is one field of a MemObj (base)
 */
class GepObjPN: public ObjPN
{
private:
    LocationSet ls;
    NodeID base = 0;

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepObjPN *)
    {
        return true;
    }
    static inline bool classof(const ObjPN * node)
    {
        return node->getNodeKind() == PAGNode::GepObjNode
               || node->getNodeKind() == PAGNode::CloneGepObjNode;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::GepObjNode
               || node->getNodeKind() == PAGNode::CloneGepObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::GepObjNode
               || node->getNodeKind() == PAGNode::CloneGepObjNode;
    }
    //@}

    /// Constructor
    GepObjPN(const MemObj* mem, NodeID i, const LocationSet& l, PNODEK ty = GepObjNode) :
        ObjPN(mem->getRefVal(), i, mem, ty), ls(l)
    {
        base = mem->getSymId();
    }

    /// offset of the mem object
    inline const LocationSet& getLocationSet() const
    {
        return ls;
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
    inline virtual const llvm::Type* getType() const
    {
        return SymbolTableInfo::SymbolInfo()->getOrigSubTypeWithByteOffset(mem->getType(), ls.getByteOffset());
    }

    /// Return name of a LLVM value
    inline const std::string getValueName() const
    {
        if (value && value->hasName())
            return value->getName().str() + "_" + llvm::itostr(ls.getOffset());
        return "offset_" + llvm::itostr(ls.getOffset());
    }

    virtual const std::string toString() const;
};

/*
 * Field-insensitive Gep Obj node, this is dynamic generated for field sensitive analysis
 * Each field-insensitive gep obj node represents all fields of a MemObj (base)
 */
class FIObjPN: public ObjPN
{
public:

    ///  Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const FIObjPN *)
    {
        return true;
    }
    static inline bool classof(const ObjPN * node)
    {
        return node->getNodeKind() == PAGNode::FIObjNode
               || node->getNodeKind() == PAGNode::CloneFIObjNode;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::FIObjNode
               || node->getNodeKind() == PAGNode::CloneFIObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::FIObjNode
               || node->getNodeKind() == PAGNode::CloneFIObjNode;
    }
    //@}

    /// Constructor
    FIObjPN(const Value* val, NodeID i, const MemObj* mem, PNODEK ty = FIObjNode) :
        ObjPN(val, i, mem, ty)
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
class RetPN: public PAGNode
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::RetNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::RetNode;
    }
    //@}

    /// Constructor
    RetPN(const SVFFunction* val, NodeID i) :
        PAGNode(val->getLLVMFun(), i, RetNode)
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
class VarArgPN: public PAGNode
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const VarArgPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::VarargNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::VarargNode;
    }
    //@}

    /// Constructor
    VarArgPN(const SVFFunction* val, NodeID i) :
        PAGNode(val->getLLVMFun(), i, VarargNode)
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
 * Dummy node
 */
class DummyValPN: public ValPN
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyValPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::DummyValNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::DummyValNode;
    }
    //@}

    /// Constructor
    DummyValPN(NodeID i) : ValPN(nullptr, i, DummyValNode)
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
 * Dummy node
 */
class DummyObjPN: public ObjPN
{

public:

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DummyObjPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::DummyObjNode
               || node->getNodeKind() == PAGNode::CloneDummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::DummyObjNode
               || node->getNodeKind() == PAGNode::CloneDummyObjNode;
    }
    //@}

    /// Constructor
    DummyObjPN(NodeID i,const MemObj* m, PNODEK ty = DummyObjNode)
        : ObjPN(nullptr, i, m, ty)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "dummyObj";
    }

    virtual const std::string toString() const;
};

/*
 * Clone object node for dummy objects.
 */
class CloneDummyObjPN: public DummyObjPN
{
public:
    //@{ Methods to support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CloneDummyObjPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::CloneDummyObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::CloneDummyObjNode;
    }
    //@}

    /// Constructor
    CloneDummyObjPN(NodeID i, const MemObj* m, PNODEK ty = CloneDummyObjNode)
        : DummyObjPN(i, m, ty)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "clone of " + ObjPN::getValueName();
    }

    virtual const std::string toString() const;
};

/*
 * Clone object for GEP objects.
 */
class CloneGepObjPN : public GepObjPN
{
public:
    //@{ Methods to support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CloneGepObjPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::CloneGepObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::CloneGepObjNode;
    }
    //@}

    /// Constructor
    CloneGepObjPN(const MemObj* mem, NodeID i, const LocationSet& l, PNODEK ty = CloneGepObjNode) :
        GepObjPN(mem, i, l, ty)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "clone (gep) of " + GepObjPN::getValueName();
    }

    virtual const std::string toString() const;
};

/*
 * Clone object for FI objects.
 */
class CloneFIObjPN : public FIObjPN
{
public:
    //@{ Methods to support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CloneFIObjPN *)
    {
        return true;
    }
    static inline bool classof(const PAGNode *node)
    {
        return node->getNodeKind() == PAGNode::CloneFIObjNode;
    }
    static inline bool classof(const GenericPAGNodeTy *node)
    {
        return node->getNodeKind() == PAGNode::CloneFIObjNode;
    }
    //@}

    /// Constructor
    CloneFIObjPN(const Value* val, NodeID i, const MemObj* mem, PNODEK ty = CloneFIObjNode) :
        FIObjPN(val, i, mem, ty)
    {
    }

    /// Return name of this node
    inline const std::string getValueName() const
    {
        return "clone (FI) of " + FIObjPN::getValueName();
    }

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* PAGNODE_H_ */
