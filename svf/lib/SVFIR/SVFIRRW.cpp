#include "SVFIR/SVFIRRW.h"
#include "Graphs/CHG.h"
#include "SVFIR/SVFIR.h"
#include "Util/CommandLine.h"
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const Option<bool> humanReadableOption(
    "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{
template <typename T, typename... Args> T* create(Args...);
template <> SVFType* create<SVFType>(SVFType::GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false,
                    "Impossible SVFTyKind " << kind << " in createType()");
    case SVFType::SVFTy:
        ABORT_IFNOT(false, "Creation of RAW SVFType isn't allowed");
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
template <> SVFValue* create<SVFValue>(SVFValue::GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false,
                    "Impossible SVFValue kind " << kind << " in createValue()");
    case SVFValue::SVFVal:
        ABORT_IFNOT(false, "Creation of RAW SVFValue isn't allowed");
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
template <> ICFGNode* create<ICFGNode>(NodeID id, ICFGNode::GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false, "Impossible ICFGNode kind: " << kind << "in createICFGNode()");
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
template <> ICFGEdge* create<ICFGEdge>(ICFGEdge::GEdgeKind kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false, "Impossible ICFGEdge kind: "
                    << kind << "in createICFGEdge()");
    case ICFGEdge::IntraCF:
        return new IntraCFGEdge(nullptr, nullptr);
    case ICFGEdge::CallCF:
        return new CallCFGEdge(nullptr, nullptr, nullptr);
    case ICFGEdge::RetCF:
        return new RetCFGEdge(nullptr, nullptr, nullptr);
    }
}
template <> SVFVar* create<SVFVar>(NodeID id, SVFVar::GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false,
                    "Impossible SVFVar kind: " << kind << "in createSVFVar()");
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
template <> SVFStmt* create<SVFStmt>(SVFStmt::GEdgeKind kind)
{
    switch (kind)
    {
    default:
        ABORT_IFNOT(false, "Impossible SVFStmt kind: " << kind << "in createSVFStmt()");
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
template <> CHNode* create<CHNode>(NodeID id, CHNode::GNodeK kind)
{
    ABORT_IFNOT(kind == 0, "Impossible CHNode kind " << kind);
    return new CHNode("", id);
}
template <> CHEdge* create<CHEdge>(CHEdge::GEdgeKind kind)
{
    ABORT_IFNOT(kind == 0, "Impossible CHEdge kind");
    return new CHEdge(nullptr, nullptr, {});
}

template <typename Edge>
static inline Edge* createEdgeWithFlag(typename Edge::GEdgeFlag flag)
{
    auto kind = SVFIRReader::applyEdgeMask(flag);
    Edge* edge = create<Edge>(kind);
    SVFIRReader::setEdgeFlag(edge, flag);
    return edge;
}

template <typename SmallNumberType>
static inline void readSmallNumber(const cJSON* obj, SmallNumberType& val)
{
    ABORT_IFNOT(jsonIsNumber(obj), "Expect number JSON for " << obj->string);
    val = obj->valuedouble;
}

template <typename BigNumberType, typename CStrToVal>
static inline void readBigNumber(const cJSON* obj, BigNumberType& val, CStrToVal conv)
{
    ABORT_IFNOT(jsonIsString(obj),
                "Expect (number) string JSON for " << obj->string);
    val = conv(obj->valuestring);
}

static void readGenericNodeIdKindFwd(const cJSON*& fieldJson, NodeID& nodeId,
                                     s64_t& kind)
{
    const cJSON* id = fieldJson;
    CHECK_JSON_KEY(id);
    SVFIRReader::readJson(fieldJson, nodeId);

    const cJSON* nodeKind = id->next;
    CHECK_JSON_KEY(nodeKind);
    SVFIRReader::readJson(nodeKind, kind);

    fieldJson = nodeKind->next;
}

cJSON* SVFIRWriter::toJson(bool flag)
{
    return jsonCreateBool(flag);
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
#define F(f) JSON_WRITE_FIELD(root, value, f);
    F(isDecl);
    F(intrinsic);
    F(addrTaken);
    F(isUncalled);
    F(isNotRet);
    F(varArg);
    F(funcType);
    F(loopAndDom);
    F(realDefFun);
    F(allBBs);
    F(allArgs);
#undef F
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
    for (const SVFType* svfType : symbolTableInfoWriter.svfTypePool)
    {
        cJSON* svfTypeObj = contentToJson(svfType);
        jsonAddItemToArray(allSvfType, svfTypeObj);
    }

    // stInfos
    cJSON* allStInfo = jsonCreateArray();
    for (const StInfo* stInfo : symbolTableInfoWriter.stInfoPool)
    {
        cJSON* stInfoObj = contentToJson(stInfo);
        jsonAddItemToArray(allStInfo, stInfoObj);
    }

    cJSON* root = jsonCreateObject();

    jsonAddItemToObject(root, FIELD_NAME_ITEM(allMemObj));
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSvfType));
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allStInfo));

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

