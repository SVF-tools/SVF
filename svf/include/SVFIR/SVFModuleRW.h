//===- SVFModuleRW.h -- SVFModuleReader* class--------------------------------//
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
 * SVFModuleRW.h
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

class SVFModuleWrite
{
private:
    const SVFModule* module; ///< Borrowed pointer to the SVFModule.
    const char* jsonStr; ///< Json string of the SVFModule. It gets freed by `cJSON_free()` in destructor.

    std::unordered_map<const SVFType*, TypeIndex> typeToIndex;
    std::vector<const SVFType*> typePool; ///< A pool of all SVFTypes in the SVFModule
    TypeIndex getTypeIndex(const SVFType* type);
    const char* getStrTypeIndex(const SVFType* type);

    std::unordered_map<const SVFValue*, ValueIndex> valueToIndex;
    std::vector<const SVFValue*> valuePool; ///< A pool of all SVFValues in the SVFModule
    ValueIndex getValueIndex(const SVFValue* value);
    const char* getStrValueIndex(const SVFValue* value);

    std::vector<std::unique_ptr<std::string>> allIndices;
    const char* getStrOfIndex(std::size_t index);

public:
    SVFModuleWrite(const SVFModule* module);
    /// @brief  Dump the SVFModule to a file in JSON format at the given path.
    SVFModuleWrite(const SVFModule* module, const std::string& path);
    void dumpJsonToPath(const std::string& path);
    void dumpJsonToOstream(std::ostream& os);
    ~SVFModuleWrite();

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

class SVFModuleRead
{
private:
    cJSON* moduleJson; ///< Owned pointer to the root object of the SVFModule. Be
    ///< sure to delete it with `cJSON_Delete()` it in
    ///< destructor.

    SVFModule* svfModule;

    std::vector<SVFType*> typePool; ///< A pool of all SVFTypes in the SVFModule
    std::vector<cJSON*> typeArray;

    std::vector<SVFValue*>
    valuePool; ///< A pool of all SVFValues in the SVFModule
    std::vector<cJSON*> valueArray;

public:
    SVFModule* get();
    SVFModuleRead(const std::string& path);
    ~SVFModuleRead();

private:
    SVFModule* readSvfModule(cJSON* iter);

    SVFType* indexToType(TypeIndex i);
    SVFValue* indexToValue(ValueIndex i);

    void fillSVFTypeAt(size_t i);
    void fillSVFValueAt(size_t i);

    StInfo* readStInfo(cJSON* iter);
    cJSON* readJson(cJSON* iter, SVFType* type);
    cJSON* readJson(cJSON* iter, SVFPointerType* type);
    cJSON* readJson(cJSON* iter, SVFIntegerType* type);
    cJSON* readJson(cJSON* iter, SVFFunctionType* type);
    cJSON* readJson(cJSON* iter, SVFStructType* type);
    cJSON* readJson(cJSON* iter, SVFArrayType* type);
    cJSON* readJson(cJSON* iter, SVFOtherType* type);

    SVFLoopAndDomInfo* readSvfLoopAndDomInfo(cJSON* iter);
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
