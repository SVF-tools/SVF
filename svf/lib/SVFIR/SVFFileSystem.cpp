#include "SVFIR/SVFFileSystem.h"
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

SVFType* createSVFType(SVFType::GNodeK kind, bool isSingleValTy)
{
    switch (kind)
    {
    default:
        ABORT_MSG(kind << " is an impossible SVFTyKind in create()");
    case SVFType::SVFTy:
        ABORT_MSG("Creation of RAW SVFType isn't allowed");
    case SVFType::SVFPointerTy:
        ABORT_IFNOT(isSingleValTy, "Pointer type must be single-valued");
        return new SVFPointerType(nullptr);
    case SVFType::SVFIntegerTy:
        ABORT_IFNOT(isSingleValTy, "Integer type must be single-valued");
        return new SVFIntegerType();
    case SVFType::SVFFunctionTy:
        ABORT_IFNOT(!isSingleValTy, "Function type must be multi-valued");
        return new SVFFunctionType(nullptr);
    case SVFType::SVFStructTy:
        ABORT_IFNOT(!isSingleValTy, "Struct type must be multi-valued");
        return new SVFStructType();
    case SVFType::SVFArrayTy:
        ABORT_IFNOT(!isSingleValTy, "Array type must be multi-valued");
        return new SVFArrayType();
    case SVFType::SVFOtherTy:
        return new SVFOtherType(isSingleValTy);
    }
}

static SVFValue* createSVFValue(SVFValue::GNodeK kind, const SVFType* type,
                                std::string&& name)
{
    auto creator = [=]() -> SVFValue*
    {
        switch (kind)
        {
        default:
            ABORT_MSG(kind << " is an impossible SVFValueKind in create()");
        case SVFValue::SVFVal:
            ABORT_MSG("Creation of RAW SVFValue isn't allowed");
        case SVFValue::SVFFunc:
            return new SVFFunction(type, {}, {}, {}, {}, {}, {});
        case SVFValue::SVFBB:
            return new SVFBasicBlock(type, {});
        case SVFValue::SVFInst:
            return new SVFInstruction(type, {}, {}, {});
        case SVFValue::SVFCall:
            return new SVFCallInst(type, {}, {}, {});
        case SVFValue::SVFVCall:
            return new SVFVirtualCallInst(type, {}, {}, {});
        case SVFValue::SVFGlob:
            return new SVFGlobalValue(type);
        case SVFValue::SVFArg:
            return new SVFArgument(type, {}, {}, {});
        case SVFValue::SVFConst:
            return new SVFConstant(type);
        case SVFValue::SVFConstData:
            return new SVFConstantData(type);
        case SVFValue::SVFConstInt:
            return new SVFConstantInt(type, {}, {});
        case SVFValue::SVFConstFP:
            return new SVFConstantFP(type, {});
        case SVFValue::SVFNullPtr:
            return new SVFConstantNullPtr(type);
        case SVFValue::SVFBlackHole:
            return new SVFBlackHoleValue(type);
        case SVFValue::SVFMetaAsValue:
            return new SVFMetadataAsValue(type);
        case SVFValue::SVFOther:
            return new SVFOtherValue(type);
        }
    };
    auto val = creator();
    val->setName(std::move(name));
    return val;
}

template <typename SmallNumberType>
static inline void readSmallNumber(const cJSON* obj, SmallNumberType& val)
{
    val = static_cast<SmallNumberType>(jsonGetNumber(obj));
}

template <typename BigNumberType, typename CStrToVal>
static inline void readBigNumber(const cJSON* obj, BigNumberType& val, CStrToVal conv)
{
    ABORT_IFNOT(jsonIsString(obj),
                "Expect (number) string JSON for " << JSON_KEY(obj));
    val = conv(obj->valuestring);
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

cJSON* SVFIRWriter::toJson(const std::string& str)
{
    return jsonCreateString(str.c_str());
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
        return contentToJson(static_cast<const Kind##pe*>(type))

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
    JSON_WRITE_FIELD(root, var, ap);
    JSON_WRITE_FIELD(root, var, gepValType);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const GepObjVar* var)
{
    cJSON* root = contentToJson(static_cast<const ObjVar*>(var));
    JSON_WRITE_FIELD(root, var, apOffset);
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
    JSON_WRITE_FIELD(root, type, isSingleValTy);
    JSON_WRITE_FIELD(root, type, getPointerToTy);
    JSON_WRITE_FIELD(root, type, typeinfo);
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
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, signAndWidth);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFFunctionType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, retTy);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFStructType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, name);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFArrayType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, numOfElement);
    JSON_WRITE_FIELD(root, type, typeOfElement);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFOtherType* type)
{
    cJSON* root = contentToJson(static_cast<const SVFType*>(type));
    JSON_WRITE_FIELD(root, type, repr);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const SVFValue* value)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, value, kind);
    JSON_WRITE_FIELD(root, value, type);
    JSON_WRITE_FIELD(root, value, name);
    JSON_WRITE_FIELD(root, value, ptrInUncalledFun);
    JSON_WRITE_FIELD(root, value, constDataOrAggData);
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
    JSON_WRITE_FIELD(root, edge, ap);
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

