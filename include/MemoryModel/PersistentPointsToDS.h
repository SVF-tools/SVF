/// PTData (AbstractPointsToDS.h) implementations with a persistent backend.
/// Each Key is given a cheap points-to ID which refers to some real points-to set.

#ifndef PERSISTENT_POINTSTO_H_
#define PERSISTENT_POINTSTO_H_

#include "MemoryModel/AbstractPointsToDS.h"
#include "MemoryModel/PersistentPointsToCache.h"
#include "Util/PointsTo.h"
#include "Util/SVFUtil.h"

namespace SVF
{

template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentDFPTData;
template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentIncDFPTData;
template <typename Key, typename KeySet, typename Datum, typename DataSet, typename VersionedKey, typename VersionedKeySet>
class PersistentVersionedPTData;

/// PTData backed by a PersistentPointsToCache.
template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentPTData : public PTData<Key, KeySet, Datum, DataSet>
{
    template <typename K, typename KS, typename D, typename DS, typename VK, typename VKS>
    friend class PersistentVersionedPTData;
    friend class PersistentDFPTData<Key, KeySet, Datum, DataSet>;
    friend class PersistentIncDFPTData<Key, KeySet, Datum, DataSet>;
public:
    typedef PTData<Key, KeySet, Datum, DataSet> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef Map<Key, PointsToID> KeyToIDMap;
    typedef Map<Datum, KeySet> RevPtsMap;

    /// Constructor
    PersistentPTData(PersistentPointsToCache<DataSet> &cache, bool reversePT = true, PTDataTy ty = PTDataTy::PersBase)
        : BasePTData(reversePT, ty), ptCache(cache) { }

    virtual ~PersistentPTData() { }

    virtual inline void clear() override
    {
        ptsMap.clear();
        revPtsMap.clear();
    }

    virtual inline const DataSet& getPts(const Key& var) override
    {
        PointsToID id = ptsMap[var];
        return ptCache.getActualPts(id);
    }

    virtual inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "PersistentPTData::getRevPts: constructed without reverse PT support!");
        return revPtsMap[datum];
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        DataSet srcPts(BasePTData::defaultData);
        srcPts.set(element);
        PointsToID srcId = ptCache.emplacePts(srcPts);
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        PointsToID srcId = ptsMap[srcKey];
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcData) override
    {
        PointsToID srcId = ptCache.emplacePts(srcData);
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline void dumpPTData() override
    {
    }

    virtual void clearPts(const Key& var, const Datum& element) override
    {
        DataSet toRemoveData(BasePTData::defaultData);
        toRemoveData.set(element);
        PointsToID toRemoveId = ptCache.emplacePts(toRemoveData);
        PointsToID varId = ptsMap[var];
        PointsToID complementId = ptCache.complementPts(varId, toRemoveId);
        if (varId != complementId)
        {
            ptsMap[var] = complementId;
            //revPtsMap[element].erase(var);
        }
    }

    virtual void clearFullPts(const Key& var) override
    {
        ptsMap[var] = PersistentPointsToCache<DataSet>::emptyPointsToId();
    }

    virtual void setDefaultData(const DataSet &dataSet) override
    {
        BasePTData::setDefaultData(dataSet);
        ptCache.setDefaultData(dataSet);
    }

    virtual Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        Map<DataSet, unsigned> allPts;
        if (liveOnly)
        {
            for (const typename KeyToIDMap::value_type &ki : ptsMap)
            {
                ++allPts[ptCache.getActualPts(ki.second)];
            }
        }
        else
        {
            allPts = ptCache.getAllPts();
        }

        return allPts;
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentPTData<Key, KeySet, Datum, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Datum, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::PersBase;
    }
    ///@}

