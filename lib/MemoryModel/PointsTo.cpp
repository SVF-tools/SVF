//===- PointsTo.cpp -- Wrapper of set-like data structures  ------------//

/*
 * PointsTo.cpp
 *
 * Abstracts away data structures to be used as points-to sets (implementation).
 *
 *  Created on: Feb 01, 2021
 *      Author: Mohamad Barbar
 */

#include <new>

#include "Util/Options.h"
#include "MemoryModel/PointsTo.h"
#include "Util/BasicTypes.h"

namespace SVF
{

PointsTo::MappingPtr PointsTo::currentBestNodeMapping = nullptr;
PointsTo::MappingPtr PointsTo::currentBestReverseNodeMapping = nullptr;

PointsTo::PointsTo(void)
    : type(Options::PtType), nodeMapping(currentBestNodeMapping),
      reverseNodeMapping(currentBestReverseNodeMapping)
{
    if (type == SBV) new (&sbv) SparseBitVector();
    else if (type == CBV) new (&cbv) CoreBitVector();
    else if (type == BV) new (&bv) BitVector();
    else assert(false && "PointsTo::PointsTo: unknown type");
}

PointsTo::PointsTo(const PointsTo &pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping)
{
    if (type == SBV) new (&sbv) SparseBitVector(pt.sbv);
    else if (type == CBV) new (&cbv) CoreBitVector(pt.cbv);
    else if (type == BV) new (&bv) BitVector(pt.bv);
    else assert(false && "PointsTo::PointsTo&: unknown type");
}

PointsTo::PointsTo(PointsTo &&pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping)
{
    if (type == SBV) new (&sbv) SparseBitVector(std::move(pt.sbv));
    else if (type == CBV) new (&cbv) CoreBitVector(std::move(pt.cbv));
    else if (type == BV) new (&bv) BitVector(std::move(pt.bv));
    else assert(false && "PointsTo::PointsTo&&: unknown type");
}

PointsTo::~PointsTo(void)
{
    if (type == SBV) sbv.~SparseBitVector();
    else if (type == CBV) cbv.~CoreBitVector();
    else if (type == BV) bv.~BitVector();
    else assert(false && "PointsTo::~PointsTo: unknown type");

    nodeMapping = nullptr;
    reverseNodeMapping = nullptr;
}

PointsTo &PointsTo::operator=(const PointsTo &rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // Placement new because if type has changed, we have
    // not constructed the new type yet.
    if (type == SBV) new (&sbv) SparseBitVector(rhs.sbv);
    else if (type == CBV) new (&cbv) CoreBitVector(rhs.cbv);
    else if (type == BV) new (&bv) BitVector(rhs.bv);
    else assert(false && "PointsTo::PointsTo=&: unknown type");

    return *this;
}

PointsTo &PointsTo::operator=(PointsTo &&rhs)
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // See comment in copy assignment.
    if (type == SBV) new (&sbv) SparseBitVector(std::move(rhs.sbv));
    else if (type == CBV) new (&cbv) CoreBitVector(std::move(rhs.cbv));
    else if (type == BV) new (&bv) BitVector(std::move(rhs.bv));
    else assert(false && "PointsTo::PointsTo=&&: unknown type");

    return *this;
}

bool PointsTo::empty(void) const
{
    if (type == CBV) return cbv.empty();
    else if (type == SBV) return sbv.empty();
    else if (type == BV) return bv.empty();
    else {
        assert(false && "PointsTo::empty: unknown type");
        abort();
    }
}

/// Returns number of elements.
u32_t PointsTo::count(void) const
{
    if (type == CBV) return cbv.count();
    else if (type == SBV) return sbv.count();
    else if (type == BV) return bv.count();
    else {
        assert(false && "PointsTo::count: unknown type");
        abort();
    }
}