bool jsonAddStringToObject(cJSON* obj, const char* name, const std::string& s)
{
    return jsonAddStringToObject(obj, name, s.c_str());
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

std::pair<const cJSON*,const cJSON*> jsonUnpackPair(const cJSON* item)
{
    ABORT_IFNOT(jsonIsArray(item), "Expected array as pair");
    cJSON* child1 = item->child;
    ABORT_IFNOT(child1, "Missing first child of pair");
    cJSON* child2 = child1->next;
    ABORT_IFNOT(child2, "Missing first child of pair");
    ABORT_IFNOT(!child2->next, "Pair has more than two children");
    return {child1, child2};
}

double jsonGetNumber(const cJSON* item)
{
    ABORT_IFNOT(jsonIsNumber(item), "Expected number for " << JSON_KEY(item));
    return item->valuedouble;
}

cJSON* jsonCreateNullId()
{
    // TODO: optimize
    return cJSON_CreateNull();
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

SVFModuleWriter::SVFModuleWriter(const SVFModule* svfModule)
{
    // TODO: SVFType & StInfo are managed by SymbolTableInfo. Refactor it?
    auto symTab = SymbolTableInfo::SymbolInfo();

    const auto& svfTypes = symTab->getSVFTypes();
    svfTypePool.reserve(svfTypes.size());
    for (const SVFType* type : svfTypes)
    {
        svfTypePool.saveID(type);
    }

    const auto& stInfos = symTab->getStInfos();
    stInfoPool.reserve(stInfos.size());
    for (const StInfo* stInfo : stInfos)
    {
        stInfoPool.saveID(stInfo);
    }

    svfValuePool.reserve(svfModule->getFunctionSet().size() +
                         svfModule->getConstantSet().size() +
                         svfModule->getOtherValueSet().size());
}

SVFIRWriter::SVFIRWriter(const SVFIR* svfir)
    : svfIR(svfir), svfModuleWriter(svfir->svfModule), icfgWriter(svfir->icfg),
      chgWriter(SVFUtil::dyn_cast<CHGraph>(svfir->chgraph)),
      irGraphWriter(svfir)
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
    const IRGraph* const irGraph = svfIR;
    NodeIDAllocator* nodeIDAllocator = NodeIDAllocator::allocator;
    assert(nodeIDAllocator && "NodeIDAllocator is not initialized?");

    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, svfIR, field)
    F(svfModule);
    F(symInfo);
    F(icfg);
    F(chgraph);
    jsonAddJsonableToObject(root, FIELD_NAME_ITEM(irGraph));
    F(icfgNode2SVFStmtsMap);
    F(icfgNode2PTASVFStmtsMap);
    F(GepValObjMap);
    F(typeLocSetsMap);
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
    jsonAddJsonableToObject(root, FIELD_NAME_ITEM(nodeIDAllocator));
#undef F

    return {root, cJSON_Delete};
}

cJSON* SVFIRWriter::toJson(const SVFType* type)
{
    return jsonCreateIndex(svfModuleWriter.getSVFTypeID(type));
}

cJSON* SVFIRWriter::toJson(const SVFValue* value)
{
    return jsonCreateIndex(svfModuleWriter.getSVFValueID(value));
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
    cJSON* allSvfLoop = jsonCreateArray(); // all indices seen in constructor
    for (const SVFLoop* svfLoop : icfgWriter.svfLoopPool)
    {
        cJSON* svfLoopObj = contentToJson(svfLoop);
        jsonAddItemToArray(allSvfLoop, svfLoopObj);
    }

#define F(field) JSON_WRITE_FIELD(root, icfg, field)
    cJSON* root = genericGraphToJson(icfg, icfgWriter.edgePool.getPool());
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSvfLoop)); // Meta field
    F(totalICFGNode);
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
    JSON_WRITE_FIELD(root, memObj, typeInfo);
    JSON_WRITE_FIELD(root, memObj, refVal);
    return root;
}

cJSON* SVFIRWriter::contentToJson(const StInfo* stInfo)
{
    cJSON* root = jsonCreateObject();
#define F(field) JSON_WRITE_FIELD(root, stInfo, field)
    F(stride);
    F(numOfFlattenElements);
    F(numOfFlattenFields);
    F(fldIdxVec);
    F(elemIdxVec);
    F(fldIdx2TypeMap);
    F(finfo);
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
    return jsonCreateIndex(svfModuleWriter.getStInfoID(stInfo));
}

cJSON* SVFIRWriter::toJson(const AccessPath& ap)
{
    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, &ap, fldIdx);
    JSON_WRITE_FIELD(root, &ap, offsetVarAndGepTypePairs);
    return root;
}

cJSON* SVFIRWriter::toJson(const NodeIDAllocator* nodeIDAllocator)
{
    ENSURE_NOT_VISITED(nodeIDAllocator);

    cJSON* root = jsonCreateObject();
    JSON_WRITE_FIELD(root, nodeIDAllocator, strategy);
    JSON_WRITE_FIELD(root, nodeIDAllocator, numObjects);
    JSON_WRITE_FIELD(root, nodeIDAllocator, numValues);
    JSON_WRITE_FIELD(root, nodeIDAllocator, numSymbols);
    JSON_WRITE_FIELD(root, nodeIDAllocator, numNodes);
    return root;
}