private:
    /// Internal unionPts since other methods follow the same pattern.
    /// Renamed because PointsToID and Key may be the same type...
    inline bool unionPtsFromId(const Key &dstKey, PointsToID srcId)
    {
        PointsToID dstId = ptsMap[dstKey];
        PointsToID newDstId = ptCache.unionPts(dstId, srcId);

        bool changed = newDstId != dstId;
        if (changed)
        {
            ptsMap[dstKey] = newDstId;

            // Reverse points-to only needs to be handled when dst's
            // points-to set has changed (i.e., do it the first time only).
            if (this->rev)
            {
                const DataSet &srcPts = ptCache.getActualPts(srcId);
                for (const Datum &d : srcPts) insertKey(dstKey, revPtsMap[d]);
            }
        }

        return changed;
    }

protected:
    PersistentPointsToCache<DataSet> &ptCache;
    KeyToIDMap ptsMap;
    RevPtsMap revPtsMap;
};

/// DiffPTData implemented with a persistent points-to backing.
template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentDiffPTData : public DiffPTData<Key, KeySet, Datum, DataSet>
{
public:
    typedef PTData<Key, KeySet, Datum, DataSet> BasePTData;
    typedef DiffPTData<Key, KeySet, Datum, DataSet> BaseDiffPTData;
    typedef PersistentPTData<Key, KeySet, Datum, DataSet> BasePersPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BasePersPTData::KeyToIDMap KeyToIDMap;
    typedef typename BasePersPTData::RevPtsMap RevPtsMap;

    /// Constructor
    PersistentDiffPTData(PersistentPointsToCache<DataSet> &cache, bool reversePT = true, PTDataTy ty = PTDataTy::PersDiff)
        : BaseDiffPTData(reversePT, ty), persPTData(cache, reversePT), ptCache(cache) { }

    virtual ~PersistentDiffPTData() { }

    virtual inline void clear() override
    {
        persPTData.clear();
        diffPtsMap.clear();
        propaPtsMap.clear();
    }

    virtual inline const DataSet& getPts(const Key& var) override
    {
        return persPTData.getPts(var);
    }

    virtual inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "PersistentDiffPTData::getRevPts: constructed without reverse PT support!");
        return persPTData.getRevPts(datum);
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        return persPTData.addPts(dstKey, element);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return persPTData.unionPts(dstKey, srcKey);
    }

    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcDataSet) override
    {
        return persPTData.unionPts(dstKey, srcDataSet);
    }

    virtual void clearPts(const Key& var, const Datum& element) override
    {
        return persPTData.clearPts(var, element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        return persPTData.clearFullPts(var);
    }

    virtual inline void dumpPTData() override
    {
        // TODO.
    }

    virtual inline const DataSet &getDiffPts(Key &var) override
    {
        PointsToID id = diffPtsMap[var];
        return ptCache.getActualPts(id);
    }

    virtual inline bool computeDiffPts(Key &var, const DataSet &all) override
    {
        PointsToID propaId = propaPtsMap[var];
        PointsToID allId = ptCache.emplacePts(all);
        // Diff is made up of the entire points-to set minus what has been propagated.
        PointsToID diffId = ptCache.complementPts(allId, propaId);
        diffPtsMap[var] = diffId;

        // We've now propagated the entire thing.
        propaPtsMap[var] = allId;

        // Whether diff is empty or not; just need to check against the ID since it
        // is the only empty set.
        return diffId != ptCache.emptyPointsToId();
    }

    virtual inline void updatePropaPtsMap(Key &src, Key &dst) override
    {
        PointsToID dstId = propaPtsMap[dst];
        PointsToID srcId = propaPtsMap[src];
        propaPtsMap[dst] = ptCache.intersectPts(dstId, srcId);
    }

    virtual inline void clearPropaPts(Key &var) override
    {
        propaPtsMap[var] = ptCache.emptyPointsToId();
    }

    virtual void setDefaultData(const DataSet &dataSet) override
    {
        BasePTData::setDefaultData(dataSet);
        persPTData.setDefaultData(dataSet);
        ptCache.setDefaultData(dataSet);
    }

    virtual Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        return persPTData.getAllPts(liveOnly);
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentDiffPTData<Key, KeySet, Datum, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Datum, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::PersDiff;
    }
    ///@}

