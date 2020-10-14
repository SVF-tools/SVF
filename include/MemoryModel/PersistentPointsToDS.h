/// PTData (AbstractPointsToDS.h) implementations with a persistent backend.
/// Each Key is given a cheap points-to ID which refers to some real points-to set.

#ifndef PERSISTENT_POINTSTO_H_
#define PERSISTENT_POINTSTO_H_

#include "MemoryModel/AbstractPointsToDS.h"
#include "MemoryModel/PersistentPointsToCache.h"

namespace SVF
{

/// PTData backed by a PersistentPointsToCache.
template <typename Key, typename Datum, typename Data>
class PersistentPTData : public PTData<Key, Datum, Data>
{
    friend class MutableDFPTData<Key, Datum, Data>;
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;
    typedef typename BasePTData::KeySet KeySet;

    typedef Map<Key, PointsToID> KeyToIDMap;
    typedef Map<Datum, KeySet> RevPtsMap;

    /// Constructor
    PersistentPTData(bool reversePT = true, PTDataTy ty = PTDataTy::PersBase)
        : BasePTData(reversePT, ty), ptCache(Data()) { }

    virtual ~PersistentPTData() { }

    PersistentPointsToCache<Data> &getPtCache(void)
    {
        return ptCache;
    }

    virtual inline void clear() override
    {
        ptsMap.clear();
        revPtsMap.clear();
    }

    virtual inline const Data& getPts(const Key& var) override
    {
        PointsToID id = ptsMap[var];
        return ptCache.getActualPts(id);
    }

    virtual inline const KeySet& getRevPts(const Datum& datum) override
    {
        return revPtsMap[datum];
    }

    virtual inline bool addPts(const Key &dstKey, const Datum& element) override
    {
        Data srcPts;
        srcPts.set(element);
        PointsToID srcId = ptCache.emplacePts(srcPts);
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        PointsToID srcId = ptsMap[srcKey];
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        PointsToID srcId = ptCache.emplacePts(srcData);
        return unionPtsFromId(dstKey, srcId);
    }

    virtual inline void dumpPTData() override
    {
    }

    virtual void clearPts(const Key& var, const Datum& element) override
    {
        Data toRemoveData;
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
        ptsMap[var] = PersistentPointsToCache<Data>::emptyPointsToId();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentPTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
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
            const Data &srcPts = ptCache.getActualPts(srcId);
            for (const Datum &d : srcPts) revPtsMap[d].insert(dstKey);
        }

        return changed;
    }

protected:
    PersistentPointsToCache<Data> ptCache;
    KeyToIDMap ptsMap;
    RevPtsMap revPtsMap;
};

/// DiffPTData implemented with a persistent points-to backing.
template <typename Key, typename Datum, typename Data>
class PersistentDiffPTData : public DiffPTData<Key, Datum, Data>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef DiffPTData<Key, Datum, Data> BaseDiffPTData;
    typedef PersistentPTData<Key, Datum, Data> BasePersPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;
    typedef typename BasePTData::KeySet KeySet;

    typedef typename BasePersPTData::KeyToIDMap KeyToIDMap;
    typedef typename BasePersPTData::RevPtsMap RevPtsMap;

    /// Constructor
    PersistentDiffPTData(bool reversePT = true, PTDataTy ty = PTDataTy::PersDiff)
        : BaseDiffPTData(reversePT, ty), persPTData(reversePT) { }

    virtual ~PersistentDiffPTData() { }

    virtual inline void clear() override
    {
        persPTData.clear();
        diffPtsMap.clear();
        propaPtsMap.clear();
    }

    virtual inline const Data& getPts(const Key& var) override
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

    virtual inline bool unionPts(const Key& dstKey, const Data& srcData) override
    {
        return persPTData.unionPts(dstKey, srcData);
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

    virtual inline const Data &getDiffPts(Key &var) override
    {
        PointsToID id = diffPtsMap[var];
        return persPTData.getPtCache().getActualPts(id);
    }

    virtual inline bool computeDiffPts(Key &var, const Data &all) override
    {
        // Cache the cache.
        PersistentPointsToCache<Data> &ptCache = persPTData.getPtCache();

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
        propaPtsMap[dst] = persPTData.getPtCache().intersectPts(dstId, srcId);
    }

    virtual inline void clearPropaPts(Key &var) override
    {
        propaPtsMap[var] = persPTData.getPtCache().emptyPointsToId();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const PersistentDiffPTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::PersDiff;
    }
    ///@}

private:
    /// Backing to implement basic PTData methods. Allows us to avoid multiple inheritance.
    PersistentPTData<Key, Datum, Data> persPTData;
    /// Diff points-to to be propagated.
    KeyToIDMap diffPtsMap;
    /// Points-to already propagated.
    KeyToIDMap propaPtsMap;
};

} // End namespace SVF
#endif  // MUTABLE_POINTSTO_H_