cJSON* SVFIRWriter::toJson(const SymbolTableInfo* symTable)
{
    ENSURE_NOT_VISITED(symTable);

    cJSON* root = jsonCreateObject();
    cJSON* allMemObj = jsonCreateArray();
    for (const auto& pair : symTable->objMap)
    {
        const MemObj* memObj = pair.second;
        cJSON* memObjJson = contentToJson(memObj);
        jsonAddItemToArray(allMemObj, memObjJson);
    }

#define F(field) JSON_WRITE_FIELD(root, symTable, field)
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allMemObj)); // Actual field

    F(valSymMap);
    F(objSymMap);
    F(returnSymMap);
    F(varargSymMap);
    // Field objMap can be represented by allMemObj
    // Field mod can be represented by svfIR->svfModule. Delete it?
    assert(symTable->mod == svfIR->svfModule && "SVFModule mismatch!");
    F(modelConstants);
    F(totalSymNum);
    F(maxStruct);
    F(maxStSize);
    // Field svfTypes can be represented by svfModuleWriter.svfTypePool
    // Field stInfos can be represented by svfModuleWriter.stInfoPool
#undef F

    return root;
}

cJSON* SVFIRWriter::toJson(const SVFModule* module)
{
    cJSON* root = jsonCreateObject();
    cJSON* allSVFType = jsonCreateArray();
    cJSON* allStInfo = jsonCreateArray();
    cJSON* allSVFValue = jsonCreateArray();

    for (const SVFType* svfType : svfModuleWriter.svfTypePool)
    {
        cJSON* svfTypeObj = virtToJson(svfType);
        jsonAddItemToArray(allSVFType, svfTypeObj);
    }

    for (const StInfo* stInfo : svfModuleWriter.stInfoPool)
    {
        cJSON* stInfoObj = contentToJson(stInfo);
        jsonAddItemToArray(allStInfo, stInfoObj);
    }

#define F(field) JSON_WRITE_FIELD(root, module, field)
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSVFType));  // Meta field
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allStInfo));   // Meta field
    jsonAddItemToObject(root, FIELD_NAME_ITEM(allSVFValue)); // Meta field
    F(pagReadFromTxt);
    F(moduleIdentifier);

    F(FunctionSet);
    F(GlobalSet);
    F(AliasSet);
    F(ConstantSet);
    F(OtherValueSet);
#undef F

    for (size_t i = 1; i <= svfModuleWriter.sizeSVFValuePool(); ++i)
    {
        cJSON* value = virtToJson(svfModuleWriter.getSVFValuePtr(i));
        jsonAddItemToArray(allSVFValue, value);
    }

    return root;
}

SVFIR* SVFIRReader::read(const cJSON* root)
{
    const cJSON* svfirField = createObjs(root);

    SVFIR* svfIR = SVFIR::getPAG(); // SVFIR constructor sets symInfo
    IRGraph* irGraph = svfIR;

    auto svfModule = SVFModule::getSVFModule();
    auto icfg = new ICFG();
    auto chgraph = new CHGraph(svfModule);
    auto symInfo = SymbolTableInfo::SymbolInfo();
    symInfo->mod = svfModule;

    svfIR->svfModule = svfModule;
    svfIR->icfg = icfg;
    svfIR->chgraph = chgraph;

#define F(field) JSON_READ_FIELD_FWD(svfirField, svfIR, field)
    readJson(symInfo);
    readJson(irGraph);
    readJson(icfg);
    readJson(chgraph);
    readJson(svfModule);

    F(icfgNode2SVFStmtsMap);
    F(icfgNode2PTASVFStmtsMap);
    F(GepValObjMap);
    F(typeLocSetsMap);
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
    assert(!NodeIDAllocator::allocator && "NodeIDAllocator should be NULL");
    auto nodeIDAllocator = NodeIDAllocator::get();
    JSON_READ_OBJ_FWD(svfirField, nodeIDAllocator);

    return svfIR;
}

