/// PTData (AbstractPointsToDS.h) implementations with a mutable backend.
/// Each Key is given a points-to set which is itself updated till the analysis terminates.

#ifndef MUTABLE_POINTSTO_H_
#define MUTABLE_POINTSTO_H_

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
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef std::map<const Key, Data> PtsMap;
    typedef typename PtsMap::iterator PtsMapIter;
    typedef typename PtsMap::const_iterator PtsMapConstIter;
    typedef typename Data::iterator iterator;

    /// Constructor
    MutablePTData(PTDataTy ty = PTDataTy::MutBase) : BasePTData(ty) { }

    virtual ~MutablePTData() { }

    /// Return Points-to map
    virtual inline const PtsMap& getPtsMap() const
    {
        return ptsMap;
    }

    virtual inline void clear() override
    {
        ptsMap.clear();
        revPtsMap.clear();
    }

    virtual inline const Data& getPts(const Key& var) override
    {
        return ptsMap[var];
    }

    virtual inline const Data& getRevPts(const Key& var) override
    {
        return revPtsMap[var];
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        addSingleRevPts(revPtsMap[element], dstKey);
        return addPts(ptsMap[dstKey], element);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        addRevPts(ptsMap[srcKey], dstKey);
        return unionPts(ptsMap[dstKey], getPts(srcKey));
    }

    virtual inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        addRevPts(srcData,dstKey);
        return unionPts(ptsMap[dstKey], srcData);
    }

    virtual inline void dumpPTData() override
    {
        dumpPts(ptsMap);
    }

    virtual void clearPts(const Key& var, const Datum& element)
    {
        ptsMap[var].reset(element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        ptsMap[var].clear();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutablePTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutBase || ptd->getPTDTY() == PTDataTy::MutDiff;
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
    inline void addSingleRevPts(Data &revData, const Datum& tgr)
    {
        addPts(revData,tgr);
    }
    inline void addRevPts(const Data &ptsData, const Datum& tgr)
    {
        for(iterator it = ptsData.begin(), eit = ptsData.end(); it!=eit; ++it)
            addSingleRevPts(revPtsMap[*it], tgr);
    }
    ///@}

protected:
    PtsMap ptsMap;
    PtsMap revPtsMap;
};

/// DiffPTData implemented with points-to sets which are updated continuously.
/// CachePtsMap is an additional map which maintains cached points-to information.
template <typename Key, typename Datum, typename Data, typename CacheKey>
class MutableDiffPTData : public DiffPTData<Key, Datum, Data, CacheKey>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef DiffPTData<Key, Datum, Data, CacheKey> BaseDiffPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename MutablePTData<Key, Datum, Data>::PtsMap PtsMap;
    typedef typename MutablePTData<CacheKey, Datum, Data>::PtsMap CachePtsMap;

    /// Constructor
    MutableDiffPTData(PTDataTy ty = PTDataTy::Diff) : BaseDiffPTData(ty) { }

    virtual ~MutableDiffPTData() { }

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    virtual inline void clear() override
    {
        mutPTData.clear();
    }

    virtual inline const Data& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    virtual inline const Data& getRevPts(const Key& var) override
    {
        return mutPTData.getRevPts(var);
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        return mutPTData.addPts(dstKey, element);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return mutPTData.unionPts(dstKey, srcKey);
    }

    virtual inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        return mutPTData.unionPts(dstKey, srcData);
    }

    virtual void clearPts(const Key& var, const Datum& element) override
    {
        mutPTData.clearPts(var, element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        mutPTData.clearFullPts(var);
    }

    virtual inline void dumpPTData() override
    {
        mutPTData.dumpPTData();
    }

    virtual inline Data &getDiffPts(Key &var) override
    {
        return diffPtsMap[var];
    }

    virtual inline Data &getPropaPts(Key &var) override
    {
        return propaPtsMap[var];
    }

    virtual inline bool computeDiffPts(Key &var, const Data &all) override
    {
        /// Clear diff pts.
        Data& diff = getDiffPts(var);
        diff.clear();
        /// Get all pts.
        Data& propa = getPropaPts(var);
        diff.intersectWithComplement(all, propa);
        propa = all;
        return !diff.empty();
    }

    virtual inline void updatePropaPtsMap(Key &src, Key &dst) override
    {
        Data& srcPropa = getPropaPts(src);
        Data& dstPropa = getPropaPts(dst);
        dstPropa &= srcPropa;
    }

    virtual inline void clearPropaPts(Key &var) override
    {
        getPropaPts(var).clear();
    }

    virtual inline Data& getCachePts(CacheKey &cache) override
    {
        return cacheMap[cache];
    }

    virtual inline void addCachePts(CacheKey &cache, Data &data) override
    {
        cacheMap[cache] |= data;
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableDiffPTData<Key, Datum, Data, CacheKey> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutDiff;
    }
    ///@}

private:
    /// Backing to implement the basic PTData methods. This allows us to avoid multiple-inheritance.
    MutablePTData<Key, Datum, Data> mutPTData;
    /// Diff points-to to be propagated.
    PtsMap diffPtsMap;
    /// Points-to already propagated.
    PtsMap propaPtsMap;
    /// Points-to processed at load/store edges.
    CachePtsMap cacheMap;
};

