//===- CoreBitVector.h -- Dynamically sized bit vector data structure ------------//

/*
 * CoreBitVector.h
 *
 * Contiguous bit vector which resizes as required by common operations.
 *
 *  Created on: Jan 31, 2021
 *      Author: Mohamad Barbar
 */

#ifndef COREBITVECTOR_H_
#define COREBITVECTOR_H_

#include <assert.h>
#include <vector>

#include "Util/SVFBasicTypes.h"

namespace SVF
{

/// A contiguous bit vector that only contains what it needs according
/// to the operations carried. For example, when two bit vectors are unioned,
/// their sizes may be increased to fit all the bits from the other set.
/// This implementation never shrinks. Since points-to sets (generally) grow
/// monotonically, does not have too big an impact on points-to analysis and
/// so it isn't implemented.
/// Abbreviated CBV.
class CoreBitVector
{
public:
    typedef unsigned long long Word;
    static const size_t WordSize;

    class CoreBitVectorIterator;
    typedef CoreBitVectorIterator const_iterator;
    typedef const_iterator iterator;

public:
    /// Construct empty CBV.
    CoreBitVector(void);

    /// Construct empty CBV with space reserved for n Words.
    CoreBitVector(size_t n);

    /// Copy constructor.
    CoreBitVector(const CoreBitVector &cbv);

    /// Move constructor.
    CoreBitVector(CoreBitVector &&cbv);

    /// Copy assignment.
    CoreBitVector &operator=(const CoreBitVector &rhs);

    /// Move assignment.
    CoreBitVector &operator=(CoreBitVector &&rhs);

    /// Returns true if no bits are set.
    bool empty(void) const;

    /// Returns number of bits set.
    u32_t count(void) const;

    /// Empty the CBV.
    void clear(void);

    /// Returns true if bit is set in this CBV.
    bool test(u32_t bit) const;

    /// Check if bit is set. If it is, returns false.
    /// Otherwise, sets bit and returns true.
    bool test_and_set(u32_t bit);

    /// Sets bit in the CBV.
    void set(u32_t bit);

    /// Resets bit in the CBV.
    void reset(u32_t bit);

    /// Returns true if this CBV is a superset of rhs.
    bool contains(const CoreBitVector &rhs) const;

    /// Returns true if this CBV and rhs share any set bits.
    bool intersects(const CoreBitVector &rhs) const;

    /// Returns true if this CBV and rhs have the same bits set.
    bool operator==(const CoreBitVector &rhs) const;

    /// Returns true if either this CBV or rhs has a bit set unique to the other.
    bool operator!=(const CoreBitVector &rhs) const;

    /// Put union of this CBV and rhs into this CBV.
    /// Returns true if CBV changed.
    bool operator|=(const CoreBitVector &rhs);

    /// Put intersection of this CBV and rhs into this CBV.
    /// Returns true if CBV changed.
    bool operator&=(const CoreBitVector &rhs);

    /// Remove set bits in rhs from this CBV.
    /// Returns true if CBV changed.
    bool operator-=(const CoreBitVector &rhs);

    /// Put intersection of this CBV with complemenet of rhs into this CBV.
    /// Returns true if this CBV changed.
    bool intersectWithComplement(const CoreBitVector &rhs);

    /// Put intersection of lhs with complemenet of rhs into this CBV.
    void intersectWithComplement(const CoreBitVector &lhs, const CoreBitVector &rhs);

    /// Hash for this CBV.
    size_t hash(void) const;

    const_iterator begin(void) const { return CoreBitVectorIterator(this); }
    const_iterator end(void) const { return CoreBitVectorIterator(this, true); }

private:
    /// Add enough words (prepend) to be able to include bit.
    void extendBackward(u32_t bit);

    /// Add enough words (append) to be able to include bit.
    void extendForward(u32_t bit);

    /// Add enough words (append xor prepend) to be able to include bit.
    void extendTo(u32_t bit);

    /// Returns the index into words which would hold bit.
    size_t indexForBit(u32_t bit) const;

    /// Returns true if bit can fit in this CBV without resizing.
    bool canHold(u32_t bit) const;

    /// Returns the last bit that this CBV can hold.
    u32_t finalBit(void) const;

