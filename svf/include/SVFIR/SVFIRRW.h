#ifndef INCLUDE_SVFIRRW_H_
#define INCLUDE_SVFIRRW_H_

#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include "Graphs/GenericGraph.h"
#include "Graphs/ICFG.h"
#include "Graphs/IRGraph.h"

#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            SVFUtil::errs()                                                    \
                << __FILE__ << ':' << __LINE__ << ": " << reason << '\n';      \
            abort();                                                           \
        }                                                                      \
    } while (0)

namespace SVF
{

cJSON* jsonCreateObject();
cJSON* jsonCreateArray();
cJSON* jsonCreateMap();
cJSON* jsonCreateString(const char* str);
bool jsonAddPairToMap(cJSON* obj, cJSON* key, cJSON* value);
bool jsonAddItemToObject(cJSON* obj, const char* name, cJSON* item);
bool jsonAddItemToArray(cJSON* array, cJSON* item);
/// @brief Helper function to write a number to a JSON object.
bool jsonAddNumberToObject(cJSON* obj, const char* name, double number);
bool jsonAddStringToObject(cJSON* obj, const char* name, const char* str);
bool jsonAddStringToObject(cJSON* obj, const char* name,
                           const std::string& str);

#define JSON_WRITE_NUMBER_FIELD(root, objptr, field)                           \
    jsonAddNumberToObject(root, #field, (objptr)->field)
#define JSON_WRITE_STDSTRING_FIELD(root, objptr, field)                        \
    jsonAddStringToObject(root, #field, (objptr)->field)

class SVFIR;
class SVFIRWriter;

template <typename T>
class PtrPool
{
private:
    Map<const T*, size_t> ptrToId;
    std::vector<const T*> ptrPool;

public:
    size_t getID(const T* ptr)
    {
        if (!ptr)
            return 0;
        auto it_inserted = ptrToId.emplace(ptr, 1 + ptrPool.size());
        if (it_inserted.second)
            ptrPool.push_back(ptr);
        return it_inserted.first->second;
    }

    const std::vector<const T*>& getPool() const
    {
        return ptrPool;
    }
};

template <typename NodeType, typename EdgeType> 
class GenericGraphWriter
{
private:
    using GraphType = GenericGraph<NodeType, EdgeType>;

    const GraphType* graph;
    OrderedMap<const NodeType*, NodeID> nodeToID;

public:
    GenericGraphWriter(const GraphType* g) : graph(g) {
        for (const auto& entry : graph->IDToNodeMap)
        {
            nodeToID[entry.second] = entry.first;
        }
    }

    cJSON* toJson() {
        cJSON* root = jsonCreateObject();
        
        JSON_WRITE_NUMBER_FIELD(root, graph, edgeNum);
        JSON_WRITE_NUMBER_FIELD(root, graph, nodeNum);

        cJSON* map = jsonCreateMap();
        for (const auto& pair : graph->IDToNodeMap)
        {
            // pair: [NodeID, NodeType*]
            nodeToID[pair.second] = pair.first;

            cJSON *jsonID = cJSON_CreateNumber(pair.first);
            cJSON *jsonNode = jsonCreateObject();
            jsonAddPairToMap(map, jsonID, jsonNode);
        }
        jsonAddItemToObject(root, "IDToNodeMap", map);

        return root;
    }
};

class ICFGWriter : public GenericGraphWriter<ICFGNode, ICFGEdge>
{
public:
    ICFGWriter(const ICFG* icfg) : GenericGraphWriter<ICFGNode, ICFGEdge>(icfg)
    {
    }
};

class IRGraphWriter : public GenericGraphWriter<SVFVar, SVFStmt>
{
public:
    IRGraphWriter(const IRGraph* irGraph) : GenericGraphWriter<SVFVar, SVFStmt>(irGraph) {}
    // TODO
};

class SVFIRWriter
{
    const SVFIR* svfIR;

    PtrPool<SVFType> svfTypePool;
    PtrPool<SVFValue> svfValuePool;
    // Map<const SVFType*, size_t> svfTypeToId;
    // std::vector<const SVFType*> svfTypePool;
    // size_t getSvfTypeId(const SVFType* type);
    // const char* getSvfTypeIdStr(const SVFType* type);

    // Map<const SVFValue*, size_t> valueToId;
    // std::vector<const SVFValue*> valuePool;
    // size_t getSvfValueId(const SVFValue* value);
    // const char* getSvfValueIdStr(const SVFValue* value);

    OrderedMap<size_t, std::string> numToStrMap;
    IRGraphWriter irGraphWriter;
    ICFGWriter icfgWriter;

public:
    SVFIRWriter(const SVFIR* svfir);

    cJSON *toJson(const SVFModule* module);

    /// @brief Main logic to dump a SVFIR to a JSON object.
    cJSON* toJson();
private:
    const char* numToStr(size_t n);

    template <typename T>
    bool jsonAddSvfTypePtrContainerToObject(cJSON* obj, const char* name,
                                            const T& container)
    {
        cJSON* array = jsonCreateArray();
        for (const auto& item : container)
        {
            const char* strIdx = numToStr(svfTypePool.getID(item));
            cJSON* itemObj = jsonCreateString(strIdx);
            jsonAddItemToArray(array, itemObj);
        }
        return jsonAddItemToObject(obj, name, array);
    }

    template <typename T>
    bool jsonAddSvfValuePtrContainerToObject(cJSON* obj, const char* name,
                                             const T& container)
    {
        cJSON* array = jsonCreateArray();
        for (const auto& item : container)
        {
            const char* strIdx = numToStr(svfValuePool.getID(item));
            cJSON* itemObj = jsonCreateString(strIdx);
            jsonAddItemToArray(array, itemObj);
        }
        return jsonAddItemToObject(obj, name, array);
    }

    template <typename T>
    bool jsonAddToJsonableToObject(cJSON* obj, const char* name, const T* item)
    {
        cJSON* itemObj = toJson(item);
        return jsonAddItemToObject(obj, name, itemObj);
    }
};

#define JSON_WRITE_SVFTYPE_CONTAINER_FILED(root, objptr, field)                \
    jsonAddSvfTypePtrContainerToObject(root, #field, (objptr)->field)

#define JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, objptr, field)               \
    jsonAddSvfValuePtrContainerToObject(root, #field, (objptr)->field)

#define JSON_WRITE_TOJSONABLE_FIELD(root, objptr, field)                       \
    jsonAddToJsonableToObject(root, #field, (objptr)->field)

} // namespace SVF

#endif // !INCLUDE_SVFIRRW_H_