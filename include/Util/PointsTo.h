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
    };

    class PointsToIterator;
    typedef PointsToIterator const_iterator;
    typedef const_iterator iterator;

public:
    /// Construct empty points-to set.
    PointsTo(void);
    /// Copy costructor.
    PointsTo(const PointsTo &pt);
    /// Move constructor.
    PointsTo(const PointsTo &&pt);

    /// Copy assignment.
    PointsTo &operator=(const PointsTo &rhs);

    /// Move assignment.
    PointsTo &operator=(const PointsTo &&rhs);

    /// Returns true if set is empty.
    bool empty(void) const;

    /// Returns number of elements.
    unsigned count(void) const;

    /// Empty the set.
    void clear(void);

    /// Returns true if n is in this set.
    bool test(unsigned n) const;

    /// Check if n is in set. If it is, returns false.
    /// Otherwise, inserts n and returns true.
    bool test_and_set(unsigned n);

    /// Inserts n in the set.
    void set(unsigned n);

    /// Removes n from the set.
    void reset(unsigned n);

    /// Returns true if this set is a superset of rhs.
    bool contains(const PointsTo &rhs) const;

    /// Returns true if this set and rhs share any elements.
    bool intersects(const PointsTo &rhs) const;

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

    /// Return a hash of this set.
    size_t hash(void) const;

    const_iterator begin(void) const { return PointsToIterator(this); }
    const_iterator end(void) const { return PointsToIterator(this, true); }

public:
    /// Set mapping to be given to newly constructed points-to sets.
    /// nullptr == no mapping.
    static void setConstructMapping(std::shared_ptr<std::vector<NodeID>> nodeMapping);

    /// Set the type of points-to set that should be constructed
    /// when a constructor is called.
    static void setConstructType(enum Type type);

private:
    /// Returns nodeMapping[n], checking for nullptr and size.
    NodeID getInternalNode(NodeID n) const;

    /// Returns reverseNodeMapping[n], checking for nullptr and size.
    NodeID getExternalNode(NodeID n) const;

    /// Returns true if this points-to set and pt have the same type, nodeMapping,
    /// and reverseNodeMapping
    bool metaSame(const PointsTo &pt) const;

private:
    // TODO: will change to a union or polymorphic type eventually.
    /// Sparse bit vector backing.
    SparseBitVector sbv;

    /// Type of this points-to set.
    enum Type type;
    /// External nodes -> internal nodes.
    std::shared_ptr<std::vector<NodeID>> nodeMapping;
    /// Internal nodes -> external nodes.
    std::shared_ptr<std::vector<NodeID>> reverseNodeMapping;

    /// Node mapping for newly constructed points-to sets.
    static std::shared_ptr<std::vector<NodeID>> constructNodeMapping;
    /// Reverse of constructNodeMapping; for newly constructed points-to sets.
    static std::shared_ptr<std::vector<NodeID>> constructReverseNodeMapping;
    /// Type of points-to set to construct.
    static enum Type constructType;

public:
    class PointsToIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = unsigned;
        using difference_type = std::ptrdiff_t;
        using pointer = unsigned *;
        using reference = unsigned &;

        PointsToIterator(void) = default;

        /// Returns an iterator to the beginning of pt if end is false, and to
        /// the end of pt if end is true.
        PointsToIterator(const PointsTo *pt, bool end=false);

        /// Pre-increment: ++it.
        const PointsToIterator &operator++(void);

        /// Post-increment: it++.
        const PointsToIterator operator++(int);

        /// Dereference: *it.
        const unsigned operator*(void) const;

        /// Equality: *this == rhs.
        bool operator==(const PointsToIterator &rhs) const;

        /// Inequality: *this != rhs.
        bool operator!=(const PointsToIterator &rhs) const;

    private:
        bool atEnd(void) const;

    private:
        /// PointsTo we are iterating over.
        const PointsTo *pt;
        /// Iterator into the PointsTo.
        /// TODO:
        SparseBitVector::iterator sbvIt;
    };
};

/// Returns lhs | rhs.
PointsTo operator|(const PointsTo &lhs, const PointsTo &rhs);

/// Returns lhs & rhs.
PointsTo operator&(const PointsTo &lhs, const PointsTo &rhs);

/// Returns lhs - rhs.
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
