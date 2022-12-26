//===- DDAVFSolver.h -- Demand-driven analysis value-flow solver------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * DDAVFSolver.h
 *
 *  Created on: Jul 3, 2014
 *      Author: Yulei Sui
 */

#ifndef VALUEFLOWDDA_H_
#define VALUEFLOWDDA_H_

#include "DDA/DDAStat.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"
#include "Util/SCC.h"
#include "MemoryModel/PointsTo.h"
#include <algorithm>

namespace SVF
{

/*!
 * Value-Flow Based Demand-Driven Points-to Analysis
 */
template<class CVar, class CPtSet, class DPIm>
class DDAVFSolver
{
    friend class DDAStat;
public:
    typedef SCCDetection<SVFG*> SVFGSCC;
    typedef SCCDetection<PTACallGraph*> CallGraphSCC;
    typedef PTACallGraphEdge::CallInstSet CallInstSet;
    typedef SVFIR::CallSiteSet CallSiteSet;
    typedef OrderedSet<DPIm> DPTItemSet;
    typedef OrderedMap<DPIm, CPtSet> DPImToCPtSetMap;
    typedef OrderedMap<DPIm,CVar> DPMToCVarMap;
    typedef OrderedMap<DPIm,DPIm> DPMToDPMMap;
    typedef OrderedMap<NodeID, DPTItemSet> LocToDPMVecMap;
    typedef OrderedSet<const SVFGEdge* > ConstSVFGEdgeSet;
    typedef SVFGEdge::SVFGEdgeSetTy SVFGEdgeSet;
    typedef OrderedMap<const SVFGNode*, DPTItemSet> StoreToPMSetMap;

