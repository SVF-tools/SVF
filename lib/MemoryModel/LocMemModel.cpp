//===- LocMemModel.cpp -- Location set based memory model---------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

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
 * LocMemModel.cpp
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei Sui
 */


#include "MemoryModel/LocMemModel.h"
#include "Util/AnalysisUtil.h"

#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include "Util/GEPTypeBridgeIterator.h" // include bridge_gep_iterator 
#include <vector>

using namespace llvm;
using namespace std;
using namespace analysisUtil;


/*!
 * Compute gep offset
 */
bool LocSymTableInfo::computeGepOffset(const llvm::User *V, LocationSet& ls) {

    assert(V);
    int baseIndex = -1;
    int index = 0;
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi, ++index) {
        if(llvm::isa<ConstantInt>(gi.getOperand()) == false)
            baseIndex = index;
    }

    index = 0;
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi, ++index) {

        if (index <= baseIndex) {
            /// variant offset
            // Handling pointer types
            if (const PointerType* pty = dyn_cast<PointerType>(*gi)) {
                const Type* et = pty->getElementType();
                Size_t sz = getTypeSizeInBytes(et);

                Size_t num = 1;
                if (const ArrayType* aty = dyn_cast<ArrayType>(et))
                    num = aty->getNumElements();
                else
                    num = SymbolTableInfo::getMaxFieldLimit();

                ls.addElemNumStridePair(std::make_pair(num, sz));
            }
            // Calculate the size of the array element
            else if(const ArrayType* at = dyn_cast<ArrayType>(*gi)) {
                const Type* et = at->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                Size_t num = at->getNumElements();
                ls.addElemNumStridePair(std::make_pair(num, sz));
            }
            else
                assert(false && "what other types?");
        }
        // constant offset
        else {
            assert(isa<ConstantInt>(gi.getOperand()) && "expecting a constant");

            ConstantInt *op = cast<ConstantInt>(gi.getOperand());

            //The actual index
            Size_t idx = op->getSExtValue();

            // Handling pointer types
            // These GEP instructions are simply making address computations from the base pointer address
            // e.g. idx1 = (char*) &MyVar + 4,  at this case gep only one offset index (idx)
            if (const PointerType* pty = dyn_cast<PointerType>(*gi)) {
                const Type* et = pty->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                ls.offset += idx * sz;
            }
            // Calculate the size of the array element
            else if(const ArrayType* at = dyn_cast<ArrayType>(*gi)) {
                const Type* et = at->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                ls.offset += idx * sz;
            }
            // Handling struct here
            else if (const StructType *ST = dyn_cast<StructType>(*gi)) {
                assert(op && "non-const struct index in GEP");
                const vector<u32_t> &so = SymbolTableInfo::Symbolnfo()->getStructOffsetVec(ST);
                if ((unsigned)idx >= so.size()) {
                    outs() << "!! Struct index out of bounds" << idx << "\n";
                    assert(0);
                }
                //add the translated offset
                ls.offset += so[idx];
            }
            else
                assert(false && "what other types?");
        }
    }
    return true;
}

/*!
 * Collect array information
 */
void LocSymTableInfo::collectArrayInfo(const llvm::ArrayType *ty) {
    StInfo *stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    /// Array itself only has one field which is the inner most element
    stinfo->getFieldOffsetVec().push_back(0);

    /// If this is an array type, calculate the outmost array
    /// information and append them to the inner elements' type
    /// information later.
    u64_t out_num = ty->getNumElements();
    const llvm::Type* elemTy = ty->getElementType();
    u32_t out_stride = getTypeSizeInBytes(elemTy);

    while (const ArrayType* aty = dyn_cast<ArrayType>(elemTy)) {
        out_num *= aty->getNumElements();
        elemTy = aty->getElementType();
        out_stride = getTypeSizeInBytes(elemTy);
    }

    /// Array's flatten field infor is the same as its element's
    /// flatten infor with an additional slot for array's element
    /// number and stride pair.
    StInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getFlattenFieldInfoVec().size();
    for (u32_t j = 0; j < nfE; j++) {
        u32_t off = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
        const Type* fieldTy = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
        FieldInfo::ElemNumStridePairVec pair = elemStInfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
        /// append the additional number
        pair.push_back(std::make_pair(out_num, out_stride));
        FieldInfo field(off, fieldTy, pair);
        stinfo->getFlattenFieldInfoVec().push_back(field);
    }
}


/*
 * Recursively collect the memory layout information for a struct type
 */
