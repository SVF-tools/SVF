//===- SparseBitVector.h - Efficient Sparse BitVector --*- C++ -*-===//
//
// From the LLVM Project with some modifications, under the Apache License v2.0
// with LLVM Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef SPARSEBITVECTOR_H
#define SPARSEBITVECTOR_H

#include <ostream>
#include <cassert>
#include <cstring>
#include <climits>
#include <limits>
#include <iterator>
#include <list>

// Appease GCC?
#ifdef __has_builtin
#  define HAS_CLZ __has_builtin(__builtin_clz)
#  define HAS_CLZLL __has_builtin(__builtin_clzll)
#else
#  define HAS_CLZ 0
#  define HAS_CLZLL 0
#endif

namespace SVF
{

/// The behavior an operation has on an input of 0.
enum ZeroBehavior
{
    /// The returned value is undefined.
    ZB_Undefined,
    /// The returned value is numeric_limits<T>::max()
    ZB_Max,
    /// The returned value is numeric_limits<T>::digits
    ZB_Width
};

template <typename T, std::size_t SizeOfT> struct TrailingZerosCounter
{
    static unsigned count(T Val, ZeroBehavior)
    {
        if (!Val)
            return std::numeric_limits<T>::digits;
        if (Val & 0x1)
            return 0;

        // Bisection method.
        unsigned ZeroBits = 0;
        T Shift = std::numeric_limits<T>::digits >> 1;
        T Mask = std::numeric_limits<T>::max() >> Shift;
        while (Shift)
        {
            if ((Val & Mask) == 0)
            {
                Val >>= Shift;
                ZeroBits |= Shift;
            }
            Shift >>= 1;
            Mask >>= Shift;
        }
        return ZeroBits;
    }
};

/// Count number of 0's from the least significant bit to the most
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// \param ZB the behavior on an input of 0. Only ZB_Width and ZB_Undefined are
///   valid arguments.
template <typename T>
unsigned countTrailingZeros(T Val, ZeroBehavior ZB = ZB_Width)
{
    static_assert(std::numeric_limits<T>::is_integer &&
                  !std::numeric_limits<T>::is_signed,
                  "Only unsigned integral types are allowed.");
    return TrailingZerosCounter<T, sizeof(T)>::count(Val, ZB);
}

template <typename T, std::size_t SizeOfT> struct PopulationCounter
{
    static unsigned count(T Value)
    {
        // Generic version, forward to 32 bits.
        static_assert(SizeOfT <= 4, "Not implemented!");
#if defined(__GNUC__)
        return __builtin_popcount(Value);
#else
        uint32_t v = Value;
        v = v - ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
#endif
    }
};

template <typename T, std::size_t SizeOfT> struct LeadingZerosCounter
{
    static unsigned count(T Val, ZeroBehavior)
    {
        if (!Val)
            return std::numeric_limits<T>::digits;

        // Bisection method.
        unsigned ZeroBits = 0;
        for (T Shift = std::numeric_limits<T>::digits >> 1; Shift; Shift >>= 1)
        {
            T Tmp = Val >> Shift;
            if (Tmp)
                Val = Tmp;
            else
                ZeroBits |= Shift;
        }
        return ZeroBits;
    }
};

#if defined(__GNUC__) || defined(_MSC_VER)
template <typename T> struct LeadingZerosCounter<T, 4>
{
    static unsigned count(T Val, ZeroBehavior ZB)
    {
        if (ZB != ZB_Undefined && Val == 0)
            return 32;

#if defined(__GNUC__) || HAS_CLZ
        return __builtin_clz(Val);
#elif defined(_MSC_VER)
        unsigned long Index;
        _BitScanReverse(&Index, Val);
        return Index ^ 31;
#endif
    }
};

#if !defined(_MSC_VER) || defined(_M_X64)
template <typename T> struct LeadingZerosCounter<T, 8>
{
    static unsigned count(T Val, ZeroBehavior ZB)
    {
        if (ZB != ZB_Undefined && Val == 0)
            return 64;

#if defined(__GNUC__) || HAS_CLZLL
        return __builtin_clzll(Val);
#elif defined(_MSC_VER)
        unsigned long Index;
        _BitScanReverse64(&Index, Val);
        return Index ^ 63;
#endif
    }
};
#endif
#endif

/// Count number of 0's from the most significant bit to the least
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// \param ZB the behavior on an input of 0. Only ZB_Width and ZB_Undefined are
///   valid arguments.
template <typename T>
unsigned countLeadingZeros(T Val, ZeroBehavior ZB = ZB_Width)
{
    static_assert(std::numeric_limits<T>::is_integer &&
                  !std::numeric_limits<T>::is_signed,
                  "Only unsigned integral types are allowed.");
    return LeadingZerosCounter<T, sizeof(T)>::count(Val, ZB);
}

template <typename T> struct PopulationCounter<T, 8>
{
    static unsigned count(T Value)
    {
#if defined(__GNUC__)
        return __builtin_popcountll(Value);
#else
        uint64_t v = Value;
        v = v - ((v >> 1) & 0x5555555555555555ULL);
        v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
        v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
        return unsigned((uint64_t)(v * 0x0101010101010101ULL) >> 56);
#endif
    }
};

/// Count the number of set bits in a value.
/// Ex. countPopulation(0xF000F000) = 8
/// Returns 0 if the word is zero.
template <typename T>
inline unsigned countPopulation(T Value)
{
    static_assert(std::numeric_limits<T>::is_integer &&
                  !std::numeric_limits<T>::is_signed,
                  "Only unsigned integral types are allowed.");
    return PopulationCounter<T, sizeof(T)>::count(Value);
}

/// SparseBitVector is an implementation of a bitvector that is sparse by only
/// storing the elements that have non-zero bits set.  In order to make this
/// fast for the most common cases, SparseBitVector is implemented as a linked
/// list of SparseBitVectorElements.  We maintain a pointer to the last
/// SparseBitVectorElement accessed (in the form of a list iterator), in order
/// to make multiple in-order test/set constant time after the first one is
/// executed.  Note that using vectors to store SparseBitVectorElement's does
/// not work out very well because it causes insertion in the middle to take
/// enormous amounts of time with a large amount of bits.  Other structures that
/// have better worst cases for insertion in the middle (various balanced trees,
/// etc) do not perform as well in practice as a linked list with this iterator
/// kept up to date.  They are also significantly more memory intensive.
template <unsigned ElementSize = 128> struct SparseBitVectorElement
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    using BitWord = unsigned long;
    using size_type = unsigned;
    enum
    {
        BITWORD_SIZE = sizeof(BitWord) * CHAR_BIT,
        // N.B. (+ BITWORD_SIZE - 1) is to round up, to ensure we can have
        // sufficient bits to represent *at least* ElementSize bits.
        BITWORDS_PER_ELEMENT = (ElementSize + BITWORD_SIZE - 1) / BITWORD_SIZE,
        BITS_PER_ELEMENT = ElementSize
    };

private:
    // Index of Element in terms of where first bit starts.
    unsigned ElementIndex = 0;
    BitWord Bits[BITWORDS_PER_ELEMENT] = {0};

