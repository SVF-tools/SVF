//===- MemPartition.h -- Memory region partition-----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * @file: DisnctMRGenerator.h
 * @author: yesen
 * @date: 07/12/2013
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#ifndef DISNCTMRGENERATOR_H_
#define DISNCTMRGENERATOR_H_

#include "MSSA/MemRegion.h"

/*!
 * Distinct memory region generator.
 */
class DistinctMRG : public MRGenerator {
public:
    DistinctMRG(BVDataPTAImpl* p) : MRGenerator(p)
    {}

    ~DistinctMRG() {}

protected:
    /// Partition regions
    virtual void partitionMRs();

    /// Get memory region at a load
    virtual void getMRsForLoad(MRSet& aliasMRs, const PointsTo& cpts, const llvm::Function* fun);

    /// Get memory regions to be inserted at a load statement.
    virtual void getMRsForCallSiteRef(MRSet& aliasMRs, const PointsTo& cpts, const llvm::Function* fun);
private:
    /// Create memory regions for each points-to target.
    void createDistinctMR(const llvm::Function* func, const PointsTo& cpts);

};

/*!
 * Create memory regions which don't have intersections with each other in the same function scope.
 */
class IntraDisjointMRG : public MRGenerator {
public:
    typedef std::map<PointsTo, PointsToList> PtsToSubPtsMap;
    typedef std::map<const llvm::Function*, PtsToSubPtsMap> FunToPtsMap;
    typedef std::map<const llvm::Function*, PointsToList> FunToInterMap;

    IntraDisjointMRG(BVDataPTAImpl* p) : MRGenerator(p)
    {}

    ~IntraDisjointMRG() {}

protected:

    /// Partition regions
    virtual void partitionMRs();

    /**
     * Get memory regions to be inserted at a load statement.
     * @param cpts The conditional points-to set of load statement.
     * @param fun The function being analyzed.
     * @param mrs Memory region set contains all possible target memory regions.
     */
    virtual inline void getMRsForLoad(MRSet& aliasMRs, const PointsTo& cpts,
                                      const llvm::Function* fun) {
        const PointsToList& inters = getIntersList(fun);
        getMRsForLoadFromInterList(aliasMRs, cpts, inters);
    }

    void getMRsForLoadFromInterList(MRSet& mrs, const PointsTo& cpts, const PointsToList& inters);

    /// Get memory regions to be inserted at a load statement.
    virtual void getMRsForCallSiteRef(MRSet& aliasMRs, const PointsTo& cpts, const llvm::Function* fun);

    /// Create disjoint memory region
    void createDisjointMR(const llvm::Function* func, const PointsTo& cpts);

    /// Compute intersections between cpts and computed cpts intersections before.
    void computeIntersections(const PointsTo& cpts, PointsToList& inters);

private:
    inline PtsToSubPtsMap& getPtsSubSetMap(const llvm::Function* func) {
        return funcToPtsMap[func];
    }

    inline PointsToList& getIntersList(const llvm::Function* func) {
        return funcToInterMap[func];
    }

    inline const PtsToSubPtsMap& getPtsSubSetMap(const llvm::Function* func) const {
        FunToPtsMap::const_iterator it = funcToPtsMap.find(func);
        assert(it != funcToPtsMap.end() && "can not find pts map for specified function");
        return it->second;
    }

    FunToPtsMap funcToPtsMap;
    FunToInterMap funcToInterMap;
};

/*!
 * Create memory regions which don't have intersections with each other in the whole program scope.
 */
class InterDisjointMRG : public IntraDisjointMRG {
public:
    InterDisjointMRG(BVDataPTAImpl* p) : IntraDisjointMRG(p)
    {}

    ~InterDisjointMRG() {}

protected:
    /// Partition regions
    virtual void partitionMRs();

    /**
     * Get memory regions to be inserted at a load statement.
     * @param cpts The conditional points-to set of load statement.
     * @param fun The function being analyzed.
     * @param mrs Memory region set contains all possible target memory regions.
     */
    virtual inline void getMRsForLoad(MRSet& aliasMRs, const PointsTo& cpts,
                                      const llvm::Function* fun) {
        getMRsForLoadFromInterList(aliasMRs, cpts, inters);
    }

private:
    PointsToList inters;
};

#endif /* DISNCTMRGENERATOR_H_ */
