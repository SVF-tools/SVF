#include "SVFIR/SVFIRRW.h"
#include "Graphs/CHG.h"
#include "SVFIR/SVFIR.h"
#include "Util/CommandLine.h"

static const Option<bool> humanReadableOption(
    "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{
/// @brief Helper function to create a new empty SVFType instance
static SVFType* createSVFType(SVFType::SVFTyKind kind)
{
    // TODO
    return nullptr;
}

/// @brief Helper function to create a new empty SVFValue instance
static SVFValue* createSVFValue(SVFValue::SVFValKind kind)
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

static ICFGNode* createICFGNode(NodeID id, ICFGNode::ICFGNodeK kind)
{
    switch (kind)
    {
    default:
        SVFUtil::outs() << "Impossible ICFGNode kind: " << kind
                        << "in createICFGNode()\n";
        assert(false);
    case ICFGNode::IntraBlock:
        return new InterICFGNode(id, {});
    case ICFGNode::FunEntryBlock:
        return new FunEntryICFGNode(id, {});
    case ICFGNode::FunExitBlock:
        return new FunExitICFGNode(id, {});
    case ICFGNode::FunCallBlock:
        return new CallICFGNode(id, {});
    case ICFGNode::FunRetBlock:
        return new RetICFGNode(id, {}, {});
    case ICFGNode::GlobalBlock:
        return new GlobalICFGNode(id);
    }
}

static ICFGEdge* createICFGEdge(ICFGEdge::ICFGEdgeK kind)
{
    switch (kind)
    {
    default:
        SVFUtil::outs() << "Impossible ICFGEdge kind: " << kind
                        << "in createICFGEdge()\n";
        assert(false);
    case ICFGEdge::IntraCF:
        return new IntraCFGEdge(nullptr, nullptr);
    case ICFGEdge::CallCF:
        return new CallCFGEdge(nullptr, nullptr, nullptr);
    case ICFGEdge::RetCF:
        return new RetCFGEdge(nullptr, nullptr, nullptr);
    }
}

static SVFVar* createSVFVar(NodeID id, SVFVar::PNODEK kind)
{
    switch (kind)
    {
    default:
        SVFUtil::outs() << "Impossible SVFVar kind: " << kind
                        << "in createSVFVar()\n";
        assert(false);
    case SVFVar::ValNode:
        return new ValVar(nullptr, id);
    case SVFVar::ObjNode:
        assert(false && "ObjNode should not be created by SVFIRReader");
    case SVFVar::RetNode:
        return new RetPN({}, id);
    case SVFVar::VarargNode:
        return new VarArgPN(nullptr, id);
    case SVFVar::GepValNode:
        return new GepValVar(nullptr, id, LocationSet(), nullptr);
    case SVFVar::GepObjNode:
        return new GepObjVar(nullptr, id, LocationSet());
    case SVFVar::FIObjNode:
        return new FIObjVar(nullptr, id, nullptr);
    case SVFVar::DummyValNode:
        return new DummyValVar(id);
    case SVFVar::DummyObjNode:
        return new DummyObjVar(id, nullptr);
    }
}

static SVFStmt* createSVFStmt(SVFStmt::PEDGEK kind)
{
    switch (kind)
    {
    default:
        SVFUtil::outs() << "Impossible SVFStmt kind: " << kind
                        << "in createSVFStmt()\n";
        assert(false);
    case SVFStmt::Addr:
        return new AddrStmt(nullptr, nullptr);
    case SVFStmt::Copy:
        return new CopyStmt(nullptr, nullptr);
    case SVFStmt::Store:
        return new StoreStmt(nullptr, nullptr, nullptr);
    case SVFStmt::Load:
        return new LoadStmt(nullptr, nullptr);
    case SVFStmt::Call:
        return new CallPE(nullptr, nullptr, nullptr, nullptr);
    case SVFStmt::Ret:
        return new RetPE(nullptr, nullptr, nullptr, nullptr);
    case SVFStmt::Gep:
        return new GepStmt(nullptr, nullptr, LocationSet());
    case SVFStmt::Phi:
        return new PhiStmt(nullptr, {}, {});
    case SVFStmt::Select:
        return new SelectStmt(nullptr, {}, nullptr);
    case SVFStmt::Cmp:
        return new CmpStmt(nullptr, {}, 0);
    case SVFStmt::BinaryOp:
        return new BinaryOPStmt(nullptr, {}, 0);
    case SVFStmt::UnaryOp:
        return new UnaryOPStmt(nullptr, nullptr, 0);
    case SVFStmt::Branch:
        return new BranchStmt(nullptr, nullptr, {});
    case SVFStmt::ThreadFork:
        return new TDForkPE(nullptr, nullptr, nullptr, nullptr);
    case SVFStmt::ThreadJoin:
        return new TDJoinPE(nullptr, nullptr, nullptr, nullptr);
    }
}

static CHNode* createCHNode(NodeID id, CHNode::GNodeK kind)
{
    ABORT_IFNOT(kind == 0, "Impossible CHNode kind");
    return new CHNode("", id);
}

static CHEdge* createCHEdge(CHEdge::GEdgeKind kind)
{
    ABORT_IFNOT(kind == 0, "Impossible CHEdge kind");
    //return new CHEdge(nullptr, nullptr, {}, );
    return nullptr; // TODO
}

cJSON* SVFIRWriter::toJson(unsigned number)
{
    // OK, double precision enough
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(int number)
{
    // OK, double precision enough
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(float number)
{
    return jsonCreateNumber(number);
}

cJSON* SVFIRWriter::toJson(unsigned long number)
{
    // unsigned long is subset of unsigned long long
    return toJson(static_cast<unsigned long long>(number));
}

cJSON* SVFIRWriter::toJson(long long number)
{
    return toJson(static_cast<unsigned long long>(number));
}

cJSON* SVFIRWriter::toJson(unsigned long long number)
{
    return jsonCreateString(numToStr(number));
}

cJSON* SVFIRWriter::virtToJson(const SVFType* type)
{
    auto kind = type->getKind();

    switch (kind)
    {
    default:
        assert(false && "Impossible SVFType kind");

#define CASE(Kind)                                                             \
    case SVFType::Kind:                                                        \
        return contentToJson(SVFUtil::dyn_cast<Kind##pe>(type))

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

cJSON* SVFIRWriter::virtToJson(const SVFValue* value)
{
    auto kind = value->getKind();

    switch (kind)
    {
    default:
        assert(false && "Impossible SVFValue kind");

#define CASE(ValueKind, type)                                                  \
    case SVFValue::ValueKind:                                                  \
        return contentToJson(static_cast<const type*>(value))

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

cJSON* SVFIRWriter::virtToJson(const SVFVar* var)
{
    switch (var->getNodeKind())
    {
    default:
        assert(false && "Unknown SVFVar kind");

#define CASE(VarKind, VarType)                                                 \
    case SVFVar::VarKind:                                                      \
        return contentToJson(static_cast<const VarType*>(var))

        CASE(ValNode, ValVar);
        CASE(ObjNode, ObjVar);
        CASE(RetNode, RetPN);
        CASE(VarargNode, VarArgPN);
        CASE(GepValNode, GepValVar);
        CASE(GepObjNode, GepObjVar);
        CASE(FIObjNode, FIObjVar);
        CASE(DummyValNode, DummyValVar);
        CASE(DummyObjNode, DummyObjVar);
#undef CASE
    }
}

cJSON* SVFIRWriter::virtToJson(const SVFStmt* stmt)
{
    switch (stmt->getEdgeKind())
    {
    default:
        assert(false && "Unknown SVFStmt kind");

#define CASE(EdgeKind, EdgeType)                                               \
    case SVFStmt::EdgeKind:                                                    \
        return contentToJson(static_cast<const EdgeType*>(stmt))

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

#define CASE(NodeKind, NodeType)                                               \
    case ICFGNode::NodeKind:                                                   \
        return contentToJson(static_cast<const NodeType*>(node))

        CASE(IntraBlock, IntraICFGNode);
        CASE(FunEntryBlock, FunEntryICFGNode);
        CASE(FunExitBlock, FunExitICFGNode);
        CASE(FunCallBlock, CallICFGNode);
        CASE(FunRetBlock, RetICFGNode);
        CASE(GlobalBlock, GlobalICFGNode);
#undef CASE
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

cJSON* SVFIRWriter::contentToJson(const CHEdge* edge)
{
    cJSON* root = genericEdgeToJson(edge);
    JSON_WRITE_FIELD(root, edge, edgeType);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFType* type)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, type, kind);
    JSON_WRITE_FIELD(root, type, getPointerToTy);
    JSON_WRITE_FIELD(root, type, typeinfo);
    JSON_WRITE_FIELD(root, type, isSingleValTy);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFPointerType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, ptrElementType);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFIntegerType* type)
{
    return contentToJson(static_cast<const SVFType*>(type));
}

cJSON* SVFIRWriter::contentToJson(const SVFFunctionType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, retTy);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFStructType* type)
{
    return contentToJson(static_cast<const SVFType*>(type));
}

cJSON* SVFIRWriter::contentToJson(const SVFArrayType* type)
{
    return contentToJson(static_cast<const SVFType*>(type));
}

cJSON* SVFIRWriter::contentToJson(const SVFOtherType* type)
{
    return contentToJson(static_cast<const SVFType*>(type));
}

cJSON* SVFIRWriter::contentToJson(const SVFValue* value)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, value, kind);
    JSON_WRITE_FIELD(root, value, ptrInUncalledFun);
    JSON_WRITE_FIELD(root, value, constDataOrAggData);
    JSON_WRITE_FIELD(root, value, type);
    JSON_WRITE_FIELD(root, value, name);
    JSON_WRITE_FIELD(root, value, sourceLoc);

    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFFunction* value)
{
    cJSON* root = contentToJson(static_cast<const SVFValue*>(value));
    JSON_WRITE_FIELD(root, value, isDecl);
    JSON_WRITE_FIELD(root, value, intrinsic);
    JSON_WRITE_FIELD(root, value, addrTaken);
    JSON_WRITE_FIELD(root, value, isUncalled);
    JSON_WRITE_FIELD(root, value, isNotRet);
    JSON_WRITE_FIELD(root, value, varArg);
    JSON_WRITE_FIELD(root, value, funcType);
    JSON_WRITE_FIELD(root, value, loopAndDom);
    JSON_WRITE_FIELD(root, value, realDefFun);
    JSON_WRITE_FIELD(root, value, allBBs);
    JSON_WRITE_FIELD(root, value, allArgs);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFBasicBlock* value)
{
    cJSON* root = contentToJson(static_cast<const SVFValue*>(value));
    JSON_WRITE_FIELD(root, value, allInsts);
    JSON_WRITE_FIELD(root, value, succBBs);
    JSON_WRITE_FIELD(root, value, predBBs);
    JSON_WRITE_FIELD(root, value, fun);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFInstruction* value)
{
    cJSON* root = contentToJson(static_cast<const SVFValue*>(value));
    JSON_WRITE_FIELD(root, value, bb);
    JSON_WRITE_FIELD(root, value, terminator);
    JSON_WRITE_FIELD(root, value, ret);
    JSON_WRITE_FIELD(root, value, succInsts);
    JSON_WRITE_FIELD(root, value, predInsts);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFCallInst* value)
{
    cJSON* root = contentToJson(static_cast<const SVFInstruction*>(value));
    JSON_WRITE_FIELD(root, value, args);
    JSON_WRITE_FIELD(root, value, varArg);
    JSON_WRITE_FIELD(root, value, calledVal);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFVirtualCallInst* value)
{
    cJSON* root = contentToJson(static_cast<const SVFCallInst*>(value));
    JSON_WRITE_FIELD(root, value, vCallVtblPtr);
    JSON_WRITE_FIELD(root, value, virtualFunIdx);
    JSON_WRITE_FIELD(root, value, funNameOfVcall);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFConstant* value)
{
    return contentToJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFIRWriter::contentToJson(const SVFGlobalValue* value)
{
    cJSON* root = contentToJson(static_cast<const SVFConstant*>(value));
    JSON_WRITE_FIELD(root, value, realDefGlobal);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFArgument* value)
{
    cJSON* root = contentToJson(static_cast<const SVFValue*>(value));
    JSON_WRITE_FIELD(root, value, fun);
    JSON_WRITE_FIELD(root, value, argNo);
    JSON_WRITE_FIELD(root, value, uncalled);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFConstantData* value)
{
    return contentToJson(static_cast<const SVFConstant*>(value));
}

cJSON* SVFIRWriter::contentToJson(const SVFConstantInt* value)
{
    cJSON* root = contentToJson(static_cast<const SVFConstantData*>(value));
    JSON_WRITE_FIELD(root, value, zval);
    JSON_WRITE_FIELD(root, value, sval);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFConstantFP* value)
{
    cJSON* root = contentToJson(static_cast<const SVFConstantData*>(value));
    JSON_WRITE_FIELD(root, value, dval);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFConstantNullPtr* value)
{
    return contentToJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFIRWriter::contentToJson(const SVFBlackHoleValue* value)
{
    return contentToJson(static_cast<const SVFConstantData*>(value));
}

cJSON* SVFIRWriter::contentToJson(const SVFOtherValue* value)
{
    return contentToJson(static_cast<const SVFValue*>(value));
}

cJSON* SVFIRWriter::contentToJson(const SVFMetadataAsValue* value)
{
    return contentToJson(static_cast<const SVFOtherValue*>(value));
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
    // objMap
    cJSON* allMemObj = jsonCreateArray();
    for (const auto& pair : symTable->objMap)
    {
        const MemObj* memObj = pair.second;
        cJSON* memObjJson = contentToJson(memObj);
        jsonAddItemToArray(allMemObj, memObjJson);
    }

    // svfTypes
    cJSON* allSvfType = jsonCreateArray();
    for (const SVFType* svfType : symbolTableInfoWriter.svfTypePool.getPool())
    {
        cJSON* svfTypeObj = contentToJson(svfType);
        jsonAddItemToArray(allSvfType, svfTypeObj);
    }

    cJSON* root = jsonCreateObject();

    jsonAddItemToObject(root, FIELD_NAME_ITEM(allMemObj));
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSvfType));

    JSON_WRITE_FIELD(root, symTable, valSymMap);
    JSON_WRITE_FIELD(root, symTable, objSymMap);
    JSON_WRITE_FIELD(root, symTable, returnSymMap);
    JSON_WRITE_FIELD(root, symTable, varargSymMap);
    // TODO: Check symTable->module == module
    JSON_WRITE_FIELD(root, symTable, modelConstants);
    JSON_WRITE_FIELD(root, symTable, totalSymNum);

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

bool jsonAddStringToObject(cJSON* obj, const char* name, const std::string& str)
{
    return jsonAddStringToObject(obj, name, str.c_str());
}

cJSON* jsonCreateNullId()
{
    // TODO: optimize
    return cJSON_CreateNull();
}

bool jsonIsNumber(const cJSON* item)
{
    return cJSON_IsNumber(item);
}

bool jsonIsNullId(const cJSON* item)
{
    // TODO: optimize
    return cJSON_IsNull(item);
}

bool jsonIsArray(const cJSON* item)
{
    return cJSON_IsArray(item);
}

bool jsonIsMap(const cJSON* item)
{
    return cJSON_IsArray(item);
}

bool jsonIsObject(const cJSON* item)
{
    return humanReadableOption() ? cJSON_IsObject(item) : cJSON_IsArray(item);
}

bool jsonKeyEquals(const cJSON* item, const char* key)
{
    return item && !(humanReadableOption() && std::strcmp(key, item->string));
}

void jsonUnpackPair(const cJSON* item, const cJSON*& key,
                       const cJSON*& value)
{
    ABORT_IFNOT(jsonIsArray(item), "Expected array as map pair");
    cJSON* child1 = item->child;
    ABORT_IFNOT(child1 != nullptr, "Missing first child of map pair");
    cJSON* child2 = child1->next;
    ABORT_IFNOT(child2 != nullptr, "Missing first child of map pair");
    ABORT_IFNOT(child2->next == nullptr, "Too many children of map pair");
    key = child1;
    value = child2;
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
    constexpr size_t maxPreciseIntInDouble = (1ull << 53);
    (void)maxPreciseIntInDouble; // silence unused warning
    assert(index <= maxPreciseIntInDouble);
    return cJSON_CreateNumber(index);
}

cJSON* jsonCreateNumber(double num)
{
    return cJSON_CreateNumber(num);
}

bool jsonAddPairToMap(cJSON* mapObj, cJSON* key, cJSON* value)
{
    cJSON* pair = cJSON_CreateArray();
    jsonAddItemToArray(pair, key);
    jsonAddItemToArray(pair, value);
    jsonAddItemToArray(mapObj, pair);
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

SymbolTableInfoWriter::SymbolTableInfoWriter(const SymbolTableInfo* symTab)
{
    assert(symTab && "SymbolTableInfo is null!");

    const auto& svfTypes = symTab->getSVFTypes();
    svfTypePool.reserve(svfTypes.size());
    for (const SVFType* type : svfTypes)
    {
        svfTypePool.saveID(type);
    }
}

SymID SymbolTableInfoWriter::getMemObjID(const MemObj* memObj)
{
    return memObj->getId();
}

size_t SymbolTableInfoWriter::getSvfTypeID(const SVFType* type)
{
    return svfTypePool.getID(type);
}

size_t SVFModuleWriter::getSvfValueID(const SVFValue* value)
{
    return svfValuePool.getID(value);
}

SVFIRWriter::SVFIRWriter(const SVFIR* svfir)
    : svfIR(svfir), irGraphWriter(svfir), icfgWriter(svfir->icfg),
      chgWriter(SVFUtil::dyn_cast<CHGraph>(svfir->chgraph)),
      symbolTableInfoWriter(svfir->symInfo)
{
}

void SVFIRWriter::writeJsonToOstream(const SVFIR* svfir, std::ostream& os)
{
    SVFIRWriter writer(svfir);
    os << writer.generateJsonString().get() << '\n';
}

void SVFIRWriter::writeJsonToPath(const SVFIR* svfir, const std::string& path)
{
    std::ofstream jsonFile(path);
    if (jsonFile.is_open())
    {
        writeJsonToOstream(svfir, jsonFile);
        jsonFile.close();
    }
    else
    {
        SVFUtil::errs() << "Failed to open file '" << path
                        << "' to write SVFIR's JSON\n";
    }
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

SVFIRWriter::autoCStr SVFIRWriter::generateJsonString()
{
    autoJSON object = generateJson();
    char* str = humanReadableOption() ? cJSON_Print(object.get())
                : cJSON_PrintUnformatted(object.get());
    return {str, cJSON_free};
}

SVFIRWriter::autoJSON SVFIRWriter::generateJson()
{
    const IRGraph* const irgraph = svfIR;
    cJSON* allStInfo = jsonCreateArray();

    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, svfIR, field)
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allStInfo));
    jsonAddJsonableToObject(root, FIELD_NAME_ITEM(irgraph));
    F(svfModule);
    F(icfg);
    F(chgraph);

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
    F(callSiteSet);
#undef F

    for (const StInfo* stInfo : stInfoPool.getPool())
    {
        cJSON* stInfoObj = contentToJson(stInfo);
        jsonAddItemToArray(allStInfo, stInfoObj);
    }

    return {root, cJSON_Delete};
}

cJSON* SVFIRWriter::toJson(const SVFType* type)
{
    return jsonCreateIndex(symbolTableInfoWriter.getSvfTypeID(type));
}

cJSON* SVFIRWriter::toJson(const SVFValue* value)
{
    return jsonCreateIndex(svfModuleWriter.getSvfValueID(value));
}

cJSON* SVFIRWriter::toJson(const IRGraph* graph)
{
    ENSURE_NOT_VISITED(graph);

    cJSON* root = genericGraphToJson(graph, irGraphWriter.edgePool.getPool());
#define F(field) JSON_WRITE_FIELD(root, graph, field)
    F(KindToSVFStmtSetMap);
    F(KindToPTASVFStmtSetMap);
    F(fromFile);
    F(nodeNumAfterPAGBuild);
    F(totalPTAPAGEdge);
    F(valueToEdgeMap);
    const SymbolTableInfo* symInfo = graph->symInfo;
    jsonAddContentToObject(root, FIELD_NAME_ITEM(symInfo));
#undef F
    return root;
}

cJSON* SVFIRWriter::toJson(const SVFVar* var)
{
    return var ? jsonCreateIndex(var->getId())
           : jsonCreateNullId();
}

cJSON* SVFIRWriter::toJson(const SVFStmt* stmt)
{
    return jsonCreateIndex(irGraphWriter.getEdgeID(stmt));
}

cJSON* SVFIRWriter::toJson(const ICFG* icfg)
{
    cJSON* allSvfLoops = jsonCreateArray(); // all indices seen in constructor
    for (const SVFLoop* svfLoop : icfgWriter.svfLoopPool.getPool())
    {
        cJSON* svfLoopObj = contentToJson(svfLoop);
        jsonAddItemToArray(allSvfLoops, svfLoopObj);
    }

    cJSON* root = genericGraphToJson(icfg, icfgWriter.edgePool.getPool());
#define F(field) JSON_WRITE_FIELD(root, icfg, field)
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSvfLoops));
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

cJSON* SVFIRWriter::toJson(const ICFGNode* node)
{
    assert(node && "ICFGNode is null!");
    return jsonCreateIndex(node->getId());
}

cJSON* SVFIRWriter::toJson(const ICFGEdge* edge)
{
    assert(edge && "ICFGNode is null!");
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
    return jsonCreateIndex(node->getId());
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
    JSON_WRITE_FIELD(root, memObj, symId);
    JSON_WRITE_FIELD(root, memObj, typeInfo); // Owns this pointer?
    JSON_WRITE_FIELD(root, memObj, refVal);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const StInfo* stInfo)
{
    // ENSURE_NOT_VISITED(stInfo);

    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, stInfo, field)
    F(fldIdxVec);
    F(elemIdxVec);
    F(fldIdx2TypeMap);
    F(finfo);
    F(stride);
    F(numOfFlattenElements);
    F(flattenElementTypes);
#undef F
    return root;
}

cJSON* SVFIRWriter::toJson(const ObjTypeInfo* objTypeInfo)
{
    ENSURE_NOT_VISITED(objTypeInfo);

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
    return jsonCreateIndex(memObj->getId());
}

cJSON* SVFIRWriter::toJson(const SVFLoopAndDomInfo* ldInfo)
{
    ENSURE_NOT_VISITED(ldInfo);

    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, ldInfo, reachableBBs);
    JSON_WRITE_FIELD(root, ldInfo, dtBBsMap);
    JSON_WRITE_FIELD(root, ldInfo, pdtBBsMap);
    JSON_WRITE_FIELD(root, ldInfo, dfBBsMap);
    JSON_WRITE_FIELD(root, ldInfo, bb2LoopMap);
    return root;
}

cJSON* SVFIRWriter::toJson(const StInfo* stInfo)
{
    return jsonCreateIndex(stInfoPool.getID(stInfo));
//     ENSURE_NOT_VISITED(type);

//     cJSON* root = jsonCreateObject();
// #define F(field) JSON_WRITE_FIELD(root, type, field)
//     F(fldIdxVec);
//     F(elemIdxVec);
//     F(fldIdx2TypeMap);
//     F(finfo);
//     F(stride);
//     F(numOfFlattenElements);
//     F(flattenElementTypes);
// #undef F
//     return root;
}

cJSON* SVFIRWriter::toJson(const LocationSet& ls)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, &ls, fldIdx);
    JSON_WRITE_FIELD(root, &ls, offsetVarAndGepTypePairs);
    return root;
}

