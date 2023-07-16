//===- PointerAnalysisImpl.h -- Pointer analysis implementation--------------------//
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
 * PointerAnalysis.h
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_MEMORYMODEL_POINTERANALYSISIMPL_H_
#define INCLUDE_MEMORYMODEL_POINTERANALYSISIMPL_H_

#include <Graphs/ConsG.h>
#include "MemoryModel/PointerAnalysis.h"

namespace SVF
{

/*!
 * Pointer analysis implementation which uses bit vector based points-to data structure
 */
class BVDataPTAImpl : public PointerAnalysis
{

public:
    typedef PTData<NodeID, NodeSet, NodeID, PointsTo> PTDataTy;
    typedef DiffPTData<NodeID, NodeSet, NodeID, PointsTo> DiffPTDataTy;
    typedef DFPTData<NodeID, NodeSet, NodeID, PointsTo> DFPTDataTy;
    typedef VersionedPTData<NodeID, NodeSet, NodeID, PointsTo, VersionedVar, Set<VersionedVar>> VersionedPTDataTy;

    typedef MutablePTData<NodeID, NodeSet, NodeID, PointsTo> MutPTDataTy;
    typedef MutableDiffPTData<NodeID, NodeSet, NodeID, PointsTo> MutDiffPTDataTy;
    typedef MutableDFPTData<NodeID, NodeSet, NodeID, PointsTo> MutDFPTDataTy;
    typedef MutableIncDFPTData<NodeID, NodeSet, NodeID, PointsTo> MutIncDFPTDataTy;
    typedef MutableVersionedPTData<NodeID, NodeSet, NodeID, PointsTo, VersionedVar, Set<VersionedVar>> MutVersionedPTDataTy;

    typedef PersistentPTData<NodeID, NodeSet, NodeID, PointsTo> PersPTDataTy;
    typedef PersistentDiffPTData<NodeID, NodeSet, NodeID, PointsTo> PersDiffPTDataTy;
    typedef PersistentDFPTData<NodeID, NodeSet, NodeID, PointsTo> PersDFPTDataTy;
    typedef PersistentIncDFPTData<NodeID, NodeSet, NodeID, PointsTo> PersIncDFPTDataTy;
    typedef PersistentVersionedPTData<NodeID, NodeSet, NodeID, PointsTo, VersionedVar, Set<VersionedVar>> PersVersionedPTDataTy;

    /// How the PTData used is implemented.
    enum PTBackingType
    {
        Mutable,
        Persistent,
    };

    /// Constructor
    BVDataPTAImpl(SVFIR* pag, PointerAnalysis::PTATY type, bool alias_check = true);

    /// Destructor
    ~BVDataPTAImpl() override = default;

    inline PersistentPointsToCache<PointsTo> &getPtCache()
    {
        return ptCache;
    }

    static inline bool classof(const PointerAnalysis *pta)
    {
        return pta->getImplTy() == BVDataImpl;
    }

    /// Get points-to and reverse points-to
    ///@{
    inline const PointsTo& getPts(NodeID id) override
    {
        return ptD->getPts(id);
    }
    inline const NodeSet& getRevPts(NodeID nodeId) override
    {
        return ptD->getRevPts(nodeId);
    }
    //@}

    /// Remove element from the points-to set of id.
    virtual inline void clearPts(NodeID id, NodeID element)
    {
        ptD->clearPts(id, element);
    }

    /// Clear points-to set of id.
    virtual inline void clearFullPts(NodeID id)
    {
        ptD->clearFullPts(id);
    }

    /// Union/add points-to. Add the reverse points-to for node collapse purpose
    /// To be noted that adding reverse pts might incur 10% total overhead during solving
    //@{
    virtual inline bool unionPts(NodeID id, const PointsTo& target)
    {
        return ptD->unionPts(id, target);
    }
    virtual inline bool unionPts(NodeID id, NodeID ptd)
    {
        return ptD->unionPts(id,ptd);
    }
    virtual inline bool addPts(NodeID id, NodeID ptd)
    {
        return ptD->addPts(id,ptd);
    }
    //@}

