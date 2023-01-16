#include "SVFIR/SVFType.h"

using namespace SVF;

cJSON* StInfo::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_NUMBER_LIST_FIELD(root, fldIdxVec);

    JSON_DUMP_NUMBER_LIST_FIELD(root, elemIdxVec);

    cJSON* nodeFldMap = cJSON_CreateArray();
    for (const auto &pair : fldIdx2TypeMap)
    {
        cJSON_AddItemToObjectCS(
            nodeFldMap, dumpInfo.getStrOfIndex(pair.first),
            cJSON_CreateStringReference(dumpInfo.getStrTypeIndex(pair.second))
        );
    }
    cJSON_AddItemToObjectCS(root, "fldIdx2TypeMap", nodeFldMap);

    JSON_DUMP_TYPE_LIST_FIELD(dumpInfo, root, finfo);

    JSON_DUMP_NUMBER_FIELD(root, stride);

    JSON_DUMP_NUMBER_FIELD(root, numOfFlattenElements);

    JSON_DUMP_NUMBER_FIELD(root, numOfFlattenFields);

    JSON_DUMP_TYPE_LIST_FIELD(dumpInfo, root, flattenElementTypes);

    return root;
}

cJSON* SVFType::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_NUMBER_FIELD(root, kind);

    JSON_DUMP_TYPE_FIELD(dumpInfo, root, getPointerToTy);

    cJSON* nodeTypeInfo = typeinfo->toJson(dumpInfo);
    cJSON_AddItemToObjectCS(root, "typeinfo", nodeTypeInfo);

    JSON_DUMP_BOOL_FIELD(root, isSingleValTy);

    return root;
}

cJSON* SVFPointerType::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFType::toJson(dumpInfo);
    JSON_DUMP_TYPE_FIELD(dumpInfo, root, ptrElementType);
    return root;
}

cJSON* SVFIntegerType::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFType::toJson(dumpInfo);
}

cJSON* SVFFunctionType::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFType::toJson(dumpInfo);
    JSON_DUMP_TYPE_FIELD(dumpInfo, root, retTy);
    return root;
}

cJSON* SVFStructType::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFType::toJson(dumpInfo);
}

cJSON* SVFArrayType::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFType::toJson(dumpInfo);
}

cJSON* SVFOtherType::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFType::toJson(dumpInfo);
}