//===- MutablePointsToDS.h -- Mutable points-to data structure-------------//
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

/// PTData (AbstractPointsToDS.h) implementations with a mutable backend.
/// Each Key is given a points-to set which is itself updated till the analysis terminates.

/*
 * MutablePointsToDS.h
 *
 * Authors: Mohamad Barbar and Yulei Sui
 *
 * The implementation is based on
 * Mohamad Barbar and Yulei Sui. Hash Consed Points-To Sets.
 * 28th Static Analysis Symposium (SAS'21)
 */

#ifndef MUTABLE_POINTSTO_H_
#define MUTABLE_POINTSTO_H_

#include<fstream>

#include "MemoryModel/AbstractPointsToDS.h"
#include "SVFIR/SVFType.h"
#include "Util/SVFUtil.h"

namespace SVF
{

template <typename Key, typename KeySet, typename Data, typename DataSet>
class MutableDFPTData;

/// PTData implemented using points-to sets which are created once and updated continuously.
template <typename Key, typename KeySet, typename Data, typename DataSet>
class MutablePTData : public PTData<Key, KeySet, Data, DataSet>
{
    friend class MutableDFPTData<Key, KeySet, Data, DataSet>;
public:
    typedef PTData<Key, KeySet, Data, DataSet> BasePTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef Map<Key, DataSet> PtsMap;
    typedef Map<Data, KeySet> RevPtsMap;
    typedef typename PtsMap::iterator PtsMapIter;
    typedef typename PtsMap::const_iterator PtsMapConstIter;
    typedef typename DataSet::iterator iterator;

    /// Constructor
    MutablePTData(bool reversePT = true, PTDataTy ty = PTDataTy::MutBase) : BasePTData(reversePT, ty) { }

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

    virtual inline const DataSet& getPts(const Key& var) override
    {
        return ptsMap[var];
    }

    virtual inline const KeySet& getRevPts(const Data& datum) override
    {
        assert(this->rev && "MutablePTData::getRevPts: constructed without reverse PT support!");
        return revPtsMap[datum];
    }

    virtual inline bool addPts(const Key &dstKey, const Data& element) override
    {
        addSingleRevPts(revPtsMap[element], dstKey);
        return addPts(ptsMap[dstKey], element);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        addRevPts(ptsMap[srcKey], dstKey);
        return unionPts(ptsMap[dstKey], getPts(srcKey));
    }

    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcDataSet) override
    {
        addRevPts(srcDataSet,dstKey);
        return unionPts(ptsMap[dstKey], srcDataSet);
    }

    virtual inline void dumpPTData() override
    {
        dumpPts(ptsMap);
    }

    virtual void clearPts(const Key& var, const Data& element) override
    {
        clearSingleRevPts(revPtsMap[element], var);
        ptsMap[var].reset(element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        DataSet &pts = ptsMap[var];
        clearRevPts(pts, var);
        pts.clear();
    }

    virtual void remapAllPts(void) override
    {
        for (typename PtsMap::value_type &ppt : ptsMap) ppt.second.checkAndRemap();
    }

    virtual inline Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        Map<DataSet, unsigned> allPts;
        for (typename PtsMap::value_type ppt : ptsMap)
        {
            const DataSet &pt = ppt.second;
            ++allPts[pt];
        }

        return allPts;
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutablePTData<Key, KeySet, Data, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Data, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutBase;
    }
    ///@}

protected:
    virtual inline void dumpPts(const PtsMap & ptsSet,OutStream & O = SVFUtil::outs()) const
    {
        for (PtsMapConstIter nodeIt = ptsSet.begin(); nodeIt != ptsSet.end(); nodeIt++)
        {
            const Key& var = nodeIt->first;
            const DataSet & pts = nodeIt->second;
            if (pts.empty())
                continue;
            O << var << " ==> { ";
            for(typename DataSet::iterator cit = pts.begin(), ecit=pts.end(); cit!=ecit; ++cit)
            {
                O << *cit << " ";
            }
            O << "}\n";
        }
    }

private:
    /// Internal union/add points-to helper methods.
    ///@{
    inline bool unionPts(DataSet& dstDataSet, const DataSet& srcDataSet)
    {
        return dstDataSet |= srcDataSet;
    }
    inline bool addPts(DataSet &d, const Data& e)
    {
        return d.test_and_set(e);
    }
    inline void addSingleRevPts(KeySet &revData, const Key& tgr)
    {
        if (this->rev)
        {
            SVFUtil::insertKey(tgr, revData);
        }
    }
    inline void addRevPts(const DataSet &ptsData, const Key& tgr)
    {
        if (this->rev)
        {
            for(iterator it = ptsData.begin(), eit = ptsData.end(); it!=eit; ++it)
                addSingleRevPts(revPtsMap[*it], tgr);
        }
    }
    inline void clearSingleRevPts(KeySet &revSet, const Key &k)
    {
        if (this->rev)
        {
            SVFUtil::removeKey(k, revSet);
        }
    }
    inline void clearRevPts(const DataSet &pts, const Key &k)
    {
        if (this->rev)
        {
            for (const Data &d : pts) clearSingleRevPts(revPtsMap[d], k);
        }
    }
    ///@}

protected:
    PtsMap ptsMap;
    RevPtsMap revPtsMap;
};