const cJSON* SVFIRReader::createObjs(const cJSON* root)
{
#define READ_CREATE_NODE_FWD(GType)                                            \
    [](const cJSON*& nodeJson) {                                               \
        JSON_DEF_READ_FWD(nodeJson, NodeID, id);                               \
        JSON_DEF_READ_FWD(nodeJson, GNodeK, nodeKind);                         \
        return std::make_pair(id, create##GType##Node(id, nodeKind));          \
    }
#define READ_CREATE_EDGE_FWD(GType)                                            \
    [](const cJSON*& edgeJson) {                                               \
        JSON_DEF_READ_FWD(edgeJson, GEdgeFlag, edgeFlag);                      \
        auto kind = applyEdgeMask(edgeFlag);                                   \
        auto edge = create##GType##Edge(kind);                                 \
        setEdgeFlag(edge, edgeFlag);                                           \
        return edge;                                                           \
    }

    ABORT_IFNOT(jsonIsObject(root), "Root should be an object");

    const cJSON* const svfModule = root->child;
    CHECK_JSON_KEY(svfModule);
    svfModuleReader.createObjs(
        svfModule,
        // SVFType Creator
        [](const cJSON*& svfTypeFldJson)
    {
        JSON_DEF_READ_FWD(svfTypeFldJson, SVFType::GNodeK, kind);
        JSON_DEF_READ_FWD(svfTypeFldJson, bool, isSingleValTy);
        return createSVFType(kind, isSingleValTy);
    },
    // SVFType Filler
    [this](const cJSON*& svfVarFldJson, SVFType* type)
    {
        virtFill(svfVarFldJson, type);
    },
    // SVFValue Creator
    [this](const cJSON*& svfValueFldJson)
    {
        JSON_DEF_READ_FWD(svfValueFldJson, SVFValue::GNodeK, kind);
        JSON_DEF_READ_FWD(svfValueFldJson, const SVFType*, type, {});
        JSON_DEF_READ_FWD(svfValueFldJson, std::string, name);
        return createSVFValue(kind, type, std::move(name));
    },
    // SVFValue Filler
    [this](const cJSON*& svfVarFldJson, SVFValue* value)
    {
        virtFill(svfVarFldJson, value);
    },
    // StInfo Creator (no filler needed)
    [this](const cJSON*& stInfoFldJson)
    {
        JSON_DEF_READ_FWD(stInfoFldJson, u32_t, stride);
        auto si = new StInfo(stride);
        fill(stInfoFldJson, si);
        ABORT_IFNOT(!stInfoFldJson, "StInfo has extra field");
        return si;
    });

    const cJSON* const symInfo = svfModule->next;
    CHECK_JSON_KEY(symInfo);
    symTableReader.createObjs(
        symInfo,
        // MemObj Creator (no filler needed)
        [this](const cJSON*& memObjFldJson)
    {
        JSON_DEF_READ_FWD(memObjFldJson, SymID, symId);
        JSON_DEF_READ_FWD(memObjFldJson, ObjTypeInfo*, typeInfo, {});
        JSON_DEF_READ_FWD(memObjFldJson, const SVFValue*, refVal, {});
        return std::make_pair(symId, new MemObj(symId, typeInfo, refVal));
    });

    const cJSON* const icfg = symInfo->next;
    CHECK_JSON_KEY(icfg);
    icfgReader.createObjs(icfg, READ_CREATE_NODE_FWD(ICFG),
                          READ_CREATE_EDGE_FWD(ICFG),
                          [](auto)
    {
        return new SVFLoop({}, 0);
    });

    const cJSON* const chgraph = icfg->next;
    CHECK_JSON_KEY(chgraph);
    chGraphReader.createObjs(chgraph, READ_CREATE_NODE_FWD(CH),
                             READ_CREATE_EDGE_FWD(CH));

    const cJSON* const irGraph = chgraph->next;
    CHECK_JSON_KEY(irGraph);
    irGraphReader.createObjs(irGraph, READ_CREATE_NODE_FWD(PAG),
                             READ_CREATE_EDGE_FWD(PAG));

    icfgReader.fillObjs(
        [this](const cJSON*& j, ICFGNode* node)
    {
        virtFill(j, node);
    },
    [this](const cJSON*& j, ICFGEdge* edge)
    {
        virtFill(j, edge);
    },
    [this](const cJSON*& j, SVFLoop* loop)
    {
        fill(j, loop);
    });
    chGraphReader.fillObjs(
        [this](const cJSON*& j, CHNode* node)
    {
        virtFill(j, node);
    },
    [this](const cJSON*& j, CHEdge* edge)
    {
        virtFill(j, edge);
    });
    irGraphReader.fillObjs(
        [this](const cJSON*& j, SVFVar* var)
    {
        virtFill(j, var);
    },
    [this](const cJSON*& j, SVFStmt* stmt)
    {
        virtFill(j, stmt);
    });

    return irGraph->next;

#undef READ_CREATE_EDGE_FWD
#undef READ_CREATE_NODE_FWD
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

void SVFIRReader::readJson(const cJSON* obj, short& val)
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
                  [](const char* s)
    {
        return std::strtoul(s, nullptr, 10);
    });
}

void SVFIRReader::readJson(const cJSON* obj, long long& val)
{
    readBigNumber(obj, val,
                  [](const char* s)
    {
        return std::strtoll(s, nullptr, 10);
    });
}

void SVFIRReader::readJson(const cJSON* obj, unsigned long long& val)
{
    readBigNumber(obj, val,
                  [](const char* s)
    {
        return std::strtoull(s, nullptr, 10);
    });
}

void SVFIRReader::readJson(const cJSON* obj, std::string& str)
{
    ABORT_IFNOT(jsonIsString(obj), "Expect string for " << obj->string);
    str = obj->valuestring;
}

