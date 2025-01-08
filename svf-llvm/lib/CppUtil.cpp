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
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/Casting.h"
#include "Util/SVFUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/ObjTypeInference.h"

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

const std::string znwm = "_Znwm";
const std::string zn1Label = "_ZN1"; // c++ constructor
const std::string znstLabel = "_ZNSt";
const std::string znst5Label = "_ZNSt5"; // _ZNSt5dequeIPK1ASaIS2_EE5frontEv -> std::deque<A const*, std::allocator<A const*> >::front()
const std::string znst12Label = "_ZNSt12"; // _ZNSt12forward_listIPK1ASaIS2_EEC2Ev -> std::forward_list<A const*, std::allocator<A const*> >::forward_list()
const std::string znst6Label = "_ZNSt6"; // _ZNSt6vectorIP1ASaIS1_EEC2Ev -> std::vector<A*, std::allocator<A*> >::vector()
const std::string znst7Label = "_ZNSt7"; // _ZNSt7__cxx114listIPK1ASaIS3_EEC2Ev -> std::__cxx11::list<A const*, std::allocator<A const*> >::list()
const std::string znst14Label = "_ZNSt14"; // _ZNSt14_Fwd_list_baseI1ASaIS0_EEC2Ev -> std::_Fwd_list_base<A, std::allocator<A> >::_Fwd_list_base()


const std::string znkstLabel = "_ZNKSt";
const std::string znkst5Label = "_ZNKSt15_"; // _ZNKSt15_Deque_iteratorIPK1ARS2_PS2_EdeEv -> std::_Deque_iterator<A const*, A const*&, A const**>::operator*() const
const std::string znkst20Label = "_ZNKSt20_"; // _ZNKSt20_List_const_iteratorIPK1AEdeEv -> std::_List_const_iterator<A const*>::operator*() const

const std::string znkst23Label = "_ZNKSt23_"; // _ZNKSt23_Rb_tree_const_iteratorISt4pairIKi1AEEptEv -> std::_List_const_iterator<A const*>::operator*() const


const std::string znkLabel = "_ZNK";
const std::string znk9Label = "_ZNK9"; // _ZNK9__gnu_cxx17__normal_iteratorIPK1ASt6vectorIS1_SaIS1_EEEdeEv -> __gnu_cxx::__normal_iterator<A const*, std::vector<A, std::allocator<A> > >::operator*() const

const std::string ztilabel = "_ZTI";
const std::string ztiprefix = "typeinfo for ";
const std::string dyncast = "__dynamic_cast";


static bool isOperOverload(const std::string& name)
{
    u32_t leftnum = 0, rightnum = 0;
    std::string subname = name;
    size_t leftpos, rightpos;
    leftpos = subname.find('<');
    while (leftpos != std::string::npos)
    {
        subname = subname.substr(leftpos + 1);
        leftpos = subname.find('<');
        leftnum++;
    }
    subname = name;
    rightpos = subname.find('>');
    while (rightpos != std::string::npos)
    {
        subname = subname.substr(rightpos + 1);
        rightpos = subname.find('>');
        rightnum++;
    }
    return leftnum != rightnum;
}

