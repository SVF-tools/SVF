#include "SVFIR/SVFModuleJsonDumper.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include <fstream>

using namespace SVF;

/// Create a bool cJSON node, with bool value `_objptr->_field` and string name
/// #_field, then attach it to the _root cJSON node.
#define JSON_DUMP_BOOL(_root, _objptr, _field)                                 \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateBool((_objptr)->_field);            \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Create a number cJSON node, with number `_objptr->_field` and string name
/// #_field, then attach it to the _root cJSON node.
#define JSON_DUMP_NUMBER(_root, _objptr, _field)                               \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateNumber((_objptr)->_field);          \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Get the string representation of the index of a SVFType pointer in the
/// SVFType pool (represeted by `typePool`), create a string cJSON node of it,
/// then attach it to the `_root` cJSON node.
#define JSON_DUMP_SVFTYPE(_root, _objptr, _field)                              \
    do                                                                         \
    {                                                                          \
        const char* strIdx = getStrTypeIndex((_objptr)->_field);               \
        cJSON* node_##_field = cJSON_CreateStringReference(strIdx);            \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Get the string representation of the index of a SVFValue pointer in the
/// SVFType pool (represeted by `valuePool`), create a string cJSON node of it,
/// then attach it to the `_root` cJSON node.
#define JSON_DUMP_SVFVALUE(_root, _objptr, _field)                             \
    do                                                                         \
    {                                                                          \
        const char* strIdx = getStrValueIndex((_objptr)->_field);              \
        cJSON* node_##_field = cJSON_CreateStringReference(strIdx);            \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Create a string cJSON node with value `_objptr->_field.c_str()` and name
/// #_field, then attach it to the `_root` cJSON node.
#define JSON_DUMP_STRING(_root, _objptr, _field)                               \
    do                                                                         \
    {                                                                          \
        const char* str = (_objptr)->_field.c_str();                           \
        cJSON* node_##_field = cJSON_CreateStringReference(str);               \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Create a cJSON array with name #_field and contents in `_obj->_field` where
/// each element is a SVFValue pointer, and then attach it to the `_root` cJSON
/// node.
#define JSON_DUMP_CONTAINER_OF_SVFVALUE(_root, _obj, _field)                   \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateArray();                            \
        for (const auto val : (_obj)->_field)                                  \
            cJSON_AddItemToArray(node_##_field, cJSON_CreateStringReference(   \
                                                    getStrValueIndex(val)));   \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Create a cJSON array with name #_field and contents in `_obj->_field` where
/// each element is a SVFType pointer, and then attach it to the `_root` cJSON
/// node.
#define JSON_DUMP_CONTAINER_OF_SVFTYPE(_root, _obj, _field)                    \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateArray();                            \
        for (const auto ty : (_obj)->_field)                                   \
            cJSON_AddItemToArray(node_##_field, cJSON_CreateStringReference(   \
                                                    getStrTypeIndex(ty)));     \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

/// Create a cJSON array with name #_field and contents in `_obj->_field` where
/// each element is a number, and then attach it to the `_root` cJSON node.
#define JSON_DUMP_CONTAINER_OF_NUMBER(_root, _obj, _field)                     \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateArray();                            \
        for (const auto num : (_obj)->_field)                                  \
            cJSON_AddItemToArray(node_##_field, cJSON_CreateNumber(num));      \
        cJSON_AddItemToObjectCS((_root), #_field, node_##_field);              \
    } while (0)

SVFModuleJsonDumper::SVFModuleJsonDumper(const SVFModule* module)
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

SVFModuleJsonDumper::SVFModuleJsonDumper(const SVFModule* module,
                                         const std::string& path)
    : SVFModuleJsonDumper(module)
{
    dumpJsonToPath(path);
}

SVFModuleJsonDumper::~SVFModuleJsonDumper()
{
    if (jsonStr)
    {
        cJSON_free((void*)jsonStr);
    }
}

void SVFModuleJsonDumper::dumpJsonToPath(const std::string& path)
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

void SVFModuleJsonDumper::dumpJsonToOstream(std::ostream& os)
{
    if (!jsonStr)
    {
        cJSON* json = moduleToJson(module);
        jsonStr = cJSON_Print(json);
        cJSON_Delete(json);
    }
    os << jsonStr << std::endl;
}

TypeIndex SVFModuleJsonDumper::getTypeIndex(const SVFType* type)
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

ValueIndex SVFModuleJsonDumper::getValueIndex(const SVFValue* value)
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

const char* SVFModuleJsonDumper::getStrOfIndex(std::size_t index)
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

const char* SVFModuleJsonDumper::getStrValueIndex(const SVFValue* value)
{
    auto i = getStrOfIndex(getValueIndex(value));
    SVFUtil::outs() << "i = " << i << ", size=" << allIndices.size() << "\n";
    assert(i < allIndices.size());
    return i;
}

const char* SVFModuleJsonDumper::getStrTypeIndex(const SVFType* type)
{
    return getStrOfIndex(getTypeIndex(type));
}

cJSON* SVFModuleJsonDumper::moduleToJson(const SVFModule* module)
{
    // Top-level json creation
    cJSON* root = cJSON_CreateObject();

    cJSON* nodeAllTypes = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(root, "typePool", nodeAllTypes);
    cJSON* nodeAllValues = cJSON_CreateArray();
    cJSON_AddItemToObjectCS(root, "valuePool", nodeAllValues);

    JSON_DUMP_STRING(root, module, pagReadFromTxt);
    JSON_DUMP_STRING(root, module, moduleIdentifier);
    // JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, FunctionSet);
    {
        cJSON* node = cJSON_CreateArray();
        for (const auto val : (module)->FunctionSet)
        {
            const char* str_i = getStrValueIndex(val);
            SVFUtil::outs()
                << "Note: " << str_i << " " << (void*)(str_i) << "\n";
            cJSON_AddItemToArray(node, cJSON_CreateStringReference(str_i));
        }
        cJSON_AddItemToObjectCS((root), "FunctionSet", node);
    }
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, GlobalSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, AliasSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, ConstantSet);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, module, OtherValueSet);

    // N.B. Be sure to use index-based loop instead of iterater based loop
    // (including range-based loop). Because all `toJson()` functions may append
    // new elements to the end of the vectors and the vector buffer might get
    // reallocated, invalidating any iterators.
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

    for (size_t i = 0; i < allIndices.size(); ++i)
    {
        SVFUtil::outs() << i << ": " << *allIndices[i] << " "
                        << (void*)(allIndices[i].get()->c_str()) << "\n";
    }

    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const StInfo* stInfo)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_CONTAINER_OF_NUMBER(root, stInfo, fldIdxVec);

    JSON_DUMP_CONTAINER_OF_NUMBER(root, stInfo, elemIdxVec);

    cJSON* nodeFldMap = cJSON_CreateArray();
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

