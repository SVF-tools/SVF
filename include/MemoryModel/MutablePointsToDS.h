/// PTData (AbstractPointsToDS.h) implementations with a mutable backend.
/// Each Key is given a points-to set which is itself updated till the analysis terminates.

#ifndef MUTABLE_POINTSTO_H_
#define MUTABLE_POINTSTO_H_

#include "MemoryModel/AbstractPointsToDS.h"
#include "Util/BasicTypes.h"
namespace SVF
{

template <typename Key, typename Datum, typename Data>
class MutableDFPTData;

/// PTData implemented using points-to sets which are created once and updated continuously.
template <typename Key, typename Datum, typename Data>
class MutablePTData : public PTData<Key, Datum, Data>
{
    friend class MutableDFPTData<Key, Datum, Data>;
public:
    using BasePTData = PTData<Key, Datum, Data>;
    using PTDataTy = typename BasePTData::PTDataTy;
    using KeySet = typename BasePTData::KeySet;

    using PtsMap = Map<Key, Data>;
    using RevPtsMap = Map<Datum, KeySet>;
    using PtsMapIter = typename PtsMap::iterator;
    using PtsMapConstIter = typename PtsMap::const_iterator;
    using iterator = typename Data::iterator;

    /// Constructor
    MutablePTData(bool reversePT = true, PTDataTy ty = PTDataTy::MutBase) : BasePTData(reversePT, ty) { }

    virtual ~MutablePTData() { }

    /// Return Points-to map
    virtual inline const PtsMap& getPtsMap() const
    {
        return ptsMap;
    }

    inline void clear() override
    {
        ptsMap.clear();
        revPtsMap.clear();
    }

    inline const Data& getPts(const Key& var) override
    {
        return ptsMap[var];
    }

    inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "MutablePTData::getRevPts: constructed without reverse PT support!");
        return revPtsMap[datum];
    }

    inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        addSingleRevPts(revPtsMap[element], dstKey);
        return addPts(ptsMap[dstKey], element);
    }

    inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        addRevPts(ptsMap[srcKey], dstKey);
        return unionPts(ptsMap[dstKey], getPts(srcKey));
    }

    inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        addRevPts(srcData,dstKey);
        return unionPts(ptsMap[dstKey], srcData);
    }

    inline void dumpPTData() override
    {
        dumpPts(ptsMap);
    }

    void clearPts(const Key& var, const Datum& element) override
    {
        ptsMap[var].reset(element);
    }

    void clearFullPts(const Key& var) override
    {
        ptsMap[var].clear();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutablePTData<Key, Datum, Data> * /*unused*/)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutBase;
    }
    ///@}

protected:
    virtual inline void dumpPts(const PtsMap & ptsSet,raw_ostream & O = SVFUtil::outs()) const
    {
        for (PtsMapConstIter nodeIt = ptsSet.begin(); nodeIt != ptsSet.end(); nodeIt++)
        {
            const Key& var = nodeIt->first;
            const Data & pts = nodeIt->second;
            if (pts.empty())
                continue;
            O << var << " ==> { ";
            for(typename Data::iterator cit = pts.begin(), ecit=pts.end(); cit!=ecit; ++cit)
            {
                O << *cit << " ";
            }
            O << "}\n";
        }
    }

private:
    /// Internal union/add points-to helper methods.
    ///@{
    inline bool unionPts(Data& dstData, const Data& srcData)
    {
        return dstData |= srcData;
    }
    inline bool addPts(Data &d, const Datum& e)
    {
        return d.test_and_set(e);
    }
    inline void addSingleRevPts(KeySet &revData, const Key& tgr)
    {
        if (this->rev) revData.insert(tgr);
    }
    inline void addRevPts(const Data &ptsData, const Key& tgr)
    {
        if (this->rev)
        {
            for(iterator it = ptsData.begin(), eit = ptsData.end(); it!=eit; ++it)
                addSingleRevPts(revPtsMap[*it], tgr);
        }
    }
    ///@}

