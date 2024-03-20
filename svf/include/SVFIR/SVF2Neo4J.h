//===- SVF2Neo4J.h -- SVF IR Reader and Writer ------------------------===//
//
//  SVF - Static Value-Flow Analysis Framework
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2024>  <Yulei Sui>
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

#ifndef INCLUDE_SVF2NEO4J_H_
#define INCLUDE_SVF2NEO4J_H_

#include "Graphs/GenericGraph.h"
#include "Util/SVFUtil.h"
// #include "Util/cJSON.h"
#include <type_traits>
#include "SVFIR/Neo4jClient.h"


#define ABORT_MSG(reason)                                                      \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << __FILE__ << ':' << __LINE__ << ": " << reason       \
                        << '\n';                                               \
        abort();                                                               \
    } while (0)
#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            ABORT_MSG(reason);                                                 \
    } while (0)


#define FIELD_NAME_ITEM(field) #field, (field)

#define ITEM_FIELD_OR(item, field, default) ((item) ? (item)->field : default)
#define ITEM_KEY(item) ITEM_FIELD_OR(item, string, "NULL")
#define ITEM_CHILD(item) JSON_FIELD_OR(item, child, nullptr)

#define ITEM_WRITE_FIELD(root, objptr, field)                                  \
    itemAddItemableToObject(root, #field, (objptr)->field)

#define CITEM_PUBLIC(type) type

namespace SVF
{
/// @brief Forward declarations.
///@{
class NodeIDAllocator;
// Classes created upon SVFMoudle construction
class SVFType;
class SVFPointerType;
class SVFIntegerType;
class SVFFunctionType;
class SVFStructType;
class SVFArrayType;
class SVFOtherType;

class StInfo; // Every SVFType is linked to a StInfo. It also references SVFType

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

class SVFLoopAndDomInfo; // Part of SVFFunction

// Classes created upon buildSymbolTableInfo
class MemObj;

// Classes created upon ICFG construction
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

class SVFIR;
class SVFIRWriter;
class SVFLoop;
class ICFG;
class IRGraph;
class CHGraph;
class CommonCHGraph;
class SymbolTableInfo;

class SVFModule;

class AccessPath;
class ObjTypeInfo; // Need SVFType

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

class CHNode;
class CHEdge;
class CHGraph;
// End of forward declarations
///@}

#include <stddef.h>

/* cITEM Types: */
#define cITEM_Invalid (0)
#define cITEM_False  (1 << 0)
#define cITEM_True   (1 << 1)
#define cITEM_NULL   (1 << 2)
#define cITEM_Number (1 << 3)
#define cITEM_String (1 << 4)
#define cITEM_Array  (1 << 5)
#define cITEM_Object (1 << 6)
#define cITEM_Raw    (1 << 7) /* raw item */
/* The cITEM structure: */
typedef struct cITEM
{
    /* next/prev allow you to walk array/object chains. */
    struct cITEM *next;
    struct cITEM *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct cITEM *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==cITEM_String and type == cITEM_Raw */
    char *valuestring;
    /* writing to valueint is DEPRECATED, use cITEM_SetNumberValue instead */
    int valueint;
    /* The item's number, if type==cITEM_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} cITEM;

/* Delete a cITEM entity and all subentities. */
CITEM_PUBLIC(void) cITEM_Delete(cITEM *item);
/* malloc/free objects using the malloc/free functions that have been set with cJSON_InitHooks */
CITEM_PUBLIC(void *) cITEM_malloc(size_t size);
CITEM_PUBLIC(void) cITEM_free(void *object);

/* Render a cJSON entity to text for transfer/storage. */
CITEM_PUBLIC(char *) cITEM_Print(const cITEM *item);

bool itemIsBool(const cITEM* item);
bool itemIsBool(const cITEM* item, bool& flag);
bool itemIsNumber(const cITEM* item);
bool itemIsString(const cITEM* item);
bool itemIsNullId(const cITEM* item);
bool itemIsArray(const cITEM* item);
bool itemIsMap(const cITEM* item);
bool itemIsObject(const cITEM* item);
bool itemKeyEquals(const cITEM* item, const char* key);
std::pair<const cITEM*, const cITEM*> itemUnpackPair(const cITEM* item);
double itemGetNumber(const cITEM* item);
cITEM* itemCreateNullId();
cITEM* itemCreateObject();
cITEM* itemCreateArray();
cITEM* itemCreateString(const char* str);
cITEM* itemCreateIndex(size_t index);
cITEM* itemCreateBool(bool flag);
cITEM* itemCreateNumber(double num);
bool itemAddPairToMap(cITEM* obj, cITEM* key, cITEM* value);
bool itemAddItemToObject(cITEM* obj, const char* name, cITEM* item);
bool itemAddItemToArray(cITEM* array, cITEM* item);
/// @brief Helper function to write a number to a ITEM object.
bool itemAddNumberToObject(cITEM* obj, const char* name, double number);
bool itemAddStringToObject(cITEM* obj, const char* name, const char* str);
bool itemAddStringToObject(cITEM* obj, const char* name, const std::string& s);
#define itemForEach(field, array)                                              \
    for (const cITEM* field = ITEM_CHILD(array); field; field = field->next)

/// @brief Bookkeeping class to keep track of the IDs of objects that doesn't
/// have any ID. E.g., SVFValue, XXXEdge.
/// @tparam T
template <typename T> 
class WriterDbPtrPool
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

template <typename NodeTy, typename EdgeTy> 
class GenericGraphDbWriter
{
    friend class SVFIRDbWriter;

private:
    using NodeType = NodeTy;
    using EdgeType = EdgeTy;
    using GraphType = GenericGraph<NodeType, EdgeType>;

    // const GraphType* graph;
    WriterDbPtrPool<EdgeType> edgePool;

public:
    GenericGraphDbWriter(const GraphType* graph)
    {
        assert(graph && "Graph pointer should never be null");
        edgePool.reserve(graph->getTotalEdgeNum());


        // for (const auto& pair : graph->IDToNodeMap)
        // {
        //     const NodeType* node = pair.second;

        //     for (const EdgeType* edge : node->getOutEdges())
        //     {
        //         edgePool.saveID(edge);
        //     }
        // }
    }

    inline size_t getEdgeID(const EdgeType* edge)
    {
        return edgePool.getID(edge);
    }
};

using GenericICFGDbWriter = GenericGraphDbWriter<ICFGNode, ICFGEdge>;

class ICFGDbWriter : public GenericICFGDbWriter
{
    friend class SVFIRWriter;

private:
    WriterDbPtrPool<SVFLoop> svfLoopPool;

public:
    ICFGDbWriter(const ICFG* icfg);

    inline size_t getSvfLoopID(const SVFLoop* loop)
    {
        return svfLoopPool.getID(loop);
    }
};

using IRGraphDbWriter = GenericGraphDbWriter<SVFVar, SVFStmt>;
using CHGraphDbWriter = GenericGraphDbWriter<CHNode, CHEdge>;

class SVFModuleDbWriter
{
    friend class SVFIRWriter;

private:
    WriterDbPtrPool<SVFType> svfTypePool;
    WriterDbPtrPool<StInfo> stInfoPool;
    WriterDbPtrPool<SVFValue> svfValuePool;

public:
    SVFModuleDbWriter(const SVFModule* svfModule);

    inline size_t getSVFValueID(const SVFValue* value)
    {
        return svfValuePool.getID(value);
    }
    inline const SVFValue* getSVFValuePtr(size_t id) const
    {
        return svfValuePool.getPtr(id);
    }
    inline size_t getSVFTypeID(const SVFType* type)
    {
        return svfTypePool.getID(type);
    }
    inline size_t getStInfoID(const StInfo* stInfo)
    {
        return stInfoPool.getID(stInfo);
    }
    inline size_t sizeSVFValuePool() const
    {
        return svfValuePool.size();
    }
};


class SVFIRDbWriter
{
private:
    const SVFIR* svfIR;

    SVFModuleDbWriter svfModuleWriter;
    ICFGDbWriter icfgWriter;
    CHGraphDbWriter chgWriter;
    IRGraphDbWriter irGraphWriter;
    Neo4jClient* db;

    OrderedMap<size_t, std::string> numToStrMap;

public:
    using autoITEM = std::unique_ptr<cITEM, decltype(&cITEM_Delete)>;
    // using autoCStr = std::unique_ptr<char, decltype(&cITEM_free)>;

    /// @brief Constructor.
    SVFIRDbWriter(const SVFIR* svfir);

    static void writeSVFIRToOstream(const SVFIR* svfir, std::ostream& os);
    static void writeToDatabase(const SVFIR* svfir, const std::string& path);

private:
    /// @brief Main logic to dump a SVFIR to a Database Items object.
    autoITEM generateItems();
    const char * generateDataBaseItems();

    const char* numToStr(size_t n);

    cITEM* toItem(const NodeIDAllocator* nodeIDAllocator);
    cITEM* toItem(const SymbolTableInfo* symTable);
    cITEM* toItem(const SVFModule* module);
    cITEM* toItem(const SVFType* type);
    cITEM* toItem(const SVFValue* value);
    cITEM* toItem(const IRGraph* graph);       // IRGraph Graph
    cITEM* toItem(const SVFVar* var);          // IRGraph Node
    cITEM* toItem(const SVFStmt* stmt);        // IRGraph Edge
    cITEM* toItem(const ICFG* icfg);           // ICFG Graph
    cITEM* toItem(const ICFGNode* node);       // ICFG Node
    cITEM* toItem(const ICFGEdge* edge);       // ICFG Edge
    cITEM* toItem(const CommonCHGraph* graph); // CHGraph Graph
    cITEM* toItem(const CHGraph* graph);       // CHGraph Graph
    cITEM* toItem(const CHNode* node);         // CHGraph Node
    cITEM* toItem(const CHEdge* edge);         // CHGraph Edge

    cITEM* toItem(const CallSite& cs);
    cITEM* toItem(const AccessPath& ap);
    cITEM* toItem(const SVFLoop* loop);
    cITEM* toItem(const MemObj* memObj);
    cITEM* toItem(const ObjTypeInfo* objTypeInfo);  // Only owned by MemObj
    cITEM* toItem(const SVFLoopAndDomInfo* ldInfo); // Only owned by SVFFunction
    cITEM* toItem(const StInfo* stInfo);

    // No need for 'toItem(short)' because of promotion to int
    static cITEM* toItem(bool flag);
    static cITEM* toItem(unsigned number);
    static cITEM* toItem(int number);
    static cITEM* toItem(float number);
    static cITEM* toItem(const std::string& str);
    cITEM* toItem(unsigned long number);
    cITEM* toItem(long long number);
    cITEM* toItem(unsigned long long number);

    /// \brief Parameter types of these functions are all pointers.
    /// When they are used as arguments of toItem(), they will be
    /// dumped as an index. `contentToItem()` will dump the actual content.
    ///@{
    cITEM* virtToItem(const SVFType* type);
    cITEM* virtToItem(const SVFValue* value);
    cITEM* virtToItem(const SVFVar* var);
    cITEM* virtToItem(const SVFStmt* stmt);
    cITEM* virtToItem(const ICFGNode* node);
    cITEM* virtToItem(const ICFGEdge* edge);
    cITEM* virtToItem(const CHNode* node);
    cITEM* virtToItem(const CHEdge* edge);

    // Classes inherited from SVFVar
    cITEM* contentToItem(const SVFVar* var);
    cITEM* contentToItem(const ValVar* var);
    cITEM* contentToItem(const ObjVar* var);
    cITEM* contentToItem(const GepValVar* var);
    cITEM* contentToItem(const GepObjVar* var);
    cITEM* contentToItem(const FIObjVar* var);
    cITEM* contentToItem(const RetPN* var);
    cITEM* contentToItem(const VarArgPN* var);
    cITEM* contentToItem(const DummyValVar* var);
    cITEM* contentToItem(const DummyObjVar* var);

    // Classes inherited from SVFStmt
    cITEM* contentToItem(const SVFStmt* edge);
    cITEM* contentToItem(const AssignStmt* edge);
    cITEM* contentToItem(const AddrStmt* edge);
    cITEM* contentToItem(const CopyStmt* edge);
    cITEM* contentToItem(const StoreStmt* edge);
    cITEM* contentToItem(const LoadStmt* edge);
    cITEM* contentToItem(const GepStmt* edge);
    cITEM* contentToItem(const CallPE* edge);
    cITEM* contentToItem(const RetPE* edge);
    cITEM* contentToItem(const MultiOpndStmt* edge);
    cITEM* contentToItem(const PhiStmt* edge);
    cITEM* contentToItem(const SelectStmt* edge);
    cITEM* contentToItem(const CmpStmt* edge);
    cITEM* contentToItem(const BinaryOPStmt* edge);
    cITEM* contentToItem(const UnaryOPStmt* edge);
    cITEM* contentToItem(const BranchStmt* edge);
    cITEM* contentToItem(const TDForkPE* edge);
    cITEM* contentToItem(const TDJoinPE* edge);

    // Classes inherited from ICFGNode
    cITEM* contentToItem(const ICFGNode* node);
    cITEM* contentToItem(const GlobalICFGNode* node);
    cITEM* contentToItem(const IntraICFGNode* node);
    cITEM* contentToItem(const InterICFGNode* node);
    cITEM* contentToItem(const FunEntryICFGNode* node);
    cITEM* contentToItem(const FunExitICFGNode* node);
    cITEM* contentToItem(const CallICFGNode* node);
    cITEM* contentToItem(const RetICFGNode* node);

    // Classes inherited from ICFGEdge
    cITEM* contentToItem(const ICFGEdge* edge);
    cITEM* contentToItem(const IntraCFGEdge* edge);
    cITEM* contentToItem(const CallCFGEdge* edge);
    cITEM* contentToItem(const RetCFGEdge* edge);

    // CHNode & CHEdge
    cITEM* contentToItem(const CHNode* node);
    cITEM* contentToItem(const CHEdge* edge);

    cITEM* contentToItem(const SVFType* type);
    cITEM* contentToItem(const SVFPointerType* type);
    cITEM* contentToItem(const SVFIntegerType* type);
    cITEM* contentToItem(const SVFFunctionType* type);
    cITEM* contentToItem(const SVFStructType* type);
    cITEM* contentToItem(const SVFArrayType* type);
    cITEM* contentToItem(const SVFOtherType* type);

    cITEM* contentToItem(const SVFValue* value);
    cITEM* contentToItem(const SVFFunction* value);
    cITEM* contentToItem(const SVFBasicBlock* value);
    cITEM* contentToItem(const SVFInstruction* value);
    cITEM* contentToItem(const SVFCallInst* value);
    cITEM* contentToItem(const SVFVirtualCallInst* value);
    cITEM* contentToItem(const SVFConstant* value);
    cITEM* contentToItem(const SVFGlobalValue* value);
    cITEM* contentToItem(const SVFArgument* value);
    cITEM* contentToItem(const SVFConstantData* value);
    cITEM* contentToItem(const SVFConstantInt* value);
    cITEM* contentToItem(const SVFConstantFP* value);
    cITEM* contentToItem(const SVFConstantNullPtr* value);
    cITEM* contentToItem(const SVFBlackHoleValue* value);
    cITEM* contentToItem(const SVFOtherValue* value);
    cITEM* contentToItem(const SVFMetadataAsValue* value);

    // Other classes
    cITEM* contentToItem(const SVFLoop* loop);
    cITEM* contentToItem(const MemObj* memObj); // Owned by SymbolTable->objMap
    cITEM* contentToItem(const StInfo* stInfo);
    ///@}

    template <typename NodeTy, typename EdgeTy>
    cITEM* genericNodeToJson(const GenericNode<NodeTy, EdgeTy>* node)
    {
        cITEM* root = itemCreateObject();
        ITEM_WRITE_FIELD(root, node, id);
        ITEM_WRITE_FIELD(root, node, nodeKind);
        ITEM_WRITE_FIELD(root, node, InEdges);
        ITEM_WRITE_FIELD(root, node, OutEdges);
        return root;
    }

    template <typename NodeTy>
    cITEM* genericEdgeToJson(const GenericEdge<NodeTy>* edge)
    {
        cITEM* root = itemCreateObject();
        ITEM_WRITE_FIELD(root, edge, edgeFlag);
        ITEM_WRITE_FIELD(root, edge, src);
        ITEM_WRITE_FIELD(root, edge, dst);
        return root;
    }

    template <typename NodeTy, typename EdgeTy>
    cITEM* genericGraphToJson(const GenericGraph<NodeTy, EdgeTy>* graph,
                              const std::vector<const EdgeTy*>& edgePool)
    {
        cITEM* root = itemCreateObject();

        cITEM* allNode = itemCreateArray();
        for (const auto& pair : graph->IDToNodeMap)
        {
            NodeTy* node = pair.second;
            cITEM* itemNode = virtToItem(node);
            itemAddItemToArray(allNode, itemNode);
        }

        cITEM* allEdge = itemCreateArray();
        for (const EdgeTy* edge : edgePool)
        {
            cITEM* edgeJson = virtToItem(edge);
            itemAddItemToArray(allEdge, edgeJson);
        }

        ITEM_WRITE_FIELD(root, graph, nodeNum);
        itemAddItemToObject(root, FIELD_NAME_ITEM(allNode));
        ITEM_WRITE_FIELD(root, graph, edgeNum);
        itemAddItemToObject(root, FIELD_NAME_ITEM(allEdge));

        return root;
    }

    /** The following 2 functions are intended to convert SparseBitVectors
     * to JSON. But they're buggy. Commenting them out would enable the
     * toItem(T) where is_iterable_v<T> is true. But that implementation is less
     * space-efficient if the bitvector contains many elements.
     * It is observed that upon construction, SVF IR bitvectors contain at most
     * 1 element. In that case, we can just use the toItem(T) for iterable T
     * without much space overhead.

    template <unsigned ElementSize>
    cITEM* toItem(const SparseBitVectorElement<ElementSize>& element)
    {
        cITEM* array = itemCreateArray();
        for (const auto v : element.Bits)
        {
            itemAddItemToArray(array, toItem(v));
        }
        return array;
    }

    template <unsigned ElementSize>
    cITEM* toItem(const SparseBitVector<ElementSize>& bv)
    {
        return toItem(bv.Elements);
    }
    */

    template <typename T, typename U> cITEM* toItem(const std::pair<T, U>& pair)
    {
        cITEM* obj = itemCreateArray();
        itemAddItemToArray(obj, toItem(pair.first));
        itemAddItemToArray(obj, toItem(pair.second));
        return obj;
    }

    template <typename T,
              typename = std::enable_if_t<SVFUtil::is_iterable_v<T>>>
                                          cITEM* toItem(const T& container)
    {
        cITEM* array = itemCreateArray();
        for (const auto& item : container)
        {
            cITEM* itemObj = toItem(item);
            itemAddItemToArray(array, itemObj);
        }
        return array;
    }

    template <typename T>
    bool itemAddItemableToObject(cITEM* obj, const char* name, const T& item)
    {
        cITEM* itemObj = toItem(item);
        return itemAddItemToObject(obj, name, itemObj);
    }

    template <typename T>
    bool itemAddContentToObject(cITEM* obj, const char* name, const T& item)
    {
        cITEM* itemObj = contentToItem(item);
        return itemAddItemToObject(obj, name, itemObj);
    }
};

/*
 * Reader Part
 */





} // namespace SVF

#endif // !INCLUDE_SVF2NEO4J_H_