    ///Constructor
    DDAVFSolver(): outOfBudgetQuery(false),_pag(nullptr),_svfg(nullptr),_ander(nullptr),_callGraph(nullptr), _callGraphSCC(nullptr), _svfgSCC(nullptr), ddaStat(nullptr)
    {
    }
    /// Destructor
    virtual ~DDAVFSolver()
    {
        if(_ander != nullptr)
        {
            // AndersenWaveDiff::releaseAndersenWaveDiff();
            _ander = nullptr;
        }

        if (_svfg != nullptr)
        {
            // DDASVFGBuilder::releaseDDASVFG();
            _svfg = nullptr;
        }

        if (_svfgSCC != nullptr)
            delete _svfgSCC;
        _svfgSCC = nullptr;

        _callGraph = nullptr;
        _callGraphSCC = nullptr;
    }
    /// Return candidate pointers for DDA
    inline NodeBS& getCandidateQueries()
    {
        return candidateQueries;
    }
    /// Given CVar and location (SVFGNode) return a new DPItem
    virtual inline DPIm getDPIm(const CVar& var, const SVFGNode* loc) const
    {
        DPIm dpm(var,loc);
        return dpm;
    }
    /// Union pts
    virtual bool unionDDAPts(CPtSet& pts, const CPtSet& targetPts)
    {
        return (pts |= targetPts);
    }
    /// Union pts
    virtual bool unionDDAPts(DPIm dpm, const CPtSet& targetPts)
    {
        CPtSet& pts = isTopLevelPtrStmt(dpm.getLoc()) ? dpmToTLCPtSetMap[dpm] : dpmToADCPtSetMap[dpm];
        return pts |= targetPts;
    }
    /// Add pts
    virtual void addDDAPts(CPtSet& pts, const CVar& var)
    {
        pts.set(var);
    }
    /// Return SVFG
    inline SVFG* getSVFG() const
    {
        return _svfg;
    }
    /// Return SVFGSCC
    inline SVFGSCC* getSVFGSCC() const
    {
        return _svfgSCC;
    }
    // Dump cptsSet
    inline void dumpCPtSet(const CPtSet& cpts) const
    {
        SVFUtil::outs() << "{";
        for(typename CPtSet::iterator it = cpts.begin(), eit = cpts.end(); it!=eit; ++it)
        {
            SVFUtil::outs() << (*it) << " ";
        }
        SVFUtil::outs() << "}\n";
    }
    /// Compute points-to
    virtual const CPtSet& findPT(const DPIm& dpm)
    {

        if(isbkVisited(dpm))
        {
            const CPtSet& cpts = getCachedPointsTo(dpm);
            DBOUT(DDDA, SVFUtil::outs() << "\t already backward visited dpm: ");
            DBOUT(DDDA, dpm.dump());
            DBOUT(DDDA, SVFUtil::outs() << "\t return points-to: ");
            DBOUT(DDDA, dumpCPtSet(cpts));
            return cpts;
        }

        DBOUT(DDDA, SVFUtil::outs() << "\t backward visit dpm: ");
        DBOUT(DDDA, dpm.dump());
        markbkVisited(dpm);
        addDpmToLoc(dpm);

        if(testOutOfBudget(dpm) == false)
        {

            CPtSet pts;
            handleSingleStatement(dpm, pts);

            /// Add successors of current stmt if its pts has been changed.
            updateCachedPointsTo(dpm, pts);
        }
        return getCachedPointsTo(dpm);
    }

protected:
    /// Handle single statement
    virtual void handleSingleStatement(const DPIm& dpm, CPtSet& pts)
    {
        /// resolve function pointer first at indirect callsite
        resolveFunPtr(dpm);

        const SVFGNode* node = dpm.getLoc();
        if(SVFUtil::isa<AddrSVFGNode>(node))
        {
            handleAddr(pts,dpm,SVFUtil::cast<AddrSVFGNode>(node));
        }
        else if (SVFUtil::isa<CopySVFGNode, PHISVFGNode, ActualParmSVFGNode,
                 FormalParmSVFGNode, ActualRetSVFGNode,
                 FormalRetSVFGNode, NullPtrSVFGNode>(node))
        {
            backtraceAlongDirectVF(pts,dpm);
        }
        else if(SVFUtil::isa<GepSVFGNode>(node))
        {
            CPtSet gepPts;
            backtraceAlongDirectVF(gepPts,dpm);
            unionDDAPts(pts, processGepPts(SVFUtil::cast<GepSVFGNode>(node),gepPts));
        }
        else if(SVFUtil::isa<LoadSVFGNode>(node))
        {
            CPtSet loadpts;
            startNewPTCompFromLoadSrc(loadpts,dpm);
            for(typename CPtSet::iterator it = loadpts.begin(), eit = loadpts.end(); it!=eit; ++it)
            {
                backtraceAlongIndirectVF(pts,getDPImWithOldCond(dpm,*it,node));
            }
        }
        else if(SVFUtil::isa<StoreSVFGNode>(node))
        {
            if(isMustAlias(getLoadDpm(dpm),dpm))
            {
                DBOUT(DDDA, SVFUtil::outs() << "+++must alias for load and store:");
                DBOUT(DDDA, getLoadDpm(dpm).dump());
                DBOUT(DDDA, dpm.dump());
                DBOUT(DDDA, SVFUtil::outs() << "+++\n");
                DOSTAT(ddaStat->_NumOfMustAliases++);
                backtraceToStoreSrc(pts,dpm);
            }
            else
            {
                CPtSet storepts;
                startNewPTCompFromStoreDst(storepts,dpm);
                for(typename CPtSet::iterator it = storepts.begin(), eit = storepts.end(); it!=eit; ++it)
                {
                    if(propagateViaObj(*it,getLoadCVar(dpm)))
                    {
                        backtraceToStoreSrc(pts,getDPImWithOldCond(dpm,*it,node));

                        if(isStrongUpdate(storepts,SVFUtil::cast<StoreSVFGNode>(node)))
                        {
                            DBOUT(DDDA, SVFUtil::outs() << "backward strong update for obj " << dpm.getCurNodeID() << "\n");
                            DOSTAT(addSUStat(dpm,node);)
                        }
                        else
                        {
                            DOSTAT(rmSUStat(dpm,node);)
                            backtraceAlongIndirectVF(pts,getDPImWithOldCond(dpm,*it,node));
                        }
                    }
                    else
                    {
                        backtraceAlongIndirectVF(pts,dpm);
                    }
                }
            }
        }
        else if(SVFUtil::isa<MRSVFGNode>(node))
        {
            backtraceAlongIndirectVF(pts,dpm);
        }
        else
            assert(false && "unexpected kind of SVFG nodes");
    }

