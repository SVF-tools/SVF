//===- VFG.h ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * VFG.h
 *
 *  Created on: 18 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_UTIL_VFG_H_
#define INCLUDE_UTIL_VFG_H_


#include "SVFIR/SVFIR.h"
#include "Graphs/CallGraph.h"
#include "Graphs/VFGNode.h"
#include "Graphs/VFGEdge.h"

namespace SVF
{

class PointerAnalysis;
class VFGStat;
class CallICFGNode;

/*!
 *  Value Flow Graph (VFG)
 */
typedef GenericGraph<VFGNode,VFGEdge> GenericVFGTy;
class VFG : public GenericVFGTy
{

public:
    /// VFG kind
    enum VFGK
    {
        FULLSVFG, PTRONLYSVFG, FULLSVFG_OPT, PTRONLYSVFG_OPT
    };

    typedef OrderedMap<NodeID, VFGNode *> VFGNodeIDToNodeMapTy;
    typedef Set<VFGNode*> VFGNodeSet;
    typedef Map<const ValVar*, NodeID> ValVarToDefMapTy;
    typedef Map<std::pair<NodeID,const CallICFGNode*>, ActualParmVFGNode *> SVFVarToActualParmMapTy;
    typedef Map<const SVFVar*, ActualRetVFGNode *> SVFVarToActualRetMapTy;
    typedef Map<const SVFVar*, FormalParmVFGNode *> SVFVarToFormalParmMapTy;
    typedef Map<const SVFVar*, FormalRetVFGNode *> SVFVarToFormalRetMapTy;
    typedef Map<const SVFStmt*, StmtVFGNode*> SVFStmtToStmtVFGNodeMapTy;
    typedef Map<const SVFVar*, IntraPHIVFGNode*> SVFVarToPHIVFGNodeMapTy;
    typedef Map<const SVFVar*, BinaryOPVFGNode*> SVFVarToBinaryOPVFGNodeMapTy;
    typedef Map<const SVFVar*, UnaryOPVFGNode*> SVFVarToUnaryOPVFGNodeMapTy;
    typedef Map<const SVFVar*, BranchVFGNode*> SVFVarToBranchVFGNodeMapTy;
    typedef Map<const SVFVar*, CmpVFGNode*> SVFVarToCmpVFGNodeMapTy;
    typedef Map<const FunObjVar*, VFGNodeSet > FunToVFGNodesMapTy;

    typedef FormalParmVFGNode::CallPESet CallPESet;
    typedef FormalRetVFGNode::RetPESet RetPESet;
    typedef VFGEdge::VFGEdgeSetTy VFGEdgeSetTy;
    typedef VFGEdge::SVFGEdgeSetTy SVFGEdgeSetTy;
    typedef VFGEdge::VFGEdgeSetTy::iterator VFGNodeIter;
    typedef VFGNodeIDToNodeMapTy::iterator iterator;
    typedef VFGNodeIDToNodeMapTy::const_iterator const_iterator;
    typedef SVFIR::SVFStmtSet SVFStmtSet;
    typedef Set<const VFGNode*> GlobalVFGNodeSet;
    typedef Set<const SVFVar*> SVFVarSet;


protected:
    NodeID totalVFGNode;
    ValVarToDefMapTy ValVarToDefMap;	///< map a pag node to its definition SVG node
    SVFVarToActualParmMapTy SVFVarToActualParmMap; ///< map a SVFVar to an actual parameter
    SVFVarToActualRetMapTy SVFVarToActualRetMap; ///< map a SVFVar to an actual return
    SVFVarToFormalParmMapTy SVFVarToFormalParmMap; ///< map a SVFVar to a formal parameter
    SVFVarToFormalRetMapTy SVFVarToFormalRetMap; ///< map a SVFVar to a formal return
    SVFVarToPHIVFGNodeMapTy SVFVarToIntraPHIVFGNodeMap;	///< map a SVFVar to its PHIVFGNode
    SVFVarToBinaryOPVFGNodeMapTy SVFVarToBinaryOPVFGNodeMap;	///< map a SVFVar to its BinaryOPVFGNode
    SVFVarToUnaryOPVFGNodeMapTy SVFVarToUnaryOPVFGNodeMap;	///< map a SVFVar to its UnaryOPVFGNode
    SVFVarToBranchVFGNodeMapTy SVFVarToBranchVFGNodeMap;	///< map a SVFVar to its BranchVFGNode
    SVFVarToCmpVFGNodeMapTy SVFVarToCmpVFGNodeMap;	///< map a SVFVar to its CmpVFGNode
    SVFStmtToStmtVFGNodeMapTy SVFStmtToStmtVFGNodeMap;	///< map a SVFStmt to its StmtVFGNode
    FunToVFGNodesMapTy funToVFGNodesMap; ///< map a function to its VFGNodes;

