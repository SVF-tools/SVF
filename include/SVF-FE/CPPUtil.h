//===- CPPUtil.h -- Base class of pointer analyses ---------------------------//
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
 * CPPUtil.h
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#ifndef CPPUtil_H_
#define CPPUtil_H_

#include "Util/BasicTypes.h"

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

struct DemangledName demangle(const std::string &name);

std::string getBeforeBrackets(const std::string &name);
bool isValVtbl(const Value *val);
bool isLoadVtblInst(const LoadInst *loadInst);
bool isVirtualCallSite(CallSite cs);
bool isConstructor(const Function *F);
bool isDestructor(const Function *F);
bool isCPPThunkFunction(const Function *F);
const Function *getThunkTarget(const Function *F);

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
const Value *getVCallThisPtr(CallSite cs);
const Value *getVCallVtblPtr(CallSite cs);
u64_t getVCallIdx(CallSite cs);
std::string getClassNameFromVtblObj(const Value *value);
std::string getClassNameFromType(const Type *ty);
std::string getClassNameOfThisPtr(CallSite cs);
std::string getFunNameOfVCallSite(CallSite cs);
bool VCallInCtorOrDtor(CallSite cs);

/*
 *  A(A* this){
 *      store this this.addr;
 *      tmp = load this.addr;
 *      this1 = bitcast(tmp);
 *      B(this1);
 *  }
 *  this and this1 are the same thisPtr in the constructor
 */
bool isSameThisPtrInConstructor(const Argument* thisPtr1, const Value* thisPtr2);

/// Constants pertaining to CTir, for C and C++.
/// TODO: move helper functions here too?
namespace ctir
{
/// On loads, stores, GEPs representing dereferences, and calls
/// representing virtual calls.
/// (The static type.)
const std::string derefMDName  = "ctir";
/// On the (global) virtual table itself.
/// (The class it corresponds to.)
const std::string vtMDName     = "ctir.vt";
/// On the bitcast of `this` to i8*.
/// (The class the constructor it corresponds to.)
const std::string vtInitMDName = "ctir.vt.init";

/// Value we expect a ctir-annotated module to have.
const uint32_t moduleFlagValue = 1;
}

} // End namespace cppUtil

} // End namespace SVF

#endif /* CPPUtil_H_ */