    /// recompute points-to for value-flow cycles and indirect calls
    void reCompute(const DPIm& dpm)
    {
        /// re-compute due to indirect calls
        SVFGEdgeSet newIndirectEdges;
        if(_pag->isFunPtr(dpm.getCurNodeID()))
        {
            const CallSiteSet& csSet = _pag->getIndCallSites(dpm.getCurNodeID());
            for(CallSiteSet::const_iterator it = csSet.begin(), eit = csSet.end(); it!=eit; ++it)
                updateCallGraphAndSVFG(dpm, (*it),newIndirectEdges);
        }
        /// callgraph scc detection for local variable in recursion
        if(!newIndirectEdges.empty())
            _callGraphSCC->find();
        reComputeForEdges(dpm,newIndirectEdges,true);

        /// re-compute for transitive closures
        SVFGEdgeSet edgeSet(dpm.getLoc()->getOutEdges());
        reComputeForEdges(dpm,edgeSet,false);
    }

    /// Traverse along out edges to find all nodes which may be affected by locDPM.
    void reComputeForEdges(const DPIm& dpm, const SVFGEdgeSet& edgeSet, bool indirectCall = false)
    {
        for (SVFGNode::const_iterator it = edgeSet.begin(), eit = edgeSet.end(); it != eit; ++it)
        {
            const SVFGEdge* edge = *it;
            const SVFGNode* dst = edge->getDstNode();
            typename LocToDPMVecMap::const_iterator locIt = getLocToDPMVecMap().find(dst->getId());
            /// Only collect nodes we have traversed
            if (locIt == getLocToDPMVecMap().end())
                continue;
            DPTItemSet dpmSet(locIt->second.begin(), locIt->second.end());
            for(typename DPTItemSet::const_iterator it = dpmSet.begin(),eit = dpmSet.end(); it!=eit; ++it)
            {
                const DPIm& dstDpm = *it;
                if(!indirectCall && SVFUtil::isa<IndirectSVFGEdge>(edge) && !SVFUtil::isa<LoadSVFGNode>(edge->getDstNode()))
                {
                    if(dstDpm.getCurNodeID() == dpm.getCurNodeID())
                    {
                        DBOUT(DDDA,SVFUtil::outs() << "\t Recompute, forward from :");
                        DBOUT(DDDA, dpm.dump());
                        DOSTAT(ddaStat->_NumOfStepInCycle++);
                        clearbkVisited(dstDpm);
                        findPT(dstDpm);
                    }
                }
                else
                {
                    if(indirectCall)
                        DBOUT(DDDA,SVFUtil::outs() << "\t Recompute for indirect call from :");
                    else
                        DBOUT(DDDA,SVFUtil::outs() << "\t Recompute forward from :");
                    DBOUT(DDDA, dpm.dump());
                    DOSTAT(ddaStat->_NumOfStepInCycle++);
                    clearbkVisited(dstDpm);
                    findPT(dstDpm);
                }
            }
        }
    }

