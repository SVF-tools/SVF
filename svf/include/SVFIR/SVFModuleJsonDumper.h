#ifndef INCLUDE_SVFMODULE_JSON_DUMPER_H_
#define INCLUDE_SVFMODULE_JSON_DUMPER_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>
#include <memory>

class cJSON;

namespace SVF
{
class SVFModule;

class StInfo;
class SVFType;
class SVFPointerType;
class SVFIntegerType;
class SVFFunctionType;
class SVFStructType;
class SVFArrayType;
class SVFOtherType;

class SVFLoopAndDomInfo;
class SVFValue;
class SVFFunction;
class SVFBasicBlock;
class SVFInstruction;
class SVFCallInst;
class SVFVirtualCallInst;
class SVFConstant;
class SVFGlobalValue;
class SVFArgument;
class SVFConstantData;
class SVFConstantInt;
class SVFConstantFP;
class SVFConstantNullPtr;
class SVFBlackHoleValue;
class SVFOtherValue;
class SVFMetadataAsValue;

using TypeIndex = std::size_t;
using ValueIndex = std::size_t;

class SVFModuleJsonDumper
{
private:
    const SVFModule* module;
    const char* jsonStr;

    std::unordered_map<const SVFType*, TypeIndex> typeToIndex;
    std::vector<const SVFType*>
        typePool; /// < A pool of all SVFTypes in the SVFModule
    TypeIndex getTypeIndex(const SVFType* type);
    const char* getStrTypeIndex(const SVFType* type);

    std::unordered_map<const SVFValue*, ValueIndex> valueToIndex;
    std::vector<const SVFValue*>
        valuePool; /// < A pool of all SVFValues in the SVFModule
    ValueIndex getValueIndex(const SVFValue* value);
    const char* getStrValueIndex(const SVFValue* value);

    std::vector<std::unique_ptr<std::string>> allIndices;
    const char* getStrOfIndex(std::size_t index);

public:
    SVFModuleJsonDumper(const SVFModule* module);
    SVFModuleJsonDumper(const SVFModule* module, const std::string& path);
    void dumpJsonToPath(const std::string& path);
    void dumpJsonToOstream(std::ostream& os);
    ~SVFModuleJsonDumper();

private:
    cJSON* moduleToJson(const SVFModule* module);

    cJSON* typeToJson(const SVFType* type);
    cJSON* toJson(const StInfo* stInfo);
    // SVFType and all its descendants/subclasses.
    cJSON* toJson(const SVFType* type);
    cJSON* toJson(const SVFPointerType* type);
    cJSON* toJson(const SVFIntegerType* type);
    cJSON* toJson(const SVFFunctionType* type);
    cJSON* toJson(const SVFStructType* type);
    cJSON* toJson(const SVFArrayType* type);
    cJSON* toJson(const SVFOtherType* type);

    cJSON* valueToJson(const SVFValue* value);
    cJSON* toJson(const SVFLoopAndDomInfo* ldInfo);
    // SVFValue and all its descendants/subclasses.
    cJSON* toJson(const SVFValue* value);
    cJSON* toJson(const SVFFunction* value);
    cJSON* toJson(const SVFBasicBlock* value);
    cJSON* toJson(const SVFInstruction* value);
    cJSON* toJson(const SVFCallInst* value);
    cJSON* toJson(const SVFVirtualCallInst* value);
    cJSON* toJson(const SVFConstant* value);
    cJSON* toJson(const SVFGlobalValue* value);
    cJSON* toJson(const SVFArgument* value);
    cJSON* toJson(const SVFConstantData* value);
    cJSON* toJson(const SVFConstantInt* value);
    cJSON* toJson(const SVFConstantFP* value);
    cJSON* toJson(const SVFConstantNullPtr* value);
    cJSON* toJson(const SVFBlackHoleValue* value);
    cJSON* toJson(const SVFOtherValue* value);
    cJSON* toJson(const SVFMetadataAsValue* value);
};

} // namespace SVF

#endif // !INCLUDE_SVFMODULE_JSON_DUMPER_H_