template <typename Key, typename Datum, typename Data>
class MutableDFPTData : public DFPTData<Key, Datum, Data>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef MutablePTData<Key, Datum, Data> BaseMutPTData;
    typedef DFPTData<Key, Datum, Data> BaseDFPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BaseDFPTData::LocID LocID;
    typedef typename BaseMutPTData::PtsMap PtsMap;
    typedef typename BaseMutPTData::PtsMapConstIter PtsMapConstIter;
    typedef DenseMap<LocID, PtsMap> DFPtsMap;	///< Data-flow point-to map
    typedef typename DFPtsMap::iterator DFPtsMapIter;
    typedef typename DFPtsMap::const_iterator DFPtsMapconstIter;

    /// Constructor
    MutableDFPTData(PTDataTy ty = BaseDFPTData::MutDataFlow) : BaseDFPTData(ty) { }

    virtual ~MutableDFPTData() { }

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    virtual inline void clear() override
    {
        mutPTData.clear();
    }

    virtual inline const Data& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    virtual inline const Data& getRevPts(const Key& var) override
    {
        return mutPTData.getRevPts(var);
    }

    virtual inline bool hasDFInSet(LocID loc) const override
    {
        return (dfInPtsMap.find(loc) != dfInPtsMap.end());
    }

    virtual inline bool hasDFOutSet(LocID loc) const override
    {
        return (dfOutPtsMap.find(loc) != dfOutPtsMap.end());
    }

    virtual inline bool hasDFInSet(LocID loc,const Key& var) const override
    {
        DFPtsMapconstIter it = dfInPtsMap.find(loc);
        if ( it == dfInPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }

    virtual inline bool hasDFOutSet(LocID loc, const Key& var) const override
    {
        DFPtsMapconstIter it = dfOutPtsMap.find(loc);
        if ( it == dfOutPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }

    virtual inline Data& getDFInPtsSet(LocID loc, const Key& var) override
    {
        PtsMap& inSet = dfInPtsMap[loc];
        return inSet[var];
    }

    virtual inline Data& getDFOutPtsSet(LocID loc, const Key& var) override
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

    virtual inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }

    virtual inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFOutPtsSet(srcLoc,srcVar));
    }

    virtual inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->unionPts(getDFOutPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }

    virtual inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->updateDFInFromOut(srcLoc,srcVar,dstLoc,dstVar);
    }

    virtual inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return this->updateDFInFromIn(srcLoc,srcVar,dstLoc,dstVar);
    }

    virtual inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) override
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

    virtual inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) override
    {
        return this->unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
    }

    virtual inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->getPts(srcVar)));
    }

    virtual inline void clearAllDFOutUpdatedVar(LocID) override
    {
    }

    /// Override the methods defined in PTData.
    /// Union/add points-to without adding reverse points-to, used internally
    ///@{
    virtual inline bool addPts(const Key &dstKey, const Key& srcKey) override
    {
        return addPts(mutPTData.ptsMap[dstKey], srcKey);
    }
    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return unionPts(mutPTData.ptsMap[dstKey], getPts(srcKey));
    }
    virtual inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        return unionPts(mutPTData.ptsMap[dstKey],srcData);
    }
    virtual void clearPts(const Key& var, const Datum& element) override
    {
        mutPTData.clearPts(var, element);
    }
    virtual void clearFullPts(const Key& var) override
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
    virtual inline void dumpPTData() override
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

            for (NodeBS::iterator it = locs.begin(), eit = locs.end(); it != eit; it++)
            {
                LocID loc = *it;
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
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef MutablePTData<Key, Datum, Data> BaseMutPTData;
    typedef DFPTData<Key, Datum, Data> BaseDFPTData;
    typedef MutableDFPTData<Key, Datum, Data> BaseMutDFPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BaseDFPTData::LocID LocID;
    typedef DenseMap<LocID, Data> UpdatedVarMap;	///< for propagating only newly added variable in IN/OUT set
    typedef typename UpdatedVarMap::iterator UpdatedVarMapIter;
    typedef typename UpdatedVarMap::const_iterator UpdatedVarconstIter;
    typedef typename Data::iterator DataIter;

private:
    UpdatedVarMap outUpdatedVarMap;
    UpdatedVarMap inUpdatedVarMap;

public:
    /// Constructor
    IncMutableDFPTData(PTDataTy ty = BasePTData::IncMutDataFlow) : BaseMutDFPTData(ty) { }

    virtual ~IncMutableDFPTData() { }

    virtual inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(varHasNewDFInPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    virtual inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(varHasNewDFOutPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    virtual inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
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

    virtual inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    virtual inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar)))
        {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }

    virtual inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) override
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

    virtual inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) override
    {
        if(varHasNewDFInPts(srcLoc,srcVar))
        {
            removeVarFromDFInUpdatedSet(srcLoc,srcVar);
            return this->mutPTData.unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
        }
        return false;
    }

    virtual inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->mutPTData.getPts(srcVar)))
        {
            setVarDFOutSetUpdated(dstLoc, dstVar);
            return true;
        }
        return false;
    }

    virtual inline void clearAllDFOutUpdatedVar(LocID loc) override
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

} // End namespace SVF

#endif  // MUTABLE_POINTSTO_H_