void LocSymTableInfo::collectStructInfo(const StructType *ty) {
    StInfo *stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    const StructLayout *stTySL = getDataLayout(getModule().getMainLLVMModule())->getStructLayout( const_cast<StructType *>(ty) );

    u32_t field_idx = 0;
    for (StructType::element_iterator it = ty->element_begin(), ie =
                ty->element_end(); it != ie; ++it, ++field_idx) {
        const Type *et = *it;

        // The offset is where this element will be placed in the struct.
        // This offset is computed after alignment with the current struct
        u64_t eOffsetInBytes = stTySL->getElementOffset(field_idx);

        //The offset is where this element will be placed in the exp. struct.
        /// FIXME: As the layout size is uint_64, here we assume
        /// offset with uint_32 (Size_t) is large enough and will not cause overflow
        stinfo->getFieldOffsetVec().push_back(static_cast<u32_t>(eOffsetInBytes));

        StInfo* fieldStinfo = getStructInfo(et);
        u32_t nfE = fieldStinfo->getFlattenFieldInfoVec().size();
        //Copy ST's info, whose element 0 is the size of ST itself.
        for (u32_t j = 0; j < nfE; j++) {
            u32_t oft = eOffsetInBytes + fieldStinfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
            const Type* elemTy = fieldStinfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
            FieldInfo::ElemNumStridePairVec pair = fieldStinfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
            pair.push_back(std::make_pair(1, 0));
            FieldInfo newField(oft, elemTy, pair);
            stinfo->getFlattenFieldInfoVec().push_back(newField);
        }
    }

//	verifyStructSize(stinfo,stTySL->getSizeInBytes());

    //Record the size of the complete struct and update max_struct.
    if (stTySL->getSizeInBytes() > maxStSize) {
        maxStruct = ty;
        maxStSize = stTySL->getSizeInBytes();
    }
}


/*!
 * Given LocationSet from a Gep Instruction, return a new LocationSet which matches
 * the field information of this ObjTypeInfo by considering memory layout
 */
LocationSet LocSymTableInfo::getModulusOffset(ObjTypeInfo* tyInfo, const LocationSet& ls) {
    llvm::Type* ety = tyInfo->getLLVMType();

    if (isa<StructType>(ety) || isa<ArrayType>(ety)) {
        /// Find an appropriate field for this LocationSet
        const std::vector<FieldInfo>& infovec = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(ety);
        std::vector<FieldInfo>::const_iterator it = infovec.begin();
        std::vector<FieldInfo>::const_iterator eit = infovec.end();
        for (; it != eit; ++it) {
            const FieldInfo& fieldLS = *it;
            LocationSet rhsLS(fieldLS);
            LocationSet::LSRelation result = LocationSet::checkRelation(ls, rhsLS);
            if (result == LocationSet::Same ||
                    result == LocationSet::Superset ||
                    result == LocationSet::Subset)
                return ls;
            else if (result == LocationSet::Overlap) {
                // TODO:
                return ls;
            }
            else if (result == LocationSet::NonOverlap) {
                continue;
            }

            assert(false && "cannot find an appropriate field for specified LocationSet");
            return ls;
        }
    }
    else {
        if (tyInfo->isStaticObj() || tyInfo->isHeap()) {
            // TODO: Objects which cannot find proper field for a certain offset including
            //       arguments in main(), static objects allocated before main and heap
            //       objects. Right now they're considered to have infinite fields. So we
            //       just return the location set without modifying it.
            return ls;
        }
        else {
            // TODO: Using new memory model (locMM) may create objects with spurious offset
            //       as we simply return new offset by mod operation without checking its
            //       correctness in LocSymTableInfo::getModulusOffset(). So the following
            //       assertion may fail. Try to refine the new memory model.
            assert(ls.isConstantOffset() && "expecting a constant location set");
            return ls;
        }
    }

    /// This location set represent one object
    if (ls.isConstantOffset()) {
        /// if the offset is negative, it's possible that we're looking for an obj node out of range
        /// of current struct. Make the offset positive so we can still get a node within current
        /// struct to represent this obj.
        Size_t offset = ls.getOffset();
        if(offset < 0) {
            wrnMsg("try to create a gep node with negative offset.");
            offset = abs(offset);
        }
        u32_t maxOffset = tyInfo->getMaxFieldOffsetLimit();
        if (maxOffset != 0)
            offset = offset % maxOffset;
        else
            offset = 0;
    }
    /// This location set represents multiple objects
    else {

    }

    return ls;
}

/*!
 * Verify struct size
 */
void LocSymTableInfo::verifyStructSize(StInfo *stinfo, u32_t structSize) {

    u32_t lastOff = stinfo->getFlattenFieldInfoVec().back().getFlattenOffset();
    u32_t strideSize = 0;
    FieldInfo::ElemNumStridePairVec::const_iterator pit = stinfo->getFlattenFieldInfoVec().back().elemStridePairBegin();
    FieldInfo::ElemNumStridePairVec::const_iterator epit = stinfo->getFlattenFieldInfoVec().back().elemStridePairEnd();
    for(; pit!=epit; ++pit)
        strideSize += pit->first * pit->second;

    u32_t lastSize = getTypeSizeInBytes(stinfo->getFlattenFieldInfoVec().back().getFlattenElemTy());
    /// Please note this verify may not be complete as different machine has different alignment mechanism
    assert((structSize == lastOff + strideSize + lastSize) && "struct size not consistent");

}

/*!
 * Get the size of this object
 */
u32_t LocObjTypeInfo::getObjSize(const Value* val) {

    Type* ety  = cast<PointerType>(val->getType())->getElementType();
    return LocSymTableInfo::Symbolnfo()->getTypeSizeInBytes(ety);
}