    /// Clear all data
    virtual inline void clearAllPts()
    {
        ptD->clear();
    }

    /// Expand FI objects
    virtual void expandFIObjs(const PointsTo& pts, PointsTo& expandedPts);
    /// TODO: remove repetition.
    virtual void expandFIObjs(const NodeBS& pts, NodeBS& expandedPts);

    /// Remap all points-to sets to use the current mapping.
    void remapPointsToSets(void);

    /// Interface for analysis result storage on filesystem.
    //@{
    virtual void writeToFile(const std::string& filename);
    virtual void writeObjVarToFile(const std::string& filename);
    virtual bool readFromFile(const std::string& filename);
    //@}

protected:
    /// Get points-to data structure
    inline PTDataTy* getPTDataTy() const
    {
        return ptD.get();
    }


    /// Finalization of pointer analysis, and normalize points-to information to Bit Vector representation
    void finalize() override;

    /// Update callgraph. This should be implemented by its subclass.
    virtual inline bool updateCallGraph(const CallSiteToFunPtrMap&)
    {
        assert(false && "Virtual function not implemented!");
        return false;
    }

    inline DiffPTDataTy* getDiffPTDataTy() const
    {
        DiffPTDataTy* diff = SVFUtil::dyn_cast<DiffPTDataTy>(ptD.get());
        assert(diff && "BVDataPTAImpl::getDiffPTDataTy: not a DiffPTDataTy!");
        return diff;
    }

    inline DFPTDataTy* getDFPTDataTy() const
    {
        DFPTDataTy* df = SVFUtil::dyn_cast<DFPTDataTy>(ptD.get());
        assert(df && "BVDataPTAImpl::getDFPTDataTy: not a DFPTDataTy!");
        return df;
    }

    inline MutDFPTDataTy* getMutDFPTDataTy() const
    {
        MutDFPTDataTy* mdf = SVFUtil::dyn_cast<MutDFPTDataTy>(ptD.get());
        assert(mdf && "BVDataPTAImpl::getMutDFPTDataTy: not a MutDFPTDataTy!");
        return mdf;
    }

    inline VersionedPTDataTy* getVersionedPTDataTy() const
    {
        VersionedPTDataTy* v = SVFUtil::dyn_cast<VersionedPTDataTy>(ptD.get());
        assert(v && "BVDataPTAImpl::getVersionedPTDataTy: not a VersionedPTDataTy!");
        return v;
    }

    /// On the fly call graph construction
    virtual void onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges);

    /// Normalize points-to information for field-sensitive analysis,
    /// i.e., replace fieldObj with baseObj if it is field-insensitive
    virtual void normalizePointsTo();

private:
    /// Points-to data
    std::unique_ptr<PTDataTy> ptD;

    PersistentPointsToCache<PointsTo> ptCache;

public:
    /// Interface expose to users of our pointer analysis, given Value infos
    AliasResult alias(const SVFValue* V1,
                      const SVFValue* V2) override;

    /// Interface expose to users of our pointer analysis, given PAGNodeID
    AliasResult alias(NodeID node1, NodeID node2) override;

    /// Interface expose to users of our pointer analysis, given two pts
    virtual AliasResult alias(const PointsTo& pts1, const PointsTo& pts2);

    /// dump and debug, print out conditional pts
    //@{
    void dumpCPts() override
    {
        ptD->dumpPTData();
    }

    void dumpTopLevelPtsTo() override;

    void dumpAllPts() override;
    //@}
};

/*!
 * Pointer analysis implementation which uses conditional points-to map data structure (context/path sensitive analysis)
 */
template<class Cond>
class CondPTAImpl : public PointerAnalysis
{

public:
    typedef CondVar<Cond> CVar;
    typedef CondStdSet<CVar>  CPtSet;
    typedef PTData<CVar, Set<CVar>, CVar, CPtSet> PTDataTy;
    typedef MutablePTData<CVar, Set<CVar>, CVar, CPtSet> MutPTDataTy;
    typedef Map<NodeID,PointsTo> PtrToBVPtsMap; /// map a pointer to its BitVector points-to representation
    typedef Map<NodeID, NodeSet> PtrToNSMap;
    typedef Map<NodeID,CPtSet> PtrToCPtsMap;	 /// map a pointer to its conditional points-to set