cJSON* SVFModuleJsonDumper::toJson(const SVFType* type)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_NUMBER(root, type, kind);

    JSON_DUMP_SVFTYPE(root, type, getPointerToTy);

    cJSON* nodeTypeInfo = toJson(type->typeinfo);
    cJSON_AddItemToObjectCS(root, "typeinfo", nodeTypeInfo);

    JSON_DUMP_BOOL(root, type, isSingleValTy);

    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFPointerType* type)
{
    cJSON* root = toJson(static_cast<const SVFType*>(type));
    JSON_DUMP_SVFTYPE(root, type, ptrElementType);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFIntegerType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFFunctionType* type)
{
    cJSON* root = toJson(static_cast<const SVFType*>(type));
    JSON_DUMP_SVFTYPE(root, type, retTy);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFStructType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFArrayType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFOtherType* type)
{
    return toJson(static_cast<const SVFType*>(type));
}

cJSON* SVFModuleJsonDumper::typeToJson(const SVFType* type)
{
    using SVFUtil::dyn_cast;

    switch (type->getKind())
    {
    default:
        assert(false && "Impossible SVFType kind");
    case SVFType::SVFTy:
        return toJson(type);
    case SVFType::SVFPointerTy:
        return toJson(dyn_cast<SVFPointerType>(type));
    case SVFType::SVFIntegerTy:
        return toJson(dyn_cast<SVFIntegerType>(type));
    case SVFType::SVFFunctionTy:
        return toJson(dyn_cast<SVFFunctionType>(type));
    case SVFType::SVFStructTy:
        return toJson(dyn_cast<SVFStructType>(type));
    case SVFType::SVFArrayTy:
        return toJson(dyn_cast<SVFArrayType>(type));
    case SVFType::SVFOtherTy:
        return toJson(dyn_cast<SVFOtherType>(type));
    }
}

cJSON* SVFModuleJsonDumper::toJson(const SVFLoopAndDomInfo* ldInfo)
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, ldInfo, reachableBBs);

#define JSON_DUMP_BB_MAP(_field)                                               \
    do                                                                         \
    {                                                                          \
        cJSON* node_##_field = cJSON_CreateObject();                           \
        for (const auto& BB2BBs : (ldInfo)->_field)                            \
        {                                                                      \
            cJSON* nodeBBs = cJSON_CreateArray();                              \
            for (const SVFBasicBlock* bb : BB2BBs.second)                      \
                cJSON_AddItemToArray(nodeBBs, cJSON_CreateStringReference(     \
                                                  getStrValueIndex(bb)));      \
            cJSON_AddItemToObjectCS(node_##_field,                             \
                                    getStrValueIndex(BB2BBs.first), nodeBBs);  \
        }                                                                      \
        cJSON_AddItemToObjectCS(root, #_field, node_##_field);                 \
    } while (0)
    JSON_DUMP_BB_MAP(dtBBsMap);
    JSON_DUMP_BB_MAP(pdtBBsMap);
    JSON_DUMP_BB_MAP(dfBBsMap);
    JSON_DUMP_BB_MAP(bb2LoopMap);
#undef BB_MAP_TO_JSON

    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFValue* value)
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