static std::string getBeforeParenthesis(const std::string& name)
{
    size_t lastRightParen = name.rfind(')');
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

/// get class name before brackets
/// e.g., for `namespace::A<...::...>::f', we get `namespace::A'
std::string cppUtil::getBeforeBrackets(const std::string& name)
{
    if (name.empty() || name[name.size() - 1] != '>')
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

// Extract class name in parameters
// e.g., given "WithSemaphore::WithSemaphore(AP_HAL::Semaphore&)", return "AP_HAL::Semaphore"
Set<std::string> cppUtil::getClsNamesInBrackets(const std::string& name)
{
    Set<std::string> res;
    // Lambda to trim whitespace from both ends of a string
    auto trim = [](std::string& s)
    {
        size_t first = s.find_first_not_of(' ');
        size_t last = s.find_last_not_of(' ');
        if (first != std::string::npos && last != std::string::npos)
        {
            s = s.substr(first, (last - first + 1));
        }
        else
        {
            s.clear();
        }
    };

    // Lambda to remove trailing '*' and '&' characters
    auto removePointerAndReference = [](std::string& s)
    {
        while (!s.empty() && (s.back() == '*' || s.back() == '&'))
        {
            s.pop_back();
        }
    };

    s32_t status;
    char* realname = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
    if (realname == nullptr)
    {
        // do nothing
    }
    else
    {
        std::string realnameStr = std::string(realname);

        // Find the start and end of the parameter list
        size_t start = realnameStr.find('(');
        size_t end = realnameStr.find(')');
        if (start == std::string::npos || end == std::string::npos || start >= end)
        {
            return res; // Return empty set if the format is incorrect
        }

        // Extract the parameter list
        std::string paramList = realnameStr.substr(start + 1, end - start - 1);

        // Split the parameter list by commas
        std::istringstream ss(paramList);
        std::string param;
        while (std::getline(ss, param, ','))
        {
            trim(param);
            removePointerAndReference(param);
            res.insert(param);
        }
        std::free(realname);
    }
    return res;
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
    assert((isConstructor(fun) || isDestructor(fun)) &&
           "not a constructor?");
    assert(fun->arg_size() >= 1 && "argument size >= 1?");
    const Argument* thisPtr = &*(fun->arg_begin());
    return thisPtr;
}

/// strip off brackets and namespace from classname
/// e.g., for `namespace::A<...::...>::f', we get `A' by stripping off namespace and <>
void stripBracketsAndNamespace(cppUtil::DemangledName& dname)
{
    dname.funcName = cppUtil::getBeforeBrackets(dname.funcName);
    dname.className = cppUtil::getBeforeBrackets(dname.className);
    size_t colon = dname.className.rfind("::");
    if (colon == std::string::npos)
    {
        dname.className = cppUtil::getBeforeBrackets(dname.className);
    }
    else
    {
        // strip off namespace
        dname.className =
            cppUtil::getBeforeBrackets(dname.className.substr(colon + 2));
    }
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
    stripBracketsAndNamespace(dname);
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
    stripBracketsAndNamespace(dname);
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
    Set<std::string> classNameOfThisPtrs = cppUtil::getClassNameOfThisPtr(cs);
    const Function* func = cs->getCaller();
    for (const auto &classNameOfThisPtr: classNameOfThisPtrs)
    {
        if (cppUtil::isConstructor(func) || cppUtil::isDestructor(func))
        {
            cppUtil::DemangledName dname = cppUtil::demangle(func->getName().str());
            if (classNameOfThisPtr.compare(dname.className) == 0)
                return true;
        }
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

Set<std::string> cppUtil::getClassNameOfThisPtr(const CallBase* inst)
{
    Set<std::string> thisPtrNames;
    std::string thisPtrClassName = "";
    if (const MDNode* N = inst->getMetadata("VCallPtrType"))
    {
        const MDString* mdstr = SVFUtil::cast<MDString>(N->getOperand(0).get());
        thisPtrClassName = mdstr->getString().str();
    }
    if (thisPtrClassName.size() == 0)
    {
        const Value* thisPtr = getVCallThisPtr(inst);
        Set<std::string>& names = LLVMModuleSet::getLLVMModuleSet()->getTypeInference()->inferThisPtrClsName(thisPtr);
        thisPtrNames.insert(names.begin(), names.end());
    }

    Set<std::string> ans;
    std::transform(thisPtrNames.begin(), thisPtrNames.end(), std::inserter(ans, ans.begin()),
                   [](const std::string &thisPtrName) -> std::string
    {
        size_t found = thisPtrName.find_last_not_of("0123456789");
        if (found != std::string::npos)
        {
            if (found != thisPtrName.size() - 1 &&
                    thisPtrName[found] == '.')
            {
                return thisPtrName.substr(0, found);
            }
        }
        return thisPtrName;
    });
    return ans;
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
        idx_value = LLVMUtil::getIntegerValue(idx).first;
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

/*!
 * extract class name from the c++ function name, e.g., constructor/destructors
 *
 * @param foo
 * @return
 */
Set<std::string> cppUtil::extractClsNamesFromFunc(const Function *foo)
{
    const std::string &name = foo->getName().str();
    if (isConstructor(foo) || isDestructor(foo))
    {
        // c++ constructor or destructor
        DemangledName demangledName = cppUtil::demangle(name);
        Set<std::string> clsNameInBrackets =
            cppUtil::getClsNamesInBrackets(name);
        clsNameInBrackets.insert(demangledName.className);
        return clsNameInBrackets;
    }
    else if (isTemplateFunc(foo))
    {
        // array index
        Set<std::string> classNames = extractClsNamesFromTemplate(name);
        assert(!classNames.empty() && "empty class names?");
        return classNames;
    }
    return {};
}

/*!
 * find the innermost brackets,
 * e.g., return "int const, A" for  "__gnu_cxx::__aligned_membuf<std::pair<int const, A> >::_M_ptr() const"
 * @param input
 * @return
 */
std::vector<std::string> findInnermostBrackets(const std::string &input)
{
    typedef std::pair<u32_t, u32_t> StEdIdxPair;
    std::stack<int> stack;
    std::vector<StEdIdxPair> innerMostPairs;
    std::vector<bool> used(input.length(), false);

    for (u32_t i = 0; i < input.length(); ++i)
    {
        if (input[i] == '<')
        {
            stack.push(i);
        }
        else if (input[i] == '>' && i > 0 && input[i - 1] != '-')
        {
            if (!stack.empty())
            {
                int openIndex = stack.top();
                stack.pop();

                // Check if this pair is innermost
                bool isInnermost = true;
                for (u32_t j = openIndex + 1; j < i && isInnermost; ++j)
                {
                    if (used[j])
                    {
                        isInnermost = false;
                    }
                }

                if (isInnermost)
                {
                    innerMostPairs.emplace_back(openIndex, i);
                    used[openIndex] = used[i] = true; // Mark these indices as used
                }
            }
        }
    }
    std::vector<std::string> ans(innerMostPairs.size());
    std::transform(innerMostPairs.begin(), innerMostPairs.end(), ans.begin(), [&input](StEdIdxPair &p) -> std::string
    {
        return input.substr(p.first + 1, p.second - p.first - 1);
    });
    return ans;
}

/*!
 * strip off the whitespaces from the beginning and ending of str
 * @param str
 * @return
 */
std::string stripWhitespaces(const std::string &str)
{
    auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    });
    auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }).base();

    return (start < end) ? std::string(start, end) : std::string();
}

std::vector<std::string> splitAndStrip(const std::string &input, char delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;

    while ((end = input.find(delimiter, start)) != std::string::npos)
    {
        tokens.push_back(stripWhitespaces(input.substr(start, end - start)));
        start = end + 1;
    }

    tokens.push_back(stripWhitespaces(input.substr(start)));

    return tokens;
}

/*!
 * extract class names from template functions
 * @param oname
 * @return
 */
Set<std::string> cppUtil::extractClsNamesFromTemplate(const std::string &oname)
{
    // "std::array<A const*, 2ul>" -> A
    // "std::queue<A*, std::deque<A*, std::allocator<A*> > >" -> A
    // __gnu_cxx::__aligned_membuf<std::pair<int const, A> >::_M_ptr() const -> A
    Set<std::string> ans;
    std::string demangleName = llvm::demangle(oname);
    std::vector<std::string> innermosts = findInnermostBrackets(demangleName);
    for (const auto &innermost: innermosts)
    {
        const std::vector<std::string> &allstrs = splitAndStrip(innermost, ',');
        for (const auto &str: allstrs)
        {
            size_t spacePos = str.find(' ');
            if (spacePos != std::string::npos)
            {
                // A const* -> A
                ans.insert(str.substr(0, spacePos));
            }
            else
            {
                size_t starPos = str.find('*');
                if (starPos != std::string::npos)
                    // A* -> A
                    ans.insert(str.substr(0, starPos));
                else
                    ans.insert(str);
            }
        }
    }
    return ans;
}


/*!
 * class sources are functions
 * where we can extract the class name (constructors/destructors or template functions)
 * @param val
 * @return
 */
bool cppUtil::isClsNameSource(const Value *val)
{
    if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(val))
    {
        const Function *foo = callBase->getCalledFunction();
        // indirect call
        if(!foo) return false;
        return isConstructor(foo) || isDestructor(foo) || isTemplateFunc(foo) || isDynCast(foo);
    }
    else if (const auto *func = SVFUtil::dyn_cast<Function>(val))
    {
        return isConstructor(func) || isDestructor(func) || isTemplateFunc(func);
    }
    return false;
}

