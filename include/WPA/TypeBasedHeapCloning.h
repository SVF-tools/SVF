//===- TypeBasedHeapCloning.h -- Type filter/type-based heap cloning base ------------//

/*
 * TypeBasedHeapCloning.h
 *
 * Contains data structures and functions to extend a pointer analysis
 * with type-based heap cloning/type filtering.
 *
 *  Created on: Feb 08, 2020
 *      Author: Mohamad Barbar
 */

#include "SVF-FE/DCHG.h"
#include "MemoryModel/SVFIR.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Util/BasicTypes.h"

namespace SVF
{

class TypeBasedHeapCloning
{
public:
    /// Returns raw ctir metadata of a Value. Returns null if it doesn't exist.
    static const MDNode *getRawCTirMetadata(const Value *);

    virtual ~TypeBasedHeapCloning() { };

protected:
    /// The undefined type (•); void.
    static const DIType *undefType;

    /// deref function for TBHC alias tests.
    static const std::string derefFnName;
    /// deref function (mangled) for TBHC alias tests.
    static const std::string mangledDerefFnName;

    /// Constructor. pta is the pointer analysis using this object (i.e. that which is extending).
    TypeBasedHeapCloning(BVDataPTAImpl *pta);

    /// Required by user. Handles back-propagation of newly created clone after all
    /// metadata has been set. Used by cloneObject.
    virtual void backPropagate(NodeID clone) = 0;

    /// Class hierarchy graph built from debug information. Required, CHG from
    /// IR is insufficient.
    DCHGraph *dchg = nullptr;

    /// DCHG *must* be set by extending class once the DCHG is available.
    void setDCHG(DCHGraph *dchg);
    /// SVFIR *must* be set by extending class once the SVFIR is available.
    void setPAG(SVFIR *pag);

    /// Check if an object is a black hole obj or a constant object. Required since
    /// other implementations obviously do not account for clones.
    bool isBlkObjOrConstantObj(NodeID o) const;

    /// Wrapper around DCHGraph::isBase. Purpose is to keep our conditions clean
    /// by only passing two parameters like the rules.
    bool isBase(const DIType *a, const DIType *b) const;

    /// Returns true if o is a clone.
    bool isClone(NodeID o) const;

    /// Sets the type (in objToType) of o.
    void setType(NodeID o, const DIType *t);
    /// Returns the type (from objToType) of o. Asserts existence.
    const DIType *getType(NodeID o) const;

    /// Sets the allocation site (in objToAllocation) of o.
    void setAllocationSite(NodeID o, NodeID site);
    /// Returns the allocation site (from objToAllocation) of o. Asserts existence.
    NodeID getAllocationSite(NodeID o) const;

    /// Returns objects that have clones (any key in objToClones).
    const NodeBS getObjsWithClones(void);
    /// Add a clone c to object o.
    void addClone(NodeID o, NodeID c);
    /// Returns all the clones of o.
    const NodeBS &getClones(NodeID o);

    // Set o as the original object of clone c.
    void setOriginalObj(NodeID c, NodeID o);
    /// Returns the original object c is cloned from. If c is not a clone, returns itself.
    NodeID getOriginalObj(NodeID c) const;

    /// Returns the filter set of a location. Not const; could create empty PointsTo.
    PointsTo &getFilterSet(NodeID loc);

    /// Associates gep with base (through objToGeps and memObjToGeps).
    void addGepToObj(NodeID gep, NodeID base, unsigned offset);
    /// Returns all gep objects at a particular offset for memory object.
    /// Not const; could create empty set.
    const NodeBS &getGepObjsFromMemObj(const MemObj *memObj, unsigned offset);
    /// Returns all gep objects under an object.
    /// Not const; could create empty set.
    const NodeBS &getGepObjs(NodeID base);