protected:
    PtsMap ptsMap;
    RevPtsMap revPtsMap;
};

/// DiffPTData implemented with points-to sets which are updated continuously.
template <typename Key, typename Datum, typename Data>
class MutableDiffPTData : public DiffPTData<Key, Datum, Data>
{
public:
    using BasePTData = PTData<Key, Datum, Data>;
    using BaseDiffPTData = DiffPTData<Key, Datum, Data>;
    using PTDataTy = typename BasePTData::PTDataTy;
    using KeySet = typename BasePTData::KeySet;

    using PtsMap = typename MutablePTData<Key, Datum, Data>::PtsMap;

    /// Constructor
    MutableDiffPTData(bool reversePT = true, PTDataTy ty = PTDataTy::Diff) : BaseDiffPTData(reversePT, ty), mutPTData(reversePT) { }

    virtual ~MutableDiffPTData() { }

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    inline void clear() override
    {
        mutPTData.clear();
    }

    inline const Data& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "MutableDiffPTData::getRevPts: constructed without reverse PT support!");
        return mutPTData.getRevPts(datum);
    }

    inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        return mutPTData.addPts(dstKey, element);
    }

    inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return mutPTData.unionPts(dstKey, srcKey);
    }

    inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        return mutPTData.unionPts(dstKey, srcData);
    }

    void clearPts(const Key& var, const Datum& element) override
    {
        mutPTData.clearPts(var, element);
    }

    void clearFullPts(const Key& var) override
    {
        mutPTData.clearFullPts(var);
    }

    inline void dumpPTData() override
    {
        mutPTData.dumpPTData();
    }

    inline const Data &getDiffPts(Key &var) override
    {
        return getMutDiffPts(var);
    }

    inline bool computeDiffPts(Key &var, const Data &all) override
    {
        /// Clear diff pts.
        Data& diff = getMutDiffPts(var);
        diff.clear();
        /// Get all pts.
        Data& propa = getPropaPts(var);
        diff.intersectWithComplement(all, propa);
        propa = all;
        return !diff.empty();
    }

    inline void updatePropaPtsMap(Key &src, Key &dst) override
    {
        Data& srcPropa = getPropaPts(src);
        Data& dstPropa = getPropaPts(dst);
        dstPropa &= srcPropa;
    }

    inline void clearPropaPts(Key &var) override
    {
        getPropaPts(var).clear();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableDiffPTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutDiff;
    }
    ///@}

protected:
    /// Get diff PTS that can be modified.
    inline Data &getMutDiffPts(Key &var)
    {
        return diffPtsMap[var];
    }

    /// Get propagated points to.
    inline Data &getPropaPts(Key &var)
    {
        return propaPtsMap[var];
    }

private:
    /// Backing to implement the basic PTData methods. This allows us to avoid multiple-inheritance.
    MutablePTData<Key, Datum, Data> mutPTData;
    /// Diff points-to to be propagated.
    PtsMap diffPtsMap;
    /// Points-to already propagated.
    PtsMap propaPtsMap;
};

template <typename Key, typename Datum, typename Data>
class MutableDFPTData : public DFPTData<Key, Datum, Data>
{
public:
    using BasePTData = PTData<Key, Datum, Data>;
    using BaseMutPTData = MutablePTData<Key, Datum, Data>;
    using BaseDFPTData = DFPTData<Key, Datum, Data>;
    using PTDataTy = typename BasePTData::PTDataTy;
    using KeySet = typename BasePTData::KeySet;

    using LocID = typename BaseDFPTData::LocID;
    using PtsMap = typename BaseMutPTData::PtsMap;
    using PtsMapConstIter = typename BaseMutPTData::PtsMapConstIter;
    using DFPtsMap = Map<LocID, PtsMap>;	///< Data-flow point-to map
    using DFPtsMapIter = typename DFPtsMap::iterator;
    using DFPtsMapconstIter = typename DFPtsMap::const_iterator;

