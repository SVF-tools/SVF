/// Contains abstract classes for:
/// PTData: basic points-to data structure derived by all others.
/// DiffPTData: PTData which only propagates new changes, not entire points-to sets.
/// DFPTData: flow-sensitive PTData as defined by Hardekopf and Lin (CGO '11).
///
/// Hierarchy (square brackets indicate abstract class):
///
///       +------------> [PTData] <----------------+---------------------+
///       |                 ^                      |                     |
///       |                 |                      |                     |
/// MutablePTData      [DiffPTData]            [DFPTData]         [VersionedPTData]
///                         ^                      ^                     ^
///                         |                      |                     |
///                 MutableDiffPTData        MutableDFPTData    MutableVersionedPTData
///                                                ^
///                                                |
///                                        MutableIncDFPTData

#ifndef ABSTRACT_POINTSTO_H_
#define ABSTRACT_POINTSTO_H_

#include "Util/SVFBasicTypes.h"
#include "Util/PointsTo.h"

namespace SVF
{
/// Basic points-to data structure
/// Given a key (variable/condition variable), return its points-to data (pts/condition pts)
/// It is designed flexible for different context, heap and path sensitive analysis
/// Context Insensitive			   Key --> Variable, Data --> PointsTo
/// Context sensitive:  			   Key --> CondVar,  Data --> PointsTo
/// Heap sensitive:     			   Key --> Variable  Data --> CondPointsToSet
/// Context and heap sensitive:     Key --> CondVar,  Data --> CondPointsToSet
///
/// This class is abstract to allow for multiple methods of actually storing points-to sets.
/// Key:   "owning" variable of a points-to set.
/// Datum: elements in points-to sets.
/// Data:  the points-to set; a collection of Datums.
template <typename Key, typename Datum, typename Data>
class PTData
{
public:
    typedef Set<Key> KeySet;

    /// Types of a points-to data structures.
    enum PTDataTy
    {
        Base,
        MutBase,
        Diff,
        MutDiff,
        DataFlow,
        MutDataFlow,
        MutIncDataFlow,
        Versioned,
        MutVersioned,
        PersBase,
        PersDiff,
        PersDataFlow,
        PersIncDataFlow,
        PersVersioned,
    };

    PTData(bool reversePT = true, PTDataTy ty = PTDataTy::Base) : rev(reversePT), ptdTy(ty) { }

    virtual ~PTData() { }

    /// Get the type of points-to data structure that this is.
    inline PTDataTy getPTDTY() const
    {
        return ptdTy;
    }

    /// Clears all points-to sets as if nothing is stored.
    virtual void clear() = 0;

    /// Get points-to set of var.
    virtual const Data& getPts(const Key& var) = 0;
    /// Get reverse points-to set of datum.
    virtual const KeySet& getRevPts(const Datum& datum) = 0;

    /// Adds element to the points-to set associated with var.
    virtual bool addPts(const Key& var, const Datum& element) = 0;

    /// Performs pts(dstVar) = pts(dstVar) U pts(srcVar).
    virtual bool unionPts(const Key& dstVar, const Key& srcVar) = 0;
    /// Performs pts(dstVar) = pts(dstVar) U srcData.
    virtual bool unionPts(const Key& dstVar, const Data& srcData) = 0;

    /// Clears element from the points-to set of var.
    virtual void clearPts(const Key& var, const Datum& element) = 0;
    /// Fully clears the points-to set of var.
    virtual void clearFullPts(const Key& var) = 0;

    /// Dump stored keys and points-to sets.
    virtual void dumpPTData() = 0;

    /// Set the empty points-to set to be copied when a new set is requested.
    /// TODO: override for Persistent.
    virtual void setDefaultData(const Data &data)
    {
        defaultData = data;
    }

protected:
    /// Whether we maintain reverse points-to sets or not.
    bool rev;
    PTDataTy ptdTy;
    /// Data (points-to set) to freshly construct.
    Data defaultData;
};

/// Abstract diff points-to data with cached information.
/// This is an optimisation on top of the base points-to data structure.
/// The points-to information is propagated incrementally only for the different parts.
template <typename Key, typename Datum, typename Data>
class DiffPTData : public PTData<Key, Datum, Data>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    DiffPTData(bool reversePT = true, PTDataTy ty = PTDataTy::Diff) : BasePTData(reversePT, ty) { }

    virtual ~DiffPTData() { }

    /// Get diff points to.
    virtual const Data& getDiffPts(Key& var) = 0;

    /// Compute diff points to. Return TRUE if diff is not empty.
    /// 1. calculate diff: diff = all - propa.
    /// 2. update propagated pts: propa = all.
    virtual bool computeDiffPts(Key& var, const Data& all) = 0;

    /// Update dst's propagated points-to set with src's.
    /// The final result is the intersection of these two sets.
    virtual void updatePropaPtsMap(Key& src, Key& dst) = 0;

    /// Clear propagated points-to set of var.
    virtual void clearPropaPts(Key& var) = 0;

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const DiffPTData<Key, Datum, Data> *)
    {
        return true;
    }
    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::Diff
               || ptd->getPTDTY() == PTDataTy::MutDiff
               || ptd->getPTDTY() == PTDataTy::PersDiff;
    }
    ///@}
};

