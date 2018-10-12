//===- CPPUtil.cpp -- Base class of pointer analyses -------------------------//
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
 * CPPUtil.cpp
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#include "Util/CPPUtil.h"
#include "MemoryModel/CHA.h"
#include <cxxabi.h>   // for demangling
#include "Util/BasicTypes.h"
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable

using namespace llvm;
using namespace std;

// label for global vtbl value before demangle
const string vtblLabelBeforeDemangle = "_ZTV";

// label for global vtbl value before demangle
const string vtblLabelAfterDemangle = "vtable for ";

// label for virtual functions
const string vfunPreLabel = "_Z";

// label for multi inheritance virtual function
const string mInheritanceVFunLabel = "non-virtual thunk to ";

const string clsName = "class.";
const string structName = "struct.";

static bool isOperOverload(const string name) {
    s32_t leftnum = 0, rightnum = 0;
    string subname = name;
    size_t leftpos, rightpos;
    leftpos = subname.find("<");
    while (leftpos != string::npos) {
        subname = subname.substr(leftpos+1);
        leftpos = subname.find("<");
        leftnum++;
    }
    subname = name;
    rightpos = subname.find(">");
    while (rightpos != string::npos) {
        subname = subname.substr(rightpos+1);
        rightpos = subname.find(">");
        rightnum++;
    }
    if (leftnum != rightnum) {
        return true;
    } else {
        return false;
    }
}

static string getBeforeParenthesis(const string name) {
    size_t lastRightParen = name.rfind(")");
    assert(lastRightParen > 0);

    s32_t paren_num = 1, pos;
    for (pos = lastRightParen - 1; pos >= 0; pos--) {
        if (name[pos] == ')')
            paren_num++;
        if (name[pos] == '(')
            paren_num--;
        if (paren_num == 0)
            break;
    }
    return name.substr(0, pos);
}

string cppUtil::getBeforeBrackets(const string name) {
    if (name[name.size() - 1] != '>') {
        return name;
    }
    s32_t bracket_num = 1, pos;
    for (pos = name.size() - 2; pos >= 0; pos--) {
        if (name[pos] == '>')
            bracket_num++;
        if (name[pos] == '<')
            bracket_num--;
        if (bracket_num == 0)
            break;
    }
    return name.substr(0, pos);
}

bool cppUtil::isValVtbl(const Value *val) {
    if (!isa<GlobalVariable>(val))
        return false;
    string valName = val->getName().str();
    if (valName.compare(0, vtblLabelBeforeDemangle.size(),
                        vtblLabelBeforeDemangle) == 0)
        return true;
    else
        return false;
}

/*
 * input: _ZN****
 * after abi::__cxa_demangle:
 * namespace::A<...::...>::f<...::...>(...)
 *                       ^
 *                    delimiter
 *
 * step1: getBeforeParenthesis
 * namespace::A<...::...>::f<...::...>
 *
 * step2: getBeforeBrackets
 * namespace::A<...::...>::f
 *
 * step3: find delimiter
 * namespace::A<...::...>::
 *                       ^
 *
 * className: namespace::A<...::...>
 * functionName: f<...::...>
 */

struct cppUtil::DemangledName cppUtil::demangle(const string name) {
    struct cppUtil::DemangledName dname;

    s32_t status;
    char *realname = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
    if (realname == NULL) {
        dname.className = "";
        dname.funcName = "";
    } else {
        string realnameStr = string(realname);
        string beforeParenthesis = getBeforeParenthesis(realnameStr);
        if (beforeParenthesis.find("::") == string::npos ||
                isOperOverload(beforeParenthesis)) {
            dname.className = "";
            dname.funcName = "";
        } else {
            string beforeBracket = getBeforeBrackets(beforeParenthesis);
            size_t colon = beforeBracket.rfind("::");
            if (colon == string::npos) {
                dname.className = "";
                dname.funcName = "";
            } else {
                dname.className = beforeParenthesis.substr(0, colon);
                dname.funcName = beforeParenthesis.substr(colon + 2);
            }
        }
    }
    /// multiple inheritance
    if (dname.className.size() > mInheritanceVFunLabel.size() &&
            dname.className.compare(0, mInheritanceVFunLabel.size(),
                                    mInheritanceVFunLabel) == 0) {
        dname.className = dname.className.substr(mInheritanceVFunLabel.size());
    }

