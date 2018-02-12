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

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>
#include "Util/BasicTypes.h"

class CHGraph;
/*
 * Util class to assist pointer analysis for cpp programs
 */

namespace cppUtil {

struct DemangledName {
    std::string className;
    std::string funcName;
};

struct DemangledName demangle(const std::string name);

std::string getBeforeBrackets(const std::string name);
bool isValVtbl(const llvm::Value *val);
bool isLoadVtblInst(const llvm::LoadInst *loadInst);
bool isVirtualCallSite(llvm::CallSite cs);
bool isConstructor(const llvm::Function *F);
bool isDestructor(const llvm::Function *F);

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
 *
 * getVCallThisPtr(cs) return p (this pointer)
 * getVCallVtblPtr(cs) return vtptr
 * getVCallIdx(cs) return i
 * getClassNameFromVtblObj(VtableA) return
 * getClassNameFromType(type of p) return type A
 */
const llvm::Value *getVCallThisPtr(llvm::CallSite cs);
const llvm::Value *getVCallVtblPtr(llvm::CallSite cs);
u64_t getVCallIdx(llvm::CallSite cs);
std::string getClassNameFromVtblObj(const llvm::Value *value);
std::string getClassNameFromType(const llvm::Type *ty);

void printCH(const CHGraph *chgraph);
void dumpCHAStats(const CHGraph *chgraph);
}

#endif /* CPPUtil_H_ */
