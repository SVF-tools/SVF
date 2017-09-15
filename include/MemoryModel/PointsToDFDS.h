//===- PointsToDFDS.h -- Points-to data structure for flow-sensitive analysis-//
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
 * PointsToDSDF.h
 *
 *  Created on: Mar 25, 2014
 *      Author: Yulei Sui
 */

#ifndef POINTSTODSDF_H_
#define POINTSTODSDF_H_

#include "MemoryModel/PointsToDS.h"
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FileSystem.h>		// for file open flag

/*!
 * Data-flow points-to data structure, points-to is maintained for each program point (statement)
 * For address-taken variables, every program point has two sets IN and OUT points-to sets
 * For top-level variables, their points-to sets are maintained in a flow-insensitive manner via getPts(var).
 */
template<class Key, class Data>
class DFPTData : public PTData<Key,Data> {
public:
    typedef NodeID LocID;
    typedef typename PTData<Key,Data>::PtsMap PtsMap;
    typedef typename PTData<Key,Data>::PtsMapConstIter PtsMapConstIter;
    typedef std::map<LocID, PtsMap> DFPtsMap;	///< Data-flow point-to map
    typedef typename DFPtsMap::iterator DFPtsMapIter;
    typedef typename DFPtsMap::const_iterator DFPtsMapconstIter;
    typedef typename PTData<Key,Data>::PTDataTY PTDataTy;

    DFPtsMap dfInPtsMap;	///< Data-flow IN set
    DFPtsMap dfOutPtsMap;	///< Data-flow OUT set
    /// Constructor
    DFPTData(PTDataTy ty = (PTData<Key,Data>::DFPTD)): PTData<Key,Data>(ty) {
    }
    /// Destructor
    virtual ~DFPTData() {
    }

