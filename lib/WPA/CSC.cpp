//===- CSC.cpp -- Cycle Stride Calculation algorithm---------------------------------------//
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
 * CSC.cpp
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#include "WPA/CSC.h"
#include "Util/SVFUtil.h"

using namespace SVFUtil;


void CSC::clear() {
    _I = 0;
    _D.clear();
    _S.clear();
}

/*!
 *
 */
void CSC::find(NodeStack& candidates) {
    clear();

    NodeStack revCandidates;
    while (!candidates.empty()) {
        NodeID nId = candidates.top();
        revCandidates.push(nId);
        candidates.pop();

        if (_scc->subNodes(nId).count()>1) {
//            _pwcReps.insert(nId);

            if (!_S.find(nId)) {
                _D[nId] = _I;
                _S.push(nId);

                ConstraintNode* node = _consG->getConstraintNode(nId);
                for (ConstraintNode::const_iterator eit = node->directOutEdgeBegin(); eit != node->directOutEdgeEnd(); ++eit) {
                    NodeID dstId = (*eit)->getDstID();
                    if (_scc->repNode(nId) == _scc->repNode(dstId) && nId != dstId)
                        visit(dstId, *eit);
                }
            }
        }
    }

    while (!revCandidates.empty()) {
        NodeID nId = revCandidates.top();
        candidates.push(nId);
        revCandidates.pop();
    }
}

/*!
 *
 */
void CSC::visit(NodeID nodeId, const ConstraintEdge* edge) {
    if (const NormalGepCGEdge* gepCGEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge)) {
        Size_t _w = gepCGEdge->getLocationSet().getOffset();
        _I += _w;
    }
    _D[nodeId] = _I;
    _S.push(nodeId);

    ConstraintNode* node = _consG->getConstraintNode(nodeId);
    for (ConstraintNode::const_iterator eit = node->directOutEdgeBegin(); eit != node->directOutEdgeEnd(); ++eit) {
        NodeID dstId = (*eit)->getDstID();
        if (_scc->repNode(nodeId) == _scc->repNode(dstId) && nodeId != dstId)
            visit(dstId, *eit);
    }

    WorkStack _revS;
    NodeSet _C;
    _revS.clear();
    _C.clear();

    while (!_S.empty()) {
        NodeID backNodeId = _S.pop();
        _revS.push(backNodeId);
        ConstraintNode* backNode = _consG->getConstraintNode(backNodeId);
        if (_consG->hasEdge(node, backNode, ConstraintEdge::NormalGep)) {
            NormalGepCGEdge* normalGep = SVFUtil::dyn_cast<NormalGepCGEdge>(_consG->getEdge(node, backNode, ConstraintEdge::NormalGep));
            Size_t _w = normalGep->getLocationSet().getOffset();
            Size_t _l = _I+_w-_D[backNodeId];
            _nodeStrides[backNodeId].set(_l);
            for (auto cNodeId : _C)
                _nodeStrides[cNodeId].set(_l);
        } else if (_consG->hasEdge(node, backNode, ConstraintEdge::VariantGep) ||
                   _consG->hasEdge(node, backNode, ConstraintEdge::Copy)) {
            Size_t _l = _I-_D[backNodeId];
            _nodeStrides[backNodeId].set(_l);
            for (auto cNodeId : _C)
                _nodeStrides[cNodeId].set(_l);
        }
        _C.insert(backNodeId);
    }

    while (!_revS.empty()) {
        NodeID visitedId = _revS.pop();
        _S.push(visitedId);
    }

    _S.pop();
}