ICFGNode* SVFIRReader::createICFGNode(NodeID id, GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_MSG(kind << " is an impossible ICFGNodeKind in create()");
#define CASE(kind, constructor)                                                \
    case ICFGNode::kind:                                                       \
        return new constructor(id);
        CASE(IntraBlock, IntraICFGNode);
        CASE(FunEntryBlock, FunEntryICFGNode);
        CASE(FunExitBlock, FunExitICFGNode);
        CASE(FunCallBlock, CallICFGNode);
        CASE(FunRetBlock, RetICFGNode);
        CASE(GlobalBlock, GlobalICFGNode);
#undef CASE
    }
}

ICFGEdge* SVFIRReader::createICFGEdge(GEdgeKind kind)
{
    constexpr ICFGNode* src = nullptr;
    constexpr ICFGNode* dst = nullptr;

    switch (kind)
    {
    default:
        ABORT_MSG(kind << " is an impossible ICFGEdgeKind in create()");
    case ICFGEdge::IntraCF:
        return new IntraCFGEdge(src, dst);
    case ICFGEdge::CallCF:
        return new CallCFGEdge(src, dst, nullptr);
    case ICFGEdge::RetCF:
        return new RetCFGEdge(src, dst, nullptr);
    }
}

CHNode* SVFIRReader::createCHNode(NodeID id, GNodeK kind)
{
    ABORT_IFNOT(kind == 0, "Impossible CHNode kind " << kind);
    return new CHNode("", id);
}

CHEdge* SVFIRReader::createCHEdge(GEdgeKind kind)
{
    ABORT_IFNOT(kind == 0, "Unsupported CHEdge kind " << kind);
    return new CHEdge(nullptr, nullptr, {});
}

SVFVar* SVFIRReader::createPAGNode(NodeID id, GNodeK kind)
{
    switch (kind)
    {
    default:
        ABORT_MSG(kind << " is an impossible SVFVarKind in create()");
#define CASE(kind, constructor)                                                \
    case SVFVar::kind:                                                         \
        return new constructor(id);
        CASE(ValNode, ValVar);
        CASE(RetNode, RetPN);
        CASE(ObjNode, ObjVar);
        CASE(VarargNode, VarArgPN);
        CASE(GepValNode, GepValVar);
        CASE(GepObjNode, GepObjVar);
        CASE(FIObjNode, FIObjVar);
        CASE(DummyValNode, DummyValVar);
        CASE(DummyObjNode, DummyObjVar);
#undef CASE
    }
}