    GlobalVFGNodeSet globalVFGNodes;	///< set of global store VFG nodes
    CallGraph* callgraph;
    SVFIR* pag;
    VFGK kind;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    VFG(CallGraph* callgraph, VFGK k = FULLSVFG);

    /// Destructor
    virtual ~VFG()
    {
        destroy();
    }

    /// Get VFG kind
    inline VFGK getKind() const
    {
        return kind;
    }

    /// Return true if this VFG only contains pointer related SVFGNodes for pointer analysis
    inline bool isPtrOnlySVFG() const
    {
        return (kind == PTRONLYSVFG) || (kind == PTRONLYSVFG_OPT);
    }

    /// Return SVFIR
    inline SVFIR* getPAG() const
    {
        return pag;
    }

    /// Return PTACallGraph
    inline CallGraph* getCallGraph() const
    {
        return callgraph;
    }

    /// Get a VFG node
    inline VFGNode* getVFGNode(NodeID id) const
    {
        return getGNode(id);
    }

    /// Whether has the VFGNode
    inline bool hasVFGNode(NodeID id) const
    {
        return hasGNode(id);
    }
    /// Return global stores
    inline GlobalVFGNodeSet& getGlobalVFGNodes()
    {
        return globalVFGNodes;
    }

    /// Get a SVFG edge according to src and dst
    VFGEdge* getIntraVFGEdge(const VFGNode* src, const VFGNode* dst, VFGEdge::VFGEdgeK kind);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Dump graph into dot file
    void view();

    /// Update VFG based on pointer analysis results
    void updateCallGraph(PointerAnalysis* pta);

    /// Connect VFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(const CallICFGNode* cs, const FunObjVar* callee, VFGEdgeSetTy& edges);

    /// Get callsite given a callsiteID
    //@{
    inline CallSiteID getCallSiteID(const CallICFGNode* cs, const FunObjVar* func) const
    {
        return callgraph->getCallSiteID(cs, func);
    }
    inline const CallICFGNode* getCallSite(CallSiteID id) const
    {
        return callgraph->getCallSite(id);
    }
    //@}

    /// Given a valVar, return its definition site
    inline const VFGNode* getDefVFGNode(const ValVar* valVar) const
    {
        return getVFGNode(getDef(valVar));
    }

    // Given an VFG node, return true if it has a left hand side top level pointer (PAGnode)
    inline bool hasLHSTopLevPtr(const VFGNode* node) const
    {
        return node && SVFUtil::isa<AddrVFGNode,
               CopyVFGNode,
               GepVFGNode,
               LoadVFGNode,
               PHIVFGNode,
               CmpVFGNode,
               BinaryOPVFGNode,
               UnaryOPVFGNode,
               ActualParmVFGNode,
               FormalParmVFGNode,
               ActualRetVFGNode,
               FormalRetVFGNode,
               NullPtrVFGNode>(node);
    }

    // Given an VFG node, return its left hand side top level pointer (PAGnode)
    const SVFVar* getLHSTopLevPtr(const VFGNode* node) const;