    /// Constructor
    MutableDFPTData(bool reversePT = true, PTDataTy ty = BaseDFPTData::MutDataFlow) : BaseDFPTData(reversePT, ty), mutPTData(reversePT) { }

    virtual ~MutableDFPTData() { }

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    inline void clear() override
    {
        mutPTData.clear();
    }

    inline const Data& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "MutableDFPTData::getRevPts: constructed without reverse PT support!");
        return mutPTData.getRevPts(datum);
    }

    inline bool hasDFInSet(LocID loc) const override
    {
        return (dfInPtsMap.find(loc) != dfInPtsMap.end());
    }

    inline bool hasDFOutSet(LocID loc) const override
    {
        return (dfOutPtsMap.find(loc) != dfOutPtsMap.end());
    }

    inline bool hasDFInSet(LocID loc,const Key& var) const override
    {
        DFPtsMapconstIter it = dfInPtsMap.find(loc);
        if ( it == dfInPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }

    inline bool hasDFOutSet(LocID loc, const Key& var) const override
    {
        DFPtsMapconstIter it = dfOutPtsMap.find(loc);
        if ( it == dfOutPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }

    inline Data& getDFInPtsSet(LocID loc, const Key& var) override
    {
        PtsMap& inSet = dfInPtsMap[loc];
        return inSet[var];
    }

    inline Data& getDFOutPtsSet(LocID loc, const Key& var) override
    {
        PtsMap& outSet = dfOutPtsMap[loc];
        return outSet[var];
    }

    /// Get internal flow-sensitive data structures.
    ///@{
    inline const PtsMap& getDFInPtsMap(LocID loc)
    {
        return dfInPtsMap[loc];
    }
    inline const PtsMap& getDFOutPtsMap(LocID loc)
    {
        return dfOutPtsMap[loc];
    }
    inline const DFPtsMap& getDFIn()
    {
        return dfInPtsMap;
    }
    inline const DFPtsMap& getDFOut()
    {
        return dfOutPtsMap;
    }
    ///@}

    inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }

    inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFOutPtsSet(srcLoc,srcVar));
    }

    inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFOutPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }

    inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->updateDFInFromOut(srcLoc,srcVar,dstLoc,dstVar);
    }

    inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->updateDFInFromIn(srcLoc,srcVar,dstLoc,dstVar);
    }

    inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) override
    {
        bool changed = false;
        if (this->hasDFInSet(loc))
        {
            /// Only variables has new pts from IN set need to be updated.
            const PtsMap & ptsMap = getDFInPtsMap(loc);
            for (typename PtsMap::const_iterator ptsIt = ptsMap.begin(), ptsEit = ptsMap.end(); ptsIt != ptsEit; ++ptsIt)
            {
                const Key var = ptsIt->first;
                /// Enable strong updates if it is required to do so
                if (strongUpdates && var == singleton)
                    continue;
                if (updateDFOutFromIn(loc, var, loc, var))
                    changed = true;
            }
        }
        return changed;
    }

    inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) override
    {
        return this->unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
    }

    inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->getPts(srcVar)));
    }

    inline void clearAllDFOutUpdatedVar(LocID) override
    {
    }

    /// Override the methods defined in PTData.
    /// Union/add points-to without adding reverse points-to, used internally
    ///@{
    inline bool addPts(const Key &dstKey, const Key& srcKey) override
    {
        return addPts(mutPTData.ptsMap[dstKey], srcKey);
    }
    inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return unionPts(mutPTData.ptsMap[dstKey], getPts(srcKey));
    }
    inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        return unionPts(mutPTData.ptsMap[dstKey],srcData);
    }
    void clearPts(const Key& var, const Datum& element) override
    {
        mutPTData.clearPts(var, element);
    }
    void clearFullPts(const Key& var) override
    {
        mutPTData.clearFullPts(var);
    }
    ///@}

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableDFPTData<Key, Datum, Data> *)
    {
        return true;
    }
    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == BaseDFPTData::MutDataFlow
               || ptd->getPTDTY() == BaseDFPTData::IncMutDataFlow;
    }
    ///@}

