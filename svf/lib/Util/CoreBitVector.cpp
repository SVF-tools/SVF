//===- CoreBitVector.cpp -- Dynamically sized bit vector data structure ------------//

/*
 * CoreBitVector.h
 *
 * Contiguous bit vector which resizes as required by common operations (implementation).
 *
 *  Created on: Jan 31, 2021
 *      Author: Mohamad Barbar
 */

#include <limits.h>

#include "Util/SparseBitVector.h"  // For LLVM's countPopulation.
#include "Util/CoreBitVector.h"
#include "SVFIR/SVFType.h"
#include "Util/SVFUtil.h"

namespace SVF
{

const size_t CoreBitVector::WordSize = sizeof(Word) * CHAR_BIT;

CoreBitVector::CoreBitVector(void)
    : CoreBitVector(0) { }

CoreBitVector::CoreBitVector(size_t n)
    : offset(0), words(n, 0) { }

CoreBitVector::CoreBitVector(const CoreBitVector &cbv)
    : offset(cbv.offset), words(cbv.words) { }

CoreBitVector::CoreBitVector(CoreBitVector &&cbv)
    : offset(cbv.offset), words(std::move(cbv.words)) { }

CoreBitVector &CoreBitVector::operator=(const CoreBitVector &rhs)
{
    this->offset = rhs.offset;
    this->words = rhs.words;
    return *this;
}

CoreBitVector &CoreBitVector::operator=(CoreBitVector &&rhs)
{
    this->offset = rhs.offset;
    this->words = std::move(rhs.words);
    return *this;
}

bool CoreBitVector::empty(void) const
{
    for (const Word& w : words)
        if (w)
            return false;
    return true;
}

u32_t CoreBitVector::count(void) const
{
    u32_t n = 0;
    for (const Word &w : words) n += countPopulation(w);
    return n;
}

void CoreBitVector::clear(void)
{
    offset = 0;
    words.clear();
    words.shrink_to_fit();
}

bool CoreBitVector::test(u32_t bit) const
{
    if (bit < offset || bit >= offset + words.size() * WordSize) return false;
    const Word &containingWord = words[(bit - offset) / WordSize];
    const Word mask = (Word)0b1 << (bit % WordSize);
    return mask & containingWord;
}

bool CoreBitVector::test_and_set(u32_t bit)
{
    // TODO: can be faster.
    if (test(bit)) return false;
    set(bit);
    return true;
}

void CoreBitVector::set(u32_t bit)
{
    extendTo(bit);

    Word &containingWord = words[(bit - offset) / WordSize];
    Word mask = (Word)0b1 << (bit % WordSize);
    containingWord |= mask;
}

void CoreBitVector::reset(u32_t bit)
{
    if (bit < offset || bit >= offset + words.size() * WordSize) return;
    Word &containingWord = words[(bit - offset) / WordSize];
    Word mask = ~((Word)0b1 << (bit % WordSize));
    containingWord &= mask;
}

bool CoreBitVector::contains(const CoreBitVector &rhs) const
{
    CoreBitVector tmp(*this);
    tmp &= rhs;
    return tmp == rhs;
}

bool CoreBitVector::intersects(const CoreBitVector &rhs) const
{
    // TODO: want some common iteration method.
    if (empty() && rhs.empty()) return false;

    const CoreBitVector &earlierOffsetCBV = offset <= rhs.offset ? *this : rhs;
    const CoreBitVector &laterOffsetCBV = offset <= rhs.offset ? rhs : *this;

    size_t earlierOffset = (offset < rhs.offset ? offset : rhs.offset) / WordSize;
    size_t laterOffset = (offset > rhs.offset ? offset : rhs.offset) / WordSize;
    laterOffset -= earlierOffset;

    const Word *eWords = &earlierOffsetCBV.words[0];
    const size_t eSize = earlierOffsetCBV.words.size();
    const Word *lWords = &laterOffsetCBV.words[0];
    const size_t lSize = laterOffsetCBV.words.size();

    size_t e = 0;
    for ( ; e != laterOffset && e != eSize; ++e) { }

    size_t l = 0;
    for ( ; e != eSize && l != lSize; ++e, ++l)
    {
        if (eWords[e] & lWords[l]) return true;
    }

    return false;
}

bool CoreBitVector::operator==(const CoreBitVector &rhs) const
{
    if (this == &rhs) return true;

    // TODO: maybe a simple equal offset, equal size path?

    size_t lhsSetIndex = nextSetIndex(0);
    size_t rhsSetIndex = rhs.nextSetIndex(0);
    // Iterate comparing only words with set bits, if there is ever a mismatch,
    // then the bit-vectors aren't equal.
    while (lhsSetIndex < words.size() && rhsSetIndex < rhs.words.size())
    {
        // If the first bit is not the same in the word or words are different,
        // then we have a mismatch.
        if (lhsSetIndex * WordSize + offset != rhsSetIndex * WordSize + rhs.offset
                || words[lhsSetIndex] != rhs.words[rhsSetIndex])
        {
            return false;
        }

        lhsSetIndex = nextSetIndex(lhsSetIndex + 1);
        rhsSetIndex = rhs.nextSetIndex(rhsSetIndex + 1);
    }

    // Make sure both got to the end at the same time.
    return lhsSetIndex >= words.size() && rhsSetIndex >= rhs.words.size();
}

bool CoreBitVector::operator!=(const CoreBitVector &rhs) const
{
    return !(*this == rhs);
}

bool CoreBitVector::operator|=(const CoreBitVector &rhs)
{
    if (words.size() == 0)
    {
        *this = rhs;
        return words.size() != 0;
    }

    if (rhs.words.size() == 0) return false;

    if (this == &rhs) return false;

    // TODO: some redundancy in extendTo calls.
    if (finalBit() < rhs.finalBit()) extendForward(rhs.finalBit());
    if (offset > rhs.offset) extendBackward(rhs.offset);

    // Start counting this where rhs starts.
    const size_t thisIndex = indexForBit(rhs.offset);
    size_t rhsIndex = 0;

    // Only need to test against rhs's size since we extended this to hold rhs.
    Word *thisWords = &words[thisIndex];
    const Word *rhsWords = &rhs.words[rhsIndex];
    const size_t length = rhs.words.size();
    Word changed = 0;

    // Can start counting from 0 because we took the addresses of both
    // word vectors at the correct index.
    // #pragma omp simd
    for (size_t i = 0 ; i < length; ++i)
    {
        const Word oldWord = thisWords[i];
        // Is there anything in rhs not in *this?
        thisWords[i] = thisWords[i] | rhsWords[i];
        changed |= oldWord ^ thisWords[i];
    }

    return changed;
}

bool CoreBitVector::operator&=(const CoreBitVector &rhs)
{
    // The first bit this and rhs have in common is the greater of
    // their offsets if the CBV with the smaller offset can hold
    // the greater offset.
    u32_t greaterOffset = std::max(offset, rhs.offset);

    // If there is no overlap, then clear this CBV.
    if (!canHold(greaterOffset) || !rhs.canHold(greaterOffset))
    {
        bool changed = false;
        for (size_t i = 0; i < words.size(); ++i)
        {
            if (!changed) changed = words[i] != 0;
            words[i] = 0;
        }

        return changed;
    }

    bool changed = false;
    size_t thisIndex = indexForBit(greaterOffset);
    size_t rhsIndex = rhs.indexForBit(greaterOffset);

    // Clear everything before the overlapping part.
    for (size_t i = 0; i < thisIndex; ++i)
    {
        if (!changed) changed = words[i] != 0;
        words[i] = 0;
    }

    Word oldWord;
    for ( ; thisIndex < words.size() && rhsIndex < rhs.words.size(); ++thisIndex, ++rhsIndex)
    {
        if (!changed) oldWord = words[thisIndex];
        words[thisIndex] &= rhs.words[rhsIndex];
        if (!changed) changed = oldWord != words[thisIndex];
    }

    // Clear the remaining bits with no rhs analogue.
    for ( ; thisIndex < words.size(); ++thisIndex)
    {
        if (!changed && words[thisIndex] != 0) changed = true;
        words[thisIndex] = 0;
    }

    return changed;
}

bool CoreBitVector::operator-=(const CoreBitVector &rhs)
{
    // Similar to |= in that we only iterate over rhs within this, but we
    // don't need to extend anything since nothing from rhs is being added.
    u32_t greaterOffset = std::max(offset, rhs.offset);
    // TODO: calling twice.
    // No overlap if either cannot hold the greater offset.
    if (!canHold(greaterOffset) || !rhs.canHold(greaterOffset)) return false;

    bool changed = false;
    size_t thisIndex = indexForBit(greaterOffset);
    size_t rhsIndex = rhs.indexForBit(greaterOffset);
    Word oldWord;
    for ( ; thisIndex < words.size() && rhsIndex < rhs.words.size(); ++thisIndex, ++rhsIndex)
    {
        if (!changed) oldWord = words[thisIndex];
        words[thisIndex] &= ~rhs.words[rhsIndex];
        if (!changed) changed = oldWord != words[thisIndex];
    }

    return changed;
}

bool CoreBitVector::intersectWithComplement(const CoreBitVector &rhs)
{
    return *this -= rhs;
}

void CoreBitVector::intersectWithComplement(const CoreBitVector &lhs, const CoreBitVector &rhs)
{
    // TODO: inefficient!
    *this = lhs;
    intersectWithComplement(rhs);
}

size_t CoreBitVector::hash(void) const
{
    // From https://stackoverflow.com/a/27216842
    size_t h = words.size();
    for (const Word &w : words)
    {
        h ^= w + 0x9e3779b9 + (h << 6) + (h >> 2);
    }

    return h + offset;
}

CoreBitVector::const_iterator CoreBitVector::end(void) const
{
    return CoreBitVectorIterator(this, true);
}

CoreBitVector::const_iterator CoreBitVector::begin(void) const
{
    return CoreBitVectorIterator(this);
}

void CoreBitVector::extendBackward(u32_t bit)
{
    // New offset is the starting bit of the word which contains bit.
    u32_t newOffset = (bit / WordSize) * WordSize;

    // TODO: maybe assertions?
    // Check if bit can already be included in this BV or if it's extendForward's problem.
    if (newOffset >= offset) return;

    words.insert(words.begin(), (offset - newOffset) / WordSize, 0);
    offset = newOffset;
}

void CoreBitVector::extendForward(u32_t bit)
{
    // TODO: maybe assertions?
    // Not our problem; extendBackward's problem, or there is nothing to do.
    if (bit < offset || canHold(bit)) return;

    // Starting bit of word which would contain bit.
    u32_t bitsWord = (bit / WordSize) * WordSize;

    // Add 1 to represent the final word starting at bitsWord.
    u32_t wordsToAdd = 1 + (bitsWord - words.size() * WordSize - offset) / WordSize;
    words.insert(words.end(), wordsToAdd, 0);
}

void CoreBitVector::extendTo(u32_t bit)
{
    if (offset == 0 && words.size() == 0)
    {
        offset = (bit / WordSize) * WordSize;
        words.push_back(0);
    }
    else if (bit < offset) extendBackward(bit);
    else if (bit >= offset + words.size() * WordSize) extendForward(bit);
}

size_t CoreBitVector::indexForBit(u32_t bit) const
{
    assert(canHold(bit));
    // Recall, offset (and the bits in that word) are represented by words[0],
    // so solve (offset + x) / WordSize == 0... x == -offset.
    return (bit - offset) / WordSize;
}

bool CoreBitVector::canHold(u32_t bit) const
{
    return bit >= offset && bit < offset + words.size() * WordSize;
}

u32_t CoreBitVector::finalBit(void) const
{
    return offset + words.size() * WordSize - 1;
}

size_t CoreBitVector::nextSetIndex(const size_t start) const
{
    size_t index = start;
    for ( ; index < words.size(); ++index)
    {
        if (words[index]) break;
    }

    return index;
}

CoreBitVector::CoreBitVectorIterator::CoreBitVectorIterator(const CoreBitVector *cbv, bool end)
    : cbv(cbv), bit(0)
{
    wordIt = end ? cbv->words.end() : cbv->words.begin();
    // If user didn't request an end iterator, or words is non-empty,
    // from 0, go to the next bit. But if the 0 bit is set, we don't
    // need to because that is the first element.
    if (wordIt != cbv->words.end() && !(cbv->words[0] & (Word)0b1)) ++(*this);
}

const CoreBitVector::CoreBitVectorIterator &CoreBitVector::CoreBitVectorIterator::operator++(void)
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

const CoreBitVector::CoreBitVectorIterator CoreBitVector::CoreBitVectorIterator::operator++(int)
{
    assert(!atEnd() && "CoreBitVectorIterator::++(pre): incrementing past end!");
    CoreBitVectorIterator old = *this;
    ++*this;
    return old;
}

u32_t CoreBitVector::CoreBitVectorIterator::operator*(void) const
{
    assert(!atEnd() && "CoreBitVectorIterator::*: dereferencing end!");
    size_t wordsIndex = wordIt - cbv->words.begin();
    // Add where the bit vector starts (offset), with the number of bits
    // in the passed words (the index encodes how many we have completely
    // passed since it is position - 1) and the bit we are up to for the
    // current word (i.e., in the n+1th)
    return cbv->offset + wordsIndex * WordSize + bit;
}

bool CoreBitVector::CoreBitVectorIterator::operator==(const CoreBitVectorIterator &rhs) const
{
    assert(cbv == rhs.cbv && "CoreBitVectorIterator::==: comparing iterators from different CBVs");
    // When we're at the end we don't care about bit.
    if (wordIt == cbv->words.end()) return rhs.wordIt == cbv->words.end();
    return wordIt == rhs.wordIt && bit == rhs.bit;
}

bool CoreBitVector::CoreBitVectorIterator::operator!=(const CoreBitVectorIterator &rhs) const
{
    assert(cbv == rhs.cbv && "CoreBitVectorIterator::!=: comparing iterators from different CBVs");
    return !(*this == rhs);
}

bool CoreBitVector::CoreBitVectorIterator::atEnd(void) const
{
    return wordIt == cbv->words.end();
}

};  // namespace SVF