    SparseBitVectorElement() = default;

public:
    explicit SparseBitVectorElement(unsigned Idx) :  ElementIndex(Idx) {}

    // Comparison.
    bool operator==(const SparseBitVectorElement &RHS) const
    {
        if (ElementIndex != RHS.ElementIndex)
            return false;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
            if (Bits[i] != RHS.Bits[i])
                return false;
        return true;
    }

    bool operator!=(const SparseBitVectorElement &RHS) const
    {
        return !(*this == RHS);
    }

    // Return the bits that make up word Idx in our element.
    BitWord word(unsigned Idx) const
    {
        assert(Idx < BITWORDS_PER_ELEMENT);
        return Bits[Idx];
    }

    unsigned index() const
    {
        return ElementIndex;
    }

    bool empty() const
    {
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
            if (Bits[i])
                return false;
        return true;
    }

    void set(unsigned Idx)
    {
        Bits[Idx / BITWORD_SIZE] |= 1L << (Idx % BITWORD_SIZE);
    }

    bool test_and_set(unsigned Idx)
    {
        bool old = test(Idx);
        if (!old)
        {
            set(Idx);
            return true;
        }
        return false;
    }

    void reset(unsigned Idx)
    {
        Bits[Idx / BITWORD_SIZE] &= ~(1L << (Idx % BITWORD_SIZE));
    }

