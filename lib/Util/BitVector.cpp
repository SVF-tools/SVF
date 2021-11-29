//===- BitVector.cpp -- bit vector data structure implementation ------------//

/*
 * BitVector.cpp
 *
 * Contiguous bit vector with trailing 0s stripped (implementation).
 *
 *  Created on: Jul 4, 2021
 *      Author: Mohamad Barbar
 */

#include <Util/BitVector.h>

namespace SVF
{

BitVector::BitVector(void)
    : BitVector(0) { }

BitVector::BitVector(size_t n)
    : CoreBitVector(n)
{
    // This ensures that leading zeroes are never stripped.
    set(0);
    reset(0);
}

BitVector::BitVector(const BitVector &bv)
    : CoreBitVector(bv) { }

BitVector::BitVector(BitVector &&bv)
    : CoreBitVector(bv) { }

BitVector &BitVector::operator=(const BitVector &rhs)
{
    CoreBitVector::operator=(rhs);
    return *this;
}

BitVector &BitVector::operator=(BitVector &&rhs)
{
    CoreBitVector::operator=(rhs);
    return *this;
}

}  // namespace SVF
