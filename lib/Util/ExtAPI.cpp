//===- ExtAPI.cpp -- External functions -----------------------------------------//
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
 * ExtAPI.cpp
 *
 *  Created on: July 1, 2022
 *      Author: Shuangxiang Kan
 */

#include "Util/ExtAPI.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

using namespace SVF;

ExtAPI *ExtAPI::extOp = nullptr;
cJSON *ExtAPI::root = nullptr;

// Get environment variables $SVF_DIR and "npm root" through popen() method
static std::string GetStdoutFromCommand(const std::string &command)
{
    char buffer[128];
    std::string result = "";
    // Open pipe to file
    FILE *pipe = popen(command.c_str(), "r");
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
static std::string getJsonFile(const std::string &path)
{
    std::string jsonFilePath = GetStdoutFromCommand(path);
    if (path.compare("npm root") == 0)
    {
        int os_flag = 1;
        // SVF installed via npm needs to determine the type of operating system,
        // otherwise the ExtAPI.json path may not be found
        // Linux os
#ifdef linux
        os_flag = 0;
        jsonFilePath.append("/svf-lib/SVF-linux");
#endif
        // Mac os
        if (os_flag == 1)
        {
            jsonFilePath.append("/svf-lib/SVF-osx");
        }
    }
    jsonFilePath.append(EXTAPI_JSON_PATH);
    return jsonFilePath;
}

static cJSON *parseJson(const std::string &path, off_t fileSize)
{
    FILE *file = fopen(path.c_str(), "r");
    if (!file)
    {
        return nullptr;
    }

    // allocate memory size matched with file size
    char *jsonStr = (char *)calloc(fileSize + 1, sizeof(char));

    // read json string from file
    u32_t size = fread(jsonStr, sizeof(char), fileSize, file);
    if (size == 0)
    {
        SVFUtil::errs() << SVFUtil::errMsg("\t Wrong ExtAPI.json path!! ") << "The current ExtAPI.json path is: " << path << "\n";
        assert(false && "Read ExtAPI.json file fails!");
        return nullptr;
    }
    fclose(file);

    // convert json string to json pointer variable
    cJSON *root = cJSON_Parse(jsonStr);
    if (!root)
    {
        free(jsonStr);
        return nullptr;
    }
    free(jsonStr);
    return root;
}

ExtAPI *ExtAPI::getExtAPI(const std::string &path)
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

        jsonFilePath = PROJECT_PATH + std::string(EXTAPI_JSON_PATH);
        if (!stat(jsonFilePath.c_str(), &statbuf))
        {
            root = parseJson(jsonFilePath, statbuf.st_size);
            return extOp;
        }

        jsonFilePath = getJsonFile("$SVF_DIR");
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

        assert(false && "Open ExtAPI.json file fails!");
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

void ExtAPI::add_entry(const char* funName, extType type, bool overwrite_app_function)
{
    assert(root);
    assert(get_type(funName) == EFT_NULL);
    auto entry = cJSON_CreateObject();

    // add the type field
    auto typeString = cJSON_CreateString(extType_toString(type).c_str());
    cJSON_AddItemToObject(entry, JSON_OPT_FUNCTIONTYPE, typeString);

    // add the 'overwrite_app_function' field
    auto overwriteBool = cJSON_CreateNumber(overwrite_app_function ? 1 : 0);
    cJSON_AddItemToObject(entry, JSON_OPT_OVERWRITE, overwriteBool);

    // we don't know where the `funName` comes from, so copy it just in case
    cJSON_AddItemToObject(root, strdup(funName), entry);
}

// Get the corresponding name in ext_t, e.g. "EXT_ADDR" in {"addr", EXT_ADDR},
ExtAPI::extf_t ExtAPI::get_opName(const std::string& s)
{
    std::map<std::string, extf_t>::iterator pos = op_pair.find(s);
    if (pos != op_pair.end())
    {
        return pos->second;
    }
    else
    {
        return EXT_OTHER;
    }
}

const std::string& ExtAPI::extType_toString(extType type)
{
    auto it = llvm::find_if(type_pair, [&](const auto& pair)
    {
        return pair.second == type;
    });
    assert(it != type_pair.end());
    return it->first;
}

// Get numeric index of the argument in external function
u32_t ExtAPI::getArgPos(std::string s)
{
    if(s[0] != 'A')
        assert(false && "the argument of extern function in ExtAPI.json should start with 'A' !");
    u32_t i = 1;
    u32_t start = i;
    while(i < s.size() && isdigit(s[i]))
        i++;
    std::string digitStr = s.substr(start, i-start);
    u32_t argNum = atoi(digitStr.c_str());
    return argNum;
}

// return value >= 0 is an argument node
// return value = -1 is an inst node
// return value = -2 is a Dummy node
// return value = -2 is an illegal operand format
int ExtAPI::getNodeIDType(std::string s)
{
    size_t argPos = -1;
    // 'A' represents an argument
    if (s.size() == 0)
        return -3;
    if (s[0] == 'A')
    {
        size_t start = 1;
        size_t end = 1;
        while(end < s.size() && isdigit(s[end]))
            end++;
        std::string digitStr = s.substr(start, end - start);
        argPos = atoi(digitStr.c_str());
        return argPos;
    }
    if(s[0] == 'L')
        return -1;
    if(s[0] == 'D')
        return -2;

    return -3;
}

// Get external function name, e.g "memcpy"
std::string ExtAPI::get_name(const SVFFunction *F)
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
cJSON *ExtAPI::get_FunJson(const std::string &funName)
{
    assert(root && "JSON not loaded");
    return cJSON_GetObjectItemCaseSensitive(root, funName.c_str());
}

// Get all operations of an extern function
std::vector<std::vector<ExtAPI::Operation *>> ExtAPI::getAllOperations(std::string funName)
{
    std::vector<std::vector<ExtAPI::Operation *>> allOperations;
    cJSON *item = get_FunJson(funName);
    if (item != nullptr)
    {
        cJSON *obj = item->child;
        //  Get the first operation of the function
        obj = obj -> next -> next;
        std::vector<ExtAPI::Operation *> operations;
        while (obj)
        {
            std::string operationName;
            std::vector<std::string> arguments;
            Operation operation;
            // All operations in "compound" are related to each other.
            // For example, the first parameter of the second operation
            // depends on the second parameter of the first operation.
            // Therefore, all operations in "compound" need to be processed uniformly
            if (strstr(obj->string, "compound") != NULL)
            {
                if (obj->type == cJSON_Object)
                {
                    cJSON *value = obj->child;
                    while (value)
                    {
                        operationName = value -> string;
                        if (value->type == cJSON_Object)
                        {
                            cJSON *edge = value->child;
                            arguments = ExtAPI::getExtAPI()->get_opArgs(edge);
                        }
                        else
                        {
                            if (value->type == cJSON_String)
                                arguments.push_back(value->valuestring);
                            else
                                assert(false && "The function operation format is illegal!");
                        }
                        operations.push_back(new ExtAPI::Operation(operationName, arguments));
                        arguments.clear();
                        value = value->next;
                    }
                }
            }
            // General operation(Independent operation, the operation does not need to dependent other operations' arguments)
            else
            {
                if (obj->type == cJSON_Object || obj->type == cJSON_Array)
                {
                    operationName = obj -> string;
                    cJSON *edge = obj->child;
                    arguments = ExtAPI::getExtAPI()->get_opArgs(edge);
                    operations.push_back(new ExtAPI::Operation(operationName, arguments));
                    arguments.clear();
                }
            }
            allOperations.push_back(operations);
            operations.clear();

            obj = obj -> next;
        }
    }
    return allOperations;
}

// Get arguments of the operation, e.g. ["A1R", "A0", "A2"]
std::vector<std::string> ExtAPI::get_opArgs(const cJSON *value)
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
    cJSON *item = get_FunJson(funName);
    std::string type = "";
    if (item != nullptr)
    {
        //  Get the first operation of the function
        cJSON *obj = item->child;
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
ExtAPI::extType ExtAPI::get_type(const SVF::SVFFunction *F)
{
    return get_type(get_name(F));
}

// Get priority of he function, return value
// 0: Apply user-defined functions
// 1: Apply function specification in ExtAPI.json
u32_t ExtAPI::isOverwrittenAppFunction(const SVF::SVFFunction *callee)
{
    std::string funName = get_name(callee);
    cJSON *item = get_FunJson(funName);
    if (item != nullptr)
    {
        cJSON *obj = item->child;
        obj = obj->next;
        if (strcmp(obj->string, JSON_OPT_OVERWRITE) == 0)
            return obj->valueint;
        else
            assert(false && "The function operation format is illegal!");
    }
    return 0;
}

// Does (F) have a static var X (unavailable to us) that its return points to?
bool ExtAPI::has_static(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_STAT || t == EFT_STAT2;
}

// Assuming hasStatic(F), does (F) have a second static Y where X -> Y?
bool ExtAPI::has_static2(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_STAT2;
}

bool ExtAPI::is_alloc(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_ALLOC || t == EFT_NOSTRUCT_ALLOC;
}

// Does (F) allocate a new object and assign it to one of its arguments?
bool ExtAPI::is_arg_alloc(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_A0R_NEW || t == EFT_A1R_NEW || t == EFT_A2R_NEW || t == EFT_A4R_NEW || t == EFT_A11R_NEW;
}

// Get the position of argument which holds the new object
s32_t ExtAPI::get_alloc_arg_pos(const SVFFunction *F)
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
bool ExtAPI::no_struct_alloc(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_NOSTRUCT_ALLOC;
}

// Does (F) not free/release any memory?
bool ExtAPI::is_dealloc(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_FREE;
}

// Does (F) not do anything with the known pointers?
bool ExtAPI::is_noop(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_NOOP || t == EFT_FREE;
}

// Does (F) reallocate a new object?
bool ExtAPI::is_realloc(const SVFFunction *F)
{
    ExtAPI::extType t = get_type(F);
    return t == EFT_REALLOC;
}

// Should (F) be considered "external" (either not defined in the program
//   or a user-defined version of a known alloc or no-op)?
bool ExtAPI::is_ext(const SVFFunction *F)
{
    assert(F);
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
            // overwrittenAppFunction = 1: Execute function specification in ExtAPI.json
            // F is considered as external function
            if (overwrittenAppFunction == 1)
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