protected:
    /// Internal union/add points-to helper methods.
    ///@{
    inline bool unionPts(Data& dstData, const Data& srcData)
    {
        return dstData |= srcData;
    }
    inline bool addPts(Data &d, const Datum& e)
    {
        return d.test_and_set(e);
    }
    ///@}

public:
    /// Dump the DF IN/OUT set information for debugging purpose
    ///@{
    inline void dumpPTData() override
    {
        /// dump points-to of top-level pointers
        mutPTData.dumpPTData();
        /// dump points-to of address-taken variables
        std::error_code ErrInfo;
        ToolOutputFile F("svfg_pts.data", ErrInfo, llvm::sys::fs::F_None);
        if (!ErrInfo)
        {
            raw_fd_ostream & osm = F.os();
            NodeBS locs;
            for(DFPtsMapconstIter it = dfInPtsMap.begin(), eit = dfInPtsMap.end(); it!=eit; ++it)
                locs.set(it->first);

            for(DFPtsMapconstIter it = dfOutPtsMap.begin(), eit = dfOutPtsMap.end(); it!=eit; ++it)
                locs.set(it->first);

            for (auto loc : locs)
            {
                 if (this->hasDFInSet(loc))
                {
                    osm << "Loc:" << loc << " IN:{";
                    this->dumpPts(this->getDFInPtsMap(loc), osm);
                    osm << "}\n";
                }

                if (this->hasDFOutSet(loc))
                {
                    osm << "Loc:" << loc << " OUT:{";
                    this->dumpPts(this->getDFOutPtsMap(loc), osm);
                    osm << "}\n";
                }
            }
            F.os().close();
            if (!F.os().has_error())
            {
                SVFUtil::outs() << "\n";
                F.keep();
                return;
            }
        }
        SVFUtil::outs() << "  error opening file for writing!\n";
        F.os().clear_error();
    }

    virtual inline void dumpPts(const PtsMap & ptsSet,raw_ostream & O = SVFUtil::outs()) const
    {
        for (PtsMapConstIter nodeIt = ptsSet.begin(); nodeIt != ptsSet.end(); nodeIt++)
        {
            const Key& var = nodeIt->first;
            const Data & pts = nodeIt->second;
            if (pts.empty())
                continue;
            O << "<" << var << ",{";
            SVFUtil::dumpSet(pts,O);
            O << "}> ";
        }
    }
    ///@}

protected:
    /// Data-flow IN set.
    DFPtsMap dfInPtsMap;
    /// Data-flow OUT set.
    DFPtsMap dfOutPtsMap;
    /// Backing to implement the basic PTData methods which are not overridden.
    /// This allows us to avoid multiple-inheritance.
    MutablePTData<Key, Datum, Data> mutPTData;
};

/// Incremental version of the mutable data-flow points-to data structure.
template <typename Key, typename Datum, typename Data>
class IncMutableDFPTData : public MutableDFPTData<Key, Datum, Data>
{
public:
    using BasePTData = PTData<Key, Datum, Data>;
    using BaseMutPTData = MutablePTData<Key, Datum, Data>;
    using BaseDFPTData = DFPTData<Key, Datum, Data>;
    using BaseMutDFPTData = MutableDFPTData<Key, Datum, Data>;
    using PTDataTy = typename BasePTData::PTDataTy;

    using LocID = typename BaseDFPTData::LocID;
    using UpdatedVarMap = Map<LocID, Data>;	///< for propagating only newly added variable in IN/OUT set
    using UpdatedVarMapIter = typename UpdatedVarMap::iterator;
    using UpdatedVarconstIter = typename UpdatedVarMap::const_iterator;
    using DataIter = typename Data::iterator;

private:
    UpdatedVarMap outUpdatedVarMap;
    UpdatedVarMap inUpdatedVarMap;

public:
    /// Constructor
    IncMutableDFPTData(bool reversePT = true, PTDataTy ty = BasePTData::IncMutDataFlow) : BaseMutDFPTData(reversePT, ty) { }

