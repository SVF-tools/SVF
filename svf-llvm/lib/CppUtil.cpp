//===- CPPUtil.cpp -- Base class of pointer analyses -------------------------//
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
 * CPPUtil.cpp
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#include "SVF-LLVM/CppUtil.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/SVFUtil.h"
#include "SVF-LLVM/LLVMModule.h"

#include <cxxabi.h> // for demangling

using namespace SVF;

// label for global vtbl value before demangle
const std::string vtblLabelAfterDemangle = "vtable for ";

// label for multi inheritance virtual function
const std::string NVThunkFunLabel = "non-virtual thunk to ";
const std::string VThunkFuncLabel = "virtual thunk to ";

// label for global vtbl value before demangle
const std::string vtblLabelBeforeDemangle = "_ZTV";

// label for virtual functions
const std::string vfunPreLabel = "_Z";

const std::string clsName = "class.";
const std::string structName = "struct.";
const std::string vtableType = "(...)**";


static bool isOperOverload(const std::string& name)
{
    u32_t leftnum = 0, rightnum = 0;
    std::string subname = name;
    size_t leftpos, rightpos;
    leftpos = subname.find("<");
    while (leftpos != std::string::npos)
    {
        subname = subname.substr(leftpos + 1);
        leftpos = subname.find("<");
        leftnum++;
    }
    subname = name;
    rightpos = subname.find(">");
    while (rightpos != std::string::npos)
    {
        subname = subname.substr(rightpos + 1);
        rightpos = subname.find(">");
        rightnum++;
    }
    return leftnum != rightnum;
}

static std::string getBeforeParenthesis(const std::string& name)
{
    size_t lastRightParen = name.rfind(")");
    assert(lastRightParen > 0);

    s32_t paren_num = 1, pos;
    for (pos = lastRightParen - 1; pos >= 0; pos--)
    {
        if (name[pos] == ')')
            paren_num++;
        if (name[pos] == '(')
            paren_num--;
        if (paren_num == 0)
            break;
    }
    return name.substr(0, pos);
}

std::string cppUtil::getBeforeBrackets(const std::string& name)
{
    if (name.size() == 0 || name[name.size() - 1] != '>')
    {
        return name;
    }
    s32_t bracket_num = 1, pos;
    for (pos = name.size() - 2; pos >= 0; pos--)
    {
        if (name[pos] == '>')
            bracket_num++;
        if (name[pos] == '<')
            bracket_num--;
        if (bracket_num == 0)
            break;
    }
    return name.substr(0, pos);
}

