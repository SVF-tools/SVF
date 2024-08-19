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
#include <utility>

#include "Util/Options.h"
#include "MemoryModel/PointsTo.h"
#include "SVFIR/SVFValue.h"

namespace SVF
{

PointsTo::MappingPtr PointsTo::currentBestNodeMapping = nullptr;
PointsTo::MappingPtr PointsTo::currentBestReverseNodeMapping = nullptr;

PointsTo::PointsTo()
    : type(Options::PtType()), nodeMapping(currentBestNodeMapping),
      reverseNodeMapping(currentBestReverseNodeMapping)
{
    #define CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&(pt_name)) (PtClassName)();
    PT_DO(type, "PointsTo::PointsTo: unknown type", CONSTRUCT);
    #undef CONSTRUCT
    // if (type == SBV) new (&sbv) SparseBitVector<>();
    // ...
}

PointsTo::PointsTo(const PointsTo &pt)
    : type(pt.type), nodeMapping(pt.nodeMapping),
      reverseNodeMapping(pt.reverseNodeMapping)
{
    #define COPY_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&(pt_name)) (PtClassName)(pt.pt_name);
    PT_DO(type, "PointsTo::PointsTo: unknown type", COPY_CONSTRUCT);
    #undef COPY_CONSTRUCT
    // if (type == SBV) new (&sbv) SparseBitVector<>(pt.sbv);
    // ...
}

PointsTo::PointsTo(PointsTo &&pt)
noexcept     : type(pt.type), nodeMapping(std::move(pt.nodeMapping)),
    reverseNodeMapping(std::move(pt.reverseNodeMapping))
{
    #define MOVE_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&(pt_name)) PtClassName(std::move(pt.pt_name));
    PT_DO(type, "PointsTo::PointsTo: unknown type", MOVE_CONSTRUCT);
    #undef MOVE_CONSTRUCT
    // if (type == SBV) new (&sbv) SparseBitVector<>(std::move(pt.sbv));
    // ...
}

PointsTo::~PointsTo()
{
    #define DESTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        pt_name.~PtClassName();
    PT_DO(type, "PointsTo::~PointsTo: unknown type", DESTRUCT);
    #undef DESTRUCT
    // if (type == SBV) sbv.~SparseBitVector<>();
    // ...

    nodeMapping = nullptr;
    reverseNodeMapping = nullptr;
}

PointsTo &PointsTo::operator=(const PointsTo &rhs)
{
    if (this == &rhs)
        return *this;
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // Placement new because if type has changed, we have
    // not constructed the new type yet.
    #define PLACEMENT_NEW(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&(pt_name)) PtClassName(std::move(rhs.pt_name));
    PT_DO(type, "PointsTo::PointsTo=&: unknown type", PLACEMENT_NEW);
    #undef PLACEMENT_NEW
    // if (type == SBV) new (&sbv) SparseBitVector<>(rhs.sbv);
    // ...

    return *this;
}

PointsTo &PointsTo::operator=(PointsTo &&rhs)
noexcept
{
    this->type = rhs.type;
    this->nodeMapping = rhs.nodeMapping;
    this->reverseNodeMapping = rhs.reverseNodeMapping;
    // See comment in copy assignment.
    #define COPY_ASSIGN(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&(pt_name)) PtClassName(std::move(rhs.pt_name));
    PT_DO(type, "PointsTo::PointsTo=&&: unknown type", COPY_ASSIGN);
    #undef COPY_ASSIGN
    // if (type == SBV) new (&sbv) SparseBitVector<>(std::move(rhs.sbv));
    // else ...

    return *this;
}

bool PointsTo::empty() const
{
    #define EMPTY(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).empty();
    PT_DO(type, "PointsTo::empty: unknown type", EMPTY);
    #undef EMPTY
    // if (type == SBV) return sbv.empty();
    // ...
}

/// Returns number of elements.
u32_t PointsTo::count(void) const
{
    #define COUNT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).count();
    PT_DO(type, "PointsTo::count: unknown type", COUNT);
    #undef COUNT
    // if (type == CBV) return cbv.count();
    // ...
}

void PointsTo::clear()
{
    #define CLEAR(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        (pt_name).clear();
    PT_DO(type, "PointsTo::clear: unknown type", CLEAR);
    #undef CLEAR
    // if (type == CBV) cbv.clear();
    // ...
}

bool PointsTo::test(u32_t n) const
{
    n = getInternalNode(n);
    #define TEST(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).test(n);
    PT_DO(type, "PointsTo::clear: unknown type", TEST);
    #undef TEST
    // if (type == CBV) return cbv.test(n);
    // ...
}

bool PointsTo::test_and_set(u32_t n)
{
    n = getInternalNode(n);
    #define TEST_N_SET(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).test_and_set(n);
    PT_DO(type, "PointsTo::clear: unknown type", TEST_N_SET);
    #undef TEST_N_SET
    // if (type == CBV) return cbv.test_and_set(n);
    // ...
}

void PointsTo::set(u32_t n)
{
    n = getInternalNode(n);
    #define SET(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        (pt_name).set(n);
    PT_DO(type, "PointsTo::set: unknown type", SET);
    #undef SET
    // if (type == CBV) cbv.set(n);
    // else ...
}

void PointsTo::reset(u32_t n)
{
    n = getInternalNode(n);
    #define RESET(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        (pt_name).reset(n);
    PT_DO(type, "PointsTo::reset: unknown type", RESET);
    #undef RESET
    // if (type == CBV) cbv.reset(n);
    // ...
}

bool PointsTo::contains(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::contains: mappings of operands do not match!");

    #define CONTAINS(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).contains(rhs.pt_name);
    PT_DO(type, "PointsTo::contains: unknown type", CONTAINS);
    #undef CONTAINS
    // if (type == CBV) return cbv.contains(rhs.cbv);
    // ...
}

bool PointsTo::intersects(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::intersects: mappings of operands do not match!");

    #define INTERSECTS(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).intersects(rhs.pt_name);
    PT_DO(type, "PointsTo::intersects: unknown type", INTERSECTS);
    #undef INTERSECTS
    // if (type == CBV) return cbv.intersects(rhs.cbv);
    // ...
}

int PointsTo::find_first()
{
    if (count() == 0) return -1;
    return *begin();
}

bool PointsTo::operator==(const PointsTo &rhs) const
{
    assert(metaSame(rhs) && "PointsTo::==: mappings of operands do not match!");

    #define EQUALS(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name) == (rhs.pt_name);
    PT_DO(type, "PointsTo::intersects: unknown type", EQUALS);
    #undef EQUALS
    // if (type == CBV) return cbv == rhs.cbv;
    // ...
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

    #define OR_EQUALS(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return pt_name |= rhs.pt_name;
    PT_DO(type, "PointsTo::|=: unknown type", OR_EQUALS);
    #undef OR_EQUALS

    // if (type == CBV) return cbv |= rhs.cbv;
    // ...
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

    #define AND_EQUALS(PT_ENUM, PtClass, pt_name, ptItName) \
        return pt_name &= rhs.pt_name;
    PT_DO(type, "PointsTo::&=: unknown type", AND_EQUALS);
    #undef AND_EQUALS

    // if (type == CBV) return cbv &= rhs.cbv;
    // ...
}

bool PointsTo::operator-=(const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::-=: mappings of operands do not match!");

    #define INTERSECT_COMPLEMENT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).intersectWithComplement(rhs.pt_name);
    PT_DO(type, "PointsTo::-=: unknown type", INTERSECT_COMPLEMENT);
    #undef INTERSECT_COMPLEMENT

    // if (type == CBV) return cbv.intersectWithComplement(rhs.cbv);
    // ...
}

bool PointsTo::intersectWithComplement(const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");

    #define INTERSECT_COMPLEMENT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).intersectWithComplement(rhs.pt_name);
    PT_DO(type, "PointsTo::intersectWithComplement: unknown type", INTERSECT_COMPLEMENT);
    #undef INTERSECT_COMPLEMENT

    // if (type == CBV) return cbv.intersectWithComplement(rhs.cbv);
    // ...
}

void PointsTo::intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs)
{
    assert(metaSame(rhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");
    assert(metaSame(lhs) && "PointsTo::intersectWithComplement: mappings of operands do not match!");

    #define INTERSECT_COMPLEMENT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return (pt_name).intersectWithComplement(lhs.pt_name, rhs.pt_name);
    PT_DO(type, "PointsTo::intersects: unknown type", INTERSECT_COMPLEMENT);
    #undef INTERSECT_COMPLEMENT
    // if (type == CBV) cbv.intersectWithComplement(lhs.cbv, rhs.cbv);
    // ...
}

NodeBS PointsTo::toNodeBS() const
{
    NodeBS nbs;
    for (const NodeID o : *this) nbs.set(o);
    return nbs;
}

size_t PointsTo::hash() const
{
    // TODO: common hash interfaces
    
    if (type == CBV) return cbv.hash();
    else if (type == SBV)
    {
        std::hash<SparseBitVector<>> h;
        return h(sbv);
    }
    else if (type == BV) return bv.hash();

    else
    {
        assert(false && "PointsTo::hash: unknown type");
        abort();
    }
}

PointsTo::MappingPtr PointsTo::getNodeMapping() const
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

PointsTo::MappingPtr PointsTo::getCurrentBestNodeMapping()
{
    return currentBestNodeMapping;
}

PointsTo::MappingPtr PointsTo::getCurrentBestReverseNodeMapping()
{
    return currentBestReverseNodeMapping;
}

void PointsTo::setCurrentBestNodeMapping(MappingPtr newCurrentBestNodeMapping,
        MappingPtr newCurrentBestReverseNodeMapping)
{
    currentBestNodeMapping = std::move(newCurrentBestNodeMapping);
    currentBestReverseNodeMapping = std::move(newCurrentBestReverseNodeMapping);
}

void PointsTo::checkAndRemap()
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
    #define IT_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&ptItName) PtClassName::iterator(end ? pt->pt_name.end() : pt->pt_name.begin());
    PT_DO(pt->type, "PointsTo::PointsToIterator: unknown type", IT_CONSTRUCT);
    #undef IT_CONSTRUCT
    // if (pt->type == Type::RBM)
    // {
    //     new (&rbmIt) RoaringBitmap::iterator(end ? pt->rbm.end() : pt->rbm.begin());
    // }
    // ...
}

PointsTo::PointsToIterator::PointsToIterator(const PointsToIterator &pt)
    : pt(pt.pt)
{
    #define IT_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&ptItName) PtClassName::iterator(pt.ptItName);
    PT_DO(this->pt->type, "PointsTo::PointsToIterator&: unknown type", IT_CONSTRUCT);
    #undef IT_CONSTRUCT

    // if (this->pt->type == PointsTo::Type::RBM)
    // {
    //     new (&rbmIt) RoaringBitmap::iterator(pt.rbmIt);
    // }
    // ...
}

PointsTo::PointsToIterator::PointsToIterator(PointsToIterator &&pt)
noexcept     : pt(pt.pt)
{
    #define IT_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&ptItName) PtClassName::iterator(std::move(pt.ptItName));
    PT_DO(this->pt->type, "PointsTo::PointsToIterator&&: unknown type", IT_CONSTRUCT);
    #undef IT_CONSTRUCT

    // if (this->pt->type == PointsTo::Type::RBM)
    // {
    //     new (&rbmIt) RoaringBitmap::iterator(std::move(pt.rbmIt));
    // }
    // ...
}

PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator=(const PointsToIterator &rhs)
{
    this->pt = rhs.pt;

    #define IT_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&ptItName) PtClassName::iterator(rhs.ptItName);
    PT_DO(this->pt->type, "PointsTo::PointsToIterator&: unknown type", IT_CONSTRUCT);
    #undef IT_CONSTRUCT

    // if (this->pt->type == PointsTo::Type::RBM)
    // {
    //     new (&rbmIt) RoaringBitmap::iterator(rhs.rbmIt);
    // }
    // ...

    return *this;
}

PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator=(PointsToIterator &&rhs) noexcept
{
    this->pt = rhs.pt;

    #define IT_CONSTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        new (&ptItName) PtClassName::iterator(std::move(rhs.ptItName));
    PT_DO(this->pt->type, "PointsTo::PointsToIterator&: unknown type", IT_CONSTRUCT);
    #undef IT_CONSTRUCT

    // if (this->pt->type == PointsTo::Type::RBM)
    // {
    //     new (&rbmIt) RoaringBitmap::iterator(std::move(rhs.rbmIt));
    // }
    // ...

    return *this;
}

const PointsTo::PointsToIterator &PointsTo::PointsToIterator::operator++()
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");

    #define INCREMENT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        ++ptItName;
    PT_DO(pt->type, "PointsTo::++(void): unknown type", INCREMENT);
    #undef INCREMENT

    // if (pt->type == Type::RBM) ++rbmIt;
    // ...

    return *this;
}