    /// Build SVFG
    virtual inline void buildSVFG(SVFIR* pag)
    {
        _ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
        _svfg = svfgBuilder.buildPTROnlySVFG(_ander);
        _pag = _svfg->getPAG();
    }
    /// Reset visited map for next points-to query
    virtual inline void resetQuery()
    {
        if(outOfBudgetQuery)
            OOBResetVisited();

        locToDpmSetMap.clear();
        dpmToloadDpmMap.clear();
        loadToPTCVarMap.clear();
        outOfBudgetQuery = false;
        ddaStat->_NumOfStep = 0;
    }
    /// Reset visited map if the current query is out-of-budget
    inline void OOBResetVisited()
    {
        for(typename LocToDPMVecMap::const_iterator it = locToDpmSetMap.begin(),eit = locToDpmSetMap.end(); it!=eit; ++it)
        {
            DPTItemSet dpmSet(it->second.begin(), it->second.end());
            for(typename DPTItemSet::const_iterator dit = dpmSet.begin(),deit=dpmSet.end(); dit!=deit; ++dit)
                if(isOutOfBudgetDpm(*dit)==false)
                    clearbkVisited(*dit);
        }
    }
    /// GetDefinition SVFG
    inline const SVFGNode* getDefSVFGNode(const PAGNode* pagNode) const
    {
        return getSVFG()->getDefSVFGNode(pagNode);
    }
    /// Backward traverse along indirect value flows
    void backtraceAlongIndirectVF(CPtSet& pts, const DPIm& oldDpm)
    {
        const SVFGNode* node = oldDpm.getLoc();
        NodeID obj = oldDpm.getCurNodeID();
        if (_pag->isConstantObj(obj) || _pag->isNonPointerObj(obj))
            return;
        const SVFGEdgeSet edgeSet(node->getInEdges());
        for (SVFGNode::const_iterator it = edgeSet.begin(), eit = edgeSet.end(); it != eit; ++it)
        {
            if(const IndirectSVFGEdge* indirEdge = SVFUtil::dyn_cast<IndirectSVFGEdge>(*it))
            {
                const NodeBS& guard = indirEdge->getPointsTo();
                if(guard.test(obj))
                {
                    DBOUT(DDDA, SVFUtil::outs() << "\t\t==backtrace indirectVF svfgNode " <<
                          indirEdge->getDstID() << " --> " << indirEdge->getSrcID() << "\n");
                    backwardPropDpm(pts,oldDpm.getCurNodeID(),oldDpm,indirEdge);
                }
            }
        }
    }
    /// Backward traverse along direct value flows
    void backtraceAlongDirectVF(CPtSet& pts, const DPIm& oldDpm)
    {
        const SVFGNode* node = oldDpm.getLoc();
        const SVFGEdgeSet edgeSet(node->getInEdges());
        for (SVFGNode::const_iterator it = edgeSet.begin(), eit = edgeSet.end(); it != eit; ++it)
        {
            if(const DirectSVFGEdge* dirEdge = SVFUtil::dyn_cast<DirectSVFGEdge>(*it))
            {
                DBOUT(DDDA, SVFUtil::outs() << "\t\t==backtrace directVF svfgNode " <<
                      dirEdge->getDstID() << " --> " << dirEdge->getSrcID() << "\n");
                const SVFGNode* srcNode = dirEdge->getSrcNode();
                backwardPropDpm(pts,getSVFG()->getLHSTopLevPtr(srcNode)->getId(),oldDpm,dirEdge);
            }
        }
    }

    /// Backward traverse for top-level pointers of load/store statements
    ///@{
    inline void startNewPTCompFromLoadSrc(CPtSet& pts, const DPIm& oldDpm)
    {
        const LoadSVFGNode* load = SVFUtil::cast<LoadSVFGNode>(oldDpm.getLoc());
        const SVFGNode* loadSrc = getDefSVFGNode(load->getPAGSrcNode());
        DBOUT(DDDA, SVFUtil::outs() << "!##start new computation from loadSrc svfgNode " <<
              load->getId() << " --> " << loadSrc->getId() << "\n");
        const SVFGEdge* edge = getSVFG()->getIntraVFGEdge(loadSrc,load,SVFGEdge::IntraDirectVF);
        assert(edge && "Edge not found!!");
        backwardPropDpm(pts,load->getPAGSrcNodeID(),oldDpm,edge);

    }
    inline void startNewPTCompFromStoreDst(CPtSet& pts, const DPIm& oldDpm)
    {
        const StoreSVFGNode* store = SVFUtil::cast<StoreSVFGNode>(oldDpm.getLoc());
        const SVFGNode* storeDst = getDefSVFGNode(store->getPAGDstNode());
        DBOUT(DDDA, SVFUtil::outs() << "!##start new computation from storeDst svfgNode " <<
              store->getId() << " --> " << storeDst->getId() << "\n");
        const SVFGEdge* edge = getSVFG()->getIntraVFGEdge(storeDst,store,SVFGEdge::IntraDirectVF);
        assert(edge && "Edge not found!!");
        backwardPropDpm(pts,store->getPAGDstNodeID(),oldDpm,edge);
    }
    inline void backtraceToStoreSrc(CPtSet& pts, const DPIm& oldDpm)
    {
        const StoreSVFGNode* store = SVFUtil::cast<StoreSVFGNode>(oldDpm.getLoc());
        const SVFGNode* storeSrc = getDefSVFGNode(store->getPAGSrcNode());
        DBOUT(DDDA, SVFUtil::outs() << "++backtrace to storeSrc from svfgNode " << getLoadDpm(oldDpm).getLoc()->getId() << " to "<<
              store->getId() << " to " << storeSrc->getId() <<"\n");
        const SVFGEdge* edge = getSVFG()->getIntraVFGEdge(storeSrc,store,SVFGEdge::IntraDirectVF);
        assert(edge && "Edge not found!!");
        backwardPropDpm(pts,store->getPAGSrcNodeID(),oldDpm,edge);
    }
    //@}