private:
    PersistentPointsToCache<DataSet> &ptCache;
    /// Backing to implement basic PTData methods. Allows us to avoid multiple inheritance.
    PersistentPTData<Key, KeySet, Datum, DataSet> persPTData;
    /// Diff points-to to be propagated.
    KeyToIDMap diffPtsMap;
    /// Points-to already propagated.
    KeyToIDMap propaPtsMap;
};

/// DFPTData backed by a PersistentPointsToCache.
template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentDFPTData : public DFPTData<Key, KeySet, Datum, DataSet>
{
public:
    typedef PTData<Key, KeySet, Datum, DataSet> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;
    typedef DFPTData<Key, KeySet, Datum, DataSet> BaseDFPTData;
    typedef PersistentPTData<Key, KeySet, Datum, DataSet> BasePersPTData;

    typedef typename BaseDFPTData::LocID LocID;
    typedef typename BasePersPTData::KeyToIDMap KeyToIDMap;
    typedef Map<LocID, KeyToIDMap> DFKeyToIDMap;

    PersistentDFPTData(PersistentPointsToCache<DataSet> &cache, bool reversePT = true, PTDataTy ty = PTDataTy::PersDataFlow)
        : BaseDFPTData(reversePT, ty), ptCache(cache), persPTData(cache, reversePT) { }

    virtual ~PersistentDFPTData() { }

    virtual inline void clear() override
    {
        // TODO: clear dfIn/OutPtsMaps.
        persPTData.clear();
    }

    virtual inline const DataSet& getPts(const Key& var) override
    {
        return persPTData.getPts(var);
    }

    virtual inline const KeySet& getRevPts(const Datum& datum) override
    {
        assert(false && "PersistentDFPTData::getRevPts: not supported yet!");
        exit(1);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return persPTData.unionPts(dstKey, srcKey);
    }

    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcDataSet) override
    {
        return persPTData.unionPts(dstKey, srcDataSet);
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        return persPTData.addPts(dstKey, element);
    }

    virtual void clearPts(const Key& var, const Datum& element) override
    {
        persPTData.clearPts(var, element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        persPTData.clearFullPts(var);
    }

    virtual inline void dumpPTData() override
    {
        persPTData.dumpPTData();
    }

    virtual bool hasDFInSet(LocID loc) const override
    {
        return dfInPtsMap.find(loc) != dfInPtsMap.end();
    }

    virtual bool hasDFOutSet(LocID loc) const override
    {
        return dfOutPtsMap.find(loc) != dfOutPtsMap.end();
    }

    virtual bool hasDFInSet(LocID loc, const Key& var) const override
    {
        typename DFKeyToIDMap::const_iterator foundInKeyToId = dfInPtsMap.find(loc);
        if (foundInKeyToId == dfInPtsMap.end()) return false;
        const KeyToIDMap &inKeyToId = foundInKeyToId->second;
        return (inKeyToId.find(var) != inKeyToId.end());
    }

    virtual bool hasDFOutSet(LocID loc, const Key& var) const override
    {
        typename DFKeyToIDMap::const_iterator foundOutKeyToId = dfOutPtsMap.find(loc);
        if (foundOutKeyToId == dfOutPtsMap.end()) return false;
        const KeyToIDMap &outKeyToId = foundOutKeyToId->second;
        return (outKeyToId.find(var) != outKeyToId.end());
    }

    virtual const DataSet& getDFInPtsSet(LocID loc, const Key& var) override
    {
        PointsToID id = dfInPtsMap[loc][var];
        return ptCache.getActualPts(id);
    }

    virtual const DataSet& getDFOutPtsSet(LocID loc, const Key& var) override
    {
        PointsToID id = dfOutPtsMap[loc][var];
        return ptCache.getActualPts(id);
    }

    virtual bool updateDFInFromIn(LocID srcLoc, const Key &srcVar, LocID dstLoc, const Key &dstVar) override
    {
        return unionPtsThroughIds(getDFInPtIdRef(dstLoc, dstVar), getDFInPtIdRef(srcLoc, srcVar));
    }

    virtual bool updateAllDFInFromIn(LocID srcLoc, const Key &srcVar, LocID dstLoc, const Key &dstVar) override
    {
        return updateDFInFromIn(srcLoc, srcVar, dstLoc, dstVar);
    }

    virtual bool updateDFInFromOut(LocID srcLoc, const Key &srcVar, LocID dstLoc, const Key &dstVar) override
    {
        return unionPtsThroughIds(getDFInPtIdRef(dstLoc, dstVar), getDFOutPtIdRef(srcLoc, srcVar));
    }

    virtual bool updateAllDFInFromOut(LocID srcLoc, const Key &srcVar, LocID dstLoc, const Key &dstVar) override
    {
        return updateDFInFromOut(srcLoc, srcVar, dstLoc, dstVar);
    }

    virtual bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return unionPtsThroughIds(getDFOutPtIdRef(dstLoc, dstVar), getDFInPtIdRef(srcLoc, srcVar));
    }

    virtual bool updateAllDFOutFromIn(LocID loc, const Key &singleton, bool strongUpdates) override
    {
        bool changed = false;
        if (this->hasDFInSet(loc))
        {
            const KeyToIDMap &inKeyToId = dfInPtsMap[loc];
            for (const typename KeyToIDMap::value_type &ki : inKeyToId)
            {
                const Key var = ki.first;
                /// Enable strong updates if required.
                if (strongUpdates && var == singleton) continue;

                if (updateDFOutFromIn(loc, var, loc, var)) changed = true;
            }
        }

        return changed;
    }

    virtual void clearAllDFOutUpdatedVar(LocID) override
    {
    }

    /// Update points-to set of top-level pointers with IN[srcLoc:srcVar].
    virtual bool updateTLVPts(LocID srcLoc, const Key &srcVar, const Key &dstVar) override
    {
        return unionPtsThroughIds(persPTData.ptsMap[dstVar], getDFInPtIdRef(srcLoc, srcVar));
    }

    virtual bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        return unionPtsThroughIds(getDFOutPtIdRef(dstLoc, dstVar), persPTData.ptsMap[srcVar]);
    }

    virtual void setDefaultData(const DataSet &dataSet) override
    {
        BasePTData::setDefaultData(dataSet);
        persPTData.setDefaultData(dataSet);
        ptCache.setDefaultData(dataSet);
    }

    virtual Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        Map<DataSet, unsigned> allPts = persPTData.getAllPts(liveOnly);
        for (const typename DFKeyToIDMap::value_type &lki : dfInPtsMap)
        {
            for (const typename KeyToIDMap::value_type &ki : lki.second)
            {
                ++allPts[ptCache.getActualPts(ki.second)];
            }
        }

        for (const typename DFKeyToIDMap::value_type &lki : dfOutPtsMap)
        {
            for (const typename KeyToIDMap::value_type &ki : lki.second)
            {
                ++allPts[ptCache.getActualPts(ki.second)];
            }
        }

        if (!liveOnly)
        {
            // Subtract 1 from each counted points-to set because the live points-to
            // sets have already been inserted and accounted for how often they occur.
            // They will each occur one more time in the cache.
            // In essence, we want the ptCache.getAllPts() to just add the unused, non-GC'd
            // points-to sets to allPts.
            for (typename Map<DataSet, unsigned>::value_type pto : allPts) pto.second -= 1;
            SVFUtil::mergePtsOccMaps<DataSet>(allPts, ptCache.getAllPts());
        }

        return allPts;
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentDFPTData<Key, KeySet, Datum, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Datum, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::PersDataFlow
               || ptd->getPTDTY() == PTDataTy::PersIncDataFlow;
    }
    ///@}

