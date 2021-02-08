//===- DynamicBitVector.cpp -- Dynamically sized bit vector data structure ------------//

/*
 * DynamicBitVector.h
 *
 * Contiguous bit vector which resizes as required by common operations (implementation).
 *
 *  Created on: Jan 31, 2021
 *      Author: Mohamad Barbar
 */

#include <limits.h>
#include <llvm/Support/MathExtras.h>

#include "Util/DynamicBitVector.h"
#include "Util/SVFUtil.h"

namespace SVF
{

void dump(const DynamicBitVector &dbv) {
    for (auto x : dbv) {
        SVFUtil::outs() << x << ",";
    }
        SVFUtil::outs() << "END\n";
}

const size_t DynamicBitVector::WordSize = sizeof(Word) * CHAR_BIT;

DynamicBitVector::DynamicBitVector(void)
    : DynamicBitVector(0) { }

DynamicBitVector::DynamicBitVector(size_t n)
    : offset(0), words(n, 0) { }

DynamicBitVector::DynamicBitVector(const DynamicBitVector &dbv)
    : offset(dbv.offset), words(dbv.words) { }

DynamicBitVector::DynamicBitVector(const DynamicBitVector &&dbv)
    : offset(dbv.offset), words(std::move(dbv.words)) { }

DynamicBitVector &DynamicBitVector::operator=(const DynamicBitVector &rhs)
{
    this->offset = rhs.offset;
    this->words = rhs.words;
    return *this;
}

DynamicBitVector &DynamicBitVector::operator=(const DynamicBitVector &&rhs)
{
    this->offset = rhs.offset;
    this->words = std::move(rhs.words);
    return *this;
}

bool DynamicBitVector::empty(void) const
{
    for (const Word &w : words) if (w) return false;
    return true;
}

unsigned DynamicBitVector::count(void) const
{
    unsigned n = 0;
    for (const Word &w : words) n += llvm::countPopulation(w);
    return n;
}

void DynamicBitVector::clear(void)
{
    offset = 0;
    words.clear();
    words.shrink_to_fit();
}

bool DynamicBitVector::test(unsigned bit) const
{
    if (bit < offset || bit >= offset + words.size() * WordSize) return false;
    const Word &containingWord = words[(bit - offset) / WordSize];
    const Word mask = (Word)0b1 << (bit % WordSize);
    return mask & containingWord;
}

bool DynamicBitVector::test_and_set(unsigned bit)
{
    // TODO: can be faster.
    if (test(bit)) return false;
    set(bit);
    return true;
}

void DynamicBitVector::set(unsigned bit)
{
    extendTo(bit);

    Word &containingWord = words[(bit - offset) / WordSize];
    Word mask = (Word)0b1 << (bit % WordSize);
    containingWord |= mask;
}

void DynamicBitVector::reset(unsigned bit)
{
    if (bit < offset || bit >= offset + words.size() * WordSize) return;
    Word &containingWord = words[(bit - offset) / WordSize];
    Word mask = ~((Word)0b1 << (bit % WordSize));
    containingWord &= mask;
}

bool DynamicBitVector::contains(const DynamicBitVector &rhs) const
{
    // TODO.
    assert(false && "contains unimplemented");
    exit(1);
    return false;
}

bool DynamicBitVector::intersects(const DynamicBitVector &rhs) const
{
    // TODO.
    assert(false && "intersects unimplemented");
    exit(1);
    return false;
}

bool DynamicBitVector::operator==(const DynamicBitVector &rhs) const
{
    // We specifically don't want to equalise the offset and length, because
    // 1) it's const, and 2) imagine testing equality for { 0, 1 } and { 0, 500 }...
    // TODO: repetition.
    const DynamicBitVector &earlierOffsetDBV = offset < rhs.offset ? *this : rhs;
    const DynamicBitVector &laterOffsetDBV = offset > rhs.offset ? *this : rhs;

    unsigned smallerOffset = (offset < rhs.offset ? offset : rhs.offset) / WordSize;
    unsigned largerOffset = (offset > rhs.offset ? offset : rhs.offset) / WordSize;

    unsigned e = smallerOffset;
    for ( ; e != largerOffset; e += WordSize)
    {
        // If a bit is set where the other DBV doesn't even start,
        // they are obviously not equal.
        if (earlierOffsetDBV.words[e]) return false;
    }

    unsigned l = 0;
    for ( ; e != earlierOffsetDBV.words.size() && l != laterOffsetDBV.words.size(); ++e, ++l)
    {
        if (earlierOffsetDBV.words[e] != laterOffsetDBV.words[l]) return false;
    }

    // In case a disparity in the sizes caused one to terminate earlier than the other,
    // the one which terminated earlier (i.e., has more to iterate over) needs to have
    // no bits set in its extended tail.
    for ( ; e != earlierOffsetDBV.words.size(); ++e) if (earlierOffsetDBV.words[e]) return false;
    for ( ; l != laterOffsetDBV.words.size(); ++l) if (laterOffsetDBV.words[l]) return false;

    // We've come so far. It really has been a long ride. Congratulations; you're equal.
    return true;
}

bool DynamicBitVector::operator!=(const DynamicBitVector &rhs) const
{
    return !(*this == rhs);
}

bool DynamicBitVector::operator|=(const DynamicBitVector &rhs)
{
    if (words.size() == 0)
    {
        *this = rhs;
        return words.size() != 0;
    }

    if (rhs.words.size() == 0) return false;

    if (this == &rhs) return false;

    // TODO: some redundancy in extendTo calls.
    if (finalBit() <= rhs.offset || finalBit() < rhs.finalBit()) extendForward(rhs.finalBit());
    if (offset > rhs.offset) extendBackward(rhs.offset);

    // Start counting this where rhs starts.
    const size_t thisIndex = indexForBit(rhs.offset);
    size_t rhsIndex = 0;

    // Only need to test against rhs's size since we extended this to hold rhs.
    Word *thisWords = &words[thisIndex];
    const Word *rhsWords = &rhs.words[rhsIndex];
    size_t length = rhs.words.size();
    Word changed = 0;

    // Can start counting from 0 because we took the addresses of both
    // word vectors at the correct index.
    #pragma omp simd reduction(&:unchanged)
    for (size_t i = 0 ; i < length; ++i)
    {
        const Word oldWord = thisWords[i];
        // Is there anything in rhs not in *this?
        changed |= (~oldWord) & rhsWords[i];
        thisWords[i] = thisWords[i] | rhsWords[i];
    }

    return changed;
}

bool DynamicBitVector::operator&=(const DynamicBitVector &rhs)
{
    // The first bit this and rhs have in common is the greater of
    // their offsets if the DBV with the smaller offset can hold
    // the greater offset.
    unsigned greaterOffset = std::max(offset, rhs.offset);

    // If there is no overlap, then clear this DBV.
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

bool DynamicBitVector::operator-=(const DynamicBitVector &rhs)
{
    // Similar to |= in that we only iterate over rhs within this, but we
    // don't need to extend anything since nothing from rhs is being added.
    unsigned greaterOffset = std::max(offset, rhs.offset);
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

bool DynamicBitVector::intersectWithComplement(const DynamicBitVector &rhs)
{
    return *this -= rhs;
}

void DynamicBitVector::intersectWithComplement(const DynamicBitVector &lhs, const DynamicBitVector &rhs)
{
    // TODO: inefficient!
    *this = lhs;
    intersectWithComplement(rhs);
}

size_t DynamicBitVector::hash(void) const
{
    if (words.size() == 0) return 0;
    std::hash<std::pair<size_t, std::pair<Word, Word>>> h;
    // TODO: best combination.
    return h(std::make_pair(words.size(), std::make_pair(words[0], words[words.size() - 1])));
}

void DynamicBitVector::extendBackward(unsigned bit)
{
    // New offset is the starting bit of the word which contains bit.
    unsigned newOffset = (bit / WordSize) * WordSize;

    // TODO: maybe assertions?
    // Check if bit can already be included in this BV or if it's extendForward's problem.
    if (newOffset >= offset) return;

    words.insert(words.begin(), (offset - newOffset) / WordSize, 0);
    offset = newOffset;
}

void DynamicBitVector::extendForward(unsigned bit)
{
    // TODO: maybe assertions?
    // Not our problem; extendBackward's problem, or there is nothing to do.
    if (bit < offset || canHold(bit)) return;

    // Starting bit of word which would contain bit.
    unsigned bitsWord = (bit / WordSize) * WordSize;

    // Add 1 to represent the final word starting at bitsWord.
    unsigned wordsToAdd = 1 + (bitsWord - words.size() * WordSize - offset) / WordSize;
    words.insert(words.end(), wordsToAdd, 0);
}

void DynamicBitVector::extendTo(unsigned bit)
{
    if (offset == 0 && words.size() == 0)
    {
        offset = (bit / WordSize) * WordSize;
        words.push_back(0);
    }
    else if (bit < offset) extendBackward(bit);
    else if (bit >= offset + words.size() * WordSize) extendForward(bit);
}

size_t DynamicBitVector::indexForBit(unsigned bit) const
{
    assert(canHold(bit));
    // Recall, offset (and the bits in that word) are represented by words[0],
    // so solve (offset + x) / WordSize == 0... x == -offset.
    return (bit - offset) / WordSize;
}

bool DynamicBitVector::canHold(unsigned bit) const
{
    return bit >= offset && bit < offset + words.size() * WordSize;
}

unsigned DynamicBitVector::finalBit(void) const
{
    return offset + words.size() * WordSize - 1;
}

};  // namespace SVF
