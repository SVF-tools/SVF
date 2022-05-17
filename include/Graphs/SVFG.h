//===- SVFG.h -- Sparse value-flow graph--------------------------------------//
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
 * SVFG.h
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#ifndef SVFG_H_
#define SVFG_H_

#include "Graphs/VFG.h"
#include "Graphs/SVFGNode.h"

namespace SVF
{

class PointerAnalysis;
class SVFGStat;

typedef VFGEdge SVFGEdge;
typedef VFGNode SVFGNode;
typedef ActualParmVFGNode ActualParmSVFGNode;
typedef ActualRetVFGNode ActualRetSVFGNode;
typedef FormalParmVFGNode FormalParmSVFGNode;
typedef FormalRetVFGNode FormalRetSVFGNode;

typedef NullPtrVFGNode NullPtrSVFGNode;
typedef StmtVFGNode StmtSVFGNode;
typedef AddrVFGNode AddrSVFGNode;
typedef CopyVFGNode CopySVFGNode;
typedef StoreVFGNode StoreSVFGNode;
typedef LoadVFGNode LoadSVFGNode;
typedef GepVFGNode GepSVFGNode;
typedef PHIVFGNode PHISVFGNode;
typedef IntraPHIVFGNode IntraPHISVFGNode;
typedef InterPHIVFGNode InterPHISVFGNode;


/*!
 * Sparse value flow graph
 * Each node stands for a definition, each edge stands for value flow relations
 */
class SVFG : public VFG
{
    friend class SVFGBuilder;
    friend class SaberSVFGBuilder;
    friend class TaintSVFGBuilder;
    friend class DDASVFGBuilder;
    friend class MTASVFGBuilder;
    friend class RcSvfgBuilder;

public:
    typedef VFGNodeIDToNodeMapTy SVFGNodeIDToNodeMapTy;
    typedef Map<const PAGNode*, NodeID> PAGNodeToDefMapTy;
    typedef Map<const MRVer*, NodeID> MSSAVarToDefMapTy;
    typedef NodeBS ActualINSVFGNodeSet;
    typedef NodeBS ActualOUTSVFGNodeSet;
    typedef NodeBS FormalINSVFGNodeSet;
    typedef NodeBS FormalOUTSVFGNodeSet;
    typedef Map<const CallICFGNode*, ActualINSVFGNodeSet>  CallSiteToActualINsMapTy;
    typedef Map<const CallICFGNode*, ActualOUTSVFGNodeSet>  CallSiteToActualOUTsMapTy;
    typedef Map<const SVFFunction*, FormalINSVFGNodeSet>  FunctionToFormalINsMapTy;
    typedef Map<const SVFFunction*, FormalOUTSVFGNodeSet>  FunctionToFormalOUTsMapTy;
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

protected:
    MSSAVarToDefMapTy MSSAVarToDefMap;	///< map a memory SSA operator to its definition SVFG node
    CallSiteToActualINsMapTy callSiteToActualINMap;
    CallSiteToActualOUTsMapTy callSiteToActualOUTMap;
    FunctionToFormalINsMapTy funToFormalINMap;
    FunctionToFormalOUTsMapTy funToFormalOUTMap;
    SVFGStat * stat;
    MemSSA* mssa;
    PointerAnalysis* pta;

    /// Clean up memory
    void destroy();

    /// Constructor
    SVFG(MemSSA* mssa, VFGK k);

    /// Start building SVFG
    virtual void buildSVFG();

public:
    /// Destructor
    virtual ~SVFG()
    {
        destroy();
    }

    /// Return statistics
    inline SVFGStat* getStat() const
    {
        return stat;
    }

    /// Clear MSSA
    inline void clearMSSA()
    {
        delete mssa;
        mssa = nullptr;
    }

    /// Get SVFG memory SSA
    inline MemSSA* getMSSA() const
    {
        return mssa;
    }

    /// Get Pointer Analysis
    inline PointerAnalysis* getPTA() const
    {
        return pta;
    }

    /// Get a SVFG node
    inline SVFGNode* getSVFGNode(NodeID id) const
    {
        return getVFGNode(id);
    }

    /// Whether has the SVFGNode
    inline bool hasSVFGNode(NodeID id) const
    {
        return hasVFGNode(id);
    }

    /// Get all inter value flow edges of a indirect call site
    void getInterVFEdgesForIndirectCallSite(const CallICFGNode* cs, const SVFFunction* callee, SVFGEdgeSetTy& edges);

    /// Dump graph into dot file
    void dump(const std::string& file, bool simple = false);

