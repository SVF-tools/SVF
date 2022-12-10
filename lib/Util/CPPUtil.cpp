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
#include "Util/SVFUtil.h"

#include <cxxabi.h> // for demangling

using namespace std;
using namespace SVF;

// label for global vtbl value before demangle
const string vtblLabelAfterDemangle = "vtable for ";

// label for multi inheritance virtual function
const string NVThunkFunLabel = "non-virtual thunk to ";
const string VThunkFuncLabel = "virtual thunk to ";

static bool isOperOverload(const string& name)
{
    u32_t leftnum = 0, rightnum = 0;
    string subname = name;
    size_t leftpos, rightpos;
    leftpos = subname.find("<");
    while (leftpos != string::npos)
    {
        subname = subname.substr(leftpos + 1);
        leftpos = subname.find("<");
        leftnum++;
    }
    subname = name;
    rightpos = subname.find(">");
    while (rightpos != string::npos)
    {
        subname = subname.substr(rightpos + 1);
        rightpos = subname.find(">");
        rightnum++;
    }
    return leftnum != rightnum;
}

static string getBeforeParenthesis(const string& name)
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

string cppUtil::getBeforeBrackets(const string& name)
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

    static vector<string> thunkPrefixes = {VThunkFuncLabel, NVThunkFunLabel};
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

struct cppUtil::DemangledName cppUtil::demangle(const string& name)
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
        string realnameStr = string(realname);
        string beforeParenthesis = getBeforeParenthesis(realnameStr);
        if (beforeParenthesis.find("::") == string::npos ||
            isOperOverload(beforeParenthesis))
        {
            dname.className = "";
            dname.funcName = "";
        }
        else
        {
            string beforeBracket = getBeforeBrackets(beforeParenthesis);
            size_t colon = beforeBracket.rfind("::");
            if (colon == string::npos)
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

string cppUtil::getClassNameFromVtblObj(const std::string& vtblName)
{
    string className = "";

    s32_t status;
    char* realname = abi::__cxa_demangle(vtblName.c_str(), 0, 0, &status);
    if (realname != nullptr)
    {
        string realnameStr = string(realname);
        if (realnameStr.compare(0, vtblLabelAfterDemangle.size(),
                                vtblLabelAfterDemangle) == 0)
        {
            className = realnameStr.substr(vtblLabelAfterDemangle.size());
        }
        std::free(realname);
    }
    return className;
}