SVFStmt* SVFIRReader::createPAGEdge(GEdgeKind kind)
{
    switch (kind)
    {
    default:
        ABORT_MSG(kind << " is an impossible SVFStmtKind in create()");
#define CASE(kind, constructor)                                                \
    case SVFStmt::kind:                                                        \
        return new constructor;
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

void SVFIRReader::readJson(const cJSON* obj, NodeIDAllocator* idAllocator)
{
    assert(idAllocator && "idAllocator should be nonempty");

    ABORT_IFNOT(jsonIsObject(obj), "Expect object for " << JSON_KEY(obj));
    obj = obj->child;

    JSON_DEF_READ_FWD(obj, int, strategy);
    static_assert(sizeof(idAllocator->strategy) == sizeof(strategy),
                  "idAllocator->strategy should be represented by int");
    idAllocator->strategy = static_cast<NodeIDAllocator::Strategy>(strategy);
    JSON_READ_FIELD_FWD(obj, idAllocator, numObjects);
    JSON_READ_FIELD_FWD(obj, idAllocator, numValues);
    JSON_READ_FIELD_FWD(obj, idAllocator, numSymbols);
    JSON_READ_FIELD_FWD(obj, idAllocator, numNodes);

    ABORT_IFNOT(!obj, "Extra field " << JSON_KEY(obj) << " in NodeIDAllocator");
}

void SVFIRReader::readJson(SymbolTableInfo* symTabInfo)
{
    const cJSON* obj = symTableReader.getFieldJson();
#define F(field) JSON_READ_FIELD_FWD(obj, symTabInfo, field)
    // `allMemObj` was consumed during create & fill phase.
    F(valSymMap);
    F(objSymMap);
    F(returnSymMap);
    F(varargSymMap);
    symTableReader.memObjMap.saveToIDToObjMap(symTabInfo->objMap); // objMap
    F(modelConstants);
    F(totalSymNum);
    F(maxStruct);
    F(maxStSize);
#undef F
    ABORT_IFNOT(!obj, "Extra field " << JSON_KEY(obj) << " in SymbolTableInfo");
}

void SVFIRReader::readJson(IRGraph* graph)
{
    assert(SymbolTableInfo::symInfo && "SymbolTableInfo should be nonempty");
    assert(graph->symInfo == SymbolTableInfo::SymbolInfo() && "symInfo differ");

    auto& valToEdgeMap = graph->valueToEdgeMap;
    valToEdgeMap.clear();

    irGraphReader.saveToGenericGraph(graph);
    const cJSON* obj = irGraphReader.getFieldJson();
#define F(field) JSON_READ_FIELD_FWD(obj, graph, field)
    // base and symInfo have already been read
    F(KindToSVFStmtSetMap);
    F(KindToPTASVFStmtSetMap);
    F(fromFile);
    F(nodeNumAfterPAGBuild);
    F(totalPTAPAGEdge);
    F(valueToEdgeMap);
#undef F

    auto nullit = valToEdgeMap.find(nullptr);
    ABORT_IFNOT(nullit != valToEdgeMap.end(), "valueToEdgeMap should has key NULL");
    ABORT_IFNOT(nullit->second.empty(), "valueToEdgeMap[NULL] should be empty");
}

void SVFIRReader::readJson(ICFG* icfg)
{
    icfgReader.saveToGenericGraph(icfg);
    const cJSON* obj = icfgReader.getFieldJson();
#define F(field) JSON_READ_FIELD_FWD(obj, icfg, field)
    F(totalICFGNode);
    F(FunToFunEntryNodeMap);
    F(FunToFunExitNodeMap);
    F(CSToCallNodeMap);
    F(CSToRetNodeMap);
    F(InstToBlockNodeMap);
    F(globalBlockNode);
    F(icfgNodeToSVFLoopVec);
#undef F
}

void SVFIRReader::readJson(CHGraph* graph)
{
    chGraphReader.saveToGenericGraph(graph);
    const cJSON* obj = chGraphReader.getFieldJson();
#define F(field) JSON_READ_FIELD_FWD(obj, graph, field)
    F(classNum);
    F(vfID);
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
}

void SVFIRReader::readJson(SVFModule* module)
{
    const cJSON* obj = svfModuleReader.getFieldJson();
    auto symInfo = SymbolTableInfo::symInfo;
    assert(symInfo && "SymbolTableInfo should be non-NULL");
    svfModuleReader.svfTypePool.saveToSet(symInfo->svfTypes);
    svfModuleReader.stInfoPool.saveToSet(symInfo->stInfos);

#define F(field) JSON_READ_FIELD_FWD(obj, module, field)
    F(pagReadFromTxt);
    F(moduleIdentifier);
    F(FunctionSet);
    F(GlobalSet);
    F(AliasSet);
    F(ConstantSet);
    F(OtherValueSet);
#undef F
}

void SVFIRReader::readJson(const cJSON* obj, SVFType*& type)
{
    assert(!type && "SVFType already read?");
    type = svfModuleReader.getSVFTypePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, StInfo*& stInfo)
{
    assert(!stInfo && "StInfo already read?");
    stInfo = svfModuleReader.getStInfoPtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, SVFValue*& value)
{
    assert(!value && "SVFValue already read?");
    value = svfModuleReader.getSVFValuePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, SVFVar*& var)
{
    assert(!var && "SVFVar already read?");
    if (jsonIsNullId(obj))
        var = nullptr;
    else
        var = irGraphReader.getNodePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, SVFStmt*& stmt)
{
    assert(!stmt && "SVFStmt already read?");
    stmt = irGraphReader.getEdgePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, ICFGNode*& node)
{
    assert(!node && "ICFGNode already read?");
    NodeID id = jsonGetNumber(obj);
    node = icfgReader.getNodePtr(id);
}

void SVFIRReader::readJson(const cJSON* obj, ICFGEdge*& edge)
{
    assert(!edge && "ICFGEdge already read?");
    edge = icfgReader.getEdgePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, CHNode*& node)
{
    assert(!node && "CHNode already read?");
    node = chGraphReader.getNodePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, CHEdge*& edge)
{
    assert(!edge && "CHEdge already read?");
    edge = chGraphReader.getEdgePtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, CallSite& cs)
{
    readJson(obj, cs.CB);
}

void SVFIRReader::readJson(const cJSON* obj, AccessPath& ap)
{
    ABORT_IFNOT(jsonIsObject(obj), "Expected obj for AccessPath");
    obj = obj->child;
    JSON_READ_FIELD_FWD(obj, &ap, fldIdx);
    JSON_READ_FIELD_FWD(obj, &ap, offsetVarAndGepTypePairs);
    ABORT_IFNOT(!obj, "Extra field " << JSON_KEY(obj) << " in AccessPath");
}

void SVFIRReader::readJson(const cJSON* obj, SVFLoop*& loop)
{
    assert(!loop && "SVFLoop already read?");
    unsigned id = jsonGetNumber(obj);
    loop = icfgReader.getSVFLoopPtr(id);
}

void SVFIRReader::readJson(const cJSON* obj, MemObj*& memObj)
{
    assert(!memObj && "MemObj already read?");
    memObj = symTableReader.getMemObjPtr(jsonGetNumber(obj));
}

void SVFIRReader::readJson(const cJSON* obj, ObjTypeInfo*& objTypeInfo)
{
    assert(!objTypeInfo && "ObjTypeInfo already read?");
    ABORT_IFNOT(jsonIsObject(obj), "Expected object for objTypeInfo");
    cJSON* field = obj->child;

    JSON_DEF_READ_FWD(field, SVFType*, type, {});
    JSON_DEF_READ_FWD(field, u32_t, flags);
    JSON_DEF_READ_FWD(field, u32_t, maxOffsetLimit);
    JSON_DEF_READ_FWD(field, u32_t, elemNum);

    ABORT_IFNOT(!field, "Extra field in objTypeInfo: " << JSON_KEY(field));
    objTypeInfo = new ObjTypeInfo(type, maxOffsetLimit);
    objTypeInfo->flags = flags;
    objTypeInfo->elemNum = elemNum;
}

void SVFIRReader::readJson(const cJSON* obj, SVFLoopAndDomInfo*& ldInfo)
{
    assert(!ldInfo && "SVFLoopAndDomInfo already read?");
    ABORT_IFNOT(jsonIsObject(obj), "Expected object for SVFLoopAndDomInfo");
    cJSON* field = obj->child;

    ldInfo = new SVFLoopAndDomInfo();

    JSON_READ_FIELD_FWD(field, ldInfo, reachableBBs);
    JSON_READ_FIELD_FWD(field, ldInfo, dtBBsMap);
    JSON_READ_FIELD_FWD(field, ldInfo, pdtBBsMap);
    JSON_READ_FIELD_FWD(field, ldInfo, dfBBsMap);
    JSON_READ_FIELD_FWD(field, ldInfo, bb2LoopMap);

    ABORT_IFNOT(!field,
                "Extra field in SVFLoopAndDomInfo: " << JSON_KEY(field));
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
    JSON_READ_FIELD_FWD(fieldJson, var, ap);
    JSON_READ_FIELD_FWD(fieldJson, var, gepValType);
}

void SVFIRReader::fill(const cJSON*& fieldJson, GepObjVar* var)
{
    fill(fieldJson, static_cast<ObjVar*>(var));
    JSON_READ_FIELD_FWD(fieldJson, var, apOffset);
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
        ABORT_MSG("Unknown SVFStmt kind " << kind);

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
    JSON_READ_FIELD_FWD(fieldJson, stmt, ap);
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

void SVFIRReader::fill(const cJSON*& fieldJson, MemObj* memObj)
{
    // symId has already been read
    JSON_READ_FIELD_FWD(fieldJson, memObj, typeInfo);
    JSON_READ_FIELD_FWD(fieldJson, memObj, refVal);
}

void SVFIRReader::fill(const cJSON*& fieldJson, StInfo* stInfo)
{
#define F(field) JSON_READ_FIELD_FWD(fieldJson, stInfo, field)
    // stride has already been read upon construction
    F(numOfFlattenElements);
    F(numOfFlattenFields);
    F(fldIdxVec);
    F(elemIdxVec);
    F(fldIdx2TypeMap);
    F(finfo);
    F(flattenElementTypes);
#undef F
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, ICFGNode* node)
{
    switch (node->getNodeKind())
    {
    default:
        ABORT_MSG("Unknown ICFGNode kind " << node->getNodeKind());

#define CASE(NodeKind, NodeType)                                               \
    case ICFGNode::NodeKind:                                                   \
        return fill(fieldJson, static_cast<NodeType*>(node))

        CASE(IntraBlock, IntraICFGNode);
        CASE(FunEntryBlock, FunEntryICFGNode);
        CASE(FunExitBlock, FunExitICFGNode);
        CASE(FunCallBlock, CallICFGNode);
        CASE(FunRetBlock, RetICFGNode);
        CASE(GlobalBlock, GlobalICFGNode);
#undef CASE
    }
}

void SVFIRReader::fill(const cJSON*& fieldJson, ICFGNode* node)
{
    fill(fieldJson, static_cast<GenericICFGNodeTy*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, fun);
    JSON_READ_FIELD_FWD(fieldJson, node, bb);
    // Skip VFGNodes as it is empty
    JSON_READ_FIELD_FWD(fieldJson, node, pagEdges);
}

void SVFIRReader::fill(const cJSON*& fieldJson, GlobalICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
}

void SVFIRReader::fill(const cJSON*& fieldJson, IntraICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, inst);
}

void SVFIRReader::fill(const cJSON*& fieldJson, InterICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
}

void SVFIRReader::fill(const cJSON*& fieldJson, FunEntryICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, FPNodes);
}

void SVFIRReader::fill(const cJSON*& fieldJson, FunExitICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, formalRet);
}