/// DiffPTData implemented with points-to sets which are updated continuously.
template <typename Key, typename KeySet, typename Data, typename DataSet>
class MutableDiffPTData : public DiffPTData<Key, KeySet, Data, DataSet>
{
public:
    typedef PTData<Key, KeySet, Data, DataSet> BasePTData;
    typedef DiffPTData<Key, KeySet, Data, DataSet> BaseDiffPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename MutablePTData<Key, KeySet, Data, DataSet>::PtsMap PtsMap;

    /// Constructor
    explicit MutableDiffPTData(bool reversePT = true, PTDataTy ty = PTDataTy::Diff) : BaseDiffPTData(reversePT, ty), mutPTData(reversePT) { }

    ~MutableDiffPTData() override = default;

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    inline void clear() override
    {
        mutPTData.clear();
    }

    virtual inline const DataSet& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    virtual inline const KeySet& getRevPts(const Data& datum) override
    {
        assert(this->rev && "MutableDiffPTData::getRevPts: constructed without reverse PT support!");
        return mutPTData.getRevPts(datum);
    }

    virtual inline bool addPts(const Key &dstKey, const Data& element) override
    {
        return mutPTData.addPts(dstKey, element);
    }

    virtual inline bool unionPts(const Key& dstKey, const Key& srcKey) override
    {
        return mutPTData.unionPts(dstKey, srcKey);
    }

    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcDataSet) override
    {
        return mutPTData.unionPts(dstKey, srcDataSet);
    }

    virtual void clearPts(const Key& var, const Data& element) override
    {
        mutPTData.clearPts(var, element);
    }

    virtual void clearFullPts(const Key& var) override
    {
        mutPTData.clearFullPts(var);
    }

    virtual void remapAllPts(void) override
    {
        mutPTData.remapAllPts();
        for (typename PtsMap::value_type &ppt : diffPtsMap) ppt.second.checkAndRemap();
        for (typename PtsMap::value_type &ppt : propaPtsMap) ppt.second.checkAndRemap();
    }

    virtual inline void dumpPTData() override
    {
        mutPTData.dumpPTData();
    }

    virtual inline const DataSet &getDiffPts(Key &var) override
    {
        return diffPtsMap[var];
    }

    virtual inline bool computeDiffPts(Key &var, const DataSet &all) override
    {
        /// Clear diff pts.
        DataSet& diff = diffPtsMap[var];
        diff.clear();
        /// Get all pts.
        DataSet& propa = getPropaPts(var);
        diff.intersectWithComplement(all, propa);
        propa = all;
        return !diff.empty();
    }

    virtual inline void updatePropaPtsMap(Key &src, Key &dst) override
    {
        DataSet& srcPropa = getPropaPts(src);
        DataSet& dstPropa = getPropaPts(dst);
        dstPropa &= srcPropa;
    }

    virtual inline void clearPropaPts(Key &var) override
    {
        getPropaPts(var).clear();
    }

    virtual inline Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        return mutPTData.getAllPts(liveOnly);
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableDiffPTData<Key, KeySet, Data, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Data, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutDiff;
    }
    ///@}