    /// Determine whether the DF IN/OUT sets have ptsMap
    //@{
    inline bool hasDFInSet(LocID loc) const {
        return (dfInPtsMap.find(loc) != dfInPtsMap.end());
    }
    inline bool hasDFOutSet(LocID loc) const {
        return (dfOutPtsMap.find(loc) != dfOutPtsMap.end());
    }
    inline bool hasDFInSet(LocID loc,const Key& var) const {
        DFPtsMapconstIter it = dfInPtsMap.find(loc);
        if ( it == dfInPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }
    inline bool hasDFOutSet(LocID loc,const Key& var) const {
        DFPtsMapconstIter it = dfOutPtsMap.find(loc);
        if ( it == dfOutPtsMap.end())
            return false;
        const PtsMap& ptsMap = it->second;
        return (ptsMap.find(var) != ptsMap.end());
    }
    inline const PtsMap& getDFInPtsMap(LocID loc)  {
        return dfInPtsMap[loc];
    }
    inline const PtsMap& getDFOutPtsMap(LocID loc)  {
        return dfOutPtsMap[loc];
    }
    inline const DFPtsMap& getDFIn()  {
        return dfInPtsMap;
    }
    inline const DFPtsMap& getDFOut()  {
        return dfOutPtsMap;
    }
    //@}

    /// Get points-to from data-flow IN/OUT set
    ///@{
    inline Data& getDFInPtsSet(LocID loc, const Key& var) {
        PtsMap& inSet = dfInPtsMap[loc];
        return inSet[var];
    }
    inline Data& getDFOutPtsSet(LocID loc, const Key& var) {
        PtsMap& outSet = dfOutPtsMap[loc];
        return outSet[var];
    }
    ///@}

    /// Update points-to for IN/OUT set
    /// IN[loc:var] represents the points-to of variable var from IN set of location loc
    /// union(ptsDst,ptsSrc) represents union ptsSrc to ptsDst
    //@{
    /// union (IN[dstLoc:dstVar], IN[srcLoc:srcVar])
    virtual inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }
    /// union (IN[dstLoc:dstVar], OUT[srcLoc:srcVar])
    virtual inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return this->unionPts(getDFInPtsSet(dstLoc,dstVar), getDFOutPtsSet(srcLoc,srcVar));
    }
    /// union (OUT[dstLoc:dstVar], IN[srcLoc:srcVar])
    virtual inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return this->unionPts(getDFOutPtsSet(dstLoc,dstVar), getDFInPtsSet(srcLoc,srcVar));
    }
    /// union (IN[dstLoc::dstVar], OUT[srcLoc:srcVar]. It differs from the above method in that there's
    /// no flag check.
    virtual inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return this->updateDFInFromOut(srcLoc,srcVar,dstLoc,dstVar);
    }
    /// union (IN[dstLoc::dstVar], IN[srcLoc:srcVar]. It differs from the above method in that there's
    /// no flag check.
    virtual inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return this->updateDFInFromIn(srcLoc,srcVar,dstLoc,dstVar);
    }
    /// for each variable var in IN at loc, do updateDFOutFromIn(loc,var,loc,var)
    virtual inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates)
    {
        bool changed = false;
        if (this->hasDFInSet(loc)) {
            /// Only variables has new pts from IN set need to be updated.
            const PtsMap & ptsMap = getDFInPtsMap(loc);
            for (typename PtsMap::const_iterator ptsIt = ptsMap.begin(), ptsEit = ptsMap.end(); ptsIt != ptsEit; ++ptsIt) {
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
    /// Update points-to of top-level pointers with IN[srcLoc:srcVar]
    virtual inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) {
        return this->unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
    }
    /// Update address-taken variables OUT[dstLoc:dstVar] with points-to of top-level pointers
    virtual inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        return (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->getPts(srcVar)));
    }
    virtual inline void clearAllDFOutUpdatedVar(LocID loc) {
    }
    //@}

    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const DFPTData<Key,Data> *) {
        return true;
    }
    static inline bool classof(const PTData<Key,Data>* ptd) {
        return ptd->getPTDTY() == PTData<Key,Data>::DFPTD;
    }
    //@}

    /// Override the methods defined in PTData.
    /// Union/add points-to without adding reverse points-to, used internally
    //@{
    inline bool addPts(const Key &dstKey, const Key& srcKey) {
        return addPts(this->getPts(dstKey),srcKey);
    }
    inline bool unionPts(const Key& dstKey, const Key& srcKey) {
        return unionPts(this->getPts(dstKey),this->getPts(srcKey));
    }
    inline bool unionPts(const Key& dstKey, const Data& srcData) {
        return unionPts(this->getPts(dstKey),srcData);
    }
    //@}

protected:
    /// Union two points-to sets
    inline bool unionPts(Data& dstData, const Data& srcData) {
        return dstData |= srcData;
    }

public:
    /// Dump the DF IN/OUT set information for debugging purpose
    //@{
    virtual inline void dumpPTData() {
        /// dump points-to of top-level pointers
        PTData<Key,Data>::dumpPts(this->ptsMap);
        /// dump points-to of address-taken variables
        std::error_code ErrInfo;
        llvm::tool_output_file F("svfg_pts.data", ErrInfo, llvm::sys::fs::F_None);
        if (!ErrInfo) {
            llvm::raw_fd_ostream & osm = F.os();
            NodeBS locs;
            for(DFPtsMapconstIter it = dfInPtsMap.begin(), eit = dfInPtsMap.end(); it!=eit; ++it)
                locs.set(it->first);

            for(DFPtsMapconstIter it = dfOutPtsMap.begin(), eit = dfOutPtsMap.end(); it!=eit; ++it)
                locs.set(it->first);

            for (NodeBS::iterator it = locs.begin(), eit = locs.end(); it != eit; it++) {
                LocID loc = *it;
                if (this->hasDFInSet(loc)) {
                    osm << "Loc:" << loc << " IN:{";
                    this->dumpPts(this->getDFInPtsMap(loc), osm);
                    osm << "}\n";
                }

                if (this->hasDFOutSet(loc)) {
                    osm << "Loc:" << loc << " OUT:{";
                    this->dumpPts(this->getDFOutPtsMap(loc), osm);
                    osm << "}\n";
                }
            }
            F.os().close();
            if (!F.os().has_error()) {
                llvm::outs() << "\n";
                F.keep();
                return;
            }
        }
        llvm::outs() << "  error opening file for writing!\n";
        F.os().clear_error();
    }

    virtual inline void dumpPts(const PtsMap & ptsSet,llvm::raw_ostream & O = llvm::outs()) const {
        for (PtsMapConstIter nodeIt = ptsSet.begin(); nodeIt != ptsSet.end(); nodeIt++) {
            const Key& var = nodeIt->first;
            const Data & pts = nodeIt->second;
            if (pts.empty())
                continue;
            O << "<" << var << ",{";
            analysisUtil::dumpSet(pts,O);
            O << "}> ";
        }
    }
    //@}

};

