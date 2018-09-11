//===- ICFGEdge.h -- SVFG edge------------------------------------------------//
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
 * ICFGEdge.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef ICFGEdge_H_
#define ICFGEdge_H_


#include "MSSA/MemSSA.h"

class ICFGNode;

/*!
 * Interprocedural control-flow and value-flow edge, representing the control- and value-flow dependence between two nodes
 */
typedef GenericEdge<ICFGNode> GenericICFGEdgeTy;
class ICFGEdge : public GenericICFGEdgeTy {

public:
    /// seven kinds of SVFG edge
    enum ICFGEdgeK {
    		CFIntra, CFDirCall, CFDirRet, CFIndCall, CFIndRet,
		VFIntraDirect, VFIntraIndirect, VFDirCall, VFDirRet,
		VFIndCall, VFIndRet, VFTheadMHPIndirect
    };

public:
    /// Constructor
    ICFGEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k) : GenericICFGEdgeTy(s,d,k) {
    }
    /// Destructor
    ~ICFGEdge() {
    }

    /// Get methods of the components
    //@{
    inline bool isDirectVFGEdge() const {
        return getEdgeKind() == VFIntraDirect || getEdgeKind() == VFDirCall || getEdgeKind() == VFDirRet;
    }
    inline bool isIndirectVFGEdge() const {
        return getEdgeKind() == VFIntraIndirect || getEdgeKind() == VFIndCall || getEdgeKind() == VFIndRet || getEdgeKind() == VFTheadMHPIndirect;
    }
    inline bool isCallVFGEdge() const {
        return getEdgeKind() == VFDirCall || getEdgeKind() == VFIndCall;
    }
    inline bool isRetVFGEdge() const {
        return getEdgeKind() == VFDirRet || getEdgeKind() == VFIndRet;
    }
    inline bool isCallDirectVFGEdge() const {
        return getEdgeKind() == VFDirCall;
    }
    inline bool isRetDirectVFGEdge() const {
        return getEdgeKind() == VFDirRet;
    }
    inline bool isCallIndirectVFGEdge() const {
        return getEdgeKind() == VFIndCall;
    }
    inline bool isRetIndirectVFGEdge() const {
        return getEdgeKind() == VFIndRet;
    }
    inline bool isIntraVFGEdge() const {
        return getEdgeKind() == VFIntraDirect || getEdgeKind() == VFIntraIndirect;
    }
    inline bool isThreadMHPIndirectVFGEdge() const {
        return getEdgeKind() == VFTheadMHPIndirect;
    }
    //@}
    typedef GenericNode<ICFGNode,ICFGEdge>::GEdgeSetTy ICFGEdgeSetTy;

    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs) {
        return (cs << EdgeKindMaskBits) | k;
    }
};



/*!
 * SVFG edge representing direct value-flows
 */
class DirectVFEdge : public ICFGEdge {

public:
    /// Constructor
	DirectVFEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k): ICFGEdge(s,d,k) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const DirectVFEdge *) {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIntraDirect  ||
               edge->getEdgeKind() == VFDirCall ||
               edge->getEdgeKind() == VFDirRet;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIntraDirect  ||
               edge->getEdgeKind() == VFDirCall ||
               edge->getEdgeKind() == VFDirRet;
    }
    //@}
};


/*!
 * Intra SVFG edge representing direct intra-procedural value-flows
 */
class IntraDirVFEdge : public DirectVFEdge {

public:
    /// Constructor
	IntraDirVFEdge(ICFGNode* s, ICFGNode* d): DirectVFEdge(s,d,VFIntraDirect) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraDirVFEdge*) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
        return edge->getEdgeKind() == VFIntraDirect;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIntraDirect;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIntraDirect;
    }
    //@}
};


/*!
 * SVFG call edge representing direct value-flows from a caller to its callee at a callsite
 */
class CallDirVFEdge : public DirectVFEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    CallDirVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectVFEdge(s,d,makeEdgeFlagWithInvokeID(VFDirCall,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallDirVFEdge *) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
        return edge->getEdgeKind() == VFDirCall;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFDirCall ;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFDirCall ;
    }
    //@}
};

/*!
 * SVFG return edge connecting direct value-flows from a callee to its caller at a callsite
 */
class RetDirVFEdge : public DirectVFEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    RetDirVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectVFEdge(s,d,makeEdgeFlagWithInvokeID(VFDirRet,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetDirVFEdge *) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
        return edge->getEdgeKind() == VFDirRet;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFDirRet;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFDirRet;
    }
    //@}
};

/*!
 * SVFG edge representing indirect value-flows from a caller to its callee at a callsite
 */
class IndirectVFEdge : public ICFGEdge {

public:
    typedef std::set<const MRVer*> MRVerSet;
private:
    MRVerSet mrs;
    PointsTo cpts;
public:
    /// Constructor
    IndirectVFEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k): ICFGEdge(s,d,k) {
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
    static inline bool classof(const IndirectVFEdge *) {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIntraIndirect  ||
               edge->getEdgeKind() == VFIndCall ||
               edge->getEdgeKind() == VFIndRet ||
               edge->getEdgeKind() == VFTheadMHPIndirect;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIntraIndirect  ||
               edge->getEdgeKind() == VFIndCall ||
               edge->getEdgeKind() == VFIndRet ||
               edge->getEdgeKind() == VFTheadMHPIndirect;
    }
    //@}
};

/*!
 * Intra SVFG edge representing indirect intra-procedural value-flows
 */
class IntraIndVFEdge : public IndirectVFEdge {

public:
	IntraIndVFEdge(ICFGNode* s, ICFGNode* d): IndirectVFEdge(s,d,VFIntraIndirect) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntraIndVFEdge*) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
        return edge->getEdgeKind() == VFIntraIndirect;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIntraIndirect;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIntraIndirect;
    }
    //@}
};

/*!
 * SVFG call edge representing indirect value-flows from a caller to its callee at a callsite
 */
class CallIndVFEdge : public IndirectVFEdge {

private:
    CallSiteID csId;
public:
    CallIndVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectVFEdge(s,d,makeEdgeFlagWithInvokeID(VFIndCall,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CallIndVFEdge *) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
        return edge->getEdgeKind() == VFIndCall;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIndCall ;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIndCall ;
    }
    //@}
};

/*!
 * SVFG return edge representing direct value-flows from a callee to its caller at a callsite
 */
class RetIndVFEdge : public IndirectVFEdge {

private:
    CallSiteID csId;
public:
    RetIndVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectVFEdge(s,d,makeEdgeFlagWithInvokeID(VFIndRet,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetIndVFEdge *) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
        return edge->getEdgeKind() == VFIndRet;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFIndRet;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFIndRet;
    }
    //@}
};


/*!
 * MHP SVFG edge representing indirect value-flows between
 * two memory access may-happen-in-parallel in multithreaded program
 */
class ThreadMHPIndVFEdge : public IndirectVFEdge {

public:
	ThreadMHPIndVFEdge(ICFGNode* s, ICFGNode* d): IndirectVFEdge(s,d,VFTheadMHPIndirect) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ThreadMHPIndVFEdge*) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
        return edge->getEdgeKind() == VFTheadMHPIndirect;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == VFTheadMHPIndirect;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == VFTheadMHPIndirect;
    }
    //@}
};

#endif /* ICFGEdge_H_ */