bool jsonIsBool(const cJSON* item)
{
    return humanReadableOption()
               ? cJSON_IsBool(item)
               : cJSON_IsNumber(item) &&
                     (item->valuedouble == 0 || item->valuedouble == 1);
}

bool jsonIsBool(const cJSON* item, bool& flag)
{
    if (humanReadableOption())
    {
        if (!cJSON_IsBool(item))
            return false;
        flag = cJSON_IsTrue(item);
        return true;
    }
    else
    {
        if (!cJSON_IsNumber(item))
            return false;
        flag = item->valuedouble == 1;
        return true;
    }
}

bool jsonIsNumber(const cJSON* item)
{
    return cJSON_IsNumber(item);
}

bool jsonIsString(const cJSON* item)
{
    return cJSON_IsString(item);
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
    return item && !(humanReadableOption() && std::strcmp(item->string, key));
}

void jsonUnpackPair(const cJSON* item, const cJSON*& key,
                       const cJSON*& value)
{
    ABORT_IFNOT(jsonIsArray(item), "Expected array as pair");
    cJSON* child1 = item->child;
    ABORT_IFNOT(child1 != nullptr, "Missing first child of pair");
    cJSON* child2 = child1->next;
    ABORT_IFNOT(child2 != nullptr, "Missing first child of pair");
    ABORT_IFNOT(child2->next == nullptr, "Too many children of pair");
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

cJSON* jsonCreateBool(bool flag)
{
    bool hr = humanReadableOption();
    return hr ? cJSON_CreateBool(flag) : cJSON_CreateNumber(flag);
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

    for (const StInfo* stInfo : symTab->getStInfos())
    {
        stInfoPool.saveID(stInfo);
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

size_t SymbolTableInfoWriter::getStInfoID(const StInfo* type)
{
    return stInfoPool.getID(type);
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

    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, svfIR, field)
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
    return var ? jsonCreateIndex(var->getId()) : jsonCreateNullId();
}

cJSON* SVFIRWriter::toJson(const SVFStmt* stmt)
{
    return jsonCreateIndex(irGraphWriter.getEdgeID(stmt));
}

cJSON* SVFIRWriter::toJson(const ICFG* icfg)
{
    cJSON* allSvfLoops = jsonCreateArray(); // all indices seen in constructor
    for (const SVFLoop* svfLoop : icfgWriter.svfLoopPool)
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
    return jsonCreateIndex(this->symbolTableInfoWriter.getStInfoID(stInfo));
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
            ICFGNode::GNodeK kind;
            readGenericNodeIdKindFwd(icfgNodeFieldJson, id, kind);
            return std::make_pair(id, create<ICFGNode>(id, kind));
        },
        [](const cJSON*& icfgEdgeFieldJson) {
            ICFGEdge::GEdgeFlag flag;
            JSON_READ_OBJ_FWD(icfgEdgeFieldJson, flag);
            return createEdgeWithFlag<ICFGEdge>(flag);
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
        SVFType::GNodeK kind;
        JSON_READ_OBJ_FWD(svfTypeFieldJson, kind);
        return create<SVFType>(kind);
    });
}

