//===- SVFG.h -- Sparse value-flow graph--------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * SVFG.h
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#ifndef SVFG_H_
#define SVFG_H_

#include "MSSA/SVFGNode.h"
#include "MSSA/SVFGEdge.h"

class PointerAnalysis;
class SVFGStat;

/*!
 * Sparse value flow graph
 * Each node stands for a definition, each edge stands for value flow relations
 */
typedef GenericGraph<SVFGNode,SVFGEdge> GenericSVFGGraphTy;
class SVFG : public GenericSVFGGraphTy {
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
    typedef llvm::DenseMap<NodeID, SVFGNode *> SVFGNodeIDToNodeMapTy;
    typedef llvm::DenseMap<const PAGNode*, NodeID> PAGNodeToDefMapTy;
    typedef llvm::DenseMap<const MRVer*, NodeID> MSSAVarToDefMapTy;
    typedef std::map<std::pair<NodeID,llvm::CallSite>, ActualParmSVFGNode *> PAGNodeToActualParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, ActualRetSVFGNode *> PAGNodeToActualRetMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalParmSVFGNode *> PAGNodeToFormalParmMapTy;
    typedef llvm::DenseMap<const PAGNode*, FormalRetSVFGNode *> PAGNodeToFormalRetMapTy;
    typedef NodeBS ActualINSVFGNodeSet;
    typedef NodeBS ActualOUTSVFGNodeSet;
    typedef NodeBS FormalINSVFGNodeSet;
    typedef NodeBS FormalOUTSVFGNodeSet;
    typedef std::map<llvm::CallSite, ActualINSVFGNodeSet>  CallSiteToActualINsMapTy;
    typedef std::map<llvm::CallSite, ActualOUTSVFGNodeSet>  CallSiteToActualOUTsMapTy;
    typedef llvm::DenseMap<const llvm::Function*, FormalINSVFGNodeSet>  FunctionToFormalINsMapTy;
    typedef llvm::DenseMap<const llvm::Function*, FormalOUTSVFGNodeSet>  FunctionToFormalOUTsMapTy;
    typedef FormalParmSVFGNode::CallPESet CallPESet;
    typedef FormalRetSVFGNode::RetPESet RetPESet;
    typedef SVFGEdge::SVFGEdgeSetTy SVFGEdgeSetTy;
    typedef SVFGEdge::SVFGEdgeSetTy::iterator SVFGNodeIter;
    typedef SVFGNodeIDToNodeMapTy::iterator iterator;
    typedef SVFGNodeIDToNodeMapTy::const_iterator const_iterator;
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
    typedef std::set<StoreSVFGNode*> StoreNodeSet;

protected:
    NodeID totalSVFGNode;
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
    SVFGStat * stat;
    SVFGK kind;
    MemSSA* mssa;
    PTACallGraph* ptaCallGraph;

    /// Clean up memory
    void destroy();

    /// Constructor
    SVFG(PTACallGraph* cg, SVFGK k = ORIGSVFGK);

    /// Start building SVFG
    virtual void buildSVFG(MemSSA* m);

public:
    /// Destructor
    virtual ~SVFG() {
        destroy();
    }

    /// Return statistics
    inline SVFGStat* getStat() const {
        return stat;
    }

    /// Return PAG
    inline PAG* getPAG() {
        return PAG::getPAG();
    }

    /// Clear MSSA
    inline void clearMSSA() {
        mssa = NULL;
    }

    /// Get SVFG kind
    inline SVFGK getKind() const {
        return kind;
    }

    /// Get a SVFG node
    inline SVFGNode* getSVFGNode(NodeID id) const {
        return getGNode(id);
    }

    /// Whether has the SVFGNode
    inline bool hasSVFGNode(NodeID id) const {
        return hasGNode(id);
    }

