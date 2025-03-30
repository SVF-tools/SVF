//===- SVFIR.h -- SVF IR Graph or PAG (Program Assignment Graph)--------------//
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
 * IRGraph.h
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */


#ifndef IRGRAPH_H_
#define IRGRAPH_H_

#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include "Util/NodeIDAllocator.h"
#include "Util/SVFUtil.h"
#include "Graphs/ICFG.h"

namespace SVF
{
typedef SVFVar PAGNode;
typedef SVFStmt PAGEdge;

class ObjTypeInfo;

/*
 * Graph representation of SVF IR.
 * It can be seen as a program assignment graph (PAG).
 */
class IRGraph : public GenericGraph<SVFVar, SVFStmt>
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;
    friend class SymbolTableBuilder;

public:

    /// Symbol types
    enum SYMTYPE
    {
        NullPtr,
        BlkPtr,
        BlackHole,
        ConstantObj,
        ValSymbol,
        ObjSymbol,
        RetSymbol,
        VarargSymbol
    };

    /// various maps defined
    //{@
    /// sym id to obj type info map
    typedef OrderedMap<NodeID, ObjTypeInfo*> IDToTypeInfoMapTy;

    /// function to sym id map
    typedef OrderedMap<const FunObjVar*, NodeID> FunObjVarToIDMapTy;

    /// struct type to struct info map
    typedef Set<const SVFType*> SVFTypeSet;
    //@}

private:
    FunObjVarToIDMapTy returnFunObjSymMap; ///< return map
    FunObjVarToIDMapTy varargFunObjSymMap; ///< vararg map
    IDToTypeInfoMapTy objTypeInfoMap;       ///< map a memory sym id to its obj

    /// (owned) All SVF Types
    /// Every type T is mapped to StInfo
    /// which contains size (fsize) , offset(foffset)
    /// fsize[i] is the number of fields in the largest such struct, else fsize[i] = 1.
    /// fsize[0] is always the size of the expanded struct.
    SVFTypeSet svfTypes;

    /// @brief (owned) All StInfo
    Set<const StInfo*> stInfos;

    /// total number of symbols
    NodeID totalSymNum;

    void destorySymTable();

public:
    typedef Set<const SVFStmt*> SVFStmtSet;

protected:
    SVFStmt::KindToSVFStmtMapTy KindToSVFStmtSetMap; ///< SVFIR edge map containing all PAGEdges
    SVFStmt::KindToSVFStmtMapTy KindToPTASVFStmtSetMap; ///< SVFIR edge map containing only pointer-related edges, i.e., both LHS and RHS are of pointer type
    bool fromFile; ///< Whether the SVFIR is built according to user specified data from a txt file
    NodeID nodeNumAfterPAGBuild; ///< initial node number after building SVFIR, excluding later added nodes, e.g., gepobj nodes
    u32_t totalPTAPAGEdge;
    u32_t valVarNum;
    u32_t objVarNum;

    /// Add a node into the graph
    inline NodeID addNode(SVFVar* node)
    {
        assert(node && "cannot add a null node");
        addGNode(node->getId(),node);
        return node->getId();
    }
    /// Add an edge into the graph
    bool addEdge(SVFVar* src, SVFVar* dst, SVFStmt* edge);

    //// Return true if this edge exits
    SVFStmt* hasNonlabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind);
    /// Return true if this labeled edge exits, including store, call and load
    /// two store edge can have same dst and src but located in different basic
    /// blocks, thus flags are needed to distinguish them
    SVFStmt* hasLabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind,
                            const ICFGNode* cs);
    SVFStmt* hasEdge(SVFStmt* edge, SVFStmt::PEDGEK kind);
    /// Return MultiOpndStmt since it has more than one operands (we use operand
    /// 2 here to make the flag)
    SVFStmt* hasLabeledEdge(SVFVar* src, SVFVar* op1, SVFStmt::PEDGEK kind,
                            const SVFVar* op2);

