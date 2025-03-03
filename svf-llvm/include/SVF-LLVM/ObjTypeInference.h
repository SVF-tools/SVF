//===- ObjTypeInference.h -- Type inference----------------------------//
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
 * ObjTypeInference.h
 *
 *  Created by Xiao Cheng on 10/01/24.
 *
 */

#ifndef SVF_OBJTYPEINFERENCE_H
#define SVF_OBJTYPEINFERENCE_H

#include "Util/SVFUtil.h"
#include "SVF-LLVM/BasicTypes.h"
#include "Util/ThreadAPI.h"

namespace SVF
{
class ObjTypeInference
{

public:
    typedef Set<const Value *> ValueSet;
    typedef Map<const Value *, ValueSet> ValueToValueSet;
    typedef ValueToValueSet ValueToInferSites;
    typedef ValueToValueSet ValueToSources;
    typedef Map<const Value *, const Type *> ValueToType;
    typedef std::pair<const Value *, bool> ValueBoolPair;
    typedef Map<const Value *, Set<std::string>> ValueToClassNames;
    typedef Map<const Value *, Set<const CallBase *>> ObjToClsNameSources;


private:
    ValueToInferSites _valueToInferSites; // value inference site cache
    ValueToType _valueToType; // value type cache
    ValueToSources _valueToAllocs; // value allocations (stack, static, heap) cache
    ValueToClassNames _thisPtrClassNames; // thisptr class name cache
    ValueToSources _valueToAllocOrClsNameSources; // value alloc/clsname sources cache
    ObjToClsNameSources _objToClsNameSources; // alloc clsname sources cache


public:

    explicit ObjTypeInference() = default;

    ~ObjTypeInference() = default;


    /// get or infer the type of the object pointed by the value
    const Type *inferObjType(const Value *var);

    const Type *inferPointsToType(const Value *var);

    /// validate type inference
    void validateTypeCheck(const CallBase *cs);

    void typeSizeDiffTest(const PointerType *oPTy, const Type *iTy, const Value *val);

    /// default type
    const Type *defaultType(const Value *val);

    /// pointer type
    inline const Type *ptrType()
    {
        return PointerType::getUnqual(getLLVMCtx());
    }

    /// int8 type
    inline const IntegerType *int8Type()
    {
        return Type::getInt8Ty(getLLVMCtx());
    }

    LLVMContext &getLLVMCtx();

private:

    /// forward infer the type of the object pointed by var
    const Type *fwInferObjType(const Value *var);

    /// backward collect all possible allocation sites (stack, static, heap) of var
    Set<const Value *>& bwfindAllocOfVar(const Value *var);

    /// is allocation (stack, static, heap)
    bool isAlloc(const SVF::Value *val);

public:
    /// select the largest (conservative) type from all types
    const Type *selectLargestSizedType(Set<const Type *> &objTys);

    u32_t objTyToNumFields(const Type *objTy);

    u32_t getArgPosInCall(const CallBase *callBase, const Value *arg);

public:
    /// get or infer the class names of thisptr
    Set<std::string> &inferThisPtrClsName(const Value *thisPtr);

protected:

    /// find all possible allocations or
    /// class name sources (e.g., constructors/destructors or template functions) starting from a value
    Set<const Value *> &bwFindAllocOrClsNameSources(const Value *startValue);

    /// forward find class name sources starting from an allocation
    Set<const CallBase *> &fwFindClsNameSources(const Value *startValue);
};
}
#endif //SVF_OBJTYPEINFERENCE_H