static void handleThunkFunction(cppUtil::DemangledName& dname)
{
    // when handling multi-inheritance,
    // the compiler may generate thunk functions
    // to perform `this` pointer adjustment
    // they are indicated with `virtual thunk to `
    // and `nun-virtual thunk to`.
    // if the classname starts with part of a
    // demangled name starts with
    // these prefixes, we need to remove the prefix
    // to get the real class name

    static std::vector<std::string> thunkPrefixes = {VThunkFuncLabel,
                                                     NVThunkFunLabel
                                                    };
    for (unsigned i = 0; i < thunkPrefixes.size(); i++)
    {
        auto prefix = thunkPrefixes[i];
        if (dname.className.size() > prefix.size() &&
                dname.className.compare(0, prefix.size(), prefix) == 0)
        {
            dname.className = dname.className.substr(prefix.size());
            dname.isThunkFunc = true;
            return;
        }
    }
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

struct cppUtil::DemangledName cppUtil::demangle(const std::string& name)
{
    struct cppUtil::DemangledName dname;
    dname.isThunkFunc = false;

    s32_t status;
    char* realname = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
    if (realname == nullptr)
    {
        dname.className = "";
        dname.funcName = "";
    }
    else
    {
        std::string realnameStr = std::string(realname);
        std::string beforeParenthesis = getBeforeParenthesis(realnameStr);
        if (beforeParenthesis.find("::") == std::string::npos ||
                isOperOverload(beforeParenthesis))
        {
            dname.className = "";
            dname.funcName = "";
        }
        else
        {
            std::string beforeBracket = getBeforeBrackets(beforeParenthesis);
            size_t colon = beforeBracket.rfind("::");
            if (colon == std::string::npos)
            {
                dname.className = "";
                dname.funcName = "";
            }
            else
            {
                dname.className = beforeParenthesis.substr(0, colon);
                dname.funcName = beforeParenthesis.substr(colon + 2);
            }
        }
        std::free(realname);
    }

    handleThunkFunction(dname);

    return dname;
}
std::string cppUtil::getClassNameFromVtblObj(const std::string& vtblName)
{
    std::string className = "";

    s32_t status;
    char* realname = abi::__cxa_demangle(vtblName.c_str(), 0, 0, &status);
    if (realname != nullptr)
    {
        std::string realnameStr = std::string(realname);
        if (realnameStr.compare(0, vtblLabelAfterDemangle.size(),
                                vtblLabelAfterDemangle) == 0)
        {
            className = realnameStr.substr(vtblLabelAfterDemangle.size());
        }
        std::free(realname);
    }
    return className;
}

const ConstantStruct *cppUtil::getVtblStruct(const GlobalValue *vtbl)
{
    const ConstantStruct *vtblStruct = SVFUtil::dyn_cast<ConstantStruct>(vtbl->getOperand(0));
    assert(vtblStruct && "Initializer of a vtable not a struct?");

    if (vtblStruct->getNumOperands() == 2 &&
            SVFUtil::isa<ConstantStruct>(vtblStruct->getOperand(0)) &&
            vtblStruct->getOperand(1)->getType()->isArrayTy())
        return SVFUtil::cast<ConstantStruct>(vtblStruct->getOperand(0));

    return vtblStruct;
}

bool cppUtil::isValVtbl(const Value* val)
{
    if (!SVFUtil::isa<GlobalVariable>(val))
        return false;
    std::string valName = val->getName().str();
    return valName.compare(0, vtblLabelBeforeDemangle.size(),
                           vtblLabelBeforeDemangle) == 0;
}

/*
 * a virtual callsite follows the following instruction sequence pattern:
 * %vtable = load this
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (this)
 */
bool cppUtil::isVirtualCallSite(const CallBase* cs)
{
    // the callsite must be an indirect one with at least one argument (this
    // ptr)
    if (cs->getCalledFunction() != nullptr || cs->arg_empty())
        return false;

    // the first argument (this pointer) must be a pointer type and must be a
    // class name
    if (cs->getArgOperand(0)->getType()->isPointerTy() == false)
        return false;

    const Value* vfunc = cs->getCalledOperand();
    if (const LoadInst* vfuncloadinst = SVFUtil::dyn_cast<LoadInst>(vfunc))
    {
        const Value* vfuncptr = vfuncloadinst->getPointerOperand();
        if (const GetElementPtrInst* vfuncptrgepinst =
                    SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr))
        {
            if (vfuncptrgepinst->getNumIndices() != 1)
                return false;
            const Value* vtbl = vfuncptrgepinst->getPointerOperand();
            if (SVFUtil::isa<LoadInst>(vtbl))
            {
                return true;
            }
        }
    }
    return false;
}

bool cppUtil::isCPPThunkFunction(const Function* F)
{
    cppUtil::DemangledName dname = cppUtil::demangle(F->getName().str());
    return dname.isThunkFunc;
}

const Function* cppUtil::getThunkTarget(const Function* F)
{
    const Function* ret = nullptr;

    for (auto& bb : *F)
    {
        for (auto& inst : bb)
        {
            if (const CallBase* callbase = SVFUtil::dyn_cast<CallBase>(&inst))
            {
                // assert(cs.getCalledFunction() &&
                //        "Indirect call detected in thunk func");
                // assert(ret == nullptr && "multiple callsites in thunk func");

                ret = callbase->getCalledFunction();
            }
        }
    }

    return ret;
}

const Value* cppUtil::getVCallThisPtr(const CallBase* cs)
{
    if (cs->paramHasAttr(0, llvm::Attribute::StructRet))
    {
        return cs->getArgOperand(1);
    }
    else
    {
        return cs->getArgOperand(0);
    }
}

/*!
 * Given a inheritance relation B is a child of A
 * We assume B::B(thisPtr1){ A::A(thisPtr2) } such that thisPtr1 == thisPtr2
 * In the following code thisPtr1 is "%class.B1* %this" and thisPtr2 is
 * "%class.A* %0".
 *
 *
 * define linkonce_odr dso_local void @B1::B1()(%class.B1* %this) unnamed_addr #6 comdat
 *   %this.addr = alloca %class.B1*, align 8
 *   store %class.B1* %this, %class.B1** %this.addr, align 8
 *   %this1 = load %class.B1*, %class.B1** %this.addr, align 8
 *   %0 = bitcast %class.B1* %this1 to %class.A*
 *   call void @A::A()(%class.A* %0)
 */
