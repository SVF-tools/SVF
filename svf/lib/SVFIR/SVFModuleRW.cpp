#include "SVFIR/SVFModuleRW.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include <fstream>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace SVF;

/// @brief Helper function to create a new empty SVFType instance
static SVFType* createType(SVFType::SVFTyKind kind);
/// @brief Helper function to create a new empty SVFValue instance
static SVFValue* createValue(SVFValue::SVFValKind kind);

#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            SVFUtil::errs()                                                    \
                << __FILE__ << ":" << __LINE__ << ": " << reason << "\n";      \
            abort();                                                           \
        }                                                                      \
    } while (0)

/// Create a bool cJSON node, with bool value `objptr->field` and string name
/// #field, then attach it to the root cJSON node.
#define JSON_DUMP_BOOL(root, objptr, field)                                    \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateBool((objptr)->field);               \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Create a number cJSON node, with number `objptr->field` and string name
/// #field, then attach it to the root cJSON node.
#define JSON_DUMP_NUMBER(root, objptr, field)                                  \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateNumber((objptr)->field);             \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Get the string representation of the index of a SVFType pointer in the
/// SVFType pool (represeted by `typePool`), create a string cJSON node of it,
/// then attach it to the `root` cJSON node.
#define JSON_DUMP_SVFTYPE(root, objptr, field)                                 \
    do                                                                         \
    {                                                                          \
        const char* _strIdx = getStrTypeIndex((objptr)->field);                \
        cJSON* node_##field = cJSON_CreateStringReference(_strIdx);            \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Get the string representation of the index of a SVFValue pointer in the
/// SVFType pool (represeted by `valuePool`), create a string cJSON node of it,
/// then attach it to the `root` cJSON node.
#define JSON_DUMP_SVFVALUE(root, objptr, field)                                \
    do                                                                         \
    {                                                                          \
        const char* _strIdx = getStrValueIndex((objptr)->field);               \
        cJSON* node_##field = cJSON_CreateStringReference(_strIdx);            \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Create a string cJSON node with value `objptr->field.c_str()` and name
/// #field, then attach it to the `root` cJSON node.
#define JSON_DUMP_STRING(root, objptr, field)                                  \
    do                                                                         \
    {                                                                          \
        const char* _str = (objptr)->field.c_str();                            \
        cJSON* node_##field = cJSON_CreateStringReference(_str);               \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Create a cJSON array with name #field and contents in `obj->field` where
/// each element is a SVFValue pointer, and then attach it to the `root` cJSON
/// node.
#define JSON_DUMP_CONTAINER_OF_SVFVALUE(root, obj, field)                      \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateArray();                             \
        for (const auto val : (obj)->field)                                    \
            cJSON_AddItemToArray(node_##field, cJSON_CreateStringReference(    \
                                                   getStrValueIndex(val)));    \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Create a cJSON array with name #field and contents in `obj->field` where
/// each element is a SVFType pointer, and then attach it to the `root` cJSON
/// node.
#define JSON_DUMP_CONTAINER_OF_SVFTYPE(root, obj, field)                       \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateArray();                             \
        for (const auto ty : (obj)->field)                                     \
            cJSON_AddItemToArray(node_##field, cJSON_CreateStringReference(    \
                                                   getStrTypeIndex(ty)));      \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

/// Create a cJSON array with name #field and contents in `obj->field` where
/// each element is a number, and then attach it to the `root` cJSON node.
#define JSON_DUMP_CONTAINER_OF_NUMBER(root, obj, field)                        \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateArray();                             \
        for (const auto num : (obj)->field)                                    \
            cJSON_AddItemToArray(node_##field, cJSON_CreateNumber(num));       \
        cJSON_AddItemToObjectCS((root), #field, node_##field);                 \
    } while (0)

SVFModuleWrite::SVFModuleWrite(const SVFModule* module)
    : module(module), jsonStr(nullptr)
{
    const std::size_t reserveSize =
        module->getFunctionSet().size() + module->getGlobalSet().size() +
        module->getAliasSet().size() + module->getConstantSet().size() +
        module->getConstantSet().size() +
        module->getOtherValueSet().size(); // Estimated size

    typePool.reserve(reserveSize);
    valuePool.reserve(reserveSize);
    allIndices.reserve(1000);
    getStrOfIndex(0);
}

SVFModuleWrite::SVFModuleWrite(const SVFModule* module, const std::string& path)
    : SVFModuleWrite(module)
{
    dumpJsonToPath(path);
}

SVFModuleWrite::~SVFModuleWrite()
{
    if (jsonStr)
    {
        cJSON_free((void*)jsonStr);
    }
}

void SVFModuleWrite::dumpJsonToPath(const std::string& path)
{
    std::ofstream jsonFile(path);
    if (jsonFile.is_open())
    {
        dumpJsonToOstream(jsonFile);
        jsonFile.close();
    }
    else
    {
        SVFUtil::errs() << "Failed to open '" << path
                        << "' to dump SVFModule\n";
    }
}

void SVFModuleWrite::dumpJsonToOstream(std::ostream& os)
{
    if (!jsonStr)
    {
        cJSON* json = moduleToJson(module);
        jsonStr = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
    }
    os << jsonStr << std::endl;
}

TypeIndex SVFModuleWrite::getTypeIndex(const SVFType* type)
{
    if (type == nullptr)
        return 0;
    auto pair = typeToIndex.emplace(type, 1 + typePool.size());
    if (pair.second)
    {
        // This SVFType pointer was NOT recorded before. It gets inserted.
        typePool.push_back(type);
    }
    return pair.first->second;
}

ValueIndex SVFModuleWrite::getValueIndex(const SVFValue* value)
{
    if (value == nullptr)
        return 0;
    auto pair = valueToIndex.emplace(value, 1 + valuePool.size());
    if (pair.second)
    {
        // The SVFValue pointer was NOT recorded before. It gets inserted.
        valuePool.push_back(value);
    }
    return pair.first->second;
}

const char* SVFModuleWrite::getStrOfIndex(std::size_t index)
{
    // Invariant: forall i: allIndices[i] == hex(i) /\ len(allIndices) == N
    for (std::size_t i = allIndices.size(); i <= index; ++i)
    {
        allIndices.emplace_back(
            std::make_unique<std::string>(std::to_string(i)));
    }

    return allIndices[index]->c_str();
    // Postcondition: ensures len(allIndices) >= index + 1
}

const char* SVFModuleWrite::getStrValueIndex(const SVFValue* value)
{
    return getStrOfIndex(getValueIndex(value));
}

const char* SVFModuleWrite::getStrTypeIndex(const SVFType* type)
{
    return getStrOfIndex(getTypeIndex(type));
}

cJSON* SVFModuleWrite::moduleToJson(const SVFModule* module)
{
    // Top-level json creation
    cJSON* root = cJSON_CreateObject();

    cJSON* nodeAllTypes = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(root, "typePool", nodeAllTypes);
    cJSON* nodeAllValues = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(root, "valuePool", nodeAllValues);

    JSON_DUMP_STRING(root, module, pagReadFromTxt);
    JSON_DUMP_STRING(root, module, moduleIdentifier);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, FunctionSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, GlobalSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, AliasSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, ConstantSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, OtherValueSet);

    // N.B. Be sure to use index-based loop instead of iterater based loop
    // (including range-based loop). Because all `toJson()` functions may append
    // new elements to the end of the vectors and the vector buffer might get
    // reallocated, thus invalidating all old iterators.
    for (size_t i = 0; i < valuePool.size(); ++i)
    {
        cJSON* nodeVal = valueToJson(valuePool[i]);
        cJSON_AddItemToArray(nodeAllValues, nodeVal);
    }

    for (size_t i = 0; i < typePool.size(); ++i)
    {
        cJSON* nodeType = typeToJson(typePool[i]);
        cJSON_AddItemToArray(nodeAllTypes, nodeType);
    }

    return root;
}

cJSON* SVFModuleWrite::toJson(const StInfo* stInfo)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_CONTAINER_OF_NUMBER(root, stInfo, fldIdxVec);
    JSON_DUMP_CONTAINER_OF_NUMBER(root, stInfo, elemIdxVec);

    cJSON* nodeFldMap = cJSON_CreateObject();
    for (const auto& pair : stInfo->fldIdx2TypeMap)
    {
        const char* key = getStrOfIndex(pair.first);
        cJSON* ty = cJSON_CreateStringReference(getStrTypeIndex(pair.second));
        cJSON_AddItemToObjectCS(nodeFldMap, key, ty);
    }
    cJSON_AddItemToObjectCS(root, "fldIdx2TypeMap", nodeFldMap);

    JSON_DUMP_CONTAINER_OF_SVFTYPE(root, stInfo, finfo);
    JSON_DUMP_NUMBER(root, stInfo, stride);
    JSON_DUMP_NUMBER(root, stInfo, numOfFlattenElements);
    JSON_DUMP_NUMBER(root, stInfo, numOfFlattenFields);
    JSON_DUMP_CONTAINER_OF_SVFTYPE(root, stInfo, flattenElementTypes);

    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFType* type)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_NUMBER(root, type, kind);
    JSON_DUMP_SVFTYPE(root, type, getPointerToTy);

    cJSON* nodeTypeInfo = toJson(type->typeinfo);
    cJSON_AddItemToObjectCS(root, "typeinfo", nodeTypeInfo);

    JSON_DUMP_BOOL(root, type, isSingleValTy);

    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFPointerType* type)
{
    cJSON* root = toJson(static_cast<const SVFType*>(type));
    JSON_DUMP_SVFTYPE(root, type, ptrElementType);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFIntegerType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleWrite::toJson(const SVFFunctionType* type)
{
    cJSON* root = toJson(static_cast<const SVFType*>(type));
    JSON_DUMP_SVFTYPE(root, type, retTy);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFStructType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleWrite::toJson(const SVFArrayType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleWrite::toJson(const SVFOtherType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleWrite::typeToJson(const SVFType* type)
{
    using SVFUtil::dyn_cast;
    auto kind = type->getKind();

    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFType kind " << kind
                        << " in typeToJson()\n";
        assert(false);

#define CASE(Kind)                                                             \
    case SVFType::Kind:                                                        \
        return toJson(SVFUtil::dyn_cast<Kind##pe>(type));

        CASE(SVFTy);
        CASE(SVFPointerTy);
        CASE(SVFIntegerTy);
        CASE(SVFFunctionTy);
        CASE(SVFStructTy);
        CASE(SVFArrayTy);
        CASE(SVFOtherTy);
#undef CASE
    }
}

cJSON* SVFModuleWrite::toJson(const SVFLoopAndDomInfo* ldInfo)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, ldInfo, reachableBBs);

#define JSON_DUMP_BB_MAP(field)                                                \
    do                                                                         \
    {                                                                          \
        cJSON* node_##field = cJSON_CreateObject();                            \
        for (const auto& BB2BBs : (ldInfo)->field)                             \
        {                                                                      \
            cJSON* nodeBBs = cJSON_CreateArray();                              \
            for (const SVFBasicBlock* bb : BB2BBs.second)                      \
                cJSON_AddItemToArray(nodeBBs, cJSON_CreateStringReference(     \
                                                  getStrValueIndex(bb)));      \
            cJSON_AddItemToObjectCS(node_##field,                              \
                                    getStrValueIndex(BB2BBs.first), nodeBBs);  \
        }                                                                      \
        cJSON_AddItemToObjectCS(root, #field, node_##field);                   \
    } while (0)
    JSON_DUMP_BB_MAP(dtBBsMap);
    JSON_DUMP_BB_MAP(pdtBBsMap);
    JSON_DUMP_BB_MAP(dfBBsMap);
    JSON_DUMP_BB_MAP(bb2LoopMap);
#undef BB_MAP_TO_JSON

    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFValue* value)
{
    cJSON* root = cJSON_CreateObject();
    JSON_DUMP_NUMBER(root, value, kind);
    JSON_DUMP_BOOL(root, value, ptrInUncalledFun);
    JSON_DUMP_BOOL(root, value, constDataOrAggData);
    JSON_DUMP_SVFTYPE(root, value, type);
    JSON_DUMP_STRING(root, value, name);
    JSON_DUMP_STRING(root, value, sourceLoc);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFFunction* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));

    // isDecl
    JSON_DUMP_BOOL(root, value, isDecl);
    // intrinsic
    JSON_DUMP_BOOL(root, value, intrinsic);
    // addrTaken
    JSON_DUMP_BOOL(root, value, addrTaken);
    // isUncalled
    JSON_DUMP_BOOL(root, value, isUncalled);
    // isNotRet
    JSON_DUMP_BOOL(root, value, isNotRet);
    // varArg
    JSON_DUMP_BOOL(root, value, varArg);
    // funcType
    JSON_DUMP_SVFTYPE(root, value, funcType);
    // loopAndDom
    cJSON* nodeLD = toJson(value->loopAndDom);
    cJSON_AddItemToObjectCS(root, "loopAndDom", nodeLD);
    // realDefFun
    JSON_DUMP_SVFVALUE(root, value, realDefFun);
    // allBBs
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, allBBs);
    // owns allArgs
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, allArgs);

    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFBasicBlock* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, allInsts);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, succBBs);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, predBBs);
    JSON_DUMP_SVFVALUE(root, value, fun);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFInstruction* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_SVFVALUE(root, value, bb);
    JSON_DUMP_BOOL(root, value, terminator);
    JSON_DUMP_BOOL(root, value, ret);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, succInsts);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, predInsts);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFCallInst* value)
{
    cJSON* root = toJson(static_cast<const SVFInstruction*>(value));
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, args);
    JSON_DUMP_BOOL(root, value, varArg);
    JSON_DUMP_SVFVALUE(root, value, calledVal);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFVirtualCallInst* value)
{
    cJSON* root = toJson(static_cast<const SVFCallInst*>(value));
    JSON_DUMP_SVFVALUE(root, value, vCallVtblPtr);
    JSON_DUMP_NUMBER(root, value, virtualFunIdx);
    JSON_DUMP_STRING(root, value, funNameOfVcall);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFConstant* value)
{
    return toJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFModuleWrite::toJson(const SVFGlobalValue* value)
{
    cJSON* root = toJson(static_cast<const SVFConstant*>(value));
    JSON_DUMP_SVFVALUE(root, value, realDefGlobal);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFArgument* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_SVFVALUE(root, value, fun);
    JSON_DUMP_NUMBER(root, value, argNo);
    JSON_DUMP_BOOL(root, value, uncalled);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFConstantData* value)
{
    return toJson(static_cast<const SVFConstant*>(value));
}

cJSON* SVFModuleWrite::toJson(const SVFConstantInt* value)
{
    cJSON* root = toJson(static_cast<const SVFConstantData*>(value));
    JSON_DUMP_NUMBER(root, value, zval);
    JSON_DUMP_NUMBER(root, value, sval);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFConstantFP* value)
{
    cJSON* root = toJson(static_cast<const SVFConstantData*>(value));
    JSON_DUMP_NUMBER(root, value, dval);
    return root;
}

cJSON* SVFModuleWrite::toJson(const SVFConstantNullPtr* value)
{
    return toJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFModuleWrite::toJson(const SVFBlackHoleValue* value)
{
    return toJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFModuleWrite::toJson(const SVFOtherValue* value)
{
    return toJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFModuleWrite::toJson(const SVFMetadataAsValue* value)
{
    return toJson(static_cast<const SVFOtherValue*>(value));
}

cJSON* SVFModuleWrite::valueToJson(const SVFValue* value)
{
    using SVFUtil::dyn_cast;
    auto kind = value->getKind();

    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFValue kind in" << kind
                        << " in typeToJson()\n";
        assert(false);

#define CASE(kind, type)                                                       \
    case SVFValue::kind:                                                       \
        return toJson(SVFUtil::dyn_cast<type>(value));
        CASE(SVFVal, SVFValue);
        CASE(SVFFunc, SVFFunction);
        CASE(SVFBB, SVFBasicBlock);
        CASE(SVFInst, SVFInstruction);
        CASE(SVFCall, SVFCallInst);
        CASE(SVFVCall, SVFVirtualCallInst);
        CASE(SVFGlob, SVFGlobalValue);
        CASE(SVFArg, SVFArgument);
        CASE(SVFConst, SVFConstant);
        CASE(SVFConstData, SVFConstantData);
        CASE(SVFConstInt, SVFConstantInt);
        CASE(SVFConstFP, SVFConstantFP);
        CASE(SVFNullPtr, SVFConstantNullPtr);
        CASE(SVFBlackHole, SVFBlackHoleValue);
        CASE(SVFMetaAsValue, SVFMetadataAsValue);
        CASE(SVFOther, SVFOtherValue);
#undef CASE
    }
}

#define JSON_READ_VECTOR_OF_NUMBER(iter, obj, field)                           \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsArray(iter) && !std::strcmp(#field, (iter)->string)),     \
            #field " expects `" #iter "` to be an array");                     \
        cJSON* _element;                                                       \
        cJSON_ArrayForEach(_element, (iter))                                   \
        {                                                                      \
            ABORT_IFNOT(cJSON_IsNumber(_element),                              \
                        #field " expects numbers in object `" #iter "`");      \
            using _T = decltype(obj->field)::value_type;                       \
            (obj)->field.push_back(static_cast<_T>(_element->valuedouble));    \
        }                                                                      \
        iter = (iter)->next;                                                   \
    } while (0)

#define JSON_READ_VECTOR_OF_SVFREF(iter, obj, field, type)                     \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsArray(iter) && !std::strcmp(#field, (iter)->string)),     \
            #field " expects `" #field "` to be an array");                    \
        cJSON* _element;                                                       \
        cJSON_ArrayForEach(_element, (iter))                                   \
        {                                                                      \
            ABORT_IFNOT(cJSON_IsString(_element),                              \
                        #field " expects string indices in `" #iter "`");      \
            SVF##type* _v = indexTo##type(std::atoi(_element->valuestring));   \
            ABORT_IFNOT(_v, "Some index in `" #iter "` refers to NULL");       \
            using _T = std::decay_t<decltype(*(obj)->field[0])>;               \
            _T* _t = SVFUtil::dyn_cast<_T>(_v);                                \
            ABORT_IFNOT(_t,                                                    \
                        "dyn_cast from SVF" #type "* for " #field " failed");  \
            (obj)->field.push_back(_t);                                        \
        }                                                                      \
        iter = (iter)->next;                                                   \
    } while (0)

#define JSON_READ_VECTOR_OF_SVFVALUE(iter, obj, field)                         \
    JSON_READ_VECTOR_OF_SVFREF(iter, obj, field, Value)

#define JSON_READ_VECTOR_OF_SVFTYPE(iter, obj, field)                          \
    JSON_READ_VECTOR_OF_SVFREF(iter, obj, field, Type)

#define JSON_READ_SVFREF(iter, obj, field, type)                               \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsString(iter) && !std::strcmp(#field, (iter)->string)),    \
            #field " expects `" #iter "` to be a " #type "Index JSON string"); \
        SVF##type* _p = indexTo##type(std::atoi((iter)->valuestring));         \
        using _T = std::decay_t<decltype(*(obj)->field)>;                      \
        ABORT_IFNOT(_p, "Some index in `" #iter "` refers to NULL");           \
        _T* _t = SVFUtil::dyn_cast<_T>(_p);                                    \
        ABORT_IFNOT(_t, "dyn_cast from SVF" #type "* for " #field " failed");  \
        (obj)->field = _t;                                                     \
        iter = (iter)->next;                                                   \
    } while (0)

#define JSON_READ_SVFTYPE(iter, obj, field)                                    \
    JSON_READ_SVFREF(iter, obj, field, Type)

#define JSON_READ_SVFVALUE(iter, obj, field)                                   \
    JSON_READ_SVFREF(iter, obj, field, Value)

#define JSON_READ_STRING(iter, obj, field)                                     \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsString(iter) && !std::strcmp(#field, (iter)->string)),    \
            #field " expects  `" #iter "` to be a JSON string");               \
        (obj)->field = (iter)->valuestring;                                    \
        iter = (iter)->next;                                                   \
    } while (0)

#define JSON_READ_NUMBER(iter, obj, field)                                     \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsNumber(iter) && !std::strcmp(#field, (iter)->string)),    \
            #field " expects `" #iter "` to be a JSON number");                \
        (obj)->field =                                                         \
            static_cast<decltype((obj)->field)>((iter)->valuedouble);          \
        iter = (iter)->next;                                                   \
    } while (0)

#define JSON_READ_BOOL(iter, obj, field)                                       \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsBool(iter) && !std::strcmp(#field, (iter)->string)),      \
            #field " expects `" #field "` to be a JSON bool");                 \
        (obj)->field = static_cast<bool>(cJSON_IsTrue(iter));                  \
        iter = (iter)->next;                                                   \
    } while (0)

SVFType* createType(SVFType::SVFTyKind kind)
{
    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFTyKind " << kind
                        << " in createType()\n";
        assert(false);
    case SVFType::SVFTy:
        SVFUtil::errs() << "Construction of RAW SVFType isn't allowed\n";
        assert(false);
    case SVFType::SVFPointerTy:
        return new SVFPointerType({});
    case SVFType::SVFIntegerTy:
        return new SVFIntegerType();
    case SVFType::SVFFunctionTy:
        return new SVFFunctionType({});
    case SVFType::SVFStructTy:
        return new SVFStructType();
    case SVFType::SVFArrayTy:
        return new SVFArrayType();
    case SVFType::SVFOtherTy:
        return new SVFOtherType({});
    }
}

SVFValue* createValue(SVFValue::SVFValKind kind)
{
    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFValue kind " << kind
                        << " in createValue()\n";
        assert(false);
    case SVFValue::SVFVal:
        SVFUtil::errs() << "Creation of RAW SVFValue isn't allowed\n";
        assert(false);
    case SVFValue::SVFFunc:
        return new SVFFunction({}, {}, {}, {}, {}, {}, {}, {});
    case SVFValue::SVFBB:
        return new SVFBasicBlock({}, {}, {});
    case SVFValue::SVFInst:
        return new SVFInstruction({}, {}, {}, {}, {});
    case SVFValue::SVFCall:
        return new SVFCallInst({}, {}, {}, {}, {});
    case SVFValue::SVFVCall:
        return new SVFVirtualCallInst({}, {}, {}, {}, {});
    case SVFValue::SVFGlob:
        return new SVFGlobalValue({}, {});
    case SVFValue::SVFArg:
        return new SVFArgument({}, {}, {}, {}, {});
    case SVFValue::SVFConst:
        return new SVFConstant({}, {});
    case SVFValue::SVFConstData:
        return new SVFConstantData({}, {});
    case SVFValue::SVFConstInt:
        return new SVFConstantInt({}, {}, {}, {});
    case SVFValue::SVFConstFP:
        return new SVFConstantFP({}, {}, {});
    case SVFValue::SVFNullPtr:
        return new SVFConstantNullPtr({}, {});
    case SVFValue::SVFBlackHole:
        return new SVFBlackHoleValue({}, {});
    case SVFValue::SVFMetaAsValue:
        return new SVFMetadataAsValue({}, {});
    case SVFValue::SVFOther:
        return new SVFOtherValue({}, {});
    }
}

SVFModule* SVFModuleRead::get()
{
    if (!svfModule)
    {
        ABORT_IFNOT(cJSON_IsObject(moduleJson), "Invalid moduleJson");
        svfModule = readSvfModule(moduleJson->child);
    }
    return svfModule;
}

SVFModule* SVFModuleRead::readSvfModule(cJSON* iter)
{
    cJSON* element;

    // Read typePool
    ABORT_IFNOT((cJSON_IsArray(iter) && !std::strcmp("typePool", iter->string)),
                "Module's first child should be a typePool array");
    cJSON_ArrayForEach(element, iter)
    {
        ABORT_IFNOT(cJSON_IsObject(element),
                    "Element in typePool is not a json object");
        typeArray.push_back(element);
    }
    typePool.reserve(typeArray.size());
    for (const cJSON* typeElement : typeArray)
    {
        cJSON* elementChild = typeElement->child;
        ABORT_IFNOT((cJSON_IsNumber(elementChild) &&
                     !std::strcmp("kind", elementChild->string)),
                    "Type JSON's 1st element is not kind number");
        auto kind = static_cast<SVFType::SVFTyKind>(elementChild->valuedouble);
        typePool.push_back(createType(kind));
    }
    iter = iter->next;

    // Read valuePool
    ABORT_IFNOT(
        (cJSON_IsArray(iter) && !std::strcmp("valuePool", iter->string)),
        "Module's 2nd child should be valuePool array");
    cJSON_ArrayForEach(element, iter)
    {
        ABORT_IFNOT(cJSON_IsObject(element),
                    "Element in valuePool is not json object");
        valueArray.push_back(element);
    }
    valuePool.reserve(valueArray.size());
    for (const cJSON* valueElement : valueArray)
    {
        cJSON* elementChild = valueElement->child;
        ABORT_IFNOT((cJSON_IsNumber(elementChild) &&
                     !std::strcmp("kind", elementChild->string)),
                    "Value JSON's 1st element is not kind number");
        auto kind =
            static_cast<SVFValue::SVFValKind>(elementChild->valuedouble);
        valuePool.push_back(createValue(kind));
    }
    iter = iter->next;

    // Read pagReadFromTxt
    ABORT_IFNOT(
        (cJSON_IsString(iter) && !std::strcmp("pagReadFromTxt", iter->string)),
        "Module's 3rd child should be pagReadFromTxt string");
    const char* pagReadFromTxt = iter->valuestring;
    iter = iter->next;

    // Read moduleIdentifier
    ABORT_IFNOT((cJSON_IsString(iter) &&
                 !std::strcmp("moduleIdentifier", iter->string)),
                "Module's 3rd child should be moduleIdentifier string");

    SVFModule* svfModule = new SVFModule(iter->valuestring);
    svfModule->setPagFromTXT(pagReadFromTxt);
    iter = iter->next;

    // Read other stuff
    JSON_READ_VECTOR_OF_SVFVALUE(iter, svfModule, FunctionSet);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, svfModule, GlobalSet);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, svfModule, AliasSet);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, svfModule, ConstantSet);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, svfModule, OtherValueSet);

    ABORT_IFNOT(iter == nullptr, "Module has more children than expected");

    // Fill incomplete values in valuePool and typePool
    for (size_t i = 0; i < typePool.size(); ++i)
        fillSVFTypeAt(i);
    for (size_t i = 0; i < valuePool.size(); ++i)
        fillSVFValueAt(i);

    return svfModule;
}

