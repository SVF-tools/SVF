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

#include "Util/SVFUtil.h"
#include "Util/cJSON.h"
#include "Graphs/GenericGraph.h"
#include <type_traits>

#define JSON_KEY_ST_INFO "stInfoPool"

#define SVFIR_DEBUG 1
#if SVFIR_DEBUG
#    define ENSURE_NOT_VISITED(graph)                                          \
        do                                                                     \
        {                                                                      \
            static std::set<decltype(graph)> visited;                          \
            bool inserted = visited.insert(graph).second;                      \
            ABORT_IFNOT(!inserted, #graph " already visited!");                \
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

#define JSON_WRITE_FIELD(root, objptr, field)                                  \
    jsonAddJsonableToObject(root, #field, (objptr)->field)

/// @brief Type trait to check if a type is iterable.
///@{
template <typename T>
decltype(std::begin(std::declval<T&>()) !=
         std::end(std::declval<T&>()), // begin/end and operator!=
         void(),                           // Handle evil operator,
         ++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator++
         *begin(std::declval<T&>()),                             // operator*
         std::true_type {})
is_iterable_impl(int);
template <typename T> std::false_type is_iterable_impl(...);
template <typename T> using is_iterable = decltype(is_iterable_impl<T>(0));
template <typename T> constexpr bool is_iterable_v = is_iterable<T>::value;
///@}

/// @brief Type trait to check if a type is a map or unordered_map.
///@{
template <typename T> struct is_map : std::false_type {};
template <typename... Ts> struct is_map<std::map<Ts...>> : std::true_type {};
template <typename... Ts> struct is_map<std::unordered_map<Ts...>> : std::true_type {};
template <typename... Ts> constexpr bool is_map_v = is_map<Ts...>::value;
///@}

/// @brief Type trait to check if a type is a set or unordered_set.
///@{
template <typename T> struct is_set : std::false_type {};
template <typename... Ts> struct is_set<std::set<Ts...>> : std::true_type {};
template <typename... Ts> struct is_set<std::unordered_set<Ts...>> : std::true_type {};
template <typename... Ts> constexpr bool is_set_v = is_set<Ts...>::value;
///@}

/// @brief Type trait to check if a type is vector or list.
template <typename T> struct is_sequence_container : std::false_type {};
template <typename... Ts> struct is_sequence_container<std::vector<Ts...>> : std::true_type {};
template <typename... Ts> struct is_sequence_container<std::deque<Ts...>> : std::true_type {};
template <typename... Ts> struct is_sequence_container<std::list<Ts...>> : std::true_type {};
template <typename... Ts> constexpr bool is_sequence_container_v = is_sequence_container<Ts...>::value;
///@}

namespace SVF
{
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

class CHNode;
class CHEdge;
class CHGraph;

cJSON* jsonCreateNullId();
bool jsonIsNumber(const cJSON* item);
bool jsonIsNullId(const cJSON* item);
bool jsonIsArray(const cJSON* item);
bool jsonIsMap(const cJSON* item);
bool jsonIsObject(const cJSON* item);
bool jsonKeyEquals(const cJSON* item, const char* key);
void jsonUnpackPair(const cJSON* item, const cJSON*& key, const cJSON*& value);
cJSON* jsonCreateObject();
cJSON* jsonCreateArray();
cJSON* jsonCreateString(const char* str);
cJSON* jsonCreateIndex(size_t index);
cJSON* jsonCreateNumber(double num);
bool jsonAddPairToMap(cJSON* obj, cJSON* key, cJSON* value);
bool jsonAddItemToObject(cJSON* obj, const char* name, cJSON* item);
bool jsonAddItemToArray(cJSON* array, cJSON* item);
/// @brief Helper function to write a number to a JSON object.
bool jsonAddNumberToObject(cJSON* obj, const char* name, double number);
bool jsonAddStringToObject(cJSON* obj, const char* name, const char* str);
bool jsonAddStringToObject(cJSON* obj, const char* name,
                           const std::string& str);
#define jsonForEach(element, array)                                            \
    for (const cJSON* element = (array != NULL) ? (array)->child : NULL;       \
         element != NULL; element = element->next)
#define jsonKeyMatch(element) jsonKeyEquals(element, #element)
/*
#define jsonCheck(element, jsonType)                                           \
    ABORT_IFNOT((jsonIs##jsonType(element) && jsonKeyMatch(element)),          \
                ("Expect " #jsonType " with name " #element))
            */

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

private:
    // const SymbolTableInfo* symbolTableInfo; // Don't need to keep this?
    WriterPtrPool<SVFType> svfTypePool;
};

using IRGraphWriter = GenericGraphWriter<SVFVar, SVFStmt>;
using CHGraphWriter = GenericGraphWriter<CHNode, CHEdge>;

class SVFModuleWriter
{
    friend class SVFIRWriter;

    WriterPtrPool<SVFValue> svfValuePool;
    size_t getSvfValueID(const SVFValue* value);
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
    WriterPtrPool<StInfo> stInfoPool; // Owned by LLVMModuleSet. Can't access LLVM.

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

    cJSON* toJson(const SVFModule* module);
    cJSON* toJson(const SVFType* type);
    cJSON* toJson(const SVFValue* value);
    cJSON* toJson(const IRGraph* graph); // IRGraph Graph
    cJSON* toJson(const SVFVar* var);    // IRGraph Node
    cJSON* toJson(const SVFStmt* stmt);  // IRGraph Edge
    cJSON* toJson(const ICFG* icfg);     // ICFG Graph
    cJSON* toJson(const ICFGNode* node); // ICFG Node
    cJSON* toJson(const ICFGEdge* edge); // ICFG Edge
    cJSON* toJson(const CommonCHGraph* graph); // CHGraph Graph
    cJSON* toJson(const CHGraph* graph); // CHGraph Graph
    cJSON* toJson(const CHNode* node);   // CHGraph Node
    cJSON* toJson(const CHEdge* edge);   // CHGraph Edge

    cJSON* toJson(const CallSite& cs);
    cJSON* toJson(const LocationSet& ls);
    cJSON* toJson(const SVFLoop* loop);
    cJSON* toJson(const MemObj* memObj);
    cJSON* toJson(const ObjTypeInfo* objTypeInfo); // Only owned by MemObj
    cJSON* toJson(const SVFLoopAndDomInfo* ldInfo); // Only owned by SVFFunction
    cJSON* toJson(const StInfo* stInfo); // Ensure Only owned by SVFType

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
    cJSON* contentToJson(const SymbolTableInfo* symTable);
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

        cJSON* allNodes = jsonCreateArray();
        for (const auto& pair : graph->IDToNodeMap)
        {
            NodeTy* node = pair.second;
            cJSON* jsonNode = virtToJson(node);
            jsonAddItemToArray(allNodes, jsonNode);
        }
        jsonAddItemToObject(root, FIELD_NAME_ITEM(allNodes));

        cJSON* allEdges = jsonCreateArray();
        for (const EdgeTy* edge : edgePool)
        {
            cJSON* edgeJson = virtToJson(edge);
            jsonAddItemToArray(allEdges, edgeJson);
        }
        jsonAddItemToObject(root, FIELD_NAME_ITEM(allEdges));

        return root;
    }

    template <unsigned ElementSize>
    cJSON* toJson(const SparseBitVectorElement<ElementSize> &element)
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

/// @brief Keeps a map from IDs to T objects.
/// @tparam T: The type of the objects.
template <typename T> class ReaderIDToObjMap
{
private:
    OrderedMap<unsigned, std::pair<const cJSON*, T*>> idMap;

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
            ABORT_IFNOT(inserted, "duplicate ID");
        }
    }
};

template <typename T> class ReaderPtrPool
{
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
        assert(jsonArray.empty() && "jsonArray should be empty when creating objects");
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

    T* getPtr(size_t id)
    {
        assert(id <= jsonArray.size() && "Invalid ID");
        return id ? ptrPool[id - 1] : nullptr;
    }

private:
    std::vector<const cJSON*> jsonArray;
    std::vector<T*> ptrPool;
};

template <typename NodeTy, typename EdgeTy> class GenericGraphReader
{
private:
    ReaderIDToObjMap<NodeTy> IdToNodeMap;
    ReaderPtrPool<EdgeTy> edgePool;

protected:
    const cJSON* graphFieldJson = nullptr;

public:
    template <typename NodeCreator, typename EdgeCreator>
    void createObjs(const cJSON* graphJson, NodeCreator nodeCreator, EdgeCreator edgeCreator)
    {
        assert(graphFieldJson == nullptr && "graphFieldJson should be empty");
        // TODO: Check graphJson is an cJSON object
        const cJSON* edgeNumJson = graphJson->child;
        // TODO: Check edgeNumJson is an cJSON number & read it
        const cJSON* nodeNumJson = edgeNumJson->next;
        // TODO: Check nodeNumJson is an cJSON number & read it
        const cJSON* nodesJson = nodeNumJson->next;
        // TODO: Check nodesJson is an cJSON array
        IdToNodeMap.createObjs(nodesJson, nodeCreator);
        // TODO: Check edgesJson
        const cJSON* edgesJson = nodesJson->next;
        edgePool.createObjs(edgesJson, edgeCreator);

        graphFieldJson = edgesJson->next;
    }

    // TODO: fill
};

using GenericICFGReader = GenericGraphReader<ICFGNode, ICFGEdge>;
using IRGraphReader = GenericGraphReader<SVFVar, SVFStmt>;
using CHGraphReader = GenericGraphReader<CHNode, CHEdge>;
class ICFGReader : public GenericICFGReader
{
private:
    ReaderPtrPool<SVFLoop> svfLoopPool;

public:
    void createObjs(const cJSON* icfgJson);
};

class SymbolTableInfoReader
{
private:
    const cJSON* symTabFieldJson = nullptr;
    ReaderPtrPool<SVFType> svfTypePool;
    ReaderIDToObjMap<MemObj> memObjMap;
public:
    void createObjs(const cJSON* symTableJson);
};

class SVFModuleReader
{
    const cJSON* svfModuleFieldJson = nullptr;
    ReaderPtrPool<SVFValue> svfValuePool;
public:
    void createObjs(const cJSON* svfModuleJson);
};

#define JSON_READ_FIELD(json, objptr, field)                                   \
    do                                                                         \
    {                                                                          \
        ABORT_IFNOT(jsonKeyEquals(json, #field), "expect " #field);            \
        readJson(json, #field, (objptr)->field);                               \
        json = json->next;                                                     \
    } while (0)
/* SVFIRReader
 * Read SVFIR from JSON
 */
class SVFIRReader
{
private:
    ReaderPtrPool<StInfo> stInfoPool;
    IRGraphReader irGraphReader;
    SymbolTableInfoReader symTableReader;
    SVFModuleReader svfModuleReader;
    ICFGReader icfgReader;
    CHGraphReader chGraphReader;
public:
    static inline s64_t applyEdgeMask(u64_t edgeFlag) {
        return edgeFlag & GenericEdge<void>::EdgeKindMask;
    }

    void readJson(cJSON* root);

    static void readJson(const cJSON* obj, long long& val);
    static void readJson(const cJSON* obj, unsigned& val);
    static void readJson(const cJSON* obj, unsigned long& val);
    static void readJson(const cJSON* obj, unsigned long long& val);
    void readJson(const cJSON* obj, SVFIR*& svfIR);
    void readJson(const cJSON* obj, SVFModule*& module);
    void readJson(const cJSON* obj, SVFType*& type);

    template <typename Container>
    std::enable_if_t<is_sequence_container_v<Container>>
    readJson(const cJSON* obj, Container& container)
    {
        assert(container.empty() && "container should be empty");
        ABORT_IFNOT(jsonIsArray(obj), "vector expects an array");
        jsonForEach(elemJson, obj)
        {
            std::remove_const_t<typename Container::value_type> elem;
            readJson(elemJson, elem);
            container.push_back(std::move(elem));
            elemJson = elemJson->next;
        }
    }

    template <typename Map>
    std::enable_if_t<is_map_v<Map>> readJson(const cJSON* obj, Map& map)
    {
        assert(map.empty() && "map should be empty");
        ABORT_IFNOT(jsonIsMap(obj), "expects an map (represted by array)");
        jsonForEach(elemJson, obj)
        {
            const cJSON* keyJson;
            const cJSON* valJson;
            jsonUnpackPair(elemJson, keyJson, valJson);
            std::remove_const_t<typename Map::key_type> key;
            std::remove_const_t<typename Map::mapped_type> val;
            readJson(keyJson, key);
            readJson(valJson, val);
            map.emplace(std::move(key), std::move(val));
            elemJson = elemJson->next;
        }
    }

    //template <typename Set, typename = std::enable_if_t<is_set_v<Set>>>
    //void readJson(const)
    void fill(const cJSON*& fieldJson, StInfo* stInfo);
private:
};

} // namespace SVF

#endif // !INCLUDE_SVFIRRW_H_