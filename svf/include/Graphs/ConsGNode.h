//===- ConsGNode.h -- Constraint graph node-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * ConsGNode.h
 *
 *  Created on: Mar 19, 2014
 *      Author: Yulei Sui
 */

#ifndef CONSGNODE_H_
#define CONSGNODE_H_

namespace SVF
{

/*!
 * Constraint node
 */
typedef GenericNode<ConstraintNode,ConstraintEdge> GenericConsNodeTy;
class ConstraintNode : public GenericConsNodeTy
{

public:
    typedef ConstraintEdge::ConstraintEdgeSetTy::iterator iterator;
    typedef ConstraintEdge::ConstraintEdgeSetTy::const_iterator const_iterator;
    bool _isPWCNode;

private:
    ConstraintEdge::ConstraintEdgeSetTy loadInEdges; ///< all incoming load edge of this node
    ConstraintEdge::ConstraintEdgeSetTy loadOutEdges; ///< all outgoing load edge of this node

    ConstraintEdge::ConstraintEdgeSetTy storeInEdges; ///< all incoming store edge of this node
    ConstraintEdge::ConstraintEdgeSetTy storeOutEdges; ///< all outgoing store edge of this node

    /// Copy/call/ret/gep incoming edge of this node,
    /// To be noted: this set is only used when SCC detection, and node merges
    ConstraintEdge::ConstraintEdgeSetTy directInEdges;
    ConstraintEdge::ConstraintEdgeSetTy directOutEdges;

    ConstraintEdge::ConstraintEdgeSetTy copyInEdges;
    ConstraintEdge::ConstraintEdgeSetTy copyOutEdges;

    ConstraintEdge::ConstraintEdgeSetTy gepInEdges;
    ConstraintEdge::ConstraintEdgeSetTy gepOutEdges;

    ConstraintEdge::ConstraintEdgeSetTy addressInEdges; ///< all incoming address edge of this node
    ConstraintEdge::ConstraintEdgeSetTy addressOutEdges; ///< all outgoing address edge of this node

public:
    /// For stride-based field representation
    NodeBS strides;
    NodeBS baseIds;

    ConstraintNode(NodeID i) : GenericConsNodeTy(i, 0), _isPWCNode(false)
    {

    }

    /// Whether a node involves in PWC, if so, all its points-to elements should become field-insensitive.
    //@{
    inline bool isPWCNode() const
    {
        return _isPWCNode;
    }
    inline void setPWCNode()
    {
        _isPWCNode = true;
    }
    //@}

    /// Direct and Indirect SVFIR edges
    inline bool isdirectEdge(ConstraintEdge::ConstraintEdgeK kind)
    {
        return (kind == ConstraintEdge::Copy || kind == ConstraintEdge::NormalGep || kind == ConstraintEdge::VariantGep );
    }
    inline bool isIndirectEdge(ConstraintEdge::ConstraintEdgeK kind)
    {
        return (kind == ConstraintEdge::Load || kind == ConstraintEdge::Store);
    }

