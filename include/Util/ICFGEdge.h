//===- ICFGEdge.h -- ICFG edge------------------------------------------------//
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
 * ICFGEdge.h
 *
 *  Created on: Sep 11, 2018
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
    /// eight kinds of ICFG edge
	/// three types of control-flow edges
	/// five types of value-flow edges
    enum ICFGEdgeK {
    	IntraCF, CallDirCF, RetDirCF,
		IntraDirectVF, IntraIndirectVF,
		CallIndVF, RetIndVF, TheadMHPIndirectVF
    };

    typedef ICFGEdgeK SVFGEdgeK;

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
        return getEdgeKind() == IntraDirectVF || getEdgeKind() == CallDirCF || getEdgeKind() == RetDirCF;
    }
    inline bool isIndirectVFGEdge() const {
        return getEdgeKind() == IntraIndirectVF || getEdgeKind() == CallIndVF || getEdgeKind() == RetIndVF || getEdgeKind() == TheadMHPIndirectVF;
    }
    inline bool isCallVFGEdge() const {
        return getEdgeKind() == CallDirCF || getEdgeKind() == CallIndVF;
    }
    inline bool isRetVFGEdge() const {
        return getEdgeKind() == RetDirCF || getEdgeKind() == RetIndVF;
    }
    inline bool isCallDirectVFGEdge() const {
        return getEdgeKind() == CallDirCF;
    }
    inline bool isRetDirectVFGEdge() const {
        return getEdgeKind() == RetDirCF;
    }
    inline bool isCallIndirectVFGEdge() const {
        return getEdgeKind() == CallIndVF;
    }
    inline bool isRetIndirectVFGEdge() const {
        return getEdgeKind() == RetIndVF;
    }
    inline bool isIntraVFGEdge() const {
        return getEdgeKind() == IntraDirectVF || getEdgeKind() == IntraIndirectVF;
    }
    inline bool isThreadMHPIndirectVFGEdge() const {
        return getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}
    typedef GenericNode<ICFGNode,ICFGEdge>::GEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGEdgeSetTy SVFGEdgeSetTy;
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
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirCF ||
               edge->getEdgeKind() == RetDirCF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirCF ||
               edge->getEdgeKind() == RetDirCF;
    }
    //@}
};


/*!
 * Intra SVFG edge representing direct intra-procedural value-flows
 */
class IntraDirVFEdge : public DirectVFEdge {

public:
    /// Constructor
	IntraDirVFEdge(ICFGNode* s, ICFGNode* d): DirectVFEdge(s,d,IntraDirectVF) {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraDirVFEdge*) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
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
class CallDirCFEdge : public DirectVFEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    CallDirCFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectVFEdge(s,d,makeEdgeFlagWithInvokeID(CallDirCF,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallDirCFEdge *) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
        return edge->getEdgeKind() == CallDirCF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == CallDirCF ;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == CallDirCF ;
    }
    //@}
};

/*!
 * SVFG return edge connecting direct value-flows from a callee to its caller at a callsite
 */
class RetDirCFEdge : public DirectVFEdge {

private:
    CallSiteID csId;
public:
    /// Constructor
    RetDirCFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        DirectVFEdge(s,d,makeEdgeFlagWithInvokeID(RetDirCF,id)),csId(id) {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetDirCFEdge *) {
        return true;
    }
    static inline bool classof(const DirectVFEdge *edge) {
        return edge->getEdgeKind() == RetDirCF;
    }
    static inline bool classof(const ICFGEdge *edge) {
        return edge->getEdgeKind() == RetDirCF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge) {
        return edge->getEdgeKind() == RetDirCF;
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
class IntraIndVFEdge : public IndirectVFEdge {

public:
	IntraIndVFEdge(ICFGNode* s, ICFGNode* d): IndirectVFEdge(s,d,IntraIndirectVF) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const IntraIndVFEdge*) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
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
class CallIndVFEdge : public IndirectVFEdge {

private:
    CallSiteID csId;
public:
    CallIndVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectVFEdge(s,d,makeEdgeFlagWithInvokeID(CallIndVF,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const CallIndVFEdge *) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
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
class RetIndVFEdge : public IndirectVFEdge {

private:
    CallSiteID csId;
public:
    RetIndVFEdge(ICFGNode* s, ICFGNode* d, CallSiteID id):
        IndirectVFEdge(s,d,makeEdgeFlagWithInvokeID(RetIndVF,id)),csId(id) {
    }
    inline CallSiteID getCallSiteId() const {
        return csId;
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const RetIndVFEdge *) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
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
class ThreadMHPIndVFEdge : public IndirectVFEdge {

public:
	ThreadMHPIndVFEdge(ICFGNode* s, ICFGNode* d): IndirectVFEdge(s,d,TheadMHPIndirectVF) {
    }
    //@{ Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const ThreadMHPIndVFEdge*) {
        return true;
    }
    static inline bool classof(const IndirectVFEdge *edge) {
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

#endif /* ICFGEdge_H_ */
