//===- ExtAPI.h -- External functions -----------------------------------------//
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
 * ExtAPI.h
 *
 *  Created on: July 1, 2022
 *      Author: Shuangxiang Kan
 */

#ifndef __ExtAPI_H
#define __ExtAPI_H

#include "SVFIR/SVFValue.h"
#include <Util/cJSON.h>
#include <Util/config.h>
#include <string>
#include <map>

#define EXTAPI_JSON_PATH "svf/include/Util/ExtAPI.json"
#define JSON_OPT_OVERWRITE "overwrite_app_function"
#define JSON_OPT_FUNCTIONTYPE "type"

namespace SVF
{

/*

** Specifications in ExtAPI.json

****  Overview of the Specification Language
The specification language of external functions is based on the JSON format. And every function defined by the Specification Language is an object that represents the specification rules. These Specification Language objects for functions contain four parts:
1. the signature of the function, (Mandatory)
2. the type of the function, (Mandatory)
3. the switch that controls whether the specification rules defined in the ExtAPI.json overwrite the functions defined in the user code, (Mandatory)
4. the side-effect of the function. (Optional)

*** [1] the signature of the function (Mandatory)
"return": return value type (Only care about whether the return value is a pointer during analysis)
"argument": argument types (Only care about the number of arguments during analysis)


*** [2] the type of the function (Mandatory)
Function type represents the properties of the function.
For example,
"EFT_ALLOC" represents if this external function allocates a new object and assigns it to one of its arguments,
For the selection of function type and a more detailed explanation, please refer to the definition of enum *extType* in ExtAPI.h.


*** [3] the switch that controls whether the specification rules defined in the ExtAPI.json overwrite the functions defined in the user code (Mandatory)
The switch *overwrite_app_function* controls whether the specification rules defined in the ExtAPI.json overwrite the functions defined in the user code (e.g., CPP files). When the switch *overwrite_app_function* is set to a value of 1, SVF will use the specification rules in ExtAPI.json to conduct the analysis and ignore the user-defined functions in the input CPP/bc files.
overwrite_app_function = 0: Analyze the user-defined functions.
overwrite_app_function = 1: Use specifications in ExtAPI.json to overwrite the user-defined functions.

For example, The following is the code to be analyzed, which has a foo() function,
--------------------------------------------------------
char* foo(char* arg)
{
    return arg;
}

int main()
{
    char* ret = foo("abc");
    return 0;
}
--------------------------------------------------------
function foo() has a definition, but you want SVF not to use this definition when analyzing, and you think foo () should do nothing, Then you can add a new entry in ExtAPI.json, and set *"overwrite_app_function": 1*
 "foo": {
        "return":  "char*",
        "arguments":  "(char*)",
        "type": "EFT_NOOP",
        "overwrite_app_function": 1
 }
When SVF is analyzing foo(), SVF will use the entry you defined in ExtAPI.json, and ignore the actual definition of foo() in the program.

Most of the time, overwrite_app_function is 0, unless you want to redefine a function.


*** [4] the side-effect of the function (Optional, if there is no side-effect of that function)
Function side-effect indicate the relationships between input and output,
mainly between function parameters or between parameters and return values after the execution.
For example,
"CopyStmt": ["Arg2", "Ret"] indicates that after this external function is executed, the value of the 2nd parameter is copied into the return value.

For operators of function operation, there are the following options:
"AddrStmt",
"CopyStmt",
"LoadStmt"
"StoreStmt",
"GepStmt",
"BinaryOPStmt",
"UnaryOPStmt",
"CmpStmt",
"memset_like": the function has similar side-effect to function "void *memset(void *str, int c, size_t n)",
"memcpy_like": the function has similar side-effect to function "void *memcpy(void *dest, const void * src, size_t n)",
"funptr_ops": the function has similar side-effect to function "void *dlsym(void *handle, const char *symbol)",
"Rb_tree_ops: the function has similar side-effect to function "_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_".

For operands of function operation,, there are the following options:
"Arg": represents a parameter,
"Obj": represents a object,
"Ret": represents a return value,
"Dummy": represents a dummy node.

*/

class ExtAPI
{
public:

    // External Function types
    // Assume a call in the form LHS= F(arg0, arg1, arg2, arg3).
    enum extType
    {
        EFT_NOOP = 0,        // no effect on pointers
        EFT_ALLOC,           // returns a ptr to a newly allocated object
        EFT_REALLOC,         // like L_A0 if arg0 is a non-null ptr, else ALLOC
        EFT_FREE,            // free memory arg0 and all pointers passing into free function
        EFT_FREE_MULTILEVEL, // any argument with 2-level pointer passing too a free wrapper function e.g., XFree(void**) which frees memory for void* and void**
        EFT_NOSTRUCT_ALLOC,  // like ALLOC but only allocates non-struct data
        EFT_STAT,            // retval points to an unknown static var X
        EFT_STAT2,           // ret -> X -> Y (X, Y - external static vars)
        EFT_L_A0,            // copies arg0, arg1, or arg2 into LHS
        EFT_L_A1,
        EFT_L_A2,
        EFT_L_A8,
        EFT_L_A0__A0R_A1,  // stores arg1 into *arg0 and returns arg0 (currently only for memset)
        EFT_L_A0__A0R_A1R, // copies the data that arg1 points to into the location
        EFT_L_A1__FunPtr,  // obtain the address of a symbol based on the arg1 (char*) and parse a function to LHS (e.g., void *dlsym(void *handle, char *funname);)
        //  arg0 points to; note that several fields may be
        //  copied at once if both point to structs.
        //  Returns arg0.
        EFT_A1R_A0R,      // copies *arg0 into *arg1, with non-ptr return
        EFT_A3R_A1R_NS,   // copies *arg1 into *arg3 (non-struct copy only)
        EFT_A1R_A0,       // stores arg0 into *arg1
        EFT_A2R_A1,       // stores arg1 into *arg2
        EFT_A4R_A1,       // stores arg1 into *arg4
        EFT_L_A0__A2R_A0, // stores arg0 into *arg2 and returns it
        EFT_L_A0__A1_A0,  // store arg1 into arg0's base and returns arg0
        EFT_A0R_NEW,      // stores a pointer to an allocated object in *arg0
        EFT_A1R_NEW,      // as above, into *arg1, etc.
        EFT_A2R_NEW,
        EFT_A4R_NEW,
        EFT_A11R_NEW,
        EFT_STD_RB_TREE_INSERT_AND_REBALANCE, // Some complex effects
        EFT_STD_RB_TREE_INCREMENT,            // Some complex effects
        EFT_STD_LIST_HOOK,                    // Some complex effects

        CPP_EFT_A0R_A1,       // stores arg1 into *arg0
        CPP_EFT_A0R_A1R,      // copies *arg1 into *arg0
        CPP_EFT_A1R,          // load arg1
        EFT_CXA_BEGIN_CATCH,  //__cxa_begin_catch
        CPP_EFT_DYNAMIC_CAST, // dynamic_cast
        EFT_NULL              // not found in the list
    };

private:

    std::map<std::string, extType> type_pair =
    {
        {"EFT_NOOP", EFT_NOOP},
        {"EFT_ALLOC", EFT_ALLOC},
        {"EFT_REALLOC", EFT_REALLOC},
        {"EFT_FREE", EFT_FREE},
        {"EFT_FREE_MULTILEVEL", EFT_FREE_MULTILEVEL},
        {"EFT_NOSTRUCT_ALLOC", EFT_NOSTRUCT_ALLOC},
        {"EFT_STAT", EFT_STAT},
        {"EFT_STAT2", EFT_STAT2},
        {"EFT_L_A0", EFT_L_A0},
        {"EFT_L_A1", EFT_L_A1},
        {"EFT_L_A2", EFT_L_A2},
        {"EFT_L_A8", EFT_L_A8},
        {"EFT_L_A0__A0R_A1", EFT_L_A0__A0R_A1},
        {"EFT_L_A0__A0R_A1R", EFT_L_A0__A0R_A1R},
        {"EFT_L_A1__FunPtr", EFT_L_A1__FunPtr},
        {"EFT_A1R_A0R", EFT_A1R_A0R},
        {"EFT_A3R_A1R_NS", EFT_A3R_A1R_NS},
        {"EFT_A1R_A0", EFT_A1R_A0},
        {"EFT_A2R_A1", EFT_A2R_A1},
        {"EFT_A4R_A1", EFT_A4R_A1},
        {"EFT_L_A0__A2R_A0", EFT_L_A0__A2R_A0},
        {"EFT_L_A0__A1_A0", EFT_L_A0__A1_A0},
        {"EFT_A0R_NEW", EFT_A0R_NEW},
        {"EFT_A1R_NEW", EFT_A1R_NEW},
        {"EFT_A2R_NEW", EFT_A2R_NEW},
        {"EFT_A4R_NEW", EFT_A4R_NEW},
        {"EFT_A11R_NEW", EFT_A11R_NEW},
        {"EFT_STD_RB_TREE_INSERT_AND_REBALANCE", EFT_STD_RB_TREE_INSERT_AND_REBALANCE},
        {"EFT_STD_RB_TREE_INCREMENT", EFT_STD_RB_TREE_INCREMENT},
        {"EFT_STD_LIST_HOOK", EFT_STD_LIST_HOOK},
        {"CPP_EFT_A0R_A1R", CPP_EFT_A0R_A1R},
        {"CPP_EFT_A1R", CPP_EFT_A1R},
        {"EFT_CXA_BEGIN_CATCH", EFT_CXA_BEGIN_CATCH},
        {"CPP_EFT_DYNAMIC_CAST", CPP_EFT_DYNAMIC_CAST},
        {"", EFT_NULL}
    };

