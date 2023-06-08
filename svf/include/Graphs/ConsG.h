//===- ConsG.h -- Constraint graph representation-----------------------------//
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
 * ConstraintGraph.h
 *
 *  Created on: Oct 14, 2013
 *      Author: Yulei Sui
 */

#ifndef CONSG_H_
#define CONSG_H_

#include "Graphs/ConsGEdge.h"
#include "Graphs/ConsGNode.h"

namespace SVF
{

/*!
 * Constraint graph for Andersen's analysis
 * ConstraintNodes are same as PAGNodes
 * ConstraintEdges are self-defined edges (initialized with ConstraintEdges)
 */
class ConstraintGraph :  public GenericGraph<ConstraintNode,ConstraintEdge>
{

public:
    typedef OrderedMap<NodeID, ConstraintNode *> ConstraintNodeIDToNodeMapTy;
    typedef ConstraintEdge::ConstraintEdgeSetTy::iterator ConstraintNodeIter;
    typedef Map<NodeID, NodeID> NodeToRepMap;
    typedef Map<NodeID, NodeBS> NodeToSubsMap;
    typedef FIFOWorkList<NodeID> WorkList;

protected:
    SVFIR* pag;
    NodeToRepMap nodeToRepMap;
    NodeToSubsMap nodeToSubsMap;
    WorkList nodesToBeCollapsed;
    EdgeID edgeIndex;

    ConstraintEdge::ConstraintEdgeSetTy AddrCGEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy directEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy LoadCGEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy StoreCGEdgeSet;

    void buildCG();

    void destroy();

    SVFStmt::SVFStmtSetTy& getPAGEdgeSet(SVFStmt::PEDGEK kind)
    {
        return pag->getPTASVFStmtSet(kind);
    }

    /// Wappers used internally, not expose to Andernsen Pass
    //@{
    inline NodeID getValueNode(const SVFValue* value) const
    {
        return sccRepNode(pag->getValueNode(value));
    }

    inline NodeID getReturnNode(const SVFFunction* value) const
    {
        return pag->getReturnNode(value);
    }

    inline NodeID getVarargNode(const SVFFunction* value) const
    {
        return pag->getVarargNode(value);
    }
    //@}

public:
    /// Constructor
    ConstraintGraph(SVFIR* p): pag(p), edgeIndex(0)
    {
        buildCG();
    }
    /// Destructor
    virtual ~ConstraintGraph()
    {
        destroy();
    }

    /// Get/add/remove constraint node
    //@{
    inline ConstraintNode* getConstraintNode(NodeID id) const
    {
        id = sccRepNode(id);
        return getGNode(id);
    }
    inline void addConstraintNode(ConstraintNode* node, NodeID id)
    {
        addGNode(id,node);
    }
    inline bool hasConstraintNode(NodeID id) const
    {
        id = sccRepNode(id);
        return hasGNode(id);
    }
    inline void removeConstraintNode(ConstraintNode* node)
    {
        removeGNode(node);
    }
    //@}

    //// Return true if this edge exits
    inline bool hasEdge(ConstraintNode* src, ConstraintNode* dst, ConstraintEdge::ConstraintEdgeK kind)
    {
        ConstraintEdge edge(src,dst,kind);
        if(kind == ConstraintEdge::Copy ||
                kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep)
            return directEdgeSet.find(&edge) != directEdgeSet.end();
        else if(kind == ConstraintEdge::Addr)
            return AddrCGEdgeSet.find(&edge) != AddrCGEdgeSet.end();
        else if(kind == ConstraintEdge::Store)
            return StoreCGEdgeSet.find(&edge) != StoreCGEdgeSet.end();
        else if(kind == ConstraintEdge::Load)
            return LoadCGEdgeSet.find(&edge) != LoadCGEdgeSet.end();
        else
            assert(false && "no other kind!");
        return false;
    }

