//===- ConsGNode.h -- Constraint graph node-----------------------------------//
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
 * ConsGNode.h
 *
 *  Created on: Mar 19, 2014
 *      Author: Yulei Sui
 */

#ifndef CONSGNODE_H_
#define CONSGNODE_H_

/*!
 * Constraint node
 */
typedef GenericNode<ConstraintNode,ConstraintEdge> GenericConsNodeTy;
class ConstraintNode : public GenericConsNodeTy {

public:
    typedef ConstraintEdge::ConstraintEdgeSetTy::iterator iterator;
    typedef ConstraintEdge::ConstraintEdgeSetTy::const_iterator const_iterator;
private:
    bool _isPWCNode;

    ConstraintEdge::ConstraintEdgeSetTy loadInEdges; ///< all incoming load edge of this node
    ConstraintEdge::ConstraintEdgeSetTy loadOutEdges; ///< all outgoing load edge of this node

    ConstraintEdge::ConstraintEdgeSetTy storeInEdges; ///< all incoming store edge of this node
    ConstraintEdge::ConstraintEdgeSetTy storeOutEdges; ///< all outgoing store edge of this node

    /// Copy/call/ret/gep incoming edge of this node,
    /// To be noted: this set is only used when SCC detection, and node merges
    ConstraintEdge::ConstraintEdgeSetTy directInEdges;
    ConstraintEdge::ConstraintEdgeSetTy directOutEdges;

    ConstraintEdge::ConstraintEdgeSetTy addressInEdges; ///< all incoming address edge of this node
    ConstraintEdge::ConstraintEdgeSetTy addressOutEdges; ///< all outgoing address edge of this node

public:

    ConstraintNode(NodeID i): GenericConsNodeTy(i,0), _isPWCNode(false) {

    }

    /// Whether a node involves in PWC, if so, all its points-to elements should become field-insensitive.
    //@{
    inline bool isPWCNode() const {
        return _isPWCNode;
    }
    inline void setPWCNode() {
        _isPWCNode = true;
    }
    //@}

    /// Direct and Indirect PAG edges
    inline bool isdirectEdge(ConstraintEdge::ConstraintEdgeK kind) {
        return (kind == ConstraintEdge::Copy || kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep );
    }
    inline bool isIndirectEdge(ConstraintEdge::ConstraintEdgeK kind) {
        return (kind == ConstraintEdge::Load || kind == ConstraintEdge::Store);
    }

    ///  Iterators
    //@{
    inline iterator directOutEdgeBegin() {
        return directOutEdges.begin();
    }
    inline iterator directOutEdgeEnd() {
        return directOutEdges.end();
    }
    inline iterator directInEdgeBegin() {
        return directInEdges.begin();
    }
    inline iterator directInEdgeEnd() {
        return directInEdges.end();
    }

    inline const_iterator directOutEdgeBegin() const {
        return directOutEdges.begin();
    }
    inline const_iterator directOutEdgeEnd() const {
        return directOutEdges.end();
    }
    inline const_iterator directInEdgeBegin() const {
        return directInEdges.begin();
    }
    inline const_iterator directInEdgeEnd() const {
        return directInEdges.end();
    }

    ConstraintEdge::ConstraintEdgeSetTy& incomingAddrEdges() {
        return addressInEdges;
    }
    ConstraintEdge::ConstraintEdgeSetTy& outgoingAddrEdges() {
        return addressOutEdges;
    }

    inline const_iterator outgoingAddrsBegin() const {
        return addressOutEdges.begin();
    }
    inline const_iterator outgoingAddrsEnd() const {
        return addressOutEdges.end();
    }
    inline const_iterator incomingAddrsBegin() const {
        return addressInEdges.begin();
    }
    inline const_iterator incomingAddrsEnd() const {
        return addressInEdges.end();
    }

