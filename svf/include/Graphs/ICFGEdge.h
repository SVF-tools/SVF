//===- ICFGEdge.h -- ICFG edge------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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

namespace SVF
{

class ICFGNode;
class CallPE;
class RetPE;

/*!
 * Interprocedural control-flow and value-flow edge, representing the control- and value-flow dependence between two nodes
 */
typedef GenericEdge<ICFGNode> GenericICFGEdgeTy;
class ICFGEdge : public GenericICFGEdgeTy
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    /// ten types of ICFG edge
    /// three types of control-flow edges
    /// seven types of value-flow edges
    enum ICFGEdgeK
    {
        IntraCF,
        CallCF,
        RetCF,
    };

    typedef ICFGEdgeK SVFGEdgeK;

public:
    /// Constructor
    ICFGEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k) : GenericICFGEdgeTy(s, d, k)
    {
    }
    /// Destructor
    ~ICFGEdge() {}

    /// Get methods of the components
    //@{
    inline bool isCFGEdge() const
    {
        return getEdgeKind() == IntraCF || getEdgeKind() == CallCF ||
               getEdgeKind() == RetCF;
    }
    inline bool isCallCFGEdge() const
    {
        return getEdgeKind() == CallCF;
    }
    inline bool isRetCFGEdge() const
    {
        return getEdgeKind() == RetCF;
    }
    inline bool isIntraCFGEdge() const
    {
        return getEdgeKind() == IntraCF;
    }
    //@}
    typedef GenericNode<ICFGNode, ICFGEdge>::GEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGEdgeSetTy SVFGEdgeSetTy;
    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs)
    {
        return (cs << EdgeKindMaskBits) | k;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<<(OutStream& o, const ICFGEdge& edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * Intra ICFG edge representing control-flows between basic blocks within a function
 */
class IntraCFGEdge : public ICFGEdge
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    /// Constructor
    IntraCFGEdge(ICFGNode* s, ICFGNode* d)
        : ICFGEdge(s, d, IntraCF), conditionVar(nullptr), branchCondVal(0)
    {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraCFGEdge*)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge* edge)
    {
        return edge->getEdgeKind() == IntraCF;
    }
    static inline bool classof(const GenericICFGEdgeTy* edge)
    {
        return edge->getEdgeKind() == IntraCF;
    }
    //@}

    const SVFValue* getCondition() const
    {
        return conditionVar;
    }

    s64_t getSuccessorCondValue() const
    {
        assert(getCondition() && "this is not a conditional branch edge");
        return branchCondVal;
    }

    void setBranchCondition(const SVFValue* c, s64_t bVal)
    {
        conditionVar = c;
        branchCondVal = bVal;
    }

    virtual const std::string toString() const;

private:
    /// conditionVar is a boolean (for if/else) or numeric condition variable
    /// (for switch).
    /// branchCondVal is the value when this condition should hold to execute
    /// this CFGEdge.
    /// E.g., Inst1: br %cmp label 0, label 1,
    ///       Inst2: label 0
    ///       Inst3: label 1;
    /// for edge between Inst1 and Inst 2, the first element is %cmp and
    /// the second element is 0
    const SVFValue* conditionVar;
    s64_t branchCondVal;
};

/*!
 * Call ICFG edge representing parameter passing/return from a caller to a callee
 */
class CallCFGEdge : public ICFGEdge
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const SVFInstruction* cs;
    std::vector<const CallPE*> callPEs;

public:
    /// Constructor
    CallCFGEdge(ICFGNode* s, ICFGNode* d, const SVFInstruction* c)
        : ICFGEdge(s, d, CallCF), cs(c)
    {
    }
    /// Return callsite ID
    inline const SVFInstruction* getCallSite() const
    {
        return cs;
    }
    /// Add call parameter edge to this CallCFGEdge
    inline void addCallPE(const CallPE* callPE)
    {
        callPEs.push_back(callPE);
    }
    /// Add get parameter edge to this CallCFGEdge
    inline const std::vector<const CallPE*>& getCallPEs() const
    {
        return callPEs;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallCFGEdge*)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge* edge)
    {
        return edge->getEdgeKind() == CallCF;
    }
    static inline bool classof(const GenericICFGEdgeTy* edge)
    {
        return edge->getEdgeKind() == CallCF;
    }
    //@}
    virtual const std::string toString() const;
};

/*!
 * Return ICFG edge representing parameter/return passing from a callee to a caller
 */
class RetCFGEdge : public ICFGEdge
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const SVFInstruction* cs;
    const RetPE* retPE;

public:
    /// Constructor
    RetCFGEdge(ICFGNode* s, ICFGNode* d, const SVFInstruction* c)
        : ICFGEdge(s, d, RetCF), cs(c), retPE(nullptr)
    {
    }
    /// Return callsite ID
    inline const SVFInstruction* getCallSite() const
    {
        return cs;
    }
    /// Add call parameter edge to this CallCFGEdge
    inline void addRetPE(const RetPE* ret)
    {
        assert(!retPE && "we can only have one retPE for each RetCFGEdge");
        retPE = ret;
    }
    /// Add get parameter edge to this CallCFGEdge
    inline const RetPE* getRetPE() const
    {
        return retPE;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetCFGEdge*)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge* edge)
    {
        return edge->getEdgeKind() == RetCF;
    }
    static inline bool classof(const GenericICFGEdgeTy* edge)
    {
        return edge->getEdgeKind() == RetCF;
    }
    //@}
    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* ICFGEdge_H_ */