/*!
 * whether fooName matches the mangler label
 * @param foo
 * @param label
 * @return
 */
bool cppUtil::matchesLabel(const std::string &foo, const std::string &label)
{
    return foo.compare(0, label.size(), label) == 0;
}

/*!
 * whether foo is a cpp template function
 * TODO: we only consider limited label for now (see the very beginning of CppUtil.cpp)
 * @param foo
 * @return
 */
bool cppUtil::isTemplateFunc(const Function *foo)
{
    const std::string &name = foo->getName().str();
    bool matchedLabel = matchesLabel(name, znstLabel) || matchesLabel(name, znkstLabel) ||
                        matchesLabel(name, znkLabel);
    // we exclude "_ZNK6cArray3dupEv" -> cArray::dup() const
    const std::string &demangledName = llvm::demangle(name);
    return matchedLabel && demangledName.find('<') != std::string::npos && demangledName.find('>') != std::string::npos;
}

/*!
 * whether foo is a cpp dyncast function
 * @param foo
 * @return
 */
bool cppUtil::isDynCast(const Function *foo)
{
    return foo->getName().str() == dyncast;
}

/*!
 * extract class name from cpp dyncast function
 * @param callBase
 * @return
 */
std::string cppUtil::extractClsNameFromDynCast(const CallBase* callBase)
{
    Value *tgtCast = callBase->getArgOperand(2);
    const std::string &valueStr = LLVMUtil::dumpValue(tgtCast);
    u32_t leftPos = valueStr.find(ztilabel);
    assert(leftPos != (u32_t) std::string::npos && "does not find ZTI for dyncast?");
    u32_t rightPos = leftPos;
    while (rightPos < valueStr.size() && valueStr[rightPos] != ' ') rightPos++;
    const std::string &substr = valueStr.substr(leftPos, rightPos - leftPos);
    std::string demangleName = llvm::demangle(substr);
    const std::string &realName = demangleName.substr(ztiprefix.size(),
                                  demangleName.size() - ztiprefix.size());
    assert(realName != "" && "real name for dyncast empty?");
    return realName;
}

const Type *cppUtil::cppClsNameToType(const std::string &className)
{
    StructType *classTy = StructType::getTypeByName(LLVMModuleSet::getLLVMModuleSet()->getContext(),
                          clsName + className);
    return classTy ? classTy : LLVMModuleSet::getLLVMModuleSet()->getTypeInference()->ptrType();
}