    bool test(unsigned Idx) const
    {
        return Bits[Idx / BITWORD_SIZE] & (1L << (Idx % BITWORD_SIZE));
    }

    size_type count() const
    {
        unsigned NumBits = 0;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
            NumBits += countPopulation(Bits[i]);
        return NumBits;
    }

    /// find_first - Returns the index of the first set bit.
    int find_first() const
    {
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
            if (Bits[i] != 0)
                return i * BITWORD_SIZE + countTrailingZeros(Bits[i]);
        assert(false && "SBV: find_first: SBV cannot be empty");
        abort();
    }

    /// find_last - Returns the index of the last set bit.
    int find_last() const
    {
        for (unsigned I = 0; I < BITWORDS_PER_ELEMENT; ++I)
        {
            unsigned Idx = BITWORDS_PER_ELEMENT - I - 1;
            if (Bits[Idx] != 0)
                return Idx * BITWORD_SIZE + BITWORD_SIZE -
                       countLeadingZeros(Bits[Idx]) - 1;
        }
        assert(false && "SBV: find_last: SBV cannot be empty");
        abort();
    }

    /// find_next - Returns the index of the next set bit starting from the
    /// "Curr" bit. Returns -1 if the next set bit is not found.
    int find_next(unsigned Curr) const
    {
        if (Curr >= BITS_PER_ELEMENT)
            return -1;

        unsigned WordPos = Curr / BITWORD_SIZE;
        unsigned BitPos = Curr % BITWORD_SIZE;
        BitWord Copy = Bits[WordPos];
        assert(WordPos <= BITWORDS_PER_ELEMENT
               && "Word Position outside of element");

        // Mask off previous bits.
        Copy &= ~0UL << BitPos;

        if (Copy != 0)
            return WordPos * BITWORD_SIZE + countTrailingZeros(Copy);

        // Check subsequent words.
        for (unsigned i = WordPos+1; i < BITWORDS_PER_ELEMENT; ++i)
            if (Bits[i] != 0)
                return i * BITWORD_SIZE + countTrailingZeros(Bits[i]);
        return -1;
    }

    // Union this element with RHS and return true if this one changed.
    bool unionWith(const SparseBitVectorElement &RHS)
    {
        bool changed = false;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
        {
            BitWord old = changed ? 0 : Bits[i];

            Bits[i] |= RHS.Bits[i];
            if (!changed && old != Bits[i])
                changed = true;
        }
        return changed;
    }

    // Return true if we have any bits in common with RHS
    bool intersects(const SparseBitVectorElement &RHS) const
    {
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
        {
            if (RHS.Bits[i] & Bits[i])
                return true;
        }
        return false;
    }

    // Intersect this Element with RHS and return true if this one changed.
    // BecameZero is set to true if this element became all-zero bits.
    bool intersectWith(const SparseBitVectorElement &RHS,
                       bool &BecameZero)
    {
        bool changed = false;
        bool allzero = true;

        BecameZero = false;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
        {
            BitWord old = changed ? 0 : Bits[i];

            Bits[i] &= RHS.Bits[i];
            if (Bits[i] != 0)
                allzero = false;

            if (!changed && old != Bits[i])
                changed = true;
        }
        BecameZero = allzero;
        return changed;
    }

    // Intersect this Element with the complement of RHS and return true if this
    // one changed.  BecameZero is set to true if this element became all-zero
    // bits.
    bool intersectWithComplement(const SparseBitVectorElement &RHS,
                                 bool &BecameZero)
    {
        bool changed = false;
        bool allzero = true;

        BecameZero = false;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
        {
            BitWord old = changed ? 0 : Bits[i];

            Bits[i] &= ~RHS.Bits[i];
            if (Bits[i] != 0)
                allzero = false;

            if (!changed && old != Bits[i])
                changed = true;
        }
        BecameZero = allzero;
        return changed;
    }