bool cppUtil::isSameThisPtrInConstructor(const Argument* thisPtr1,
        const Value* thisPtr2)
{
    if (thisPtr1 == thisPtr2)
        return true;
    for (const Value* thisU : thisPtr1->users())
    {
        if (const StoreInst* store = SVFUtil::dyn_cast<StoreInst>(thisU))
        {
            for (const Value* storeU : store->getPointerOperand()->users())
            {
                if (const LoadInst* load = SVFUtil::dyn_cast<LoadInst>(storeU))
                {
                    if (load->getNextNode() &&
                            SVFUtil::isa<CastInst>(load->getNextNode()))
                        return SVFUtil::cast<CastInst>(load->getNextNode()) ==
                               (thisPtr2->stripPointerCasts());
                }
            }
        }
    }
    return false;
}

const Argument* cppUtil::getConstructorThisPtr(const Function* fun)
{
    assert((cppUtil::isConstructor(fun) || cppUtil::isDestructor(fun)) &&
           "not a constructor?");
    assert(fun->arg_size() >= 1 && "argument size >= 1?");
    const Argument* thisPtr = &*(fun->arg_begin());
    return thisPtr;
}

bool cppUtil::isConstructor(const Function* F)
{
    if (F->isDeclaration())
        return false;
    std::string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0)
    {
        return false;
    }
    struct cppUtil::DemangledName dname = cppUtil::demangle(funcName.c_str());
    if (dname.className.size() == 0)
    {
        return false;
    }
    dname.funcName = cppUtil::getBeforeBrackets(dname.funcName);
    dname.className = cppUtil::getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == std::string::npos)
    {
        dname.className = cppUtil::getBeforeBrackets(dname.className);
    }
    else
    {
        dname.className =
            cppUtil::getBeforeBrackets(dname.className.substr(colon + 2));
    }
    /// TODO: on mac os function name is an empty string after demangling
    return dname.className.size() > 0 &&
           dname.className.compare(dname.funcName) == 0;
}

bool cppUtil::isDestructor(const Function* F)
{
    if (F->isDeclaration())
        return false;
    std::string funcName = F->getName().str();
    if (funcName.compare(0, vfunPreLabel.size(), vfunPreLabel) != 0)
    {
        return false;
    }
    struct cppUtil::DemangledName dname = cppUtil::demangle(funcName.c_str());
    if (dname.className.size() == 0)
    {
        return false;
    }
    dname.funcName = cppUtil::getBeforeBrackets(dname.funcName);
    dname.className = cppUtil::getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == std::string::npos)
    {
        dname.className = cppUtil::getBeforeBrackets(dname.className);
    }
    else
    {
        dname.className =
            cppUtil::getBeforeBrackets(dname.className.substr(colon + 2));
    }
    return (dname.className.size() > 0 && dname.funcName.size() > 0 &&
            dname.className.size() + 1 == dname.funcName.size() &&
            dname.funcName.compare(0, 1, "~") == 0 &&
            dname.className.compare(dname.funcName.substr(1)) == 0);
}

/*
 * get the ptr "vtable" for a given virtual callsite:
 * %vtable = load ...
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (...)
 */
const Value* cppUtil::getVCallVtblPtr(const CallBase* cs)
{
    const LoadInst* loadInst =
        SVFUtil::dyn_cast<LoadInst>(cs->getCalledOperand());
    assert(loadInst != nullptr);
    const Value* vfuncptr = loadInst->getPointerOperand();
    const GetElementPtrInst* gepInst =
        SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr);
    assert(gepInst != nullptr);
    const Value* vtbl = gepInst->getPointerOperand();
    return vtbl;
}

/*
 * Is this virtual call inside its own constructor or destructor?
 */
bool cppUtil::VCallInCtorOrDtor(const CallBase* cs)
{
    std::string classNameOfThisPtr = cppUtil::getClassNameOfThisPtr(cs);
    const Function* func = cs->getCaller();
    if (cppUtil::isConstructor(func) || cppUtil::isDestructor(func))
    {
        cppUtil::DemangledName dname = cppUtil::demangle(func->getName().str());
        if (classNameOfThisPtr.compare(dname.className) == 0)
            return true;
    }
    return false;
}

