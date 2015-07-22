//===- ConsGEdge.h -- Constraint graph edge-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * ConsGEdge.h
 *
 *  Created on: Mar 19, 2014
 *      Author: Yulei Sui
 */

#ifndef CONSGEDGE_H_
#define CONSGEDGE_H_

#include "MemoryModel/PAG.h"
#include "Util/WorkList.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/IR/CallSite.h>

#include <map>
#include <set>

class ConstraintNode;
/*!
 * Self-defined edge for constraint resolution
 * including add/remove/re-target, but all the operations do not affect original PAG Edges
 */
typedef GenericEdge<ConstraintNode> GenericConsEdgeTy;
class ConstraintEdge : public GenericConsEdgeTy {

public:
    /// five kinds of constraint graph edges
    /// Gep edge is used for field sensitivity
    enum ConstraintEdgeK {
        Addr, Copy, Store, Load, NormalGep, VariantGep
    };
private:
    EdgeID edgeId;
public:
    /// Constructor
    ConstraintEdge(ConstraintNode* s, ConstraintNode* d, ConstraintEdgeK k, EdgeID id = 0) : GenericConsEdgeTy(s,d,k),edgeId(id) {
    }
    /// Destructor
    ~ConstraintEdge() {
    }
    /// Return edge ID
    inline EdgeID getEdgeID() const {
        return edgeId;
    }
    /// ClassOf
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == Addr ||
               edge->getEdgeKind() == Copy ||
               edge->getEdgeKind() == Store ||
               edge->getEdgeKind() == Load ||
               edge->getEdgeKind() == NormalGep ||
               edge->getEdgeKind() == VariantGep;
    }
    /// Constraint edge type
    typedef GenericNode<ConstraintNode,ConstraintEdge>::GEdgeSetTy ConstraintEdgeSetTy;

};


/*!
 * Copy edge
 */
class AddrCGEdge: public ConstraintEdge {
private:
    AddrCGEdge();                      ///< place holder
    AddrCGEdge(const AddrCGEdge &);  ///< place holder
    void operator=(const AddrCGEdge &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const AddrCGEdge *) {
        return true;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == Addr;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == Addr;
    }
    //@}

    /// constructor
    AddrCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id);
};


/*!
 * Copy edge
 */
class CopyCGEdge: public ConstraintEdge {
private:
    CopyCGEdge();                      ///< place holder
    CopyCGEdge(const CopyCGEdge &);  ///< place holder
    void operator=(const CopyCGEdge &); ///< place holder
public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CopyCGEdge *) {
        return true;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == Copy;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == Copy;
    }
    //@}

    /// constructor
    CopyCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id) : ConstraintEdge(s,d,Copy,id) {
    }
};


/*!
 * Store edge
 */
class StoreCGEdge: public ConstraintEdge {
private:
    StoreCGEdge();                      ///< place holder
    StoreCGEdge(const StoreCGEdge &);  ///< place holder
    void operator=(const StoreCGEdge &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const StoreCGEdge *) {
        return true;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == Store;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == Store;
    }
    //@}

    /// constructor
    StoreCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id) : ConstraintEdge(s,d,Store,id) {
    }
};


/*!
 * Load edge
 */
class LoadCGEdge: public ConstraintEdge {
private:
    LoadCGEdge();                      ///< place holder
    LoadCGEdge(const LoadCGEdge &);  ///< place holder
    void operator=(const LoadCGEdge &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const LoadCGEdge *) {
        return true;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == Load;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == Load;
    }
    //@}

    /// Constructor
    LoadCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id) : ConstraintEdge(s,d,Load,id) {
    }
};


/*!
 * Gep edge
 */
class GepCGEdge: public ConstraintEdge {
private:
    GepCGEdge();                      ///< place holder
    GepCGEdge(const GepCGEdge &);  ///< place holder
    void operator=(const GepCGEdge &); ///< place holder

protected:

    /// Constructor
    GepCGEdge(ConstraintNode* s, ConstraintNode* d, ConstraintEdgeK k, EdgeID id)
        : ConstraintEdge(s,d,k,id) {

    }

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const GepCGEdge *) {
        return true;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == NormalGep ||
               edge->getEdgeKind() == VariantGep;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == NormalGep ||
               edge->getEdgeKind() == VariantGep;
    }
    //@}

};

/*!
 * Gep edge with fixed offset size
 */
class NormalGepCGEdge : public GepCGEdge {
private:
    NormalGepCGEdge();                      ///< place holder
    NormalGepCGEdge(const NormalGepCGEdge &);  ///< place holder
    void operator=(const NormalGepCGEdge &); ///< place holder

    LocationSet ls;	///< location set of the gep edge

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const NormalGepCGEdge *) {
        return true;
    }
    static inline bool classof(const GepCGEdge *edge) {
        return edge->getEdgeKind() == NormalGep;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == NormalGep;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == NormalGep;
    }
    //@}

    /// Constructor
    NormalGepCGEdge(ConstraintNode* s, ConstraintNode* d, const LocationSet& l, EdgeID id)
        : GepCGEdge(s,d,NormalGep,id), ls(l)
    {}

    /// Get location set of the gep edge
    inline const LocationSet& getLocationSet() const {
        return ls;
    }
};

/*!
 * Gep edge with variant offset size
 */
class VariantGepCGEdge : public GepCGEdge {
private:
    VariantGepCGEdge();                      ///< place holder
    VariantGepCGEdge(const VariantGepCGEdge &);  ///< place holder
    void operator=(const VariantGepCGEdge &); ///< place holder

public:
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const VariantGepCGEdge *) {
        return true;
    }
    static inline bool classof(const GepCGEdge *edge) {
        return edge->getEdgeKind() == VariantGep;
    }
    static inline bool classof(const ConstraintEdge *edge) {
        return edge->getEdgeKind() == VariantGep;
    }
    static inline bool classof(const GenericConsEdgeTy *edge) {
        return edge->getEdgeKind() == VariantGep;
    }
    //@}

    /// Constructor
    VariantGepCGEdge(ConstraintNode* s, ConstraintNode* d, EdgeID id)
        : GepCGEdge(s,d,VariantGep,id)
    {}
};


#endif /* CONSGEDGE_H_ */