cJSON* SVFIRWriter::toJson(const SVFModule* module)
{
    cJSON* root = jsonCreateObject();

    cJSON* allValues = jsonCreateArray();
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allValues));
    JSON_WRITE_FIELD(root, module, pagReadFromTxt);
    JSON_WRITE_FIELD(root, module, moduleIdentifier);

    JSON_WRITE_FIELD(root, module, FunctionSet);
    JSON_WRITE_FIELD(root, module, GlobalSet);
    JSON_WRITE_FIELD(root, module, AliasSet);
    JSON_WRITE_FIELD(root, module, ConstantSet);
    JSON_WRITE_FIELD(root, module, OtherValueSet);

    for (size_t i = 1; i <= svfModuleWriter.svfValuePool.size(); ++i)
    {
        cJSON* value = contentToJson(svfModuleWriter.svfValuePool.getPtr(i));
        jsonAddItemToArray(allValues, value);
    }

    return root;
}

void ICFGReader::createObjs(const cJSON* icfgJson)
{
    GenericICFGReader::createObjs(
        icfgJson,
        [](const cJSON*& icfgNodeFieldJson) {
            NodeID id;
            SVFIRReader::readJson(icfgNodeFieldJson, id);
            icfgNodeFieldJson = icfgNodeFieldJson->next;

            ICFGNode::GNodeK kind;
            ABORT_IFNOT(jsonKeyEquals(icfgNodeFieldJson, "kind"),
                        "Expecting 'kind' field");
            SVFIRReader::readJson(icfgNodeFieldJson, kind);
            icfgNodeFieldJson = icfgNodeFieldJson->next;

            return std::make_pair(
                id, createICFGNode(id, static_cast<ICFGNode::ICFGNodeK>(kind)));
        },

        [](const cJSON*& icfgEdgeFieldJson) {
            ICFGEdge::GEdgeFlag flag;
            SVFIRReader::readJson(icfgEdgeFieldJson, flag);
            icfgEdgeFieldJson = icfgEdgeFieldJson->next;

            unsigned kind = SVFIRReader::applyEdgeMask(flag);
            auto edge = createICFGEdge(static_cast<ICFGEdge::ICFGEdgeK>(kind));
            //edge->edgeFlag = flag; TODO
            return edge;
        });

    // TODO

        // GenericICFGReader::createObjs(icfgJson, nodeCreator, edgeCreator);
        // // TODO: Check svfLoopsJson
        // const cJSON* svfLoopsJson = graphFieldJson;
        // svfLoopPool.createObjs(svfLoopsJson, loopCreator);
        // graphFieldJson = svfLoopsJson->next;
}