    /// Returns the first bit position that both this CBV and rhs *can* hold.
    u32_t firstCommonBit(const CoreBitVector &rhs) const;

    /// Returns the next index in the words array at or after start which contains
    /// set bits. This index and start are indices into the words array not
    /// accounting for the offset. Returns a value greater than or equal to
    /// words.size() when there are no more set bits.
    size_t nextSetIndex(const size_t start) const;

public:
    class CoreBitVectorIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = u32_t;
        using difference_type = std::ptrdiff_t;
        using pointer = u32_t *;
        using reference = u32_t &;

        CoreBitVectorIterator(void) = delete;

        /// Returns an iterator to the beginning of cbv if end is false, and to
        /// the end of cbv if end is true.
        CoreBitVectorIterator(const CoreBitVector *cbv, bool end=false)
            : cbv(cbv), bit(0)
        {
            wordIt = end ? cbv->words.end() : cbv->words.begin();
            // If user didn't request an end iterator, or words is non-empty,
            // from 0, go to the next bit. But if the 0 bit is set, we don't
            // need to because that is the first element.
            if (wordIt != cbv->words.end() && !(cbv->words[0] & (Word)0b1)) ++(*this);
        }

        CoreBitVectorIterator(const CoreBitVectorIterator &cbv) = default;
        CoreBitVectorIterator(CoreBitVectorIterator &&cbv) = default;

        CoreBitVectorIterator &operator=(const CoreBitVectorIterator &cbv) = default;
        CoreBitVectorIterator &operator=(CoreBitVectorIterator &&cbv) = default;

        /// Pre-increment: ++it.
        const CoreBitVectorIterator &operator++(void)
        {
            assert(!atEnd() && "CoreBitVectorIterator::++(pre): incrementing past end!");

            ++bit;
            // Check if no more bits in wordIt. Find word with a bit set.
            if (bit == WordSize || (*wordIt >> bit) == 0)
            {
                bit = 0;
                ++wordIt;
                while (wordIt != cbv->words.end() && *wordIt == 0) ++wordIt;
            }

            // Find set bit if we're not at the end.
            if (wordIt != cbv->words.end())
            {
                while (bit != WordSize && (*wordIt & ((Word)0b1 << bit)) == 0) ++bit;
            }

            return *this;
        }

        /// Post-increment: it++.
        const CoreBitVectorIterator operator++(int)
        {
            assert(!atEnd() && "CoreBitVectorIterator::++(pre): incrementing past end!");
            CoreBitVectorIterator old = *this;
            ++*this;
            return old;
        }

        /// Dereference: *it.
        const u32_t operator*(void) const
        {
            assert(!atEnd() && "CoreBitVectorIterator::*: dereferencing end!");
            size_t wordsIndex = wordIt - cbv->words.begin();
            // Add where the bit vector starts (offset), with the number of bits
            // in the passed words (the index encodes how many we have completely
            // passed since it is position - 1) and the bit we are up to for the
            // current word (i.e., in the n+1th)
            return cbv->offset + wordsIndex * WordSize + bit;
        }

        /// Equality: *this == rhs.
        bool operator==(const CoreBitVectorIterator &rhs) const
        {
            assert(cbv == rhs.cbv && "CoreBitVectorIterator::==: comparing iterators from different CBVs");
            // When we're at the end we don't care about bit.
            if (wordIt == cbv->words.end()) return rhs.wordIt == cbv->words.end();
            return wordIt == rhs.wordIt && bit == rhs.bit;
        }

        /// Inequality: *this != rhs.
        bool operator!=(const CoreBitVectorIterator &rhs) const
        {
            assert(cbv == rhs.cbv && "CoreBitVectorIterator::!=: comparing iterators from different CBVs");
            return !(*this == rhs);
        }

    private:
        bool atEnd(void) const
        {
            return wordIt == cbv->words.end();
        }

    private:
        /// CoreBitVector we are iterating over.
        const CoreBitVector *cbv;
        /// Word in words we are looking at.
        std::vector<Word>::const_iterator wordIt;
        /// Current bit in wordIt we are looking at
        /// (index into *wordIt).
        u32_t bit;
    };

private:
    /// The first bit of the first word.
    u32_t offset;
    /// Our actual bit vector.
    std::vector<Word> words;
};

};

#endif  // COREBITVECTOR_H_