    static ExtAPI *extOp;

    // Store specifications of external functions in ExtAPI.json file
    static cJSON *root;

    ExtAPI() = default;

public:

    enum OperationType
    {
        Addr,
        Copy,
        Load,
        Store,
        Gep,
        Call,
        Return,
        Rb_tree_ops,
        Memcpy_like,
        Memset_like,
        Other
    };

    class Operand
    {
    public:
        OperationType getType() const
        {
            return opType;
        }
        void setType(OperationType type)
        {
            opType = type;
        }
        const std::string &getSrcValue() const
        {
            return src;
        }
        void setSrcValue(const std::string &value)
        {
            src = value;
        }
        const std::string &getDstValue() const
        {
            return dst;
        }
        void setDstValue(const std::string &value)
        {
            dst = value;
        }
        NodeID getSrcID() const
        {
            return srcID;
        }
        void setSrcID(NodeID id)
        {
            srcID = id;
        }
        NodeID getDstID() const
        {
            return dstID;
        }
        void setDstID(NodeID id)
        {
            dstID = id;
        }
        const std::string &getOffsetOrSizeStr() const
        {
            return offsetOrSizeStr;
        }
        void setOffsetOrSizeStr(const std::string &str)
        {
            offsetOrSizeStr = str;
        }
        u32_t getOffsetOrSize() const
        {
            return offsetOrSize;
        }
        void setOffsetOrSize(u32_t off)
        {
            offsetOrSize = off;
        }
    private:
        OperationType opType; // 操作数类型
        std::string src; // 操作数值
        std::string dst; //
        std::string offsetOrSizeStr; // Gep
        NodeID srcID;
        NodeID dstID;
        u32_t offsetOrSize;
    };

    class ExtOperation
    {
    public:
        bool isCallOp() const
        {
            return callOp;
        }
        void setCallOp(bool isCall)
        {
            callOp = isCall;
        }
        bool isConOp() const
        {
            return conOp;
        }
        void setConOp(bool isCon)
        {
            conOp = isCon;
        }
        Operand& getBasicOp()
        {
            return basicOp;
        }
        void setBasicOp(Operand op)
        {
            basicOp = op;
        }
        const std::string &getCondition() const
        {
            return condition;
        }
        void setCondition(const std::string &cond)
        {
            condition = cond;
        }
        std::vector<Operand>& getTrueBranchOperands()
        {
            return trueBranch_operands;
        }
        void setTrueBranchOperands(std::vector<Operand> tbo)
        {
            trueBranch_operands = tbo;
        }
        std::vector<Operand>& getFalseBranchOperands()
        {
            return falseBranch_operands;
        }
        void setFalseBranchOperands(std::vector<Operand> fbo)
        {
            falseBranch_operands = fbo;
        }
        const std::string &getCalleeName() const
        {
            return callee_name;
        }
        void setCalleeName(const std::string &name)
        {
            callee_name = name;
        }
        const std::string &getCalleeReturn() const
        {
            return callee_return;
        }
        void setCalleeReturn(const std::string &ret)
        {
            callee_return = ret;
        }
        const std::string &getCalleeArguments() const
        {
            return callee_arguments;
        }
        void setCalleeArguments(const std::string &args)
        {
            callee_arguments = args;
        }
        std::vector<Operand>& getCalleeOperands()
        {
            return callee_operands;
        }
        void setCalleeOperands(std::vector<Operand> ops)
        {
            callee_operands = ops;
        }
    private:
        bool callOp = false;
        bool conOp = false;
        Operand basicOp;
        std::string condition;
        std::vector<Operand> trueBranch_operands;
        std::vector<Operand> falseBranch_operands;
        std::string callee_name;
        std::string callee_return;
        std::string callee_arguments;
        std::vector<Operand> callee_operands;
    };

