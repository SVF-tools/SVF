//===- BitVector.h -- variably sized bit vector data structure ------------//

/*
 * BitVector.h
 *
 * Contiguous bit vector with trailing 0s stripped.
 *
 *  Created on: Jul 4, 2021
 *      Author: Mohamad Barbar
 */

#ifndef BITVECTOR_H_
#define BITVECTOR_H_

#include <Util/CoreBitVector.h>

namespace SVF
{

/// A contiguous bit vector in which trailing 0s are stripped.
/// Does not shrink - only grows. This isn't a problem for
/// points-to analysis, usually, but may be for other applications.
///
/// Implemented as a CBV that has an offset of zero.
/// There is an extremely slight performance penalty in the form
/// of checking if we need to extend backwards.
/// Since the CBV also grows monotonically, this means this BV will
/// always contain a word for 0 till the last ever set bit.
/// TODO: if we change the CBV to shrink, we need to modify this
///       implementation accordingly.
class BitVector : public CoreBitVector
{
public:
    /// Construct empty BV.
    BitVector(void);

    /// Construct empty BV with space reserved for n Words.
    BitVector(size_t n);

    /// Copy constructor.
    BitVector(const BitVector &bv);

    /// Move constructor.
    BitVector(BitVector &&bv);

    /// Copy assignment.
    BitVector &operator=(const BitVector &rhs);

    /// Move assignment.
    BitVector &operator=(BitVector &&rhs);
};

};  // namespace SVF

#endif  // BITVECTOR_H_
