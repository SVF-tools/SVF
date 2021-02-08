//===- DynamicBitVector.h -- Dynamically sized bit vector data structure ------------//

/*
 * DynamicBitVector.h
 *
 * Contiguous bit vector which resizes as required by common operations.
 *
 *  Created on: Jan 31, 2021
 *      Author: Mohamad Barbar
 */

#ifndef DYNAMICBITVECTOR_H_
#define DYNAMICBITVECTOR_H_

#include <vector>

namespace SVF
{

/// A contiguous bit vector that only contains what it needs according
/// to the operations carried. For example, when two bit vectors are unioned,
/// their sizes may be increased to fit all the bits from the other set.
/// Abbreviated DBV.
class DynamicBitVector
{
public:
    typedef unsigned long long Word;
    static const size_t WordSize;

    class DynamicBitVectorIterator;
    typedef DynamicBitVectorIterator const_iterator;
    typedef const_iterator iterator;

public:
    /// Construct empty DBV.
    DynamicBitVector(void);

    /// Construct empty DBV with space reserved for n Words.
    DynamicBitVector(size_t n);

    /// Copy constructor.
    DynamicBitVector(const DynamicBitVector &dbv);

    /// Move constructor.
    DynamicBitVector(const DynamicBitVector &&dbv);

    /// Copy assignment.
    DynamicBitVector &operator=(const DynamicBitVector &rhs);

    /// Move assignment.
    DynamicBitVector &operator=(const DynamicBitVector &&rhs);

    /// Returns true if no bits are set.
    bool empty(void) const;

    /// Returns number of bits set.
    unsigned count(void) const;

    /// Empty the DBV.
    void clear(void);

    /// Returns true if bit is set in this DBV.
    bool test(unsigned bit) const;

    /// Check if bit is set. If it is, returns false.
    /// Otherwise, sets bit and returns true.
    bool test_and_set(unsigned bit);

    /// Sets bit in the DBV.
    void set(unsigned bit);

    /// Resets bit in the DBV.
    void reset(unsigned bit);

    /// Returns true if this DBV is a superset of rhs.
    bool contains(const DynamicBitVector &rhs) const;

    /// Returns true if this DBV and rhs share any set bits.
    bool intersects(const DynamicBitVector &rhs) const;

    /// Returns true if this DBV and rhs have the same bits set.
    bool operator==(const DynamicBitVector &rhs) const;

    /// Returns true if either this DBV or rhs has a bit set unique to the other.
    bool operator!=(const DynamicBitVector &rhs) const;

    /// Put union of this DBV and rhs into this DBV.
    /// Returns true if DBV changed.
    bool operator|=(const DynamicBitVector &rhs);

    /// Put intersection of this DBV and rhs into this DBV.
    /// Returns true if DBV changed.
    bool operator&=(const DynamicBitVector &rhs);

    /// Remove set bits in rhs from this DBV.
    /// Returns true if DBV changed.
    bool operator-=(const DynamicBitVector &rhs);

    /// Put intersection of this DBV with complemenet of rhs into this DBV.
    /// Returns true if this DBV changed.
    bool intersectWithComplement(const DynamicBitVector &rhs);

    /// Put intersection of lhs with complemenet of rhs into this DBV.
    void intersectWithComplement(const DynamicBitVector &lhs, const DynamicBitVector &rhs);

    /// Hash for this DBV.
    size_t hash(void) const;

    const_iterator begin(void) const { return DynamicBitVectorIterator(this); }
    const_iterator end(void) const { return DynamicBitVectorIterator(this, true); }

private:
    /// Add enough words (prepend) to be able to include bit.
    void extendBackward(unsigned bit);

    /// Add enough words (append) to be able to include bit.
    void extendForward(unsigned bit);

    /// Add enough words (append xor prepend) to be able to include bit.
    void extendTo(unsigned bit);

    /// Returns the index into words which would hold bit.
    size_t indexForBit(unsigned bit) const;