    /// Existence checks for VFGNodes
    //@{
    inline bool hasStmtVFGNode(const SVFStmt* svfStmt) const
    {
        return SVFStmtToStmtVFGNodeMap.find(svfStmt) != SVFStmtToStmtVFGNodeMap.end();
    }
    inline bool hasIntraPHIVFGNode(const SVFVar* svfVar) const
    {
        return SVFVarToIntraPHIVFGNodeMap.find(svfVar) != SVFVarToIntraPHIVFGNodeMap.end();
    }
    inline bool hasBinaryOPVFGNode(const SVFVar* svfVar) const
    {
        return SVFVarToBinaryOPVFGNodeMap.find(svfVar) != SVFVarToBinaryOPVFGNodeMap.end();
    }
    inline bool hasUnaryOPVFGNode(const SVFVar* svfVar) const
    {
        return SVFVarToUnaryOPVFGNodeMap.find(svfVar) != SVFVarToUnaryOPVFGNodeMap.end();
    }
    inline bool hasBranchVFGNode(const SVFVar* svfVar) const
    {
        return SVFVarToBranchVFGNodeMap.find(svfVar) != SVFVarToBranchVFGNodeMap.end();
    }
    inline bool hasCmpVFGNode(const SVFVar* svfVar) const
    {
        return SVFVarToCmpVFGNodeMap.find(svfVar) != SVFVarToCmpVFGNodeMap.end();
    }
    inline bool hasActualParmVFGNode(const SVFVar* aparm,const CallICFGNode* cs) const
    {
        return SVFVarToActualParmMap.find(std::make_pair(aparm->getId(),cs)) != SVFVarToActualParmMap.end();
    }
    inline bool hasActualRetVFGNode(const SVFVar* aret) const
    {
        return SVFVarToActualRetMap.find(aret) != SVFVarToActualRetMap.end();
    }
    inline bool hasFormalParmVFGNode(const SVFVar* fparm) const
    {
        return SVFVarToFormalParmMap.find(fparm) != SVFVarToFormalParmMap.end();
    }
    inline bool hasFormalRetVFGNode(const SVFVar* fret) const
    {
        return SVFVarToFormalRetMap.find(fret) != SVFVarToFormalRetMap.end();
    }
    //@}

    /// Get an VFGNode
    //@{
    inline StmtVFGNode* getStmtVFGNode(const SVFStmt* svfStmt) const
    {
        SVFStmtToStmtVFGNodeMapTy::const_iterator it = SVFStmtToStmtVFGNodeMap.find(svfStmt);
        assert(it != SVFStmtToStmtVFGNodeMap.end() && "StmtVFGNode can not be found??");
        return it->second;
    }
    inline IntraPHIVFGNode* getIntraPHIVFGNode(const SVFVar* svfVar) const
    {
        SVFVarToPHIVFGNodeMapTy::const_iterator it = SVFVarToIntraPHIVFGNodeMap.find(svfVar);
        assert(it != SVFVarToIntraPHIVFGNodeMap.end() && "PHIVFGNode can not be found??");
        return it->second;
    }
    inline BinaryOPVFGNode* getBinaryOPVFGNode(const SVFVar* svfVar) const
    {
        SVFVarToBinaryOPVFGNodeMapTy::const_iterator it = SVFVarToBinaryOPVFGNodeMap.find(svfVar);
        assert(it != SVFVarToBinaryOPVFGNodeMap.end() && "BinaryOPVFGNode can not be found??");
        return it->second;
    }
    inline UnaryOPVFGNode* getUnaryOPVFGNode(const SVFVar* svfVar) const
    {
        SVFVarToUnaryOPVFGNodeMapTy::const_iterator it = SVFVarToUnaryOPVFGNodeMap.find(svfVar);
        assert(it != SVFVarToUnaryOPVFGNodeMap.end() && "UnaryOPVFGNode can not be found??");
        return it->second;
    }
    inline BranchVFGNode* getBranchVFGNode(const SVFVar* svfVar) const
    {
        SVFVarToBranchVFGNodeMapTy::const_iterator it = SVFVarToBranchVFGNodeMap.find(svfVar);
        assert(it != SVFVarToBranchVFGNodeMap.end() && "BranchVFGNode can not be found??");
        return it->second;
    }
    inline CmpVFGNode* getCmpVFGNode(const SVFVar* svfVar) const
    {
        SVFVarToCmpVFGNodeMapTy::const_iterator it = SVFVarToCmpVFGNodeMap.find(svfVar);
        assert(it != SVFVarToCmpVFGNodeMap.end() && "CmpVFGNode can not be found??");
        return it->second;
    }
    inline ActualParmVFGNode* getActualParmVFGNode(const SVFVar* aparm,const CallICFGNode* cs) const
    {
        SVFVarToActualParmMapTy::const_iterator it = SVFVarToActualParmMap.find(std::make_pair(aparm->getId(),cs));
        assert(it!=SVFVarToActualParmMap.end() && "actual parameter VFG node can not be found??");
        return it->second;
    }
    inline ActualRetVFGNode* getActualRetVFGNode(const SVFVar* aret) const
    {
        SVFVarToActualRetMapTy::const_iterator it = SVFVarToActualRetMap.find(aret);
        assert(it!=SVFVarToActualRetMap.end() && "actual return VFG node can not be found??");
        return it->second;
    }
    inline FormalParmVFGNode* getFormalParmVFGNode(const SVFVar* fparm) const
    {
        SVFVarToFormalParmMapTy::const_iterator it = SVFVarToFormalParmMap.find(fparm);
        assert(it!=SVFVarToFormalParmMap.end() && "formal parameter VFG node can not be found??");
        return it->second;
    }
    inline FormalRetVFGNode* getFormalRetVFGNode(const SVFVar* fret) const
    {
        SVFVarToFormalRetMapTy::const_iterator it = SVFVarToFormalRetMap.find(fret);
        assert(it!=SVFVarToFormalRetMap.end() && "formal return VFG node can not be found??");
        return it->second;
    }
    //@}