    virtual ~IncMutableDFPTData() { }

    inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(varHasNewDFInPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(varHasNewDFOutPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(varHasNewDFInPts(srcLoc,srcVar))
        {
            removeVarFromDFInUpdatedSet(srcLoc,srcVar);
            if (this->unionPts(this->getDFOutPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar)))
            {
                setVarDFOutSetUpdated(dstLoc,dstVar);
                return true;
            }
        }
        return false;
    }

    inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) override
    {
        bool changed = false;
        if (this->hasDFInSet(loc))
        {
            /// Only variables has new pts from IN set need to be updated.
            Data pts = getDFInUpdatedVar(loc);
            for (DataIter ptsIt = pts.begin(), ptsEit = pts.end(); ptsIt != ptsEit; ++ptsIt)
            {
                const Key var = *ptsIt;
                /// Enable strong updates if it is required to do so
                if (strongUpdates && var == singleton)
                    continue;
                if (updateDFOutFromIn(loc, var, loc, var))
                    changed = true;
            }
        }
        return changed;
    }

    inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) override
    {
        if(varHasNewDFInPts(srcLoc,srcVar))
        {
            removeVarFromDFInUpdatedSet(srcLoc,srcVar);
            return this->mutPTData.unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
        }
        return false;
    }

    inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->mutPTData.getPts(srcVar)))
        {
            setVarDFOutSetUpdated(dstLoc, dstVar);
            return true;
        }
        return false;
    }

    inline void clearAllDFOutUpdatedVar(LocID loc) override
    {
        if (this->hasDFOutSet(loc))
        {
            Data pts = getDFOutUpdatedVar(loc);
            for (DataIter ptsIt = pts.begin(), ptsEit = pts.end(); ptsIt != ptsEit; ++ptsIt)
            {
                const Key var = *ptsIt;
                removeVarFromDFOutUpdatedSet(loc, var);
            }
        }
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const IncMutableDFPTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == BasePTData::IncMutDataFlow;
    }
    ///@}
private:
    /// Handle address-taken variables whose IN pts changed
    //@{
    /// Add var into loc's IN updated set. Called when var's pts in loc's IN set changed
    inline void setVarDFInSetUpdated(LocID loc,const Key& var)
    {
        inUpdatedVarMap[loc].set(var);
    }
    /// Remove var from loc's IN updated set
    inline void removeVarFromDFInUpdatedSet(LocID loc,const Key& var)
    {
        UpdatedVarMapIter it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end())
            it->second.reset(var);
    }
    /// Return TRUE if var has new pts in loc's IN set
    inline bool varHasNewDFInPts(LocID loc,const Key& var)
    {
        UpdatedVarMapIter it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end())
            return it->second.test(var);
        return false;
    }
    /// Get all var which have new pts informationin loc's IN set
    inline const Data& getDFInUpdatedVar(LocID loc)
    {
        return inUpdatedVarMap[loc];
    }
    //@}

    /// Handle address-taken variables whose OUT pts changed
    //@{
    /// Add var into loc's OUT updated set. Called when var's pts in loc's OUT set changed
    inline void setVarDFOutSetUpdated(LocID loc,const Key& var)
    {
        outUpdatedVarMap[loc].set(var);
    }
    /// Remove var from loc's OUT updated set
    inline void removeVarFromDFOutUpdatedSet(LocID loc,const Key& var)
    {
        UpdatedVarMapIter it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end())
            it->second.reset(var);
    }
    /// Return TRUE if var has new pts in loc's OUT set
    inline bool varHasNewDFOutPts(LocID loc,const Key& var)
    {
        UpdatedVarMapIter it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end())
            return it->second.test(var);
        return false;
    }
    /// Get all var which have new pts informationin loc's OUT set
    inline const Data& getDFOutUpdatedVar(LocID loc)
    {
        return outUpdatedVarMap[loc];
    }
    //@}
};