    /// Constructor
    CondPTAImpl(SVFIR* pag, PointerAnalysis::PTATY type) : PointerAnalysis(pag, type), normalized(false)
    {
        if (type == PathS_DDA || type == Cxt_DDA)
            ptD = new MutPTDataTy();
        else
            assert(false && "no points-to data available");

        ptaImplTy = CondImpl;
    }

    /// Destructor
    virtual ~CondPTAImpl()
    {
        destroy();
    }

    static inline bool classof(const PointerAnalysis *pta)
    {
        return pta->getImplTy() == CondImpl;
    }

    /// Release memory
    inline void destroy()
    {
        delete ptD;
        ptD = nullptr;
    }

    /// Get points-to data
    inline PTDataTy* getPTDataTy() const
    {
        return ptD;
    }

    inline MutPTDataTy* getMutPTDataTy() const
    {
        MutPTDataTy* mut = SVFUtil::dyn_cast<MutPTDataTy>(ptD);
        assert(mut && "BVDataPTAImpl::getMutPTDataTy: not a MutPTDataTy!");
        return mut;
    }

    inline bool hasPtsMap(void) const
    {
        return SVFUtil::isa<MutPTDataTy>(ptD);
    }

    inline const typename MutPTDataTy::PtsMap& getPtsMap() const
    {
        if (MutPTDataTy *m = SVFUtil::dyn_cast<MutPTDataTy>(ptD)) return m->getPtsMap();
        assert(false && "CondPTAImpl::getPtsMap: not a PTData with a PtsMap!");
        exit(1);
    }

    /// Get points-to and reverse points-to
    ///@{
    virtual inline const CPtSet& getPts(CVar id)
    {
        return ptD->getPts(id);
    }
    virtual inline const Set<CVar>& getRevPts(CVar nodeId)
    {
        return ptD->getRevPts(nodeId);
    }
    //@}

    /// Clear all data
    virtual inline void clearPts()
    {
        ptD->clear();
    }

    /// Whether cpts1 and cpts2 have overlap points-to targets
    bool overlap(const CPtSet& cpts1, const CPtSet& cpts2) const
    {
        for (typename CPtSet::const_iterator it1 = cpts1.begin(); it1 != cpts1.end(); ++it1)
        {
            for (typename CPtSet::const_iterator it2 = cpts2.begin(); it2 != cpts2.end(); ++it2)
            {
                if(isSameVar(*it1,*it2))
                    return true;
            }
        }
        return false;
    }

    /// Expand all fields of an aggregate in all points-to sets
    void expandFIObjs(const CPtSet& cpts, CPtSet& expandedCpts)
    {
        expandedCpts = cpts;;
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit)
        {
            if(pag->getBaseObjVar(cit->get_id())==cit->get_id())
            {
                NodeBS& fields = pag->getAllFieldsObjVars(cit->get_id());
                for(NodeBS::iterator it = fields.begin(), eit = fields.end(); it!=eit; ++it)
                {
                    CVar cvar(cit->get_cond(),*it);
                    expandedCpts.set(cvar);
                }
            }
        }
    }

