//===- ICFG.h ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFG.h
 *
 *  Created on: 11 Sep. 2018
 *      Author: Yulei
 */

#ifndef INCLUDE_UTIL_ICFG_H_
#define INCLUDE_UTIL_ICFG_H_

#include "Util/ICFGNode.h"
#include "Util/ICFGEdge.h"

class PointerAnalysis;
class ICFGStat;

/*!
 * Interprocedural Control-Flow Graph (ICFG)
 */
typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
class ICFG : public GenericICFGTy {

public:

    typedef llvm::DenseMap<NodeID, ICFGNode *> ICFGNodeIDToNodeMapTy;
    typedef llvm::DenseMap<const PAGNode*, NodeID> PAGNodeToDefMapTy;
    typedef std::map<std::pair<NodeID,llvm::CallSite>, ActualParmICFGNode *> PAGNodeToActualParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, ActualRetICFGNode *> PAGNodeToActualRetMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalParmICFGNode *> PAGNodeToFormalParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalRetICFGNode *> PAGNodeToFormalRetMapTy;
    typedef FormalParmICFGNode::CallPESet CallPESet;
    typedef FormalRetICFGNode::RetPESet RetPESet;
    typedef ICFGEdge::ICFGEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGEdge::SVFGEdgeSetTy SVFGEdgeSetTy;
    typedef ICFGEdge::ICFGEdgeSetTy::iterator ICFGNodeIter;
    typedef ICFGNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef PAG::PAGEdgeSet PAGEdgeSet;
    typedef std::set<StoreICFGNode*> StoreNodeSet;
    typedef std::map<const StorePE*,const StoreICFGNode*> StorePEToICFGNodeMap;

protected:
    NodeID totalICFGNode;
    PAGNodeToDefMapTy PAGNodeToDefMap;	///< map a pag node to its definition SVG node
    PAGNodeToActualParmMapTy PAGNodeToActualParmMap;
    PAGNodeToActualRetMapTy PAGNodeToActualRetMap;
    PAGNodeToFormalParmMapTy PAGNodeToFormalParmMap;
    PAGNodeToFormalRetMapTy PAGNodeToFormalRetMap;
    StoreNodeSet globalStore;	///< set of global store SVFG nodes
    StorePEToICFGNodeMap storePEToICFGNodeMap;	///< map store inst to store ICFGNode
    ICFGStat * stat;
    PointerAnalysis* pta;

    /// Clean up memory
    void destroy();

    /// Constructor
    ICFG();

    /// Start building SVFG
    virtual void buildICFG(PointerAnalysis* pta);

public:
    /// Destructor
    virtual ~ICFG() {
        destroy();
    }

    /// Return statistics
    inline ICFGStat* getStat() const {
        return stat;
    }

    /// Return PAG
    inline PAG* getPAG() {
        return PAG::getPAG();
    }

    /// Get a SVFG node
    inline ICFGNode* getICFGNode(NodeID id) const {
        return getGNode(id);
    }

    /// Whether has the ICFGNode
    inline bool hasICFGNode(NodeID id) const {
        return hasGNode(id);
    }

