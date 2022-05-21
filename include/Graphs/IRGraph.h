//===- SVFIR.h -- SVF IR Graph or PAG (Program Assignment Graph)--------------------------------------//
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
 * IRGraph.h
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */


#ifndef IRGRAPH_H_
#define IRGRAPH_H_

#include "MemoryModel/SVFStatements.h"
#include "MemoryModel/SVFVariables.h"
#include "Util/NodeIDAllocator.h"
#include "Util/SVFUtil.h"
#include "Graphs/ICFG.h"

namespace SVF
{
typedef SVFVar PAGNode;
typedef SVFStmt PAGEdge;

/*
* Graph representation of SVF IR. It can be seen as a program assignment graph (PAG)
*/
class IRGraph : public GenericGraph<SVFVar,SVFStmt>
{
public:
    typedef Set<const SVFStmt*> SVFStmtSet;
    typedef Map<const Value*,SVFStmtSet> ValueToEdgeMap;

protected:
    SVFStmt::KindToSVFStmtMapTy KindToSVFStmtSetMap;  // < SVFIR edge map containing all PAGEdges
    SVFStmt::KindToSVFStmtMapTy KindToPTASVFStmtSetMap;  // < SVFIR edge map containing only pointer-related edges, i.e, both LHS and RHS are of pointer type
    bool fromFile; ///< Whether the SVFIR is built according to user specified data from a txt file
    NodeID nodeNumAfterPAGBuild; // initial node number after building SVFIR, excluding later added nodes, e.g., gepobj nodes
    u32_t totalPTAPAGEdge;
    ValueToEdgeMap valueToEdgeMap;	///< Map llvm::Values to all corresponding PAGEdges
    SymbolTableInfo* symInfo;

    /// Add a node into the graph
    inline NodeID addNode(SVFVar* node, NodeID i)
    {
        addGNode(i,node);
        return i;
    }
    /// Add an edge into the graph
    bool addEdge(SVFVar* src, SVFVar* dst, SVFStmt* edge);

    //// Return true if this edge exits
    SVFStmt* hasNonlabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind);
    /// Return true if this labeled edge exits, including store, call and load
    /// two store edge can have same dst and src but located in different basic blocks, thus flags are needed to distinguish them
    SVFStmt* hasLabeledEdge(SVFVar* src, SVFVar* dst, SVFStmt::PEDGEK kind, const ICFGNode* cs);
    /// Return MultiOpndStmt since it has more than one operands (we use operand 2 here to make the flag)
    SVFStmt* hasLabeledEdge(SVFVar* src, SVFVar* op1, SVFStmt::PEDGEK kind, const SVFVar* op2);

    /// Map a value to a set of edges
    inline void mapValueToEdge(const Value *V, SVFStmt *edge)
    {
        auto inserted = valueToEdgeMap.emplace(V, SVFStmtSet{edge});
        if (!inserted.second)
        {
            inserted.first->second.emplace(edge);
        }
    }
    /// get MemObj according to LLVM value
    inline const MemObj* getMemObj(const Value* val) const
    {
        return symInfo->getObj(symInfo->getObjSym(val));
    }

public:
    IRGraph(bool buildFromFile): fromFile(buildFromFile), nodeNumAfterPAGBuild(0), totalPTAPAGEdge(0)
    {
        symInfo = SymbolTableInfo::SymbolInfo();
        // insert dummy value if a correct value cannot be found
        valueToEdgeMap[nullptr] = SVFStmtSet();
    }

    virtual ~IRGraph();

    /// Whether this SVFIR built from a txt file
    inline bool isBuiltFromFile()
    {
        return fromFile;
    }
    /// Get all SVFIR Edges that corresponds to an LLVM value
    inline const SVFStmtSet& getValueEdges(const Value *V)
    {
        auto it = valueToEdgeMap.find(V);
        if (it == valueToEdgeMap.end())
        {
            //special empty set
            return valueToEdgeMap.at(nullptr);
        }
        return it->second;
    }

    /// Get SVFIR Node according to LLVM value
    ///getNode - Return the node corresponding to the specified pointer.
    inline NodeID getValueNode(const Value *V)
    {
        return symInfo->getValSym(V);
    }
    inline bool hasValueNode(const Value* V)
    {
        return symInfo->hasValSym(V);
    }
    /// getObject - Return the obj node id refer to the memory object for the
    /// specified global, heap or alloca instruction according to llvm value.
    inline NodeID getObjectNode(const Value *V)
    {
        return symInfo->getObjSym(V);
    }
    /// GetReturnNode - Return the unique node representing the return value of a function
    inline NodeID getReturnNode(const SVFFunction* func) const
    {
        return symInfo->getRetSym(func->getLLVMFun());
    }
    /// getVarargNode - Return the unique node representing the variadic argument of a variadic function.
    inline NodeID getVarargNode(const SVFFunction* func) const
    {
        return symInfo->getVarargSym(func->getLLVMFun());
    }
    inline NodeID getBlackHoleNode() const
    {
        return symInfo->blackholeSymID();
    }
    inline NodeID getConstantNode() const
    {
        return symInfo->constantSymID();
    }
    inline NodeID getBlkPtr() const
    {
        return symInfo->blkPtrSymID();
    }
    inline NodeID getNullPtr() const
    {
        return symInfo->nullPtrSymID();
    }
    inline const MemObj* getBlackHoleObj() const
    {
        return symInfo->getBlkObj();
    }
    inline const MemObj* getConstantObj() const
    {
        return symInfo->getConstantObj();
    }

    inline u32_t getValueNodeNum() const
    {
        return symInfo->valSyms().size();
    }
    inline u32_t getObjectNodeNum() const
    {
        return symInfo->idToObjMap().size();
    }
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

    /// Dump SVFIR
    void dump(std::string name);

    /// View graph from the debugger
    void view();
};

}


namespace llvm
{

/* !
 * GraphTraits specializations of SVFIR to be used for the generic graph algorithms.
 * Provide graph traits for tranversing from a SVFIR node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::SVFVar*> : public GraphTraits<SVF::GenericNode<SVF::SVFVar,SVF::SVFStmt>*  >
{
};

/// Inverse GraphTraits specializations for SVFIR node, it is used for inverse traversal.
template<> struct GraphTraits<Inverse<SVF::SVFVar *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::SVFVar,SVF::SVFStmt>* > >
{
};

template<> struct GraphTraits<SVF::IRGraph*> : public GraphTraits<SVF::GenericGraph<SVF::SVFVar,SVF::SVFStmt>* >
{
    typedef SVF::SVFVar *NodeRef;
};

} // End namespace llvm
#endif /* IRGRAPH_H_ */
