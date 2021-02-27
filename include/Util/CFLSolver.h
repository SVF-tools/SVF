//===- CFLSolver.h -- CFL reachability solver---------------------------------//
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
 * CFLSolver.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef CFLSOLVER_H_
#define CFLSOLVER_H_

#include "Util/WorkList.h"
#include "Util/DPItem.h"

namespace SVF
{

/*
 * Generic CFL solver for demand-driven analysis based on different graphs (e.g. PAG, VFG, ThreadVFG)
 * Extend this class for sophisticated CFL-reachability resolution (e.g. field, flow, path)
 */
template<class GraphType, class DPIm = DPItem>
class CFLSolver
{

public:
    ///Define the GTraits and node iterator
    typedef llvm::GraphTraits<GraphType> GTraits;
    typedef typename GTraits::NodeType          GNODE;
    typedef typename GTraits::EdgeType          GEDGE;
    typedef typename GTraits::nodes_iterator node_iterator;
    typedef typename GTraits::ChildIteratorType child_iterator;

    /// Define inverse GTraits and note iterator
    typedef llvm::GraphTraits<llvm::Inverse<GNODE *> > InvGTraits;
    typedef typename InvGTraits::ChildIteratorType inv_child_iterator;

    /// Define worklist
    typedef FIFOWorkList<DPIm> WorkList;

protected:

    /// Constructor
    CFLSolver(): _graph(nullptr)
    {
    }
    /// Destructor
    virtual ~CFLSolver()
    {
    }
    /// Get/Set graph methods
    //@{
    const inline GraphType graph() const
    {
        return _graph;
    }
    inline void setGraph(GraphType g)
    {
        _graph = g;
    }
    //@}

    inline GNODE* getNode(NodeID id) const
    {
        return _graph->getGNode(id);
    }
    virtual inline NodeID getNodeIDFromItem(const DPIm& item) const
    {
        return item.getCurNodeID();
    }
    /// CFL forward traverse solve
    virtual void forwardTraverse(DPIm& it)
    {
        pushIntoWorklist(it);

        while (!isWorklistEmpty())
        {
            DPIm item = popFromWorklist();
            FWProcessCurNode(item);

            GNODE* v = getNode(getNodeIDFromItem(item));
            child_iterator EI = GTraits::child_begin(v);
            child_iterator EE = GTraits::child_end(v);
            for (; EI != EE; ++EI)
            {
                FWProcessOutgoingEdge(item,*(EI.getCurrent()) );
            }
        }
    }
    /// CFL forward traverse solve
    virtual void backwardTraverse(DPIm& it)
    {
        pushIntoWorklist(it);

        while (!isWorklistEmpty())
        {
            DPIm item = popFromWorklist();
            BWProcessCurNode(item);

            GNODE* v = getNode(getNodeIDFromItem(item));
            inv_child_iterator EI = InvGTraits::child_begin(v);
            inv_child_iterator EE = InvGTraits::child_end(v);
            for (; EI != EE; ++EI)
            {
                BWProcessIncomingEdge(item,*(EI.getCurrent()) );
            }
        }
    }
    /// Process the DP item
    //@{
    virtual void FWProcessCurNode(const DPIm&)
    {
    }
    virtual void BWProcessCurNode(const DPIm&)
    {
    }
    //@}
    /// Propagation for the solving, to be implemented in the child class
    //@{
    virtual void FWProcessOutgoingEdge(const DPIm& item, GEDGE* edge)
    {
        DPIm newItem(item);
        newItem.setCurNodeID(edge->getDstID());
        pushIntoWorklist(newItem);
    }
    virtual void BWProcessIncomingEdge(const DPIm& item, GEDGE* edge)
    {
        DPIm newItem(item);
        newItem.setCurNodeID(edge->getSrcID());
        pushIntoWorklist(newItem);
    }
    //@}
    /// Worklist operations
    //@{
    inline DPIm popFromWorklist()
    {
        return worklist.pop();
    }
    inline bool pushIntoWorklist(DPIm& item)
    {
        return worklist.push(item);
    }
    inline bool isWorklistEmpty()
    {
        return worklist.empty();
    }
    inline bool isInWorklist(DPIm& item)
    {
        return worklist.find(item);
    }
    //@}

private:

    /// Graph
    GraphType _graph;

    /// Worklist for resolution
    WorkList worklist;

};

} // End namespace SVF

#endif /* CFLSOLVER_H_ */