    /// Returns the GEP object node(s) of base for ls. This may include clones.
    /// If there are no GEP objects, then getGepObjVar is called on the SVFIR
    /// (through base's getGepObjVar) which will create one.
    const NodeBS getGepObjClones(NodeID base, unsigned offset);

    /// Initialise the pointees of p at loc (which is type tildet *). reuse indicates
    /// whether reuse is a possibility for this initialisation. Returns whether p changed.
    bool init(NodeID loc, NodeID p, const DIType *tildet, bool reuse, bool gep=false);

    /// Returns a clone of o with type type. reuse indicates whether we are cloning
    /// as a result of reuse.
    NodeID cloneObject(NodeID o, const DIType *type, bool reuse);

    /// Add clone dummy object node to SVFIR.
    inline NodeID addCloneDummyObjNode(const MemObj *mem);
    /// Add clone GEP object node to SVFIR.
    inline NodeID addCloneGepObjNode(const MemObj *mem, const LocationSet &l);
    /// Add clone FI object node to SVFIR.
    inline NodeID addCloneFIObjNode(const MemObj *mem);

    /// Returns the ctir type attached to the value, nullptr if non-existant.
    /// Not static because it needs the DCHG to return the canonical type.
    /// Not static because we need dchg's getCanonicalType.
    const DIType *getTypeFromCTirMetadata(const Value *);

    /// Runs tests on MAYALIAS, NOALIAS, etc. built from TBHC_MAYALIAS,
    /// TBHC_NOALIAS, etc. macros.
    /// TBHC_XALIAS macros produce:
    ///   call XALIAS(...)
    ///   %1 = load ...
    ///   ...
    ///   %n = load %p
    ///   store ... %n-1, ...* %n !ctir !t1
    ///   call deref()
    ///   %n+1 = load ...
    ///   ...
    ///   %n+n = load %q
    ///   store ... %n+n-1, ...* %n+n !ctir !t2
    ///   call deref()
    /// We want to test the points-to sets of %n and
    /// %n+n after filtering with !t1 and !t2 respectively.
    void validateTBHCTests(SVFModule *svfMod);

    /// Dump some statistics we tracked.
    void dumpStats(void);

private:
    /// PTA extending this class.
    BVDataPTAImpl *pta;
    /// SVFIR the PTA uses. Just a shortcut for getPAG().
    SVFIR *ppag = nullptr;

    /// Object -> its type.
    Map<NodeID, const DIType *> objToType;
    /// Object -> allocation site.
    /// The value NodeID depends on the pointer analysis (could be
    /// an SVFG node or SVFIR node for example).
    Map<NodeID, NodeID> objToAllocation;
    /// (Original) object -> set of its clones.
    Map<NodeID, NodeBS> objToClones;
    /// (Clone) object -> original object (opposite of objToclones).
    Map<NodeID, NodeID> cloneToOriginalObj;
    /// Maps nodes (a location like a SVFIR node or SVFG node) to their filter set.
    Map<NodeID, PointsTo> locToFilterSet;
    /// Maps objects to the GEP nodes beneath them.
    Map<NodeID, NodeBS> objToGeps;
    /// Maps memory objects to their GEP objects. (memobj -> (fieldidx -> geps))
    Map<const MemObj *, Map<unsigned, NodeBS>> memObjToGeps;

    /// Test whether object is a GEP object. For convenience.
    bool isGep(const PAGNode *n) const;

    // Bunch of stats to keep track of.
    unsigned numInit  = 0;
    unsigned numTBWU  = 0;
    unsigned numTBSSU = 0;
    unsigned numTBSU  = 0;
    unsigned numReuse = 0;
    unsigned numAgg   = 0;

    // Previous stats but only upon stack/global objects.
    unsigned numSGInit  = 0;
    unsigned numSGTBWU  = 0;
    unsigned numSGTBSSU = 0;
    unsigned numSGTBSU  = 0;
    unsigned numSGReuse = 0;
    unsigned numSGAgg   = 0;
};

} // End namespace SVF