protected:

    /// Finalization of pointer analysis, and normalize points-to information to Bit Vector representation
    virtual void finalize()
    {
        normalizePointsTo();
        PointerAnalysis::finalize();
    }
    /// Union/add points-to, and add the reverse points-to for node collapse purpose
    /// To be noted that adding reverse pts might incur 10% total overhead during solving
    //@{
    virtual inline bool unionPts(CVar id, const CPtSet& target)
    {
        return ptD->unionPts(id, target);
    }

    virtual inline bool unionPts(CVar id, CVar ptd)
    {
        return ptD->unionPts(id,ptd);
    }

    virtual inline bool addPts(CVar id, CVar ptd)
    {
        return ptD->addPts(id,ptd);
    }
    //@}

    /// Internal interface to be used for conditional points-to set queries
    //@{
    inline bool mustAlias(const CVar& var1, const CVar& var2)
    {
        if(isSameVar(var1,var2))
            return true;

        bool singleton = !(isHeapMemObj(var1.get_id()) || isLocalVarInRecursiveFun(var1.get_id()));
        if(isCondCompatible(var1.get_cond(),var2.get_cond(),singleton) == false)
            return false;

        const CPtSet& cpts1 = getPts(var1);
        const CPtSet& cpts2 = getPts(var2);
        return (contains(cpts1,cpts2) && contains(cpts2,cpts1));
    }

    //  Whether cpts1 contains all points-to targets of pts2
    bool contains(const CPtSet& cpts1, const CPtSet& cpts2)
    {
        if (cpts1.empty() || cpts2.empty())
            return false;

        for (typename CPtSet::const_iterator it2 = cpts2.begin(); it2 != cpts2.end(); ++it2)
        {
            bool hasObj = false;
            for (typename CPtSet::const_iterator it1 = cpts1.begin(); it1 != cpts1.end(); ++it1)
            {
                if(isSameVar(*it1,*it2))
                {
                    hasObj = true;
                    break;
                }
            }
            if(hasObj == false)
                return false;
        }
        return true;
    }

    /// Whether two pointers/objects are the same one by considering their conditions
    bool isSameVar(const CVar& var1, const CVar& var2) const
    {
        if(var1.get_id() != var2.get_id())
            return false;

        /// we distinguish context sensitive memory allocation here
        bool singleton = !(isHeapMemObj(var1.get_id()) || isLocalVarInRecursiveFun(var1.get_id()));
        return isCondCompatible(var1.get_cond(),var2.get_cond(),singleton);
    }
    //@}

    /// Normalize points-to information to BitVector/conditional representation
    virtual void normalizePointsTo()
    {
        normalized = true;
        if (hasPtsMap())
        {
            const typename MutPTDataTy::PtsMap& ptsMap = getPtsMap();
            for(typename MutPTDataTy::PtsMap::const_iterator it = ptsMap.begin(), eit=ptsMap.end(); it!=eit; ++it)
            {
                for(typename CPtSet::const_iterator cit = it->second.begin(), ecit=it->second.end(); cit!=ecit; ++cit)
                {
                    ptrToBVPtsMap[(it->first).get_id()].set(cit->get_id());
                    objToNSRevPtsMap[cit->get_id()].insert((it->first).get_id());
                    ptrToCPtsMap[(it->first).get_id()].set(*cit);
                }
            }
        }
        else
        {
            assert(false && "CondPTAImpl::NormalizePointsTo: could not normalize points-to sets");
        }
    }
    /// Points-to data
    PTDataTy* ptD;
    /// Normalized flag
    bool normalized;
    /// Normal points-to representation (without conditions)
    PtrToBVPtsMap ptrToBVPtsMap;
    /// Normal points-to representation (without conditions)
    PtrToNSMap objToNSRevPtsMap;
    /// Conditional points-to representation (with conditions)
    PtrToCPtsMap ptrToCPtsMap;
