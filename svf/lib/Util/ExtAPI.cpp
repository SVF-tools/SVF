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
#include "Util/SVFUtil.h"
#include "Util/Options.h"
#include "Util/config.h"
#include <ostream>
#include <sys/stat.h>
#include "SVFIR/SVFVariables.h"
#include <dlfcn.h>

using namespace SVF;

ExtAPI* ExtAPI::extOp = nullptr;
std::string ExtAPI::extBcPath = "";

ExtAPI* ExtAPI::getExtAPI()
{
    if (extOp == nullptr)
    {
        extOp = new ExtAPI;
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
}

// Set extapi.bc file path
bool ExtAPI::setExtBcPath(const std::string& path)
{
    struct stat statbuf;
    if (!path.empty() && !stat(path.c_str(), &statbuf))
    {
        extBcPath = path;
        return true;
    }
    return false;
}

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

// Get extapi.bc file path in npm
static std::string getFilePath(const std::string& path)
{
    std::string bcFilePath = "";
    if (path.compare("SVF_DIR") == 0)
    {
        char const* svfdir = getenv("SVF_DIR");
        if (svfdir)
            bcFilePath = svfdir;
        if (!bcFilePath.empty() && bcFilePath.back() != '/')
            bcFilePath.push_back('/');
        bcFilePath.append(SVF_BUILD_TYPE "-build").append("/lib/extapi.bc");
    }
    else if (path.compare("npm root") == 0)
    {
        bcFilePath = GetStdoutFromCommand(path);
        if (!bcFilePath.empty() && bcFilePath.back() != '/')
            bcFilePath.push_back('/');
        bcFilePath.append("SVF/lib/extapi.bc");
    }
    return bcFilePath;
}

// This function returns the absolute path of the current module:
// - If SVF is built and loaded as a dynamic library (e.g., libSVFCore.so or .dylib), it returns the path to that shared object file.
// - If SVF is linked as a static library, it returns the path to the main executable.
// This is useful for locating resource files (such as extapi.bc) relative to the module at runtime.
std::string getCurrentSOPath()
{
    Dl_info info;
    if (dladdr((void*)&getCurrentSOPath, &info) && info.dli_fname)
    {
        return std::string(info.dli_fname);
    }
    return "";
}

// Get extapi.bc path
std::string ExtAPI::getExtBcPath()
{
    // Try to locate extapi.bc in the following order:
    // 1. If setExtBcPath() has been called, use that path.
    // 2. If the command line argument -extapi=path/to/extapi.bc is provided, use that path.
    // 3. If SVF is being developed and used directly from a build directory, try the build dir (SVF_BUILD_DIR).
    // 4. If the SVF_DIR environment variable is set, try $SVF_DIR with the standard relative path.
    // 5. If installed via npm, use `npm root` and append the standard relative path.
    // 6. As a last resort, use the directory of the loaded libSVFCore.so (or .dylib) and append extapi.bc.

    std::vector<std::string> candidatePaths;

    // 1. Use path set by setExtBcPath()
    if (!extBcPath.empty())
        return extBcPath;

    // 2. Use command line argument -extapi=...
    if (setExtBcPath(Options::ExtAPIPath()))
    {
        return extBcPath;
    }
    else
    {
        candidatePaths.push_back(Options::ExtAPIPath());
    }

    // 3. SVF developer: try build directory (SVF_BUILD_DIR)
    if (setExtBcPath(SVF_BUILD_DIR "/lib/extapi.bc"))
    {
        return extBcPath;
    }
    else
    {
        candidatePaths.push_back(SVF_BUILD_DIR "/lib/extapi.bc");
    }

    // 4. Use $SVF_DIR environment variable + standard relative path
    if (setExtBcPath(getFilePath("SVF_DIR")))
    {
        return extBcPath;
    }
    else
    {
        candidatePaths.push_back(getFilePath("SVF_DIR"));
    }

    // 5. Use npm root + standard relative path (for npm installations)
    if (setExtBcPath(getFilePath("npm root")))
    {
        return extBcPath;
    }
    else
    {
        candidatePaths.push_back(getFilePath("npm root"));
    }

    // 6. Use the directory of the loaded libSVFCore.so/.dylib + extapi.bc
    std::string soPath = getCurrentSOPath();
    if (!soPath.empty())
    {
        std::string dir = soPath.substr(0, soPath.find_last_of('/'));
        std::string candidate = dir + "/extapi.bc";
        if (setExtBcPath(candidate))
        {
            return extBcPath;
        }
        else
        {
            candidatePaths.push_back(candidate);
        }
    }

    // If all candidate paths failed, print error and suggestions
    SVFUtil::errs() << "ERROR: Failed to locate \"extapi.bc\". Tried the following candidate paths:" << std::endl;
    for (const auto& path : candidatePaths)
    {
        SVFUtil::errs() << "    " << path << std::endl;
    }
    SVFUtil::errs() << "To override the default locations for \"extapi.bc\", you can:" << std::endl
                    << "\t1. Use the command line argument \"-extapi=path/to/extapi.bc\"" << std::endl
                    << "\t2. Use the \"setExtBcPath()\" function *BEFORE* calling \"buildSVFModule()\"" << std::endl
                    << "\t3. Override the paths in \"include/SVF/Util/config.h\" (WARNING: will be overwritten "
                    << "when rebuilding SVF (generated by CMakeLists.txt))" << std::endl;
    abort();
}


void ExtAPI::setExtFuncAnnotations(const FunObjVar* fun, const std::vector<std::string>& funcAnnotations)
{
    assert(fun && "Null FunObjVar* pointer");
    funObjVar2Annotations[fun] = funcAnnotations;
}

bool ExtAPI::hasExtFuncAnnotation(const FunObjVar *fun, const std::string &funcAnnotation)
{
    assert(fun && "Null FunObjVar* pointer");
    auto it = funObjVar2Annotations.find(fun);
    if (it != funObjVar2Annotations.end())
    {
        for (const std::string& annotation : it->second)
            if (annotation.find(funcAnnotation) != std::string::npos)
                return true;
    }
    return false;
}


std::string ExtAPI::getExtFuncAnnotation(const FunObjVar* fun, const std::string& funcAnnotation)
{
    assert(fun && "Null FunObjVar* pointer");
    auto it = funObjVar2Annotations.find(fun);
    if (it != funObjVar2Annotations.end())
    {
        for (const std::string& annotation : it->second)
            if (annotation.find(funcAnnotation) != std::string::npos)
                return annotation;
    }
    return "";
}

const std::vector<std::string>& ExtAPI::getExtFuncAnnotations(const FunObjVar* fun)
{
    assert(fun && "Null FunObjVar* pointer");
    auto it = funObjVar2Annotations.find(fun);
    if (it != funObjVar2Annotations.end())
        return it->second;
    return funObjVar2Annotations[fun];
}

bool ExtAPI::is_memcpy(const FunObjVar *F)
{
    return F &&
           (hasExtFuncAnnotation(F, "MEMCPY") ||  hasExtFuncAnnotation(F, "STRCPY")
            || hasExtFuncAnnotation(F, "STRCAT"));
}

bool ExtAPI::is_memset(const FunObjVar *F)
{
    return F && hasExtFuncAnnotation(F, "MEMSET");
}

bool ExtAPI::is_alloc(const FunObjVar* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_HEAP_RET");
}

// Does (F) allocate a new object and assign it to one of its arguments?
bool ExtAPI::is_arg_alloc(const FunObjVar* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_HEAP_ARG");
}

bool ExtAPI::is_alloc_stack_ret(const FunObjVar* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_STACK_RET");
}

// Get the position of argument which holds the new object
s32_t ExtAPI::get_alloc_arg_pos(const FunObjVar* F)
{
    std::string allocArg = getExtFuncAnnotation(F, "ALLOC_HEAP_ARG");
    assert(!allocArg.empty() && "Not an alloc call via argument or incorrect extern function annotation!");

    std::string number;
    for (char c : allocArg)
    {
        if (isdigit(c))
            number.push_back(c);
    }
    assert(!number.empty() && "Incorrect naming convention for svf external functions(ALLOC_HEAP_ARG + number)?");
    return std::stoi(number);
}

// Does (F) reallocate a new object?
bool ExtAPI::is_realloc(const FunObjVar* F)
{
    return F && hasExtFuncAnnotation(F, "REALLOC_HEAP_RET");
}
bool ExtAPI::is_ext(const FunObjVar *F)
{
    assert(F && "Null FunObjVar* pointer");
    if (F->isDeclaration() || F->isIntrinsic())
        return true;
    else if (hasExtFuncAnnotation(F, "OVERWRITE") && getExtFuncAnnotations(F).size() == 1)
        return false;
    else
        return !getExtFuncAnnotations(F).empty();
}
