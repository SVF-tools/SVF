#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFIRRW.h"
#include "Util/CommandLine.h"
#include "Graphs/CHG.h"

static const Option<bool> humanReadableOption(
    "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{

cJSON* SVFIRWriter::toJson(unsigned number)
{
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(int number)
{
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(long unsigned number)
{
    // TODO: check number range?
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(long long number)
{
    // TODO: check number range?
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(long long unsigned number)
{
    // TODO: check number range?
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::virtToJson(const SVFVar* var)
{
    switch (var->getNodeKind())
    {
    default:
        assert(false && "Unknown SVFVar kind");
    case SVFVar::ValNode:
        return contentToJson(static_cast<const ValVar*>(var));
    case SVFVar::ObjNode:
        return contentToJson(static_cast<const ObjVar*>(var));
    case SVFVar::RetNode:
        return contentToJson(static_cast<const RetPN*>(var));
    case SVFVar::VarargNode:
        return contentToJson(static_cast<const VarArgPN*>(var));
    case SVFVar::GepValNode:
        return contentToJson(static_cast<const GepValVar*>(var));
    case SVFVar::GepObjNode:
        return contentToJson(static_cast<const GepObjVar*>(var));
    case SVFVar::FIObjNode:
        return contentToJson(static_cast<const FIObjVar*>(var));
    case SVFVar::DummyValNode:
        return contentToJson(static_cast<const DummyValVar*>(var));
    case SVFVar::DummyObjNode:
        return contentToJson(static_cast<const DummyObjVar*>(var));
    }
}

cJSON* SVFIRWriter::virtToJson(const SVFStmt* stmt)
{
    switch (stmt->getEdgeKind())
    {
    default:
        assert(false && "Unknown SVFStmt kind");
#define CASE(EdgeKind, EdgeType)                                               \
    case SVFStmt::EdgeKind: {                                                  \
        return contentToJson(static_cast<const EdgeType*>(stmt));              \
    }
        CASE(Addr, AddrStmt);
        CASE(Copy, CopyStmt);
        CASE(Store, StoreStmt);
        CASE(Load, LoadStmt);
        CASE(Call, CallPE);
        CASE(Ret, RetPE);
        CASE(Gep, GepStmt);
        CASE(Phi, PhiStmt);
        CASE(Select, SelectStmt);
        CASE(Cmp, CmpStmt);
        CASE(BinaryOp, BinaryOPStmt);
        CASE(UnaryOp, UnaryOPStmt);
        CASE(Branch, BranchStmt);
        CASE(ThreadFork, TDForkPE);
        CASE(ThreadJoin, TDJoinPE);
#undef CASE
    }
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
    switch (edge->getEdgeKind())
    {
    default:
        assert(false && "Unknown ICFGEdge kind");
    case ICFGEdge::IntraCF:
        return contentToJson(static_cast<const IntraCFGEdge*>(edge));
    case ICFGEdge::CallCF:
        return contentToJson(static_cast<const CallCFGEdge*>(edge));
    case ICFGEdge::RetCF:
        return contentToJson(static_cast<const RetCFGEdge*>(edge));
    }
}

cJSON* SVFIRWriter::virtToJson(const CHNode* node)
{
    return contentToJson(node);
}

cJSON* SVFIRWriter::virtToJson(const CHEdge* edge)
{
    return contentToJson(edge);
}

cJSON* SVFIRWriter::contentToJson(const SVFVar* var)
{
    cJSON* root = genericNodeToJson(var);
    JSON_WRITE_FIELD(root, var, value);
    JSON_WRITE_FIELD(root, var, InEdgeKindToSetMap);
    JSON_WRITE_FIELD(root, var, OutEdgeKindToSetMap);
    JSON_WRITE_FIELD(root, var, isPtr);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const ValVar* var)
{
    return contentToJson(static_cast<const SVFVar*>(var));
}

cJSON* SVFIRWriter::contentToJson(const ObjVar* var)
{
    cJSON* root = contentToJson(static_cast<const SVFVar*>(var));
    JSON_WRITE_FIELD(root, var, mem);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const GepValVar* var)
{
    cJSON* root = contentToJson(static_cast<const ValVar*>(var));
    JSON_WRITE_FIELD(root, var, ls);
    JSON_WRITE_FIELD(root, var, gepValType);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const GepObjVar* var)
{
    cJSON* root = contentToJson(static_cast<const ObjVar*>(var));
    JSON_WRITE_FIELD(root, var, ls);
    JSON_WRITE_FIELD(root, var, base);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const FIObjVar* var)
{
    return contentToJson(static_cast<const ObjVar*>(var));
}

cJSON* SVFIRWriter::contentToJson(const RetPN* var)
{
    return contentToJson(static_cast<const ValVar*>(var));
}

cJSON* SVFIRWriter::contentToJson(const VarArgPN* var)
{
    return contentToJson(static_cast<const ValVar*>(var));
}

cJSON* SVFIRWriter::contentToJson(const DummyValVar* var)
{
    return contentToJson(static_cast<const ValVar*>(var));
}

cJSON* SVFIRWriter::contentToJson(const DummyObjVar* var)
{
    return contentToJson(static_cast<const ObjVar*>(var));
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
    if (node->formalRet)
        JSON_WRITE_FIELD(root, node, formalRet);
    else
        jsonAddNumberToObject(root, "formalRet", 0);
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
    if (node->actualRet)
        JSON_WRITE_FIELD(root, node, actualRet);
    else
        jsonAddNumberToObject(root, "actualRet", 0);
    JSON_WRITE_FIELD(root, node, callBlockNode);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const ICFGEdge* edge)
{
    return genericEdgeToJson(edge);
}

cJSON* SVFIRWriter::contentToJson(const IntraCFGEdge* edge)
{
    cJSON* root = contentToJson(static_cast<const ICFGEdge*>(edge));
    JSON_WRITE_FIELD(root, edge, conditionVar);
    JSON_WRITE_FIELD(root, edge, branchCondVal);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CallCFGEdge* edge)
{
    cJSON* root = contentToJson(static_cast<const ICFGEdge*>(edge));
    JSON_WRITE_FIELD(root, edge, cs);
    JSON_WRITE_FIELD(root, edge, callPEs);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const RetCFGEdge* edge)
{
    cJSON* root = contentToJson(static_cast<const ICFGEdge*>(edge));
    JSON_WRITE_FIELD(root, edge, cs);
    JSON_WRITE_FIELD(root, edge, retPE);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CHNode* node)
{
    cJSON* root = genericNodeToJson(node);
    JSON_WRITE_FIELD(root, node, vtable);
    JSON_WRITE_FIELD(root, node, className);
    JSON_WRITE_FIELD(root, node, flags);
    JSON_WRITE_FIELD(root, node, virtualFunctionVectors);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CHEdge* edge) {
    cJSON* root = genericEdgeToJson(edge);
    JSON_WRITE_FIELD(root, edge, edgeType);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFStmt* edge)
{
    cJSON* root = genericEdgeToJson(edge);
    JSON_WRITE_FIELD(root, edge, value);
    JSON_WRITE_FIELD(root, edge, basicBlock);
    JSON_WRITE_FIELD(root, edge, icfgNode);
    JSON_WRITE_FIELD(root, edge, edgeId);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFLoop* loop)
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

cJSON* SVFIRWriter::contentToJson(const SymbolTableInfo* symTable)
{
    // Owns values of objMap & svfTypes (?)
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, symTable, valSymMap);
    JSON_WRITE_FIELD(root, symTable, objSymMap);
    JSON_WRITE_FIELD(root, symTable, returnSymMap);
    JSON_WRITE_FIELD(root, symTable, varargSymMap);
    // TODO: Dump objMap
    // TODO: Check symTable->module == module
    JSON_WRITE_FIELD(root, symTable, modelConstants);
    JSON_WRITE_FIELD(root, symTable, totalSymNum);
    JSON_WRITE_FIELD(root, symTable, svfTypes);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const AssignStmt* edge)
{
    return contentToJson(static_cast<const SVFStmt*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const AddrStmt* edge)
{
    return contentToJson(static_cast<const AssignStmt*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const CopyStmt* edge)
{
    return contentToJson(static_cast<const AssignStmt*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const StoreStmt* edge)
{
    return contentToJson(static_cast<const AssignStmt*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const LoadStmt* edge)
{
    return contentToJson(static_cast<const AssignStmt*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const GepStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const AssignStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, ls);
    JSON_WRITE_FIELD(root, edge, variantField);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CallPE* edge)
{
    cJSON* root = contentToJson(static_cast<const AssignStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, call);
    JSON_WRITE_FIELD(root, edge, entry);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const RetPE* edge)
{
    cJSON* root = contentToJson(static_cast<const AssignStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, call);
    JSON_WRITE_FIELD(root, edge, exit);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const MultiOpndStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const SVFStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, opVars);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const PhiStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const MultiOpndStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, opICFGNodes);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SelectStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const MultiOpndStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, condition);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const CmpStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const MultiOpndStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, predicate);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const BinaryOPStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const MultiOpndStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, opcode);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const UnaryOPStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const SVFStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, opcode);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const BranchStmt* edge)
{
    cJSON* root = contentToJson(static_cast<const SVFStmt*>(edge));
    JSON_WRITE_FIELD(root, edge, successors);
    JSON_WRITE_FIELD(root, edge, cond);
    JSON_WRITE_FIELD(root, edge, brInst);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const TDForkPE* edge)
{
    return contentToJson(static_cast<const CallPE*>(edge));
}

cJSON* SVFIRWriter::contentToJson(const TDJoinPE* edge)
{
    return contentToJson(static_cast<const RetPE*>(edge));
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

cJSON* jsonCreateNullId()
{
    // TODO: optimize
    return cJSON_CreateNull();
}

bool jsonIsNullId(const cJSON* item)
{
    // TODO: optimize
    return cJSON_IsNull(item);
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
    for (const auto& pair : icfg->getIcfgNodeToSVFLoopVec())
    {
        for (const SVFLoop* loop : pair.second)
        {
            svfLoopPool.saveID(loop);
        }
    }
}

SymID SymbolTableInfoWriter::getMemObjID(const MemObj* memObj)
{
    auto it = memObjToID.find(memObj);
    assert(it != memObjToID.end() && "MemObj not found!");
    return it->second;
}

SymbolTableInfoWriter::SymbolTableInfoWriter(const SymbolTableInfo* symTab)
    : symbolTableInfo(symTab)
{
    assert(symbolTableInfo && "SymbolTableInfo is null!");

    for (const auto& pair : symbolTableInfo->idToObjMap())
    {
        const SymID id = pair.first;
        const MemObj* obj = pair.second;
        memObjToID.emplace(obj, id);
    }
}

SVFIRWriter::SVFIRWriter(const SVFIR* svfir)
    : svfIR(svfir), irGraphWriter(svfir), icfgWriter(svfir->icfg),
      chgWriter(SVFUtil::dyn_cast<CHGraph>(svfir->chgraph)),
      symbolTableInfoWriter(svfir->symInfo)
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
    F(icfg);
    F(chgraph);
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
    cJSON* root = genericGraphToJson(graph, irGraphWriter.edgePool.getPool());
#define F(field) JSON_WRITE_FIELD(root, graph, field)
    F(KindToSVFStmtSetMap);
    F(KindToPTASVFStmtSetMap);
    F(fromFile);
    F(nodeNumAfterPAGBuild);
    F(totalPTAPAGEdge);
    F(valueToEdgeMap);
    jsonAddContentToObject(root, "symInfo", graph->symInfo);
#undef F
    return root;
}

cJSON* SVFIRWriter::toJson(const SVFVar* var)
{
    return var ? jsonCreateIndex(irGraphWriter.getNodeID(var))
               : jsonCreateNullId();
}

cJSON* SVFIRWriter::toJson(const SVFStmt* stmt)
{
    return jsonCreateIndex(irGraphWriter.getEdgeID(stmt));
}

cJSON* SVFIRWriter::toJson(const ICFG* icfg)
{
    cJSON* root = genericGraphToJson(icfg, icfgWriter.edgePool.getPool());

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

cJSON* SVFIRWriter::toJson(const CommonCHGraph* graph)
{
    auto chg = SVFUtil::dyn_cast<CHGraph>(graph);
    assert(chg && "Unsupported CHGraph type!");
    return toJson(chg);
}

cJSON* SVFIRWriter::toJson(const CHGraph* graph)
{
    cJSON* root = genericGraphToJson(graph, chgWriter.edgePool.getPool());
#define F(field) JSON_WRITE_FIELD(root, graph, field)
    // TODO: Ensure svfMod is the same as the SVFIR's?
    F(classNum);
    F(vfID);
    // F(buildingCHGTime); No need
    F(classNameToNodeMap);
    F(classNameToDescendantsMap);
    F(classNameToAncestorsMap);
    F(classNameToInstAndDescsMap);
    F(templateNameToInstancesMap);
    F(csToClassesMap);
    F(virtualFunctionToIDMap);
    F(csToCHAVtblsMap);
    F(csToCHAVFnsMap);
#undef F
    return root;
}

cJSON* SVFIRWriter::toJson(const CHNode* node)
{
    return jsonCreateIndex(chgWriter.getNodeID(node));
}

cJSON* SVFIRWriter::toJson(const CHEdge* edge)
{
    return jsonCreateIndex(chgWriter.getEdgeID(edge));
}

cJSON* SVFIRWriter::toJson(const CallSite& cs)
{
    return toJson(cs.getInstruction());
}

cJSON* SVFIRWriter::toJson(const SVFLoop* loop)
{
    return jsonCreateIndex(icfgWriter.getSvfLoopID(loop));
}

cJSON* SVFIRWriter::contentToJson(const MemObj* memObj)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, memObj, typeInfo); // Owns this pointer
    JSON_WRITE_FIELD(root, memObj, refVal);
    JSON_WRITE_FIELD(root, memObj, symId);
    return root;
}

cJSON* SVFIRWriter::toJson(const ObjTypeInfo* objTypeInfo)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, objTypeInfo, type);
    JSON_WRITE_FIELD(root, objTypeInfo, flags);
    JSON_WRITE_FIELD(root, objTypeInfo, maxOffsetLimit);
    JSON_WRITE_FIELD(root, objTypeInfo, elemNum);
    JSON_WRITE_FIELD(root, objTypeInfo, type);
    JSON_WRITE_FIELD(root, objTypeInfo, flags);
    JSON_WRITE_FIELD(root, objTypeInfo, maxOffsetLimit);
    JSON_WRITE_FIELD(root, objTypeInfo, elemNum);
    return root;
}

cJSON* SVFIRWriter::toJson(const MemObj* memObj)
{
    return jsonCreateIndex(symbolTableInfoWriter.getMemObjID(memObj));
}

cJSON* SVFIRWriter::toJson(const LocationSet& ls)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, &ls, fldIdx);
    JSON_WRITE_FIELD(root, &ls, offsetVarAndGepTypePairs);
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