    // Three argument version of intersectWithComplement that intersects
    // RHS1 & ~RHS2 into this element
    void intersectWithComplement(const SparseBitVectorElement &RHS1,
                                 const SparseBitVectorElement &RHS2,
                                 bool &BecameZero)
    {
        bool allzero = true;

        BecameZero = false;
        for (unsigned i = 0; i < BITWORDS_PER_ELEMENT; ++i)
        {
            Bits[i] = RHS1.Bits[i] & ~RHS2.Bits[i];
            if (Bits[i] != 0)
                allzero = false;
        }
        BecameZero = allzero;
    }
};

template <unsigned ElementSize = 128>
class SparseBitVector
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

    using ElementList = std::list<SparseBitVectorElement<ElementSize>>;
    using ElementListIter = typename ElementList::iterator;
    using ElementListConstIter = typename ElementList::const_iterator;
    enum
    {
        BITWORD_SIZE = SparseBitVectorElement<ElementSize>::BITWORD_SIZE
    };

    ElementList Elements;
    // Pointer to our current Element. This has no visible effect on the external
    // state of a SparseBitVector, it's just used to improve performance in the
    // common case of testing/modifying bits with similar indices.
    mutable ElementListIter CurrElementIter;

    // This is like std::lower_bound, except we do linear searching from the
    // current position.
    ElementListIter FindLowerBoundImpl(unsigned ElementIndex) const
    {

        // We cache a non-const iterator so we're forced to resort to const_cast to
        // get the begin/end in the case where 'this' is const. To avoid duplication
        // of code with the only difference being whether the const cast is present
        // 'this' is always const in this particular function and we sort out the
        // difference in FindLowerBound and FindLowerBoundConst.
        ElementListIter Begin =
            const_cast<SparseBitVector<ElementSize> *>(this)->Elements.begin();
        ElementListIter End =
            const_cast<SparseBitVector<ElementSize> *>(this)->Elements.end();

        if (Elements.empty())
        {
            CurrElementIter = Begin;
            return CurrElementIter;
        }

        // Make sure our current iterator is valid.
        if (CurrElementIter == End)
            --CurrElementIter;

        // Search from our current iterator, either backwards or forwards,
        // depending on what element we are looking for.
        ElementListIter ElementIter = CurrElementIter;
        if (CurrElementIter->index() == ElementIndex)
        {
            return ElementIter;
        }
        else if (CurrElementIter->index() > ElementIndex)
        {
            while (ElementIter != Begin
                    && ElementIter->index() > ElementIndex)
                --ElementIter;
        }
        else
        {
            while (ElementIter != End &&
                    ElementIter->index() < ElementIndex)
                ++ElementIter;
        }
        CurrElementIter = ElementIter;
        return ElementIter;
    }
    ElementListConstIter FindLowerBoundConst(unsigned ElementIndex) const
    {
        return FindLowerBoundImpl(ElementIndex);
    }
    ElementListIter FindLowerBound(unsigned ElementIndex)
    {
        return FindLowerBoundImpl(ElementIndex);
    }

    // Iterator to walk set bits in the bitmap.  This iterator is a lot uglier
    // than it would be, in order to be efficient.
    class SparseBitVectorIterator
    {
    private:
        bool AtEnd;

        const SparseBitVector<ElementSize> *BitVector = nullptr;

        // Current element inside of bitmap.
        ElementListConstIter Iter;

        // Current bit number inside of our bitmap.
        unsigned BitNumber;

        // Current word number inside of our element.
        unsigned WordNumber;

        // Current bits from the element.
        typename SparseBitVectorElement<ElementSize>::BitWord Bits;

        // Move our iterator to the first non-zero bit in the bitmap.
        void AdvanceToFirstNonZero()
        {
            if (AtEnd)
                return;
            if (BitVector->Elements.empty())
            {
                AtEnd = true;
                return;
            }
            Iter = BitVector->Elements.begin();
            BitNumber = Iter->index() * ElementSize;
            unsigned BitPos = Iter->find_first();
            BitNumber += BitPos;
            WordNumber = (BitNumber % ElementSize) / BITWORD_SIZE;
            Bits = Iter->word(WordNumber);
            Bits >>= BitPos % BITWORD_SIZE;
        }

        // Move our iterator to the next non-zero bit.
        void AdvanceToNextNonZero()
        {
            if (AtEnd)
                return;

            while (Bits && !(Bits & 1))
            {
                Bits >>= 1;
                BitNumber += 1;
            }

            // See if we ran out of Bits in this word.
            if (!Bits)
            {
                int NextSetBitNumber = Iter->find_next(BitNumber % ElementSize) ;
                // If we ran out of set bits in this element, move to next element.
                if (NextSetBitNumber == -1 || (BitNumber % ElementSize == 0))
                {
                    ++Iter;
                    WordNumber = 0;

                    // We may run out of elements in the bitmap.
                    if (Iter == BitVector->Elements.end())
                    {
                        AtEnd = true;
                        return;
                    }
                    // Set up for next non-zero word in bitmap.
                    BitNumber = Iter->index() * ElementSize;
                    NextSetBitNumber = Iter->find_first();
                    BitNumber += NextSetBitNumber;
                    WordNumber = (BitNumber % ElementSize) / BITWORD_SIZE;
                    Bits = Iter->word(WordNumber);
                    Bits >>= NextSetBitNumber % BITWORD_SIZE;
                }
                else
                {
                    WordNumber = (NextSetBitNumber % ElementSize) / BITWORD_SIZE;
                    Bits = Iter->word(WordNumber);
                    Bits >>= NextSetBitNumber % BITWORD_SIZE;
                    BitNumber = Iter->index() * ElementSize;
                    BitNumber += NextSetBitNumber;
                }
            }
        }

    public:
        SparseBitVectorIterator() = delete;

        SparseBitVectorIterator(const SparseBitVector<ElementSize> *RHS,
                                bool end = false):BitVector(RHS)
        {
            Iter = BitVector->Elements.begin();
            BitNumber = 0;
            Bits = 0;
            WordNumber = ~0;
            AtEnd = end;
            AdvanceToFirstNonZero();
        }

        // Preincrement.
        inline SparseBitVectorIterator& operator++()
        {
            ++BitNumber;
            Bits >>= 1;
            AdvanceToNextNonZero();
            return *this;
        }

        // Postincrement.
        inline SparseBitVectorIterator operator++(int)
        {
            SparseBitVectorIterator tmp = *this;
            ++*this;
            return tmp;
        }

        // Return the current set bit number.
        unsigned operator*() const
        {
            return BitNumber;
        }

        bool operator==(const SparseBitVectorIterator &RHS) const
        {
            // If they are both at the end, ignore the rest of the fields.
            if (AtEnd && RHS.AtEnd)
                return true;
            // Otherwise they are the same if they have the same bit number and
            // bitmap.
            return AtEnd == RHS.AtEnd && RHS.BitNumber == BitNumber;
        }

        bool operator!=(const SparseBitVectorIterator &RHS) const
        {
            return !(*this == RHS);
        }
    };