void PointsTo::clear(void)
{
    if (type == CBV) cbv.clear();
    else if (type == SBV) sbv.clear();
    else if (type == BV) bv.clear();
    else assert(false && "PointsTo::clear: unknown type");
}

bool PointsTo::test(u32_t n) const
{
    n = getInternalNode(n);
    if (type == CBV) return cbv.test(n);
    else if (type == SBV) return sbv.test(n);
    else if (type == BV) return bv.test(n);
    else {
        assert(false && "PointsTo::test: unknown type");
        abort();
    }
}

bool PointsTo::test_and_set(u32_t n)
{
    n = getInternalNode(n);
    if (type == CBV) return cbv.test_and_set(n);
    else if (type == SBV) return sbv.test_and_set(n);
    else if (type == BV) return bv.test_and_set(n);
    else {
        assert(false && "PointsTo::test_and_set: unknown type");
        abort();
    }
}

void PointsTo::set(u32_t n)
{
    n = getInternalNode(n);
    if (type == CBV) cbv.set(n);
    else if (type == SBV) sbv.set(n);
    else if (type == BV) bv.set(n);
    else assert(false && "PointsTo::set: unknown type");
}

void PointsTo::reset(u32_t n)
{
    n = getInternalNode(n);
    if (type == CBV) cbv.reset(n);
    else if (type == SBV) sbv.reset(n);
    else if (type == BV) bv.reset(n);
    else assert(false && "PointsTo::reset: unknown type");
}

bool PointsTo::contains(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::contains: mappings of operands do not match!");

    if (type == CBV) return cbv.contains(rhs.cbv);
    else if (type == SBV) return sbv.contains(rhs.sbv);
    else if (type == BV) return bv.contains(rhs.bv);
    else {
        assert(false && "PointsTo::contains: unknown type");
        abort();
    }
}

bool PointsTo::intersects(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::intersects: mappings of operands do not match!");

    if (type == CBV) return cbv.intersects(rhs.cbv);
    else if (type == SBV) return sbv.intersects(rhs.sbv);
    else if (type == BV) return bv.intersects(rhs.bv);
    else {
        assert(false && "PointsTo::intersects: unknown type");
        abort();
    }
}

int PointsTo::find_first(void)
{
    if (count() == 0) return -1;
    return *begin();
}

bool PointsTo::operator==(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::==: mappings of operands do not match!");

    if (type == CBV) return cbv == rhs.cbv;
    else if (type == SBV) return sbv == rhs.sbv;
    else if (type == BV) return bv == rhs.bv;
    else {
        assert(false && "PointsTo::==: unknown type");
        abort();
    }
}

bool PointsTo::operator!=(const PointsTo &rhs) const
{
    // TODO: we're asserting and checking twice... should be okay...
    assert(metaSame(rhs) && "PointsTo::!=: mappings of operands do not match!");

    return !(*this == rhs);
}

bool PointsTo::operator|=(const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::|=: mappings of operands do not match!");

    if (type == CBV) return cbv |= rhs.cbv;
    else if (type == SBV) return sbv |= rhs.sbv;
    else if (type == BV) return bv |= rhs.bv;
    else{
        assert(false && "PointsTo::|=: unknown type");
        abort();
    }
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
    assert(metaSame(rhs) && "PointsTo::&=: mappings of operands do not match!");

    if (type == CBV) return cbv &= rhs.cbv;
    else if (type == SBV) return sbv &= rhs.sbv;
    else if (type == BV) return bv &= rhs.bv;
    else{
        assert(false && "PointsTo::&=: unknown type");
        abort();
    }
}

bool PointsTo::operator-=(const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::-=: mappings of operands do not match!");

    if (type == CBV) return cbv.intersectWithComplement(rhs.cbv);
    else if (type == SBV) return sbv.intersectWithComplement(rhs.sbv);
    else if (type == BV) return bv.intersectWithComplement(rhs.bv);
    else{
        assert(false && "PointsTo::-=: unknown type");
        abort();
    }
}