    /// Connect SVFG nodes between caller and callee for indirect call site
    virtual void connectCallerAndCallee(const CallICFGNode* cs, const SVFFunction* callee, SVFGEdgeSetTy& edges);

    /// Given a pagNode, return its definition site
    inline const SVFGNode* getDefSVFGNode(const PAGNode* pagNode) const
    {
        return getSVFGNode(getDef(pagNode));
    }

    /// Perform statistics
    void performStat();

    /// Has a SVFGNode
    //@{
    inline bool hasActualINSVFGNodes(const CallICFGNode* cs) const
    {
        return callSiteToActualINMap.find(cs)!=callSiteToActualINMap.end();
    }

    inline bool hasActualOUTSVFGNodes(const CallICFGNode* cs) const
    {
        return callSiteToActualOUTMap.find(cs)!=callSiteToActualOUTMap.end();
    }

    inline bool hasFormalINSVFGNodes(const SVFFunction* fun) const
    {
        return funToFormalINMap.find(fun)!=funToFormalINMap.end();
    }

    inline bool hasFormalOUTSVFGNodes(const SVFFunction* fun) const
    {
        return funToFormalOUTMap.find(fun)!=funToFormalOUTMap.end();
    }
    //@}

    /// Get SVFGNode set
    //@{
    inline ActualINSVFGNodeSet& getActualINSVFGNodes(const CallICFGNode* cs)
    {
        return callSiteToActualINMap[cs];
    }

    inline ActualOUTSVFGNodeSet& getActualOUTSVFGNodes(const CallICFGNode* cs)
    {
        return callSiteToActualOUTMap[cs];
    }

    inline FormalINSVFGNodeSet& getFormalINSVFGNodes(const SVFFunction* fun)
    {
        return funToFormalINMap[fun];
    }

    inline FormalOUTSVFGNodeSet& getFormalOUTSVFGNodes(const SVFFunction* fun)
    {
        return funToFormalOUTMap[fun];
    }
    //@}

    /// Whether a node is function entry SVFGNode
    const SVFFunction* isFunEntrySVFGNode(const SVFGNode* node) const;

    /// Whether a node is callsite return SVFGNode
    const CallICFGNode* isCallSiteRetSVFGNode(const SVFGNode* node) const;

    /// Remove a SVFG edge
    inline void removeSVFGEdge(SVFGEdge* edge)
    {
        removeVFGEdge(edge);
    }
    /// Remove a SVFGNode
    inline void removeSVFGNode(SVFGNode* node)
    {
        removeVFGNode(node);
    }

    /// Add SVFG edge
    inline bool addSVFGEdge(SVFGEdge* edge)
    {
        return addVFGEdge(edge);
    }

    /// Return total SVFG node number
    inline u32_t getSVFGNodeNum() const
    {
        return nodeNum;
    }

    /// Used *only* for Versioned FSPTA to encode propagation of versions
    /// in the worklist (allowing for breadth-first propagation).
    /// Returns the created node.
    inline const DummyVersionPropSVFGNode *addDummyVersionPropSVFGNode(const NodeID object, const NodeID version)
    {
        DummyVersionPropSVFGNode *dvpNode = new DummyVersionPropSVFGNode(totalVFGNode++, object, version);
        // Not going through add[S]VFGNode because we have no ICFG edge.
        addGNode(dvpNode->getId(), dvpNode);
        return dvpNode;
    }

