/*  [ExtAPI.h] Info about external llvm::Functions
 *  v. 005, 2008-08-15
 *------------------------------------------------------------------------------
 *  Copyright 2008 Andrey Petrov
 *  - apetrov87@gmail.com, apetrov@cs.utexas.edu
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
 */

#ifndef __ExtAPI_H
#define __ExtAPI_H

#include "Util/BasicTypes.h"
#include "Util/cJSON.h"
#include "Util/config.h"
#include <string>
#include <map>

#define EXTAPI_JSON_PATH "/include/Util/ExtAPI.json"
#define JSON_OPT_OVERWRITE "overwrite_app_function"
#define JSON_OPT_FUNCTIONTYPE "type"

namespace SVF
{

/*

** Specifications in ExtAPI.json

*** [1] Overview of the Specification Language
The specification language of external functions is based on the JSON format. And every function defined by the Specification Language is an object that represents the specification rules. These Specification Language objects for functions contain four parts:
1. the name of the function,
2. the switch that controls whether the specification rules defined in the ExtAPI.json overwrite the functions defined in the user code,
3. the type of the function,
4. the operations conducted by the function.

*** [2] Overwriting the user-defined functions
The switch *overwrite_app_function* controls whether the specification rules defined in the ExtAPI.json overwrite the functions defined in the user code (e.g., CPP files). When the switch *overwrite_app_function* is set to a value of 1, SVF will use the specification rules in ExtAPI.json to conduct the analysis and ignore the user-defined functions in the input CPP/bc files.
overwrite_app_function = 0: Analyze the user-defined functions.
overwrite_app_function = 1: Use specifications in ExtAPI.json to overwrite the user-defined functions.

*** [3] Function types
Function type represents the properties of the function.
For example,
"EFT_ALLOC" represents if this external function allocates a new object and assigns it to one of its arguments,
For the selection of function type and a more detailed explanation, please refer to the definition of enum *extf_t* in ExtAPI.h.

*** [4] Function operations
Function operations indicate the relationships between input and output,
mainly between function parameters or between parameters and return values after the execution.
For example,
"copy": ["A2", "L"] indicates that after this external function is executed, the value of the 2nd parameter is copied into the return value.
For the selection of function type and a more detailed explanation, please refer to the definition of enum *extType* in ExtAPI.h.

For operands of function operation, e.g., "A2", "L", there are the following options:
"A": represents a parameter;
"N": represents a number;
"R": represents a reference;
"L": represents a return value;
"V": represents a dummy node;

Among them, a parameter may have multiple forms because there may be various parameters, and the parameter may be a reference or complex structure,

Here we use regular expressions "(AN)(R|RN)^*" to represent parameters, for example,
"A0": represents the 1st parameter;
"A2R": represents that the 3rd parameter is a reference;
"A1R2": represents the 3rd substructure of the 2nd parameter "A1R", where "A1R" is a complex structure;
"A2R3R": represents the 4th substructure of the 3rd parameter "A2R" is a reference, where "A2R" is a complex structure;


** Specification format:
"functionName": {
"type": "functional type",
"overwrite_app_function:" 0/1,
"function operation_1": [ operand_1, operand_2, ... , operand_n],
"function operation_2": [ operand_1, operand_2, ... , operand_n],
...
"function operation_n": [ operand_1, operand_2, ... , operand_n]
}
*/


class ExtAPI
{
public:
    enum extf_t
    {
        EXT_ADDR = 0,  // Handle addr edge
        EXT_COPY,      // Handle copy edge
        EXT_LOAD,      // Handle load edge
        EXT_STORE,     // Handle store edge
        EXT_LOADSTORE, // Handle load and store edges, and add a dummy node
        EXT_COPY_N,    // Copy the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str
        EXT_COPY_MN,   // Copies n characters from memory area src to memory area dest.
        EXT_FUNPTR,    // Handle function void *dlsym(void *handle, const char *symbol)
        EXT_COMPLEX,   // Handle function _ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_
        EXT_OTHER
    };

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
    std::map<std::string, extf_t> op_pair =
    {
        {"addr", EXT_ADDR},
        {"copy", EXT_COPY},
        {"load", EXT_LOAD},
        {"store", EXT_STORE},
        {"load_store", EXT_LOADSTORE},
        {"copy_n", EXT_COPY_N},
        {"copy_mn", EXT_COPY_MN},
        {"complex", EXT_COMPLEX},
        {"funptr", EXT_FUNPTR}
    };

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
    static ExtAPI *getExtAPI(const std::string & = "");

    static void destory();

    // Get the corresponding name in ext_t, e.g. "EXT_ADDR" in {"addr", EXT_ADDR},
    extf_t get_opName(std::string s);

    // Return the extf_t of (F).
    // Get external function name, e.g "memcpy"
    std::string get_name(const SVFFunction *F);

    // Get arguments of the operation, e.g. ["A1R", "A0", "A2"]
    std::vector<std::string> get_opArgs(const cJSON *value);

    // Get specifications of external functions in ExtAPI.json file
    cJSON *get_FunJson(const std::string &funName);

    // Get property of the operation, e.g. "EFT_A1R_A0R"
    extType get_type(const SVF::SVFFunction *callee);

    // Get priority of he function, return value
    // 0: Apply user-defined functions
    // 1: Apply function specification in ExtAPI.json
    u32_t isOverwrittenAppFunction(const SVF::SVFFunction *callee);

    // Does (F) have a static var X (unavailable to us) that its return points to?
    bool has_static(const SVFFunction *F);

    // Assuming hasStatic(F), does (F) have a second static Y where X -> Y?
    bool has_static2(const SVFFunction *F);

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

    // Should (F) be considered "external" (either not defined in the program
    //   or a user-defined version of a known alloc or no-op)?
    bool is_ext(const SVFFunction *F);
};
} // End namespace SVF

#endif