#ifndef INCLUDE_SVFIRRW_H_
#define INCLUDE_SVFIRRW_H_

#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include "Graphs/GenericGraph.h"
#include "Graphs/ICFG.h"
#include "Graphs/IRGraph.h"
#include <type_traits>

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

/// @brief Type trait to check if a type is iterable.
///@{
template <typename T, typename = void> struct is_iterable : std::false_type
{
};

// this gets used only when we can call std::begin() and std::end() on that type
template <typename T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<T&>())),
                                  decltype(std::end(std::declval<T&>()))>>
    : std::true_type
{
};
template <typename T> constexpr bool is_iterable_v = is_iterable<T>::value;
///@}

namespace SVF
{

cJSON* jsonCreateObject();
cJSON* jsonCreateArray();
cJSON* jsonCreateMap();
cJSON* jsonCreateString(const char* str);
cJSON* jsonCreateIndex(size_t index);
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
#define JSON_WRITE_STRING_FIELD(root, objptr, field)                           \
    jsonAddStringToObject(root, #field, (objptr)->field)
#define JSON_WRITE_FIELD(root, objptr, field)                                  \
    jsonAddJsonableToObject(root, #field, (objptr)->field)


class SVFIR;
class SVFIRWriter;

template <typename T>
class PtrPool
{
private:
    Map<const T*, size_t> ptrToId;
    std::vector<const T*> ptrPool;

public:
    inline size_t getID(const T* ptr)
    {
        if (!ptr)
            return 0;
        auto it_inserted = ptrToId.emplace(ptr, 1 + ptrPool.size());
        if (it_inserted.second)
            ptrPool.push_back(ptr);
        return it_inserted.first->second;
    }

    inline const T* getPtr(size_t id) const
    {
        assert(id >= 0 && id <= ptrPool.size() && "Invalid ID.");
        return id ? ptrPool[id - 1] : nullptr;
    }

    inline const std::vector<const T*>& getPool() const
    {
        return ptrPool;
    }
};

template <typename NodeType, typename EdgeType>
class GenericGraphWriter
{
    friend class SVFIRWriter;
private:
    using GraphType = GenericGraph<NodeType, EdgeType>;

    const GraphType* graph;
    OrderedMap<const NodeType*, NodeID> nodeToID;
    PtrPool<EdgeType> edgePool;

public:
    GenericGraphWriter(const GraphType* g) : graph(g)
    {
        for (const auto& entry : graph->IDToNodeMap)
        {
            const NodeID id = entry.first;
            const NodeType* node = entry.second;

            nodeToID.emplace(node, id);
            for (const EdgeType* edge : node->getOutEdges())
            {
                edgePool.getID(edge);
            }
        }
    }

    inline size_t getEdgeID(const EdgeType* edge)
    {
        return edgePool.getID(edge);
    }

    inline NodeID getNodeID(const NodeType* node) const
    {
        auto it = nodeToID.find(node);
        assert(it != nodeToID.end() && "Node not found in the graph.");
        return it->second;
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

    IRGraphWriter irGraphWriter;
    ICFGWriter icfgWriter;

    OrderedMap<size_t, std::string> numToStrMap;

public:
    SVFIRWriter(const SVFIR* svfir);

    cJSON *toJson(const SVFModule* module);

    /// @brief Main logic to dump a SVFIR to a JSON object.
    cJSON* toJson();
//private:
    const char* numToStr(size_t n);

    cJSON* toJson(const SVFType* type);
    cJSON* toJson(const SVFValue* value);
    cJSON* toJson(const SVFStmt* stmt);  // IRGraph Node
    cJSON* toJson(const SVFVar* var);    // IRGraph Edge
    cJSON* toJson(const ICFGNode* node); // ICFG Node
    cJSON* toJson(const ICFGEdge* edge); // ICFG Edge
    static cJSON* toJson(const LocationSet& ls);

    template <unsigned ElementSize>
    static cJSON* toJson(const SparseBitVector<ElementSize>& bv)
    {
        //cJSON* array = jsonCreateArray();
        //for (size_t i = 0; i < bv.size(); ++i)
        //{
        //    if (bv.test(i))
        //    {
        //        cJSON* item = cJSON_CreateNumber(i);
        //        jsonAddItemToArray(array, item);
        //    }
        //}
        //return array;
        return cJSON_CreateString("TODO: JSON BitVector");
    }

    template <typename T, typename U>
    cJSON* toJson(const std::pair<T, U>& pair)
    {
        cJSON* obj = jsonCreateObject();
        const auto* p = &pair;
        JSON_WRITE_FIELD(obj, p, first);
        JSON_WRITE_FIELD(obj, p, second);
        return obj;
    }

    template <typename T, typename = std::enable_if_t<is_iterable_v<T>>>
    cJSON* toJson(const T& container)
    {
        cJSON* array = jsonCreateArray();
        for (const auto& item : container)
        {
            cJSON* itemObj = toJson(item);
            jsonAddItemToArray(array, itemObj);
        }
        return array;
    }

    template <typename T>
    bool jsonAddJsonableToObject(cJSON* obj, const char* name, const T& item)
    {
        cJSON* itemObj = toJson(item);
        return jsonAddItemToObject(obj, name, itemObj);
    }

    // TODO: deprecated
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

    // TODO: deprecated
    // template <typename T>
    // bool jsonAddSvfValuePtrContainerToObject(cJSON* obj, const char* name,
    //                                          const T& container)
    // {
    //     cJSON* array = jsonCreateArray();
    //     for (const auto& item : container)
    //     {
    //         const char* strIdx = numToStr(svfValuePool.getID(item));
    //         cJSON* itemObj = jsonCreateString(strIdx);
    //         jsonAddItemToArray(array, itemObj);
    //     }
    //     return jsonAddItemToObject(obj, name, array);
    // }

};

// #define JSON_WRITE_SVFTYPE_CONTAINER_FILED(root, objptr, field)                
//     jsonAddSvfTypePtrContainerToObject(root, #field, (objptr)->field)

// #define JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, objptr, field)               
//     jsonAddSvfValuePtrContainerToObject(root, #field, (objptr)->field)

} // namespace SVF

#endif // !INCLUDE_SVFIRRW_H_