cJSON* SVFModuleJsonDumper::toJson(const SVFFunction* value)
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

cJSON* SVFModuleJsonDumper::toJson(const SVFBasicBlock* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, allInsts);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, succBBs);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, predBBs);
    JSON_DUMP_SVFVALUE(root, value, fun);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFInstruction* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_SVFVALUE(root, value, bb);
    JSON_DUMP_BOOL(root, value, terminator);
    JSON_DUMP_BOOL(root, value, ret);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, succInsts);
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, predInsts);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFCallInst* value)
{
    cJSON* root = toJson(static_cast<const SVFInstruction*>(value));
    JSON_DUMP_CONTAINER_OF_SVFVALUE(root, value, args);
    JSON_DUMP_BOOL(root, value, varArg);
    JSON_DUMP_SVFVALUE(root, value, calledVal);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFVirtualCallInst* value)
{
    cJSON* root = toJson(static_cast<const SVFCallInst*>(value));
    JSON_DUMP_SVFVALUE(root, value, vCallVtblPtr);
    JSON_DUMP_NUMBER(root, value, virtualFunIdx);
    JSON_DUMP_STRING(root, value, funNameOfVcall);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFConstant* value)
{
    return toJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFGlobalValue* value)
{
    cJSON* root = toJson(static_cast<const SVFConstant*>(value));
    JSON_DUMP_SVFVALUE(root, value, realDefGlobal);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFArgument* value)
{
    cJSON* root = toJson(static_cast<const SVFValue*>(value));
    JSON_DUMP_SVFVALUE(root, value, fun);
    JSON_DUMP_NUMBER(root, value, argNo);
    JSON_DUMP_BOOL(root, value, uncalled);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFConstantData* value)
{
    return toJson(static_cast<const SVFConstant*>(value));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFConstantInt* value)
{
    cJSON* root = toJson(static_cast<const SVFConstantData*>(value));
    JSON_DUMP_NUMBER(root, value, zval);
    JSON_DUMP_NUMBER(root, value, sval);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFConstantFP* value)
{
    cJSON* root = toJson(static_cast<const SVFConstantData*>(value));
    JSON_DUMP_NUMBER(root, value, dval);
    return root;
}

cJSON* SVFModuleJsonDumper::toJson(const SVFConstantNullPtr* value)
{
    return toJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFBlackHoleValue* value)
{
    return toJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFOtherValue* value)
{
    return toJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFModuleJsonDumper::toJson(const SVFMetadataAsValue* value)
{
    return toJson(static_cast<const SVFOtherValue*>(value));
}

cJSON* SVFModuleJsonDumper::valueToJson(const SVFValue* value)
{
    using SVFUtil::dyn_cast;

    switch (value->getKind())
    {
    default:
        assert(false && "Impossible SVFValue kind");
    case SVFValue::SVFVal:
        return toJson(value);
    case SVFValue::SVFFunc:
        return toJson(dyn_cast<SVFFunction>(value));
    case SVFValue::SVFBB:
        return toJson(dyn_cast<SVFBasicBlock>(value));
    case SVFValue::SVFInst:
        return toJson(dyn_cast<SVFInstruction>(value));
    case SVFValue::SVFCall:
        return toJson(dyn_cast<SVFCallInst>(value));
    case SVFValue::SVFVCall:
        return toJson(dyn_cast<SVFVirtualCallInst>(value));
    case SVFValue::SVFGlob:
        return toJson(dyn_cast<SVFGlobalValue>(value));
    case SVFValue::SVFArg:
        return toJson(dyn_cast<SVFArgument>(value));
    case SVFValue::SVFConst:
        return toJson(dyn_cast<SVFConstant>(value));
    case SVFValue::SVFConstData:
        return toJson(dyn_cast<SVFConstantData>(value));
    case SVFValue::SVFConstInt:
        return toJson(dyn_cast<SVFConstantInt>(value));
    case SVFValue::SVFConstFP:
        return toJson(dyn_cast<SVFConstantFP>(value));
    case SVFValue::SVFNullPtr:
        return toJson(dyn_cast<SVFConstantNullPtr>(value));
    case SVFValue::SVFBlackHole:
        return toJson(dyn_cast<SVFBlackHoleValue>(value));
    case SVFValue::SVFMetaAsValue:
        return toJson(dyn_cast<SVFMetadataAsValue>(value));
    case SVFValue::SVFOther:
        return toJson(dyn_cast<SVFOtherValue>(value));
    }
}