//===- ExtAPI.cpp -- External functions -----------------------------------------//
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
 * ExtAPI.cpp
 *
 *  Created on: July 1, 2022
 *      Author: Shuangxiang Kan
 */

#include "Util/ExtAPI.h"
#include "SVFIR/SVFVariables.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

using namespace SVF;

ExtAPI* ExtAPI::extOp = nullptr;
cJSON* ExtAPI::root = nullptr;

// Get environment variables $SVF_DIR and "npm root" through popen() method
static std::string GetStdoutFromCommand(const std::string& command)
{
    char buffer[128];
    std::string result = "";
    // Open pipe to file
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        return "popen failed!";
    }
    // read till end of process:
    while (!feof(pipe))
    {
        // use buffer to read and add to result
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    // remove "\n"
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

// Get ExtAPI.json file
static std::string getJsonFile(const std::string& path)
{
    std::string jsonFilePath = GetStdoutFromCommand(path);
    if (path.compare("npm root") == 0)
    {
        int os_flag = 1;
        // SVF installed via npm needs to determine the type of operating
        // system, otherwise the ExtAPI.json path may not be found.
#ifdef linux
        // Linux os
        os_flag = 0;
        jsonFilePath.append("/svf-lib/SVF-linux");
#endif
        // Mac os
        if (os_flag == 1)
        {
            jsonFilePath.append("/svf-lib/SVF-osx");
        }
    }
    if (jsonFilePath.back() != '/') jsonFilePath.push_back('/');
    jsonFilePath.append(EXTAPI_JSON_PATH);
    return jsonFilePath;
}

static cJSON* parseJson(const std::string& path, off_t fileSize)
{
    FILE* file = fopen(path.c_str(), "r");
    if (!file)
    {
        return nullptr;
    }

    // allocate memory size matched with file size
    char* jsonStr = (char*)calloc(fileSize + 1, sizeof(char));

    // read json string from file
    u32_t size = fread(jsonStr, sizeof(char), fileSize, file);
    if (size == 0)
    {
        SVFUtil::errs() << SVFUtil::errMsg("\t Wrong ExtAPI.json path!! ")
                        << "The current ExtAPI.json path is: " << path << "\n";
        assert(false && "Read ExtAPI.json file fails!");
        return nullptr;
    }
    fclose(file);

    // convert json string to json pointer variable
    cJSON* root = cJSON_Parse(jsonStr);
    if (!root)
    {
        free(jsonStr);
        return nullptr;
    }
    free(jsonStr);
    return root;
}

ExtAPI* ExtAPI::getExtAPI(const std::string& path)
{
    if (extOp == nullptr)
    {
        extOp = new ExtAPI;
    }
    if (root == nullptr)
    {
        struct stat statbuf;

        // Four ways to get ExtAPI.json path
        // 1. Explicit path provided
        // 2. default path (get ExtAPI.json path from Util/config.h)
        // 3. from $SVF_DIR
        // 4. from "npm root"(If SVF is installed via npm)

        std::string jsonFilePath = path;
        if (!jsonFilePath.empty() && !stat(jsonFilePath.c_str(), &statbuf))
        {
            root = parseJson(jsonFilePath, statbuf.st_size);
            return extOp;
        }

        jsonFilePath = std::string(PROJECT_PATH) + '/' + EXTAPI_JSON_PATH;
        if (!stat(jsonFilePath.c_str(), &statbuf))
        {
            root = parseJson(jsonFilePath, statbuf.st_size);
            return extOp;
        }

        jsonFilePath = getJsonFile("echo $SVF_DIR");
        if (!stat(jsonFilePath.c_str(), &statbuf))
        {
            root = parseJson(jsonFilePath, statbuf.st_size);
            return extOp;
        }

        jsonFilePath = getJsonFile("npm root");
        if (!stat(jsonFilePath.c_str(), &statbuf))
        {
            root = parseJson(jsonFilePath, statbuf.st_size);
            return extOp;
        }
        SVFUtil::errs() << "No JsonFile found at " << jsonFilePath << " for getExtAPI(); set $SVF_DIR first!\n";
        abort();
    }
    return extOp;
}

void ExtAPI::destory()
{
    if (extOp != nullptr)
    {
        delete extOp;
        extOp = nullptr;
    }
    if (root != nullptr)
    {
        cJSON_Delete(root);
        root = nullptr;
    }
}

void ExtAPI::add_entry(const char* funName, const char* returnType,
                       std::vector<const char*> argTypes, extType type,
                       bool overwrite_app_function)
{
    assert(root && "Parse the json before adding additional entries");
    assert(get_type(funName) == EFT_NULL && "Do not add entries that already exist");
    auto entry = cJSON_CreateObject();

    // add the signature fields
    auto returnTypeString = cJSON_CreateString(returnType);
    cJSON_AddItemToObject(entry, "return", returnTypeString);

    std::stringstream ss;
    ss << "(";
    for (auto str : argTypes)
        ss << str << ",";
    std::string formattedArgs = ss.str();
    if (formattedArgs.back() == ',')
        formattedArgs.back() = ')';
    else
        formattedArgs.append(")");
    auto argsString = cJSON_CreateString(formattedArgs.c_str());
    cJSON_AddItemToObject(entry, "argument", argsString);

    // add the type field
    auto typeString = cJSON_CreateString(extType_toString(type).c_str());
    cJSON_AddItemToObject(entry, JSON_OPT_FUNCTIONTYPE, typeString);

    // add the 'overwrite_app_function' field
    auto overwriteBool = cJSON_CreateNumber(overwrite_app_function ? 1 : 0);
    cJSON_AddItemToObject(entry, JSON_OPT_OVERWRITE, overwriteBool);

    // add object to root
    cJSON_AddItemToObject(root, funName, entry);
}

// Get the corresponding name in ext_t, e.g. "EXT_ADDR" in {"addr", EXT_ADDR},
std::string ExtAPI::get_opName(const std::string& s)
{
    u32_t end = 0;
    while (end < s.size() && !isdigit(s[end]))
        end++;
    return s.substr(0, end);
}

const std::string& ExtAPI::extType_toString(extType type)
{
    auto it =
        std::find_if(type_pair.begin(), type_pair.end(),
                     [&](const auto& pair)
    {
        return pair.second == type;
    });
    assert(it != type_pair.end());
    return it->first;
}

// Get numeric index of the argument in external function
u32_t ExtAPI::getArgPos(const std::string& s)
{
    u32_t start = 0;
    while (start < s.size() && isalpha(s[start]))
        start++;
    if (s.substr(0, start) != "Arg")
        assert(false && "the argument of extern function in ExtAPI.json should start with 'Arg' !");
    u32_t end = start + 1;
    while (end < s.size() && isdigit(s[end]))
        end++;
    std::string digitStr = s.substr(start, end - start);
    u32_t argNum = atoi(digitStr.c_str());
    return argNum;
}

// return value >= 0 is an argument node
// return value = -1 is an inst node
// return value = -2 is a Dummy node
// return value = -3 is an object node
// return value = -4 is a nullptr node
// return value = -5 is an offset
// return value = -6 is an illegal operand format
s32_t ExtAPI::getNodeIDType(const std::string& s)
{
    u32_t argPos = -1;
    // 'A' represents an argument
    if (s.size() == 0)
        return -5;
    u32_t start = 0;
    while (start < s.size() && isalpha(s[start]))
        start++;
    std::string argStr = s.substr(0, start);
    if (argStr == "Arg")
    {
        u32_t end = start + 1;
        while (end < s.size() && isdigit(s[end]))
            end++;
        std::string digitStr = s.substr(start, end - start);
        argPos = atoi(digitStr.c_str());
        return argPos;
    }

    static const Map<std::string, s32_t> argStrToBinOp =
    {
        {"Ret", -1},
        {"Dummy", -2},
        {"Obj", -3},
        {"NullPtr", -4},
        {"Add", BinaryOPStmt::Add},
        {"Sub", BinaryOPStmt::Sub},
        {"Mul", BinaryOPStmt::Mul},
        {"SDiv", BinaryOPStmt::SDiv},
        {"SRem", BinaryOPStmt::SRem},
        {"Xor", BinaryOPStmt::Xor},
        {"And", BinaryOPStmt::And},
        {"Or", BinaryOPStmt::Or},
        {"AShr", BinaryOPStmt::AShr},
        {"Shl", BinaryOPStmt::Shl},
        {"ICMP_EQ", CmpStmt::ICMP_EQ},
        {"ICMP_NE", CmpStmt::ICMP_NE},
        {"ICMP_UGT", CmpStmt::ICMP_UGT},
        {"ICMP_SGT", CmpStmt::ICMP_SGT},
        {"ICMP_UGE", CmpStmt::ICMP_UGE},
        {"ICMP_SGE", CmpStmt::ICMP_SGE},
        {"ICMP_ULT", CmpStmt::ICMP_ULT},
        {"ICMP_SLT", CmpStmt::ICMP_SLT},
        {"ICMP_ULE", CmpStmt::ICMP_ULE},
        {"ICMP_SLE", CmpStmt::ICMP_SLE},
        {"FNeg", UnaryOPStmt::FNeg},
    };

    auto it = argStrToBinOp.find(argStr);
    if (it != argStrToBinOp.cend())
        return it->second;
    // offset
    u32_t i = 0;
    while (i < s.size() && isdigit(s[i]))
        i++;
    if (i == s.size())
        return -5;
    // illegal operand format
    return -6;
}

// Get external function name, e.g "memcpy"
std::string ExtAPI::get_name(const SVFFunction* F)
{
    assert(F);
    std::string funName = F->getName();
    if (F->isIntrinsic())
    {
        unsigned start = funName.find('.');
        unsigned end = funName.substr(start + 1).find('.');
        funName = "llvm." + funName.substr(start + 1, end);
    }
    return funName;
}

// Get specifications of external functions in ExtAPI.json file
cJSON* ExtAPI::get_FunJson(const std::string& funName)
{
    assert(root && "JSON not loaded");
    return cJSON_GetObjectItemCaseSensitive(root, funName.c_str());
}

ExtAPI::Operand ExtAPI::getBasicOperation(cJSON* obj)
{
    Operand basicOp;
    if (strstr(obj->string, "AddrStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Addr);
    else if (strstr(obj->string, "CopyStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Copy);
    else if (strstr(obj->string, "LoadStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Load);
    else if (strstr(obj->string, "StoreStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Store);
    else if (strstr(obj->string, "GepStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Gep);
    else if (strstr(obj->string, "ReturnStmt") != NULL)
        basicOp.setType(ExtAPI::OperationType::Return);
    else if (strstr(obj->string, "Rb_tree_ops") != NULL)
        basicOp.setType(ExtAPI::OperationType::Rb_tree_ops);
    else if (strstr(obj->string, "memcpy_like") != NULL)
        basicOp.setType(ExtAPI::OperationType::Memcpy_like);
    else if (strstr(obj->string, "memset_like") != NULL)
        basicOp.setType(ExtAPI::OperationType::Memset_like);
    else
        assert(false && "Unknown operation type");

    cJSON* value = obj->child;
    while (value)
    {
        if (strcmp(value->string, "src") == 0)
            basicOp.setSrcValue(value->valuestring);
        else if (strcmp(value->string, "dst") == 0)
            basicOp.setDstValue(value->valuestring);
        else if (strcmp(value->string, "offset") == 0 || strcmp(value->string, "size") == 0)
            basicOp.setOffsetOrSizeStr(value->valuestring);
        else
            assert(false && "Unknown operation value");
        value = value->next;
    }
    return basicOp;
}

// Get all operations of an extern function
ExtAPI::ExtFunctionOps ExtAPI::getExtFunctionOps(const SVFFunction* extFunction)
{
    ExtAPI::ExtFunctionOps extFunctionOps;
    extFunctionOps.setExtFunName(extFunction->getName());
    if (!is_sameSignature(extFunction))
        return extFunctionOps;

    auto it = extFunToOps.find(extFunction->getName());
    if (it != extFunToOps.end())
        return it->second;

    extFunctionOps.setExtFunName(extFunction->getName());
    cJSON* item = get_FunJson(extFunction->getName());
    if (item != nullptr)
    {
        cJSON* obj = item->child;
        //  Get the first operation of the function
        obj = obj->next->next->next->next;
        while (obj)
        {
            ExtOperation operation;
            if (obj->type == cJSON_Object || obj->type == cJSON_Array)
            {
                if (strstr(obj->string, "CallStmt") != NULL)
                {
                    extFunctionOps.setCallStmtNum(extFunctionOps.getCallStmtNum() + 1);
                    operation.setCallOp(true);
                    cJSON* value = obj->child;
                    while (value)
                    {
                        if (strcmp(value->string, "callee_name") == 0)
                            operation.setCalleeName(value->valuestring);
                        else if (strcmp(value->string, "callee_return") == 0)
                            operation.setCalleeReturn(value->valuestring);
                        else if (strcmp(value->string, "callee_arguments") == 0)
                            operation.setCalleeArguments(value->valuestring);
                        else
                            operation.getCalleeOperands().push_back(getBasicOperation(value));
                        value = value->next;
                    }
                }
                else if (strstr(obj->string, "CondStmt") != NULL)
                {
                    operation.setConOp(true);
                    obj = obj->child;
                    assert(strcmp(obj->string, "Condition") == 0 && "Unknown operation type");
                    operation.setConOp(obj->valuestring);
                    obj = obj->next;
                    assert(strcmp(obj->string, "TrueBranch") == 0 && "Unknown operation type");
                    cJSON* value = obj->child;
                    while (value)
                    {
                        operation.getTrueBranchOperands().push_back(getBasicOperation(value));
                        value = value->next;
                    }
                    obj = obj->next;
                    assert(strcmp(obj->string, "FalseBranch") == 0&& "Unknown operation type");
                    value = obj->child;
                    while (value)
                    {
                        operation.getFalseBranchOperands().push_back(getBasicOperation(value));
                        value = value->next;
                    }
                }
                else
                    operation.setBasicOp(getBasicOperation(obj));
            }
            obj = obj->next;
            extFunctionOps.getOperations().push_back(operation);
        }
    }
    extFunToOps[extFunction->getName()] = extFunctionOps;
    return extFunctionOps;
}

// Get arguments of the operation, e.g. ["A1R", "A0", "A2"]
std::vector<std::string> ExtAPI::get_opArgs(const cJSON* value)
{
    std::vector<std::string> args;
    while (value)
    {
        if (value->type == cJSON_String)
            args.push_back(value->valuestring);
        value = value->next;
    }
    return args;
}

ExtAPI::extType ExtAPI::get_type(const std::string& funName)
{
    cJSON* item = get_FunJson(funName);
    std::string type = "";
    if (item != nullptr)
    {
        //  Get the first operation of the function
        cJSON* obj = item->child->next->next;
        if (strcmp(obj->string, JSON_OPT_FUNCTIONTYPE) == 0)
            type = obj->valuestring;
        else
            assert(false && "The function operation format is illegal!");
    }
    std::map<std::string, extType>::const_iterator it = type_pair.find(type);
    if (it == type_pair.end())
        return EFT_NULL;
    else
        return it->second;
}

// Get property of the operation, e.g. "EFT_A1R_A0R"
ExtAPI::extType ExtAPI::get_type(const SVF::SVFFunction* F)
{
    return get_type(get_name(F));
}

// Get priority of he function, return value
// 0: Apply user-defined functions
// 1: Apply function specification in ExtAPI.json
u32_t ExtAPI::isOverwrittenAppFunction(const SVF::SVFFunction* callee)
{
    std::string funName = get_name(callee);
    return isOverwrittenAppFunction(funName);
}

u32_t ExtAPI::isOverwrittenAppFunction(const std::string& funName)
{
    cJSON* item = get_FunJson(funName);
    if (item != nullptr)
    {
        cJSON* obj = item->child;
        obj = obj->next->next->next;
        if (strcmp(obj->string, JSON_OPT_OVERWRITE) == 0)
            return obj->valueint;
        else
            assert(false && "The function operation format is illegal!");
    }
    return 0;
}

void ExtAPI::setOverWrittenAppFunction(const std::string& funcName, u32_t overwrite_app_function)
{
    auto item = get_FunJson(funcName);
    assert(item && "Do not set fields for ExtAPI funcs that don't exist!");
    auto overwrite_obj = item->child->next->next->next;
    assert(overwrite_obj);
    assert(strcmp(overwrite_obj->string, JSON_OPT_OVERWRITE) == 0);
    cJSON_SetIntValue(overwrite_obj, overwrite_app_function);
}

// Does (F) have a static var X (unavailable to us) that its return points to?
bool ExtAPI::has_static(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_STAT || t == EFT_STAT2;
}

// Assuming hasStatic(F), does (F) have a second static Y where X -> Y?
bool ExtAPI::has_static2(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_STAT2;
}

bool ExtAPI::is_memset_or_memcpy(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_L_A0__A0R_A1 || t == EFT_L_A0__A0R_A1R || t == EFT_A1R_A0R || t == EFT_A3R_A1R_NS
           || t == EFT_STD_RB_TREE_INSERT_AND_REBALANCE || t == EFT_L_A0__A1_A0;
}

bool ExtAPI::is_alloc(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_ALLOC || t == EFT_NOSTRUCT_ALLOC;
}

// Does (F) allocate a new object and assign it to one of its arguments?
bool ExtAPI::is_arg_alloc(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_A0R_NEW || t == EFT_A1R_NEW || t == EFT_A2R_NEW ||
           t == EFT_A4R_NEW || t == EFT_A11R_NEW;
}

// Get the position of argument which holds the new object
s32_t ExtAPI::get_alloc_arg_pos(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    switch (t)
    {
    case EFT_A0R_NEW:
        return 0;
    case EFT_A1R_NEW:
        return 1;
    case EFT_A2R_NEW:
        return 2;
    case EFT_A4R_NEW:
        return 4;
    case EFT_A11R_NEW:
        return 11;
    default:
        assert(false && "Not an alloc call via argument.");
        return -1;
    }
}

// Does (F) allocate only non-struct objects?
bool ExtAPI::no_struct_alloc(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_NOSTRUCT_ALLOC;
}

// Does (F) not free/release any memory?
bool ExtAPI::is_dealloc(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_FREE;
}

// Does (F) not do anything with the known pointers?
bool ExtAPI::is_noop(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_NOOP || t == EFT_FREE;
}

// Does (F) reallocate a new object?
bool ExtAPI::is_realloc(const SVFFunction* F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_REALLOC;
}

// Does (F) have the same return type(pointer or nonpointer) and same number of
// arguments
bool ExtAPI::is_sameSignature(const SVFFunction* F)
{
    cJSON* item = get_FunJson(F->getName());
    // If return type is pointer
    bool isPointer = false;
    // The number of arguments
    u32_t argNum = 0;
    if (item != nullptr)
    {
        // Get the "return" attribute
        cJSON* obj = item->child;
        if (strlen(obj->valuestring) == 0) // e.g. "return":  ""
            assert(false && "'return' should not be empty!");
        // If "return": "..." includes "*", the return type of extern function is a pointer
        if (strstr(obj->valuestring, "*") != NULL)
            isPointer = true;
        // Get the "arguments" attribute
        obj = obj->next;
        if (strlen(obj->valuestring) == 0) // e.g. "arguments":  "",
            assert(false && "'arguments' should not be empty!");
        // If "arguments":  "()", the number of arguments is 0, otherwise, number >= 1;
        if (strcmp(obj->valuestring, "()") != 0)
        {
            argNum++;
            for (u32_t i = 0; i < strlen(obj->valuestring); i++)
                if (obj->valuestring[i] == ',') // Calculate the number of arguments based on the number of ","
                    argNum++;
        }
    }

    if ( !F->isVarArg() && F->arg_size() != argNum) // The number of arguments is different
        return false;
    // Is the return type the same?
    return F->getReturnType()->isPointerTy() == isPointer;
}

// Should (F) be considered "external" (either not defined in the program
//   or a user-defined version of a known alloc or no-op)?
bool ExtAPI::is_ext(const SVFFunction* F)
{
    assert(F && "Null SVFFunction* pointer");
    bool res;
    if (F->isDeclaration() || F->isIntrinsic())
    {
        res = 1;
    }
    else
    {
        ExtAPI::extType t = get_type(F);
        if (t != EFT_NULL)
        {
            u32_t overwrittenAppFunction = isOverwrittenAppFunction(F);
            if (!is_sameSignature(F))
                res = 0;
            // overwrittenAppFunction = 1: Execute function specification in
            // ExtAPI.json F is considered as external function
            else if (overwrittenAppFunction == 1)
                res = 1;
            // overwrittenAppFunction = 0: Execute user-defined functions
            // F is not considered as external function
            else
                res = 0;
        }
        else
            res = 0;
    }
    return res;
}
