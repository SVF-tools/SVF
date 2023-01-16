#ifndef DUMP_METADATA_H_
#define DUMP_METADATA_H_

#include <vector>
#include <map>
#include <string>

#define JSON_DUMP_BOOL_FIELD(_root, _field)                                    \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateBool(_field);                        \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_NUMBER_FIELD(_root, _field)                                  \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateNumber(_field);                      \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_TYPE_FIELD(_info, _root, _field)                             \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field =                                                  \
            cJSON_CreateStringReference(_info.getStrTypeIndex(_field));        \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_VALUE_FIELD(_info, _root, _field)                            \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field =                                                  \
            cJSON_CreateStringReference(_info.getStrValueIndex(_field));       \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_STRING_FIELD(_root, _field)                                  \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateStringReference(_field.c_str());     \
        cJSON_AddItemToObjectCS(root, #_field, node##_field);                  \
    } while (0)

#define JSON_DUMP_VALUE_LIST_FIELD(_info, _root, _field)                       \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateArray();                             \
        for (const auto val : this->_field)                                    \
            cJSON_AddItemToArray(                                              \
                node##_field,                                                  \
                cJSON_CreateStringReference(_info.getStrValueIndex(val)));     \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_TYPE_LIST_FIELD(_info, _root, _field)                        \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateArray();                             \
        for (const auto ty : this->_field)                                     \
            cJSON_AddItemToArray(                                              \
                node##_field,                                                  \
                cJSON_CreateStringReference(_info.getStrTypeIndex(ty)));       \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

#define JSON_DUMP_NUMBER_LIST_FIELD(_root, _field)                             \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateArray();                             \
        for (const auto num : this->_field)                                    \
            cJSON_AddItemToArray(node##_field, cJSON_CreateNumber(num));       \
        cJSON_AddItemToObjectCS(_root, #_field, node##_field);                 \
    } while (0)

namespace SVF {

class SVFType;
class SVFValue;

typedef std::size_t TypeIndex;
typedef std::size_t ValueIndex;

struct DumpInfo {
    std::map<const SVFType*, TypeIndex> typeToIndex;
    std::vector<const SVFType*> allTypes;
    TypeIndex getTypeIndex(const SVFType* type);
    const char* getStrTypeIndex(const SVFType* type);

    std::map<const SVFValue*, ValueIndex> valueToIndex;
    std::vector<const SVFValue*> allValues;
    ValueIndex getValueIndex(const SVFValue* value);
    const char* getStrValueIndex(const SVFValue* value);

    std::vector<std::string> allIndices;


    const char* getStrOfIndex(std::size_t index);

    DumpInfo() {
        const std::size_t reserveSize = 10000;
        allTypes.reserve(reserveSize);
        allValues.reserve(reserveSize);
        allIndices.reserve(reserveSize);
    }
};

}

#endif // !DUMP_METADATA_H_