void SVFModuleRead::fillSVFTypeAt(size_t i)
{
    cJSON* childIter = typeArray[i]->child;
    SVFType* type = typePool[i];
    auto kind = type->getKind();

    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFType kind " << kind
                        << " in fillSVFTypeAt()\n";
        assert(false);

#define CASE(Kind)                                                             \
    case SVFType::Kind: {                                                      \
        auto _iter =                                                           \
            readJson(childIter->next, SVFUtil::dyn_cast<Kind##pe>(type));      \
        ABORT_IFNOT(_iter == nullptr, #Kind " elements left unread");          \
        break;                                                                 \
    }
        CASE(SVFTy);
        CASE(SVFPointerTy);
        CASE(SVFIntegerTy);
        CASE(SVFFunctionTy);
        CASE(SVFStructTy);
        CASE(SVFArrayTy);
        CASE(SVFOtherTy);
#undef CASE
    }
}

void SVFModuleRead::fillSVFValueAt(size_t i)
{
    cJSON* childIter = valueArray[i]->child;
    SVFValue* value = valuePool[i];
    auto kind = value->getKind();
    switch (kind)
    {
    default:
        SVFUtil::errs() << "Impossible SVFValue kind " << kind
                        << " in fillSVFValueAt()\n";
        assert(false);

#define CASE(kind, type)                                                       \
    case SVFValue::kind: {                                                     \
        auto _iter =                                                           \
            readJson(childIter->next, SVFUtil::dyn_cast<type>(value));         \
        ABORT_IFNOT(_iter == nullptr, #type " elements left unread");          \
        break;                                                                 \
    }
        CASE(SVFVal, SVFValue);
        CASE(SVFFunc, SVFFunction);
        CASE(SVFBB, SVFBasicBlock);
        CASE(SVFInst, SVFInstruction);
        CASE(SVFCall, SVFCallInst);
        CASE(SVFVCall, SVFVirtualCallInst);
        CASE(SVFGlob, SVFGlobalValue);
        CASE(SVFArg, SVFArgument);
        CASE(SVFConst, SVFConstant);
        CASE(SVFConstData, SVFConstantData);
        CASE(SVFConstInt, SVFConstantInt);
        CASE(SVFConstFP, SVFConstantFP);
        CASE(SVFNullPtr, SVFConstantNullPtr);
        CASE(SVFBlackHole, SVFBlackHoleValue);
        CASE(SVFMetaAsValue, SVFMetadataAsValue);
        CASE(SVFOther, SVFOtherValue);
#undef CASE
    }
}

SVFType* SVFModuleRead::indexToType(TypeIndex i)
{
    ABORT_IFNOT(i <= typePool.size(), "TypeIndex too large");
    return i ? typePool[i - 1] : nullptr;
}

SVFValue* SVFModuleRead::indexToValue(ValueIndex i)
{
    ABORT_IFNOT(i <= valuePool.size(), "ValueIndex too large");
    return i ? valuePool[i - 1] : nullptr;
}

StInfo* SVFModuleRead::readStInfo(cJSON* iter)
{
    StInfo* info = new StInfo({});

    JSON_READ_VECTOR_OF_NUMBER(iter, info, fldIdxVec);
    JSON_READ_VECTOR_OF_NUMBER(iter, info, elemIdxVec);

    // Read map of StInfo::fldIdx2TypeMap (u32_t -> SVFType*)
    ABORT_IFNOT(
        (cJSON_IsObject(iter) && !std::strcmp("fldIdx2TypeMap", iter->string)),
        "fldIdx2TypeMap expects an array");
    cJSON* element;
    cJSON_ArrayForEach(element, iter)
    {
        ABORT_IFNOT(cJSON_IsString(element),
                    "fldIdx2TypeMap expects TypeIndex strings in array");
        info->fldIdx2TypeMap.emplace(
            std::atoi(element->string),
            indexToType(std::atoi(element->valuestring)));
    }
    iter = iter->next;

    JSON_READ_VECTOR_OF_SVFTYPE(iter, info, finfo);
    JSON_READ_NUMBER(iter, info, stride);
    JSON_READ_NUMBER(iter, info, numOfFlattenElements);
    JSON_READ_NUMBER(iter, info, numOfFlattenFields);
    JSON_READ_VECTOR_OF_SVFTYPE(iter, info, flattenElementTypes);
    return info;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFType* type)
{

    JSON_READ_SVFREF(iter, type, getPointerToTy, Type);

    // Read typeinfo
    ABORT_IFNOT(
        (cJSON_IsObject(iter) && !std::strcmp("typeinfo", iter->string)),
        "Field should be a typeinfo JSON object");
    type->typeinfo = readStInfo(iter->child);
    iter = iter->next;

    JSON_READ_BOOL(iter, type, isSingleValTy);

    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFPointerType* type)
{
    iter = readJson(iter, static_cast<SVFType*>(type));
    JSON_READ_SVFTYPE(iter, type, ptrElementType);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFIntegerType* type)
{
    return readJson(iter, static_cast<SVFType*>(type));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFFunctionType* type)
{
    iter = readJson(iter, static_cast<SVFType*>(type));
    JSON_READ_SVFTYPE(iter, type, retTy);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFStructType* type)
{
    return readJson(iter, static_cast<SVFType*>(type));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFArrayType* type)
{
    return readJson(iter, static_cast<SVFType*>(type));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFOtherType* type)
{
    return readJson(iter, static_cast<SVFType*>(type));
}

SVFLoopAndDomInfo* SVF::SVFModuleRead::readSvfLoopAndDomInfo(cJSON* iter)
{
    auto ldInfo = new SVFLoopAndDomInfo;
    JSON_READ_VECTOR_OF_SVFVALUE(iter, ldInfo, reachableBBs);

#define JSON_READ_BB_MAP(field)                                                \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(                                                           \
            (cJSON_IsObject(iter) && !std::strcmp(#field, iter->string)),      \
            #field " expects a JSON object to represent mapping");             \
        cJSON *_kvJson, *_vJson;                                               \
        cJSON_ArrayForEach(_kvJson, iter)                                      \
        {                                                                      \
            ABORT_IFNOT(cJSON_IsArray(_kvJson),                                \
                        "Elements in " #field " should be BB index array");    \
            SVFValue* _key = indexToValue(std::atoi(_kvJson->string));         \
            ABORT_IFNOT(_key, "Some key BB index refers to NULL");             \
            SVFBasicBlock* _bbKey = SVFUtil::dyn_cast<SVFBasicBlock>(_key);    \
            ABORT_IFNOT(_bbKey,                                                \
                        "Some key index in " #field " is not a BasicBlock*");  \
            auto& _fieldRef = ldInfo->field[_bbKey];                           \
            cJSON_ArrayForEach(_vJson, _kvJson)                                \
            {                                                                  \
                ABORT_IFNOT(cJSON_IsString(_vJson),                            \
                            "Elements in " #field                              \
                            " array should be a ValueIndex string");           \
                SVFValue* _val = indexToValue(std::atoi(_vJson->valuestring)); \
                ABORT_IFNOT(_val, "Some val BB index refers to NULL");         \
                SVFBasicBlock* _bb = SVFUtil::dyn_cast<SVFBasicBlock>(_val);   \
                ABORT_IFNOT(_bb, "Some Value* in " #field                      \
                                 " array is not a BasicBlock*");               \
                _fieldRef.insert(_fieldRef.end(), _bb);                        \
            }                                                                  \
        }                                                                      \
        iter = iter->next;                                                     \
    } while (0);

    JSON_READ_BB_MAP(dtBBsMap);
    JSON_READ_BB_MAP(pdtBBsMap);
    JSON_READ_BB_MAP(dfBBsMap);
    JSON_READ_BB_MAP(bb2LoopMap);

#undef JSON_READ_BB_MAP
    return ldInfo;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFValue* value)
{
    JSON_READ_BOOL(iter, value, ptrInUncalledFun);
    JSON_READ_BOOL(iter, value, constDataOrAggData);
    JSON_READ_SVFTYPE(iter, value, type);
    JSON_READ_STRING(iter, value, name);
    JSON_READ_STRING(iter, value, sourceLoc);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFFunction* value)
{
    iter = readJson(iter, static_cast<SVFValue*>(value));
    JSON_READ_BOOL(iter, value, isDecl);
    JSON_READ_BOOL(iter, value, intrinsic);
    JSON_READ_BOOL(iter, value, addrTaken);
    JSON_READ_BOOL(iter, value, isUncalled);
    JSON_READ_BOOL(iter, value, isNotRet);
    JSON_READ_BOOL(iter, value, varArg);
    JSON_READ_SVFTYPE(iter, value, funcType);
    // Read Loop and Dom
    ABORT_IFNOT(
        (cJSON_IsObject(iter) && !std::strcmp("loopAndDom", iter->string)),
        "Expect a `loopAndDom' json object");
    value->loopAndDom = readSvfLoopAndDomInfo(iter->child);
    iter = iter->next;
    // Others
    JSON_READ_SVFVALUE(iter, value, realDefFun);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, allBBs);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, allArgs);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFBasicBlock* value)
{
    iter = readJson(iter, static_cast<SVFValue*>(value));
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, allInsts);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, succBBs);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, predBBs);
    JSON_READ_SVFVALUE(iter, value, fun);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFInstruction* value)
{
    iter = readJson(iter, static_cast<SVFValue*>(value));
    JSON_READ_SVFVALUE(iter, value, bb);
    JSON_READ_BOOL(iter, value, terminator);
    JSON_READ_BOOL(iter, value, ret);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, succInsts);
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, predInsts);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFCallInst* value)
{
    iter = readJson(iter, static_cast<SVFInstruction*>(value));
    JSON_READ_VECTOR_OF_SVFVALUE(iter, value, args);
    JSON_READ_BOOL(iter, value, varArg);
    JSON_READ_SVFVALUE(iter, value, calledVal);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFVirtualCallInst* value)
{
    iter = readJson(iter, static_cast<SVFCallInst*>(value));
    JSON_READ_SVFVALUE(iter, value, vCallVtblPtr);
    JSON_READ_NUMBER(iter, value, virtualFunIdx);
    JSON_READ_STRING(iter, value, funNameOfVcall);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFConstant* value)
{
    return readJson(iter, static_cast<SVFValue*>(value));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFGlobalValue* value)
{
    iter = readJson(iter, static_cast<SVFConstant*>(value));
    JSON_READ_SVFVALUE(iter, value, realDefGlobal);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFArgument* value)
{
    iter = readJson(iter, static_cast<SVFValue*>(value));
    JSON_READ_SVFVALUE(iter, value, fun);
    JSON_READ_NUMBER(iter, value, argNo);
    JSON_READ_BOOL(iter, value, uncalled);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFConstantData* value)
{
    return readJson(iter, static_cast<SVFConstant*>(value));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFConstantInt* value)
{
    iter = readJson(iter, static_cast<SVFConstantData*>(value));
    JSON_READ_NUMBER(iter, value, zval);
    JSON_READ_NUMBER(iter, value, sval);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFConstantFP* value)
{
    iter = readJson(iter, static_cast<SVFConstantData*>(value));
    JSON_READ_NUMBER(iter, value, dval);
    return iter;
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFConstantNullPtr* value)
{
    return readJson(iter, static_cast<SVFConstantData*>(value));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFBlackHoleValue* value)
{
    return readJson(iter, static_cast<SVFConstantData*>(value));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFOtherValue* value)
{
    return readJson(iter, static_cast<SVFValue*>(value));
}

cJSON* SVFModuleRead::readJson(cJSON* iter, SVFMetadataAsValue* value)
{
    return readJson(iter, static_cast<SVFOtherValue*>(value));
}

SVFModuleRead::SVFModuleRead(const std::string& path) : svfModule(nullptr)
{
    struct stat buf;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        std::string info = "open(\"" + path + "\")";
        perror(info.c_str());
        abort();
    }
    if (fstat(fd, &buf) == -1)
    {
        std::string info = "fstate(\"" + path + "\")";
        perror(info.c_str());
        abort();
    }
    auto addr =
        (char*)mmap(nullptr, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
    {
        std::string info = "mmap(content of \"" + path + "\")";
        perror(info.c_str());
        abort();
    }

    moduleJson = cJSON_ParseWithLength(addr, buf.st_size);

    if (munmap(addr, buf.st_size) == -1)
        perror("munmap()");

    if (close(fd) < 0)
        perror("close()");
}

SVFModuleRead::~SVFModuleRead()
{
    cJSON_Delete(moduleJson);
}