protected:
    inline bool unionPtsThroughIds(PointsToID &dst, PointsToID &src)
    {
        PointsToID oldDst = dst;
        dst = ptCache.unionPts(dst, src);
        return oldDst != dst;
    }

    PointsToID &getDFInPtIdRef(LocID loc, const Key &var)
    {
        return dfInPtsMap[loc][var];
    }

    PointsToID &getDFOutPtIdRef(LocID loc, const Key &var)
    {
        return dfOutPtsMap[loc][var];
    }

protected:
    PersistentPointsToCache<DataSet> &ptCache;

    /// PTData for top-level pointers. We will also use its cache for address-taken pointers.
    PersistentPTData<Key, KeySet, Datum, DataSet> persPTData;

    /// Address-taken points-to sets in IN-sets.
    DFKeyToIDMap dfInPtsMap;
    /// Address-taken points-to sets in OUT-sets.
    DFKeyToIDMap dfOutPtsMap;
};

/// Incremental version of the persistent data-flow points-to data structure.
template <typename Key, typename KeySet, typename Datum, typename DataSet>
class PersistentIncDFPTData : public PersistentDFPTData<Key, KeySet, Datum, DataSet>
{
public:
    typedef PTData<Key, KeySet, Datum, DataSet> BasePTData;
    typedef PersistentPTData<Key, KeySet, Datum, DataSet> BasePersPTData;
    typedef DFPTData<Key, KeySet, Datum, DataSet> BaseDFPTData;
    typedef PersistentDFPTData<Key, KeySet, Datum, DataSet> BasePersDFPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BaseDFPTData::LocID LocID;
    typedef Map<LocID, KeySet> UpdatedVarMap;

public:
    /// Constructor
    PersistentIncDFPTData(PersistentPointsToCache<DataSet> &cache, bool reversePT = true, PTDataTy ty = BasePTData::PersIncDataFlow)
        : BasePersDFPTData(cache, reversePT, ty) { }