    /// Whether a node is function entry VFGNode
    const FunObjVar* isFunEntryVFGNode(const VFGNode* node) const;

    /// Whether a SVFVar has a blackhole or const object as its definition
    inline bool hasBlackHoleConstObjAddrAsDef(const ValVar* valVar) const
    {
        if (hasDef(valVar))
        {
            const VFGNode* defNode = getVFGNode(getDef(valVar));
            if (const AddrVFGNode* addr = SVFUtil::dyn_cast<AddrVFGNode>(defNode))
            {
                if (SVFIR::getPAG()->isBlkObjOrConstantObj(addr->getSVFStmt()->getSrcID()))
                    return true;
            }
            else if(const CopyVFGNode* copy = SVFUtil::dyn_cast<CopyVFGNode>(defNode))
            {
                if (SVFIR::getPAG()->isNullPtr(copy->getSVFStmt()->getSrcID()))
                    return true;
            }
        }
        return false;
    }

    /// Return all the VFGNodes of a function
    ///@{
    inline VFGNodeSet& getVFGNodes(const FunObjVar *fun)
    {
        return funToVFGNodesMap[fun];
    }
    inline bool hasVFGNodes(const FunObjVar *fun) const
    {
        return funToVFGNodesMap.find(fun) != funToVFGNodesMap.end();
    }
    inline bool VFGNodes(const FunObjVar *fun) const
    {
        return funToVFGNodesMap.find(fun) != funToVFGNodesMap.end();
    }
    inline VFGNodeSet::const_iterator getVFGNodeBegin(const FunObjVar *fun) const
    {
        FunToVFGNodesMapTy::const_iterator it = funToVFGNodesMap.find(fun);
        assert(it != funToVFGNodesMap.end() && "this function does not have any VFGNode");
        return it->second.begin();
    }
    inline VFGNodeSet::const_iterator getVFGNodeEnd(const FunObjVar *fun) const
    {
        FunToVFGNodesMapTy::const_iterator it = funToVFGNodesMap.find(fun);
        assert(it != funToVFGNodesMap.end() && "this function does not have any VFGNode");
        return it->second.end();
    }
    ///@}
    /// Add control-flow edges for top level pointers
    //@{
    VFGEdge* addIntraDirectVFEdge(NodeID srcId, NodeID dstId);
    VFGEdge* addCallEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    VFGEdge* addRetEdge(NodeID srcId, NodeID dstId, CallSiteID csId);
    //@}

    /// Remove a SVFG edge
    inline void removeVFGEdge(VFGEdge* edge)
    {
        edge->getDstNode()->removeIncomingEdge(edge);
        edge->getSrcNode()->removeOutgoingEdge(edge);
        delete edge;
    }
    /// Remove a VFGNode
    inline void removeVFGNode(VFGNode* node)
    {
        removeGNode(node);
    }

    /// Whether we has a SVFG edge
    //@{
    VFGEdge* hasIntraVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind);
    VFGEdge* hasInterVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind, CallSiteID csId);
    VFGEdge* hasThreadVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind);
    //@}

    /// Add VFG edge
    inline bool addVFGEdge(VFGEdge* edge)
    {
        bool added1 = edge->getDstNode()->addIncomingEdge(edge);
        bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
        bool both_added = added1 & added2;
        assert(both_added && "VFGEdge not added??");
        return both_added;
    }

