//===- CPPUtil.h -- Base class of pointer analyses ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * CPPUtil.h
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#ifndef CPPUtil_H_
#define CPPUtil_H_

#include "SVF-LLVM/BasicTypes.h"
#include "Util/GeneralType.h"

namespace SVF
{

class CHGraph;
/*
 * Util class to assist pointer analysis for cpp programs
 */

namespace cppUtil
{

struct DemangledName
{
    std::string className;
    std::string funcName;
    bool isThunkFunc;
};

struct DemangledName demangle(const std::string& name);


Set<std::string> getClsNamesInBrackets(const std::string& name);

std::string getBeforeBrackets(const std::string& name);
std::string getClassNameFromVtblObj(const std::string& vtblName);

/*
 * Get the vtable struct of a class.
 *
 * Given the class:
 *
 *   class A {
 *     virtual ~A();
 *   };
 *   A::~A() = default;
 *
 *  The corresponding vtable @_ZTV1A is of type:
 *
 *    { [4 x i8*] }
 *
 *  If the program has been compiled with AddressSanitizer,
 *  the vtable will have redzones and appear as:
 *
 *    { { [4 x i8*] }, [32 x i8] }
 *
 *  See https://github.com/SVF-tools/SVF/issues/1114 for more.
 */
const ConstantStruct *getVtblStruct(const GlobalValue *vtbl);

bool isValVtbl(const Value* val);
bool isVirtualCallSite(const CallBase* cs);
bool isConstructor(const Function* F);
bool isDestructor(const Function* F);
bool isCPPThunkFunction(const Function* F);
const Function* getThunkTarget(const Function* F);

/*
 * VtableA = {&A::foo}
 * A::A(this){
 *   *this = &VtableA;
 * }
 *
 *
 * A* p = new A;
 * cs: p->foo(...)
 * ==>
 *  vtptr = *p;
 *  vfn = &vtptr[i]
 *  %funp = *vfn
 *  call %funp(p,...)
 * getConstructorThisPtr(A) return "this" pointer
 * getVCallThisPtr(cs) return p (this pointer)
 * getVCallVtblPtr(cs) return vtptr
 * getVCallIdx(cs) return i
 * getClassNameFromVtblObj(VtableA) return
 * getClassNameFromType(type of p) return type A
 */
const Argument* getConstructorThisPtr(const Function* fun);
const Value* getVCallThisPtr(const CallBase* cs);
const Value* getVCallVtblPtr(const CallBase* cs);
s32_t getVCallIdx(const CallBase* cs);
bool classTyHasVTable(const StructType* ty);
std::string getClassNameFromType(const StructType* ty);
Set<std::string> getClassNameOfThisPtr(const CallBase* cs);
std::string getFunNameOfVCallSite(const CallBase* cs);
bool VCallInCtorOrDtor(const CallBase* cs);

/*
 *  A(A* this){
 *      store this this.addr;
 *      tmp = load this.addr;
 *      this1 = bitcast(tmp);
 *      B(this1);
 *  }
 *  this and this1 are the same thisPtr in the constructor
 */
bool isSameThisPtrInConstructor(const Argument* thisPtr1,
                                const Value* thisPtr2);

/// extract class name from the c++ function name, e.g., constructor/destructors
Set<std::string> extractClsNamesFromFunc(const Function *foo);

/// extract class names from template functions
Set<std::string> extractClsNamesFromTemplate(const std::string &oname);

/// class sources can be heap allocation
/// or functions where we can extract the class name (constructors/destructors or template functions)
bool isClsNameSource(const Value *val);

/// whether foo matches the mangler label
bool matchesLabel(const std::string &foo, const std::string &label);

/// whether foo is a cpp template function
bool isTemplateFunc(const Function *foo);

/// whether foo is a cpp dyncast function
bool isDynCast(const Function *foo);

/// extract class name from cpp dyncast function
std::string extractClsNameFromDynCast(const CallBase* callBase);

const Type *cppClsNameToType(const std::string &className);



/// Constants pertaining to CTir, for C and C++.
/// TODO: move helper functions here too?
namespace ctir
{
/// On loads, stores, GEPs representing dereferences, and calls
/// representing virtual calls.
/// (The static type.)
const std::string derefMDName = "ctir";
/// On the (global) virtual table itself.
/// (The class it corresponds to.)
const std::string vtMDName = "ctir.vt";
/// On the bitcast of `this` to i8*.
/// (The class the constructor it corresponds to.)
const std::string vtInitMDName = "ctir.vt.init";

/// Value we expect a ctir-annotated module to have.
const uint32_t moduleFlagValue = 1;
} // namespace ctir

} // End namespace cppUtil

} // End namespace SVF

#endif /* CPPUtil_H_ */