    /// Get an edge via its src and dst nodes and kind
    inline ConstraintEdge* getEdge(ConstraintNode* src, ConstraintNode* dst, ConstraintEdge::ConstraintEdgeK kind)
    {
        ConstraintEdge edge(src,dst,kind);
        if(kind == ConstraintEdge::Copy || kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep)
        {
            auto eit = directEdgeSet.find(&edge);
            return *eit;
        }
        else if(kind == ConstraintEdge::Addr)
        {
            auto eit = AddrCGEdgeSet.find(&edge);
            return *eit;
        }
        else if(kind == ConstraintEdge::Store)
        {
            auto eit = StoreCGEdgeSet.find(&edge);
            return *eit;
        }
        else if(kind == ConstraintEdge::Load)
        {
            auto eit = LoadCGEdgeSet.find(&edge);
            return *eit;
        }
        else
        {
            assert(false && "no other kind!");
            return nullptr;
        }
    }

    ///Add a SVFIR edge into Edge map
    //@{
    /// Add Address edge
    AddrCGEdge* addAddrCGEdge(NodeID src, NodeID dst);
    /// Add Copy edge
    CopyCGEdge* addCopyCGEdge(NodeID src, NodeID dst);
    /// Add Gep edge
    NormalGepCGEdge* addNormalGepCGEdge(NodeID src, NodeID dst, const AccessPath& ap);
    VariantGepCGEdge* addVariantGepCGEdge(NodeID src, NodeID dst);
    /// Add Load edge
    LoadCGEdge* addLoadCGEdge(NodeID src, NodeID dst);
    /// Add Store edge
    StoreCGEdge* addStoreCGEdge(NodeID src, NodeID dst);
    //@}

    ///Get SVFIR edge
    //@{
    /// Get Address edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getAddrCGEdges()
    {
        return AddrCGEdgeSet;
    }
    /// Get Copy/call/ret/gep edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getDirectCGEdges()
    {
        return directEdgeSet;
    }
    /// Get Load edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getLoadCGEdges()
    {
        return LoadCGEdgeSet;
    }
    /// Get Store edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getStoreCGEdges()
    {
        return StoreCGEdgeSet;
    }
    //@}

    /// Used for cycle elimination
    //@{
    /// Remove edge from old dst target, change edge dst id and add modifed edge into new dst
    void reTargetDstOfEdge(ConstraintEdge* edge, ConstraintNode* newDstNode);
    /// Remove edge from old src target, change edge dst id and add modifed edge into new src
    void reTargetSrcOfEdge(ConstraintEdge* edge, ConstraintNode* newSrcNode);
    /// Remove addr edge from their src and dst edge sets
    void removeAddrEdge(AddrCGEdge* edge);
    /// Remove direct edge from their src and dst edge sets
    void removeDirectEdge(ConstraintEdge* edge);
    /// Remove load edge from their src and dst edge sets
    void removeLoadEdge(LoadCGEdge* edge);
    /// Remove store edge from their src and dst edge sets
    void removeStoreEdge(StoreCGEdge* edge);
    //@}

    /// SCC rep/sub nodes methods
    //@{
    inline NodeID sccRepNode(NodeID id) const
    {
        NodeToRepMap::const_iterator it = nodeToRepMap.find(id);
        if(it==nodeToRepMap.end())
            return id;
        else
            return it->second;
    }
    inline NodeBS& sccSubNodes(NodeID id)
    {
        nodeToSubsMap[id].set(id);
        return nodeToSubsMap[id];
    }
    inline void setRep(NodeID node, NodeID rep)
    {
        nodeToRepMap[node] = rep;
    }
    inline void setSubs(NodeID node, NodeBS& subs)
    {
        nodeToSubsMap[node] |= subs;
    }
    inline void resetSubs(NodeID node)
    {
        nodeToSubsMap.erase(node);
    }
    inline NodeBS& getSubs(NodeID node)
    {
        return nodeToSubsMap[node];
    }
    inline NodeID getRep(NodeID node)
    {
        return nodeToRepMap[node];
    }
    inline void resetRep(NodeID node)
    {
        nodeToRepMap.erase(node);
    }
    //@}