public:
    using iterator = SparseBitVectorIterator;

    SparseBitVector() : Elements(), CurrElementIter(Elements.begin()) {}

    SparseBitVector(const SparseBitVector &RHS)
        : Elements(RHS.Elements), CurrElementIter(Elements.begin()) {}
    SparseBitVector(SparseBitVector &&RHS)
    noexcept         : Elements(std::move(RHS.Elements)), CurrElementIter(Elements.begin()) {}

    // Clear.
    void clear()
    {
        Elements.clear();
    }

    // Assignment
    SparseBitVector& operator=(const SparseBitVector& RHS)
    {
        if (this == &RHS)
            return *this;

        Elements = RHS.Elements;
        CurrElementIter = Elements.begin();
        return *this;
    }
    SparseBitVector &operator=(SparseBitVector &&RHS)
    {
        Elements = std::move(RHS.Elements);
        CurrElementIter = Elements.begin();
        return *this;
    }

    // Test, Reset, and Set a bit in the bitmap.
    bool test(unsigned Idx) const
    {
        if (Elements.empty())
            return false;

        unsigned ElementIndex = Idx / ElementSize;
        ElementListConstIter ElementIter = FindLowerBoundConst(ElementIndex);

        // If we can't find an element that is supposed to contain this bit, there
        // is nothing more to do.
        if (ElementIter == Elements.end() ||
                ElementIter->index() != ElementIndex)
            return false;
        return ElementIter->test(Idx % ElementSize);
    }

    void reset(unsigned Idx)
    {
        if (Elements.empty())
            return;

        unsigned ElementIndex = Idx / ElementSize;
        ElementListIter ElementIter = FindLowerBound(ElementIndex);

        // If we can't find an element that is supposed to contain this bit, there
        // is nothing more to do.
        if (ElementIter == Elements.end() ||
                ElementIter->index() != ElementIndex)
            return;
        ElementIter->reset(Idx % ElementSize);

        // When the element is zeroed out, delete it.
        if (ElementIter->empty())
        {
            ++CurrElementIter;
            Elements.erase(ElementIter);
        }
    }

    void set(unsigned Idx)
    {
        unsigned ElementIndex = Idx / ElementSize;
        ElementListIter ElementIter;
        if (Elements.empty())
        {
            ElementIter = Elements.emplace(Elements.end(), ElementIndex);
        }
        else
        {
            ElementIter = FindLowerBound(ElementIndex);

            if (ElementIter == Elements.end() ||
                    ElementIter->index() != ElementIndex)
            {
                // We may have hit the beginning of our SparseBitVector, in which case,
                // we may need to insert right after this element, which requires moving
                // the current iterator forward one, because insert does insert before.
                if (ElementIter != Elements.end() &&
                        ElementIter->index() < ElementIndex)
                    ++ElementIter;
                ElementIter = Elements.emplace(ElementIter, ElementIndex);
            }
        }
        CurrElementIter = ElementIter;

        ElementIter->set(Idx % ElementSize);
    }

    bool test_and_set(unsigned Idx)
    {
        bool old = test(Idx);
        if (!old)
        {
            set(Idx);
            return true;
        }
        return false;
    }

    bool operator!=(const SparseBitVector &RHS) const
    {
        return !(*this == RHS);
    }

    bool operator==(const SparseBitVector &RHS) const
    {
        ElementListConstIter Iter1 = Elements.begin();
        ElementListConstIter Iter2 = RHS.Elements.begin();

        for (; Iter1 != Elements.end() && Iter2 != RHS.Elements.end();
                ++Iter1, ++Iter2)
        {
            if (*Iter1 != *Iter2)
                return false;
        }
        return Iter1 == Elements.end() && Iter2 == RHS.Elements.end();
    }

    // Union our bitmap with the RHS and return true if we changed.
    bool operator|=(const SparseBitVector &RHS)
    {
        if (this == &RHS)
            return false;

        bool changed = false;
        ElementListIter Iter1 = Elements.begin();
        ElementListConstIter Iter2 = RHS.Elements.begin();

        // If RHS is empty, we are done
        if (RHS.Elements.empty())
            return false;

        while (Iter2 != RHS.Elements.end())
        {
            if (Iter1 == Elements.end() || Iter1->index() > Iter2->index())
            {
                Elements.insert(Iter1, *Iter2);
                ++Iter2;
                changed = true;
            }
            else if (Iter1->index() == Iter2->index())
            {
                changed |= Iter1->unionWith(*Iter2);
                ++Iter1;
                ++Iter2;
            }
            else
            {
                ++Iter1;
            }
        }
        CurrElementIter = Elements.begin();
        return changed;
    }

    // Intersect our bitmap with the RHS and return true if ours changed.
    bool operator&=(const SparseBitVector &RHS)
    {
        if (this == &RHS)
            return false;

        bool changed = false;
        ElementListIter Iter1 = Elements.begin();
        ElementListConstIter Iter2 = RHS.Elements.begin();

        // Check if both bitmaps are empty.
        if (Elements.empty() && RHS.Elements.empty())
            return false;

        // Loop through, intersecting as we go, erasing elements when necessary.
        while (Iter2 != RHS.Elements.end())
        {
            if (Iter1 == Elements.end())
            {
                CurrElementIter = Elements.begin();
                return changed;
            }

            if (Iter1->index() > Iter2->index())
            {
                ++Iter2;
            }
            else if (Iter1->index() == Iter2->index())
            {
                bool BecameZero;
                changed |= Iter1->intersectWith(*Iter2, BecameZero);
                if (BecameZero)
                {
                    ElementListIter IterTmp = Iter1;
                    ++Iter1;
                    Elements.erase(IterTmp);
                }
                else
                {
                    ++Iter1;
                }
                ++Iter2;
            }
            else
            {
                ElementListIter IterTmp = Iter1;
                ++Iter1;
                Elements.erase(IterTmp);
                changed = true;
            }
        }
        if (Iter1 != Elements.end())
        {
            Elements.erase(Iter1, Elements.end());
            changed = true;
        }
        CurrElementIter = Elements.begin();
        return changed;
    }

    // Intersect our bitmap with the complement of the RHS and return true
    // if ours changed.
    bool intersectWithComplement(const SparseBitVector &RHS)
    {
        if (this == &RHS)
        {
            if (!empty())
            {
                clear();
                return true;
            }
            return false;
        }

        bool changed = false;
        ElementListIter Iter1 = Elements.begin();
        ElementListConstIter Iter2 = RHS.Elements.begin();

        // If either our bitmap or RHS is empty, we are done
        if (Elements.empty() || RHS.Elements.empty())
            return false;

        // Loop through, intersecting as we go, erasing elements when necessary.
        while (Iter2 != RHS.Elements.end())
        {
            if (Iter1 == Elements.end())
            {
                CurrElementIter = Elements.begin();
                return changed;
            }

            if (Iter1->index() > Iter2->index())
            {
                ++Iter2;
            }
            else if (Iter1->index() == Iter2->index())
            {
                bool BecameZero;
                changed |= Iter1->intersectWithComplement(*Iter2, BecameZero);
                if (BecameZero)
                {
                    ElementListIter IterTmp = Iter1;
                    ++Iter1;
                    Elements.erase(IterTmp);
                }
                else
                {
                    ++Iter1;
                }
                ++Iter2;
            }
            else
            {
                ++Iter1;
            }
        }
        CurrElementIter = Elements.begin();
        return changed;
    }

    bool intersectWithComplement(const SparseBitVector<ElementSize> *RHS) const
    {
        return intersectWithComplement(*RHS);
    }

    //  Three argument version of intersectWithComplement.
    //  Result of RHS1 & ~RHS2 is stored into this bitmap.
    void intersectWithComplement(const SparseBitVector<ElementSize> &RHS1,
                                 const SparseBitVector<ElementSize> &RHS2)
    {
        if (this == &RHS1)
        {
            intersectWithComplement(RHS2);
            return;
        }
        else if (this == &RHS2)
        {
            SparseBitVector RHS2Copy(RHS2);
            intersectWithComplement(RHS1, RHS2Copy);
            return;
        }

        Elements.clear();
        CurrElementIter = Elements.begin();
        ElementListConstIter Iter1 = RHS1.Elements.begin();
        ElementListConstIter Iter2 = RHS2.Elements.begin();

        // If RHS1 is empty, we are done
        // If RHS2 is empty, we still have to copy RHS1
        if (RHS1.Elements.empty())
            return;

        // Loop through, intersecting as we go, erasing elements when necessary.
        while (Iter2 != RHS2.Elements.end())
        {
            if (Iter1 == RHS1.Elements.end())
                return;

            if (Iter1->index() > Iter2->index())
            {
                ++Iter2;
            }
            else if (Iter1->index() == Iter2->index())
            {
                bool BecameZero = false;
                Elements.emplace_back(Iter1->index());
                Elements.back().intersectWithComplement(*Iter1, *Iter2, BecameZero);
                if (BecameZero)
                    Elements.pop_back();
                ++Iter1;
                ++Iter2;
            }
            else
            {
                Elements.push_back(*Iter1++);
            }
        }

        // copy the remaining elements
        std::copy(Iter1, RHS1.Elements.end(), std::back_inserter(Elements));
    }

    void intersectWithComplement(const SparseBitVector<ElementSize> *RHS1,
                                 const SparseBitVector<ElementSize> *RHS2)
    {
        intersectWithComplement(*RHS1, *RHS2);
    }

    bool intersects(const SparseBitVector<ElementSize> *RHS) const
    {
        return intersects(*RHS);
    }

    // Return true if we share any bits in common with RHS
    bool intersects(const SparseBitVector<ElementSize> &RHS) const
    {
        ElementListConstIter Iter1 = Elements.begin();
        ElementListConstIter Iter2 = RHS.Elements.begin();

        // Check if both bitmaps are empty.
        if (Elements.empty() && RHS.Elements.empty())
            return false;

        // Loop through, intersecting stopping when we hit bits in common.
        while (Iter2 != RHS.Elements.end())
        {
            if (Iter1 == Elements.end())
                return false;

            if (Iter1->index() > Iter2->index())
            {
                ++Iter2;
            }
            else if (Iter1->index() == Iter2->index())
            {
                if (Iter1->intersects(*Iter2))
                    return true;
                ++Iter1;
                ++Iter2;
            }
            else
            {
                ++Iter1;
            }
        }
        return false;
    }

    // Return true iff all bits set in this SparseBitVector are
    // also set in RHS.
    bool contains(const SparseBitVector<ElementSize> &RHS) const
    {
        SparseBitVector<ElementSize> Result(*this);
        Result &= RHS;
        return (Result == RHS);
    }

    // Return the first set bit in the bitmap.  Return -1 if no bits are set.
    int find_first() const
    {
        if (Elements.empty())
            return -1;
        const SparseBitVectorElement<ElementSize> &First = *(Elements.begin());
        return (First.index() * ElementSize) + First.find_first();
    }

    // Return the last set bit in the bitmap.  Return -1 if no bits are set.
    int find_last() const
    {
        if (Elements.empty())
            return -1;
        const SparseBitVectorElement<ElementSize> &Last = *(Elements.rbegin());
        return (Last.index() * ElementSize) + Last.find_last();
    }

    // Return true if the SparseBitVector is empty
    bool empty() const
    {
        return Elements.empty();
    }

    unsigned count() const
    {
        unsigned BitCount = 0;
        for (ElementListConstIter Iter = Elements.begin();
                Iter != Elements.end();
                ++Iter)
            BitCount += Iter->count();

        return BitCount;
    }

    iterator begin() const
    {
        return iterator(this);
    }

    iterator end() const
    {
        return iterator(this, true);
    }
};

