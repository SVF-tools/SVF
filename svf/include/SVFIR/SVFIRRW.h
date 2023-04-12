/*  SVF - Static Value-Flow Analysis Framework
 *
 *
 * Note for this module:
 *  1. This module is used to read and write SVFIR from/to JSON file.
 *
 * Information about pointer "ownership":
 * - SVFTypes are owned by
 *      symbolTableInfo -> svfTypes
 */

#ifndef INCLUDE_SVFIRRW_H_
#define INCLUDE_SVFIRRW_H_

#include "Graphs/GenericGraph.h"
#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include <type_traits>

#define SVFIR_DEBUG 1 /* Turn this on if you're debugging SVFWriter */
#if SVFIR_DEBUG
#    define ENSURE_NOT_VISITED(graph)                                          \
        do                                                                     \
        {                                                                      \
            static std::set<decltype(graph)> visited;                          \
            bool inserted = visited.insert(graph).second;                      \
            ABORT_IFNOT(inserted, #graph " already visited!");                 \
        } while (0)
#else
#    define ENSURE_NOT_VISITED(graph)                                          \
        do                                                                     \
        {                                                                      \
        } while (0)
#endif

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

#define FIELD_NAME_ITEM(field) #field, (field)

#define JSON_FIELD_OR(json, field, default) ((json) ? (json)->field : default)
#define JSON_KEY(json) JSON_FIELD_OR(json, string, "NULL")
#define JSON_CHILD(json) JSON_FIELD_OR(json, child, nullptr)

#define JSON_WRITE_FIELD(root, objptr, field)                                  \
    jsonAddJsonableToObject(root, #field, (objptr)->field)

#define JSON_READ_OBJ_WITH_NAME(json, obj, name)                               \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(jsonKeyEquals(json, name),                                 \
                    "Expect name '" << name << "', got " << JSON_KEY(json));   \
        SVFIRReader::readJson(json, obj);                                      \
    } while (0)

#define JSON_READ_OBJ_WITH_NAME_FWD(json, obj, name)                           \
    do                                                                         \
    {                                                                          \
        JSON_READ_OBJ_WITH_NAME(json, obj, name);                              \
        json = (json)->next;                                                   \
    } while (0)