protected:

    /// sanitize Intra edges, verify that both nodes belong to the same function.
    inline void checkIntraEdgeParents(const VFGNode *srcNode, const VFGNode *dstNode)
    {
        const FunObjVar *srcfun = srcNode->getFun();
        const FunObjVar *dstfun = dstNode->getFun();
        if(srcfun != nullptr && dstfun != nullptr)
        {
            assert((srcfun == dstfun) && "src and dst nodes of an intra VFG edge are not in the same function?");
        }
    }

    /// Add inter VF edge from actual to formal parameters
    inline VFGEdge* addInterEdgeFromAPToFP(ActualParmVFGNode* src, FormalParmVFGNode* dst, CallSiteID csId)
    {
        return addCallEdge(src->getId(),dst->getId(),csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline VFGEdge* addInterEdgeFromFRToAR(FormalRetVFGNode* src, ActualRetVFGNode* dst, CallSiteID csId)
    {
        return addRetEdge(src->getId(),dst->getId(),csId);
    }

    /// Add inter VF edge from actual to formal parameters
    inline VFGEdge* addInterEdgeFromAPToFP(NodeID src, NodeID dst, CallSiteID csId)
    {
        return addCallEdge(src,dst,csId);
    }
    /// Add inter VF edge from callee return to callsite receive parameter
    inline VFGEdge* addInterEdgeFromFRToAR(NodeID src, NodeID dst, CallSiteID csId)
    {
        return addRetEdge(src,dst,csId);
    }

    /// Connect VFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-param and formal param
    virtual inline void connectAParamAndFParam(const ValVar* csArg, const ValVar* funArg, const CallICFGNode* cbn, CallSiteID csId, VFGEdgeSetTy& edges)
    {
        NodeID actualParam = getActualParmVFGNode(csArg, cbn)->getId();
        NodeID formalParam = getFormalParmVFGNode(funArg)->getId();
        VFGEdge* edge = addInterEdgeFromAPToFP(actualParam, formalParam,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    /// Connect formal-ret and actual ret
    virtual inline void connectFRetAndARet(const ValVar* funReturn, const ValVar* csReturn, CallSiteID csId, VFGEdgeSetTy& edges)
    {
        NodeID formalRet = getFormalRetVFGNode(funReturn)->getId();
        NodeID actualRet = getActualRetVFGNode(csReturn)->getId();
        VFGEdge* edge = addInterEdgeFromFRToAR(formalRet, actualRet,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    //@}

    /// Given a ValVar, set/get its def VFG node (definition of top level pointers)
    //@{
    inline void setDef(const ValVar* valVar, const VFGNode* node)
    {
        ValVarToDefMapTy::iterator it = ValVarToDefMap.find(valVar);
        if(it == ValVarToDefMap.end())
        {
            ValVarToDefMap[valVar] = node->getId();
            assert(hasVFGNode(node->getId()) && "not in the map!!");
        }
        else
        {
            assert((it->second == node->getId()) && "a ValVar can only have unique definition ");
        }
    }
    inline NodeID getDef(const ValVar* valVar) const
    {
        ValVarToDefMapTy::const_iterator it = ValVarToDefMap.find(valVar);
        assert(it!=ValVarToDefMap.end() && "ValVar does not have a definition??");
        return it->second;
    }
    inline bool hasDef(const ValVar* valVar) const
    {
        return (ValVarToDefMap.find(valVar) != ValVarToDefMap.end());
    }
    //@}

    /// Create VFG nodes
    void addVFGNodes();

    /// Get SVFStmt set
    virtual inline SVFStmt::SVFStmtSetTy& getSVFStmtSet(SVFStmt::PEDGEK kind)
    {
        if (isPtrOnlySVFG())
            return pag->getPTASVFStmtSet(kind);
        else
            return pag->getSVFStmtSet(kind);
    }

    virtual inline bool isInterestedSVFVar(const SVFVar* node) const
    {
        if (isPtrOnlySVFG())
            return node->isPointer();
        else
            return true;
    }

    /// Create edges between VFG nodes within a function
    void connectDirectVFGEdges();

    /// Create edges between VFG nodes across functions
    void addVFGInterEdges(const CallICFGNode* cs, const FunObjVar* callee);

    inline bool isPhiCopyEdge(const SVFStmt* copy) const
    {
        return pag->isPhiNode(copy->getDstNode());
    }

    /// Add a VFG node
    virtual inline void addVFGNode(VFGNode* vfgNode, ICFGNode* icfgNode)
    {
        addGNode(vfgNode->getId(), vfgNode);
        vfgNode->setICFGNode(icfgNode);
        icfgNode->addVFGNode(vfgNode);

        if(const FunObjVar* fun = icfgNode->getFun())
            funToVFGNodesMap[fun].insert(vfgNode);
        else
            globalVFGNodes.insert(vfgNode);
    }

    /// Add a VFG node for program statement
    inline void addStmtVFGNode(StmtVFGNode* node, const SVFStmt* svfStmt)
    {
        assert(SVFStmtToStmtVFGNodeMap.find(svfStmt)==SVFStmtToStmtVFGNodeMap.end() && "should not insert twice!");
        SVFStmtToStmtVFGNodeMap[svfStmt] = node;
        addVFGNode(node, svfStmt->getICFGNode());
    }
    /// Add a Dummy VFG node for null pointer definition
    /// To be noted for black hole pointer it has already has address edge connected
    inline void addNullPtrVFGNode(const ValVar* svfVar)
    {
        NullPtrVFGNode* sNode = new NullPtrVFGNode(totalVFGNode++,svfVar);
        addVFGNode(sNode, pag->getICFG()->getGlobalICFGNode());
        setDef(svfVar,sNode);
    }
    /// Add an Address VFG node
    inline void addAddrVFGNode(const AddrStmt* addr)
    {
        AddrVFGNode* sNode = new AddrVFGNode(totalVFGNode++,addr);
        addStmtVFGNode(sNode, addr);
        setDef(SVFUtil::cast<ValVar>(addr->getLHSVar()),sNode);
    }
    /// Add a Copy VFG node
    inline void addCopyVFGNode(const CopyStmt* copy)
    {
        CopyVFGNode* sNode = new CopyVFGNode(totalVFGNode++,copy);
        addStmtVFGNode(sNode, copy);
        setDef(copy->getLHSVar(),sNode);
    }
    /// Add a Gep VFG node
    inline void addGepVFGNode(const GepStmt* gep)
    {
        GepVFGNode* sNode = new GepVFGNode(totalVFGNode++,gep);
        addStmtVFGNode(sNode, gep);
        setDef(gep->getLHSVar(),sNode);
    }
    /// Add a Load VFG node
    void addLoadVFGNode(const LoadStmt* load)
    {
        LoadVFGNode* sNode = new LoadVFGNode(totalVFGNode++,load);
        addStmtVFGNode(sNode, load);
        setDef(load->getLHSVar(),sNode);
    }
    /// Add a Store VFG node,
    /// To be noted store does not create a new pointer, we do not set def for any SVFIR node
    void addStoreVFGNode(const StoreStmt* store)
    {
        StoreVFGNode* sNode = new StoreVFGNode(totalVFGNode++,store);
        addStmtVFGNode(sNode, store);
    }

    /// Add an actual parameter VFG node
    /// To be noted that multiple actual parameters may have same value (SVFVar)
    /// So we need to make a pair <SVFVarID,CallSiteID> to find the right VFGParmNode
    inline void addActualParmVFGNode(const ValVar* aparm, const CallICFGNode* cs)
    {
        ActualParmVFGNode* sNode = new ActualParmVFGNode(totalVFGNode++,aparm,cs);
        addVFGNode(sNode, const_cast<CallICFGNode*>(cs));
        SVFVarToActualParmMap[std::make_pair(aparm->getId(),cs)] = sNode;
        /// do not set def here, this node is not a variable definition
    }
    /// Add a formal parameter VFG node
    inline void addFormalParmVFGNode(const ValVar* fparm, const FunObjVar* fun, const CallPE* callPE)
    {
        FormalParmVFGNode* sNode = new FormalParmVFGNode(totalVFGNode++,fparm,fun);
        addVFGNode(sNode, pag->getICFG()->getFunEntryICFGNode(fun));
        sNode->setCallPE(callPE);

        setDef(fparm,sNode);
        SVFVarToFormalParmMap[fparm] = sNode;
    }
    /// Add a callee Return VFG node
    /// To be noted that here we assume returns of a procedure have already been unified into one
    /// Otherwise, we need to handle formalRet using <SVFVarID,CallSiteID> pair to find FormalRetVFG node same as handling actual parameters
    inline void addFormalRetVFGNode(const ValVar* uniqueFunRet, const FunObjVar* fun, RetPESet& retPEs)
    {
        FormalRetVFGNode *sNode = new FormalRetVFGNode(totalVFGNode++, uniqueFunRet, fun);
        addVFGNode(sNode, pag->getICFG()->getFunExitICFGNode(fun));
        for (RetPESet::const_iterator it = retPEs.begin(), eit = retPEs.end(); it != eit; ++it)
            sNode->addRetPE(*it);

        SVFVarToFormalRetMap[uniqueFunRet] = sNode;
        /// if this uniqueFunRet is a phi node, which means it will receive values from multiple return instructions of fun
        /// we will set this phi node's def later
        /// Ideally, every function uniqueFunRet should be a PhiNode (SVFIRBuilder.cpp), unless it does not have ret instruction
        if (!pag->isPhiNode(uniqueFunRet))
        {
            std::string warn = fun->getName();
            SVFUtil::writeWrnMsg(warn + " does not have any ret instruction!");
            setDef(uniqueFunRet, sNode);
        }
    }
    /// Add a callsite Receive VFG node
    inline void addActualRetVFGNode(const ValVar* ret,const CallICFGNode* cs)
    {
        ActualRetVFGNode* sNode = new ActualRetVFGNode(totalVFGNode++,ret,cs);
        addVFGNode(sNode, const_cast<RetICFGNode*>(cs->getRetICFGNode()));
        setDef(ret,sNode);
        SVFVarToActualRetMap[ret] = sNode;
    }
    /// Add an llvm PHI VFG node
    inline void addIntraPHIVFGNode(const MultiOpndStmt* edge)
    {
        IntraPHIVFGNode* sNode = new IntraPHIVFGNode(totalVFGNode++, edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVerAndBB(pos, var, edge->getICFGNode());
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        SVFVarToIntraPHIVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a Compare VFG node
    inline void addCmpVFGNode(const CmpStmt* edge)
    {
        CmpVFGNode* sNode = new CmpVFGNode(totalVFGNode++, edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVer(pos, var);
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        SVFVarToCmpVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a BinaryOperator VFG node
    inline void addBinaryOPVFGNode(const BinaryOPStmt* edge)
    {
        BinaryOPVFGNode* sNode = new BinaryOPVFGNode(totalVFGNode++, edge->getRes());
        u32_t pos = 0;
        for(auto var : edge->getOpndVars())
        {
            sNode->setOpVer(pos, var);
            pos++;
        }
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        SVFVarToBinaryOPVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a UnaryOperator VFG node
    inline void addUnaryOPVFGNode(const UnaryOPStmt* edge)
    {
        UnaryOPVFGNode* sNode = new UnaryOPVFGNode(totalVFGNode++, edge->getRes());
        sNode->setOpVer(0, edge->getOpVar());
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getRes(),sNode);
        SVFVarToUnaryOPVFGNodeMap[edge->getRes()] = sNode;
    }
    /// Add a BranchVFGNode
    inline void addBranchVFGNode(const BranchStmt* edge)
    {
        BranchVFGNode* sNode = new BranchVFGNode(totalVFGNode++, edge);
        addVFGNode(sNode,edge->getICFGNode());
        setDef(edge->getBranchInst(),sNode);
        SVFVarToBranchVFGNodeMap[edge->getBranchInst()] = sNode;
    }
};

} // End namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::VFGNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::VFGNode,SVF::VFGEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::VFGNode *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::VFGNode,SVF::VFGEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::VFG*> : public GenericGraphTraits<SVF::GenericGraph<SVF::VFGNode,SVF::VFGEdge>* >
{
    typedef SVF::VFGNode *NodeRef;
};

} // End namespace llvm

#endif /* INCLUDE_UTIL_VFG_H_ */
