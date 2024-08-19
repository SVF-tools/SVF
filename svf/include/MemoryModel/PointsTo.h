//===- PointsTo.h -- Wrapper of set-like data structures  ------------//

/*
 * PointsTo.h
 *
 * Abstracts away data structures to be used as points-to sets.
 *
 *  Created on: Feb 01, 2021
 *      Author: Mohamad Barbar
 */

#ifndef POINTSTO_H_
#define POINTSTO_H_

#include <vector>

#include "SVFIR/SVFType.h"
#include "Util/BitVector.h"
#include "Util/CoreBitVector.h"
#include "Util/SparseBitVector.h"

// These `PT_*` macro definitions help to perform operations on the underlying `PointsTo` data structure, 
// without having to care about exactly what kind of data structure it is.
// It is essentially template substitution. 
//
// Available data structures with different types must be registered in these places:
// - The `PT_DO` macro below.
// - The `PointsTo` class.
// - The `PointsTo::PointsToIterator` class.
//
// You may also want to add your types into `NodeIDAllocator` to support clustering.
// Additionally, these data structures must share the same interface of operations.

/// Generate a single case block for a specific `PointsTo` type within a switch statement.
///
/// `PT_ENUM_NAME`: The enum tag for the type. For example, `PointsTo::Type::SBV`.
/// `PtClassName`: The class name of the data structure. For example, `SparseBitVector<>`.
/// `pt_name`: The lowercase name of the corresponding member defined in `PointsTo`. For example, `sbv`.
/// `ptItName`: The iterator name defined in `PointsToIterator`. For example, `sbvIt`.
/// `operation`: A macro to do the real work with names replaced.
#define PT_TYPE_CASE(PT_ENUM_NAME, PtClassName, pt_name, ptItName, operation) \
    case PT_ENUM_NAME: { \
        operation(PT_ENUM_NAME, PtClassName, pt_name, ptItName) \
        break; \
    }

/// Performs a custom operation on the `PointsTo` data structure.
/// It uses `type` to check the real type of the underlying data structure,
/// then user-defined `operation` is invoked with correct names.
///
/// Usage:
/// ```
/// #define MY_DESTRUCT(PT_ENUM_NAME, PtClassName, pt_name, ptItName) (pt_name).~(PtClassName)();
/// PT_DO(type, "PointsTo::PointsTo: unknown type", MY_DESTRUCT);
/// #undef MY_DESTRUCT
/// ```
///
/// `type`: The variable being switched on.
/// `assert_msg`: The assertion message to display if the underlying data structure is not recognized.
/// `operation`: A macro or lambda function that defines the operation to be performed.
///
/// TODO: Automatically insert the current function name into the assertion message
#define PT_DO(type, assert_msg, operation) \
do { \
    switch(type) { \
        /* Underlying Data Structure Registrations: */\
        PT_TYPE_CASE(PointsTo::Type::SBV, SparseBitVector<>, sbv, sbvIt, operation) \
        PT_TYPE_CASE(PointsTo::Type::CBV, CoreBitVector, cbv, cbvIt, operation) \
        PT_TYPE_CASE(PointsTo::Type::BV, BitVector, bv, bvIt, operation) \
        default: \
            assert(false && (assert_msg)); \
    }\
} while(0)

namespace SVF
{

/// Wraps data structures to provide a points-to set.
/// Underlying data structure can be changed globally.
/// Includes support for mapping nodes for better internal representation.
class PointsTo
{
public:
    enum Type
    {
        SBV,
        CBV,
        BV,
    };

    class PointsToIterator;
    typedef PointsToIterator const_iterator;
    typedef const_iterator iterator;

    typedef std::shared_ptr<std::vector<NodeID>> MappingPtr;

public:
    /// Construct empty points-to set.
    PointsTo();
    /// Copy constructor.
    PointsTo(const PointsTo &pt);
    /// Move constructor.
    PointsTo(PointsTo &&pt) noexcept ;

    ~PointsTo();

    /// Copy assignment.
    PointsTo &operator=(const PointsTo &rhs);

    /// Move assignment.
    PointsTo &operator=(PointsTo &&rhs) noexcept ;

    /// Returns true if set is empty.
    bool empty() const;

    /// Returns number of elements.
    u32_t count() const;

    /// Empty the set.
    void clear();

    /// Returns true if n is in this set.
    bool test(u32_t n) const;

    /// Check if n is in set. If it is, returns false.
    /// Otherwise, inserts n and returns true.
    bool test_and_set(u32_t n);

    /// Inserts n in the set.
    void set(u32_t n);

    /// Removes n from the set.
    void reset(u32_t n);

    /// Returns true if this set is a superset of rhs.
    bool contains(const PointsTo &rhs) const;

    /// Returns true if this set and rhs share any elements.
    bool intersects(const PointsTo &rhs) const;

    /// Returns the first element the set. Returns -1 when the set is empty.
    /// TODO: should we diverge from LLVM about the int return?
    int find_first();

