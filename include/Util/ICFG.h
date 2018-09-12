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

    friend class SVFGBuilder;
    friend class SaberSVFGBuilder;
    friend class DDASVFGBuilder;
    friend class MTASVFGBuilder;
    friend class RcSvfgBuilder;

public:
    /// SVFG kind
    enum SVFGK {
        ORIGSVFGK,OPTSVFGK
    };
    typedef llvm::DenseMap<NodeID, ICFGNode *> ICFGNodeIDToNodeMapTy;
    typedef llvm::DenseMap<const PAGNode*, NodeID> PAGNodeToDefMapTy;
    typedef llvm::DenseMap<const MRVer*, NodeID> MSSAVarToDefMapTy;
    typedef std::map<std::pair<NodeID,llvm::CallSite>, ActualParmICFGNode *> PAGNodeToActualParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, ActualRetICFGNode *> PAGNodeToActualRetMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalParmICFGNode *> PAGNodeToFormalParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalRetICFGNode *> PAGNodeToFormalRetMapTy;
    typedef NodeBS ActualINICFGNodeSet;
    typedef NodeBS ActualOUTICFGNodeSet;
    typedef NodeBS FormalINICFGNodeSet;
    typedef NodeBS FormalOUTICFGNodeSet;
    typedef std::map<llvm::CallSite, ActualINICFGNodeSet>  CallSiteToActualINsMapTy;
    typedef std::map<llvm::CallSite, ActualOUTICFGNodeSet>  CallSiteToActualOUTsMapTy;
    typedef llvm::DenseMap<const llvm::Function*, FormalINICFGNodeSet>  FunctionToFormalINsMapTy;
    typedef llvm::DenseMap<const llvm::Function*, FormalOUTICFGNodeSet>  FunctionToFormalOUTsMapTy;
    typedef FormalParmICFGNode::CallPESet CallPESet;
    typedef FormalRetICFGNode::RetPESet RetPESet;
    typedef ICFGEdge::ICFGEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGEdge::SVFGEdgeSetTy SVFGEdgeSetTy;
    typedef ICFGEdge::ICFGEdgeSetTy::iterator ICFGNodeIter;
    typedef ICFGNodeIDToNodeMapTy::iterator iterator;
    typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef MemSSA::MUSet MUSet;
    typedef MemSSA::CHISet CHISet;
    typedef MemSSA::PHISet PHISet;
    typedef MemSSA::MU MU;
    typedef MemSSA::CHI CHI;
    typedef MemSSA::LOADMU LOADMU;
    typedef MemSSA::STORECHI STORECHI;
    typedef MemSSA::RETMU RETMU;
    typedef MemSSA::ENTRYCHI ENTRYCHI;
    typedef MemSSA::CALLCHI CALLCHI;
    typedef MemSSA::CALLMU CALLMU;
    typedef PAG::PAGEdgeSet PAGEdgeSet;
    typedef std::set<StoreICFGNode*> StoreNodeSet;
    typedef std::map<const StorePE*,const StoreICFGNode*> StorePEToICFGNodeMap;