bool cppUtil::classTyHasVTable(const StructType* ty)
{
    if(getClassNameFromType(ty).empty()==false)
    {
        for(auto it = ty->element_begin(); it!=ty->element_end(); it++)
        {
            const std::string& str = LLVMUtil::dumpType(*it);
            if (str.find(vtableType) != std::string::npos)
                return true;
        }
    }
    return false;
}

std::string cppUtil::getClassNameFromType(const StructType* ty)
{
    std::string className = "";
    if (!((SVFUtil::cast<StructType>(ty))->isLiteral()))
    {
        std::string elemTypeName = ty->getStructName().str();
        if (elemTypeName.compare(0, clsName.size(), clsName) == 0)
        {
            className = elemTypeName.substr(clsName.size());
        }
        else if (elemTypeName.compare(0, structName.size(), structName) == 0)
        {
            className = elemTypeName.substr(structName.size());
        }
    }
    return className;
}

std::string cppUtil::getClassNameOfThisPtr(const CallBase* inst)
{
    std::string thisPtrClassName = "";
    if (const MDNode* N = inst->getMetadata("VCallPtrType"))
    {
        const MDString* mdstr = SVFUtil::cast<MDString>(N->getOperand(0).get());
        thisPtrClassName = mdstr->getString().str();
    }
    if (thisPtrClassName.size() == 0)
    {
        const Value* thisPtr = getVCallThisPtr(inst);
        if (const PointerType *ptrTy = SVFUtil::dyn_cast<PointerType>(thisPtr->getType()))
        {
            // TODO: getPtrElementType need type inference
            if (const StructType *st = SVFUtil::dyn_cast<StructType>(LLVMUtil::getPtrElementType(ptrTy)))
            {
                thisPtrClassName = getClassNameFromType(st);
            }
        }
    }

    size_t found = thisPtrClassName.find_last_not_of("0123456789");
    if (found != std::string::npos)
    {
        if (found != thisPtrClassName.size() - 1 &&
                thisPtrClassName[found] == '.')
        {
            return thisPtrClassName.substr(0, found);
        }
    }

    return thisPtrClassName;
}

std::string cppUtil::getFunNameOfVCallSite(const CallBase* inst)
{
    std::string funName;
    if (const MDNode* N = inst->getMetadata("VCallFunName"))
    {
        const MDString* mdstr = SVFUtil::cast<MDString>(N->getOperand(0).get());
        funName = mdstr->getString().str();
    }
    return funName;
}

s32_t cppUtil::getVCallIdx(const CallBase* cs)
{
    const LoadInst* vfuncloadinst =
        SVFUtil::dyn_cast<LoadInst>(cs->getCalledOperand());
    assert(vfuncloadinst != nullptr);
    const Value* vfuncptr = vfuncloadinst->getPointerOperand();
    const GetElementPtrInst* vfuncptrgepinst =
        SVFUtil::dyn_cast<GetElementPtrInst>(vfuncptr);
    User::const_op_iterator oi = vfuncptrgepinst->idx_begin();
    const ConstantInt* idx = SVFUtil::dyn_cast<ConstantInt>(oi->get());
    s32_t idx_value;
    if (idx == nullptr)
    {
        SVFUtil::errs() << "vcall gep idx not constantint\n";
        idx_value = 0;
    }
    else
    {
        idx_value = (s32_t)idx->getSExtValue();
    }
    return idx_value;
}

/*!
 * Check whether this value points-to a constant object
 */
bool LLVMUtil::isConstantObjSym(const Value* val)
{
    if (const GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (cppUtil::isValVtbl(v))
            return false;
        else if (!v->hasInitializer())
        {
            return !v->isExternalLinkage(v->getLinkage());
        }
        else
        {
            StInfo *stInfo = LLVMModuleSet::getLLVMModuleSet()->getSVFType(v->getInitializer()->getType())->getTypeInfo();
            const std::vector<const SVFType*> &fields = stInfo->getFlattenFieldTypes();
            for (std::vector<const SVFType*>::const_iterator it = fields.begin(), eit = fields.end(); it != eit; ++it)
            {
                const SVFType* elemTy = *it;
                assert(!SVFUtil::isa<SVFFunctionType>(elemTy) && "Initializer of a global is a function?");
                if (SVFUtil::isa<SVFPointerType>(elemTy))
                    return false;
            }

            return v->isConstant();
        }
    }
    return LLVMUtil::isConstDataOrAggData(val);
}
