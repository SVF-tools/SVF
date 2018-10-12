//===- ConsG.h -- Constraint graph representation-----------------------------//
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
 * ConstraintGraph.h
 *
 *  Created on: Oct 14, 2013
 *      Author: Yulei Sui
 */

#ifndef CONSG_H_
#define CONSG_H_

#include "MemoryModel/ConsGEdge.h"
#include "MemoryModel/ConsGNode.h"

/*!
 * Constraint graph for Andersen's analysis
 * ConstraintNodes are same as PAGNodes
 * ConstraintEdges are self-defined edges (initialized with ConstraintEdges)
 */
class ConstraintGraph :  public GenericGraph<ConstraintNode,ConstraintEdge> {

public:
    typedef llvm::DenseMap<NodeID, ConstraintNode *> ConstraintNodeIDToNodeMapTy;
    typedef ConstraintEdge::ConstraintEdgeSetTy::iterator ConstraintNodeIter;
    typedef llvm::DenseMap<NodeID, NodeID> NodeToRepMap;
    typedef llvm::DenseMap<NodeID, NodeBS> NodeToSubsMap;
    typedef FIFOWorkList<NodeID> WorkList;
private:
    PAG*pag;
    NodeToRepMap nodeToRepMap;
    NodeToSubsMap nodeToSubsMap;

    ConstraintEdge::ConstraintEdgeSetTy AddrCGEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy directEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy LoadCGEdgeSet;
    ConstraintEdge::ConstraintEdgeSetTy StoreCGEdgeSet;

    EdgeID edgeIndex;

    WorkList nodesToBeCollapsed;

    void buildCG();

    void destroy();

    /// Wappers used internally, not expose to Andernsen Pass
    //@{
    inline NodeID getValueNode(const llvm::Value* value) const {
        return sccRepNode(pag->getValueNode(value));
    }

    inline NodeID getReturnNode(const llvm::Function* value) const {
        return pag->getReturnNode(value);
    }

    inline NodeID getVarargNode(const llvm::Function* value) const {
        return pag->getVarargNode(value);
    }
    //@}

public:
    /// Constructor
    ConstraintGraph(PAG* p): pag(p), edgeIndex(0) {
        buildCG();
    }
    /// Destructor
    virtual ~ConstraintGraph() {
        destroy();
    }

    /// Get/add/remove constraint node
    //@{
    inline ConstraintNode* getConstraintNode(NodeID id) const {
        id = sccRepNode(id);
        return getGNode(id);
    }
    inline void addConstraintNode(ConstraintNode* node, NodeID id) {
        addGNode(id,node);
    }
    inline bool hasConstraintNode(NodeID id) const {
        return hasGNode(id);
    }
    inline void removeConstraintNode(ConstraintNode* node) {
        removeGNode(node);
    }
    //@}

