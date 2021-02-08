//===- PointsTo.cpp -- Wrapper of set-like data structures  ------------//

/*
 * PointsTo.cpp
 *
 * Abstracts away data structures to be used as points-to sets (implementation).
 *
 *  Created on: Feb 01, 2021
 *      Author: Mohamad Barbar
 */

#include "Util/PointsTo.h"
#include "Util/BasicTypes.h"

namespace SVF
{

PointsTo::PointsTo(Type type, MappingPtr nodeMapping, MappingPtr reverseNodeMapping)
    : type(type), nodeMapping(nodeMapping), reverseNodeMapping(reverseNodeMapping) { }

PointsTo::PointsTo(const PointsTo &pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping), sbv(pt.sbv), dbv(pt.dbv) { }

PointsTo::PointsTo(const PointsTo &&pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping),
      sbv(std::move(pt.sbv)), dbv(std::move(pt.dbv)) { }

PointsTo &PointsTo::operator=(const PointsTo &rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // TODO:
    this->sbv = rhs.sbv;
    this->dbv = rhs.dbv;

    return *this;
}

PointsTo &PointsTo::operator=(const PointsTo &&rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // TODO:
    this->sbv = std::move(rhs.sbv);
    this->dbv = std::move(rhs.dbv);

    return *this;
}

bool PointsTo::empty(void) const
{
    if (type == DBV) return dbv.empty();
    else if (type == SBV) return sbv.empty();
    assert(false && "PointsTo::empty: unknown type");
}

/// Returns number of elements.
unsigned PointsTo::count(void) const
{
    if (type == DBV) return dbv.count();
    else if (type == SBV) return sbv.count();
    assert(false && "PointsTo::count: unknown type");
}

void PointsTo::clear(void)
{
    if (type == DBV) dbv.clear();
    else if (type == SBV) sbv.clear();
    else assert(false && "PointsTo::clear: unknown type");
}

bool PointsTo::test(unsigned n) const
{
    n = getInternalNode(n);
    if (type == DBV) return dbv.test(n);
    else if (type == SBV) return sbv.test(n);
    assert(false && "PointsTo::test: unknown type");
}

bool PointsTo::test_and_set(unsigned n)
{
    n = getInternalNode(n);
    if (type == DBV) return dbv.test_and_set(n);
    else if (type == SBV) return sbv.test_and_set(n);
    assert(false && "PointsTo::test_and_set: unknown type");
}

void PointsTo::set(unsigned n)
{
    n = getInternalNode(n);
    if (type == DBV) dbv.set(n);
    else if (type == SBV) sbv.set(n);
    else assert(false && "PointsTo::set: unknown type");
}

void PointsTo::reset(unsigned n)
{
    n = getInternalNode(n);
    if (type == DBV) dbv.reset(n);
    else if (type == SBV) sbv.reset(n);
    else assert(false && "PointsTo::reset: unknown type");
}

bool PointsTo::contains(const PointsTo &rhs) const
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv.contains(rhs.dbv);
    else if (type == SBV) return sbv.contains(rhs.sbv);
    assert(false && "PointsTo::contains: unknown type");
}

bool PointsTo::intersects(const PointsTo &rhs) const
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv.intersects(rhs.dbv);
    else if (type == SBV) return sbv.intersects(rhs.sbv);
    assert(false && "PointsTo::intersects: unknown type");
}

bool PointsTo::operator==(const PointsTo &rhs) const
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv == rhs.dbv;
    else if (type == SBV) return sbv == rhs.sbv;
    assert(false && "PointsTo::==: unknown type");
}

bool PointsTo::operator!=(const PointsTo &rhs) const
{
    // TODO: we're asserting twice... should be okay...
    assert(metaSame(rhs));
    return !(*this == rhs);
}

bool PointsTo::operator|=(const PointsTo &rhs)
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv |= rhs.dbv;
    else if (type == SBV) return sbv |= rhs.sbv;
    assert(false && "PointsTo::|=: unknown type");
}

bool PointsTo::operator|=(const NodeBS &rhs)
{
    // TODO:
    bool changed = false;
    for (NodeID n : rhs)
    {
        if (changed) set(n);
        else changed = test_and_set(n);
    }

    return changed;
}

bool PointsTo::operator&=(const PointsTo &rhs)
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv &= rhs.dbv;
    else if (type == SBV) return sbv &= rhs.sbv;
    assert(false && "PointsTo::&=: unknown type");
}

