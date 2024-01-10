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

#define ABORT_MSG(reason)                                                      \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << __FILE__ << ':' << __LINE__ << ": " << reason       \
                        << '\n';                                               \
        abort();                                                               \
    } while (0)
#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            ABORT_MSG(reason);                                                 \
    } while (0)

#define VALUE_WITH_DBGINFO(value)                                              \
    LLVMUtil::dumpValue(value) + LLVMUtil::getSourceLoc(value)

#define TYPE_DEBUG 1 /* Turn this on if you're debugging type inference */

#if TYPE_DEBUG
#define DBLOG(msg)                                                             \
    do                                                                         \
    {                                                                          \
        SVFUtil::outs() << __FILE__ << ':' << __LINE__ << ": "                 \
            << SVFUtil::wrnMsg(msg)  << '\n';                                  \
    } while (0)

#else
#define DBLOG(msg)                                                             \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

namespace SVF {
class TypeInference {

public:
    typedef Map<const Value *, Set<const Value *>> ValueToInferSites;

private:
    static std::unique_ptr<TypeInference> _typeInference;
    ValueToInferSites _valueToInferSites; // value inference site cache

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

    static const Type *infersiteToType(const Value *val);

    /// Forward collect all possible infer sites starting from a value
    void forwardCollectAllInfersites(const Value *startValue);

    /// Validate type inference
    void validateTypeCheck(const CallBase *cs);

    const ValueToInferSites &getValueToInferSites() {
        return _valueToInferSites;
    }
};
}
#endif //SVF_TYPEINFERENCE_H
