/*
 * SVFModuleJsonDumper.h
 *
 *  Created on: 27 Jan 2023
 *      Author: Xudong Wang
 */

#ifndef INCLUDE_SVFMODULE_JSON_DUMPER_H_
#define INCLUDE_SVFMODULE_JSON_DUMPER_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>
#include <memory>

struct cJSON;

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

class SVFModuleJsonReader
{
private:
    const SVFModule* module;

    std::vector<SVFType*>
        typePool; /// < A pool of all SVFTypes in the SVFModule
    std::vector<cJSON*> typeArray;

    std::vector<SVFValue*>
        valuePool; /// < A pool of all SVFValues in the SVFModule
    std::vector<cJSON*> valueArray;

public:
    const SVFModule* readSvfModule(cJSON* node);

private:
    SVFType* indexToType(TypeIndex i);
    SVFValue* indexToValue(ValueIndex i);

    void fillSvfTypeAt(TypeIndex i);
    void fillSvfValueAt(ValueIndex i);

    StInfo* readStInfo(cJSON* iter);
    cJSON* readJson(cJSON* iter, SVFType* type);
    cJSON* readJson(cJSON* iter, SVFPointerType* type);
    cJSON* readJson(cJSON* iter, SVFIntegerType* type);
    cJSON* readJson(cJSON* iter, SVFFunctionType* type);
    cJSON* readJson(cJSON* iter, SVFStructType* type);
    cJSON* readJson(cJSON* iter, SVFArrayType* type);
    cJSON* readJson(cJSON* iter, SVFOtherType* type);

    cJSON* readJson(cJSON* iter, SVFValue* value);
    cJSON* readJson(cJSON* iter, SVFFunction* value);
    cJSON* readJson(cJSON* iter, SVFBasicBlock* value);
    cJSON* readJson(cJSON* iter, SVFInstruction* value);
    cJSON* readJson(cJSON* iter, SVFCallInst* value);
    cJSON* readJson(cJSON* iter, SVFVirtualCallInst* value);
    cJSON* readJson(cJSON* iter, SVFConstant* value);
    cJSON* readJson(cJSON* iter, SVFGlobalValue* value);
    cJSON* readJson(cJSON* iter, SVFArgument* value);
    cJSON* readJson(cJSON* iter, SVFConstantData* value);
    cJSON* readJson(cJSON* iter, SVFConstantInt* value);
    cJSON* readJson(cJSON* iter, SVFConstantFP* value);
    cJSON* readJson(cJSON* iter, SVFConstantNullPtr* value);
    cJSON* readJson(cJSON* iter, SVFBlackHoleValue* value);
    cJSON* readJson(cJSON* iter, SVFOtherValue* value);
    cJSON* readJson(cJSON* iter, SVFMetadataAsValue* value);
};

} // namespace SVF

#endif // !INCLUDE_SVFMODULE_JSON_DUMPER_H_