const PointsTo::PointsToIterator PointsTo::PointsToIterator::operator++(int)
{
    assert(!atEnd() && "PointsToIterator::++(pre): incrementing past end!");
    PointsToIterator old = *this;
    ++*this;
    return old;
}

NodeID PointsTo::PointsToIterator::operator*() const
{
    assert(!atEnd() && "PointsToIterator: dereferencing end!");

    #define DEREF(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return pt->getExternalNode(*ptItName);
    PT_DO(pt->type, "PointsTo::*: unknown type", DEREF);
    #undef DEREF

    // if (pt->type == Type::RBM) return pt->getExternalNode(*rbmIt);
    // ...
}

bool PointsTo::PointsToIterator::operator==(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::==: comparing iterators from different PointsTos!");

    // Handles end implicitly.
    #define IT_EQUAL(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return ptItName == rhs.ptItName;
    PT_DO(pt->type, "PointsTo::==: unknown type", IT_EQUAL);
    #undef IT_EQUAL

    // if (pt->type == Type::RBM) return rbmIt == rhs.rbmIt;
    // ...
}

bool PointsTo::PointsToIterator::operator!=(const PointsToIterator &rhs) const
{
    assert(pt == rhs.pt
           && "PointsToIterator::!=: comparing iterators from different PointsTos!");
    return !(*this == rhs);
}

bool PointsTo::PointsToIterator::atEnd() const
{
    assert(pt != nullptr && "PointsToIterator::atEnd: iterator iterating over nothing!");
    
    #define IT_EQUAL(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        return ptItName == pt->pt_name.end();
    PT_DO(pt->type, "PointsTo::atEnd: unknown type", IT_EQUAL);
    #undef IT_EQUAL

    // if (pt->type == Type::RBM) return rbmIt == pt->rbm.end();
    // ...
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