public:
    /// Print out conditional pts
    virtual void dumpCPts()
    {
        ptD->dumpPTData();
    }
    /// Given a conditional pts return its bit vector points-to
    virtual inline PointsTo getBVPointsTo(const CPtSet& cpts) const
    {
        PointsTo pts;
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit)
            pts.set(cit->get_id());
        return pts;
    }
    /// Given a pointer return its bit vector points-to
    virtual inline PointsTo& getPts(NodeID ptr)
    {
        assert(normalized && "Pts of all context-var have to be merged/normalized. Want to use getPts(CVar cvar)??");
        return ptrToBVPtsMap[ptr];
    }
    /// Given a pointer return its conditional points-to
    virtual inline const CPtSet& getCondPointsTo(NodeID ptr)
    {
        assert(normalized && "Pts of all context-vars have to be merged/normalized. Want to use getPts(CVar cvar)??");
        return ptrToCPtsMap[ptr];
    }
    /// Given an object return all pointers points to this object
    virtual inline NodeSet& getRevPts(NodeID obj)
    {
        assert(normalized && "Pts of all context-var have to be merged/normalized. Want to use getPts(CVar cvar)??");
        return objToNSRevPtsMap[obj];
    }

    /// Interface expose to users of our pointer analysis, given Value infos
    virtual inline AliasResult alias(const SVFValue* V1, const SVFValue* V2)
    {
        return  alias(pag->getValueNode(V1),pag->getValueNode(V2));
    }
    /// Interface expose to users of our pointer analysis, given two pointers
    virtual inline AliasResult alias(NodeID node1, NodeID node2)
    {
        return alias(getCondPointsTo(node1),getCondPointsTo(node2));
    }
    /// Interface expose to users of our pointer analysis, given conditional variables
    virtual AliasResult alias(const CVar& var1, const CVar& var2)
    {
        return alias(getPts(var1),getPts(var2));
    }
    /// Interface expose to users of our pointer analysis, given two conditional points-to sets
    virtual inline AliasResult alias(const CPtSet& pts1, const CPtSet& pts2)
    {
        CPtSet cpts1;
        expandFIObjs(pts1,cpts1);
        CPtSet cpts2;
        expandFIObjs(pts2,cpts2);
        if (containBlackHoleNode(cpts1) || containBlackHoleNode(cpts2))
            return AliasResult::MayAlias;
        else if(this->getAnalysisTy()==PathS_DDA && contains(cpts1,cpts2) && contains(cpts2,cpts1))
        {
            return AliasResult::MustAlias;
        }
        else if(overlap(cpts1,cpts2))
            return AliasResult::MayAlias;
        else
            return AliasResult::NoAlias;
    }
    /// Test blk node for cpts
    inline bool containBlackHoleNode(const CPtSet& cpts)
    {
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit)
        {
            if(cit->get_id() == pag->getBlackHoleNode())
                return true;
        }
        return false;
    }
    /// Test constant node for cpts
    inline bool containConstantNode(const CPtSet& cpts)
    {
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit)
        {
            if(cit->get_id() == pag->getConstantNode())
                return true;
        }
        return false;
    }
    /// Whether two conditions are compatible (to be implemented by child class)
    virtual bool isCondCompatible(const Cond& cxt1, const Cond& cxt2, bool singleton) const = 0;

    /// Dump points-to information of top-level pointers
    void dumpTopLevelPtsTo()
    {
        for (OrderedNodeSet::iterator nIter = this->getAllValidPtrs().begin(); nIter != this->getAllValidPtrs().end(); ++nIter)
        {
            const PAGNode* node = this->getPAG()->getGNode(*nIter);
            if (this->getPAG()->isValidTopLevelPtr(node))
            {
                if (SVFUtil::isa<DummyObjVar>(node))
                {
                    SVFUtil::outs() << "##<Blackhole or constant> id:" << node->getId();
                }
                else if (!SVFUtil::isa<DummyValVar>(node))
                {
                    SVFUtil::outs() << "##<" << node->getValue()->getName() << "> ";
                    //SVFUtil::outs() << "Source Loc: " << SVFUtil::getSourceLoc(node->getValue());
                }

                const PointsTo& pts = getPts(node->getId());
                SVFUtil::outs() << "\nNodeID " << node->getId() << " ";
                if (pts.empty())
                {
                    SVFUtil::outs() << "\t\tPointsTo: {empty}\n\n";
                }
                else
                {
                    SVFUtil::outs() << "\t\tPointsTo: { ";
                    for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit; ++it)
                        SVFUtil::outs() << *it << " ";
                    SVFUtil::outs() << "}\n\n";
                }
            }
        }

        SVFUtil::outs().flush();
    }
};

} // End namespace SVF

#endif /* INCLUDE_MEMORYMODEL_POINTERANALYSISIMPL_H_ */