void SVFModuleReader::createObjs(const cJSON* svfModuleJson)
{
    assert(svfModuleFieldJson == nullptr && "Already created?");

    cJSON* allValues = svfModuleJson->child;
    CHECK_JSON_KEY(allValues);
    svfValuePool.createObjs(allValues, [](const cJSON*& svfValueFieldJson) {
        SVFValue::GNodeK kind;
        JSON_READ_OBJ_FWD(svfValueFieldJson, kind);
        return create<SVFValue>(kind);
    });

    svfModuleFieldJson = allValues->next;
}

void SVFIRReader::read(cJSON* root)
{
    ABORT_IFNOT(jsonIsObject(root), "Root is not an object");
    // Phase 1. Create objects
    cJSON* irgraph = root->child;
    irGraphReader.createObjs(
        irgraph,
        // SVFVar creator: cJSON -> (id, SVFVar*)
        [](const cJSON*& fieldJson) {
            NodeID id;
            SVFVar::GNodeK kind;
            readGenericNodeIdKindFwd(fieldJson, id, kind);
            return std::make_pair(id, create<SVFVar>(id, kind));
        },
        // SVFStmt creator: cJSON -> SVFStmt*
        [](const cJSON*& fieldJson) {
            SVFStmt::GEdgeFlag edgeFlag;
            JSON_READ_OBJ_FWD(fieldJson, edgeFlag);
            return createEdgeWithFlag<SVFStmt>(edgeFlag);
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
            CHNode::GNodeK kind;
            readGenericNodeIdKindFwd(fieldJson, id, kind);
            return std::make_pair(id, create<CHNode>(id, kind));
        },
        [](const cJSON*& fieldJson) {
            SVFStmt::GEdgeFlag edgeFlag;
            JSON_READ_OBJ_FWD(fieldJson, edgeFlag);
            return createEdgeWithFlag<CHEdge>(edgeFlag);
        });
    cJSON* svfirField = chgraph->next;

    // Phase 2. Fill objects
    irGraphReader.fillObjs(
        [this](const cJSON*& j, SVFVar* var) { virtFill(j, var); },
        [this](const cJSON*& j, SVFStmt* stmt) { virtFill(j, stmt); });
    svfModuleReader.fillObjs(
        [this](const cJSON*& j, SVFValue* val) { virtFill(j, val); });

    // TODO: later

    // Phase 3. Read everything else
    SVFIR* svfIR = nullptr;
    (void)(svfirField);
    (void)(svfIR);
    //JSON_READ_FIELD_FWD(svfirField, svfIR, icfgNode2SVFStmtsMap);
}

void SVFIRReader::readJson(const cJSON* obj, bool& flag)
{
    ABORT_IFNOT(jsonIsBool(obj, flag), "Expect bool for " << obj->string);
}

void SVFIRReader::readJson(const cJSON* obj, unsigned& val)
{
    readSmallNumber(obj, val);
}

void SVFIRReader::readJson(const cJSON* obj, int& val)
{
    readSmallNumber(obj, val);
}

void SVFIRReader::readJson(const cJSON* obj, float& val)
{
    readSmallNumber(obj, val);
}

void SVFIRReader::readJson(const cJSON* obj, unsigned long& val)
{
    readBigNumber(obj, val,
                  [](const char* s) { return std::strtoul(s, nullptr, 10); });
}

void SVFIRReader::readJson(const cJSON* obj, unsigned long long& val)
{
    readBigNumber(obj, val,
                  [](const char* s) { return std::strtoull(s, nullptr, 10); });
}

void SVFIRReader::readJson(const cJSON* obj, std::string& str)
{
    ABORT_IFNOT(jsonIsString(obj), "Expect string for " << obj->string);
    str = obj->valuestring;
}

