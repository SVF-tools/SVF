//===- VFGEdge.h ----------------------------------------------------------------//
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
 * VFGEdge.h
 *
 *  Created on: 18 Sep. 2018
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_UTIL_VFGEDGE_H_
#define INCLUDE_UTIL_VFGEDGE_H_

#include "Graphs/GenericGraph.h"

namespace SVF
{

class VFGNode;

/*!
 * Interprocedural control-flow and value-flow edge, representing the control- and value-flow dependence between two nodes
 */
typedef GenericEdge<VFGNode> GenericVFGEdgeTy;
class VFGEdge : public GenericVFGEdgeTy
{

public:
    /// seven types of ICFG edge
    /// four types of direct value-flow edges
    /// three types of indirect value-flow edges
    enum VFGEdgeK
    {
        IntraDirectVF,
        IntraIndirectVF,
        CallDirVF,
        RetDirVF,
        CallIndVF,
        RetIndVF,
        TheadMHPIndirectVF
    };

    typedef VFGEdgeK SVFGEdgeK;

public:
    /// Constructor
    VFGEdge(VFGNode* s, VFGNode* d, GEdgeFlag k) : GenericVFGEdgeTy(s,d,k)
    {
    }
    /// Destructor
    ~VFGEdge()
    {
    }

    /// Get methods of the components
    //@{
    inline bool isDirectVFGEdge() const
    {
        return getEdgeKind() == IntraDirectVF || getEdgeKind() == CallDirVF || getEdgeKind() == RetDirVF;
    }
    inline bool isIndirectVFGEdge() const
    {
        return getEdgeKind() == IntraIndirectVF || getEdgeKind() == CallIndVF || getEdgeKind() == RetIndVF || getEdgeKind() == TheadMHPIndirectVF;
    }
    inline bool isCallVFGEdge() const
    {
        return getEdgeKind() == CallDirVF || getEdgeKind() == CallIndVF;
    }
    inline bool isRetVFGEdge() const
    {
        return getEdgeKind() == RetDirVF || getEdgeKind() == RetIndVF;
    }
    inline bool isCallDirectVFGEdge() const
    {
        return getEdgeKind() == CallDirVF;
    }
    inline bool isRetDirectVFGEdge() const
    {
        return getEdgeKind() == RetDirVF;
    }
    inline bool isCallIndirectVFGEdge() const
    {
        return getEdgeKind() == CallIndVF;
    }
    inline bool isRetIndirectVFGEdge() const
    {
        return getEdgeKind() == RetIndVF;
    }
    inline bool isIntraVFGEdge() const
    {
        return getEdgeKind() == IntraDirectVF || getEdgeKind() == IntraIndirectVF;
    }
    inline bool isThreadMHPIndirectVFGEdge() const
    {
        return getEdgeKind() == TheadMHPIndirectVF;
    }
    //@}
    typedef GenericNode<VFGNode,VFGEdge>::GEdgeSetTy VFGEdgeSetTy;
    typedef VFGEdgeSetTy SVFGEdgeSetTy;
    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs)
    {
        return (cs << EdgeKindMaskBits) | k;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const VFGEdge &edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;
};


/*!
 * SVFG edge representing direct value-flows
 */
class DirectSVFGEdge : public VFGEdge
{

public:
    /// Constructor
    DirectSVFGEdge(VFGNode* s, VFGNode* d, GEdgeFlag k): VFGEdge(s,d,k)
    {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const DirectSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirVF ||
               edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == IntraDirectVF  ||
               edge->getEdgeKind() == CallDirVF ||
               edge->getEdgeKind() == RetDirVF;
    }
    //@}

    virtual const std::string toString() const;
};


/*!
 * Intra SVFG edge representing direct intra-procedural value-flows
 */
class IntraDirSVFGEdge : public DirectSVFGEdge
{

public:
    /// Constructor
    IntraDirSVFGEdge(VFGNode* s, VFGNode* d): DirectSVFGEdge(s,d,IntraDirectVF)
    {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraDirSVFGEdge*)
    {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == IntraDirectVF;
    }
    //@}

    virtual const std::string toString() const;
};


/*!
 * SVFG call edge representing direct value-flows from a caller to its callee at a callsite
 */
class CallDirSVFGEdge : public DirectSVFGEdge
{

private:
    CallSiteID csId;
public:
    /// Constructor
    CallDirSVFGEdge(VFGNode* s, VFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(CallDirVF,id)),csId(id)
    {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const
    {
        return csId;
    }

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallDirSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == CallDirVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == CallDirVF ;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == CallDirVF ;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * SVFG return edge connecting direct value-flows from a callee to its caller at a callsite
 */
class RetDirSVFGEdge : public DirectSVFGEdge
{

private:
    CallSiteID csId;
public:
    /// Constructor
    RetDirSVFGEdge(VFGNode* s, VFGNode* d, CallSiteID id):
        DirectSVFGEdge(s,d,makeEdgeFlagWithInvokeID(RetDirVF,id)),csId(id)
    {
    }
    /// Return callsite ID
    inline CallSiteID getCallSiteId() const
    {
        return csId;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetDirSVFGEdge *)
    {
        return true;
    }
    static inline bool classof(const DirectSVFGEdge *edge)
    {
        return edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const VFGEdge *edge)
    {
        return edge->getEdgeKind() == RetDirVF;
    }
    static inline bool classof(const GenericVFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == RetDirVF;
    }
    //@}

    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* INCLUDE_UTIL_VFGEDGE_H_ */
