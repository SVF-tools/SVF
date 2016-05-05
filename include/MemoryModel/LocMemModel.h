//===- LocMemModel.h -- Location set based memory model-----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * LocMemModel.h
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei
 */

#ifndef LOCMEMMODEL_H_
#define LOCMEMMODEL_H_

#include "MemoryModel/MemModel.h"

/*!
 * Bytes/bits-level modeling of memory locations to handle weakly type languages.
 * (declared with one type but accessed as another)
 * Abstract memory objects are created according to the static allocated size.
 */
class LocSymTableInfo : public SymbolTableInfo {

public:
    /// Constructor
    LocSymTableInfo() {
    }
    /// Destructor
    virtual ~LocSymTableInfo() {
    }
    /// Compute gep offset
    virtual bool computeGepOffset(const llvm::User *V, LocationSet& ls);
    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual LocationSet getModulusOffset(ObjTypeInfo* tyInfo, const LocationSet& ls);

    /// Verify struct size construction
    void verifyStructSize(StInfo *stInfo, u32_t structSize);

protected:
    /// Collect the struct info
    virtual void collectStructInfo(const llvm::StructType *T);
    /// Collect the array info
    virtual void collectArrayInfo(const llvm::ArrayType *T);
};


class LocObjTypeInfo : public ObjTypeInfo {

public:
    /// Constructor
    LocObjTypeInfo(const llvm::Value* val, llvm::Type* t, Size_t max) : ObjTypeInfo(val,t,max) {

    }
    /// Destructor
    virtual ~LocObjTypeInfo() {

    }
    /// Get the size of this object
    u32_t getObjSize(const llvm::Value* val);

};

#endif /* LOCMEMMODEL_H_ */
