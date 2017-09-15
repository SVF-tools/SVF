//===- SVFGEdge.h -- SVFG edge------------------------------------------------//
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
 * SVFGEdge.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef SVFGEDGE_H_
#define SVFGEDGE_H_


#include "MSSA/MemSSA.h"

class SVFGNode;

/*!
 * Sparse Value Flow Graph Edge, representing the value-flow dependence between two SVFG nodes
 */
typedef GenericEdge<SVFGNode> GenericSVFGEdgeTy;
class SVFGEdge : public GenericSVFGEdgeTy {

public:
    /// seven kinds of SVFG edge
    enum SVFGEdgeK {
        IntraDirect, IntraIndirect, DirCall, DirRet, IndCall, IndRet, TheadMHPIndirect
    };

public:
    /// Constructor
    SVFGEdge(SVFGNode* s, SVFGNode* d, GEdgeFlag k) : GenericSVFGEdgeTy(s,d,k) {
    }
    /// Destructor
    ~SVFGEdge() {
    }

    /// Get methods of the components
    //@{
    inline bool isDirectVFGEdge() const {
        return getEdgeKind() == IntraDirect || getEdgeKind() == DirCall || getEdgeKind() == DirRet;
    }
    inline bool isIndirectVFGEdge() const {
        return getEdgeKind() == IntraIndirect || getEdgeKind() == IndCall || getEdgeKind() == IndRet || getEdgeKind() == TheadMHPIndirect;
    }
    inline bool isCallVFGEdge() const {
        return getEdgeKind() == DirCall || getEdgeKind() == IndCall;
    }
    inline bool isRetVFGEdge() const {
        return getEdgeKind() == DirRet || getEdgeKind() == IndRet;
    }
    inline bool isCallDirectVFGEdge() const {
        return getEdgeKind() == DirCall;
    }
    inline bool isRetDirectVFGEdge() const {
        return getEdgeKind() == DirRet;
    }
    inline bool isCallIndirectVFGEdge() const {
        return getEdgeKind() == IndCall;
    }
    inline bool isRetIndirectVFGEdge() const {
        return getEdgeKind() == IndRet;
    }
    inline bool isIntraVFGEdge() const {
        return getEdgeKind() == IntraDirect || getEdgeKind() == IntraIndirect;
    }
    inline bool isThreadMHPIndirectVFGEdge() const {
        return getEdgeKind() == TheadMHPIndirect;
    }
    //@}
    typedef GenericNode<SVFGNode,SVFGEdge>::GEdgeSetTy SVFGEdgeSetTy;

    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs) {
        return (cs << EdgeKindMaskBits) | k;
    }
};



/*!
 * SVFG edge representing direct value-flows
 */
class DirectSVFGEdge : public SVFGEdge {

public:
    /// Constructor
    DirectSVFGEdge(SVFGNode* s, SVFGNode* d, GEdgeFlag k): SVFGEdge(s,d,k) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const DirectSVFGEdge *) {
        return true;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirect  ||
               edge->getEdgeKind() == DirCall ||
               edge->getEdgeKind() == DirRet;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraDirect  ||
               edge->getEdgeKind() == DirCall ||
               edge->getEdgeKind() == DirRet;
    }
    //@}
};


/*!
 * Intra SVFG edge representing direct intra-procedural value-flows
 */
class IntraDirSVFGEdge : public DirectSVFGEdge {

public:
    /// Constructor
    IntraDirSVFGEdge(SVFGNode* s, SVFGNode* d): DirectSVFGEdge(s,d,IntraDirect) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraDirSVFGEdge*) {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirect;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IntraDirect;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraDirect;
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
    CallDirSVFGEdge(SVFGNode* s, SVFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(DirCall,id)),csId(id) {
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
        return edge->getEdgeKind() == DirCall;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == DirCall ;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == DirCall ;
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
    RetDirSVFGEdge(SVFGNode* s, SVFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(DirRet,id)),csId(id) {
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
        return edge->getEdgeKind() == DirRet;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == DirRet;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == DirRet;
    }
    //@}
};

/*!
 * SVFG edge representing indirect value-flows from a caller to its callee at a callsite
 */
class IndirectSVFGEdge : public SVFGEdge {

public:
    typedef std::set<const MRVer*> MRVerSet;
private:
    MRVerSet mrs;
    PointsTo cpts;
public:
    /// Constructor
    IndirectSVFGEdge(SVFGNode* s, SVFGNode* d, GEdgeFlag k): SVFGEdge(s,d,k) {
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
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirect  ||
               edge->getEdgeKind() == IndCall ||
               edge->getEdgeKind() == IndRet ||
               edge->getEdgeKind() == TheadMHPIndirect;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraIndirect  ||
               edge->getEdgeKind() == IndCall ||
               edge->getEdgeKind() == IndRet ||
               edge->getEdgeKind() == TheadMHPIndirect;
    }
    //@}
};

/*!
 * Intra SVFG edge representing indirect intra-procedural value-flows
 */
class IntraIndSVFGEdge : public IndirectSVFGEdge {

public:
    IntraIndSVFGEdge(SVFGNode* s, SVFGNode* d): IndirectSVFGEdge(s,d,IntraIndirect) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntraIndSVFGEdge*) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirect;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IntraIndirect;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraIndirect;
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
    CallIndSVFGEdge(SVFGNode* s, SVFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(IndCall,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CallIndSVFGEdge *) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IndCall;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IndCall ;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IndCall ;
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
    RetIndSVFGEdge(SVFGNode* s, SVFGNode* d, CallSiteID id):
        IndirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(IndRet,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetIndSVFGEdge *) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == IndRet;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == IndRet;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == IndRet;
    }
    //@}
};


/*!
 * MHP SVFG edge representing indirect value-flows between
 * two memory access may-happen-in-parallel in multithreaded program
 */
class ThreadMHPIndSVFGEdge : public IndirectSVFGEdge {

public:
    ThreadMHPIndSVFGEdge(SVFGNode* s, SVFGNode* d): IndirectSVFGEdge(s,d,TheadMHPIndirect) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ThreadMHPIndSVFGEdge*) {
        return true;
    }
    static inline bool classof(const IndirectSVFGEdge *edge) {
        return edge->getEdgeKind() == TheadMHPIndirect;
    }
    static inline bool classof(const SVFGEdge *edge) {
        return edge->getEdgeKind() == TheadMHPIndirect;
    }
    static inline bool classof(const GenericSVFGEdgeTy *edge) {
        return edge->getEdgeKind() == TheadMHPIndirect;
    }
    //@}
};

#endif /* SVFGEDGE_H_ */