void SymbolTableInfoReader::createObjs(const cJSON* symTableJson)
{
    assert(symTabFieldJson == nullptr && "Already created?");

    // TODO: enforce the check!
    ABORT_IFNOT(jsonIsObject(symTableJson), "symTableJson is not an object?");
    cJSON* objMapJson = symTableJson->child;
    ABORT_IFNOT(jsonIsMap(objMapJson) && jsonKeyEquals(objMapJson, "objMap"),
                "Expect ObjMap");
    cJSON* allSvfTypesJson = objMapJson->next;
    ABORT_IFNOT(jsonIsArray(allSvfTypesJson) &&
                    jsonKeyEquals(allSvfTypesJson, "AllSvfTypes"),
                "Expect array");
    symTabFieldJson = allSvfTypesJson->next;

    memObjMap.createObjs(objMapJson, [](const cJSON* f) {
        // TODO
        return std::make_pair(0, nullptr);
    });

    svfTypePool.createObjs(allSvfTypesJson, [](const cJSON*& svfTypeFieldJson) {
        ABORT_IFNOT(jsonIsNumber(svfTypeFieldJson), "Expect number");
        auto kind =
            static_cast<SVFType::SVFTyKind>(svfTypeFieldJson->valuedouble);
        svfTypeFieldJson = svfTypeFieldJson->next;
        return createSVFType(kind);
    });
}