/// VersionedPTData implemented with mutable points-to set (Data).
/// Implemented as a wrapper around two MutablePTDatas: one for Keys, one
/// for VersionedKeys.
template <typename Key, typename Datum, typename Data, typename VersionedKey>
class MutableVersionedPTData : public VersionedPTData<Key, Datum, Data, VersionedKey>
{
public:
    using BasePTData = PTData<Key, Datum, Data>;
    using BaseVersionedPTData = VersionedPTData<Key, Datum, Data, VersionedKey>;
    using PTDataTy = typename BasePTData::PTDataTy;
    using KeySet = typename BasePTData::KeySet;
    using VersionedKeySet = typename BaseVersionedPTData::VersionedKeySet;

    MutableVersionedPTData(bool reversePT = true, PTDataTy ty = PTDataTy::MutVersioned)
        : BaseVersionedPTData(reversePT, ty), tlPTData(reversePT), atPTData(reversePT) { }

    virtual ~MutableVersionedPTData() { }

    inline void clear() override
    {
        tlPTData.clear();
        atPTData.clear();
    }

    const Data& getPts(const Key& vk) override
    {
        return tlPTData.getPts(vk);
    }
    const Data& getPts(const VersionedKey& vk) override
    {
        return atPTData.getPts(vk);
    }

    const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "MutableVersionedPTData::getRevPts: constructed without reverse PT support!");
        return tlPTData.getRevPts(datum);
    }
    const VersionedKeySet& getVersionedKeyRevPts(const Datum& datum) override
    {
        assert(this->rev && "MutableVersionedPTData::getVersionedKeyRevPts: constructed without reverse PT support!");
        return atPTData.getRevPts(datum);
    }

    bool addPts(const Key& k, const Datum& element) override
    {
        return tlPTData.addPts(k, element);
    }
    bool addPts(const VersionedKey& vk, const Datum& element) override
    {
        return atPTData.addPts(vk, element);
    }

    bool unionPts(const Key& dstVar, const Key& srcVar) override
    {
        return tlPTData.unionPts(dstVar, srcVar);
    }
    bool unionPts(const VersionedKey& dstVar, const VersionedKey& srcVar) override
    {
        return atPTData.unionPts(dstVar, srcVar);
    }
    bool unionPts(const VersionedKey& dstVar, const Key& srcVar) override
    {
        return atPTData.unionPts(dstVar, tlPTData.getPts(srcVar));
    }
    bool unionPts(const Key& dstVar, const VersionedKey& srcVar) override
    {
        return tlPTData.unionPts(dstVar, atPTData.getPts(srcVar));
    }
    bool unionPts(const Key& dstVar, const Data& srcData) override
    {
        return tlPTData.unionPts(dstVar, srcData);
    }
    bool unionPts(const VersionedKey& dstVar, const Data& srcData) override
    {
        return atPTData.unionPts(dstVar, srcData);
    }

    void clearPts(const Key& k, const Datum& element) override
    {
        tlPTData.clearPts(k, element);
    }
    void clearPts(const VersionedKey& vk, const Datum& element) override
    {
        atPTData.clearPts(vk, element);
    }

    void clearFullPts(const Key& k) override
    {
        tlPTData.clearFullPts(k);
    }
    void clearFullPts(const VersionedKey& vk) override
    {
        atPTData.clearFullPts(vk);
    }

    inline void dumpPTData() override
    {
        SVFUtil::outs() << "== Top-level points-to information\n";
        tlPTData.dumpPTData();
        SVFUtil::outs() << "== Address-taken points-to information\n";
        atPTData.dumpPTData();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableVersionedPTData<Key, Datum, Data, VersionedKey> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutVersioned;
    }
    ///@}

private:
    /// PTData for Keys (top-level pointers, generally).
    MutablePTData<Key, Datum, Data> tlPTData;
    /// PTData for VersionedKeys (address-taken objects, generally).
    MutablePTData<VersionedKey, Datum, Data> atPTData;
};

} // End namespace SVF

#endif  // MUTABLE_POINTSTO_H_