    virtual void writeToFile(const std::string& filename);
    virtual void readFile(const std::string& filename);
    virtual MRVer* getMRVERFromString(const std::string& input);

protected:
    /// Add indirect def-use edges of a memory region between two statements,
    //@{
    SVFGEdge* addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts);
    SVFGEdge* addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts,CallSiteID csId);
    SVFGEdge* addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts,CallSiteID csId);
    SVFGEdge* addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts);
    //@}

    /// Add inter VF edge from callsite mu to function entry chi
    SVFGEdge* addInterIndirectVFCallEdge(const ActualINSVFGNode* src, const FormalINSVFGNode* dst,CallSiteID csId);

    /// Add inter VF edge from function exit mu to callsite chi
    SVFGEdge* addInterIndirectVFRetEdge(const FormalOUTSVFGNode* src, const ActualOUTSVFGNode* dst,CallSiteID csId);

    /// Connect SVFG nodes between caller and callee for indirect call site
    //@{
    /// Connect actual-in and formal-in
    virtual inline void connectAInAndFIn(const ActualINSVFGNode* actualIn, const FormalINSVFGNode* formalIn, CallSiteID csId, SVFGEdgeSetTy& edges)
    {
        SVFGEdge* edge = addInterIndirectVFCallEdge(actualIn, formalIn,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    /// Connect formal-out and actual-out
    virtual inline void connectFOutAndAOut(const FormalOUTSVFGNode* formalOut, const ActualOUTSVFGNode* actualOut, CallSiteID csId, SVFGEdgeSetTy& edges)
    {
        SVFGEdge* edge = addInterIndirectVFRetEdge(formalOut, actualOut,csId);
        if (edge != nullptr)
            edges.insert(edge);
    }
    //@}

    /// Get inter value flow edges between indirect call site and callee.
    //@{
    virtual inline void getInterVFEdgeAtIndCSFromAPToFP(const PAGNode* cs_arg, const PAGNode* fun_arg, const CallICFGNode*, CallSiteID csId, SVFGEdgeSetTy& edges)
    {
        SVFGNode* actualParam = getSVFGNode(getDef(cs_arg));
        SVFGNode* formalParam = getSVFGNode(getDef(fun_arg));
        SVFGEdge* edge = hasInterVFGEdge(actualParam, formalParam, SVFGEdge::CallDirVF, csId);
        assert(edge != nullptr && "Can not find inter value flow edge from aparam to fparam");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromFRToAR(const PAGNode* fun_ret, const PAGNode* cs_ret, CallSiteID csId, SVFGEdgeSetTy& edges)
    {
        SVFGNode* formalRet = getSVFGNode(getDef(fun_ret));
        SVFGNode* actualRet = getSVFGNode(getDef(cs_ret));
        SVFGEdge* edge = hasInterVFGEdge(formalRet, actualRet, SVFGEdge::RetDirVF, csId);
        assert(edge != nullptr && "Can not find inter value flow edge from fret to aret");
        edges.insert(edge);
    }

    virtual inline void getInterVFEdgeAtIndCSFromAInToFIn(ActualINSVFGNode* actualIn, const SVFFunction* callee, SVFGEdgeSetTy& edges)
    {
        for (SVFGNode::const_iterator outIt = actualIn->OutEdgeBegin(), outEit = actualIn->OutEdgeEnd(); outIt != outEit; ++outIt)
        {
            SVFGEdge* edge = *outIt;
            if (edge->getDstNode()->getFun() == callee)
                edges.insert(edge);
        }
    }

    virtual inline void getInterVFEdgeAtIndCSFromFOutToAOut(ActualOUTSVFGNode* actualOut, const SVFFunction* callee, SVFGEdgeSetTy& edges)
    {
        for (SVFGNode::const_iterator inIt = actualOut->InEdgeBegin(), inEit = actualOut->InEdgeEnd(); inIt != inEit; ++inIt)
        {
            SVFGEdge* edge = *inIt;
            if (edge->getSrcNode()->getFun() == callee)
                edges.insert(edge);
        }
    }
    //@}


    /// Given a PAGNode, set/get its def SVFG node (definition of top level pointers)
    //@{
    inline void setDef(const PAGNode* pagNode, const SVFGNode* node)
    {
        VFG::setDef(pagNode, node);
    }
    inline NodeID getDef(const PAGNode* pagNode) const
    {
        return VFG::getDef(pagNode);
    }
    inline bool hasDef(const PAGNode* pagNode) const
    {
        return VFG::hasDef(pagNode);
    }
    //@}

    /// Given a MSSADef, set/get its def SVFG node (definition of address-taken variables)
    //@{
    inline void setDef(const MRVer* mvar, const SVFGNode* node)
    {
        MSSAVarToDefMapTy::iterator it = MSSAVarToDefMap.find(mvar);
        if(it==MSSAVarToDefMap.end())
        {
            MSSAVarToDefMap[mvar] = node->getId();
            assert(hasSVFGNode(node->getId()) && "not in the map!!");
        }
        else
        {
            assert((it->second == node->getId()) && "a SVFIR node can only have unique definition ");
        }
    }
    inline NodeID getDef(const MRVer* mvar) const
    {
        MSSAVarToDefMapTy::const_iterator it = MSSAVarToDefMap.find(mvar);
        assert(it!=MSSAVarToDefMap.end() && "memory SSA does not have a definition??");
        return it->second;
    }
    //@}

    /// Create SVFG nodes for address-taken variables
    void addSVFGNodesForAddrTakenVars();
    /// Connect direct SVFG edges between two SVFG nodes (value-flow of top address-taken variables)
    void connectIndirectSVFGEdges();
    /// Connect indirect SVFG edges from global initializers (store) to main function entry
    void connectFromGlobalToProgEntry();

    /// Add SVFG node
    virtual inline void addSVFGNode(SVFGNode* node, ICFGNode* icfgNode)
    {
        addVFGNode(node, icfgNode);
    }

    /// Add memory Function entry chi SVFG node
    inline void addFormalINSVFGNode(const FunEntryICFGNode* funEntry,  const MRVer* resVer, const NodeID nodeId)
    {
        FormalINSVFGNode* sNode = new FormalINSVFGNode(nodeId, resVer, funEntry);
        addSVFGNode(sNode, pag->getICFG()->getFunEntryICFGNode(funEntry->getFun()));
        setDef(resVer,sNode);
        funToFormalINMap[funEntry->getFun()].set(sNode->getId());
    }

    /// Add memory Function return mu SVFG node
    inline void addFormalOUTSVFGNode(const FunExitICFGNode* funExit, const MRVer* ver, const NodeID nodeId)
    {
        FormalOUTSVFGNode* sNode = new FormalOUTSVFGNode(nodeId, ver, funExit);
        addSVFGNode(sNode,pag->getICFG()->getFunExitICFGNode(funExit->getFun()));
        funToFormalOUTMap[funExit->getFun()].set(sNode->getId());
    }

    /// Add memory callsite mu SVFG node
    inline void addActualINSVFGNode(const CallICFGNode* callsite, const MRVer* ver, const NodeID nodeId)
    {
        ActualINSVFGNode* sNode = new ActualINSVFGNode(nodeId, callsite, ver);
        addSVFGNode(sNode,pag->getICFG()->getCallICFGNode(callsite->getCallSite()));
        callSiteToActualINMap[callsite].set(sNode->getId());
    }

    /// Add memory callsite chi SVFG node
    inline void addActualOUTSVFGNode(const CallICFGNode* callsite, const MRVer* resVer, const NodeID nodeId)
    {
        ActualOUTSVFGNode* sNode = new ActualOUTSVFGNode(nodeId, callsite, resVer);
        addSVFGNode(sNode, pag->getICFG()->getRetICFGNode(callsite->getCallSite()));
        setDef(resVer,sNode);
        callSiteToActualOUTMap[callsite].set(sNode->getId());
    }

    /// Add memory SSA PHI SVFG node
    inline void addIntraMSSAPHISVFGNode(ICFGNode* BlockICFGNode, const Map<u32_t,const MRVer*>::const_iterator opVerBegin,
                                        const  Map<u32_t,const MRVer*>::const_iterator opVerEnd, const MRVer* resVer, const NodeID nodeId)
    {
        IntraMSSAPHISVFGNode* sNode = new IntraMSSAPHISVFGNode(nodeId, resVer);
        addSVFGNode(sNode, BlockICFGNode);
        for(MemSSA::PHI::OPVers::const_iterator it = opVerBegin, eit=opVerEnd; it!=eit; ++it)
            sNode->setOpVer(it->first,it->second);
        setDef(resVer,sNode);
    }

    /// Has function for EntryCHI/RetMU/CallCHI/CallMU
    //@{
    inline bool hasFuncEntryChi(const SVFFunction*  func) const
    {
        return (funToFormalINMap.find(func) != funToFormalINMap.end());
    }
    inline bool hasFuncRetMu(const SVFFunction*  func) const
    {
        return (funToFormalOUTMap.find(func) != funToFormalOUTMap.end());
    }
    inline bool hasCallSiteChi(const CallICFGNode* cs) const
    {
        return (callSiteToActualOUTMap.find(cs) != callSiteToActualOUTMap.end());
    }
    inline bool hasCallSiteMu(const CallICFGNode* cs) const
    {
        return (callSiteToActualINMap.find(cs) != callSiteToActualINMap.end());
    }
    //@}
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for SVFG to be used for generic graph algorithms.
 * Provide graph traits for traversing from a SVFG node using standard graph traversals.
 */
//template<> struct GraphTraits<SVF::SVFGNode*>: public GraphTraits<SVF::GenericNode<SVF::SVFGNode,SVF::SVFGEdge>*  > {
//};
//
///// Inverse GraphTraits specializations for Value flow node, it is used for inverse traversal.
//template<>
//struct GraphTraits<Inverse<SVF::SVFGNode *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::SVFGNode,SVF::SVFGEdge>* > > {
//};

template<> struct GraphTraits<SVF::SVFG*> : public GraphTraits<SVF::GenericGraph<SVF::SVFGNode,SVF::SVFGEdge>* >
{
    typedef SVF::SVFGNode *NodeRef;
};

} // End namespace llvm

#endif /* SVFG_H_ */