// Convenience functions to allow Or and And without dereferencing in the user
// code.

template <unsigned ElementSize>
inline bool operator |=(SparseBitVector<ElementSize> &LHS,
                        const SparseBitVector<ElementSize> *RHS)
{
    return LHS |= *RHS;
}

template <unsigned ElementSize>
inline bool operator |=(SparseBitVector<ElementSize> *LHS,
                        const SparseBitVector<ElementSize> &RHS)
{
    return LHS->operator|=(RHS);
}

template <unsigned ElementSize>
inline bool operator &=(SparseBitVector<ElementSize> *LHS,
                        const SparseBitVector<ElementSize> &RHS)
{
    return LHS->operator&=(RHS);
}

template <unsigned ElementSize>
inline bool operator &=(SparseBitVector<ElementSize> &LHS,
                        const SparseBitVector<ElementSize> *RHS)
{
    return LHS &= *RHS;
}

// Convenience functions for infix union, intersection, difference operators.

template <unsigned ElementSize>
inline SparseBitVector<ElementSize>
operator|(const SparseBitVector<ElementSize> &LHS,
          const SparseBitVector<ElementSize> &RHS)
{
    SparseBitVector<ElementSize> Result(LHS);
    Result |= RHS;
    return Result;
}

template <unsigned ElementSize>
inline SparseBitVector<ElementSize>
operator&(const SparseBitVector<ElementSize> &LHS,
          const SparseBitVector<ElementSize> &RHS)
{
    SparseBitVector<ElementSize> Result(LHS);
    Result &= RHS;
    return Result;
}

template <unsigned ElementSize>
inline SparseBitVector<ElementSize>
operator-(const SparseBitVector<ElementSize> &LHS,
          const SparseBitVector<ElementSize> &RHS)
{
    SparseBitVector<ElementSize> Result;
    Result.intersectWithComplement(LHS, RHS);
    return Result;
}

// Dump a SparseBitVector to a stream
template <unsigned ElementSize>
void dump(const SparseBitVector<ElementSize> &LHS, std::ostream &out)
{
    out << "[";

    typename SparseBitVector<ElementSize>::iterator bi = LHS.begin(),
                                                    be = LHS.end();
    if (bi != be)
    {
        out << *bi;
        for (++bi; bi != be; ++bi)
        {
            out << " " << *bi;
        }
    }
    out << "]\n";
}

} // End namespace SVF

#endif // SPARSEBITVECTOR_H