    return dname;
}

bool cppUtil::isLoadVtblInst(const LoadInst *loadInst) {
    const Value *loadSrc = loadInst->getPointerOperand();
    const Type *valTy = loadSrc->getType();
    const Type *elemTy = valTy;
    for (s32_t i = 0; i < 3; ++i) {
        if (const PointerType *ptrTy = dyn_cast<PointerType>(elemTy))
            elemTy = ptrTy->getElementType();
        else
            return false;
    }
    if (const FunctionType *functy = dyn_cast<FunctionType>(elemTy)) {
        const Type *paramty = functy->getParamType(0);
        string className = cppUtil::getClassNameFromType(paramty);
        if (className.size() > 0) {
            return true;
        }
    }
    return false;
}

/*
 * a virtual callsite follows the following instruction sequence pattern:
 * %vtable = load ...
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (...)
 */
bool cppUtil::isVirtualCallSite(CallSite cs) {
    if (cs.getCalledFunction() != NULL)
        return false;

    const Value *vfunc = cs.getCalledValue();
    if (const LoadInst *vfuncloadinst = dyn_cast<LoadInst>(vfunc)) {
        const Value *vfuncptr = vfuncloadinst->getPointerOperand();
        if (const GetElementPtrInst *vfuncptrgepinst =
                    dyn_cast<GetElementPtrInst>(vfuncptr)) {
            if (vfuncptrgepinst->getNumIndices() != 1)
                return false;
            const Value *vtbl = vfuncptrgepinst->getPointerOperand();
            if (isa<LoadInst>(vtbl)) {
                return true;
            }
        }
    }
    return false;
}

const Value *cppUtil::getVCallThisPtr(CallSite cs) {
    if (cs.paramHasAttr(0, Attribute::StructRet)) {
        return cs.getArgument(1);
    } else {
        return cs.getArgument(0);
    }
}

bool cppUtil::isSameThisPtrInConstructor(const Argument* thisPtr1, const Value* thisPtr2) {
	if (thisPtr1 == thisPtr2){
		return true;
	}
	else {
		for (const User *thisU : thisPtr1->users()) {
			if (const StoreInst *store = dyn_cast<StoreInst>(thisU)) {
				for (const User *storeU : store->getPointerOperand()->users()) {
					if (const LoadInst *load = dyn_cast<LoadInst>(storeU)) {
						return load == (thisPtr2->stripPointerCasts());
					}
				}
			}
		}
		return false;
	}
}

const Argument *cppUtil::getConstructorThisPtr(const Function* fun) {
	assert((isConstructor(fun) || isDestructor(fun)) && "not a constructor?");
	assert(fun->arg_size() >=1 && "argument size >= 1?");
	const Argument* thisPtr =  &*(fun->arg_begin());
	return thisPtr;
}

/*
 * get the ptr "vtable" for a given virtual callsite:
 * %vtable = load ...
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (...)
 */
const Value *cppUtil::getVCallVtblPtr(CallSite cs) {
    const LoadInst *loadInst = dyn_cast<LoadInst>(cs.getCalledValue());
    assert(loadInst != NULL);
    const Value *vfuncptr = loadInst->getPointerOperand();
    const GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(vfuncptr);
    assert(gepInst != NULL);
    const Value *vtbl = gepInst->getPointerOperand();
    return vtbl;
}

u64_t cppUtil::getVCallIdx(llvm::CallSite cs) {
    const LoadInst *vfuncloadinst = dyn_cast<LoadInst>(cs.getCalledValue());
    assert(vfuncloadinst != NULL);
    const Value *vfuncptr = vfuncloadinst->getPointerOperand();
    const GetElementPtrInst *vfuncptrgepinst =
        dyn_cast<GetElementPtrInst>(vfuncptr);
    User::const_op_iterator oi = vfuncptrgepinst->idx_begin();
    const ConstantInt *idx = dyn_cast<ConstantInt>(*oi);
    u64_t idx_value;
    if (idx == NULL) {
        errs() << "vcall gep idx not constantint\n";
        idx_value = 0;
    } else
        idx_value = idx->getSExtValue();
    return idx_value;
}