    /// Returns true if this set and rhs contain exactly the same elements.
    bool operator==(const PointsTo &rhs) const;

    /// Returns true if either this set or rhs has an element not in the other.
    bool operator!=(const PointsTo &rhs) const;

    /// Put union of this set and rhs into this set.
    /// Returns true if this set changed.
    bool operator|=(const PointsTo &rhs);
    bool operator|=(const NodeBS &rhs);

    /// Put intersection of this set and rhs into this set.
    /// Returns true if this set changed.
    bool operator&=(const PointsTo &rhs);

    /// Remove elements in rhs from this set.
    /// Returns true if this set changed.
    bool operator-=(const PointsTo &rhs);

    /// Put intersection of this set with complement of rhs into this set.
    /// Returns true if this set changed.
    bool intersectWithComplement(const PointsTo &rhs);

    /// Put intersection of lhs with complement of rhs into this set (overwrites).
    void intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs);

    /// Returns this points-to set as a NodeBS.
    NodeBS toNodeBS() const;

    /// Return a hash of this set.
    size_t hash() const;

    /// Checks if this points-to set is using the current best mapping.
    /// If not, remaps.
    void checkAndRemap();

    const_iterator begin() const
    {
        return PointsToIterator(this);
    }
    const_iterator end() const
    {
        return PointsToIterator(this, true);
    }

    MappingPtr getNodeMapping() const;

    static MappingPtr getCurrentBestNodeMapping();
    static MappingPtr getCurrentBestReverseNodeMapping();
    static void setCurrentBestNodeMapping(MappingPtr newCurrentBestNodeMapping,
                                          MappingPtr newCurrentBestReverseNodeMapping);

private:
    /// Returns nodeMapping[n], checking for nullptr and size.
    NodeID getInternalNode(NodeID n) const;

    /// Returns reverseNodeMapping[n], checking for nullptr and size.
    NodeID getExternalNode(NodeID n) const;

    /// Returns true if this points-to set and pt have the same type, nodeMapping,
    /// and reverseNodeMapping
    bool metaSame(const PointsTo &pt) const;

private:
    /// Best node mapping we know of the for the analyses at hand.
    static MappingPtr currentBestNodeMapping;
    /// Likewise, but reversed.
    static MappingPtr currentBestReverseNodeMapping;

    /// Holds backing data structure.
    /// TODO: std::variant when we move to C++17.
    union
    {
        /// Sparse bit vector backing.
        SparseBitVector<> sbv;
        /// Core bit vector backing.
        CoreBitVector cbv;
        /// Bit vector backing.
        BitVector bv;
    };

    /// Type of this points-to set.
    enum Type type;
    /// External nodes -> internal nodes.
    MappingPtr nodeMapping;
    /// Internal nodes -> external nodes.
    MappingPtr reverseNodeMapping;

public:
    class PointsToIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = u32_t;
        using difference_type = std::ptrdiff_t;
        using pointer = u32_t *;
        using reference = u32_t &;

        /// Deleted because we don't want iterators with null pt.
        PointsToIterator() = delete;
        PointsToIterator(const PointsToIterator &pt);
        PointsToIterator(PointsToIterator &&pt) noexcept ;

        /// Returns an iterator to the beginning of pt if end is false, and to
        /// the end of pt if end is true.
        explicit PointsToIterator(const PointsTo *pt, bool end=false);

        PointsToIterator &operator=(const PointsToIterator &rhs);
        PointsToIterator &operator=(PointsToIterator &&rhs) noexcept ;

        /// Pre-increment: ++it.
        const PointsToIterator &operator++();

        /// Post-increment: it++.
        const PointsToIterator operator++(int);

        /// Dereference: *it.
        u32_t operator*() const;

        /// Equality: *this == rhs.
        bool operator==(const PointsToIterator &rhs) const;

        /// Inequality: *this != rhs.
        bool operator!=(const PointsToIterator &rhs) const;

    private:
        bool atEnd() const;

    private:
        /// PointsTo we are iterating over.
        const PointsTo *pt;
        /// Iterator into the backing data structure. Discriminated by pt->type.
        /// TODO: std::variant when we move to C++17.
        union
        {
            SparseBitVector<>::iterator sbvIt;
            CoreBitVector::iterator cbvIt;
            BitVector::iterator bvIt;
        };
    };
};

/// Returns a new lhs | rhs.
PointsTo operator|(const PointsTo &lhs, const PointsTo &rhs);

/// Returns a new lhs & rhs.
PointsTo operator&(const PointsTo &lhs, const PointsTo &rhs);

/// Returns a new lhs - rhs.
PointsTo operator-(const PointsTo &lhs, const PointsTo &rhs);

} // End namespace SVF

template <>
struct std::hash<SVF::PointsTo>
{
    size_t operator()(const SVF::PointsTo &pt) const
    {
        return pt.hash();
    }
};


#endif  // POINTSTO_H_