void SVFModuleReader::createObjs(const cJSON* svfModuleJson)
{
    assert(svfModuleFieldJson == nullptr && "Already created?");

    cJSON* allValues = svfModuleJson->child;
    ABORT_IFNOT(jsonKeyMatch(allValues), "allValues expected");
    svfValuePool.createObjs(allValues, [](const cJSON*& svfValueFieldJson) {
        ABORT_IFNOT(jsonIsNumber(svfValueFieldJson) &&
                        jsonKeyEquals(svfValueFieldJson, "kind"),
                    "Expect number");
        auto kind =
            static_cast<SVFValue::SVFValKind>(svfValueFieldJson->valuedouble);
        svfValueFieldJson = svfValueFieldJson->next;
        return createSVFValue(kind);
    });

    svfModuleFieldJson = allValues->next;
}

void SVFIRReader::readJson(cJSON* root)
{
    ABORT_IFNOT(jsonIsObject(root), "Root is not an object");
    // Phase 1. Create objects
    cJSON* allStInfo = root->child;
    ABORT_IFNOT(jsonKeyMatch(allStInfo), "allStInfo expected array expected");
    stInfoPool.createObjs(allStInfo,
                          [](const cJSON*&) { return new StInfo(0); });

    cJSON* irgraph = allStInfo->next;
    irGraphReader.createObjs(
        irgraph,
        // SVFVar creator: cJSON -> (id, SVFVar*)
        [](const cJSON*& fieldJson) {
            NodeID id;
            SVFIRReader::readJson(fieldJson, id);
            fieldJson = fieldJson->next;

            unsigned kind;
            SVFIRReader::readJson(fieldJson, kind);
            fieldJson = fieldJson->next;

            return std::make_pair(
                id, createSVFVar(id, static_cast<SVFVar::PNODEK>(kind)));
        },
        // SVFStmt creator: cJSON -> SVFStmt*
        [](const cJSON*& fieldJson) {
            SVFStmt::GEdgeFlag flag;
            SVFIRReader::readJson(fieldJson, flag);
            fieldJson = fieldJson->next;

            auto kind = SVFIRReader::applyEdgeMask(flag);
            auto stmt = createSVFStmt(static_cast<SVFStmt::PEDGEK>(kind));
            //stmt->edgeFlag = flag; TODO
            return stmt;
        });

    cJSON* svfModule = irgraph->next;
    svfModuleReader.createObjs(svfModule);

    cJSON* icfg = svfModule->next;
    icfgReader.createObjs(icfg);

    cJSON* chgraph = icfg->next;
    chGraphReader.createObjs(
        chgraph,
        [](const cJSON*& fieldJson) {
            NodeID id;
            SVFIRReader::readJson(fieldJson, id);
            fieldJson = fieldJson->next;

            unsigned kind;
            SVFIRReader::readJson(fieldJson, kind);
            fieldJson = fieldJson->next;

            return std::make_pair(id, createCHNode(id, kind));
        },
        [](const cJSON*& fieldJson) {
            SVFStmt::GEdgeFlag flag;
            SVFIRReader::readJson(fieldJson, flag);
            fieldJson = fieldJson->next;

            auto kind = SVFIRReader::applyEdgeMask(flag);
            auto edge = createCHEdge(kind); // TODO
            //stmt->edgeFlag = flag; TODO
            return edge;
        });

    // Phase 2. Fill objects

    // Phase 3. Read everything else
}

void SVFIRReader::readJson(const cJSON* obj, unsigned& val)
{
    ABORT_IFNOT(jsonIsNumber(obj), "Expect number for unsigned/u32_t");
    val = obj->valuedouble;
}

void SVFIRReader::readJson(const cJSON* obj, long long& val)
{
    // TODO
}

void SVFIRReader::readJson(const cJSON* obj, unsigned long& val) {}

void SVFIRReader::readJson(const cJSON* obj, unsigned long long& val) {}

void SVFIRReader::readJson(const cJSON* obj, SVFIR*& svfIR)
{

}

void SVFIRReader::readJson(const cJSON* obj, SVFModule*& module)
{

}

void SVFIRReader::readJson(const cJSON* obj, SVFType*& type)
{
    assert(type == nullptr && "Type already read?");
}


void SVFIRReader::fill(const cJSON*& siFieldJson, StInfo* stInfo)
{
}

} // namespace SVF