    /// dpm transit during backward tracing
    virtual void backwardPropDpm(CPtSet& pts, NodeID ptr,const DPIm& oldDpm,const SVFGEdge* edge)
    {
        DPIm dpm(oldDpm);
        dpm.setLocVar(edge->getSrcNode(),ptr);
        DOTIMESTAT(double start = DDAStat::getClk(true));
        /// handle context-/path- sensitivity
        if(handleBKCondition(dpm,edge)==false)
        {
            DOTIMESTAT(ddaStat->_TotalTimeOfBKCondition += DDAStat::getClk(true) - start);
            DBOUT(DDDA, SVFUtil::outs() << "\t!!! infeasible path svfgNode: " << edge->getDstID() << " --| " << edge->getSrcID() << "\n");
            DOSTAT(ddaStat->_NumOfInfeasiblePath++);
            return;
        }

        /// record the source of load dpm
        if(SVFUtil::isa<IndirectSVFGEdge>(edge))
            addLoadDpmAndCVar(dpm,getLoadDpm(oldDpm),getLoadCVar(oldDpm));

        DOSTAT(ddaStat->_NumOfDPM++);
        /// handle out of budget case
        unionDDAPts(pts,findPT(dpm));
    }
    /// whether load and store are aliased
    virtual bool isMustAlias(const DPIm&, const DPIm&)
    {
        return false;
    }
    /// Return TRUE if this is a strong update STORE statement.
    virtual bool isStrongUpdate(const CPtSet& dstCPSet, const StoreSVFGNode* store)
    {
        if (dstCPSet.count() == 1)
        {
            /// Find the unique element in cpts
            typename CPtSet::iterator it = dstCPSet.begin();
            const CVar& var = *it;
            // Strong update can be made if this points-to target is not heap, array or field-insensitive.
            if (!isHeapCondMemObj(var,store) && !isArrayCondMemObj(var)
                    && !isFieldInsenCondMemObj(var) && !isLocalCVarInRecursion(var))
            {
                return true;
            }
        }
        return false;
    }
    /// Whether a local variable is in function recursions
    virtual inline bool isLocalCVarInRecursion(const CVar& var) const
    {
        NodeID id = getPtrNodeID(var);
        const MemObj* obj = _pag->getObject(id);
        assert(obj && "object not found!!");
        if(obj->isStack())
        {
            if(const SVFFunction* svffun = _pag->getGNode(id)->getFunction())
            {
                return _callGraphSCC->isInCycle(_callGraph->getCallGraphNode(svffun)->getId());
            }
        }
        return false;
    }