    /// Return constraint edges
    //@{
    inline const ConstraintEdge::ConstraintEdgeSetTy& getDirectInEdges() const
    {
        return directInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getDirectOutEdges() const
    {
        return directOutEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getCopyInEdges() const
    {
        return copyInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getCopyOutEdges() const
    {
        return copyOutEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getGepInEdges() const
    {
        return gepInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getGepOutEdges() const
    {
        return gepOutEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getLoadInEdges() const
    {
        return loadInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getLoadOutEdges() const
    {
        return loadOutEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getStoreInEdges() const
    {
        return storeInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getStoreOutEdges() const
    {
        return storeOutEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getAddrInEdges() const
    {
        return addressInEdges;
    }
    inline const ConstraintEdge::ConstraintEdgeSetTy& getAddrOutEdges() const
    {
        return addressOutEdges;
    }
    //@}

    ///  Iterators
    //@{
    iterator directOutEdgeBegin();
    iterator directOutEdgeEnd();
    iterator directInEdgeBegin();
    iterator directInEdgeEnd();
    const_iterator directOutEdgeBegin() const;
    const_iterator directOutEdgeEnd() const;
    const_iterator directInEdgeBegin() const;
    const_iterator directInEdgeEnd() const;

    ConstraintEdge::ConstraintEdgeSetTy& incomingAddrEdges()
    {
        return addressInEdges;
    }
    ConstraintEdge::ConstraintEdgeSetTy& outgoingAddrEdges()
    {
        return addressOutEdges;
    }

    inline const_iterator outgoingAddrsBegin() const
    {
        return addressOutEdges.begin();
    }
    inline const_iterator outgoingAddrsEnd() const
    {
        return addressOutEdges.end();
    }
    inline const_iterator incomingAddrsBegin() const
    {
        return addressInEdges.begin();
    }
    inline const_iterator incomingAddrsEnd() const
    {
        return addressInEdges.end();
    }

    inline const_iterator outgoingLoadsBegin() const
    {
        return loadOutEdges.begin();
    }
    inline const_iterator outgoingLoadsEnd() const
    {
        return loadOutEdges.end();
    }
    inline const_iterator incomingLoadsBegin() const
    {
        return loadInEdges.begin();
    }
    inline const_iterator incomingLoadsEnd() const
    {
        return loadInEdges.end();
    }

    inline const_iterator outgoingStoresBegin() const
    {
        return storeOutEdges.begin();
    }
    inline const_iterator outgoingStoresEnd() const
    {
        return storeOutEdges.end();
    }
    inline const_iterator incomingStoresBegin() const
    {
        return storeInEdges.begin();
    }
    inline const_iterator incomingStoresEnd() const
    {
        return storeInEdges.end();
    }
    //@}

    ///  Add constraint graph edges
    //@{
    inline void addIncomingCopyEdge(CopyCGEdge *inEdge)
    {
        addIncomingDirectEdge(inEdge);
        copyInEdges.insert(inEdge);
    }
    inline void addIncomingGepEdge(GepCGEdge* inEdge)
    {
        addIncomingDirectEdge(inEdge);
        gepInEdges.insert(inEdge);
    }
    inline void addOutgoingCopyEdge(CopyCGEdge *outEdge)
    {
        addOutgoingDirectEdge(outEdge);
        copyOutEdges.insert(outEdge);
    }
    inline void addOutgoingGepEdge(GepCGEdge* outEdge)
    {
        addOutgoingDirectEdge(outEdge);
        gepOutEdges.insert(outEdge);
    }
    inline void addIncomingAddrEdge(AddrCGEdge* inEdge)
    {
        addressInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline void addIncomingLoadEdge(LoadCGEdge* inEdge)
    {
        loadInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline void addIncomingStoreEdge(StoreCGEdge* inEdge)
    {
        storeInEdges.insert(inEdge);
        addIncomingEdge(inEdge);
    }
    inline bool addIncomingDirectEdge(ConstraintEdge* inEdge)
    {
        assert(inEdge->getDstID() == this->getId());
        bool added1 = directInEdges.insert(inEdge).second;
        bool added2 = addIncomingEdge(inEdge);
        bool both_added = added1 & added2;
        assert(both_added && "edge not added, duplicated adding!!");
        return both_added;
    }
    inline void addOutgoingAddrEdge(AddrCGEdge* outEdge)
    {
        addressOutEdges.insert(outEdge);
        addOutgoingEdge(outEdge);
    }
    inline bool addOutgoingLoadEdge(LoadCGEdge* outEdge)
    {
        bool added1 = loadOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        bool both_added = added1 & added2;
        assert(both_added && "edge not added, duplicated adding!!");
        return both_added;
    }
    inline bool addOutgoingStoreEdge(StoreCGEdge* outEdge)
    {
        bool added1 = storeOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        bool both_added = added1 & added2;
        assert(both_added && "edge not added, duplicated adding!!");
        return both_added;
    }
    inline bool addOutgoingDirectEdge(ConstraintEdge* outEdge)
    {
        assert(outEdge->getSrcID() == this->getId());
        bool added1 = directOutEdges.insert(outEdge).second;
        bool added2 = addOutgoingEdge(outEdge);
        bool both_added = added1 & added2;
        assert(both_added && "edge not added, duplicated adding!!");
        return both_added;
    }
    //@}

    /// Remove constraint graph edges
    //{@
    inline bool removeOutgoingAddrEdge(AddrCGEdge* outEdge)
    {
        u32_t num1 = addressOutEdges.erase(outEdge);
        u32_t num2 = removeOutgoingEdge(outEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeIncomingAddrEdge(AddrCGEdge* inEdge)
    {
        u32_t num1 = addressInEdges.erase(inEdge);
        u32_t num2 = removeIncomingEdge(inEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeOutgoingDirectEdge(ConstraintEdge* outEdge)
    {
        if (SVFUtil::isa<GepCGEdge>(outEdge))
            gepOutEdges.erase(outEdge);
        else
            copyOutEdges.erase(outEdge);
        u32_t num1 = directOutEdges.erase(outEdge);
        u32_t num2 = removeOutgoingEdge(outEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeIncomingDirectEdge(ConstraintEdge* inEdge)
    {
        if (SVFUtil::isa<GepCGEdge>(inEdge))
            gepInEdges.erase(inEdge);
        else
            copyInEdges.erase(inEdge);
        u32_t num1 = directInEdges.erase(inEdge);
        u32_t num2 = removeIncomingEdge(inEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeOutgoingLoadEdge(LoadCGEdge* outEdge)
    {
        u32_t num1 = loadOutEdges.erase(outEdge);
        u32_t num2 = removeOutgoingEdge(outEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeIncomingLoadEdge(LoadCGEdge* inEdge)
    {
        u32_t num1 = loadInEdges.erase(inEdge);
        u32_t num2 = removeIncomingEdge(inEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeOutgoingStoreEdge(StoreCGEdge* outEdge)
    {
        u32_t num1 = storeOutEdges.erase(outEdge);
        u32_t num2 = removeOutgoingEdge(outEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }

    inline bool removeIncomingStoreEdge(StoreCGEdge* inEdge)
    {
        u32_t num1 = storeInEdges.erase(inEdge);
        u32_t num2 = removeIncomingEdge(inEdge);
        bool removed = (num1 > 0) & (num2 > 0);
        assert(removed && "edge not in the set, can not remove!!!");
        return removed;
    }
    //@}


};

} // End namespace SVF

#endif /* CONSGNODE_H_ */