    class ExtFunctionOps
    {
    public:
        ExtFunctionOps() {};
        ExtFunctionOps(const std::string &name, std::vector<ExtOperation> ops) : extFunName(name), extOperations(ops) {};
        const std::string &getExtFunName() const
        {
            return extFunName;
        }
        void setExtFunName(const std::string &name)
        {
            extFunName = name;
        }
        std::vector<ExtOperation>& getOperations()
        {
            return extOperations;
        }
        void setOperations(std::vector<ExtOperation> ops)
        {
            extOperations = ops;
        }
        u32_t getCallStmtNum()
        {
            return callStmtNum;
        }
        void setCallStmtNum(u32_t num)
        {
            callStmtNum = num;
        }
    private:
        std::string extFunName;
        std::vector<ExtOperation> extOperations;
        u32_t callStmtNum = 0;
    };

private:
    // Map an external function to its operations
    std::map<std::string, ExtFunctionOps> extFunToOps;

public:

    static ExtAPI *getExtAPI(const std::string& = "");

    static void destory();

    // Add an entry with the specified fields to the ExtAPI, which will be reflected immediately by further ExtAPI queries
    void add_entry(const char* funName, const char* returnType,
                   std::vector<const char*> argTypes, extType type,
                   bool overwrite_app_function);

    // Get numeric index of the argument in external function
    u32_t getArgPos(const std::string& s);

    // return value >= 0 is an argument node
    // return value = -1 is an inst node
    // return value = -2 is a Dummy node
    // return value = -3 is an object node
    // return value = -4 is an offset
    // return value = -5 is an illegal operand format
    s32_t getNodeIDType(const std::string& s);

    // Get the corresponding name in ext_t, e.g. "EXT_ADDR" in {"addr", EXT_ADDR},
    std::string get_opName(const std::string& s);
    // opposite for extType
    const std::string& extType_toString(extType type);

    // Get external function name, e.g "memcpy"
    std::string get_name(const SVFFunction *F);

    // Get arguments of the operation, e.g. ["A1R", "A0", "A2"]
    std::vector<std::string> get_opArgs(const cJSON *value);

    // Get specifications of external functions in ExtAPI.json file
    cJSON *get_FunJson(const std::string &funName);

    // Get all operations of an extern function
    Operand getBasicOperation(cJSON* obj);
    ExtFunctionOps getExtFunctionOps(const SVFFunction* extFunction);

    // Get property of the operation, e.g. "EFT_A1R_A0R"
    extType get_type(const SVF::SVFFunction *callee);
    extType get_type(const std::string& funName);

    // Get priority of he function, return value
    // 0: Apply user-defined functions
    // 1: Apply function specification in ExtAPI.json
    u32_t isOverwrittenAppFunction(const SVF::SVFFunction *callee);
    u32_t isOverwrittenAppFunction(const std::string& funcName);
    // set priority of the function
    void setOverWrittenAppFunction(const std::string& funcName, u32_t overwrite_app_function);

    // Does (F) have a static var X (unavailable to us) that its return points to?
    bool has_static(const SVFFunction *F);

    // Assuming hasStatic(F), does (F) have a second static Y where X -> Y?
    bool has_static2(const SVFFunction *F);

    // Does (F) have a memset_like or memcpy_like operation?
    bool is_memset_or_memcpy(const SVFFunction *F);

    // Does (F) allocate a new object and return it?
    bool is_alloc(const SVFFunction *F);

    // Does (F) allocate a new object and assign it to one of its arguments?
    bool is_arg_alloc(const SVFFunction *F);

    // Get the position of argument which holds the new object
    s32_t get_alloc_arg_pos(const SVFFunction *F);

    // Does (F) allocate only non-struct objects?
    bool no_struct_alloc(const SVFFunction *F);

    // Does (F) not free/release any memory?
    bool is_dealloc(const SVFFunction *F);

    // Does (F) not do anything with the known pointers?
    bool is_noop(const SVFFunction *F);

    // Does (F) reallocate a new object?
    bool is_realloc(const SVFFunction *F);

    // Does (F) have the same return type(pointer or nonpointer) and same number of arguments
    bool is_sameSignature(const SVFFunction *F);

    // Should (F) be considered "external" (either not defined in the program
    //   or a user-defined version of a known alloc or no-op)?
    bool is_ext(const SVFFunction *F);
};
} // End namespace SVF

#endif