void SVFIRReader::fill(const cJSON*& fieldJson, CallICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, cs);
    JSON_READ_FIELD_FWD(fieldJson, node, ret);
    JSON_READ_FIELD_FWD(fieldJson, node, APNodes);
}

void SVFIRReader::fill(const cJSON*& fieldJson, RetICFGNode* node)
{
    fill(fieldJson, static_cast<ICFGNode*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, cs);
    JSON_READ_FIELD_FWD(fieldJson, node, actualRet);
    JSON_READ_FIELD_FWD(fieldJson, node, callBlockNode);
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, ICFGEdge* edge)
{
    auto kind = edge->getEdgeKind();
    switch (kind)
    {
    default:
        ABORT_MSG("Unknown ICFGEdge kind " << kind);
    case ICFGEdge::IntraCF:
        return fill(fieldJson, static_cast<IntraCFGEdge*>(edge));
    case ICFGEdge::CallCF:
        return fill(fieldJson, static_cast<CallCFGEdge*>(edge));
    case ICFGEdge::RetCF:
        return fill(fieldJson, static_cast<RetCFGEdge*>(edge));
    }
}

void SVFIRReader::fill(const cJSON*& fieldJson, ICFGEdge* edge)
{
    fill(fieldJson, static_cast<GenericICFGEdgeTy*>(edge));
}