void SVFIRReader::readJson(const cJSON* obj, long long& val)
{
    readBigNumber(obj, val,
                  [](const char* s) { return std::strtoll(s, nullptr, 10); });
}

void SVFIRReader::readJson(const cJSON* obj, SVFType*& type)
{
    //TODO
    assert(type == nullptr && "Type already read?");
}

void SVFIRReader::readJson(const cJSON* obj, SVFValue*& value)
{
    //TODO
}

void SVFIRReader::readJson(const cJSON* obj, SVFVar*& var)
{
    //TODO
}

void SVFIRReader::readJson(const cJSON* obj, SVFStmt*& stmt)
{
    unsigned id;
    JSON_READ_OBJ(obj, id);
    stmt = irGraphReader.getEdgePtr(id);
}

void SVFIRReader::readJson(const cJSON* obj, ICFGNode*& node)
{
    NodeID id;
    JSON_READ_OBJ(obj, id);
    node = icfgReader.getNodePtr(id);
}

void SVFIRReader::readJson(const cJSON* obj, ICFGEdge*& edge)
{
    //TODO
}

void SVFIRReader::readJson(const cJSON* obj, CHNode*& node)
{
    //TODO
}

void SVFIRReader::readJson(const cJSON* obj, CHEdge*& edge)
{
    //TODO
}

void SVFIRReader::readJson(const cJSON* obj, SVFIR*& svfIR)
{

}

void SVFIRReader::readJson(const cJSON* obj, SVFModule*& module)
{

}

void SVFIRReader::readJson(const cJSON* obj, LocationSet& ls)
{
    // TODO
}

void SVFIRReader::readJson(const cJSON* obj, MemObj*& memObj)
{
    // TODO
}

void SVFIRReader::readJson(const cJSON* obj, SVFLoopAndDomInfo*& ldInfo)
{
    // TODO
}