protected:
    /// Get propagated points to.
    inline DataSet &getPropaPts(Key &var)
    {
        return propaPtsMap[var];
    }

private:
    /// Backing to implement the basic PTData methods. This allows us to avoid multiple-inheritance.
    MutablePTData<Key, KeySet, Data, DataSet> mutPTData;
    /// Diff points-to to be propagated.
    PtsMap diffPtsMap;
    /// Points-to already propagated.
    PtsMap propaPtsMap;
};

template <typename Key, typename KeySet, typename Data, typename DataSet>
class MutableDFPTData : public DFPTData<Key, KeySet, Data, DataSet>
{
public:
    typedef PTData<Key, KeySet, Data, DataSet> BasePTData;
    typedef MutablePTData<Key, KeySet, Data, DataSet> BaseMutPTData;
    typedef DFPTData<Key, KeySet, Data, DataSet> BaseDFPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BaseDFPTData::LocID LocID;
    typedef typename BaseMutPTData::PtsMap PtsMap;
    typedef typename BaseMutPTData::PtsMapConstIter PtsMapConstIter;
    typedef Map<LocID, PtsMap> DFPtsMap;	///< Data-flow point-to map
    typedef typename DFPtsMap::iterator DFPtsMapIter;
    typedef typename DFPtsMap::const_iterator DFPtsMapconstIter;

    /// Constructor
    MutableDFPTData(bool reversePT = true, PTDataTy ty = BaseDFPTData::MutDataFlow) : BaseDFPTData(reversePT, ty), mutPTData(reversePT) { }

    virtual ~MutableDFPTData() { }

    virtual inline const PtsMap& getPtsMap() const
    {
        return mutPTData.getPtsMap();
    }

    virtual inline void clear() override
    {
        mutPTData.clear();
    }

    virtual inline const DataSet& getPts(const Key& var) override
    {
        return mutPTData.getPts(var);
    }

    virtual inline const KeySet& getRevPts(const Data& datum) override
    {
        assert(this->rev && "MutableDFPTData::getRevPts: constructed without reverse PT support!");
        return mutPTData.getRevPts(datum);
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

    virtual inline DataSet& getDFInPtsSet(LocID loc, const Key& var) override
    {
        PtsMap& inSet = dfInPtsMap[loc];
        return inSet[var];
    }

    virtual inline DataSet& getDFOutPtsSet(LocID loc, const Key& var) override
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
    virtual inline bool unionPts(const Key& dstKey, const DataSet& srcDataSet) override
    {
        return unionPts(mutPTData.ptsMap[dstKey], srcDataSet);
    }
    virtual void clearPts(const Key& var, const Data& element) override
    {
        mutPTData.clearPts(var, element);
    }
    virtual void clearFullPts(const Key& var) override
    {
        mutPTData.clearFullPts(var);
    }
    virtual void remapAllPts(void) override
    {
        mutPTData.remapAllPts();
        for (typename DFPtsMap::value_type &lopt : dfInPtsMap)
        {
            for (typename PtsMap::value_type &opt : lopt.second) opt.second.checkAndRemap();
        }

        for (typename DFPtsMap::value_type &lopt : dfOutPtsMap)
        {
            for (typename PtsMap::value_type &opt : lopt.second) opt.second.checkAndRemap();
        }
    }
    ///@}

    virtual inline Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        Map<DataSet, unsigned> allPts = mutPTData.getAllPts(liveOnly);
        for (typename DFPtsMap::value_type lptsmap : dfInPtsMap)
        {
            for (typename PtsMap::value_type vpt : lptsmap.second)
            {
                ++allPts[vpt.second];
            }
        }

        for (typename DFPtsMap::value_type lptm : dfOutPtsMap)
        {
            for (typename PtsMap::value_type vpt : lptm.second)
            {
                ++allPts[vpt.second];
            }
        }

        return allPts;
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableDFPTData<Key, KeySet, Data, DataSet> *)
    {
        return true;
    }
    static inline bool classof(const PTData<Key, KeySet, Data, DataSet>* ptd)
    {
        return ptd->getPTDTY() == BaseDFPTData::MutDataFlow
               || ptd->getPTDTY() == BaseDFPTData::MutIncDataFlow;
    }
    ///@}

protected:
    /// Internal union/add points-to helper methods.
    ///@{
    inline bool unionPts(DataSet& dstDataSet, const DataSet& srcDataSet)
    {
        return dstDataSet |= srcDataSet;
    }
    inline bool addPts(DataSet &d, const Data& e)
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
        std::fstream f("svfg_pts.data", std::ios_base::out);
        if (f.good())
        {
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
                    f << "Loc:" << loc << " IN:{";
                    this->dumpPts(this->getDFInPtsMap(loc), f);
                    f << "}\n";
                }

