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


extern "C" {
void ZSTD_freeCCtx(void) { }
void ZSTD_compress2(void) { }
void ZSTD_getErrorName(void) { }
void compress2(void) { }
void ZSTD_compressBound(void) { }
void ZSTD_decompress(void) { }
void crc32(void) { }
void ZSTD_createCCtx(void) { }
void ZSTD_isError(void) { }
void compressBound(void) { }
void ZSTD_CCtx_setParameter(void) { }
void uncompress(void) { }
}
