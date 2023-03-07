#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFIRRW.h"
#include "Util/CommandLine.h"
#include "Graphs/CHG.h"

static const Option<bool> humanReadableOption(
    "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{

cJSON* SVFIRWriter::toJson(const LocationSet& ls)
{
    // TODO: JSON
    return jsonCreateString("TODO: LocationSet");
}

cJSON* SVFIRWriter::toJson(unsigned number)
{
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(int number)
{
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(long long number)
{
    // TODO: check number range?
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::virtToJson(const SVFVar* var)
{
    return nullptr;
}

cJSON* SVFIRWriter::virtToJson(const SVFStmt* stmt)
{
    return nullptr;
}

cJSON* SVFIRWriter::virtToJson(const ICFGNode* node)
{
    switch (node->getNodeKind())
    {
        default:
            assert(false && "Unknown ICFGNode kind");
        case ICFGNode::IntraBlock:
            return contentToJson(static_cast<const IntraICFGNode*>(node));
        case ICFGNode::FunEntryBlock:
            return contentToJson(static_cast<const FunEntryICFGNode*>(node));
        case ICFGNode::FunExitBlock:
            return contentToJson(static_cast<const FunExitICFGNode*>(node));
        case ICFGNode::FunCallBlock:
            return contentToJson(static_cast<const CallICFGNode*>(node));
        case ICFGNode::FunRetBlock:
            return contentToJson(static_cast<const RetICFGNode*>(node));
        case ICFGNode::GlobalBlock:
            return contentToJson(static_cast<const GlobalICFGNode*>(node));
    }
}

cJSON* SVFIRWriter::virtToJson(const ICFGEdge* edge)
{
    return nullptr;
}

cJSON* SVFIRWriter::virtToJson(const CHNode* node)
{
    return nullptr;
}

cJSON* SVFIRWriter::virtToJson(const CHEdge* edge)
{
    return nullptr;
}

cJSON* SVFIRWriter::contentToJson(const ICFGNode* node)
{
    cJSON* root = genericNodeToJson(node);
    JSON_WRITE_FIELD(root, node, fun);
    JSON_WRITE_FIELD(root, node, bb);
    // TODO: Ensure this?
    assert(node->VFGNodes.empty() && "VFGNodes list not empty?");
    JSON_WRITE_FIELD(root, node, pagEdges);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const GlobalICFGNode* node)
{
    return contentToJson(static_cast<const ICFGNode*>(node));
}

cJSON* SVFIRWriter::contentToJson(const IntraICFGNode* node)
{
    cJSON* root = contentToJson(static_cast<const ICFGNode*>(node));
    JSON_WRITE_FIELD(root, node, inst);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const InterICFGNode* node)
{
    return contentToJson(static_cast<const ICFGNode*>(node));
}

cJSON* SVFIRWriter::contentToJson(const FunEntryICFGNode* node)
{
    cJSON* root = contentToJson(static_cast<const ICFGNode*>(node));
    JSON_WRITE_FIELD(root, node, FPNodes);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const FunExitICFGNode* node)
{
    cJSON* root = contentToJson(static_cast<const ICFGNode*>(node));
    JSON_WRITE_FIELD(root, node, formalRet);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CallICFGNode* node)
{
    cJSON* root = contentToJson(static_cast<const ICFGNode*>(node));
    JSON_WRITE_FIELD(root, node, cs);
    JSON_WRITE_FIELD(root, node, ret);
    JSON_WRITE_FIELD(root, node, APNodes);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const RetICFGNode* node)
{
    cJSON* root = contentToJson(static_cast<const ICFGNode*>(node));
    JSON_WRITE_FIELD(root, node, cs);
    JSON_WRITE_FIELD(root, node, actualRet);
    JSON_WRITE_FIELD(root, node, callBlockNode);
    return root;
}

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

cJSON* jsonCreateIndex(size_t index)
{
    return cJSON_CreateNumber(index);
}

cJSON* jsonCreateNumber(double num)
{
    return cJSON_CreateNumber(num);
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

ICFGWriter::ICFGWriter(const ICFG* icfg) : GenericICFGWriter(icfg)
{
    for (const auto& pair : icfg->icfgNodeToSVFLoopVec)
    {
        for (const SVFLoop* loop : pair.second)
        {
            svfLoopPool.saveID(loop);
        }
    }
}

CommonCHGraphWriter::CommonCHGraphWriter(const CommonCHGraph* cchg)
{
    if (auto chg = SVFUtil::dyn_cast<CHGraph>(cchg))
    {
        chGraphWriter = new CHGraphWriter(chg);
    }
    else
    {
        assert(false && "For now only support CHGraph");
        abort();
    }
}

CommonCHGraphWriter::~CommonCHGraphWriter()
{
    if (chGraphWriter)
        delete chGraphWriter;
}

SVFIRWriter::SVFIRWriter(const SVFIR* svfir)
    : svfIR(svfir), irGraphWriter(svfir), icfgWriter(svfir->icfg),
      commonCHGraphWriter(svfir->chgraph)
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

const char* SVFIRWriter::generateJsonString()
{
    cJSON* object = this->generateJson();
    return humanReadableOption() ? cJSON_Print(object)
                                 : cJSON_PrintUnformatted(object);
}

// Main logic
cJSON* SVFIRWriter::generateJson()
{
    cJSON* root = jsonCreateObject();

    cJSON* nodeAllTypes = cJSON_CreateArray();
    jsonAddItemToObject(root, "typePool", nodeAllTypes);
    cJSON* nodeAllValues = cJSON_CreateArray();
    jsonAddItemToObject(root, "valuePool", nodeAllValues);

#define F(field) JSON_WRITE_FIELD(root, svfIR, field)
    F(svfModule);

    F(icfgNode2SVFStmtsMap);
    F(icfgNode2PTASVFStmtsMap);
    F(GepValObjMap);
    F(GepObjVarMap);
    F(memToFieldsMap);
    F(globSVFStmtSet);
    F(phiNodeMap);
    F(funArgsListMap);
    F(callSiteArgsListMap);
    F(callSiteRetMap);
    F(funRetMap);
    F(indCallSiteToFunPtrMap);
    F(funPtrToCallSitesMap);
    F(candidatePointers);
    //F(svfModule);
    F(icfg);
    //F(chgraph);
    F(callSiteSet);
#undef F

    return root;
}

cJSON* SVFIRWriter::toJson(const SVFType* type)
{
    return jsonCreateIndex(svfTypePool.getID(type));
}

cJSON* SVFIRWriter::toJson(const SVFValue* value)
{
    return jsonCreateIndex(svfValuePool.getID(value));
}

cJSON* SVFIRWriter::toJson(const IRGraph* graph)
{
    return jsonCreateString("TODO: IRGraph");
}

cJSON* SVFIRWriter::toJson(const SVFVar* var) {
    return jsonCreateIndex(irGraphWriter.getNodeID(var));
}

cJSON* SVFIRWriter::toJson(const SVFStmt* stmt)
{
    return jsonCreateIndex(irGraphWriter.getEdgeID(stmt));
}

cJSON* SVFIRWriter::toJson(const ICFG* icfg)
{
    cJSON* root = genericGraphToJson(icfg, this->icfgWriter.edgePool.getPool());

#define F(field) JSON_WRITE_FIELD(root, icfg, field)
    F(FunToFunEntryNodeMap);
    F(FunToFunExitNodeMap);
    F(CSToCallNodeMap);
    F(CSToRetNodeMap);
    F(InstToBlockNodeMap);
    F(globalBlockNode);
    F(icfgNodeToSVFLoopVec);
#undef F
    return root;
}

cJSON* SVFIRWriter::toJson(const ICFGNode* node) {
    return jsonCreateIndex(icfgWriter.getNodeID(node));
}

cJSON* SVFIRWriter::toJson(const ICFGEdge* edge) {
    return jsonCreateIndex(icfgWriter.getEdgeID(edge));
}

cJSON* SVFIRWriter::toJson(const CHGraph* graph)
{
    cJSON* root = jsonCreateObject();
    // TODO
    return root;
}

cJSON* SVFIRWriter::toJson(const CHNode* node)
{
    cJSON* root = jsonCreateObject();
    // TODO
    return root;
}

cJSON* SVFIRWriter::toJson(const CHEdge* edge)
{
    cJSON* root = jsonCreateObject();
    // TODO
    return root;
}

cJSON* SVFIRWriter::toJson(const SVFLoop* loop)
{
    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, loop, field)
    F(entryICFGEdges);
    F(backICFGEdges);
    F(inICFGEdges);
    F(outICFGEdges);
    F(icfgNodes);
    F(loopBound);
#undef F
    return root;
}

cJSON *SVFIRWriter::toJson(const SVFModule* module)
{
    cJSON* root = jsonCreateObject();
    // JSON_WRITE_STRING_FIELD(root, module, pagReadFromTxt);
    // JSON_WRITE_STRING_FIELD(root, module, moduleIdentifier);

    // JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, FunctionSet);
    // JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, GlobalSet);
    // JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, AliasSet);
    // JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, ConstantSet);
    // JSON_WRITE_SVFVALUE_CONTAINER_FILED(root, module, OtherValueSet);

    return root;
}

} // namespace SVF