    inline const_iterator outgoingLoadsBegin() const {
        return loadOutEdges.begin();
    }
    inline const_iterator outgoingLoadsEnd() const {
        return loadOutEdges.end();
    }
    inline const_iterator incomingLoadsBegin() const {
        return loadInEdges.begin();
    }
    inline const_iterator incomingLoadsEnd() const {
        return loadInEdges.end();
    }

    inline const_iterator outgoingStoresBegin() const {
        return storeOutEdges.begin();
    }
    inline const_iterator outgoingStoresEnd() const {
        return storeOutEdges.end();
    }
    inline const_iterator incomingStoresBegin() const {
        return storeInEdges.begin();
    }
    inline const_iterator incomingStoresEnd() const {
        return storeInEdges.end();
    }
    //@}

    ///  Add constraint graph edges
    //@{
    inline void addIncomingCopyEdge(CopyCGEdge* inEdge) {
        addIncomingDirectEdge(inEdge);
    }
    inline void addIncomingGepEdge(GepCGEdge* inEdge) {
        addIncomingDirectEdge(inEdge);
    }
    inline void addOutgoingCopyEdge(CopyCGEdge* outEdge) {
        addOutgoingDirectEdge(outEdge);
    }
    inline void addOutgoingGepEdge(GepCGEdge* outEdge) {
        addOutgoingDirectEdge(outEdge);
    }
    inline void addIncomingAddrEdge(AddrCGEdge* inEdge) {
        addressInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline void addIncomingLoadEdge(LoadCGEdge* inEdge) {
        loadInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline void addIncomingStoreEdge(StoreCGEdge* inEdge) {
        storeInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline void addIncomingDirectEdge(ConstraintEdge* inEdge) {
        assert(inEdge->getDstID() == this->getId());
        bool added1 = directInEdges.insert(inEdge).second;
        bool added2 = addIncomingEdge(inEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingAddrEdge(AddrCGEdge* outEdge) {
        addressOutEdges.insert(outEdge);
        addOutgoingEdge(outEdge);
    }
    inline void addOutgoingLoadEdge(LoadCGEdge* outEdge) {
        bool added1 = loadOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingStoreEdge(StoreCGEdge* outEdge) {
        bool added1 = storeOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    inline void addOutgoingDirectEdge(ConstraintEdge* outEdge) {
        assert(outEdge->getSrcID() == this->getId());
        bool added1 = directOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        assert(added1 && added2 && "edge not added, duplicated adding!!");
    }
    //@}

    /// Remove constraint graph edges
    //{@
    inline void removeOutgoingAddrEdge(AddrCGEdge* outEdge) {
        Size_t num1 = addressOutEdges.erase(outEdge);
        Size_t num2 = removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingAddrEdge(AddrCGEdge* inEdge) {
        Size_t num1 = addressInEdges.erase(inEdge);
        Size_t num2 = removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingDirectEdge(ConstraintEdge* outEdge) {
        Size_t num1 = directOutEdges.erase(outEdge);
        Size_t num2 = removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingDirectEdge(ConstraintEdge* inEdge) {
        Size_t num1 = directInEdges.erase(inEdge);
        Size_t num2 = removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingLoadEdge(LoadCGEdge* outEdge) {
        Size_t num1 = loadOutEdges.erase(outEdge);
        Size_t num2 = removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingLoadEdge(LoadCGEdge* inEdge) {
        Size_t num1 = loadInEdges.erase(inEdge);
        Size_t num2 = removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeOutgoingStoreEdge(StoreCGEdge* outEdge) {
        Size_t num1 = storeOutEdges.erase(outEdge);
        Size_t num2 = removeOutgoingEdge(outEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }

    inline void removeIncomingStoreEdge(StoreCGEdge* inEdge) {
        Size_t num1 = storeInEdges.erase(inEdge);
        Size_t num2 = removeIncomingEdge(inEdge);
        assert((num1 && num2) && "edge not in the set, can not remove!!!");
    }
    //@}


};

#endif /* CONSGNODE_H_ */