    /// Whether we has a SVFG edge
    //@{
    SVFGEdge* hasIntraSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind);
    SVFGEdge* hasInterSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind, CallSiteID csId);
    SVFGEdge* hasThreadSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind);
    //@}

    /// Get a SVFG edge according to src and dst
    SVFGEdge* getSVFGEdge(const SVFGNode* src, const SVFGNode* dst, SVFGEdge::SVFGEdgeK kind);

    /// Get all inter value flow edges of a indirect call site
    void getInterVFEdgesForIndirectCallSite(const llvm::CallSite cs, const llvm::Function* callee, SVFGEdgeSetTy& edges);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Connect SVFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(llvm::CallSite cs, const llvm::Function* callee, SVFGEdgeSetTy& edges);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(llvm::CallSite cs, const llvm::Function* func) const {
        return getPTACallGraph()->getCallSiteID(cs, func);
    }
    inline llvm::CallSite getCallSite(CallSiteID id) const {
        return getPTACallGraph()->getCallSite(id);
    }
    //@}

    /// Given a pagNode, return its definition site
    inline const SVFGNode* getDefSVFGNode(const PAGNode* pagNode) const {
        return getSVFGNode(getDef(pagNode));
    }

    // Given a svfg node, return its left hand side top level pointer (PAGnode)
    const PAGNode* getLHSTopLevPtr(const SVFGNode* node) const;

    /// Perform statistics
    void performStat();

    /// Get a SVFGNode
    //@{
    inline ActualParmSVFGNode* getActualParmSVFGNode(const PAGNode* aparm,llvm::CallSite cs) const {
        PAGNodeToActualParmMapTy::const_iterator it = PAGNodeToActualParmMap.find(std::make_pair(aparm->getId(),cs));
        assert(it!=PAGNodeToActualParmMap.end() && "acutal parameter SVFG node can not be found??");
        return it->second;
    }

    inline ActualRetSVFGNode* getActualRetSVFGNode(const PAGNode* aret) const {
        PAGNodeToActualRetMapTy::const_iterator it = PAGNodeToActualRetMap.find(aret);
        assert(it!=PAGNodeToActualRetMap.end() && "actual return SVFG node can not be found??");
        return it->second;
    }

    inline FormalParmSVFGNode* getFormalParmSVFGNode(const PAGNode* fparm) const {
        PAGNodeToFormalParmMapTy::const_iterator it = PAGNodeToFormalParmMap.find(fparm);
        assert(it!=PAGNodeToFormalParmMap.end() && "formal parameter SVFG node can not be found??");
        return it->second;
    }

    inline FormalRetSVFGNode* getFormalRetSVFGNode(const PAGNode* fret) const {
        PAGNodeToFormalRetMapTy::const_iterator it = PAGNodeToFormalRetMap.find(fret);
        assert(it!=PAGNodeToFormalRetMap.end() && "formal return SVFG node can not be found??");
        return it->second;
    }
    //@}

    /// Has a SVFGNode
    //@{
    inline bool hasActualINSVFGNodes(llvm::CallSite cs) const {
        return callSiteToActualINMap.find(cs)!=callSiteToActualINMap.end();
    }

    inline bool hasActualOUTSVFGNodes(llvm::CallSite cs) const {
        return callSiteToActualOUTMap.find(cs)!=callSiteToActualOUTMap.end();
    }

    inline bool hasFormalINSVFGNodes(const llvm::Function* fun) const {
        return funToFormalINMap.find(fun)!=funToFormalINMap.end();
    }

    inline bool hasFormalOUTSVFGNodes(const llvm::Function* fun) const {
        return funToFormalOUTMap.find(fun)!=funToFormalOUTMap.end();
    }
    //@}

    /// Get SVFGNode set
    //@{
    inline ActualINSVFGNodeSet& getActualINSVFGNodes(llvm::CallSite cs) {
        return callSiteToActualINMap[cs];
    }

    inline ActualOUTSVFGNodeSet& getActualOUTSVFGNodes(llvm::CallSite cs) {
        return callSiteToActualOUTMap[cs];
    }

    inline FormalINSVFGNodeSet& getFormalINSVFGNodes(const llvm::Function* fun) {
        return funToFormalINMap[fun];
    }

    inline FormalOUTSVFGNodeSet& getFormalOUTSVFGNodes(const llvm::Function* fun) {
        return funToFormalOUTMap[fun];
    }
    //@}

    /// Whether a node is function entry SVFGNode
    const llvm::Function* isFunEntrySVFGNode(const SVFGNode* node) const;

    /// Whether a node is callsite return SVFGNode
    llvm::Instruction* isCallSiteRetSVFGNode(const SVFGNode* node) const;

