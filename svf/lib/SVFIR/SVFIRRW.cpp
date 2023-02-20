#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFIRRW.h"
#include "Util/CommandLine.h"

static const Option<bool> humanReadableOption(
    "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{

bool jsonAddNumberToObject(cJSON* obj, const char* name, double number)
{
    cJSON* node = cJSON_CreateNumber(number);
    return jsonAddItemToObject(obj, name, node);
}

bool jsonAddStringToObject(cJSON* obj, const char* name, const char* str)
{
    cJSON* node = cJSON_CreateStringReference(str);
    return jsonAddItemToObject(obj, name, node);
}

bool jsonAddStringToObject(cJSON* obj, const char* name,
                              const std::string& str)
{
    return jsonAddStringToObject(obj, name, str.c_str());
}

cJSON* jsonCreateObject()
{
    return humanReadableOption() ? cJSON_CreateObject() : cJSON_CreateArray();
}

cJSON* jsonCreateArray()
{
    return cJSON_CreateArray();
}

cJSON* jsonCreateMap()
{
    // Don't use cJSON_CreateObject() here, because it only allows string keys.
    return cJSON_CreateArray();
}

cJSON* jsonCreateString(const char* str)
{
    return cJSON_CreateStringReference(str);
}

bool jsonAddPairToMap(cJSON* mapObj, cJSON* key, cJSON* value)
{
    cJSON* pair = cJSON_CreateArray();
    cJSON_AddItemToArray(pair, key);
    cJSON_AddItemToArray(pair, value);
    cJSON_AddItemToArray(mapObj, pair);
    return pair;
}


bool jsonAddItemToObject(cJSON* obj, const char* name, cJSON* item)
{
    return humanReadableOption() ? cJSON_AddItemToObject(obj, name, item)
                                 : cJSON_AddItemToArray(obj, item);
}

bool jsonAddItemToArray(cJSON* array, cJSON* item)
{
    return cJSON_AddItemToArray(array, item);
}

SVFIRWriter::SVFIRWriter(const SVFIR* svfir)
    : svfIR(svfir), irGraphWriter(svfir), icfgWriter(svfir->getICFG())
{
}

const char* SVFIRWriter::numToStr(size_t n)
{
    auto it = numToStrMap.find(n);
    if (it != numToStrMap.end() && !(numToStrMap.key_comp()(n, it->first)))
    {
        return it->second.c_str();
    }
    return numToStrMap.emplace_hint(it, n, std::to_string(n))->second.c_str();
}

cJSON* SVFIRWriter::toJson()
{
    cJSON* root = jsonCreateObject();

    cJSON* nodeAllTypes = cJSON_CreateArray();
    jsonAddItemToObject(root, "typePool", nodeAllTypes);
    cJSON* nodeAllValues = cJSON_CreateArray();
    jsonAddItemToObject(root, "valuePool", nodeAllValues);

    JSON_WRITE_TOJSONABLE_FIELD(root, svfIR, svfModule);

    return root;
}

cJSON *SVFIRWriter::toJson(const SVFModule* module)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_STDSTRING_FIELD(root, module, pagReadFromTxt);
    JSON_WRITE_STDSTRING_FIELD(root, module, moduleIdentifier);

    JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, FunctionSet);
    JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, GlobalSet);
    JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, AliasSet);
    JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, ConstantSet);
    JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, OtherValueSet);

    return root;
}

} // namespace SVF