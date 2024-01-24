//===- TypeInference.h -- Type inference----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * TypeInference.h
 *
 *  Created by Xiao Cheng on 10/01/24.
 *
 */

#ifndef SVF_TYPEINFERENCE_H
#define SVF_TYPEINFERENCE_H

#include "SVF-LLVM/LLVMUtil.h"

namespace SVF {
class TypeInference {

public:
    typedef Set<const Value *> ValueSet;
    typedef Map<const Value *, ValueSet> ValueToValueSet;
    typedef ValueToValueSet ValueToInferSites;
    typedef ValueToValueSet ValueToSources;
    typedef Map<const Value *, const Type *> ValueToType;
    typedef std::pair<const Value *, bool> ValueBoolPair;


private:
    ValueToInferSites _valueToInferSites; // value inference site cache
    ValueToType _valueToType; // value type cache
    ValueToSources _valueToAllocs; // value allocations (stack, static, heap) cache


public:

    explicit TypeInference() = default;

    ~TypeInference() = default;


    /// get or infer the type of a value
    const Type *inferObjType(const Value *startValue);

    /// Validate type inference
    void validateTypeCheck(const CallBase *cs);

    void typeSizeDiffTest(const PointerType *oPTy, const Type *iTy, const Value *val);

    /// Default type
    const Type *defaultTy(const Value *val);

    /// Opaque pointer type
    inline const Type *defaultPtrTy() {
        return PointerType::getUnqual(getLLVMCtx());
    }

    inline LLVMContext &getLLVMCtx() {
        return LLVMModuleSet::getLLVMModuleSet()->getContext();
    }

private:

    /// Forward collect all possible infer sites starting from a value
    const Type *fwInferObjType(const Value *startValue);

    /// Backward collect all possible allocation sites (stack, static, heap) starting from a value
    Set<const Value *> bwfindAllocations(const Value *startValue);

    inline bool isAllocation(const Value *val) {
        return LLVMUtil::isObject(val);
    }

public:
    /// Select the largest (conservative) type from all types
    const Type *selectLargestType(Set<const Type *> &objTys);

    u32_t objTyToNumFields(const Type *objTy);

    u32_t getArgPosInCall(const CallBase *callBase, const Value *arg);

};
}
#endif //SVF_TYPEINFERENCE_H
