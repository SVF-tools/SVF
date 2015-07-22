//===- CFLSolver.h -- CFL reachability solver---------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
#include <llvm/ADT/GraphTraits.h>

/*
 * Generic CFL solver for demand-driven analysis based on different graphs (e.g. PAG, VFG, ThreadVFG)
 * Extend this class for sophisticated CFL-reachability resolution (e.g. field, flow, path)
 */
template<class GraphType, class DPIm = DPItem>
class CFLSolver {

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
    CFLSolver(): _graph(NULL)
    {
    }
    /// Destructor
    virtual ~CFLSolver() {
    }
    /// Get/Set graph methods
    //@{
    const inline GraphType graph() const {
        return _graph;
    }
    inline void setGraph(GraphType g) {
        _graph = g;
    }
    //@}

    inline GNODE* getNode(NodeID id) const {
        return _graph->getGNode(id);
    }
    virtual inline NodeID getNodeIDFromItem(const DPIm& item) const {
        return item.getCurNodeID();
    }
    /// CFL forward traverse solve
    virtual void forwardTraverse(DPIm& it) {
        pushIntoWorklist(it);

        while (!isWorklistEmpty()) {
            DPIm item = popFromWorklist();
            forwardProcess(item);

            GNODE* v = getNode(getNodeIDFromItem(item));
            child_iterator EI = GTraits::child_begin(v);
            child_iterator EE = GTraits::child_end(v);
            for (; EI != EE; ++EI) {
                forwardpropagate(item,*(EI.getCurrent()) );
            }
        }
    }
    /// CFL forward traverse solve
    virtual void backwardTraverse(DPIm& it) {
        pushIntoWorklist(it);

        while (!isWorklistEmpty()) {
            DPIm item = popFromWorklist();
            backwardProcess(item);

            GNODE* v = getNode(getNodeIDFromItem(item));
            inv_child_iterator EI = InvGTraits::child_begin(v);
            inv_child_iterator EE = InvGTraits::child_end(v);
            for (; EI != EE; ++EI) {
                backwardpropagate(item,*(EI.getCurrent()) );
            }
        }
    }
    /// Process the DP item
    //@{
    virtual void forwardProcess(const DPIm& item) {
    }
    virtual void backwardProcess(const DPIm& item) {
    }
    //@}
    /// Propagation for the solving, to be implemented in the child class
    //@{
    virtual void forwardpropagate(const DPIm& item, GEDGE* edge) {
        DPIm newItem(item);
        newItem.setCurNodeID(edge->getDstID());
        pushIntoWorklist(newItem);
    }
    virtual void backwardpropagate(const DPIm& item, GEDGE* edge) {
        DPIm newItem(item);
        newItem.setCurNodeID(edge->getSrcID());
        pushIntoWorklist(newItem);
    }
    //@}
    /// Worklist operations
    //@{
    inline DPIm popFromWorklist() {
        return worklist.pop();
    }
    inline bool pushIntoWorklist(DPIm& item) {
        return worklist.push(item);
    }
    inline bool isWorklistEmpty() {
        return worklist.empty();
    }
    inline bool isInWorklist(DPIm& item) {
        return worklist.find(item);
    }
    //@}

private:

    /// Graph
    GraphType _graph;

    /// Worklist for resolution
    WorkList worklist;

};

#endif /* CFLSOLVER_H_ */
