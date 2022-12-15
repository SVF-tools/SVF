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
#include "Graphs/VFGEdge.h"

namespace SVF
{

/*!
 * SVFG edge representing indirect value-flows from a caller to its callee at a callsite
 */
class IndirectSVFGEdge : public VFGEdge
{

public:
    typedef Set<const MRVer*> MRVerSet;
private:
    NodeBS cpts;
public:
    /// Constructor
    IndirectSVFGEdge(VFGNode* s, VFGNode* d, GEdgeFlag k): VFGEdge(s,d,k)
    {
    }
    /// Handle memory region
    //@{
    inline bool addPointsTo(const NodeBS& c)
    {
        return (cpts |= c);
    }
    inline const NodeBS& getPointsTo() const
    {
        return cpts;
    }
    //@}

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IndirectSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraIndirectVF  ||
               edge->getEdgeKind() == CallIndVF ||
               edge->getEdgeKind() == RetIndVF ||
               edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == IntraIndirectVF  ||
               edge->getEdgeKind() == CallIndVF ||
               edge->getEdgeKind() == RetIndVF ||
               edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * Intra SVFG edge representing indirect intra-procedural value-flows
 */
class IntraIndSVFGEdge : public IndirectSVFGEdge
{

public:
    IntraIndSVFGEdge(VFGNode* s, VFGNode* d): IndirectSVFGEdge(s,d,IntraIndirectVF)
    {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntraIndSVFGEdge*)
    {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == IntraIndirectVF;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * SVFG call edge representing indirect value-flows from a caller to its callee at a callsite
 */
class CallIndSVFGEdge : public IndirectSVFGEdge
{

private:
    CallSiteID csId;
public:
    CallIndSVFGEdge(VFGNode* s, VFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(CallIndVF,id)),csId(id)
    {
    }
    inline CallSiteID getCallSiteId() const
    {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CallIndSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == CallIndVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == CallIndVF ;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == CallIndVF ;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * SVFG return edge representing direct value-flows from a callee to its caller at a callsite
 */
class RetIndSVFGEdge : public IndirectSVFGEdge
{

private:
    CallSiteID csId;
public:
    RetIndSVFGEdge(VFGNode* s, VFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(RetIndVF,id)),csId(id)
    {
    }
    inline CallSiteID getCallSiteId() const
    {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetIndSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == RetIndVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == RetIndVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == RetIndVF;
    }
    //@}

    virtual const std::string toString() const;
};


/*!
 * MHP SVFG edge representing indirect value-flows between
 * two memory access may-happen-in-parallel in multithreaded program
 */
class ThreadMHPIndSVFGEdge : public IndirectSVFGEdge
{

public:
    ThreadMHPIndSVFGEdge(VFGNode* s, VFGNode* d): IndirectSVFGEdge(s,d,TheadMHPIndirectVF)
    {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ThreadMHPIndSVFGEdge*)
    {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* INCLUDE_MSSA_SVFGEDGE_H_ */