    //// Return true if this edge exits
    inline bool hasEdge(ConstraintNode* src, ConstraintNode* dst, ConstraintEdge::ConstraintEdgeK kind) {
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

    ///Add a PAG edge into Edge map
    //@{
    /// Add Address edge
    bool addAddrCGEdge(NodeID src, NodeID dst);
    /// Add Copy edge
    bool addCopyCGEdge(NodeID src, NodeID dst);
    /// Add Gep edge
    bool addNormalGepCGEdge(NodeID src, NodeID dst, const LocationSet& ls);
    bool addVariantGepCGEdge(NodeID src, NodeID dst);
    /// Add Load edge
    bool addLoadCGEdge(NodeID src, NodeID dst);
    /// Add Store edge
    bool addStoreCGEdge(NodeID src, NodeID dst);
    //@}

    ///Get PAG edge
    //@{
    /// Get Address edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getAddrCGEdges() {
        return AddrCGEdgeSet;
    }
    /// Get Copy/call/ret/gep edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getDirectCGEdges() {
        return directEdgeSet;
    }
    /// Get Load edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getLoadCGEdges() {
        return LoadCGEdgeSet;
    }
    /// Get Store edges
    inline ConstraintEdge::ConstraintEdgeSetTy& getStoreCGEdges() {
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
    inline NodeID sccRepNode(NodeID id) const {
        NodeToRepMap::const_iterator it = nodeToRepMap.find(id);
        if(it==nodeToRepMap.end())
            return id;
        else
            return it->second;
    }
    inline NodeBS& sccSubNodes(NodeID id) {
        if(0==nodeToSubsMap.count(id))
            nodeToSubsMap[id].set(id);
        return nodeToSubsMap[id];
    }
    inline void setRep(NodeID node, NodeID rep) {
        nodeToRepMap[node] = rep;
    }
    inline void setSubs(NodeID node, NodeBS& subs) {
        nodeToSubsMap[node] |= subs;
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
    inline bool moveEdgesToRepNode(ConstraintNode*node, ConstraintNode* rep ) {
        bool gepIn = moveInEdgesToRepNode(node, rep);
        bool gepOut = moveOutEdgesToRepNode(node, rep);
        return (gepIn || gepOut);
    }

    /// Parameter passing
    void connectCaller2CalleeParams(llvm::CallSite cs, const llvm::Function *F, NodePairSet& cpySrcNodes);

    /// Check if a given edge is a NormalGepCGEdge with 0 offset.
    inline bool isZeroOffsettedGepCGEdge(ConstraintEdge *edge) const {
        if (NormalGepCGEdge *normalGepCGEdge = llvm::dyn_cast<NormalGepCGEdge>(edge))
            if (0 == normalGepCGEdge->getLocationSet().getOffset())
                return true;
        return false;
    }

    /// Wrappers for invoking PAG methods
    //@{
    inline const PAG::CallSiteToFunPtrMap& getIndirectCallsites() const {
        return pag->getIndirectCallsites();
    }
    inline NodeID getBlackHoleNode() {
        return pag->getBlackHoleNode();
    }
    inline bool isBlkObjOrConstantObj(NodeID id) {
        return pag->isBlkObjOrConstantObj(id);
    }
    inline NodeBS& getAllFieldsObjNode(NodeID id) {
        return pag->getAllFieldsObjNode(id);
    }
    inline NodeID getBaseObjNode(NodeID id) {
        return pag->getBaseObjNode(id);
    }
    inline void setObjFieldInsensitive(NodeID id) {
        MemObj* mem =  const_cast<MemObj*>(pag->getBaseObj(id));
        mem->setFieldInsensitive();
    }
    inline bool isFieldInsensitiveObj(NodeID id) const {
        const MemObj* mem =  pag->getBaseObj(id);
        return mem->isFieldInsensitive();
    }
    inline bool isSingleFieldObj(NodeID id) const {
        const MemObj* mem = pag->getBaseObj(id);
        return (mem->getMaxFieldOffsetLimit() == 1);
    }
    /// Get a field of a memory object
    inline NodeID getGepObjNode(NodeID id, const LocationSet& ls) {
        NodeID gep =  pag->getGepObjNode(id,ls);
        /// Create a node when it is (1) not exist on graph and (2) not merged
        if(sccRepNode(gep)==gep && hasConstraintNode(gep)==false)
            addConstraintNode(new ConstraintNode(gep),gep);
        return gep;
    }
    /// Get a field-insensitive node of a memory object
    inline NodeID getFIObjNode(NodeID id) {
        NodeID fi = pag->getFIObjNode(id);
        /// Create a node when it is (1) not exist on graph and (2) not merged
        if (sccRepNode(fi) == fi && hasConstraintNode(fi)==false)
            addConstraintNode(new ConstraintNode(fi),fi);
        return fi;
    }
    //@}

    /// Check/Set PWC (positive weight cycle) flag
    //@{
    inline bool isPWCNode(NodeID nodeId) {
        return getConstraintNode(nodeId)->isPWCNode();
    }
    inline void setPWCNode(NodeID nodeId) {
        getConstraintNode(nodeId)->setPWCNode();
    }
    //@}

    /// Add/get nodes to be collapsed
    //@{
    inline bool hasNodesToBeCollapsed() const {
        return (!nodesToBeCollapsed.empty());
    }
    inline void addNodeToBeCollapsed(NodeID id) {
        nodesToBeCollapsed.push(id);
    }
    inline NodeID getNextCollapseNode() {
        return nodesToBeCollapsed.pop();
    }
    //@}

    /// Dump graph into dot file
    void dump();
    /// Print CG into terminal
    void print();
};


namespace llvm {
/* !
 * GraphTraits specializations for the generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<ConstraintNode*> : public GraphTraits<GenericNode<ConstraintNode,ConstraintEdge>*  > {
};

/// Inverse GraphTraits specializations for Value flow node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<ConstraintNode *> > : public GraphTraits<Inverse<GenericNode<ConstraintNode,ConstraintEdge>* > > {
};

template<> struct GraphTraits<ConstraintGraph*> : public GraphTraits<GenericGraph<ConstraintNode,ConstraintEdge>* > {
    typedef ConstraintNode *NodeRef;
};

}

#endif /* CONSG_H_ */