                if (this->hasDFOutSet(loc))
                {
                    f << "Loc:" << loc << " OUT:{";
                    this->dumpPts(this->getDFOutPtsMap(loc), f);
                    f << "}\n";
                }
            }
            f.close();
            if (f.good())
            {
                SVFUtil::outs() << "\n";
                return;
            }
        }
        SVFUtil::outs() << "  error opening file for writing!\n";
    }

    virtual inline void dumpPts(const PtsMap & ptsSet,OutStream & O = SVFUtil::outs()) const
    {
        for (PtsMapConstIter nodeIt = ptsSet.begin(); nodeIt != ptsSet.end(); nodeIt++)
        {
            const Key& var = nodeIt->first;
            const DataSet & pts = nodeIt->second;
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
    MutablePTData<Key, KeySet, Data, DataSet> mutPTData;
};

/// Incremental version of the mutable data-flow points-to data structure.
template <typename Key, typename KeySet, typename Data, typename DataSet>
class MutableIncDFPTData : public MutableDFPTData<Key, KeySet, Data, DataSet>
{
public:
    typedef PTData<Key, KeySet, Data, DataSet> BasePTData;
    typedef MutablePTData<Key, KeySet, Data, DataSet> BaseMutPTData;
    typedef DFPTData<Key, KeySet, Data, DataSet> BaseDFPTData;
    typedef MutableDFPTData<Key, KeySet, Data, DataSet> BaseMutDFPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    typedef typename BaseDFPTData::LocID LocID;
    typedef Map<LocID, DataSet> UpdatedVarMap;	///< for propagating only newly added variable in IN/OUT set
    typedef typename UpdatedVarMap::iterator UpdatedVarMapIter;
    typedef typename UpdatedVarMap::const_iterator UpdatedVarconstIter;
    typedef typename DataSet::iterator DataIter;

private:
    UpdatedVarMap outUpdatedVarMap;
    UpdatedVarMap inUpdatedVarMap;

public:
    /// Constructor
    MutableIncDFPTData(bool reversePT = true, PTDataTy ty = BasePTData::MutIncDataFlow) : BaseMutDFPTData(reversePT, ty) { }

    virtual ~MutableIncDFPTData() { }

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
            DataSet pts = getDFInUpdatedVar(loc);
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
            DataSet pts = getDFOutUpdatedVar(loc);
            for (DataIter ptsIt = pts.begin(), ptsEit = pts.end(); ptsIt != ptsEit; ++ptsIt)
            {
                const Key var = *ptsIt;
                removeVarFromDFOutUpdatedSet(loc, var);
            }
        }
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableIncDFPTData<Key, KeySet, Data, DataSet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Data, DataSet>* ptd)
    {
        return ptd->getPTDTY() == BasePTData::MutIncDataFlow;
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
    inline const DataSet& getDFInUpdatedVar(LocID loc)
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
    inline const DataSet& getDFOutUpdatedVar(LocID loc)
    {
        return outUpdatedVarMap[loc];
    }
    //@}
};