    virtual ~PersistentIncDFPTData() { }

    virtual inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (varHasNewDFInPts(srcLoc, srcVar)
            && this->unionPtsThroughIds(this->getDFInPtIdRef(dstLoc, dstVar), this->getDFInPtIdRef(srcLoc, srcVar)))
        {
            setVarDFInSetUpdated(dstLoc, dstVar);
            return true;
        }

        return false;
    }

    virtual inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (varHasNewDFOutPts(srcLoc, srcVar)
            && this->unionPtsThroughIds(this->getDFInPtIdRef(dstLoc, dstVar), this->getDFOutPtIdRef(srcLoc, srcVar)))
        {
            setVarDFInSetUpdated(dstLoc, dstVar);
            return true;
        }

        return false;
    }

    virtual inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (varHasNewDFInPts(srcLoc, srcVar))
        {
            removeVarFromDFInUpdatedSet(srcLoc, srcVar);
            if (this->unionPtsThroughIds(this->getDFOutPtIdRef(dstLoc, dstVar), this->getDFInPtIdRef(srcLoc, srcVar)))
            {
                setVarDFOutSetUpdated(dstLoc, dstVar);
                return true;
            }
        }

        return false;
    }

    virtual inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (this->unionPtsThroughIds(this->getDFInPtIdRef(dstLoc, dstVar), this->getDFOutPtIdRef(srcLoc, srcVar)))
        {
            setVarDFInSetUpdated(dstLoc, dstVar);
            return true;
        }

        return false;
    }

    virtual inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (this->unionPtsThroughIds(this->getDFInPtIdRef(dstLoc, dstVar), this->getDFInPtIdRef(srcLoc, srcVar)))
        {
            setVarDFInSetUpdated(dstLoc, dstVar);
            return true;
        }

        return false;
    }

    virtual inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) override
    {
        bool changed = false;
        if (this->hasDFInSet(loc))
        {
            /// Only variables which have a new (IN) pts need to be updated.
            const KeySet vars = getDFInUpdatedVar(loc);
            for (const Key &var : vars)
            {
                /// Enable strong updates if it is required to do so
                if (strongUpdates && var == singleton) continue;
                if (updateDFOutFromIn(loc, var, loc, var)) changed = true;
            }
        }

        return changed;
    }

    virtual inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) override
    {
        if (varHasNewDFInPts(srcLoc, srcVar))
        {
            removeVarFromDFInUpdatedSet(srcLoc, srcVar);
            return this->unionPtsThroughIds(this->persPTData.ptsMap[dstVar], this->getDFInPtIdRef(srcLoc, srcVar));
        }

        return false;
    }

    virtual inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) override
    {
        if (this->unionPtsThroughIds(this->getDFOutPtIdRef(dstLoc, dstVar), this->persPTData.ptsMap[srcVar]))
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
            const KeySet vars = getDFOutUpdatedVar(loc);
            for (const Key &var : vars)
            {
                removeVarFromDFOutUpdatedSet(loc, var);
            }
        }
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentIncDFPTData<Key, KeySet, Datum, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Datum, DataSet>* ptd)
    {
        return ptd->getPTDTY() == BasePTData::PersIncDataFlow;
    }
    ///@}