    /// Move incoming direct edges of a sub node which is outside the SCC to its rep node
    /// Remove incoming direct edges of a sub node which is inside the SCC from its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    bool moveInEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep );

    /// Move outgoing direct edges of a sub node which is outside the SCC to its rep node
    /// Remove outgoing direct edges of sub node which is inside the SCC from its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    bool moveOutEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep );

    /// Move incoming/outgoing direct edges of a sub node to its rep node
    /// Return TRUE if there's a gep edge inside this SCC (PWC).
    inline bool moveEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep )
    {
        bool gepIn = moveInEdgesToRepNode(node, rep);
        bool gepOut = moveOutEdgesToRepNode(node, rep);
        return (gepIn || gepOut);
    }

    /// Check if a given edge is a NormalGepCGEdge with 0 offset.
    inline bool isZeroOffsettedGepCGEdge(ConstraintEdge *edge) const
    {
        if (NormalGepCGEdge *normalGepCGEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
            if (0 == normalGepCGEdge->getConstantFieldIdx())
                return true;
        return false;
    }

    /// Wrappers for invoking SVFIR methods
    //@{
    inline const SVFIR::CallSiteToFunPtrMap& getIndirectCallsites() const
    {
        return pag->getIndirectCallsites();
    }
    inline NodeID getBlackHoleNode()
    {
        return pag->getBlackHoleNode();
    }
    inline bool isBlkObjOrConstantObj(NodeID id)
    {
        return pag->isBlkObjOrConstantObj(id);
    }
    inline NodeBS& getAllFieldsObjVars(NodeID id)
    {
        return pag->getAllFieldsObjVars(id);
    }
    inline NodeID getBaseObjVar(NodeID id)
    {
        return pag->getBaseObjVar(id);
    }
    inline bool isSingleFieldObj(NodeID id) const
    {
        const MemObj* mem = pag->getBaseObj(id);
        return (mem->getMaxFieldOffsetLimit() == 1);
    }
    /// Get a field of a memory object
    inline NodeID getGepObjVar(NodeID id, const APOffset& apOffset)
    {
        NodeID gep =  pag->getGepObjVar(id, apOffset);
        /// Create a node when it is (1) not exist on graph and (2) not merged
        if(sccRepNode(gep)==gep && hasConstraintNode(gep)==false)
            addConstraintNode(new ConstraintNode(gep),gep);
        return gep;
    }
    /// Get a field-insensitive node of a memory object
    inline NodeID getFIObjVar(NodeID id)
    {
        NodeID fi = pag->getFIObjVar(id);
        /// The fi obj in PAG must be either an existing node or merged to another rep node in ConsG
        assert((hasConstraintNode(fi) || sccRepNode(fi) != fi) && "non-existing fi obj??");
        return fi;
    }
    //@}

    /// Check/Set PWC (positive weight cycle) flag
    //@{
    inline bool isPWCNode(NodeID nodeId)
    {
        return getConstraintNode(nodeId)->isPWCNode();
    }
    inline void setPWCNode(NodeID nodeId)
    {
        getConstraintNode(nodeId)->setPWCNode();
    }
    //@}

    /// Add/get nodes to be collapsed
    //@{
    inline bool hasNodesToBeCollapsed() const
    {
        return (!nodesToBeCollapsed.empty());
    }
    inline void addNodeToBeCollapsed(NodeID id)
    {
        nodesToBeCollapsed.push(id);
    }
    inline NodeID getNextCollapseNode()
    {
        return nodesToBeCollapsed.pop();
    }
    //@}

    /// Dump graph into dot file
    void dump(std::string name);
    /// Print CG into terminal
    void print();

    /// View graph from the debugger.
    void view();
};

} // End namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for the generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::ConstraintNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::ConstraintNode,SVF::ConstraintEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for Value flow node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::ConstraintNode *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::ConstraintNode,SVF::ConstraintEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::ConstraintGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::ConstraintNode,SVF::ConstraintEdge>* >
{
    typedef SVF::ConstraintNode *NodeRef;
};

} // End namespace llvm

#endif /* CONSG_H_ */
