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

#include "Util/SVFBasicTypes.h"
#include "Util/BitVector.h"
#include "Util/CoreBitVector.h"

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
    PointsTo(void);
    /// Copy costructor.
    PointsTo(const PointsTo &pt);
    /// Move constructor.
    PointsTo(PointsTo &&pt);

    ~PointsTo(void);

    /// Copy assignment.
    PointsTo &operator=(const PointsTo &rhs);

    /// Move assignment.
    PointsTo &operator=(PointsTo &&rhs);

    /// Returns true if set is empty.
    bool empty(void) const;

    /// Returns number of elements.
    u32_t count(void) const;

    /// Empty the set.
    void clear(void);

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
    int find_first(void);

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

    /// Put intersection of lhs with complemenet of rhs into this set (overwrites).
    void intersectWithComplement(const PointsTo &lhs, const PointsTo &rhs);

    /// Returns this points-to set as a NodeBS.
    NodeBS toNodeBS(void) const;

    /// Return a hash of this set.
    size_t hash(void) const;

    /// Checks if this points-to set is using the current best mapping.
    /// If not, remaps.
    void checkAndRemap(void);

    const_iterator begin(void) const { return PointsToIterator(this); }
    const_iterator end(void) const { return PointsToIterator(this, true); }

    MappingPtr getNodeMapping(void) const;

    static MappingPtr getCurrentBestNodeMapping(void);
    static MappingPtr getCurrentBestReverseNodeMapping(void);
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
        SparseBitVector sbv;
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
        PointsToIterator(void) = delete;
        PointsToIterator(const PointsToIterator &pt);
        PointsToIterator(PointsToIterator &&pt);

        /// Returns an iterator to the beginning of pt if end is false, and to
        /// the end of pt if end is true.
        PointsToIterator(const PointsTo *pt, bool end=false);

        PointsToIterator &operator=(const PointsToIterator &rhs);
        PointsToIterator &operator=(PointsToIterator &&rhs);

        /// Pre-increment: ++it.
        const PointsToIterator &operator++(void);

        /// Post-increment: it++.
        const PointsToIterator operator++(int);

        /// Dereference: *it.
        const u32_t operator*(void) const;

        /// Equality: *this == rhs.
        bool operator==(const PointsToIterator &rhs) const;

        /// Inequality: *this != rhs.
        bool operator!=(const PointsToIterator &rhs) const;

    private:
        bool atEnd(void) const;

    private:
        /// PointsTo we are iterating over.
        const PointsTo *pt;
        /// Iterator into the backing data structure. Discriminated by pt->type.
        /// TODO: std::variant when we move to C++17.
        union
        {
            SparseBitVector::iterator sbvIt;
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

};

template <>
struct std::hash<SVF::PointsTo>
{
    size_t operator()(const SVF::PointsTo &pt) const
    {
        return pt.hash();
    }
};


#endif  // POINTSTO_H_