    /// If the points-to contain the object obj, we could move forward along indirect value-flow edge
    virtual inline bool propagateViaObj(const CVar& storeObj, const CVar& loadObj)
    {
        return getPtrNodeID(storeObj) == getPtrNodeID(loadObj);
    }
    /// resolve function pointer
    void resolveFunPtr(const DPIm& dpm)
    {
        if(const CallICFGNode* cbn= getSVFG()->isCallSiteRetSVFGNode(dpm.getLoc()))
        {
            if(_pag->isIndirectCallSites(cbn))
            {
                NodeID funPtr = _pag->getFunPtr(cbn);
                DPIm funPtrDpm(dpm);
                funPtrDpm.setLocVar(getSVFG()->getDefSVFGNode(_pag->getGNode(funPtr)),funPtr);
                findPT(funPtrDpm);
            }
        }
        else if(const SVFFunction* fun = getSVFG()->isFunEntrySVFGNode(dpm.getLoc()))
        {
            CallInstSet csSet;
            /// use pre-analysis call graph to approximate all potential callsites
            _ander->getPTACallGraph()->getIndCallSitesInvokingCallee(fun,csSet);
            for(CallInstSet::const_iterator it = csSet.begin(), eit = csSet.end(); it!=eit; ++it)
            {
                NodeID funPtr = _pag->getFunPtr(*it);
                DPIm funPtrDpm(dpm);
                funPtrDpm.setLocVar(getSVFG()->getDefSVFGNode(_pag->getGNode(funPtr)),funPtr);
                findPT(funPtrDpm);
            }
        }
    }
    /// Methods to be implemented in child class
    //@{
    /// Get variable ID (PAGNodeID) according to CVar
    virtual NodeID getPtrNodeID(const CVar& var) const = 0;
    /// ProcessGep node to generate field object nodes of a struct
    virtual CPtSet processGepPts(const GepSVFGNode* gep, const CPtSet& srcPts) = 0;
    /// Handle AddrSVFGNode to add proper points-to
    virtual void handleAddr(CPtSet& pts,const DPIm& dpm,const AddrSVFGNode* addr) = 0;
    /// Get conservative points-to results when the query is out of budget
    virtual CPtSet getConservativeCPts(const DPIm& dpm) = 0;
    /// Handle condition for context or path analysis (backward analysis)
    virtual inline bool handleBKCondition(DPIm&, const SVFGEdge*)
    {
        return true;
    }
    /// Update call graph
    virtual inline void updateCallGraphAndSVFG(const DPIm&, const CallICFGNode*, SVFGEdgeSet&) {}
    //@}

    ///Visited flags to avoid cycles
    //@{
    inline void markbkVisited(const DPIm& dpm)
    {
        backwardVisited.insert(dpm);
    }
    inline bool isbkVisited(const DPIm& dpm)
    {
        return backwardVisited.find(dpm)!=backwardVisited.end();
    }
    inline void clearbkVisited(const DPIm& dpm)
    {
        assert(backwardVisited.find(dpm)!=backwardVisited.end() && "dpm not found!");
        backwardVisited.erase(dpm);
    }
    //@}

    /// Points-to Caching for top-level pointers and address-taken objects
    //@{
    virtual inline const CPtSet& getCachedPointsTo(const DPIm& dpm)
    {
        if (isTopLevelPtrStmt(dpm.getLoc()))
            return getCachedTLPointsTo(dpm);
        else
            return getCachedADPointsTo(dpm);
    }
    virtual inline void updateCachedPointsTo(const DPIm& dpm, const CPtSet& pts)
    {
        if (unionDDAPts(dpm, pts))
        {
            DOSTAT(double start = DDAStat::getClk(true));
            reCompute(dpm);
            DOSTAT(ddaStat->_AnaTimeCyclePerQuery += DDAStat::getClk(true) - start);
        }
    }
    virtual inline const CPtSet& getCachedTLPointsTo(const DPIm& dpm)
    {
        return dpmToTLCPtSetMap[dpm];
    }
    virtual inline const CPtSet& getCachedADPointsTo(const DPIm& dpm)
    {
        return dpmToADCPtSetMap[dpm];
    }
    //@}