protected:
    NodeID totalICFGNode;
    PAGNodeToDefMapTy PAGNodeToDefMap;	///< map a pag node to its definition SVG node
    MSSAVarToDefMapTy MSSAVarToDefMap;	///< map a memory SSA operator to its definition SVFG node
    PAGNodeToActualParmMapTy PAGNodeToActualParmMap;
    PAGNodeToActualRetMapTy PAGNodeToActualRetMap;
    PAGNodeToFormalParmMapTy PAGNodeToFormalParmMap;
    PAGNodeToFormalRetMapTy PAGNodeToFormalRetMap;
    CallSiteToActualINsMapTy callSiteToActualINMap;
    CallSiteToActualOUTsMapTy callSiteToActualOUTMap;
    FunctionToFormalINsMapTy funToFormalINMap;
    FunctionToFormalOUTsMapTy funToFormalOUTMap;
    StoreNodeSet globalStore;	///< set of global store SVFG nodes
    StorePEToICFGNodeMap storePEToICFGNodeMap;	///< map store inst to store ICFGNode
    ICFGStat * stat;
    SVFGK kind;
    MemSSA* mssa;
    PointerAnalysis* pta;

    /// Clean up memory
    void destroy();

    /// Constructor
    ICFG(SVFGK k = ORIGSVFGK);

    /// Start building SVFG
    virtual void buildSVFG(MemSSA* m);

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

    /// Clear MSSA
    inline void clearMSSA() {
        delete mssa;
        mssa = NULL;
    }

    /// Get SVFG kind
    inline SVFGK getKind() const {
        return kind;
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

    /// Get all inter value flow edges of a indirect call site
    void getInterVFEdgesForIndirectCallSite(const llvm::CallSite cs, const llvm::Function* callee, ICFGEdgeSetTy& edges);

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
    inline const ICFGNode* getStoreICFGNode(const StorePE* store) const {
        StorePEToICFGNodeMap::const_iterator it = storePEToICFGNodeMap.find(store);
        assert(it!=storePEToICFGNodeMap.end() && "ICFGNode not found?");
        return it->second;
    }

    // Given a svfg node, return its left hand side top level pointer (PAGnode)
    const PAGNode* getLHSTopLevPtr(const ICFGNode* node) const;

    /// Perform statistics
    void performStat();

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

    /// Has a ICFGNode
    //@{
    inline bool hasActualINICFGNodes(llvm::CallSite cs) const {
        return callSiteToActualINMap.find(cs)!=callSiteToActualINMap.end();
    }

    inline bool hasActualOUTICFGNodes(llvm::CallSite cs) const {
        return callSiteToActualOUTMap.find(cs)!=callSiteToActualOUTMap.end();
    }

    inline bool hasFormalINICFGNodes(const llvm::Function* fun) const {
        return funToFormalINMap.find(fun)!=funToFormalINMap.end();
    }

    inline bool hasFormalOUTICFGNodes(const llvm::Function* fun) const {
        return funToFormalOUTMap.find(fun)!=funToFormalOUTMap.end();
    }
    //@}

    /// Get ICFGNode set
    //@{
    inline ActualINICFGNodeSet& getActualINICFGNodes(llvm::CallSite cs) {
        return callSiteToActualINMap[cs];
    }

    inline ActualOUTICFGNodeSet& getActualOUTICFGNodes(llvm::CallSite cs) {
        return callSiteToActualOUTMap[cs];
    }

    inline FormalINICFGNodeSet& getFormalINICFGNodes(const llvm::Function* fun) {
        return funToFormalINMap[fun];
    }

    inline FormalOUTICFGNodeSet& getFormalOUTICFGNodes(const llvm::Function* fun) {
        return funToFormalOUTMap[fun];
    }
    //@}

    /// Whether a node is function entry ICFGNode
    const llvm::Function* isFunEntryICFGNode(const ICFGNode* node) const;

    /// Whether a node is callsite return ICFGNode
    llvm::Instruction* isCallSiteRetICFGNode(const ICFGNode* node) const;

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

    /// Add direct def-use edges for top level pointers
    //@{
    ICFGEdge* addIntraDirectVFEdge(NodeID srcId, NodeID dstId);
    ICFGEdge* addCallDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    ICFGEdge* addRetDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    //@}

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraVFEdgeParents(ICFGNode *srcNode, ICFGNode *dstNode) {
        const llvm::BasicBlock *srcBB = srcNode->getBB();
        const llvm::BasicBlock *dstBB = dstNode->getBB();
        if(srcBB != nullptr && dstBB != nullptr) {
            assert(srcBB->getParent() == dstBB->getParent());
        }
    }

    /// Add indirect def-use edges of a memory region between two statements,
    //@{
    ICFGEdge* addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts);
    ICFGEdge* addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId);
    ICFGEdge* addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId);
    ICFGEdge* addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts);
    //@}

    /// Add inter VF edge from actual to formal parameters
    inline ICFGEdge* addInterVFEdgeFromAPToFP(const ActualParmICFGNode* src, const FormalParmICFGNode* dst, CallSiteID csId) {
        return addCallDirectVFEdge(src->getId(),dst->getId(),csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline ICFGEdge* addInterVFEdgeFromFRToAR(const FormalRetICFGNode* src, const ActualRetICFGNode* dst, CallSiteID csId) {
        return addRetDirectVFEdge(src->getId(),dst->getId(),csId);
    }

    /// Add inter VF edge from callsite mu to function entry chi
    ICFGEdge* addInterIndirectVFCallEdge(const ActualINICFGNode* src, const FormalINICFGNode* dst,CallSiteID csId);

    /// Add inter VF edge from function exit mu to callsite chi
    ICFGEdge* addInterIndirectVFRetEdge(const FormalOUTICFGNode* src, const ActualOUTICFGNode* dst,CallSiteID csId);

    /// Connect SVFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-param and formal param
    virtual inline void connectAParamAndFParam(const PAGNode* cs_arg, const PAGNode* fun_arg, llvm::CallSite cs, CallSiteID csId, ICFGEdgeSetTy& edges) {
        const ActualParmICFGNode* actualParam = getActualParmICFGNode(cs_arg,cs);
        const FormalParmICFGNode* formalParam = getFormalParmICFGNode(fun_arg);
        ICFGEdge* edge = addInterVFEdgeFromAPToFP(actualParam, formalParam,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect formal-ret and actual ret
    virtual inline void connectFRetAndARet(const PAGNode* fun_return, const PAGNode* cs_return, CallSiteID csId, ICFGEdgeSetTy& edges) {
        const FormalRetICFGNode* formalRet = getFormalRetICFGNode(fun_return);
        const ActualRetICFGNode* actualRet = getActualRetICFGNode(cs_return);
        ICFGEdge* edge = addInterVFEdgeFromFRToAR(formalRet, actualRet,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect actual-in and formal-in
    virtual inline void connectAInAndFIn(const ActualINICFGNode* actualIn, const FormalINICFGNode* formalIn, CallSiteID csId, ICFGEdgeSetTy& edges) {
        ICFGEdge* edge = addInterIndirectVFCallEdge(actualIn, formalIn,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect formal-out and actual-out
    virtual inline void connectFOutAndAOut(const FormalOUTICFGNode* formalOut, const ActualOUTICFGNode* actualOut, CallSiteID csId, ICFGEdgeSetTy& edges) {
        ICFGEdge* edge = addInterIndirectVFRetEdge(formalOut, actualOut,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    //@}

    /// Get inter value flow edges between indirect call site and callee.
    //@{
    virtual inline void getInterVFEdgeAtIndCSFromAPToFP(const PAGNode* cs_arg, const PAGNode* fun_arg, llvm::CallSite cs, CallSiteID csId, ICFGEdgeSetTy& edges) {
        ActualParmICFGNode* actualParam = getActualParmICFGNode(cs_arg,cs);
        FormalParmICFGNode* formalParam = getFormalParmICFGNode(fun_arg);
        ICFGEdge* edge = hasInterICFGEdge(actualParam, formalParam, ICFGEdge::VFDirCall, csId);
        assert(edge != NULL && "Can not find inter value flow edge from aparam to fparam");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromFRToAR(const PAGNode* fun_return, const PAGNode* cs_return, CallSiteID csId, ICFGEdgeSetTy& edges) {
        FormalRetICFGNode* formalRet = getFormalRetICFGNode(fun_return);
        ActualRetICFGNode* actualRet = getActualRetICFGNode(cs_return);
        ICFGEdge* edge = hasInterICFGEdge(formalRet, actualRet, ICFGEdge::VFDirRet, csId);
        assert(edge != NULL && "Can not find inter value flow edge from fret to aret");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromAInToFIn(ActualINICFGNode* actualIn, const llvm::Function* callee, ICFGEdgeSetTy& edges) {
        for (ICFGNode::const_iterator outIt = actualIn->OutEdgeBegin(), outEit = actualIn->OutEdgeEnd(); outIt != outEit; ++outIt) {
            ICFGEdge* edge = *outIt;
            if (edge->getDstNode()->getBB()->getParent() == callee)
                edges.insert(edge);
        }
    }

    virtual inline void getInterVFEdgeAtIndCSFromFOutToAOut(ActualOUTICFGNode* actualOut, const llvm::Function* callee, ICFGEdgeSetTy& edges) {
        for (ICFGNode::const_iterator inIt = actualOut->InEdgeBegin(), inEit = actualOut->InEdgeEnd(); inIt != inEit; ++inIt) {
            ICFGEdge* edge = *inIt;
            if (edge->getSrcNode()->getBB()->getParent() == callee)
                edges.insert(edge);
        }
    }
    //@}

    /// Add SVFG edge
    inline bool addICFGEdge(ICFGEdge* edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Given a PAGNode, set/get its def SVFG node (definition of top level pointers)
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

    /// Given a MSSADef, set/get its def SVFG node (definition of address-taken variables)
    //@{
    inline void setDef(const MRVer* mvar, const ICFGNode* node) {
        MSSAVarToDefMapTy::iterator it = MSSAVarToDefMap.find(mvar);
        if(it==MSSAVarToDefMap.end()) {
            MSSAVarToDefMap[mvar] = node->getId();
            assert(hasICFGNode(node->getId()) && "not in the map!!");
        }
        else {
            assert((it->second == node->getId()) && "a PAG node can only have unique definition ");
        }
    }
    inline NodeID getDef(const MRVer* mvar) const {
        MSSAVarToDefMapTy::const_iterator it = MSSAVarToDefMap.find(mvar);
        assert(it!=MSSAVarToDefMap.end() && "memory SSA does not have a definition??");
        return it->second;
    }
    //@}

    /// Create SVFG nodes for top level pointers
    void addICFGNodesForTopLevelPtrs();
    /// Create SVFG nodes for address-taken variables
    void addICFGNodesForAddrTakenVars();
    /// Connect direct SVFG edges between two SVFG nodes (value-flow of top level pointers)
    void connectDirectICFGEdges();
    /// Connect direct SVFG edges between two SVFG nodes (value-flow of top address-taken variables)
    void connectIndirectICFGEdges();
    /// Connect indirect SVFG edges from global initializers (store) to main function entry
    void connectFromGlobalToProgEntry();

    inline bool isPhiCopyEdge(const CopyPE* copy) const {
        return pta->getPAG()->isPhiNode(copy->getDstNode());
    }

    inline bool isPhiCopyNode(const PAGNode* node) const {
        return pta->getPAG()->isPhiNode(node);
    }

    /// Add SVFG node
    virtual inline void addICFGNode(ICFGNode* node) {
        addGNode(node->getId(),node);
    }
    /// Add SVFG node for program statement
    inline void addStmtICFGNode(StmtICFGNode* node) {
        addICFGNode(node);
    }
    /// Add Dummy SVFG node for null pointer definition
    /// To be noted for black hole pointer it has already has address edge connected
    inline void addNullPtrICFGNode(const PAGNode* pagNode) {
        NullPtrICFGNode* sNode = new NullPtrICFGNode(totalICFGNode++,pagNode);
        addICFGNode(sNode);
        setDef(pagNode,sNode);
    }
    /// Add Address SVFG node
    inline void addAddrICFGNode(const AddrPE* addr) {
        AddrICFGNode* sNode = new AddrICFGNode(totalICFGNode++,addr);
        addStmtICFGNode(sNode);
        setDef(addr->getDstNode(),sNode);
    }
    /// Add Copy SVFG node
    inline void addCopyICFGNode(const CopyPE* copy) {
        CopyICFGNode* sNode = new CopyICFGNode(totalICFGNode++,copy);
        addStmtICFGNode(sNode);
        setDef(copy->getDstNode(),sNode);
    }
    /// Add Gep SVFG node
    inline void addGepICFGNode(const GepPE* gep) {
        GepICFGNode* sNode = new GepICFGNode(totalICFGNode++,gep);
        addStmtICFGNode(sNode);
        setDef(gep->getDstNode(),sNode);
    }
    /// Add Load SVFG node
    void addLoadICFGNode(LoadPE* load) {
        LoadICFGNode* sNode = new LoadICFGNode(totalICFGNode++,load);
        addStmtICFGNode(sNode);
        setDef(load->getDstNode(),sNode);
    }
    /// Add Store SVFG node,
    /// To be noted store does not create a new pointer, we do not set def for any PAG node
    void addStoreICFGNode(StorePE* store) {
        StoreICFGNode* sNode = new StoreICFGNode(totalICFGNode++,store);
        assert(storePEToICFGNodeMap.find(store)==storePEToICFGNodeMap.end() && "should not insert twice!");
        storePEToICFGNodeMap[store] = sNode;
        addStmtICFGNode(sNode);
        for(CHISet::iterator pi = mssa->getCHISet(store).begin(), epi = mssa->getCHISet(store).end(); pi!=epi; ++pi) {
            setDef((*pi)->getResVer(),sNode);
        }

        const PAGEdgeSet& globalPAGStores = getPAG()->getGlobalPAGEdgeSet();
        if (globalPAGStores.find(store) != globalPAGStores.end())
            globalStore.insert(sNode);
    }
    /// Add actual parameter SVFG node
    /// To be noted that multiple actual parameters may have same value (PAGNode)
    /// So we need to make a pair <PAGNodeID,CallSiteID> to find the right SVFGParmNode
    inline void addActualParmICFGNode(const PAGNode* aparm, llvm::CallSite cs) {
        ActualParmICFGNode* sNode = new ActualParmICFGNode(totalICFGNode++,aparm,cs);
        addICFGNode(sNode);
        PAGNodeToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add formal parameter SVFG node
    inline void addFormalParmICFGNode(const PAGNode* fparm, const llvm::Function* fun, CallPESet& callPEs) {
        FormalParmICFGNode* sNode = new FormalParmICFGNode(totalICFGNode++,fparm,fun);
        addICFGNode(sNode);
        for(CallPESet::const_iterator it = callPEs.begin(), eit=callPEs.end();
                it!=eit; ++it)
            sNode->addCallPE(*it);

        setDef(fparm,sNode);
        PAGNodeToFormalParmMap[fparm] = sNode;
    }
    /// Add callee Return SVFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <PAGNodeID,CallSiteID> pair to find FormalRetSVFG node same as handling actual parameters
    inline void addFormalRetICFGNode(const PAGNode* ret, const llvm::Function* fun, RetPESet& retPEs) {
        FormalRetICFGNode* sNode = new FormalRetICFGNode(totalICFGNode++,ret,fun);
        addICFGNode(sNode);
        for(RetPESet::const_iterator it = retPEs.begin(), eit=retPEs.end();
                it!=eit; ++it)
            sNode->addRetPE(*it);

        PAGNodeToFormalRetMap[ret] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add callsite Receive SVFG node
    inline void addActualRetICFGNode(const PAGNode* ret,llvm::CallSite cs) {
        ActualRetICFGNode* sNode = new ActualRetICFGNode(totalICFGNode++,ret,cs);
        addICFGNode(sNode);
        setDef(ret,sNode);
        PAGNodeToActualRetMap[ret] = sNode;
    }
    /// Add llvm PHI SVFG node
    inline void addIntraPHIICFGNode(const PAGNode* phiResNode, PAG::PNodeBBPairList& oplist) {
        IntraPHIICFGNode* sNode = new IntraPHIICFGNode(totalICFGNode++,phiResNode);
        addICFGNode(sNode);
        u32_t pos = 0;
        for(PAG::PNodeBBPairList::const_iterator it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
            sNode->setOpVerAndBB(pos,it->first,it->second);
        setDef(phiResNode,sNode);
    }
    /// Add memory Function entry chi SVFG node
    inline void addFormalINICFGNode(const MemSSA::ENTRYCHI* chi) {
        FormalINICFGNode* sNode = new FormalINICFGNode(totalICFGNode++,chi);
        addICFGNode(sNode);
        setDef(chi->getResVer(),sNode);
        funToFormalINMap[chi->getFunction()].set(sNode->getId());
    }
    /// Add memory Function return mu SVFG node
    inline void addFormalOUTICFGNode(const MemSSA::RETMU* mu) {
        FormalOUTICFGNode* sNode = new FormalOUTICFGNode(totalICFGNode++,mu);
        addICFGNode(sNode);
        funToFormalOUTMap[mu->getFunction()].set(sNode->getId());
    }
    /// Add memory callsite mu SVFG node
    inline void addActualINICFGNode(const MemSSA::CALLMU* mu) {
        ActualINICFGNode* sNode = new ActualINICFGNode(totalICFGNode++,mu, mu->getCallSite());
        addICFGNode(sNode);
        callSiteToActualINMap[mu->getCallSite()].set(sNode->getId());
    }
    /// Add memory callsite chi SVFG node
    inline void addActualOUTICFGNode(const MemSSA::CALLCHI* chi) {
        ActualOUTICFGNode* sNode = new ActualOUTICFGNode(totalICFGNode++,chi,chi->getCallSite());
        addICFGNode(sNode);
        setDef(chi->getResVer(),sNode);
        callSiteToActualOUTMap[chi->getCallSite()].set(sNode->getId());
    }
    /// Add memory SSA PHI SVFG node
    inline void addIntraMSSAPHIICFGNode(const MemSSA::PHI* phi) {
        IntraMSSAPHIICFGNode* sNode = new IntraMSSAPHIICFGNode(totalICFGNode++,phi);
        addICFGNode(sNode);
        for(MemSSA::PHI::OPVers::const_iterator it = phi->opVerBegin(), eit=phi->opVerEnd(); it!=eit; ++it)
            sNode->setOpVer(it->first,it->second);
        setDef(phi->getResVer(),sNode);
    }

    /// Has function for EntryCHI/RetMU/CallCHI/CallMU
    //@{
    inline bool hasFuncEntryChi(const llvm::Function * func) const {
        return (funToFormalINMap.find(func) != funToFormalINMap.end());
    }
    inline bool hasFuncRetMu(const llvm::Function * func) const {
        return (funToFormalOUTMap.find(func) != funToFormalOUTMap.end());
    }
    inline bool hasCallSiteChi(llvm::CallSite cs) const {
        return (callSiteToActualOUTMap.find(cs) != callSiteToActualOUTMap.end());
    }
    inline bool hasCallSiteMu(llvm::CallSite cs) const {
        return (callSiteToActualINMap.find(cs) != callSiteToActualINMap.end());
    }
    //@}

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