#define JSON_READ_OBJ(json, obj) JSON_READ_OBJ_WITH_NAME(json, obj, #obj)
#define JSON_READ_OBJ_FWD(json, obj)                                           \
    JSON_READ_OBJ_WITH_NAME_FWD(json, obj, #obj)
#define JSON_READ_FIELD_FWD(json, objptr, field)                               \
    JSON_READ_OBJ_WITH_NAME_FWD(json, (objptr)->field, #field)
#define CHECK_JSON_KEY_EQUALS(obj, key)                                        \
    ABORT_IFNOT(jsonKeyEquals(obj, key),                                       \
                "Expect json key: " << key << ", but get " << JSON_KEY(obj));
#define CHECK_JSON_KEY(obj) CHECK_JSON_KEY_EQUALS(obj, #obj)

namespace SVF
{
/// @brief Forward declarations.
///@{
class SVFIR;
class SVFIRWriter;
class SVFLoop;
class ICFG;
class IRGraph;
class CHGraph;
class CommonCHGraph;
class SymbolTableInfo;
class MemObj;

class SVFModule;

class StInfo;
class SVFType;
class SVFPointerType;
class SVFIntegerType;
class SVFFunctionType;
class SVFStructType;
class SVFArrayType;
class SVFOtherType;
class LocationSet;
class ObjTypeInfo;

class SVFVar;
class ValVar;
class ObjVar;
class GepValVar;
class GepObjVar;
class FIObjVar;
class RetPN;
class VarArgPN;
class DummyValVar;
class DummyObjVar;

class SVFStmt;
class AssignStmt;
class AddrStmt;
class CopyStmt;
class StoreStmt;
class LoadStmt;
class GepStmt;
class CallPE;
class RetPE;
class MultiOpndStmt;
class PhiStmt;
class SelectStmt;
class CmpStmt;
class BinaryOPStmt;
class UnaryOPStmt;
class BranchStmt;
class TDForkPE;
class TDJoinPE;

class ICFGNode;
class GlobalICFGNode;
class IntraICFGNode;
class InterICFGNode;
class FunEntryICFGNode;
class FunExitICFGNode;
class CallICFGNode;
class RetICFGNode;

class ICFGEdge;
class IntraCFGEdge;
class CallCFGEdge;
class RetCFGEdge;

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

class CHNode;
class CHEdge;
class CHGraph;
// End of forward declarations
///@}

bool jsonIsBool(const cJSON* item);
bool jsonIsBool(const cJSON* item, bool& flag);
bool jsonIsNumber(const cJSON* item);
bool jsonIsString(const cJSON* item);
bool jsonIsNullId(const cJSON* item);
bool jsonIsArray(const cJSON* item);
bool jsonIsMap(const cJSON* item);
bool jsonIsObject(const cJSON* item);
bool jsonKeyEquals(const cJSON* item, const char* key);
std::pair<const cJSON*, const cJSON*> jsonUnpackPair(const cJSON* item);
double jsonGetNumber(const cJSON* item);
cJSON* jsonCreateNullId();
cJSON* jsonCreateObject();
cJSON* jsonCreateArray();
cJSON* jsonCreateString(const char* str);
cJSON* jsonCreateIndex(size_t index);
cJSON* jsonCreateBool(bool flag);
cJSON* jsonCreateNumber(double num);
bool jsonAddPairToMap(cJSON* obj, cJSON* key, cJSON* value);
bool jsonAddItemToObject(cJSON* obj, const char* name, cJSON* item);
bool jsonAddItemToArray(cJSON* array, cJSON* item);
/// @brief Helper function to write a number to a JSON object.
bool jsonAddNumberToObject(cJSON* obj, const char* name, double number);
bool jsonAddStringToObject(cJSON* obj, const char* name, const char* str);
bool jsonAddStringToObject(cJSON* obj, const char* name,
                           const std::string& str);
#define jsonForEach(field, array)                                              \
    for (const cJSON* field = JSON_CHILD(array); field; field = field->next)

/// @brief Bookkeeping class to keep track of the IDs of objects that doesn't
/// have any ID. E.g., SVFValue, XXXEdge.
/// @tparam T
template <typename T> class WriterPtrPool
{
private:
    Map<const T*, size_t> ptrToId;
    std::vector<const T*> ptrPool;

public:
    inline size_t getID(const T* ptr)
    {
        if (!ptr)
            return 0;

        typename decltype(ptrToId)::iterator it;
        bool inserted;
        std::tie(it, inserted) = ptrToId.emplace(ptr, 1 + ptrPool.size());
        if (inserted)
            ptrPool.push_back(ptr);
        return it->second;
    }

    inline void saveID(const T* ptr)
    {
        getID(ptr);
    }

    inline const T* getPtr(size_t id) const
    {
        assert(id <= ptrPool.size() && "Invalid ID");
        return id ? ptrPool[id - 1] : nullptr;
    }

    inline const std::vector<const T*>& getPool() const
    {
        return ptrPool;
    }

    inline size_t size() const
    {
        return ptrPool.size();
    }

    inline void reserve(size_t size)
    {
        ptrPool.reserve(size);
    }

    inline auto begin() const
    {
        return ptrPool.cbegin();
    }

    inline auto end() const
    {
        return ptrPool.cend();
    }
};

template <typename NodeTy, typename EdgeTy> class GenericGraphWriter
{
    friend class SVFIRWriter;

private:
    using NodeType = NodeTy;
    using EdgeType = EdgeTy;
    using GraphType = GenericGraph<NodeType, EdgeType>;

    // const GraphType* graph;
    WriterPtrPool<EdgeType> edgePool;

public:
    GenericGraphWriter(const GraphType* graph)
    {
        assert(graph && "Graph pointer should never be null");
        edgePool.reserve(graph->getTotalEdgeNum());

        for (const auto& pair : graph->IDToNodeMap)
        {
            const NodeType* node = pair.second;

            for (const EdgeType* edge : node->getOutEdges())
            {
                edgePool.saveID(edge);
            }
        }
    }

    inline size_t getEdgeID(const EdgeType* edge)
    {
        return edgePool.getID(edge);
    }
};

using GenericICFGWriter = GenericGraphWriter<ICFGNode, ICFGEdge>;

class ICFGWriter : public GenericICFGWriter
{
    friend class SVFIRWriter;

private:
    WriterPtrPool<SVFLoop> svfLoopPool;

public:
    ICFGWriter(const ICFG* icfg);

    inline size_t getSvfLoopID(const SVFLoop* loop)
    {
        return svfLoopPool.getID(loop);
    }
};

class SymbolTableInfoWriter
{
    friend class SVFIRWriter;

public:
    SymbolTableInfoWriter(const SymbolTableInfo* symbolTableInfo);
    SymID getMemObjID(const MemObj* memObj);
    size_t getSvfTypeID(const SVFType* type);
    size_t getStInfoID(const StInfo* stInfo);

private:
    WriterPtrPool<SVFType> svfTypePool;
    WriterPtrPool<StInfo> stInfoPool;
};

using IRGraphWriter = GenericGraphWriter<SVFVar, SVFStmt>;
using CHGraphWriter = GenericGraphWriter<CHNode, CHEdge>;

class SVFModuleWriter
{
private:
    WriterPtrPool<SVFValue> svfValuePool;

public:
    inline size_t getSvfValueID(const SVFValue* value);

    inline const SVFValue* getSVFValuePtr(size_t id) const
    {
        return svfValuePool.getPtr(id);
    }

    inline size_t sizeSVFValuePool() const
    {
        return svfValuePool.size();
    }
};

class SVFIRWriter
{
private:
    const SVFIR* svfIR;

    SVFModuleWriter svfModuleWriter;
    IRGraphWriter irGraphWriter;
    ICFGWriter icfgWriter;
    CHGraphWriter chgWriter;
    SymbolTableInfoWriter symbolTableInfoWriter;

    OrderedMap<size_t, std::string> numToStrMap;

public:
    using autoJSON = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
    using autoCStr = std::unique_ptr<char, decltype(&cJSON_free)>;

    /// @brief Constructor.
    SVFIRWriter(const SVFIR* svfir);

    static void writeJsonToOstream(const SVFIR* svfir, std::ostream& os);
    static void writeJsonToPath(const SVFIR* svfir, const std::string& path);

private:
    /// @brief Main logic to dump a SVFIR to a JSON object.
    autoJSON generateJson();
    autoCStr generateJsonString();

    const char* numToStr(size_t n);

    cJSON* toJson(const SymbolTableInfo* symTable);
    cJSON* toJson(const SVFModule* module);
    cJSON* toJson(const SVFType* type);
    cJSON* toJson(const SVFValue* value);
    cJSON* toJson(const IRGraph* graph);       // IRGraph Graph
    cJSON* toJson(const SVFVar* var);          // IRGraph Node
    cJSON* toJson(const SVFStmt* stmt);        // IRGraph Edge
    cJSON* toJson(const ICFG* icfg);           // ICFG Graph
    cJSON* toJson(const ICFGNode* node);       // ICFG Node
    cJSON* toJson(const ICFGEdge* edge);       // ICFG Edge
    cJSON* toJson(const CommonCHGraph* graph); // CHGraph Graph
    cJSON* toJson(const CHGraph* graph);       // CHGraph Graph
    cJSON* toJson(const CHNode* node);         // CHGraph Node
    cJSON* toJson(const CHEdge* edge);         // CHGraph Edge

    cJSON* toJson(const CallSite& cs);
    cJSON* toJson(const LocationSet& ls);
    cJSON* toJson(const SVFLoop* loop);
    cJSON* toJson(const MemObj* memObj);
    cJSON* toJson(const ObjTypeInfo* objTypeInfo);  // Only owned by MemObj
    cJSON* toJson(const SVFLoopAndDomInfo* ldInfo); // Only owned by SVFFunction
    cJSON* toJson(const StInfo* stInfo);

    static cJSON* toJson(bool flag);
    static cJSON* toJson(unsigned number);
    static cJSON* toJson(int number);
    static cJSON* toJson(float number);
    cJSON* toJson(unsigned long number);
    cJSON* toJson(long long number);
    cJSON* toJson(unsigned long long number);

    /// \brief Parameter types of these functions are all pointers.
    /// When they are used as arguments of toJson(), they will be
    /// dumped as an index. `contentToJson()` will dump the actual content.
    ///@{
    cJSON* virtToJson(const SVFType* type);
    cJSON* virtToJson(const SVFValue* value);
    cJSON* virtToJson(const SVFVar* var);
    cJSON* virtToJson(const SVFStmt* stmt);
    cJSON* virtToJson(const ICFGNode* node);
    cJSON* virtToJson(const ICFGEdge* edge);
    cJSON* virtToJson(const CHNode* node);
    cJSON* virtToJson(const CHEdge* edge);

    // Classes inherited from SVFVar
    cJSON* contentToJson(const SVFVar* var);
    cJSON* contentToJson(const ValVar* var);
    cJSON* contentToJson(const ObjVar* var);
    cJSON* contentToJson(const GepValVar* var);
    cJSON* contentToJson(const GepObjVar* var);
    cJSON* contentToJson(const FIObjVar* var);
    cJSON* contentToJson(const RetPN* var);
    cJSON* contentToJson(const VarArgPN* var);
    cJSON* contentToJson(const DummyValVar* var);
    cJSON* contentToJson(const DummyObjVar* var);

    // Classes inherited from SVFStmt
    cJSON* contentToJson(const SVFStmt* edge);
    cJSON* contentToJson(const AssignStmt* edge);
    cJSON* contentToJson(const AddrStmt* edge);
    cJSON* contentToJson(const CopyStmt* edge);
    cJSON* contentToJson(const StoreStmt* edge);
    cJSON* contentToJson(const LoadStmt* edge);
    cJSON* contentToJson(const GepStmt* edge);
    cJSON* contentToJson(const CallPE* edge);
    cJSON* contentToJson(const RetPE* edge);
    cJSON* contentToJson(const MultiOpndStmt* edge);
    cJSON* contentToJson(const PhiStmt* edge);
    cJSON* contentToJson(const SelectStmt* edge);
    cJSON* contentToJson(const CmpStmt* edge);
    cJSON* contentToJson(const BinaryOPStmt* edge);
    cJSON* contentToJson(const UnaryOPStmt* edge);
    cJSON* contentToJson(const BranchStmt* edge);
    cJSON* contentToJson(const TDForkPE* edge);
    cJSON* contentToJson(const TDJoinPE* edge);

    // Classes inherited from ICFGNode
    cJSON* contentToJson(const ICFGNode* node);
    cJSON* contentToJson(const GlobalICFGNode* node);
    cJSON* contentToJson(const IntraICFGNode* node);
    cJSON* contentToJson(const InterICFGNode* node);
    cJSON* contentToJson(const FunEntryICFGNode* node);
    cJSON* contentToJson(const FunExitICFGNode* node);
    cJSON* contentToJson(const CallICFGNode* node);
    cJSON* contentToJson(const RetICFGNode* node);

    // Classes inherited from ICFGEdge
    cJSON* contentToJson(const ICFGEdge* edge);
    cJSON* contentToJson(const IntraCFGEdge* edge);
    cJSON* contentToJson(const CallCFGEdge* edge);
    cJSON* contentToJson(const RetCFGEdge* edge);

    // CHNode & CHEdge
    cJSON* contentToJson(const CHNode* node);
    cJSON* contentToJson(const CHEdge* edge);

    cJSON* contentToJson(const SVFType* type);
    cJSON* contentToJson(const SVFPointerType* type);
    cJSON* contentToJson(const SVFIntegerType* type);
    cJSON* contentToJson(const SVFFunctionType* type);
    cJSON* contentToJson(const SVFStructType* type);
    cJSON* contentToJson(const SVFArrayType* type);
    cJSON* contentToJson(const SVFOtherType* type);

    cJSON* contentToJson(const SVFValue* value);
    cJSON* contentToJson(const SVFFunction* value);
    cJSON* contentToJson(const SVFBasicBlock* value);
    cJSON* contentToJson(const SVFInstruction* value);
    cJSON* contentToJson(const SVFCallInst* value);
    cJSON* contentToJson(const SVFVirtualCallInst* value);
    cJSON* contentToJson(const SVFConstant* value);
    cJSON* contentToJson(const SVFGlobalValue* value);
    cJSON* contentToJson(const SVFArgument* value);
    cJSON* contentToJson(const SVFConstantData* value);
    cJSON* contentToJson(const SVFConstantInt* value);
    cJSON* contentToJson(const SVFConstantFP* value);
    cJSON* contentToJson(const SVFConstantNullPtr* value);
    cJSON* contentToJson(const SVFBlackHoleValue* value);
    cJSON* contentToJson(const SVFOtherValue* value);
    cJSON* contentToJson(const SVFMetadataAsValue* value);

    // Other classes
    cJSON* contentToJson(const SVFLoop* loop);
    cJSON* contentToJson(const MemObj* memObj); // Owned by SymbolTable->objMap
    cJSON* contentToJson(const StInfo* stInfo);
    ///@}

    template <typename NodeTy, typename EdgeTy>
    cJSON* genericNodeToJson(const GenericNode<NodeTy, EdgeTy>* node)
    {
        cJSON* root = jsonCreateObject();
        JSON_WRITE_FIELD(root, node, id);
        JSON_WRITE_FIELD(root, node, nodeKind);
        JSON_WRITE_FIELD(root, node, InEdges);
        JSON_WRITE_FIELD(root, node, OutEdges);
        return root;
    }

    template <typename NodeTy>
    cJSON* genericEdgeToJson(const GenericEdge<NodeTy>* edge)
    {
        cJSON* root = jsonCreateObject();
        JSON_WRITE_FIELD(root, edge, edgeFlag);
        JSON_WRITE_FIELD(root, edge, src);
        JSON_WRITE_FIELD(root, edge, dst);
        return root;
    }

    template <typename NodeTy, typename EdgeTy>
    cJSON* genericGraphToJson(const GenericGraph<NodeTy, EdgeTy>* graph,
                              const std::vector<const EdgeTy*>& edgePool)
    {
        cJSON* root = jsonCreateObject();

        JSON_WRITE_FIELD(root, graph, edgeNum);
        JSON_WRITE_FIELD(root, graph, nodeNum);

        cJSON* allNode = jsonCreateArray();
        for (const auto& pair : graph->IDToNodeMap)
        {
            NodeTy* node = pair.second;
            cJSON* jsonNode = virtToJson(node);
            jsonAddItemToArray(allNode, jsonNode);
        }
        jsonAddItemToObject(root, FIELD_NAME_ITEM(allNode));

        cJSON* allEdge = jsonCreateArray();
        for (const EdgeTy* edge : edgePool)
        {
            cJSON* edgeJson = virtToJson(edge);
            jsonAddItemToArray(allEdge, edgeJson);
        }
        jsonAddItemToObject(root, FIELD_NAME_ITEM(allEdge));

        return root;
    }

    template <unsigned ElementSize>
    cJSON* toJson(const SparseBitVectorElement<ElementSize>& element)
    {
        cJSON* array = jsonCreateArray();
        for (const auto v : element.Bits)
        {
            jsonAddItemToArray(array, toJson(v));
        }
        return array;
    }

    template <unsigned ElementSize>
    cJSON* toJson(const SparseBitVector<ElementSize>& bv)
    {
        return toJson(bv.Elements);
    }

    template <typename T, typename U> cJSON* toJson(const std::pair<T, U>& pair)
    {
        cJSON* obj = jsonCreateArray();
        jsonAddItemToArray(obj, toJson(pair.first));
        jsonAddItemToArray(obj, toJson(pair.second));
        return obj;
    }

    template <typename T,
              typename = std::enable_if_t<SVFUtil::is_iterable_v<T>>>
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

    template <typename T>
    bool jsonAddContentToObject(cJSON* obj, const char* name, const T& item)
    {
        cJSON* itemObj = contentToJson(item);
        return jsonAddItemToObject(obj, name, itemObj);
    }
};

/* Reader Part
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */

/// @brief Type trait to get base type.
/// Helper struct to detect inheritance from Node/Edge
///@}
template <typename T, typename = void> struct KindBaseHelper
{
};
#define KIND_BASE(B, KindGetter)                                               \
    template <typename T>                                                      \
    struct KindBaseHelper<T, std::enable_if_t<std::is_base_of<B, T>::value>>   \
    {                                                                          \
        using type = B;                                                        \
        static inline s64_t getKind(T* p)                                      \
        {                                                                      \
            return p->KindGetter();                                            \
        }                                                                      \
    }
KIND_BASE(SVFType, getKind);
KIND_BASE(SVFValue, getKind);
KIND_BASE(SVFVar, getNodeKind);
KIND_BASE(SVFStmt, getEdgeKind);
KIND_BASE(ICFGNode, getNodeKind);
KIND_BASE(ICFGEdge, getEdgeKind);
KIND_BASE(CHNode, getNodeKind);
KIND_BASE(CHEdge, getEdgeKind);
#undef KIND_BASE

template <typename T> using KindBaseT = typename KindBaseHelper<T>::type;
///@}

/// @brief Keeps a map from IDs to T objects, such as XXNode.
/// @tparam T: The type of the objects.
template <typename T> class ReaderIDToObjMap
{
private:
    using IDToPairMapTy = OrderedMap<unsigned, std::pair<const cJSON*, T*>>;
    IDToPairMapTy idMap;

public:
    template <typename IdObjCreator>
    /// idObjcreator : (const cJSON*) -> (id, T*) with id set
    void createObjs(const cJSON* idObjArrayJson, IdObjCreator idObjCreator)
    {
        assert(idMap.empty() &&
               "idToObjMap should be empty when creating objects");
        ABORT_IFNOT(jsonIsArray(idObjArrayJson), "expects an array");

        jsonForEach(objJson, idObjArrayJson)
        {
            ABORT_IFNOT(jsonIsObject(objJson), "expects an object");
            const cJSON* objFieldJson = objJson->child;
            // creator is allowed to change objFieldJson
            T* obj;
            unsigned id;
            std::tie(id, obj) = idObjCreator(objFieldJson);
            auto pair = std::pair<const cJSON*, T*>(objFieldJson, obj);
            bool inserted = idMap.emplace(id, pair).second;
            ABORT_IFNOT(inserted, "duplicate ID " << id);
        }
    }

    T* getPtr(unsigned id) const
    {
        auto it = idMap.find(id);
        ABORT_IFNOT(it != idMap.end(), "ID " << id << " not found");
        return it->second.second;
    }

    template <typename FillFunc> void fillObjs(FillFunc fillFunc)
    {
        for (auto& pair : idMap)
        {
            const cJSON* objFieldJson = pair.second.first;
            T* obj = pair.second.second;
            fillFunc(objFieldJson, obj);

            ABORT_IFNOT(!objFieldJson, "json should be consumed by filler, but "
                                           << objFieldJson->string << " left");
        }
    }

    inline size_t size() const
    {
        return idMap.size();
    }

    //struct iterator : public IDToPairMapTy::iterator
    //{
    //    using value_type = T*;
    //    using pointer = value_type*;
    //    using reference = value_type&;

    //    explicit iterator(const typename IDToPairMapTy::iterator& it)
    //        : IDToPairMapTy::iterator(it) {}

    //    reference operator*() const { return this->value()->second.second; }
    //    pointer operator->() const { return &*this; }
    //};

    //iterator begin() { return iterator(idMap.begin()); }
    //iterator end() { return iterator(idMap.begin()); }

    template <typename Map> void saveToIDToObjMap(Map& idToObjMap) const
    {
        for (auto& pair : idMap)
        {
            unsigned id = pair.first;
            T* obj = pair.second.second;
            assert(obj && "obj should not be null");
            idToObjMap.insert(std::make_pair(id, obj));
        }
    }
};

/// @brief Reverse of WriterPtrPool where T is object type without ID field.
/// @tparam T
template <typename T> class ReaderPtrPool
{
private:
    std::vector<const cJSON*> jsonArray;
    std::vector<T*> ptrPool;

public:
    inline void reserve(size_t size)
    {
        jsonArray.reserve(size);
        ptrPool.reserve(size);
    }

    /// @brief
    /// @tparam Creator
    /// @param objArrayJson
    /// @param creator
    template <typename Creator>
    void createObjs(const cJSON* objArrayJson, Creator creator)
    {
        assert(jsonArray.empty() &&
               "jsonArray should be empty when creating objects");
        ABORT_IFNOT(jsonIsArray(objArrayJson), "expects an array");

        jsonForEach(objJson, objArrayJson)
        {
            ABORT_IFNOT(jsonIsObject(objJson), "expects objects in array");
            const cJSON* objFieldJson = objJson->child;
            T* obj = creator(objFieldJson);
            jsonArray.push_back(objFieldJson);
            ptrPool.push_back(obj);
        }
    }

    T* getPtr(size_t id) const
    {
        assert(id <= jsonArray.size() && "Invalid ID");
        return id ? ptrPool[id - 1] : nullptr;
    }

    template <typename FillFunc> void fillObjs(FillFunc fillFunc)
    {
        assert(jsonArray.size() == ptrPool.size() &&
               "jsonArray and ptrPool should have same size");
        for (size_t i = 0; i < jsonArray.size(); ++i)
        {
            fillFunc(jsonArray[i], ptrPool[i]);
        }
    }

    inline size_t size() const
    {
        return ptrPool.size();
    }
};

template <typename NodeTy, typename EdgeTy> class GenericGraphReader
{
private:
    ReaderIDToObjMap<NodeTy> idToNodeMap;
    ReaderPtrPool<EdgeTy> edgePool;

protected:
    const cJSON* graphFieldJson = nullptr;

public:
    template <typename NodeCreator, typename EdgeCreator>
    void createObjs(const cJSON* graphJson, NodeCreator nodeCreator,
                    EdgeCreator edgeCreator)
    {
        // Read nodeNum
        const cJSON* nodeNum = graphJson->child;
        CHECK_JSON_KEY(nodeNum);
        u32_t numOfNodes = jsonGetNumber(nodeNum);

        // Read allNode
        const cJSON* allNode = nodeNum->next;
        CHECK_JSON_KEY(allNode);
        idToNodeMap.createObjs(allNode, nodeCreator);
        ABORT_IFNOT(idToNodeMap.size() == numOfNodes, "nodeNum mismatch");

        // Read edgeNum
        const cJSON* edgeNum = allNode->next;
        CHECK_JSON_KEY(edgeNum);
        u32_t numOfEdges = jsonGetNumber(edgeNum);

        // Read allEdge
        const cJSON* allEdge = edgeNum->next;
        CHECK_JSON_KEY(allEdge);
        edgePool.createObjs(allEdge, edgeCreator);
        ABORT_IFNOT(edgePool.size() == numOfEdges, "edgeNum mismatch");

        // Rest fields
        assert(!graphFieldJson && "graphFieldJson should be empty");
        graphFieldJson = allEdge->next;
    }

    inline NodeTy* getNodePtr(unsigned id) const
    {
        return idToNodeMap.getPtr(id);
    }

    inline EdgeTy* getEdgePtr(unsigned id) const
    {
        return edgePool.getPtr(id);
    }

    template <typename NodeFiller, typename EdgeFiller>
    void fillObjs(NodeFiller nodeFiller, EdgeFiller edgeFiller)
    {
        idToNodeMap.fillObjs(nodeFiller);
        edgePool.fillObjs(edgeFiller);
    }

    void saveToGenericGraph(GenericGraph<NodeTy, EdgeTy>* graph) const
    {
        graph->edgeNum = edgePool.size();
        graph->nodeNum = idToNodeMap.size();
        idToNodeMap.saveToIDToObjMap(graph->IDToNodeMap);
    }

    const cJSON* getFieldJson() const
    {
        return graphFieldJson;
    }
};

class SymbolTableInfoReader
{
    friend class SVFIRReader;

private:
    const cJSON* symTabFieldJson = nullptr;
    ReaderIDToObjMap<MemObj> memObjMap;
    ReaderPtrPool<SVFType> svfTypePool;
    ReaderPtrPool<StInfo> stInfoPool;

public:
    void createObjs(const cJSON* symTableJson);

    inline MemObj* getMemObjPtr(unsigned id) const
    {
        return memObjMap.getPtr(id);
    }

    template <typename MemObjFiller, typename SVFTypeFiller,
              typename StInfoFiller>
    inline void fillObjs(MemObjFiller memObjFiller, SVFTypeFiller svfTypeFiller,
                         StInfoFiller stInfoFiller)
    {
        memObjMap.fillObjs(memObjFiller);
        svfTypePool.fillObjs(svfTypeFiller);
        stInfoPool.fillObjs(stInfoFiller);
    }
};

using GenericICFGReader = GenericGraphReader<ICFGNode, ICFGEdge>;
using CHGraphReader = GenericGraphReader<CHNode, CHEdge>;
using IRGraphReader = GenericGraphReader<SVFVar, SVFStmt>;

class ICFGReader : public GenericICFGReader
{
    // friend class SVFIRReader;

private:
    ReaderPtrPool<SVFLoop> svfLoopPool;

public:
    void createObjs(const cJSON* icfgJson);

    inline SVFLoop* getSVFLoopPtr(size_t id) const
    {
        return svfLoopPool.getPtr(id);
    }

    template <typename NodeFiller, typename EdgeFiller, typename LoopFiller>
    void fillObjs(NodeFiller nodeFiller, EdgeFiller edgeFiller,
                  LoopFiller loopFiller)
    {
        GenericICFGReader::fillObjs(nodeFiller, edgeFiller);
        svfLoopPool.fillObjs(loopFiller);
    }
};

class SVFModuleReader
{
    const cJSON* svfModuleFieldJson = nullptr;
    ReaderPtrPool<SVFValue> svfValuePool;

public:
    void createObjs(const cJSON* svfModuleJson);

    inline SVFValue* getSVFValuePtr(size_t id) const
    {
        return svfValuePool.getPtr(id);
    }

    template <typename SVFValueFiller>
    void fillObjs(SVFValueFiller svfValueFiller)
    {
        svfValuePool.fillObjs(svfValueFiller);
    }

    const cJSON* getFieldJson() const
    {
        return svfModuleFieldJson;
    }
};

/* SVFIRReader
 * Read SVFIR from JSON
 */

class SVFIRReader
{
private:
    SymbolTableInfoReader symTableReader;
    IRGraphReader irGraphReader;
    SVFModuleReader svfModuleReader;
    ICFGReader icfgReader;
    CHGraphReader chGraphReader;

public:
    void read(cJSON* root);
    const cJSON* createObjs(const cJSON* root);

    static void readJson(const cJSON* obj, bool& flag);
    static void readJson(const cJSON* obj, unsigned& val);
    static void readJson(const cJSON* obj, int& val);
    static void readJson(const cJSON* obj, float& val);
    static void readJson(const cJSON* obj, unsigned long& val);
    static void readJson(const cJSON* obj, long long& val);
    static void readJson(const cJSON* obj, unsigned long long& val);
    static void readJson(const cJSON* obj, std::string& str);

public:
    template <typename T> inline T constructFromJson(const cJSON* obj)
    {
        T t;
        readJson(obj, t);
        return t;
    }
    template <> inline CallSite constructFromJson<CallSite>(const cJSON* obj)
    {
        SVFCallInst* callInst;
        readJson(obj, callInst);
        return CallSite(callInst);
    }

    void readJson(const cJSON* obj, SymbolTableInfo*& symTabInfo);
    void readJson(const cJSON* obj, IRGraph*& graph); // IRGraph Graph
    void readJson(const cJSON* obj, ICFG*& icfg);     // ICFG Graph
    void readJson(const cJSON* obj, CHGraph*& graph); // CHGraph Graph
    void readJson(const cJSON* obj, SVFModule*& module);

    void readJson(const cJSON* obj, SVFType*& type);
    void readJson(const cJSON* obj, StInfo*& stInfo);
    void readJson(const cJSON* obj, SVFValue*& value);

    void readJson(const cJSON* obj, SVFVar*& var);    // IRGraph Node
    void readJson(const cJSON* obj, SVFStmt*& stmt);  // IRGraph Edge
    void readJson(const cJSON* obj, ICFGNode*& node); // ICFG Node
    void readJson(const cJSON* obj, ICFGEdge*& edge); // ICFG Edge
    // void readJson(const cJSON* obj, CHGraph*& graph); // CHGraph Graph
    void readJson(const cJSON* obj, CHNode*& node); // CHGraph Node
    void readJson(const cJSON* obj, CHEdge*& edge); // CHGraph Edge
    void readJson(const cJSON* obj, CallSite& cs);

    void readJson(const cJSON* obj, LocationSet& ls);
    void readJson(const cJSON* obj, SVFLoop*& loop);
    void readJson(const cJSON* obj, MemObj*& memObj);
    void readJson(const cJSON* obj,
                  ObjTypeInfo*& objTypeInfo); // Only owned by MemObj
    void readJson(const cJSON* obj,
                  SVFLoopAndDomInfo*& ldInfo); // Only owned by SVFFunction

    template <unsigned ElementSize>
    inline void readJson(const cJSON* obj, SparseBitVector<ElementSize>& bv)
    {
        readJson(obj, bv.Elements);
    }

    template <unsigned ElementSize>
    inline void readJson(const cJSON* obj,
                         SparseBitVectorElement<ElementSize>& element)
    {
        readJson(obj, element.Bits);
    }

    template <typename T>
    inline SVFUtil::void_t<KindBaseT<T>> readJson(const cJSON* obj, T*& ptr)
    {
        KindBaseT<T>* basePtr;
        readJson(obj, basePtr);
        ptr = SVFUtil::dyn_cast<T>(basePtr);
        ABORT_IFNOT(ptr, obj->string << " shoudn't have kind "
                                     << KindBaseHelper<T>::getKind(ptr));
    }

    template <typename T> inline void readJson(const cJSON* obj, const T*& cptr)
    {
        T* ptr;
        readJson(obj, ptr);
        cptr = ptr;
    }

    template <typename T1, typename T2>
    void readJson(const cJSON* obj, std::pair<T1, T2>& pair)
    {
        auto jpair = jsonUnpackPair(obj);
        readJson(jpair.first, pair.first);
        readJson(jpair.second, pair.second);
    }

    template <typename T, size_t N>
    void readJson(const cJSON* obj, T (&array)[N])
    {
        static_assert(N > 0, "array size should be greater than 0");
        ABORT_IFNOT(jsonIsArray(obj), "array expects an array");
        size_t i = 0;
        jsonForEach(elemJson, obj)
        {
            readJson(elemJson, array[i]);
            if (++i >= N)
                break;
        }
        ABORT_IFNOT(i == N, "expect array of size " << N);
    }

    template <typename C>
    std::enable_if_t<SVFUtil::is_sequence_container_v<C>> readJson(
        const cJSON* obj, C& container)
    {
        using T = typename C::value_type;
        assert(container.empty() && "container should be empty");
        ABORT_IFNOT(jsonIsArray(obj), "vector expects an array");
        jsonForEach(elemJson, obj)
            container.push_back(std::move(constructFromJson<T>(elemJson)));
    }

    template <typename C>
    std::enable_if_t<SVFUtil::is_map_v<C>> readJson(const cJSON* obj, C& map)
    {
        assert(map.empty() && "map should be empty");
        ABORT_IFNOT(jsonIsMap(obj), "expects an map (represted by array)");
        jsonForEach(elemJson, obj)
        {
            auto jpair = jsonUnpackPair(elemJson);
            auto k = constructFromJson<typename C::key_type>(jpair.first);
            auto v = constructFromJson<typename C::mapped_type>(jpair.second);
            map.emplace(std::move(k), std::move(v));
        }
    }

    template <typename C>
    std::enable_if_t<SVFUtil::is_set_v<C>> readJson(const cJSON* obj, C& set)
    {
        using T = typename C::value_type;
        assert(set.empty() && "set should be empty");
        ABORT_IFNOT(jsonIsArray(obj), "expects an array");
        jsonForEach(elemJson, obj)
            set.insert(std::move(constructFromJson<T>(elemJson)));
    }

    // IGRaph
    void virtFill(const cJSON*& fieldJson, SVFVar* var);
    void fill(const cJSON*& fieldJson, SVFVar* var);
    void fill(const cJSON*& fieldJson, ValVar* var);
    void fill(const cJSON*& fieldJson, ObjVar* var);
    void fill(const cJSON*& fieldJson, GepValVar* var);
    void fill(const cJSON*& fieldJson, GepObjVar* var);
    void fill(const cJSON*& fieldJson, FIObjVar* var);
    void fill(const cJSON*& fieldJson, RetPN* var);
    void fill(const cJSON*& fieldJson, VarArgPN* var);
    void fill(const cJSON*& fieldJson, DummyValVar* var);
    void fill(const cJSON*& fieldJson, DummyObjVar* var);

    void virtFill(const cJSON*& fieldJson, SVFStmt* stmt);
    void fill(const cJSON*& fieldJson, SVFStmt* stmt);
    void fill(const cJSON*& fieldJson, AssignStmt* stmt);
    void fill(const cJSON*& fieldJson, AddrStmt* stmt);
    void fill(const cJSON*& fieldJson, CopyStmt* stmt);
    void fill(const cJSON*& fieldJson, StoreStmt* stmt);
    void fill(const cJSON*& fieldJson, LoadStmt* stmt);
    void fill(const cJSON*& fieldJson, GepStmt* stmt);
    void fill(const cJSON*& fieldJson, CallPE* stmt);
    void fill(const cJSON*& fieldJson, RetPE* stmt);
    void fill(const cJSON*& fieldJson, MultiOpndStmt* stmt);
    void fill(const cJSON*& fieldJson, PhiStmt* stmt);
    void fill(const cJSON*& fieldJson, SelectStmt* stmt);
    void fill(const cJSON*& fieldJson, CmpStmt* stmt);
    void fill(const cJSON*& fieldJson, BinaryOPStmt* stmt);
    void fill(const cJSON*& fieldJson, UnaryOPStmt* stmt);
    void fill(const cJSON*& fieldJson, BranchStmt* stmt);
    void fill(const cJSON*& fieldJson, TDForkPE* stmt);
    void fill(const cJSON*& fieldJson, TDJoinPE* stmt);

    void fill(const cJSON*& fieldJson, MemObj* memObj);
    void fill(const cJSON*& fieldJson, StInfo* stInfo);
    // ICFG
    void virtFill(const cJSON*& fieldJson, ICFGNode* node);
    void fill(const cJSON*& fieldJson, ICFGNode* node);
    void fill(const cJSON*& fieldJson, GlobalICFGNode* node);
    void fill(const cJSON*& fieldJson, IntraICFGNode* node);
    void fill(const cJSON*& fieldJson, InterICFGNode* node);
    void fill(const cJSON*& fieldJson, FunEntryICFGNode* node);
    void fill(const cJSON*& fieldJson, FunExitICFGNode* node);
    void fill(const cJSON*& fieldJson, CallICFGNode* node);
    void fill(const cJSON*& fieldJson, RetICFGNode* node);

    void virtFill(const cJSON*& fieldJson, ICFGEdge* node);
    void fill(const cJSON*& fieldJson, ICFGEdge* edge);
    void fill(const cJSON*& fieldJson, IntraCFGEdge* edge);
    void fill(const cJSON*& fieldJson, CallCFGEdge* edge);
    void fill(const cJSON*& fieldJson, RetCFGEdge* edge);

    void fill(const cJSON*& fieldJson, SVFLoop* loop);
    // CHGraph
    void virtFill(const cJSON*& fieldJson, CHNode* node);
    void virtFill(const cJSON*& fieldJson, CHEdge* edge);

    // SVFModule
    void virtFill(const cJSON*& fieldJson, SVFValue* value);
    void fill(const cJSON*& fieldJson, SVFValue* value);
    void fill(const cJSON*& fieldJson, SVFFunction* value);
    void fill(const cJSON*& fieldJson, SVFBasicBlock* value);
    void fill(const cJSON*& fieldJson, SVFInstruction* value);
    void fill(const cJSON*& fieldJson, SVFCallInst* value);
    void fill(const cJSON*& fieldJson, SVFVirtualCallInst* value);
    void fill(const cJSON*& fieldJson, SVFConstant* value);
    void fill(const cJSON*& fieldJson, SVFGlobalValue* value);
    void fill(const cJSON*& fieldJson, SVFArgument* value);
    void fill(const cJSON*& fieldJson, SVFConstantData* value);
    void fill(const cJSON*& fieldJson, SVFConstantInt* value);
    void fill(const cJSON*& fieldJson, SVFConstantFP* value);
    void fill(const cJSON*& fieldJson, SVFConstantNullPtr* value);
    void fill(const cJSON*& fieldJson, SVFBlackHoleValue* value);
    void fill(const cJSON*& fieldJson, SVFOtherValue* value);
    void fill(const cJSON*& fieldJson, SVFMetadataAsValue* value);

    void virtFill(const cJSON*& fieldJson, SVFType* type);
    void fill(const cJSON*& fieldJson, SVFType* type);
    void fill(const cJSON*& fieldJson, SVFPointerType* type);
    void fill(const cJSON*& fieldJson, SVFIntegerType* type);
    void fill(const cJSON*& fieldJson, SVFFunctionType* type);
    void fill(const cJSON*& fieldJson, SVFStructType* type);
    void fill(const cJSON*& fieldJson, SVFArrayType* type);
    void fill(const cJSON*& fieldJson, SVFOtherType* type);

    template <typename NodeTy, typename EdgeTy>
    void fill(const cJSON*& fieldJson, GenericNode<NodeTy, EdgeTy>* node)
    {
        // id and nodeKind have already been read.
        JSON_READ_FIELD_FWD(fieldJson, node, InEdges);
        JSON_READ_FIELD_FWD(fieldJson, node, OutEdges);
    }

    template <typename NodeTy>
    void fill(const cJSON*& fieldJson, GenericEdge<NodeTy>* edge)
    {
        // edgeFlag has already been read.
        JSON_READ_FIELD_FWD(fieldJson, edge, src);
        JSON_READ_FIELD_FWD(fieldJson, edge, dst);
    }

public:
    // Helper functions
    static inline s64_t applyEdgeMask(u64_t edgeFlag)
    {
        return edgeFlag & GenericEdge<void>::EdgeKindMask;
    }
    template <typename T>
    static inline void setEdgeFlag(GenericEdge<T>* edge,
                                   typename GenericEdge<T>::GEdgeFlag edgeFlag)
    {
        edge->edgeFlag = edgeFlag;
    }
};

} // namespace SVF

#endif // !INCLUDE_SVFIRRW_H_