/// Data-flow points-to data structure for flow-sensitive analysis as defined by Hardekopf and Lin (CGO 11).
/// Points-to information is maintained at each program point (statement).
/// For address-taken variables, every program point has two sets: IN and OUT points-to sets.
/// For top-level variables, points-to sets are maintained flow-insensitively via getPts(var).
template <typename Key, typename Datum, typename Data>
class DFPTData : public PTData<Key, Datum, Data>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef NodeID LocID;

    /// Constructor
    DFPTData(bool reversePT = true, PTDataTy ty = BasePTData::DataFlow) : BasePTData(reversePT, ty) { }

    virtual ~DFPTData() { }

    /// Determine whether the DF IN/OUT sets have points-to sets.
    ///@{
    virtual bool hasDFInSet(LocID loc) const = 0;
    virtual bool hasDFOutSet(LocID loc) const = 0;
    ///@}

    /// Access points-to set from data-flow IN/OUT set.
    ///@{
    virtual bool hasDFOutSet(LocID loc, const Key& var) const = 0;
    virtual bool hasDFInSet(LocID loc, const Key& var) const = 0;
    virtual const Data& getDFInPtsSet(LocID loc, const Key& var) = 0;
    virtual const Data& getDFOutPtsSet(LocID loc, const Key& var) = 0;
    ///@}

    /// Update points-to for IN/OUT set
    /// IN[loc:var] represents the points-to of variable var in the IN set of location loc.
    /// union(ptsDst, ptsSrc) represents ptsDst = ptsDst U ptsSrc.
    ///@{
    /// Union (IN[dstLoc:dstVar], IN[srcLoc:srcVar]).
    virtual bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;
    /// Union (IN[dstLoc::dstVar], IN[srcLoc:srcVar]. There is no flag check, unlike the above.
    virtual bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;
    /// Union (IN[dstLoc:dstVar], OUT[srcLoc:srcVar]).
    virtual bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;
    /// Union (IN[dstLoc::dstVar], OUT[srcLoc:srcVar]. There is no flag check, unlike the above.
    virtual bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;

    /// Union (OUT[dstLoc:dstVar], IN[srcLoc:srcVar]).
    virtual bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;
    /// For each variable var in IN at loc, do updateDFOutFromIn(loc, var, loc, var).
    virtual bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates) = 0;

    virtual void clearAllDFOutUpdatedVar(LocID) = 0;

    /// Update points-to set of top-level pointers with IN[srcLoc:srcVar].
    virtual bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) = 0;
    /// Update address-taken variables OUT[dstLoc:dstVar] with points-to of top-level pointers
    virtual bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) = 0;
    ///@}

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const DFPTData<Key, Datum, Data> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == BasePTData::DataFlow
               || ptd->getPTDTY() == BasePTData::MutDataFlow
               || ptd->getPTDTY() == BasePTData::MutIncDataFlow
               || ptd->getPTDTY() == BasePTData::PersDataFlow
               || ptd->getPTDTY() == BasePTData::PersIncDataFlow;
    }
    ///@}
};

/// PTData with normal keys and versioned keys. Replicates the PTData interface for
/// versioned keys too. Intended to be used for versioned flow-sensitive PTA--hence the
/// name--but can be used anywhere where there are two types of keys at play.
template <typename Key, typename Datum, typename Data, typename VersionedKey>
class VersionedPTData : public PTData<Key, Datum, Data>
{
public:
    typedef PTData<Key, Datum, Data> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;
    typedef typename BasePTData::KeySet KeySet;

    typedef Set<VersionedKey> VersionedKeySet;

    VersionedPTData(bool reversePT = true, PTDataTy ty = PTDataTy::Versioned) : BasePTData(reversePT, ty) { }

    virtual ~VersionedPTData() { }

    virtual const Data& getPts(const VersionedKey& vk) = 0;
    virtual const VersionedKeySet& getVersionedKeyRevPts(const Datum& datum) = 0;

    virtual bool addPts(const VersionedKey& vk, const Datum& element) = 0;

    virtual bool unionPts(const VersionedKey& dstVar, const VersionedKey& srcVar) = 0;
    virtual bool unionPts(const VersionedKey& dstVar, const Key& srcVar) = 0;
    virtual bool unionPts(const Key& dstVar, const VersionedKey& srcVar) = 0;
    virtual bool unionPts(const VersionedKey& dstVar, const Data& srcData) = 0;

    virtual void clearPts(const VersionedKey& vk, const Datum& element) = 0;
    virtual void clearFullPts(const VersionedKey& vk) = 0;

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const VersionedPTData<Key, Datum, Data, VersionedKey> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, Datum, Data>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::Versioned
               || ptd->getPTDTY() == PTDataTy::MutVersioned
               || ptd->getPTDTY() == PTDataTy::PersVersioned;
    }
    ///@}
};

} // End namespace SVF

#endif  // ABSTRACT_POINTSTO_H_