string cppUtil::getClassNameFromType(const Type *ty) {
    string className = "";
    if (const PointerType *ptrType = dyn_cast<PointerType>(ty)) {
        const Type *elemType = ptrType->getElementType();
        if (isa<StructType>(elemType) &&
                !((cast<StructType>(elemType))->isLiteral())) {
            string elemTypeName = elemType->getStructName().str();
            if (elemTypeName.compare(0, clsName.size(), clsName) == 0) {
                className = elemTypeName.substr(clsName.size());
            } else if (elemTypeName.compare(0, structName.size(), structName) == 0) {
                className = elemTypeName.substr(structName.size());
            }
        }
    }
    return className;
}

string cppUtil::getClassNameFromVtblObj(const Value *value) {
    string className = "";

    string vtblName = value->getName().str();
    s32_t status;
    char *realname = abi::__cxa_demangle(vtblName.c_str(), 0, 0, &status);
    if (realname != NULL) {
        string realnameStr = string(realname);
        if (realnameStr.compare(0, vtblLabelAfterDemangle.size(),
                                vtblLabelAfterDemangle) == 0) {
            className = realnameStr.substr(vtblLabelAfterDemangle.size());
        }
    }
    return className;
}

bool cppUtil::isConstructor(const Function *F) {
    if (F->isDeclaration())
        return false;
    string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0) {
        return false;
    }
    struct cppUtil::DemangledName dname = demangle(funcName.c_str());
    dname.funcName = getBeforeBrackets(dname.funcName);
    dname.className = getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == string::npos) {
        dname.className = getBeforeBrackets(dname.className);
    } else {
        dname.className = getBeforeBrackets(dname.className.substr(colon+2));
    }
	if (dname.className.size() > 0 && (dname.className.compare(dname.funcName) == 0))
		/// TODO: on mac os function name is an empty string after demangling
		return true;
	else
		return false;
}

bool cppUtil::isDestructor(const Function *F) {
    if (F->isDeclaration())
        return false;
    string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0) {
        return false;
    }
    struct cppUtil::DemangledName dname = demangle(funcName.c_str());
    dname.funcName = getBeforeBrackets(dname.funcName);
    dname.className = getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == string::npos) {
        dname.className = getBeforeBrackets(dname.className);
    } else {
        dname.className = getBeforeBrackets(dname.className.substr(colon+2));
    }
    if (dname.className.size() > 0 && dname.funcName.size() > 0 &&
            dname.className.size() + 1 == dname.funcName.size() &&
            dname.funcName.compare(0, 1, "~") == 0 &&
            dname.className.compare(dname.funcName.substr(1)) == 0)
        return true;
    else
        return false;
}

string cppUtil::getClassNameOfThisPtr(CallSite cs) {
    string thisPtrClassName;
    Instruction *inst = cs.getInstruction();
    if (MDNode *N = inst->getMetadata("VCallPtrType")) {
        MDString *mdstr = cast<MDString>(N->getOperand(0));
        thisPtrClassName = mdstr->getString().str();
    }
    if (thisPtrClassName.size() == 0) {
        const Value *thisPtr = getVCallThisPtr(cs);
        thisPtrClassName = getClassNameFromType(thisPtr->getType());
    }

    size_t found = thisPtrClassName.find_last_not_of("0123456789");
    if (found != string::npos) {
        if (found != thisPtrClassName.size() - 1 && thisPtrClassName[found] == '.') {
            return thisPtrClassName.substr(0, found);
        }
    }

    return thisPtrClassName;
}

string cppUtil::getFunNameOfVCallSite(CallSite cs) {
    string funName;
    Instruction *inst = cs.getInstruction();
    if (MDNode *N = inst->getMetadata("VCallFunName")) {
        MDString *mdstr = cast<MDString>(N->getOperand(0));
        funName = mdstr->getString().str();
    }
    return funName;
}


/*
 * Is this virtual call inside its own constructor or destructor?
 */
bool cppUtil::VCallInCtorOrDtor(CallSite cs)  {
    std::string classNameOfThisPtr = getClassNameOfThisPtr(cs);
    const Function *func = cs.getCaller();
    if (isConstructor(func) || isDestructor(func)) {
        struct DemangledName dname = demangle(func->getName().str());
        if (classNameOfThisPtr.compare(dname.className) == 0)
            return true;
    }
    return false;
}