/*!
 * Incremental data-flow points-to data version
 */
template<class Key, class Data>
class IncDFPTData : public DFPTData<Key,Data> {
public:
    typedef typename DFPTData<Key,Data>::LocID LocID;
    typedef std::map<LocID, Data> UpdatedVarMap;	///< for propagating only newly added variable in IN/OUT set
    typedef typename UpdatedVarMap::iterator UpdatedVarMapIter;
    typedef typename UpdatedVarMap::const_iterator UpdatedVarconstIter;
    typedef typename PTData<Key,Data>::PTDataTY PTDataTy;
    typedef typename Data::iterator DataIter;
private:
    UpdatedVarMap outUpdatedVarMap;
    UpdatedVarMap inUpdatedVarMap;

public:
    /// Constructor
    IncDFPTData(PTDataTy ty = (PTData<Key,Data>::IncDFPTD)): DFPTData<Key,Data>(ty) {
    }
    /// Destructor
    virtual ~IncDFPTData() {
    }

    /// Update points-to for IN/OUT set
    /// IN[loc:var] represents the points-to of variable var from IN set of location loc
    /// union(ptsDst,ptsSrc) represents union ptsSrc to ptsDst
    //@{
    /// union (IN[dstLoc:dstVar], IN[srcLoc:srcVar])
    inline bool updateDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if(varHasNewDFInPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar))) {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }
    /// union (IN[dstLoc:dstVar], OUT[srcLoc:srcVar])
    inline bool updateDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if(varHasNewDFOutPts(srcLoc, srcVar) &&
                this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar))) {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }
    /// union (OUT[dstLoc:dstVar], IN[srcLoc:srcVar])
    inline bool updateDFOutFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if(varHasNewDFInPts(srcLoc,srcVar)) {
            removeVarFromDFInUpdatedSet(srcLoc,srcVar);
            if (this->unionPts(this->getDFOutPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar))) {
                setVarDFOutSetUpdated(dstLoc,dstVar);
                return true;
            }
        }
        return false;
    }

    /// union (IN[dstLoc::dstVar], OUT[srcLoc:srcVar]. It differs from the above method in that there's
    /// no flag check.
    inline bool updateAllDFInFromOut(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFOutPtsSet(srcLoc,srcVar))) {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }
    /// union (IN[dstLoc::dstVar], IN[srcLoc:srcVar]. It differs from the above method in that there's
    /// no flag check.
    inline bool updateAllDFInFromIn(LocID srcLoc, const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if(this->unionPts(this->getDFInPtsSet(dstLoc,dstVar), this->getDFInPtsSet(srcLoc,srcVar))) {
            setVarDFInSetUpdated(dstLoc,dstVar);
            return true;
        }
        return false;
    }
    /// for each variable var in IN at loc, do updateDFOutFromIn(loc,var,loc,var)
    inline bool updateAllDFOutFromIn(LocID loc, const Key& singleton, bool strongUpdates)
    {
        bool changed = false;
        if (this->hasDFInSet(loc)) {
            /// Only variables has new pts from IN set need to be updated.
            Data pts = getDFInUpdatedVar(loc);
            for (DataIter ptsIt = pts.begin(), ptsEit = pts.end(); ptsIt != ptsEit; ++ptsIt) {
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
    /// Update points-to of top-level pointers with IN[srcLoc:srcVar]
    virtual inline bool updateTLVPts(LocID srcLoc, const Key& srcVar, const Key& dstVar) {
        if(varHasNewDFInPts(srcLoc,srcVar)) {
            removeVarFromDFInUpdatedSet(srcLoc,srcVar);
            return this->unionPts(dstVar, this->getDFInPtsSet(srcLoc,srcVar));
        }
        return false;
    }
    /// Update address-taken variables OUT[dstLoc:dstVar] with points-to of top-level pointers
    virtual inline bool updateATVPts(const Key& srcVar, LocID dstLoc, const Key& dstVar) {
        if (this->unionPts(this->getDFOutPtsSet(dstLoc, dstVar), this->getPts(srcVar))) {
            setVarDFOutSetUpdated(dstLoc, dstVar);
            return true;
        }
        return false;
    }
    //@}

    ///Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IncDFPTData<Key,Data> *) {
        return true;
    }
    static inline bool classof(const DFPTData<Key,Data> * ptd) {
        return ptd->getPTDTY() == PTData<Key,Data>::IncDFPTD;
    }
    static inline bool classof(const PTData<Key,Data>* ptd) {
        return ptd->getPTDTY() == PTData<Key,Data>::IncDFPTD ||
               ptd->getPTDTY() == PTData<Key,Data>::DFPTD;
    }
    //@}

    inline void clearAllDFOutUpdatedVar(LocID loc) {
        if (this->hasDFOutSet(loc)) {
            Data pts = getDFOutUpdatedVar(loc);
            for (DataIter ptsIt = pts.begin(), ptsEit = pts.end(); ptsIt != ptsEit; ++ptsIt) {
                const Key var = *ptsIt;
                removeVarFromDFOutUpdatedSet(loc, var);
            }
        }
    }
private:
    /// Handle address-taken variables whose IN pts changed
    //@{
    /// Add var into loc's IN updated set. Called when var's pts in loc's IN set changed
    inline void setVarDFInSetUpdated(LocID loc,const Key& var) {
        inUpdatedVarMap[loc].set(var);
    }
    /// Remove var from loc's IN updated set
    inline void removeVarFromDFInUpdatedSet(LocID loc,const Key& var) {
        UpdatedVarMapIter it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end())
            it->second.reset(var);
    }
    /// Return TRUE if var has new pts in loc's IN set
    inline bool varHasNewDFInPts(LocID loc,const Key& var) {
        UpdatedVarMapIter it = inUpdatedVarMap.find(loc);
        if (it != inUpdatedVarMap.end())
            return it->second.test(var);
        return false;
    }
    /// Get all var which have new pts informationin loc's IN set
    inline const Data& getDFInUpdatedVar(LocID loc) {
        return inUpdatedVarMap[loc];
    }
    //@}

    /// Handle address-taken variables whose OUT pts changed
    //@{
    /// Add var into loc's OUT updated set. Called when var's pts in loc's OUT set changed
    inline void setVarDFOutSetUpdated(LocID loc,const Key& var) {
        outUpdatedVarMap[loc].set(var);
    }
    /// Remove var from loc's OUT updated set
    inline void removeVarFromDFOutUpdatedSet(LocID loc,const Key& var) {
        UpdatedVarMapIter it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end())
            it->second.reset(var);
    }
    /// Return TRUE if var has new pts in loc's OUT set
    inline bool varHasNewDFOutPts(LocID loc,const Key& var) {
        UpdatedVarMapIter it = outUpdatedVarMap.find(loc);
        if (it != outUpdatedVarMap.end())
            return it->second.test(var);
        return false;
    }
    /// Get all var which have new pts informationin loc's OUT set
    inline const Data& getDFOutUpdatedVar(LocID loc) {
        return outUpdatedVarMap[loc];
    }
    //@}
};

#endif /* POINTSTODSDF_H_ */
