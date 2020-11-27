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

namespace SVF
{

class ICFGNode;

/*!
 * Interprocedural control-flow and value-flow edge, representing the control- and value-flow dependence between two nodes
 */
typedef GenericEdge<ICFGNode> GenericICFGEdgeTy;
class ICFGEdge : public GenericICFGEdgeTy
{

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
    ICFGEdge(ICFGNode* s, ICFGNode* d, GEdgeFlag k) : GenericICFGEdgeTy(s,d,k)
    {
    }
    /// Destructor
    ~ICFGEdge()
    {
    }

    /// Get methods of the components
    //@{
    inline bool isCFGEdge() const
    {
        return getEdgeKind() == IntraCF || getEdgeKind() == CallCF || getEdgeKind() == RetCF;
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
    typedef GenericNode<ICFGNode,ICFGEdge>::GEdgeSetTy ICFGEdgeSetTy;
    typedef ICFGEdgeSetTy SVFGEdgeSetTy;
    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs)
    {
        return (cs << EdgeKindMaskBits) | k;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const ICFGEdge &edge)
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

public:
    /// the first element is a boolean (for if/else) or numeric condition value (for switch)
    /// the second element is the value when this condition should hold to execute this CFGEdge.
    /// e.g., Inst1: br %cmp label 0, label 1,  Inst2 is label 0 and Inst 3 is label 1;
    /// for edge between Inst1 and Inst 2, the first element is %cmp and second element is 0

    typedef std::pair<const Value*,NodeID> BranchCondition;

    /// Constructor
    IntraCFGEdge(ICFGNode* s, ICFGNode* d): ICFGEdge(s,d,IntraCF)
    {
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntraCFGEdge *)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge)
    {
        return edge->getEdgeKind() == IntraCF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == IntraCF;
    }
    //@}

    BranchCondition& getBranchCondtion(){
        return brCondition;
    }

    void setBranchCondtion(const Value* pNode, NodeID branchID){
        brCondition = std::make_pair(pNode,branchID);
    }

    virtual const std::string toString() const;

private:
    BranchCondition brCondition;

};



/*!
 * Call ICFG edge representing parameter passing/return from a caller to a callee
 */
class CallCFGEdge : public ICFGEdge
{

private:
	const Instruction*  cs;
public:
    /// Constructor
    CallCFGEdge(ICFGNode* s, ICFGNode* d, const Instruction*  c):
        ICFGEdge(s,d,CallCF),cs(c)
    {
    }
    /// Return callsite ID
    inline const Instruction*  getCallSite() const
    {
        return cs;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallCFGEdge *)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge)
    {
        return edge->getEdgeKind() == CallCF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge)
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

private:
	const Instruction*  cs;
public:
    /// Constructor
    RetCFGEdge(ICFGNode* s, ICFGNode* d, const Instruction*  c):
        ICFGEdge(s,d,RetCF),cs(c)
    {
    }
    /// Return callsite ID
    inline const Instruction*  getCallSite() const
    {
        return cs;
    }
    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const RetCFGEdge *)
    {
        return true;
    }
    static inline bool classof(const ICFGEdge *edge)
    {
        return edge->getEdgeKind() == RetCF;
    }
    static inline bool classof(const GenericICFGEdgeTy *edge)
    {
        return edge->getEdgeKind() == RetCF;
    }
    //@}
    virtual const std::string toString() const;
};

} // End namespace SVF

#endif /* ICFGEdge_H_ */