void SVFIRReader::fill(const cJSON*& siFieldJson, StInfo* stInfo)
{
    // TODO
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, SVFVar* var)
{
    switch (var->getNodeKind())
    {
    default:
        assert(false && "Unknown SVFVar kind");

#define CASE(VarKind, VarType)                                                 \
    case SVFVar::VarKind:                                                      \
        return fill(fieldJson, static_cast<VarType*>(var))

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

void SVFIRReader::fill(const cJSON*& fieldJson, SVFVar* var)
{
    fill(fieldJson, static_cast<GenericPAGNodeTy*>(var));
    JSON_READ_FIELD_FWD(fieldJson, var, value);
    JSON_READ_FIELD_FWD(fieldJson, var, InEdgeKindToSetMap);
    JSON_READ_FIELD_FWD(fieldJson, var, OutEdgeKindToSetMap);
    JSON_READ_FIELD_FWD(fieldJson, var, isPtr);
}

void SVFIRReader::fill(const cJSON*& fieldJson, ValVar* var)
{
    fill(fieldJson, static_cast<SVFVar*>(var));
}

void SVFIRReader::fill(const cJSON*& fieldJson, ObjVar* var)
{
    fill(fieldJson, static_cast<SVFVar*>(var));
    JSON_READ_FIELD_FWD(fieldJson, var, mem);
}

void SVFIRReader::fill(const cJSON*& fieldJson, GepValVar* var)
{
    fill(fieldJson, static_cast<ValVar*>(var));
    JSON_READ_FIELD_FWD(fieldJson, var, ls);
    JSON_READ_FIELD_FWD(fieldJson, var, gepValType);
}

void SVFIRReader::fill(const cJSON*& fieldJson, GepObjVar* var)
{
    fill(fieldJson, static_cast<ObjVar*>(var));
    JSON_READ_FIELD_FWD(fieldJson, var, ls);
    JSON_READ_FIELD_FWD(fieldJson, var, base);
}

void SVFIRReader::fill(const cJSON*& fieldJson, FIObjVar* var)
{
    fill(fieldJson, static_cast<ObjVar*>(var));
}

void SVFIRReader::fill(const cJSON*& fieldJson, RetPN* var)
{
    fill(fieldJson, static_cast<ValVar*>(var));
}

void SVFIRReader::fill(const cJSON*& fieldJson, VarArgPN* var)
{
    fill(fieldJson, static_cast<ValVar*>(var));
}

void SVFIRReader::fill(const cJSON*& fieldJson, DummyValVar* var)
{
    fill(fieldJson, static_cast<ValVar*>(var));
}

void SVFIRReader::fill(const cJSON*& fieldJson, DummyObjVar* var)
{
    fill(fieldJson, static_cast<ObjVar*>(var));
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, SVFStmt* stmt)
{
    auto kind = stmt->getEdgeKind();

    switch (kind)
    {
    default:
        ABORT_IFNOT(false, "Unknown SVFStmt kind " << kind);

#define CASE(EdgeKind, EdgeType)                                               \
    case SVFStmt::EdgeKind:                                                    \
        return fill(fieldJson, static_cast<EdgeType*>(stmt))

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

void SVFIRReader::fill(const cJSON*& fieldJson, SVFStmt* stmt)
{
    fill(fieldJson, static_cast<GenericPAGEdgeTy*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, value);
    JSON_READ_FIELD_FWD(fieldJson, stmt, basicBlock);
    JSON_READ_FIELD_FWD(fieldJson, stmt, basicBlock);
    JSON_READ_FIELD_FWD(fieldJson, stmt, icfgNode);
    JSON_READ_FIELD_FWD(fieldJson, stmt, edgeId);
}

void SVFIRReader::fill(const cJSON*& fieldJson, AssignStmt* stmt)
{
    fill(fieldJson, static_cast<SVFStmt*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, AddrStmt* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, CopyStmt* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, StoreStmt* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, LoadStmt* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, GepStmt* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, ls);
    JSON_READ_FIELD_FWD(fieldJson, stmt, variantField);
}

void SVFIRReader::fill(const cJSON*& fieldJson, CallPE* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, call);
    JSON_READ_FIELD_FWD(fieldJson, stmt, entry);
}

void SVFIRReader::fill(const cJSON*& fieldJson, RetPE* stmt)
{
    fill(fieldJson, static_cast<AssignStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, call);
    JSON_READ_FIELD_FWD(fieldJson, stmt, exit);
}

void SVFIRReader::fill(const cJSON*& fieldJson, MultiOpndStmt* stmt)
{
    fill(fieldJson, static_cast<SVFStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, opVars);
}

void SVFIRReader::fill(const cJSON*& fieldJson, PhiStmt* stmt)
{
    fill(fieldJson, static_cast<MultiOpndStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, opICFGNodes);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SelectStmt* stmt)
{
    fill(fieldJson, static_cast<MultiOpndStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, condition);
}

void SVFIRReader::fill(const cJSON*& fieldJson, CmpStmt* stmt)
{
    fill(fieldJson, static_cast<MultiOpndStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, predicate);
}

void SVFIRReader::fill(const cJSON*& fieldJson, BinaryOPStmt* stmt)
{
    fill(fieldJson, static_cast<MultiOpndStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, opcode);
}

void SVFIRReader::fill(const cJSON*& fieldJson, UnaryOPStmt* stmt)
{
    fill(fieldJson, static_cast<SVFStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, opcode);
}

void SVFIRReader::fill(const cJSON*& fieldJson, BranchStmt* stmt)
{
    fill(fieldJson, static_cast<SVFStmt*>(stmt));
    JSON_READ_FIELD_FWD(fieldJson, stmt, successors);
    JSON_READ_FIELD_FWD(fieldJson, stmt, cond);
    JSON_READ_FIELD_FWD(fieldJson, stmt, brInst);
}

void SVFIRReader::fill(const cJSON*& fieldJson, TDForkPE* stmt)
{
    fill(fieldJson, static_cast<CallPE*>(stmt));
}

void SVFIRReader::fill(const cJSON*& fieldJson, TDJoinPE* stmt)
{
    fill(fieldJson, static_cast<RetPE*>(stmt));
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, SVFValue* value)
{
    auto kind = value->getKind();

    switch (kind)
    {
    default:
        ABORT_IFNOT(false, "Impossible SVFValue kind " << kind);

#define CASE(ValueKind, Type)                                                  \
    case SVFValue::ValueKind:                                                  \
        return fill(fieldJson, static_cast<Type*>(value))

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

void SVFIRReader::fill(const cJSON*& fieldJson, SVFValue* value)
{
    // kind has already been read.
    JSON_READ_FIELD_FWD(fieldJson, value, ptrInUncalledFun);
    JSON_READ_FIELD_FWD(fieldJson, value, constDataOrAggData);
    JSON_READ_FIELD_FWD(fieldJson, value, type);
    JSON_READ_FIELD_FWD(fieldJson, value, name);
    JSON_READ_FIELD_FWD(fieldJson, value, sourceLoc);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFFunction* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
#define F(f) JSON_READ_FIELD_FWD(fieldJson, value, f)
    F(isDecl);
    F(intrinsic);
    F(addrTaken);
    F(isUncalled);
    F(isNotRet);
    F(varArg);
    F(funcType);
    F(loopAndDom);
    F(realDefFun);
    F(allBBs);
    F(allArgs);
#undef F
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFBasicBlock* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, allInsts);
    JSON_READ_FIELD_FWD(fieldJson, value, succBBs);
    JSON_READ_FIELD_FWD(fieldJson, value, predBBs);
    JSON_READ_FIELD_FWD(fieldJson, value, fun);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFInstruction* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, bb);
    JSON_READ_FIELD_FWD(fieldJson, value, terminator);
    JSON_READ_FIELD_FWD(fieldJson, value, ret);
    JSON_READ_FIELD_FWD(fieldJson, value, succInsts);
    JSON_READ_FIELD_FWD(fieldJson, value, predInsts);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFCallInst* value)
{
    fill(fieldJson, static_cast<SVFInstruction*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, args);
    JSON_READ_FIELD_FWD(fieldJson, value, varArg);
    JSON_READ_FIELD_FWD(fieldJson, value, calledVal);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFVirtualCallInst* value)
{
    fill(fieldJson, static_cast<SVFCallInst*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, vCallVtblPtr);
    JSON_READ_FIELD_FWD(fieldJson, value, virtualFunIdx);
    JSON_READ_FIELD_FWD(fieldJson, value, funNameOfVcall);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFConstant* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFGlobalValue* value)
{
    fill(fieldJson, static_cast<SVFConstant*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, realDefGlobal);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFArgument* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, fun);
    JSON_READ_FIELD_FWD(fieldJson, value, argNo);
    JSON_READ_FIELD_FWD(fieldJson, value, uncalled);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFConstantData* value)
{
    fill(fieldJson, static_cast<SVFConstant*>(value));
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFConstantInt* value)
{
    fill(fieldJson, static_cast<SVFConstantData*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, zval);
    JSON_READ_FIELD_FWD(fieldJson, value, sval);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFConstantFP* value)
{
    fill(fieldJson, static_cast<SVFConstantData*>(value));
    JSON_READ_FIELD_FWD(fieldJson, value, dval);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFConstantNullPtr* value)
{
    fill(fieldJson, static_cast<SVFConstantData*>(value));
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFBlackHoleValue* value)
{
    fill(fieldJson, static_cast<SVFConstantData*>(value));
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFOtherValue* value)
{
    fill(fieldJson, static_cast<SVFValue*>(value));
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFMetadataAsValue* value)
{
    fill(fieldJson, static_cast<SVFOtherValue*>(value));
}

} // namespace SVF