    /// Whether this is a top-level pointer statement
    inline bool isTopLevelPtrStmt(const SVFGNode* stmt)
    {
        return !SVFUtil::isa<StoreSVFGNode, MRSVFGNode>(stmt);
    }
    /// Return dpm with old context and path conditions
    virtual inline DPIm getDPImWithOldCond(const DPIm& oldDpm,const CVar& var, const SVFGNode* loc)
    {
        DPIm dpm(oldDpm);
        dpm.setLocVar(loc,getPtrNodeID(var));

        if(SVFUtil::isa<StoreSVFGNode>(loc))
            addLoadDpmAndCVar(dpm,getLoadDpm(oldDpm),var);

        if(SVFUtil::isa<LoadSVFGNode>(loc))
            addLoadDpmAndCVar(dpm,oldDpm,var);

        DOSTAT(ddaStat->_NumOfDPM++);
        return dpm;
    }
    /// SVFG SCC detection
    inline void SVFGSCCDetection()
    {
        if(_svfgSCC==nullptr)
        {
            _svfgSCC = new SVFGSCC(getSVFG());
        }
        _svfgSCC->find();
    }
    /// Get SCC rep node of a SVFG node.
    inline NodeID getSVFGSCCRepNode(NodeID id)
    {
        return _svfgSCC->repNode(id);
    }
    /// Return whether this SVFGNode is in cycle
    inline bool isSVFGNodeInCycle(const SVFGNode* node)
    {
        return _svfgSCC->isInCycle(node->getId());
    }
    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    inline bool edgeInSVFGSCC(const SVFGEdge* edge)
    {
        return (getSVFGSCCRepNode(edge->getSrcID()) == getSVFGSCCRepNode(edge->getDstID()));
    }
    /// Set callgraph
    inline void setCallGraph (PTACallGraph* cg)
    {
        _callGraph = cg;
    }
    /// Set callgraphSCC
    inline void setCallGraphSCC (CallGraphSCC* scc)
    {
        _callGraphSCC = scc;
    }
    /// Check heap and array object
    //@{
    virtual inline bool isHeapCondMemObj(const CVar& var, const StoreSVFGNode*)
    {
        const MemObj* mem = _pag->getObject(getPtrNodeID(var));
        assert(mem && "memory object is null??");
        return mem->isHeap();
    }

    inline bool isArrayCondMemObj(const CVar& var) const
    {
        const MemObj* mem = _pag->getObject(getPtrNodeID(var));
        assert(mem && "memory object is null??");
        return mem->isArray();
    }
    inline bool isFieldInsenCondMemObj(const CVar& var) const
    {
        const MemObj* mem =  _pag->getBaseObj(getPtrNodeID(var));
        return mem->isFieldInsensitive();
    }
    //@}
private:
    /// Map a SVFGNode to its dpms for handling value-flow cycles
    //@{
    inline const LocToDPMVecMap& getLocToDPMVecMap() const
    {
        return locToDpmSetMap;
    }
    inline const DPTItemSet& getDpmSetAtLoc(const SVFGNode* loc)
    {
        return locToDpmSetMap[loc->getId()];
    }
    inline void addDpmToLoc(const DPIm& dpm)
    {
        locToDpmSetMap[dpm.getLoc()->getId()].insert(dpm);
    }
    inline void removeDpmFromLoc(const DPIm& dpm)
    {
        assert(dpm == locToDpmSetMap[dpm.getLoc()].back() && "dpm not match with the end of vector");
        locToDpmSetMap[dpm.getLoc()->getId()].erase(dpm);
    }
    //@}
protected:
    /// LoadDpm for must-alias analysis
    //@{
    inline void addLoadDpmAndCVar(const DPIm& dpm,const DPIm& loadDpm,const CVar& loadVar)
    {
        addLoadCVar(dpm,loadVar);
        addLoadDpm(dpm,loadDpm);
    }
    /// Note that simply use "dpmToloadDpmMap[dpm]=loadDpm", requires DPIm have a default constructor
    inline void addLoadDpm(const DPIm& dpm,const DPIm& loadDpm)
    {
        typename DPMToDPMMap::iterator it = dpmToloadDpmMap.find(dpm);
        if(it!=dpmToloadDpmMap.end())
            it->second = loadDpm;
        else
            dpmToloadDpmMap.insert(std::make_pair(dpm,loadDpm));
    }
    inline const DPIm& getLoadDpm(const DPIm& dpm) const
    {
        typename DPMToDPMMap::const_iterator it = dpmToloadDpmMap.find(dpm);
        assert(it!=dpmToloadDpmMap.end() && "not found??");
        return it->second;
    }
    inline void addLoadCVar(const DPIm& dpm, const CVar& loadVar)
    {
        typename DPMToCVarMap::iterator it = loadToPTCVarMap.find(dpm);
        if(it!=loadToPTCVarMap.end())
            it->second = loadVar;
        else
            loadToPTCVarMap.insert(std::make_pair(dpm,loadVar));
    }
    inline const CVar& getLoadCVar(const DPIm& dpm) const
    {
        typename DPMToCVarMap::const_iterator it = loadToPTCVarMap.find(dpm);
        assert(it!=loadToPTCVarMap.end() && "not found??");
        return it->second;
    }
    //@}
    /// Return Andersen's analysis
    inline AndersenWaveDiff* getAndersenAnalysis() const
    {
        return _ander;
    }
    /// handle out-of-budget queries
    //@{
    /// Handle out-of-budget dpm
    inline void handleOutOfBudgetDpm(const DPIm& dpm) {}
    inline bool testOutOfBudget(const DPIm& dpm)
    {
        if(outOfBudgetQuery) return true;
        if(++ddaStat->_NumOfStep > DPIm::getMaxBudget())
            outOfBudgetQuery = true;
        return isOutOfBudgetDpm(dpm) || outOfBudgetQuery;
    }
    inline bool isOutOfBudgetQuery() const
    {
        return outOfBudgetQuery;
    }
    inline void addOutOfBudgetDpm(const DPIm& dpm)
    {
        outOfBudgetDpms.insert(dpm);
    }
    inline bool isOutOfBudgetDpm(const DPIm& dpm) const
    {
        return outOfBudgetDpms.find(dpm) != outOfBudgetDpms.end();
    }
    //@}

