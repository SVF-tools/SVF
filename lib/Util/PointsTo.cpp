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

std::shared_ptr<std::vector<NodeID>> PointsTo::constructNodeMapping = nullptr;
std::shared_ptr<std::vector<NodeID>> PointsTo::constructReverseNodeMapping = nullptr;
enum PointsTo::Type PointsTo::constructType = PointsTo::Type::SBV;

PointsTo::PointsTo(void)
    : type(constructType), nodeMapping(constructNodeMapping),
      reverseNodeMapping(constructReverseNodeMapping) { }

PointsTo::PointsTo(const PointsTo &pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping), sbv(pt.sbv) { }

PointsTo::PointsTo(const PointsTo &&pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping),
      sbv(std::move(pt.sbv)) { }

PointsTo &PointsTo::operator=(const PointsTo &rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // TODO:
    this->sbv = rhs.sbv;

    return *this;
}

PointsTo &PointsTo::operator=(const PointsTo &&rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // TODO:
    this->sbv = std::move(rhs.sbv);

    return *this;
}

bool PointsTo::empty(void) const
{
    return sbv.empty();
}

/// Returns number of elements.
unsigned PointsTo::count(void) const
{
    return sbv.count();
}

void PointsTo::clear(void)
{
    sbv.clear();
}

bool PointsTo::test(unsigned n) const
{
    n = getInternalNode(n);
    return sbv.test(n);
}

bool PointsTo::test_and_set(unsigned n)
{
    n = getInternalNode(n);
    return sbv.test_and_set(n);
}

void PointsTo::set(unsigned n)
{
    n = getInternalNode(n);
    sbv.set(n);
}

void PointsTo::reset(unsigned n)
{
    n = getInternalNode(n);
    sbv.reset(n);
}

bool PointsTo::contains(const PointsTo &rhs) const
{
    assert(metaSame());
    return sbv.contains(rhs.sbv);
}

bool PointsTo::intersects(const PointsTo &rhs) const
{
    assert(metaSame());
    return sbv.intersects(rhs.sbv);
}

bool PointsTo::operator==(const PointsTo &rhs) const
{
    assert(metaSame());
    return sbv == rhs.sbv;
}

bool PointsTo::operator!=(const PointsTo &rhs) const
{
    // TODO: we're asserting twice... should be okay...
    assert(metaSame());
    return !(*this == rhs);
}

bool PointsTo::operator|=(const PointsTo &rhs)
{
    assert(metaSame());
    return sbv |= rhs.sbv;
}

bool PointsTo::operator|=(const NodeBS &rhs)
{
    // TODO:
    return sbv |= rhs;
}

bool PointsTo::operator&=(const PointsTo &rhs)
{
    assert(metaSame());
    return sbv &= rhs.sbv;
}

bool PointsTo::operator-=(const PointsTo &rhs)
{
    assert(metaSame());
    return sbv.intersectWithComplement(rhs.sbv);
}

bool PointsTo::intersectWithComplement(const PointsTo &rhs)
{
    assert(metaSame());
    return sbv.intersectWithComplement(rhs.sbv);
}

void PointsTo::intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs)
{
    assert(metaSame());
    sbv.intersectWithComplement(lhs.sbv, rhs.sbv);
}

size_t PointsTo::hash(void) const
{
    // TODO:
    std::hash<SparseBitVector> h;
    return h(sbv);
}

NodeID PointsTo::getInternalNode(NodeID n) const
{
    if (nodeMapping == nullptr) return n;
    assert(n < nodeMapping.size());
    return nodeMapping->at(n);
}

NodeID PointsTo::getExternalNode(NodeID n) const
{
    if (reverseNodeMapping == nullptr) return n;
    assert(n < reverseNodeMapping.size());
    return reverseNodeMapping->at(n);
}

bool PointsTo::metaSame(const PointsTo &pt) const
{
    return type == pt.type && nodeMapping == pt.nodeMapping && reverseNodeMapping == pt.reverseNodeMapping;
}

void PointsTo::setConstructMapping(std::shared_ptr<std::vector<NodeID>> nodeMapping)
{
    constructNodeMapping = nodeMapping;
    constructReverseNodeMapping = std::make_shared<std::vector<NodeID>>(constructNodeMapping->size(), 0);

    for (size_t i = 0; i < constructNodeMapping->size(); ++i)
    {
        constructReverseNodeMapping->at(constructNodeMapping->at(i)) = i;
    }
}

void PointsTo::setConstructType(enum Type type)
{
    constructType = type;
}

PointsTo::PointsToIterator::PointsToIterator(const PointsTo *pt, bool end)
    : pt(pt)
{
    sbvIt = end ? pt->sbv.end() : pt->sbv.begin();
}

const PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator++(void)
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");
    ++sbvIt;
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
    return pt->getExternalNode(*sbvIt);
}

bool PointsTo::PointsToIterator::operator==(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::==: comparing iterators from different PointsTos!");

    // Handles end implicitly.
    return sbvIt == rhs.sbvIt;
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
    return pt == nullptr || sbvIt == pt->sbv.end();
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
