#include "Util/CallerSensitive.h"
#include "SVF-FE/LLVMUtil.h"
#include <fstream>
#include <string>

using namespace SVF;

cJSON *CallerSensitve::parseCallerJson(std::string path)
{
    FILE *file = NULL;
    std::string FILE_NAME = path;
    file = fopen(FILE_NAME.c_str(), "r");
    if (file == NULL)
    {
        assert(false && "Open Caller Json file fail!");
        return nullptr;
    }

    struct stat statbuf;
    stat(FILE_NAME.c_str(), &statbuf);
    u32_t fileSize = statbuf.st_size;

    char *jsonStr = (char *)malloc(sizeof(char) * fileSize + 1);
    memset(jsonStr, 0, fileSize + 1);

    u32_t size = fread(jsonStr, sizeof(char), fileSize, file);
    if (size == 0)
    {
        assert(false && "Read Caller Json file fails!");
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

CallerSensitve::JavaDataType CallerSensitve::getBasicType(std::string type)
{
    std::map<std::string, JavaDataType>::const_iterator it = javaBasicTypes.find(type);
    if (it == javaBasicTypes.end())
        return JAVA_NULL;
    else
        return it->second;
}

Type *CallerSensitve::getType(std::string type)
{
    if (javaPyTypes.count(type))
        return irBuilder.getInt8PtrTy();

    JavaDataType ctype = getBasicType(type);

    switch (ctype)
    {
    case JBOOLEAN:
        return irBuilder.getInt1Ty();
    case JBYTE:
        return irBuilder.getInt8Ty();
    case JCHAR:
        return irBuilder.getInt16Ty();
    case JSHORT:
        return irBuilder.getInt16Ty();
    case JINT:
        return irBuilder.getInt32Ty();
    case JLONG:
        return irBuilder.getInt64Ty();
    case JFLOAT:
        return irBuilder.getFloatTy();
    case JDOUBLE:
        return irBuilder.getDoubleTy();
    case VOID:
        return irBuilder.getVoidTy();
    default:
        assert(false && "illegal Java Type!!!");
        break;
    }
    return nullptr;
}

std::vector<Type *> CallerSensitve::getParams(std::vector<std::string> args)
{
    std::vector<Type *> params;
    for (std::vector<std::string>::iterator it = args.begin(); it != args.end(); ++it)
    {
        params.push_back(getType(*it));
    }
    return params;
}

void CallerSensitve::output2file(std::string outPath)
{
    std::error_code EC;
    raw_fd_ostream output(outPath, EC);
    WriteBitcodeToFile(*module, output);
}

Function *CallerSensitve::functionDeclarationIR(std::string funName, std::string ret, std::vector<std::string> args)
{
    Type *retType = getType(ret);
    std::vector<Type *> params = getParams(args);
    FunctionType *type = FunctionType::get(retType, params, false);
    return Function::Create(type, GlobalValue::ExternalLinkage, funName, module);
}

std::map<std::string, Value *> CallerSensitve::allocaStore(std::vector<std::string> params, Function *func)
{
    std::string arg = "arg";
    std::map<std::string, Value *> callerArgsMap;
    for (u32_t i = 0; i < params.size(); i++)
        callerArgsMap[arg + std::to_string(i)] = irBuilder.CreateAlloca(getType(params[i]));
    for (u32_t i = 0; i < params.size(); i++)
        irBuilder.CreateStore(func->getArg(i), callerArgsMap[arg + std::to_string(i)]);
    return callerArgsMap;
}

Value *CallerSensitve::allocaStoreLoad(std::string typeStr, Value *value)
{
    Type *type = getType(typeStr);
    Value *alloca = irBuilder.CreateAlloca(type, nullptr);
    irBuilder.CreateStore(value, alloca);
    return irBuilder.CreateLoad(type, alloca);
}

Value *CallerSensitve::getCalleeArgs(std::string typeStr, std::string name, std::map<std::string, Value *> callerArgs, std::map<std::string, Value *> rets)
{
    Type *type = getType(typeStr);
    if (strstr(name.c_str(), "arg"))
    {
        return irBuilder.CreateLoad(type, callerArgs[name]);
    }
    else if (strstr(name.c_str(), "ret"))
    {
        return allocaStoreLoad(typeStr, rets[name]);
    }
    return nullptr;
}

u32_t CallerSensitve::getNumber(std::string s)
{

    u32_t argPos = -1;
    u32_t start = 0;
    while (start < s.size() && isalpha(s[start]))
        start++;
    u32_t end = start + 1;
    while (end < s.size() && isdigit(s[end]))
        end++;
    std::string digitStr = s.substr(start, end - start);
    argPos = atoi(digitStr.c_str());
    return argPos;
}

std::string CallerSensitve::getJsonFile(int &argc, char **argv)
{
    for (int i = 0; i < argc; ++i)
    {
        std::string argument(argv[i]);
        // File is a .json file not a IR file
        if (strstr(argument.c_str(), ".json") && !LLVMUtil::isIRFile(argument))
        {
            for (int j = i; j < argc && j + 1 < argc; ++j)
                argv[j] = argv[j + 1];
            argc--;
            return argument;
        }
    }
    return nullptr;
}

void CallerSensitve::callerIRCreate(std::string jsonPath, std::string irPath)
{

    module = new Module("Callers", llvmContext);
    Function *callerFun = nullptr;
    Function *calleeFun = nullptr;

    std::string callerRet;
    std::vector<std::string> callerParams;
    std::map<std::string, Value *> callerArgs;
    std::map<std::string, Value *> rets;
    std::string path = jsonPath;

    cJSON *root = parseCallerJson(path);

    cJSON *callerJson = root->child;
    while (callerJson)
    {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(root, callerJson->string);
        if (item != NULL)
        {
            cJSON *obj = item->child;
            if (strcmp(obj->string, "return") == 0)
                callerRet = obj->valuestring;
            obj = obj->next;
            if (strcmp(obj->string, "parameters") == 0)
            {
                cJSON *param = obj->child;
                while (param)
                {
                    callerParams.push_back(param->valuestring);
                    param = param->next;
                }
            }
            callerFun = functionDeclarationIR(callerJson->string, callerRet, callerParams);
            BasicBlock *entryBB = BasicBlock::Create(llvmContext, "entry", callerFun);
            irBuilder.SetInsertPoint(entryBB);
            callerArgs = allocaStore(callerParams, callerFun);

            u32_t index = 1;

            while (obj)
            {
                if (strstr(obj->string, "CopyStmt"))
                {
                    cJSON *stmt = obj->child;
                    std::string src, dst;
                    if (strcmp(stmt->string, "src") == 0)
                        src = stmt->valuestring;
                    stmt = stmt->next;
                    if (strcmp(stmt->string, "dst") == 0)
                        dst = stmt->valuestring;
                    if (strcmp(src.c_str(), "void") == 0)
                    {
                        irBuilder.CreateRetVoid();
                    }
                    else if (strstr(src.c_str(), "ret"))
                    {
                        Value *retV = allocaStoreLoad(callerRet, rets[src]);
                        irBuilder.CreateRet(retV);
                    }
                }
                else if (strstr(obj->string, "callee"))
                {
                    std::string calleeName;
                    std::string calleeRet;
                    std::vector<std::string> calleeParams;
                    std::vector<Value *> putsargs;
                    cJSON *callee = obj->child;
                    if (strcmp(callee->string, "name") == 0)
                        calleeName = callee->valuestring;

                    callee = callee->next;
                    if (strcmp(callee->string, "return") == 0)
                        calleeRet = callee->valuestring;

                    callee = callee->next;
                    if (strcmp(callee->string, "parameters") == 0)
                    {
                        cJSON *item = callee->child;
                        while (item)
                        {
                            calleeParams.push_back(item->valuestring);
                            item = item->next;
                        }
                    }
                    calleeFun = functionDeclarationIR(calleeName, calleeRet, calleeParams);
                    callee = callee->next;
                    if (strcmp(callee->string, "arguments") == 0)
                    {
                        cJSON *item = callee->child;
                        u32_t i = 0;
                        while (item)
                        {
                            putsargs.push_back(getCalleeArgs(calleeParams[i], item->valuestring, callerArgs, rets));
                            item = item->next;
                            i++;
                        }
                    }
                    ArrayRef<Value *> argRef(putsargs);
                    Value *ret = irBuilder.CreateCall(calleeFun, argRef);
                    std::string retStr = "ret";
                    rets[retStr + std::to_string(index)] = ret;
                    index++;
                    callee = callee->next;
                }
                obj = obj->next;
            }
        }

        callerJson = callerJson->next;
    }

    std::string outPath = "./Callers.ll";
    // Output call IR file
    output2file(outPath);
    std::string resultFile = "./caller_callee.ll";
    // Link caller.ll and callee.ll
    std::string linkCommand = PROJECT_PATH + std::string("/llvm-13.0.0.obj/bin/llvm-link ") + outPath + std::string(" ") + irPath + std::string(" -o ") + resultFile;
    system(linkCommand.c_str());
}