void SVFIRReader::fill(const cJSON*& fieldJson, IntraCFGEdge* edge)
{
    fill(fieldJson, static_cast<ICFGEdge*>(edge));
    JSON_READ_FIELD_FWD(fieldJson, edge, conditionVar);
    JSON_READ_FIELD_FWD(fieldJson, edge, branchCondVal);
}

void SVFIRReader::fill(const cJSON*& fieldJson, CallCFGEdge* edge)
{
    fill(fieldJson, static_cast<ICFGEdge*>(edge));
    JSON_READ_FIELD_FWD(fieldJson, edge, cs);
    JSON_READ_FIELD_FWD(fieldJson, edge, callPEs);
}

void SVFIRReader::fill(const cJSON*& fieldJson, RetCFGEdge* edge)
{
    fill(fieldJson, static_cast<ICFGEdge*>(edge));
    JSON_READ_FIELD_FWD(fieldJson, edge, cs);
    JSON_READ_FIELD_FWD(fieldJson, edge, retPE);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFLoop* loop)
{
#define F(field) JSON_READ_FIELD_FWD(fieldJson, loop, field)
    F(entryICFGEdges);
    F(backICFGEdges);
    F(inICFGEdges);
    F(outICFGEdges);
    F(icfgNodes);
    F(loopBound);
#undef F
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, CHNode* node)
{
    assert(node->getNodeKind() == 0 && "Unknown CHNode kind");
    fill(fieldJson, static_cast<GenericCHNodeTy*>(node));
    JSON_READ_FIELD_FWD(fieldJson, node, vtable);
    JSON_READ_FIELD_FWD(fieldJson, node, className);
    JSON_READ_FIELD_FWD(fieldJson, node, flags);
    JSON_READ_FIELD_FWD(fieldJson, node, virtualFunctionVectors);
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, CHEdge* edge)
{
    assert(edge->getEdgeKind() == 0 && "Unknown CHEdge kind");
    fill(fieldJson, static_cast<GenericCHEdgeTy*>(edge));
    // edgeType is a enum
    JSON_DEF_READ_FWD(fieldJson, unsigned, edgeType);
    if (edgeType == CHEdge::INHERITANCE)
        edge->edgeType = CHEdge::INHERITANCE;
    else if (edgeType == CHEdge::INSTANTCE)
        edge->edgeType = CHEdge::INSTANTCE;
    else
        ABORT_MSG("Unknown CHEdge type " << edgeType);
}

void SVFIRReader::virtFill(const cJSON*& fieldJson, SVFValue* value)
{
    auto kind = value->getKind();

    switch (kind)
    {
    default:
        ABORT_MSG("Impossible SVFValue kind " << kind);

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
    // kind, type, name have already been read.
    JSON_READ_FIELD_FWD(fieldJson, value, ptrInUncalledFun);
    JSON_READ_FIELD_FWD(fieldJson, value, constDataOrAggData);
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

void SVFIRReader::virtFill(const cJSON*& fieldJson, SVFType* type)
{
    auto kind = type->getKind();

    switch (kind)
    {
    default:
        assert(false && "Impossible SVFType kind");

#define CASE(Kind)                                                             \
    case SVFType::Kind:                                                        \
        return fill(fieldJson, SVFUtil::dyn_cast<Kind##pe>(type))

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

void SVFIRReader::fill(const cJSON*& fieldJson, SVFType* type)
{
    // kind has already been read
    JSON_READ_FIELD_FWD(fieldJson, type, getPointerToTy);
    JSON_READ_FIELD_FWD(fieldJson, type, typeinfo);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFPointerType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, ptrElementType);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFIntegerType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, signAndWidth);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFFunctionType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, retTy);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFStructType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, name);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFArrayType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, numOfElement);
    JSON_READ_FIELD_FWD(fieldJson, type, typeOfElement);
}

void SVFIRReader::fill(const cJSON*& fieldJson, SVFOtherType* type)
{
    fill(fieldJson, static_cast<SVFType*>(type));
    JSON_READ_FIELD_FWD(fieldJson, type, repr);
}

SVFIR* SVFIRReader::read(const std::string& path)
{
    struct stat buf;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
    {
        std::string info = "open(\"" + path + "\")";
        perror(info.c_str());
        abort();
    }
    if (fstat(fd, &buf) == -1)
    {
        std::string info = "fstate(\"" + path + "\")";
        perror(info.c_str());
        abort();
    }
    auto addr =
        (char*)mmap(nullptr, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED)
    {
        std::string info = "mmap(content of \"" + path + "\")";
        perror(info.c_str());
        abort();
    }

    auto root = cJSON_ParseWithLength(addr, buf.st_size);

    if (munmap(addr, buf.st_size) == -1)
        perror("munmap()");

    if (close(fd) < 0)
        perror("close()");

    SVFIRReader reader;
    SVFIR* ir = reader.read(root);

    cJSON_Delete(root);
    return ir;
}

} // namespace SVF