private:

    /// Handle address-taken variables whose IN pts changed
    //@{
    /// Add var into loc's IN updated set. Called when var's pts in loc's IN set is changed.
    inline void setVarDFInSetUpdated(LocID loc, const Key& var)
    {
        inUpdatedVarMap[loc].set(var);
    }

    /// Remove var from loc's IN updated set.
    inline void removeVarFromDFInUpdatedSet(LocID loc, const Key& var)
    {
        typename UpdatedVarMap::iterator it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end()) it->second.reset(var);
    }

    /// Return TRUE if var has a new pts in loc's IN set
    inline bool varHasNewDFInPts(LocID loc, const Key& var)
    {
        typename UpdatedVarMap::iterator it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end()) return it->second.test(var);
        return false;
    }

    /// Get all variables which have new pts informationin loc's IN set
    inline const KeySet& getDFInUpdatedVar(LocID loc)
    {
        return inUpdatedVarMap[loc];
    }
    ///@}

    /// Handle address-taken variables whose OUT pts changed
    ///@{
    /// Add var into loc's OUT updated set. Called when var's pts in loc's OUT set changed
    inline void setVarDFOutSetUpdated(LocID loc, const Key& var)
    {
        outUpdatedVarMap[loc].set(var);
    }

    /// Remove var from loc's OUT updated set.
    inline void removeVarFromDFOutUpdatedSet(LocID loc, const Key& var)
    {
        typename UpdatedVarMap::iterator it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end()) it->second.reset(var);
    }

    /// Return TRUE if var has a new pts in loc's OUT set.
    inline bool varHasNewDFOutPts(LocID loc, const Key& var)
    {
        typename UpdatedVarMap::iterator it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end()) return it->second.test(var);
        return false;
    }

    /// Get all variables which have new pts info in loc's OUT set
    inline const KeySet& getDFOutUpdatedVar(LocID loc)
    {
        return outUpdatedVarMap[loc];
    }
    ///@}


private:
    UpdatedVarMap outUpdatedVarMap;
    UpdatedVarMap inUpdatedVarMap;
};

/// VersionedPTData implemented with persistent points-to sets (Data).
/// Implemented as a wrapper around two PersistentPTDatas: one for Keys, one
/// for VersionedKeys.
/// They are constructed with the same PersistentPointsToCache.
template <typename Key, typename KeySet, typename Datum, typename DataSet, typename VersionedKey, typename VersionedKeySet>
class PersistentVersionedPTData : public VersionedPTData<Key, KeySet, Datum, DataSet, VersionedKey, VersionedKeySet>
{
public:
    typedef PTData<Key, KeySet, Datum, DataSet> BasePTData;
    typedef VersionedPTData<Key, KeySet, Datum, DataSet, VersionedKey, VersionedKeySet> BaseVersionedPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    PersistentVersionedPTData(PersistentPointsToCache<DataSet> &cache, bool reversePT = true, PTDataTy ty = PTDataTy::PersVersioned)
        : BaseVersionedPTData(reversePT, ty), tlPTData(cache, reversePT), atPTData(cache, reversePT) { }

    virtual ~PersistentVersionedPTData() { }

    virtual inline void clear() override
    {
        tlPTData.clear();
        atPTData.clear();
    }

    virtual const DataSet& getPts(const Key& vk) override
    {
        return tlPTData.getPts(vk);
    }
    virtual const DataSet& getPts(const VersionedKey& vk) override
    {
        return atPTData.getPts(vk);
    }

