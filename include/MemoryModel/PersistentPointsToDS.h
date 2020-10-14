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

} // End namespace SVF
#endif  // MUTABLE_POINTSTO_H_