    /// Returns true if bit can fit in this DBV without resizing.
    bool canHold(unsigned bit) const;

    /// Returns the last bit that this DBV can hold.
    unsigned finalBit(void) const;

    /// Returns the first bit position that both this DBV and rhs *can* hold.
    unsigned firstCommonBit(const DynamicBitVector &rhs) const;

public:
    class DynamicBitVectorIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = unsigned;
        using difference_type = std::ptrdiff_t;
        using pointer = unsigned *;
        using reference = unsigned &;

        DynamicBitVectorIterator(void) = default;

        /// Returns an iterator to the beginning of dbv if end is false, and to
        /// the end of dbv if end is true.
        DynamicBitVectorIterator(const DynamicBitVector *dbv, bool end=false)
            : dbv(dbv), bit(0)
        {
            wordIt = end ? dbv->words.end() : dbv->words.begin();
            // If user didn't request an end iterator, or words is non-empty,
            // from 0, go to the next bit. But if the 0 bit is set, we don't
            // need to because that is the first element.
            if (wordIt != dbv->words.end() && !(dbv->words[0] & (Word)0b1)) ++(*this);
        }

        /// Pre-increment: ++it.
        const DynamicBitVectorIterator &operator++(void)
        {
            assert(!atEnd() && "DynamicBitVectorIterator::++(pre): incrementing past end!");

            ++bit;
            // Check if no more bits in wordIt. Find word with a bit set.
            if (bit == WordSize || (*wordIt >> bit) == 0)
            {
                bit = 0;
                ++wordIt;
                while (wordIt != dbv->words.end() && *wordIt == 0) ++wordIt;
            }

            // Find set bit if we're not at the end.
            if (wordIt != dbv->words.end())
            {
                while (bit != WordSize && (*wordIt & ((Word)0b1 << bit)) == 0) ++bit;
            }

            return *this;
        }

        /// Post-increment: it++.
        const DynamicBitVectorIterator operator++(int)
        {
            assert(!atEnd() && "DynamicBitVectorIterator::++(pre): incrementing past end!");
            DynamicBitVectorIterator old = *this;
            ++*this;
            return old;
        }

        /// Dereference: *it.
        const unsigned operator*(void) const
        {
            assert(!atEnd() && "DynamicBitVectorIterator::*: dereferencing end!");
            size_t wordsIndex = wordIt - dbv->words.begin();
            // Add where the bit vector starts (offset), with the number of bits
            // in the passed words (the index encodes how many we have completely
            // passed since it is position - 1) and the bit we are up to for the
            // current word (i.e., in the n+1th)
            return dbv->offset + wordsIndex * WordSize + bit;
        }

        /// Equality: *this == rhs.
        bool operator==(const DynamicBitVectorIterator &rhs) const
        {
            assert(dbv == rhs.dbv && "DynamicBitVectorIterator::==: comparing iterators from different DBVs");
            // When we're at the end we don't care about bit.
            if (wordIt == dbv->words.end()) return rhs.wordIt == dbv->words.end();
            return wordIt == rhs.wordIt && bit == rhs.bit;
        }

        /// Inequality: *this != rhs.
        bool operator!=(const DynamicBitVectorIterator &rhs) const
        {
            assert(dbv == rhs.dbv && "DynamicBitVectorIterator::!=: comparing iterators from different DBVs");
            return !(*this == rhs);
        }

    private:
        bool atEnd(void) const
        {
            return wordIt == dbv->words.end();
        }

    private:
        /// DynamicBitVector we are iterating over.
        const DynamicBitVector *dbv;
        /// Word in words we are looking at.
        std::vector<Word>::const_iterator wordIt;
        /// Current bit in wordIt we are looking at
        /// (index into *wordIt).
        unsigned bit;
    };

private:
    /// The first bit of the first word.
    unsigned offset;
    /// Our actual bit vector.
    std::vector<Word> words;
};

};

#endif  // DYNAMICBITVECTOR_H_