bool PointsTo::intersectWithComplement(const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");

    if (type == CBV) return cbv.intersectWithComplement(rhs.cbv);
    else if (type == SBV) return sbv.intersectWithComplement(rhs.sbv);
    else if (type == BV) return bv.intersectWithComplement(rhs.bv);

    assert(false && "PointsTo::intersectWithComplement(PT): unknown type");
    abort();
}

void PointsTo::intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");
    assert(metaSame(lhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");

    if (type == CBV) cbv.intersectWithComplement(lhs.cbv, rhs.cbv);
    else if (type == SBV) sbv.intersectWithComplement(lhs.sbv, rhs.sbv);
    else if (type == BV) bv.intersectWithComplement(lhs.bv, rhs.bv);
    else {
        assert(false && "PointsTo::intersectWithComplement(PT, PT): unknown type");
        abort();
    }
}

NodeBS PointsTo::toNodeBS(void) const
{
    NodeBS nbs;
    for (const NodeID o : *this) nbs.set(o);
    return nbs;
}

size_t PointsTo::hash(void) const
{
    if (type == CBV) return cbv.hash();
    else if (type == SBV)
    {
        std::hash<SparseBitVector> h;
        return h(sbv);
    }
    else if (type == BV) return bv.hash();

    else{
        assert(false && "PointsTo::hash: unknown type");
        abort();
    }
}

PointsTo::MappingPtr PointsTo::getNodeMapping(void) const
{
    return nodeMapping;
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
    return nodeMapping == pt.nodeMapping && reverseNodeMapping == pt.reverseNodeMapping;
}

PointsTo::MappingPtr PointsTo::getCurrentBestNodeMapping(void)
{
    return currentBestNodeMapping;
}

PointsTo::MappingPtr PointsTo::getCurrentBestReverseNodeMapping(void)
{
    return currentBestReverseNodeMapping;
}

void PointsTo::setCurrentBestNodeMapping(MappingPtr newCurrentBestNodeMapping,
                                         MappingPtr newCurrentBestReverseNodeMapping)
{
    currentBestNodeMapping = newCurrentBestNodeMapping;
    currentBestReverseNodeMapping = newCurrentBestReverseNodeMapping;
}

void PointsTo::checkAndRemap(void)
{
    if (nodeMapping != currentBestNodeMapping)
    {
        // newPt constructed with correct node mapping.
        PointsTo newPt;
        for (const NodeID o : *this) newPt.set(o);
        *this = std::move(newPt);
    }
}

PointsTo::PointsToIterator::PointsToIterator(const PointsTo *pt, bool end)
    : pt(pt)
{
    if (pt->type == Type::CBV)
    {
        new (&cbvIt) CoreBitVector::iterator(end ? pt->cbv.end() : pt->cbv.begin());
    }
    else if (pt->type == Type::SBV)
    {
        new (&sbvIt) SparseBitVector::iterator(end ? pt->sbv.end() : pt->sbv.begin());
    }
    else if (pt->type == Type::BV)
    {
        new (&bvIt) BitVector::iterator(end ? pt->bv.end() : pt->bv.begin());
    }
    else
    {
        assert(false && "PointsToIterator::PointsToIterator: unknown type");
        abort();
    }
}

PointsTo::PointsToIterator::PointsToIterator(const PointsToIterator &pt)
    : pt(pt.pt)
{
    if (this->pt->type == PointsTo::Type::SBV)
    {
        new (&sbvIt) SparseBitVector::iterator(pt.sbvIt);
    }
    else if (this->pt->type == PointsTo::Type::CBV)
    {
        new (&cbvIt) CoreBitVector::iterator(pt.cbvIt);
    }
    else if (this->pt->type == PointsTo::Type::BV)
    {
        new (&bvIt) BitVector::iterator(pt.bvIt);
    }
    else {
        assert(false && "PointsToIterator::PointsToIterator&: unknown type");
        abort();
    }
}

PointsTo::PointsToIterator::PointsToIterator(PointsToIterator &&pt)
    : pt(pt.pt)
{
    if (this->pt->type == PointsTo::Type::SBV)
    {
        new (&sbvIt) SparseBitVector::iterator(std::move(pt.sbvIt));
    }
    else if (this->pt->type == PointsTo::Type::CBV)
    {
        new (&cbvIt) CoreBitVector::iterator(std::move(pt.cbvIt));
    }
    else if (this->pt->type == PointsTo::Type::BV)
    {
        new (&bvIt) BitVector::iterator(std::move(pt.bvIt));
    }
    else {
        assert(false && "PointsToIterator::PointsToIterator&&: unknown type");
        abort();
    }
}

PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator=(const PointsToIterator &rhs)
{
    this->pt = rhs.pt;

    if (this->pt->type == PointsTo::Type::SBV)
    {
        new (&sbvIt) SparseBitVector::iterator(rhs.sbvIt);
    }
    else if (this->pt->type == PointsTo::Type::CBV)
    {
        new (&cbvIt) CoreBitVector::iterator(rhs.cbvIt);
    }
    else if (this->pt->type == PointsTo::Type::BV)
    {
        new (&bvIt) BitVector::iterator(rhs.bvIt);
    }
    else assert(false && "PointsToIterator::PointsToIterator&: unknown type");

    return *this;
}

PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator=(PointsToIterator &&rhs)
{
    this->pt = rhs.pt;

    if (this->pt->type == PointsTo::Type::SBV)
    {
        new (&sbvIt) SparseBitVector::iterator(std::move(rhs.sbvIt));
    }
    else if (this->pt->type == PointsTo::Type::CBV)
    {
        new (&cbvIt) CoreBitVector::iterator(std::move(rhs.cbvIt));
    }
    else if (this->pt->type == PointsTo::Type::BV)
    {
        new (&bvIt) BitVector::iterator(std::move(rhs.bvIt));
    }
    else assert(false && "PointsToIterator::PointsToIterator&&: unknown type");

    return *this;
}

const PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator++(void)
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");
    if (pt->type == Type::CBV) ++cbvIt;
    else if (pt->type == Type::SBV) ++sbvIt;
    else if (pt->type == Type::BV) ++bvIt;
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

NodeID PointsTo::PointsToIterator::operator*(void) const
{
    assert(!atEnd() && "PointsToIterator: dereferencing end!");
    if (pt->type == Type::CBV) return pt->getExternalNode(*cbvIt);
    else if (pt->type == Type::SBV) return pt->getExternalNode(*sbvIt);
    else if (pt->type == Type::BV) return pt->getExternalNode(*bvIt);
    else {
        assert(false && "PointsToIterator::*: unknown type");
        abort();
    }
}

bool PointsTo::PointsToIterator::operator==(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::==: comparing iterators from different PointsTos!");

    // Handles end implicitly.
    if (pt->type == Type::CBV) return cbvIt == rhs.cbvIt;
    else if (pt->type == Type::SBV) return sbvIt == rhs.sbvIt;
    else if (pt->type == Type::BV) return bvIt == rhs.bvIt;
    else {
        assert(false && "PointsToIterator::==: unknown type");
        abort();
    }
}

bool PointsTo::PointsToIterator::operator!=(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::!=: comparing iterators from different PointsTos!");
    return !(*this == rhs);
}

bool PointsTo::PointsToIterator::atEnd(void) const
{
    assert(pt != nullptr && "PointsToIterator::atEnd: iterator iterating over nothing!");
    if (pt->type == Type::CBV) return cbvIt == pt->cbv.end();
    else if (pt->type == Type::SBV) return sbvIt == pt->sbv.end();
    else if (pt->type == Type::BV) return bvIt == pt->bv.end();
    else {
        assert(false && "PointsToIterator::atEnd: unknown type");
        abort();
    }
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
