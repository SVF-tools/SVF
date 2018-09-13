//===- SVFGEdge.h -- Sparse value-flow graph edge-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * SVFGEdge.h
 *
 *  Created on: 13 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_MSSA_SVFGEDGE_H_
#define INCLUDE_MSSA_SVFGEDGE_H_

#include "MSSA/MemSSA.h"
#include "Util/ICFGEdge.h"

/*!
 * SVFG edge representing direct value-flows
 */
class DirectSVFGEdge : public ICFGEdge {

public:
    /// Constructor
	DirectSVFGEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k): ICFGEdge(s,d,k) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const DirectSVFGEdge *) {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirVF ||
               edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirVF ||
               edge->getEdgeKind() == RetDirVF;
    }
    //@}
};


/*!
 * Intra SVFG edge representing direct intra-procedural value-flows
 */
class IntraDirSVFGEdge : public DirectSVFGEdge {

public:
    /// Constructor
	IntraDirSVFGEdge(ICFGNode* s, ICFGNode* d): DirectSVFGEdge(s,d,IntraDirectVF) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraDirSVFGEdge*) {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    //@}
};


/*!
 * SVFG call edge representing direct value-flows from a caller to its callee at a callsite
 */
class CallDirSVFGEdge : public DirectSVFGEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    CallDirSVFGEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(CallDirVF,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallDirSVFGEdge *) {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge) {
        return edge->getEdgeKind() == CallDirVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == CallDirVF ;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == CallDirVF ;
    }
    //@}
};

/*!
 * SVFG return edge connecting direct value-flows from a callee to its caller at a callsite
 */
class RetDirSVFGEdge : public DirectSVFGEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    RetDirSVFGEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(RetDirVF,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetDirSVFGEdge *) {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge) {
        return edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == RetDirVF;
    }
    //@}
};

/*!
 * SVFG edge representing indirect value-flows from a caller to its callee at a callsite
 */
class IndirectSVFGEdge : public ICFGEdge {

public:
    typedef std::set<const MRVer*> MRVerSet;
private:
    MRVerSet mrs;
    PointsTo cpts;
public:
    /// Constructor
    IndirectSVFGEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k): ICFGEdge(s,d,k) {
    }
    /// Handle memory region
    //@{
    inline bool addPointsTo(const PointsTo& c) {
        return (cpts |= c);
    }
    inline const PointsTo& getPointsTo() const {
        return cpts;
    }

    inline MRVerSet& getMRVer() {
        return mrs;
    }
    inline bool addMrVer(const MRVer* mr) {
        // collect memory regions' pts to edge;
        cpts |= mr->getMR()->getPointsTo();
        return mrs.insert(mr).second;
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IndirectSVFGEdge *) {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirectVF  ||
               edge->getEdgeKind() == CallIndVF ||
               edge->getEdgeKind() == RetIndVF ||
               edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraIndirectVF  ||
               edge->getEdgeKind() == CallIndVF ||
               edge->getEdgeKind() == RetIndVF ||
               edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}
};

/*!
 * Intra SVFG edge representing indirect intra-procedural value-flows
 */
class IntraIndSVFGEdge : public IndirectSVFGEdge {

public:
	IntraIndSVFGEdge(ICFGNode* s, ICFGNode* d): IndirectSVFGEdge(s,d,IntraIndirectVF) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntraIndSVFGEdge*) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    //@}
};

/*!
 * SVFG call edge representing indirect value-flows from a caller to its callee at a callsite
 */
class CallIndSVFGEdge : public IndirectSVFGEdge {

private:
    CallSiteID csId;
public:
    CallIndSVFGEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(CallIndVF,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CallIndSVFGEdge *) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == CallIndVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == CallIndVF ;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == CallIndVF ;
    }
    //@}
};

/*!
 * SVFG return edge representing direct value-flows from a callee to its caller at a callsite
 */
class RetIndSVFGEdge : public IndirectSVFGEdge {

private:
    CallSiteID csId;
public:
    RetIndSVFGEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(RetIndVF,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetIndSVFGEdge *) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == RetIndVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == RetIndVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == RetIndVF;
    }
    //@}
};


/*!
 * MHP SVFG edge representing indirect value-flows between
 * two memory access may-happen-in-parallel in multithreaded program
 */
class ThreadMHPIndSVFGEdge : public IndirectSVFGEdge {

public:
	ThreadMHPIndSVFGEdge(ICFGNode* s, ICFGNode* d): IndirectSVFGEdge(s,d,TheadMHPIndirectVF) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ThreadMHPIndSVFGEdge*) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}
};



#endif /* INCLUDE_MSSA_SVFGEDGE_H_ */