public:
    IRGraph(bool buildFromFile)
        : totalSymNum(0), fromFile(buildFromFile), nodeNumAfterPAGBuild(0), totalPTAPAGEdge(0), valVarNum(0), objVarNum(0),
          maxStruct(nullptr), maxStSize(0)
    {
    }

    virtual ~IRGraph();


    /// Whether this SVFIR built from a txt file
    inline bool isBuiltFromFile()
    {
        return fromFile;
    }

    /// special value
    // @{
    static inline bool isBlkPtr(NodeID id)
    {
        return (id == BlkPtr);
    }
    static inline bool isNullPtr(NodeID id)
    {
        return (id == NullPtr);
    }
    static inline bool isBlkObj(NodeID id)
    {
        return (id == BlackHole);
    }
    static inline bool isConstantSym(NodeID id)
    {
        return (id == ConstantObj);
    }
    static inline bool isBlkObjOrConstantObj(NodeID id)
    {
        return (isBlkObj(id) || isConstantSym(id));
    }

    inline NodeID blkPtrSymID() const
    {
        return BlkPtr;
    }

    inline NodeID nullPtrSymID() const
    {
        return NullPtr;
    }

    inline NodeID constantSymID() const
    {
        return ConstantObj;
    }

    inline NodeID blackholeSymID() const
    {
        return BlackHole;
    }

    /// Statistics
    //@{
    inline u32_t getTotalSymNum() const
    {
        return totalSymNum;
    }
    inline u32_t getMaxStructSize() const
    {
        return maxStSize;
    }
    //@}

    /// Get different kinds of syms maps
    //@{
    inline IDToTypeInfoMapTy& idToObjTypeInfoMap()
    {
        return objTypeInfoMap;
    }

    inline const IDToTypeInfoMapTy& idToObjTypeInfoMap() const
    {
        return objTypeInfoMap;
    }

    inline FunObjVarToIDMapTy& retFunObjSyms()
    {
        return returnFunObjSymMap;
    }

    inline FunObjVarToIDMapTy& varargFunObjSyms()
    {
        return varargFunObjSymMap;
    }

    //@}

    inline ObjTypeInfo* getObjTypeInfo(NodeID id) const
    {
        IDToTypeInfoMapTy::const_iterator iter = objTypeInfoMap.find(id);
        assert(iter!=objTypeInfoMap.end() && "obj type info not found");
        return iter->second;
    }

    /// GetReturnNode - Return the unique node representing the return value of a function
    NodeID getReturnNode(const FunObjVar*func) const;

    /// getVarargNode - Return the unique node representing the variadic argument of a variadic function.
    NodeID getVarargNode(const FunObjVar*func) const;

    inline NodeID getBlackHoleNode() const
    {
        return blackholeSymID();
    }
    inline NodeID getConstantNode() const
    {
        return constantSymID();
    }
    inline NodeID getBlkPtr() const
    {
        return blkPtrSymID();
    }
    inline NodeID getNullPtr() const
    {
        return nullPtrSymID();
    }

    u32_t getValueNodeNum();

    u32_t getObjectNodeNum();

    /// Constant reader that won't change the state of the symbol table
    //@{
    inline const SVFTypeSet& getSVFTypes() const
    {
        return svfTypes;
    }

    inline const SVFType* getSVFType(const std::string& name) const
    {
        for(const SVFType* type : svfTypes)
        {
            if(type->toString() == name)
                return type;
        }
        return nullptr;
    }

    inline void addSVFTypes(const Set<SVFType*>* types)
    {
        svfTypes.insert(types->begin(), types->end());
    }

    inline void addStInfos(const Set<StInfo*>* stInfos)
    {
        this->stInfos.insert(stInfos->begin(), stInfos->end());
    }

    inline const Set<const StInfo*>& getStInfos() const
    {
        return stInfos;
    }
    //@}
    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual APOffset getModulusOffset(const BaseObjVar* baseObj, const APOffset& apOffset);
    /// Get struct info
    //@{
    ///Get a reference to StructInfo.
    const StInfo* getTypeInfo(const SVFType* T) const;
    inline bool hasSVFTypeInfo(const SVFType* T)
    {
        return svfTypes.find(T) != svfTypes.end();
    }

    /// Create an objectInfo based on LLVM type (value is null, and type could be null, representing a dummy object)
    ObjTypeInfo* createObjTypeInfo(const SVFType* type);

    const ObjTypeInfo* createDummyObjTypeInfo(NodeID symId, const SVFType* type);

    ///Get a reference to the components of struct_info.
    /// Number of flattened elements of an array or struct
    u32_t getNumOfFlattenElements(const SVFType* T);
    /// Flattened element idx of an array or struct by considering stride
    u32_t getFlattenedElemIdx(const SVFType* T, u32_t origId);
    /// Return the type of a flattened element given a flattened index
    const SVFType* getFlatternedElemType(const SVFType* baseType, u32_t flatten_idx);
    ///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
    ///  OriginalElemType of b with field_idx 1 : Struct A
    ///  FlatternedElemType of b with field_idx 1 : int
    const SVFType* getOriginalElemType(const SVFType* baseType, u32_t origId) const;
    //@}

    /// Debug method
    void printFlattenFields(const SVFType* type);


    inline u32_t getNodeNumAfterPAGBuild() const
    {
        return nodeNumAfterPAGBuild;
    }
    inline void setNodeNumAfterPAGBuild(u32_t num)
    {
        nodeNumAfterPAGBuild = num;
    }

    inline u32_t getPAGNodeNum() const
    {
        return nodeNum;
    }
    inline u32_t getPAGEdgeNum() const
    {
        return edgeNum;
    }
    inline u32_t getPTAPAGEdgeNum() const
    {
        return totalPTAPAGEdge;
    }
    /// Return graph name
    inline std::string getGraphName() const
    {
        return "SVFIR";
    }

    void dumpSymTable();

    /// Dump SVFIR
    void dump(std::string name);

    /// View graph from the debugger
    void view();



    ///The struct type with the most fields
    const SVFType* maxStruct;

    ///The number of fields in max_struct
    u32_t maxStSize;

    inline void addTypeInfo(const SVFType* ty)
    {
        bool inserted = svfTypes.insert(ty).second;
        if(!inserted)
            assert(false && "this type info has been added before");
    }

    inline void addStInfo(StInfo* stInfo)
    {
        stInfo->setStinfoId(stInfos.size());
        stInfos.insert(stInfo);
    }

protected:

    /// Return the flattened field type for struct type only
    const std::vector<const SVFType*>& getFlattenFieldTypes(const SVFStructType *T);
};

}

namespace SVF
{

/* !
 * GenericGraphTraits specializations of SVFIR to be used for the generic graph algorithms.
 * Provide graph traits for traversing from a SVFIR node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::SVFVar*> : public GenericGraphTraits<SVF::GenericNode<SVF::SVFVar,SVF::SVFStmt>*  >
{
};

/// Inverse GenericGraphTraits specializations for SVFIR node, it is used for inverse traversal.
template<> struct GenericGraphTraits<Inverse<SVF::SVFVar *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::SVFVar,SVF::SVFStmt>* > >
{
};

template<> struct GenericGraphTraits<SVF::IRGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::SVFVar,SVF::SVFStmt>* >
{
    typedef SVF::SVFVar* NodeRef;
};

} // End namespace llvm
#endif /* IRGRAPH_H_ */