    /// Whether we has a SVFG edge
    //@{
    ICFGEdge* hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    ICFGEdge* hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind, CallSiteID csId);
    ICFGEdge* hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
    //@}

    /// Get a SVFG edge according to src and dst
    ICFGEdge* getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Connect SVFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(llvm::CallSite cs, const llvm::Function* callee, ICFGEdgeSetTy& edges);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(llvm::CallSite cs, const llvm::Function* func) const {
        return pta->getPTACallGraph()->getCallSiteID(cs, func);
    }
    inline llvm::CallSite getCallSite(CallSiteID id) const {
        return pta->getPTACallGraph()->getCallSite(id);
    }
    //@}

    /// Given a pagNode, return its definition site
    inline const ICFGNode* getDefICFGNode(const PAGNode* pagNode) const {
        return getICFGNode(getDef(pagNode));
    }

    /// Given a store pagEdge, return its ICFGNode
    inline const StoreICFGNode* getStoreICFGNode(const StorePE* store) const {
        StorePEToICFGNodeMap::const_iterator it = storePEToICFGNodeMap.find(store);
        assert(it!=storePEToICFGNodeMap.end() && "ICFGNode not found?");
        return it->second;
    }

    // Given a svfg node, return its left hand side top level pointer (PAGnode)
    const PAGNode* getLHSTopLevPtr(const ICFGNode* node) const;

    /// Get a ICFGNode
    //@{
    inline ActualParmICFGNode* getActualParmICFGNode(const PAGNode* aparm,llvm::CallSite cs) const {
        PAGNodeToActualParmMapTy::const_iterator it = PAGNodeToActualParmMap.find(std::make_pair(aparm->getId(),cs));
        assert(it!=PAGNodeToActualParmMap.end() && "acutal parameter SVFG node can not be found??");
        return it->second;
    }

    inline ActualRetICFGNode* getActualRetICFGNode(const PAGNode* aret) const {
        PAGNodeToActualRetMapTy::const_iterator it = PAGNodeToActualRetMap.find(aret);
        assert(it!=PAGNodeToActualRetMap.end() && "actual return SVFG node can not be found??");
        return it->second;
    }

    inline FormalParmICFGNode* getFormalParmICFGNode(const PAGNode* fparm) const {
        PAGNodeToFormalParmMapTy::const_iterator it = PAGNodeToFormalParmMap.find(fparm);
        assert(it!=PAGNodeToFormalParmMap.end() && "formal parameter SVFG node can not be found??");
        return it->second;
    }

    inline FormalRetICFGNode* getFormalRetICFGNode(const PAGNode* fret) const {
        PAGNodeToFormalRetMapTy::const_iterator it = PAGNodeToFormalRetMap.find(fret);
        assert(it!=PAGNodeToFormalRetMap.end() && "formal return SVFG node can not be found??");
        return it->second;
    }
    //@}

    /// Whether a node is function entry ICFGNode
    const llvm::Function* isFunEntryICFGNode(const ICFGNode* node) const;