    /// Set DDAStat
    inline DDAStat* setDDAStat(DDAStat* s)
    {
        ddaStat = s;
        return ddaStat;
    }
    /// stat strong updates num
    inline void addSUStat(const DPIm& dpm, const SVFGNode* node)
    {
        if (storeToDPMs[node].insert(dpm).second)
        {
            ddaStat->_NumOfStrongUpdates++;
            ddaStat->_StrongUpdateStores.set(node->getId());
        }
    }
    /// remove strong updates num if the dpm goes to weak updates branch
    inline void rmSUStat(const DPIm& dpm, const SVFGNode* node)
    {
        DPTItemSet& dpmSet = storeToDPMs[node];
        if (dpmSet.erase(dpm))
        {
            ddaStat->_NumOfStrongUpdates--;
            if(dpmSet.empty())
                ddaStat->_StrongUpdateStores.reset(node->getId());
        }
    }

    bool outOfBudgetQuery;			///< Whether the current query is out of step limits
    SVFIR* _pag;						///< SVFIR
    SVFG* _svfg;					///< SVFG
    AndersenWaveDiff* _ander;		///< Andersen's analysis
    NodeBS candidateQueries;		///< candidate pointers;
    PTACallGraph* _callGraph;		///< CallGraph
    CallGraphSCC* _callGraphSCC;	///< SCC for CallGraph
    SVFGSCC* _svfgSCC;				///< SCC for SVFG
    DPTItemSet backwardVisited;		///< visited map during backward traversing
    DPImToCPtSetMap dpmToTLCPtSetMap;	///< points-to caching map for top-level vars
    DPImToCPtSetMap dpmToADCPtSetMap;	///< points-to caching map for address-taken vars
    LocToDPMVecMap locToDpmSetMap;	///< map location to its dpms
    DPMToDPMMap dpmToloadDpmMap;		///< dpms at loads for may/must-alias analysis with stores
    DPMToCVarMap loadToPTCVarMap;	///< map a load dpm to its cvar pointed by its pointer operand
    DPTItemSet outOfBudgetDpms;		///< out of budget dpm set
    StoreToPMSetMap storeToDPMs;	///< map store to set of DPM which have been stong updated there
    DDAStat* ddaStat;				///< DDA stat
    SVFGBuilder svfgBuilder;			///< SVFG Builder
};

} // End namespace SVF

#endif /* VALUEFLOWDDA_H_ */