    virtual const KeySet& getRevPts(const Datum& datum) override
    {
        assert(this->rev && "PersistentVersionedPTData::getRevPts: constructed without reverse PT support!");
        return tlPTData.getRevPts(datum);
    }
    virtual const VersionedKeySet& getVersionedKeyRevPts(const Datum& datum) override
    {
        assert(this->rev && "PersistentVersionedPTData::getVersionedKeyRevPts: constructed without reverse PT support!");
        return atPTData.getRevPts(datum);
    }

    virtual bool addPts(const Key& k, const Datum& element) override
    {
        return tlPTData.addPts(k, element);
    }
    virtual bool addPts(const VersionedKey& vk, const Datum& element) override
    {
        return atPTData.addPts(vk, element);
    }

    virtual bool unionPts(const Key& dstVar, const Key& srcVar) override
    {
        return tlPTData.unionPts(dstVar, srcVar);
    }
    virtual bool unionPts(const VersionedKey& dstVar, const VersionedKey& srcVar) override
    {
        return atPTData.unionPts(dstVar, srcVar);
    }
    virtual bool unionPts(const VersionedKey& dstVar, const Key& srcVar) override
    {
        return atPTData.unionPtsFromId(dstVar, tlPTData.ptsMap[srcVar]);
    }
    virtual bool unionPts(const Key& dstVar, const VersionedKey& srcVar) override
    {
        return tlPTData.unionPtsFromId(dstVar, atPTData.ptsMap[srcVar]);
    }
    virtual bool unionPts(const Key& dstVar, const DataSet& srcDataSet) override
    {
        return tlPTData.unionPts(dstVar, srcDataSet);
    }
    virtual bool unionPts(const VersionedKey& dstVar, const DataSet& srcDataSet) override
    {
        return atPTData.unionPts(dstVar, srcDataSet);
    }

    virtual void clearPts(const Key& k, const Datum& element) override
    {
        tlPTData.clearPts(k, element);
    }
    virtual void clearPts(const VersionedKey& vk, const Datum& element) override
    {
        atPTData.clearPts(vk, element);
    }

    virtual void clearFullPts(const Key& k) override
    {
        tlPTData.clearFullPts(k);
    }
    virtual void clearFullPts(const VersionedKey& vk) override
    {
        atPTData.clearFullPts(vk);
    }

    virtual Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        // Explicitly pass in true because if we call it with false,
        // we will double up on the cache, since it is shared with atPTData.
        // if liveOnly == false, we will handle it in the if below.
        Map<DataSet, unsigned> allPts = tlPTData.getAllPts(true);
        SVFUtil::mergePtsOccMaps<DataSet>(allPts, atPTData.getAllPts(true));

        if (!liveOnly)
        {
            // Subtract 1 from each counted points-to set because the live points-to
            // sets have already been inserted and accounted for how often they occur.
            // They will each occur one more time in the cache.
            // In essence, we want the ptCache.getAllPts() to just add the unused, non-GC'd
            // points-to sets to allPts.
            for (typename Map<DataSet, unsigned>::value_type &pto : allPts) pto.second -= 1;
            SVFUtil::mergePtsOccMaps<DataSet>(allPts, tlPTData.ptCache.getAllPts());
        }

        return allPts;
    }

    virtual inline void dumpPTData() override
    {
        SVFUtil::outs() << "== Top-level points-to information\n";
        tlPTData.dumpPTData();
        SVFUtil::outs() << "== Address-taken points-to information\n";
        atPTData.dumpPTData();
    }

    virtual void setDefaultData(const DataSet &dataSet) override
    {
        BasePTData::setDefaultData(dataSet);
        tlPTData.setDefaultData(dataSet);
        atPTData.setDefaultData(dataSet);
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentVersionedPTData<Key, KeySet, Datum, DataSet, VersionedKey, VersionedKeySet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Datum, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::PersVersioned;
    }
    ///@}

private:
    /// PTData for Keys (top-level pointers, generally).
    PersistentPTData<Key, KeySet, Datum, DataSet> tlPTData;
    /// PTData for VersionedKeys (address-taken objects, generally).
    PersistentPTData<VersionedKey, VersionedKeySet, Datum, DataSet> atPTData;
};

} // End namespace SVF
#endif  // MUTABLE_POINTSTO_H_
