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
    typedef Map<const Value *, Set<const Value *>> ValueToInferSites;
    typedef Map<const Value *, const Type *> ValueToType;

private:
    static std::unique_ptr<TypeInference> _typeInference;
    ValueToInferSites _valueToInferSites; // value inference site cache
    ValueToType _valueToType; // value type cache

    explicit TypeInference() = default;

public:

    ~TypeInference() = default;

    /// Singleton
    static inline std::unique_ptr<TypeInference> &getTypeInference() {
        if (_typeInference == nullptr) {
            _typeInference = std::unique_ptr<TypeInference>(new TypeInference());
        }
        return _typeInference;
    }

    /// Forward collect all possible infer sites starting from a value
    const Type *getOrInferLLVMObjType(const Value *startValue);

    /// Default type
    const Type *defaultTy(const Value *val);

    /// Validate type inference
    void validateTypeCheck(const CallBase *cs);

    void typeDiffTest(const Type *oPTy, const Type *iTy, const Value *val);

protected:
    static const Type *infersiteToType(const Value *val);
};
}
#endif //SVF_TYPEINFERENCE_H