/// VersionedPTData implemented with mutable points-to set (DataSet).
/// Implemented as a wrapper around two MutablePTDatas: one for Keys, one
/// for VersionedKeys.
template <typename Key, typename KeySet, typename Data, typename DataSet, typename VersionedKey, typename VersionedKeySet>
class MutableVersionedPTData : public VersionedPTData<Key, KeySet, Data, DataSet, VersionedKey, VersionedKeySet>
{
public:
    typedef PTData<Key, KeySet, Data, DataSet> BasePTData;
    typedef VersionedPTData<Key, KeySet, Data, DataSet, VersionedKey, VersionedKeySet> BaseVersionedPTData;
    typedef typename BasePTData::PTDataTy PTDataTy;

    MutableVersionedPTData(bool reversePT = true, PTDataTy ty = PTDataTy::MutVersioned)
        : BaseVersionedPTData(reversePT, ty), tlPTData(reversePT), atPTData(reversePT) { }

    virtual ~MutableVersionedPTData() { }

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

    virtual const KeySet& getRevPts(const Data& datum) override
    {
        assert(this->rev && "MutableVersionedPTData::getRevPts: constructed without reverse PT support!");
        return tlPTData.getRevPts(datum);
    }
    virtual const VersionedKeySet& getVersionedKeyRevPts(const Data& datum) override
    {
        assert(this->rev && "MutableVersionedPTData::getVersionedKeyRevPts: constructed without reverse PT support!");
        return atPTData.getRevPts(datum);
    }

    virtual bool addPts(const Key& k, const Data& element) override
    {
        return tlPTData.addPts(k, element);
    }
    virtual bool addPts(const VersionedKey& vk, const Data& element) override
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
        return atPTData.unionPts(dstVar, tlPTData.getPts(srcVar));
    }
    virtual bool unionPts(const Key& dstVar, const VersionedKey& srcVar) override
    {
        return tlPTData.unionPts(dstVar, atPTData.getPts(srcVar));
    }
    virtual bool unionPts(const Key& dstVar, const DataSet& srcDataSet) override
    {
        return tlPTData.unionPts(dstVar, srcDataSet);
    }
    virtual bool unionPts(const VersionedKey& dstVar, const DataSet& srcDataSet) override
    {
        return atPTData.unionPts(dstVar, srcDataSet);
    }

    virtual void clearPts(const Key& k, const Data& element) override
    {
        tlPTData.clearPts(k, element);
    }
    virtual void clearPts(const VersionedKey& vk, const Data& element) override
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

    virtual void remapAllPts(void) override
    {
        tlPTData.remapAllPts();
        atPTData.remapAllPts();
    }

    virtual inline Map<DataSet, unsigned> getAllPts(bool liveOnly) const override
    {
        Map<DataSet, unsigned> allPts = tlPTData.getAllPts(liveOnly);
        SVFUtil::mergePtsOccMaps<DataSet>(allPts, atPTData.getAllPts(liveOnly));
        return allPts;
    }

    virtual inline void dumpPTData() override
    {
        SVFUtil::outs() << "== Top-level points-to information\n";
        tlPTData.dumpPTData();
        SVFUtil::outs() << "== Address-taken points-to information\n";
        atPTData.dumpPTData();
    }

    /// Methods to support type inquiry through isa, cast, and dyn_cast:
    ///@{
    static inline bool classof(const MutableVersionedPTData<Key, KeySet, Data, DataSet, VersionedKey, VersionedKeySet> *)
    {
        return true;
    }

    static inline bool classof(const PTData<Key, KeySet, Data, DataSet>* ptd)
    {
        return ptd->getPTDTY() == PTDataTy::MutVersioned;
    }
    ///@}

private:
    /// PTData for Keys (top-level pointers, generally).
    MutablePTData<Key, KeySet, Data, DataSet> tlPTData;
    /// PTData for VersionedKeys (address-taken objects, generally).
    MutablePTData<VersionedKey, VersionedKeySet, Data, DataSet> atPTData;
};

} // End namespace SVF

#endif  // MUTABLE_POINTSTO_H_