protected:
    /// Remove a SVFG edge
    inline void removeICFGEdge(ICFGEdge* edge) {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a ICFGNode
    inline void removeICFGNode(ICFGNode* node) {
        removeGNode(node);
    }

    /// Add interprocedural control-flow edges for top level pointers
    //@{
    ICFGEdge* addCallEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    ICFGEdge* addRetEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    //@}

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(ICFGNode *srcNode, ICFGNode *dstNode) {
        const llvm::BasicBlock *srcBB = srcNode->getBB();
        const llvm::BasicBlock *dstBB = dstNode->getBB();
        if(srcBB != nullptr && dstBB != nullptr) {
            assert(srcBB->getParent() == dstBB->getParent());
        }
    }

    /// Add inter VF edge from actual to formal parameters
    inline ICFGEdge* addInterEdgeFromAPToFP(const ActualParmICFGNode* src, const FormalParmICFGNode* dst, CallSiteID csId) {
        return addCallEdge(src->getId(),dst->getId(),csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline ICFGEdge* addInterEdgeFromFRToAR(const FormalRetICFGNode* src, const ActualRetICFGNode* dst, CallSiteID csId) {
        return addRetEdge(src->getId(),dst->getId(),csId);
    }

    /// Connect SVFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-param and formal param
    virtual inline void connectAParamAndFParam(const PAGNode* cs_arg, const PAGNode* fun_arg, llvm::CallSite cs, CallSiteID csId, ICFGEdgeSetTy& edges) {
        const ActualParmICFGNode* actualParam = getActualParmICFGNode(cs_arg,cs);
        const FormalParmICFGNode* formalParam = getFormalParmICFGNode(fun_arg);
        ICFGEdge* edge = addInterEdgeFromAPToFP(actualParam, formalParam,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect formal-ret and actual ret
    virtual inline void connectFRetAndARet(const PAGNode* fun_return, const PAGNode* cs_return, CallSiteID csId, ICFGEdgeSetTy& edges) {
        const FormalRetICFGNode* formalRet = getFormalRetICFGNode(fun_return);
        const ActualRetICFGNode* actualRet = getActualRetICFGNode(cs_return);
        ICFGEdge* edge = addInterEdgeFromFRToAR(formalRet, actualRet,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    //@}

    /// Add ICFG edge
    inline bool addICFGEdge(ICFGEdge* edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Given a PAGNode, set/get its def ICFG node (definition of top level pointers)
    //@{
    inline void setDef(const PAGNode* pagNode, const ICFGNode* node) {
        PAGNodeToDefMapTy::iterator it = PAGNodeToDefMap.find(pagNode);
        if(it == PAGNodeToDefMap.end()) {
            PAGNodeToDefMap[pagNode] = node->getId();
            assert(hasICFGNode(node->getId()) && "not in the map!!");
        }
        else {
            assert((it->second == node->getId()) && "a PAG node can only have unique definition ");
        }
    }
    inline NodeID getDef(const PAGNode* pagNode) const {
        PAGNodeToDefMapTy::const_iterator it = PAGNodeToDefMap.find(pagNode);
        assert(it!=PAGNodeToDefMap.end() && "PAG node does not have a definition??");
        return it->second;
    }
    inline bool hasDef(const PAGNode* pagNode) const {
        return (PAGNodeToDefMap.find(pagNode) != PAGNodeToDefMap.end());
    }
    //@}

    /// Create ICFG nodes for top level pointers
    void addICFGNodesForTopLevelPtrs();


    inline bool isPhiCopyEdge(const CopyPE* copy) const {
        return pta->getPAG()->isPhiNode(copy->getDstNode());
    }

    inline bool isPhiCopyNode(const PAGNode* node) const {
        return pta->getPAG()->isPhiNode(node);
    }

    /// Add ICFG node
    virtual inline void addICFGNode(ICFGNode* node) {
        addGNode(node->getId(),node);
    }
    /// Add ICFG node for program statement
    inline void addStmtICFGNode(StmtICFGNode* node) {
        addICFGNode(node);
    }
    /// Add Dummy ICFG node for null pointer definition
    /// To be noted for black hole pointer it has already has address edge connected
    inline void addNullPtrICFGNode(const PAGNode* pagNode) {
        NullPtrICFGNode* sNode = new NullPtrICFGNode(totalICFGNode++,pagNode);
        addICFGNode(sNode);
        setDef(pagNode,sNode);
    }
    /// Add Address ICFG node
    inline void addAddrICFGNode(const AddrPE* addr) {
        AddrICFGNode* sNode = new AddrICFGNode(totalICFGNode++,addr);
        addStmtICFGNode(sNode);
        setDef(addr->getDstNode(),sNode);
    }
    /// Add Copy ICFG node
    inline void addCopyICFGNode(const CopyPE* copy) {
        CopyICFGNode* sNode = new CopyICFGNode(totalICFGNode++,copy);
        addStmtICFGNode(sNode);
        setDef(copy->getDstNode(),sNode);
    }
    /// Add Gep ICFG node
    inline void addGepICFGNode(const GepPE* gep) {
        GepICFGNode* sNode = new GepICFGNode(totalICFGNode++,gep);
        addStmtICFGNode(sNode);
        setDef(gep->getDstNode(),sNode);
    }
    /// Add Load ICFG node
    void addLoadICFGNode(LoadPE* load) {
        LoadICFGNode* sNode = new LoadICFGNode(totalICFGNode++,load);
        addStmtICFGNode(sNode);
        setDef(load->getDstNode(),sNode);
    }
    /// Add Store ICFG node,
    /// To be noted store does not create a new pointer, we do not set def for any PAG node
    void addStoreICFGNode(StorePE* store) {
        StoreICFGNode* sNode = new StoreICFGNode(totalICFGNode++,store);
        assert(storePEToICFGNodeMap.find(store)==storePEToICFGNodeMap.end() && "should not insert twice!");
        storePEToICFGNodeMap[store] = sNode;
        addStmtICFGNode(sNode);

        const PAGEdgeSet& globalPAGStores = getPAG()->getGlobalPAGEdgeSet();
        if (globalPAGStores.find(store) != globalPAGStores.end())
            globalStore.insert(sNode);
    }
    /// Add actual parameter ICFG node
    /// To be noted that multiple actual parameters may have same value (PAGNode)
    /// So we need to make a pair <PAGNodeID,CallSiteID> to find the right ICFGParmNode
    inline void addActualParmICFGNode(const PAGNode* aparm, llvm::CallSite cs) {
        ActualParmICFGNode* sNode = new ActualParmICFGNode(totalICFGNode++,aparm,cs);
        addICFGNode(sNode);
        PAGNodeToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add formal parameter ICFG node
    inline void addFormalParmICFGNode(const PAGNode* fparm, const llvm::Function* fun, CallPESet& callPEs) {
        FormalParmICFGNode* sNode = new FormalParmICFGNode(totalICFGNode++,fparm,fun);
        addICFGNode(sNode);
        for(CallPESet::const_iterator it = callPEs.begin(), eit=callPEs.end();
                it!=eit; ++it)
            sNode->addCallPE(*it);

        setDef(fparm,sNode);
        PAGNodeToFormalParmMap[fparm] = sNode;
    }
    /// Add callee Return ICFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <PAGNodeID,CallSiteID> pair to find FormalRetICFG node same as handling actual parameters
    inline void addFormalRetICFGNode(const PAGNode* ret, const llvm::Function* fun, RetPESet& retPEs) {
        FormalRetICFGNode* sNode = new FormalRetICFGNode(totalICFGNode++,ret,fun);
        addICFGNode(sNode);
        for(RetPESet::const_iterator it = retPEs.begin(), eit=retPEs.end();
                it!=eit; ++it)
            sNode->addRetPE(*it);

        PAGNodeToFormalRetMap[ret] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add callsite Receive ICFG node
    inline void addActualRetICFGNode(const PAGNode* ret,llvm::CallSite cs) {
        ActualRetICFGNode* sNode = new ActualRetICFGNode(totalICFGNode++,ret,cs);
        addICFGNode(sNode);
        setDef(ret,sNode);
        PAGNodeToActualRetMap[ret] = sNode;
    }
    /// Add llvm PHI ICFG node
    inline void addIntraPHIICFGNode(const PAGNode* phiResNode, PAG::PNodeBBPairList& oplist) {
        IntraPHIICFGNode* sNode = new IntraPHIICFGNode(totalICFGNode++,phiResNode);
        addICFGNode(sNode);
        u32_t pos = 0;
        for(PAG::PNodeBBPairList::const_iterator it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
            sNode->setOpVerAndBB(pos,it->first,it->second);
        setDef(phiResNode,sNode);
    }

    /// Whether a PAGNode has a blackhole or const object as its definition
    inline bool hasBlackHoleConstObjAddrAsDef(const PAGNode* pagNode) const {
        if (hasDef(pagNode)) {
            const ICFGNode* defNode = getICFGNode(getDef(pagNode));
            if (const AddrICFGNode* addr = llvm::dyn_cast<AddrICFGNode>(defNode)) {
                if (PAG::getPAG()->isBlkObjOrConstantObj(addr->getPAGEdge()->getSrcID()))
                    return true;
            }
            else if(const CopyICFGNode* copy = llvm::dyn_cast<CopyICFGNode>(defNode)) {
                if (PAG::getPAG()->isNullPtr(copy->getPAGEdge()->getSrcID()))
                    return true;
            }
        }
        return false;
    }
};


namespace llvm {
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<ICFGNode*> : public GraphTraits<GenericNode<ICFGNode,ICFGEdge>*  > {
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<ICFGNode *> > : public GraphTraits<Inverse<GenericNode<ICFGNode,ICFGEdge>* > > {
};

template<> struct GraphTraits<ICFG*> : public GraphTraits<GenericGraph<ICFGNode,ICFGEdge>* > {
    typedef ICFGNode *NodeRef;
};


}

#endif /* INCLUDE_UTIL_ICFG_H_ */