bool PointsTo::operator-=(const PointsTo &rhs)
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv.intersectWithComplement(rhs.dbv);
    else if (type == SBV) return sbv.intersectWithComplement(rhs.sbv);
    assert(false && "PointsTo::-=: unknown type");
}

bool PointsTo::intersectWithComplement(const PointsTo &rhs)
{
    assert(metaSame(rhs));
    if (type == DBV) return dbv.intersectWithComplement(rhs.dbv);
    else if (type == SBV) return sbv.intersectWithComplement(rhs.sbv);
    assert(false && "PointsTo::intersectWithComplement(PT): unknown type");
}

void PointsTo::intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs)
{
    assert(metaSame(rhs));
    if (type == DBV) dbv.intersectWithComplement(lhs.dbv, rhs.dbv);
    else if (type == SBV) sbv.intersectWithComplement(lhs.sbv, rhs.sbv);
    else assert(false && "PointsTo::intersectWithComplement(PT, PT): unknown type");
}

size_t PointsTo::hash(void) const
{
    // TODO:
    if (type == DBV) return dbv.hash();
    else if (type == SBV)
    {
        std::hash<SparseBitVector> h;
        return h(sbv);
    }

    assert(false && "PointsTo::hash: unknown type");
}

NodeID PointsTo::getInternalNode(NodeID n) const
{
    if (nodeMapping == nullptr) return n;
    assert(n < nodeMapping->size());
    return nodeMapping->at(n);
}

NodeID PointsTo::getExternalNode(NodeID n) const
{
    if (reverseNodeMapping == nullptr) return n;
    assert(n < reverseNodeMapping->size());
    return reverseNodeMapping->at(n);
}

bool PointsTo::metaSame(const PointsTo &pt) const
{
    return type == pt.type && nodeMapping == pt.nodeMapping && reverseNodeMapping == pt.reverseNodeMapping;
}

PointsTo::PointsToIterator::PointsToIterator(const PointsTo *pt, bool end)
    : pt(pt)
{
    if (pt->type == Type::DBV) dbvIt = end ? pt->dbv.end() : pt->dbv.begin();
    else if (pt->type == Type::SBV) sbvIt = end ? pt->sbv.end() : pt->sbv.begin();
}

const PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator++(void)
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");
    if (pt->type == Type::DBV) ++dbvIt;
    else if (pt->type == Type::SBV) ++sbvIt;
    else assert(false && "PointsToIterator::++(void): unknown type");

    return *this;
}

const PointsTo::PointsToIterator PointsTo::PointsToIterator::operator++(int)
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");
    PointsToIterator old = *this;
    ++*this;
    return old;
}

const NodeID PointsTo::PointsToIterator::operator*(void) const
{
    assert(!atEnd() && "PointsToIterator: dereferencing end!");
    if (pt->type == Type::DBV) return pt->getExternalNode(*dbvIt);
    else if (pt->type == Type::SBV) return pt->getExternalNode(*sbvIt);
    assert(false && "PointsToIterator::*: unknown type");
}

bool PointsTo::PointsToIterator::operator==(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::==: comparing iterators from different PointsTos!");

    // Handles end implicitly.
    if (pt->type == Type::DBV) return dbvIt == rhs.dbvIt;
    else if (pt->type == Type::SBV) return sbvIt == rhs.sbvIt;
    assert(false && "PointsToIterator::==: unknown type");
}

bool PointsTo::PointsToIterator::operator!=(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::!=: comparing iterators from different PointsTos!");
    return !(*this == rhs);
}

bool PointsTo::PointsToIterator::atEnd(void) const
{
    // TODO: null case.
    if (pt == nullptr) return true;
    if (pt->type == Type::DBV) return dbvIt == pt->dbv.end();
    else if (pt->type == Type::SBV) return sbvIt == pt->sbv.end();
    assert(false && "PointsToIterator::atEnd: unknown type");
}

PointsTo operator|(const PointsTo &lhs, const PointsTo &rhs)
{
    // TODO: optimise.
    PointsTo result = lhs;
    result |= rhs;
    return result;
}

PointsTo operator&(const PointsTo &lhs, const PointsTo &rhs)
{
    // TODO: optimise.
    PointsTo result = lhs;
    result &= rhs;
    return result;
}

PointsTo operator-(const PointsTo &lhs, const PointsTo &rhs)
{
    // TODO: optimise.
    PointsTo result = lhs;
    result -= rhs;
    return result;
}

};  // namespace SVF