protected:
    ///Return PTA callgraph
    inline PTACallGraph* getPTACallGraph() const {
        return ptaCallGraph;
    }

    /// Remove a SVFG edge
    inline void removeSVFGEdge(SVFGEdge* edge) {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a SVFGNode
    inline void removeSVFGNode(SVFGNode* node) {
        removeGNode(node);
    }

    /// Add direct def-use edges for top level pointers
    //@{
    SVFGEdge* addIntraDirectVFEdge(NodeID srcId, NodeID dstId);
    SVFGEdge* addCallDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    SVFGEdge* addRetDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    //@}

    /// Add indirect def-use edges of a memory region between two statements,
    //@{
    SVFGEdge* addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts);
    SVFGEdge* addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId);
    SVFGEdge* addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId);
    SVFGEdge* addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts);
    //@}

    /// Add inter VF edge from actual to formal parameters
    inline SVFGEdge* addInterVFEdgeFromAPToFP(const ActualParmSVFGNode* src, const FormalParmSVFGNode* dst, CallSiteID csId) {
        return addCallDirectVFEdge(src->getId(),dst->getId(),csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline SVFGEdge* addInterVFEdgeFromFRToAR(const FormalRetSVFGNode* src, const ActualRetSVFGNode* dst, CallSiteID csId) {
        return addRetDirectVFEdge(src->getId(),dst->getId(),csId);
    }

    /// Add inter VF edge from callsite mu to function entry chi
    SVFGEdge* addInterIndirectVFCallEdge(const ActualINSVFGNode* src, const FormalINSVFGNode* dst,CallSiteID csId);

    /// Add inter VF edge from function exit mu to callsite chi
    SVFGEdge* addInterIndirectVFRetEdge(const FormalOUTSVFGNode* src, const ActualOUTSVFGNode* dst,CallSiteID csId);

    /// Connect SVFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-param and formal param
    virtual inline void connectAParamAndFParam(const PAGNode* cs_arg, const PAGNode* fun_arg, llvm::CallSite cs, CallSiteID csId, SVFGEdgeSetTy& edges) {
        const ActualParmSVFGNode* actualParam = getActualParmSVFGNode(cs_arg,cs);
        const FormalParmSVFGNode* formalParam = getFormalParmSVFGNode(fun_arg);
        SVFGEdge* edge = addInterVFEdgeFromAPToFP(actualParam, formalParam,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect formal-ret and actual ret
    virtual inline void connectFRetAndARet(const PAGNode* fun_return, const PAGNode* cs_return, CallSiteID csId, SVFGEdgeSetTy& edges) {
        const FormalRetSVFGNode* formalRet = getFormalRetSVFGNode(fun_return);
        const ActualRetSVFGNode* actualRet = getActualRetSVFGNode(cs_return);
        SVFGEdge* edge = addInterVFEdgeFromFRToAR(formalRet, actualRet,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect actual-in and formal-in
    virtual inline void connectAInAndFIn(const ActualINSVFGNode* actualIn, const FormalINSVFGNode* formalIn, CallSiteID csId, SVFGEdgeSetTy& edges) {
        SVFGEdge* edge = addInterIndirectVFCallEdge(actualIn, formalIn,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    /// Connect formal-out and actual-out
    virtual inline void connectFOutAndAOut(const FormalOUTSVFGNode* formalOut, const ActualOUTSVFGNode* actualOut, CallSiteID csId, SVFGEdgeSetTy& edges) {
        SVFGEdge* edge = addInterIndirectVFRetEdge(formalOut, actualOut,csId);
        if (edge != NULL)
            edges.insert(edge);
    }
    //@}

    /// Get inter value flow edges between indirect call site and callee.
    //@{
    virtual inline void getInterVFEdgeAtIndCSFromAPToFP(const PAGNode* cs_arg, const PAGNode* fun_arg, llvm::CallSite cs, CallSiteID csId, SVFGEdgeSetTy& edges) {
        ActualParmSVFGNode* actualParam = getActualParmSVFGNode(cs_arg,cs);
        FormalParmSVFGNode* formalParam = getFormalParmSVFGNode(fun_arg);
        SVFGEdge* edge = hasInterSVFGEdge(actualParam, formalParam, SVFGEdge::DirCall, csId);
        assert(edge != NULL && "Can not find inter value flow edge from aparam to fparam");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromFRToAR(const PAGNode* fun_return, const PAGNode* cs_return, CallSiteID csId, SVFGEdgeSetTy& edges) {
        FormalRetSVFGNode* formalRet = getFormalRetSVFGNode(fun_return);
        ActualRetSVFGNode* actualRet = getActualRetSVFGNode(cs_return);
        SVFGEdge* edge = hasInterSVFGEdge(formalRet, actualRet, SVFGEdge::DirRet, csId);
        assert(edge != NULL && "Can not find inter value flow edge from fret to aret");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromAInToFIn(ActualINSVFGNode* actualIn, const llvm::Function* callee, SVFGEdgeSetTy& edges) {
        for (SVFGNode::const_iterator outIt = actualIn->OutEdgeBegin(), outEit = actualIn->OutEdgeEnd(); outIt != outEit; ++outIt) {
            SVFGEdge* edge = *outIt;
            if (edge->getDstNode()->getBB()->getParent() == callee)
                edges.insert(edge);
        }
    }

    virtual inline void getInterVFEdgeAtIndCSFromFOutToAOut(ActualOUTSVFGNode* actualOut, const llvm::Function* callee, SVFGEdgeSetTy& edges) {
        for (SVFGNode::const_iterator inIt = actualOut->InEdgeBegin(), inEit = actualOut->InEdgeEnd(); inIt != inEit; ++inIt) {
            SVFGEdge* edge = *inIt;
            if (edge->getSrcNode()->getBB()->getParent() == callee)
                edges.insert(edge);
        }
    }
    //@}

    /// Add SVFG edge
    inline bool addSVFGEdge(SVFGEdge* edge) {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        assert(added1 && added2 && "edge not added??");
        return true;
    }

    /// Given a PAGNode, set/get its def SVFG node (definition of top level pointers)
    //@{
    inline void setDef(const PAGNode* pagNode, const SVFGNode* node) {
        PAGNodeToDefMapTy::iterator it = PAGNodeToDefMap.find(pagNode);
        if(it == PAGNodeToDefMap.end()) {
            PAGNodeToDefMap[pagNode] = node->getId();
            assert(hasSVFGNode(node->getId()) && "not in the map!!");
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
    inline void setDef(const MRVer* mvar, const SVFGNode* node) {
        MSSAVarToDefMapTy::iterator it = MSSAVarToDefMap.find(mvar);
        if(it==MSSAVarToDefMap.end()) {
            MSSAVarToDefMap[mvar] = node->getId();
            assert(hasSVFGNode(node->getId()) && "not in the map!!");
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
    void addSVFGNodesForTopLevelPtrs();
    /// Create SVFG nodes for address-taken variables
    void addSVFGNodesForAddrTakenVars();
    /// Connect direct SVFG edges between two SVFG nodes (value-flow of top level pointers)
    void connectDirectSVFGEdges();
    /// Connect direct SVFG edges between two SVFG nodes (value-flow of top address-taken variables)
    void connectIndirectSVFGEdges();
    /// Connect indirect SVFG edges from global initializers (store) to main function entry
    void connectFromGlobalToProgEntry();

    inline bool isPhiCopyEdge(const CopyPE* copy) const {
        return mssa->getPAG()->isPhiNode(copy->getDstNode());
    }

    inline bool isPhiCopyNode(const PAGNode* node) const {
        return mssa->getPAG()->isPhiNode(node);
    }

    /// Add SVFG node
    virtual inline void addSVFGNode(SVFGNode* node) {
        addGNode(node->getId(),node);
    }
    /// Add SVFG node for program statement
    inline void addStmtSVFGNode(StmtSVFGNode* node) {
        addSVFGNode(node);
    }
    /// Add Dummy SVFG node for null pointer definition
    /// To be noted for black hole pointer it has already has address edge connected
    inline void addNullPtrSVFGNode(const PAGNode* pagNode) {
        NullPtrSVFGNode* sNode = new NullPtrSVFGNode(totalSVFGNode++,pagNode);
        addSVFGNode(sNode);
        setDef(pagNode,sNode);
    }
    /// Add Address SVFG node
    inline void addAddrSVFGNode(const AddrPE* addr) {
        AddrSVFGNode* sNode = new AddrSVFGNode(totalSVFGNode++,addr);
        addStmtSVFGNode(sNode);
        setDef(addr->getDstNode(),sNode);
    }
    /// Add Copy SVFG node
    inline void addCopySVFGNode(const CopyPE* copy) {
        CopySVFGNode* sNode = new CopySVFGNode(totalSVFGNode++,copy);
        addStmtSVFGNode(sNode);
        setDef(copy->getDstNode(),sNode);
    }
    /// Add Gep SVFG node
    inline void addGepSVFGNode(const GepPE* gep) {
        GepSVFGNode* sNode = new GepSVFGNode(totalSVFGNode++,gep);
        addStmtSVFGNode(sNode);
        setDef(gep->getDstNode(),sNode);
    }
    /// Add Load SVFG node
    void addLoadSVFGNode(LoadPE* load) {
        LoadSVFGNode* sNode = new LoadSVFGNode(totalSVFGNode++,load);
        addStmtSVFGNode(sNode);
        setDef(load->getDstNode(),sNode);
    }
    /// Add Store SVFG node,
    /// To be noted store does not create a new pointer, we do not set def for any PAG node
    void addStoreSVFGNode(StorePE* store) {
        StoreSVFGNode* sNode = new StoreSVFGNode(totalSVFGNode++,store);
        addStmtSVFGNode(sNode);
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
    inline void addActualParmSVFGNode(const PAGNode* aparm, llvm::CallSite cs) {
        ActualParmSVFGNode* sNode = new ActualParmSVFGNode(totalSVFGNode++,aparm,cs);
        addSVFGNode(sNode);
        PAGNodeToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add formal parameter SVFG node
    inline void addFormalParmSVFGNode(const PAGNode* fparm, const llvm::Function* fun, CallPESet& callPEs) {
        FormalParmSVFGNode* sNode = new FormalParmSVFGNode(totalSVFGNode++,fparm,fun);
        addSVFGNode(sNode);
        for(CallPESet::const_iterator it = callPEs.begin(), eit=callPEs.end();
                it!=eit; ++it)
            sNode->addCallPE(*it);

        setDef(fparm,sNode);
        PAGNodeToFormalParmMap[fparm] = sNode;
    }
    /// Add callee Return SVFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <PAGNodeID,CallSiteID> pair to find FormalRetSVFG node same as handling actual parameters
    inline void addFormalRetSVFGNode(const PAGNode* ret, const llvm::Function* fun, RetPESet& retPEs) {
        FormalRetSVFGNode* sNode = new FormalRetSVFGNode(totalSVFGNode++,ret,fun);
        addSVFGNode(sNode);
        for(RetPESet::const_iterator it = retPEs.begin(), eit=retPEs.end();
                it!=eit; ++it)
            sNode->addRetPE(*it);

        PAGNodeToFormalRetMap[ret] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add callsite Receive SVFG node
    inline void addActualRetSVFGNode(const PAGNode* ret,llvm::CallSite cs) {
        ActualRetSVFGNode* sNode = new ActualRetSVFGNode(totalSVFGNode++,ret,cs);
        addSVFGNode(sNode);
        setDef(ret,sNode);
        PAGNodeToActualRetMap[ret] = sNode;
    }
    /// Add llvm PHI SVFG node
    inline void addIntraPHISVFGNode(const PAGNode* phiResNode, PAG::PNodeBBPairList& oplist) {
        IntraPHISVFGNode* sNode = new IntraPHISVFGNode(totalSVFGNode++,phiResNode);
        addSVFGNode(sNode);
        u32_t pos = 0;
        for(PAG::PNodeBBPairList::const_iterator it = oplist.begin(), eit=oplist.end(); it!=eit; ++it,++pos)
            sNode->setOpVerAndBB(pos,it->first,it->second);
        setDef(phiResNode,sNode);
    }
    /// Add memory Function entry chi SVFG node
    inline void addFormalINSVFGNode(const MemSSA::ENTRYCHI* chi) {
        FormalINSVFGNode* sNode = new FormalINSVFGNode(totalSVFGNode++,chi);
        addSVFGNode(sNode);
        setDef(chi->getResVer(),sNode);
        funToFormalINMap[chi->getFunction()].set(sNode->getId());
    }
    /// Add memory Function return mu SVFG node
    inline void addFormalOUTSVFGNode(const MemSSA::RETMU* mu) {
        FormalOUTSVFGNode* sNode = new FormalOUTSVFGNode(totalSVFGNode++,mu);
        addSVFGNode(sNode);
        funToFormalOUTMap[mu->getFunction()].set(sNode->getId());
    }
    /// Add memory callsite mu SVFG node
    inline void addActualINSVFGNode(const MemSSA::CALLMU* mu) {
        ActualINSVFGNode* sNode = new ActualINSVFGNode(totalSVFGNode++,mu, mu->getCallSite());
        addSVFGNode(sNode);
        callSiteToActualINMap[mu->getCallSite()].set(sNode->getId());
    }
    /// Add memory callsite chi SVFG node
    inline void addActualOUTSVFGNode(const MemSSA::CALLCHI* chi) {
        ActualOUTSVFGNode* sNode = new ActualOUTSVFGNode(totalSVFGNode++,chi,chi->getCallSite());
        addSVFGNode(sNode);
        setDef(chi->getResVer(),sNode);
        callSiteToActualOUTMap[chi->getCallSite()].set(sNode->getId());
    }
    /// Add memory SSA PHI SVFG node
    inline void addIntraMSSAPHISVFGNode(const MemSSA::PHI* phi) {
        IntraMSSAPHISVFGNode* sNode = new IntraMSSAPHISVFGNode(totalSVFGNode++,phi);
        addSVFGNode(sNode);
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
            const SVFGNode* defNode = getSVFGNode(getDef(pagNode));
            if (const AddrSVFGNode* addr = llvm::dyn_cast<AddrSVFGNode>(defNode)) {
                if (PAG::getPAG()->isBlkObjOrConstantObj(addr->getPAGEdge()->getSrcID()))
                    return true;
            }
            else if(const CopySVFGNode* copy = llvm::dyn_cast<CopySVFGNode>(defNode)) {
                if (PAG::getPAG()->isNullPtr(copy->getPAGEdge()->getSrcID()))
                    return true;
            }
        }
        return false;
    }
};

namespace llvm {
/* !
 * GraphTraits specializations for SVFG to be used for generic graph algorithms.
 * Provide graph traits for traversing from a SVFG node using standard graph traversals.
 */
template<> struct GraphTraits<SVFGNode*>: public GraphTraits<GenericNode<SVFGNode,SVFGEdge>*  > {
};

/// Inverse GraphTraits specializations for Value flow node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVFGNode *> > : public GraphTraits<Inverse<GenericNode<SVFGNode,SVFGEdge>* > > {
};

template<> struct GraphTraits<SVFG*> : public GraphTraits<GenericGraph<SVFGNode,SVFGEdge>* > {
};
}

#endif /* SVFG_H_ */
