#ifndef CALLERSENSITIVE_H_
#define CALLERSENSITIVE_H_
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "Util/BasicTypes.h"
#include <sys/stat.h>
#include "Util/config.h"
#include "Util/cJSON.h"
#include <set>

using namespace llvm;

namespace SVF
{

static LLVMContext llvmContext;
static Module *module;
static llvm::IRBuilder<> irBuilder(llvmContext);

class CallerSensitve
{

public:
    
    enum JavaDataType
    {
        JBOOLEAN = 0,  // unsigned 8 bits
        JBYTE,         // signed 8 bits
        JCHAR,         // unsigned 16 bits
        JSHORT,        // signed 16 bits
        JINT,          // unsigned 32 bits
        JLONG,         // signed 64 bits
        JFLOAT,        // 32 bits
        JDOUBLE,       // 64 bits
        JOBJECT,       // any java object
        JSTRING,       // string object
        JCLASS,        // class object
        JOBJECTARRAY,  // object array
        JBOOLEANARRAY, // boolean array
        JBYTEARRAY,    // byte array
        JCHARARRAY,    // char array
        JSHORTARRAY,   // short array
        JINTARRAY,     // int array
        JLONGARRAY,    // long array
        JFLOATARRAY,   // float array
        JDOUBLEARRAY,  // double array
        VOID,           // void
        JAVA_NULL
    };

    std::map<std::string, JavaDataType> javaBasicTypes =
    {
        {"bool", JBOOLEAN},
        {"byte", JBYTE},
        {"char", JCHAR},
        {"short", JSHORT},
        {"int", JINT},
        {"long long", JLONG},
        {"float", JFLOAT},
        {"double", JDOUBLE},
        {"void", VOID},
        {"", JAVA_NULL}
    };

    // Except for the basic data types, other java data types are mapped to (void *) in C language
    std::set<std::string> javaPyTypes{"void *", "jobject", "jclass", "jstring", "jobjectArray", "jbooleanArray", "jbyteArray", "jcharArray", "jshortArray", "jintArray", "jlongArray", "jfloatArray", "jdoubleArray"};

    // Get specifications of caller functions
    cJSON *parseCallerJson(std::string path);

    // Get basic Java data type
    JavaDataType getBasicType(std::string type);

    // Get the data type of the C language corresponding to the java data type
    Type *getType(std::string type);

    // Get the data types of the a function's parameters
    std::vector<Type *> getParams(std::vector<std::string> args);

    // Output final .ll file
    void output2file(std::string outPath);

    // Create function declaration IR
    Function * functionDeclarationIR(std::string funName, std::string ret, std::vector<std::string> args);

    // Get Alloca and Store IR instructions
    std::map<std::string, Value *> allocaStore(std::vector<std::string> params, Function *func);

    // Get Alloca, Store and Load IR instructions
    Value * allocaStoreLoad(std::string typeStr, Value * value);

    // Get callee's arguments
    Value *getCalleeArgs(std::string typeStr, std::string name, std::map<std::string, Value *> callerArgs, std::map<std::string, Value *> rets);

    // Get number of a argument or return vale (e.g. arg0, ret1)
    u32_t getNumber(std::string s);

    // Get .json file path from input
    std::string getJsonFile(int &argc, char **argv);

    // Create caller IR
    void callerIRCreate(std::string jsonPath, std::string irPath);

};

}

#endif /* CALLERSENSITIVE_ */