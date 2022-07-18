/*  [ExtAPI.cpp] The actual database of external functions
 *  v. 005, 2008-08-08
 *------------------------------------------------------------------------------
 */

/*
 * Modified by Yulei Sui 2013
*/

#include "Util/ExtAPI.h"
#include "Util/SVFUtil.h"
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

using namespace SVF;

ExtAPI *ExtAPI::extOp = nullptr;
cJSON *ExtAPI::root = nullptr;

ExtAPI *ExtAPI::getExtAPI()
{
    if (extOp == nullptr)
    {
        extOp = new ExtAPI();
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

// Get the corresponding name in ext_t, e.g. "EXT_ADDR" in {"addr", EXT_ADDR},
ExtAPI::extf_t ExtAPI::get_opName(std::string s)
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

// Get environment variables $SVF_DIR and "npm root" through popen() method
std::string GetStdoutFromCommand(std::string command)
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
FILE *getJsonFile(std::string path)
{
    std::string jsonFilePath = GetStdoutFromCommand(path);
    if (path.compare("npm root") == 0)
    {
        int os_flag = 1;
        // SVF installed via npm needs to determine the type of operating system, otherwise the ExtAPI.json path may not be found
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
    FILE *file = nullptr;
    file = fopen(jsonFilePath.c_str(), "r");
    return file;
}

// Get specifications of external functions in ExtAPI.json file
cJSON *ExtAPI::get_FunJson(const std::string funName)
{
    if (!root)
    {
        // Three ways to get ExtAPI.json path
        // 1. default path(get ExtAPI.json path from Util/config.h)
        // 2. from $SVF_DIR
        // 3. from "npm root"(If SVF is installed via npm)
        std::string jsonFilePath = PROJECT_PATH;
        jsonFilePath.append(EXTAPI_JSON_PATH);
        // open file
        FILE *file = nullptr;
        file = fopen(jsonFilePath.c_str(), "r");
        if (file == nullptr)
        {
            file = getJsonFile("$SVF_DIR");
            if (file == nullptr)
            {
                file = getJsonFile("npm root");
                if (file == nullptr)
                {
                    assert(false && "Open ExtAPI.json file fails!");
                    return nullptr;
                }
            }
        }
        // get file size
        struct stat statbuf;
        stat(jsonFilePath.c_str(), &statbuf);
        u32_t fileSize = statbuf.st_size;

        // allocate memory size matched with file size
        char *jsonStr = (char *)malloc(sizeof(char) * fileSize + 1);
        memset(jsonStr, 0, fileSize + 1);

        // read json string from file
        u32_t size = fread(jsonStr, sizeof(char), fileSize, file);
        if (size == 0)
        {
            assert(false && "Read ExtAPI.json file fails!");
            return nullptr;
        }
        fclose(file);

        // convert json string to json pointer variable
        root = cJSON_Parse(jsonStr);
        if (!root)
        {
            free(jsonStr);
            return nullptr;
        }
        free(jsonStr);
    }
    return cJSON_GetObjectItemCaseSensitive(root, funName.c_str());
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

// Get property of the operation, e.g. "EFT_A1R_A0R"
ExtAPI::extType ExtAPI::get_type(const SVF::SVFFunction *F)
{
    std::string funName = get_name(F);
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
