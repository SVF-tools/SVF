#include "SVFIR/SVFValue.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

cJSON* SVFValue::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = cJSON_CreateObject();
    JSON_DUMP_NUMBER_FIELD(root, kind);
    JSON_DUMP_BOOL_FIELD(root, ptrInUncalledFun);
    JSON_DUMP_BOOL_FIELD(root, constDataOrAggData);
    JSON_DUMP_TYPE_FIELD(dumpInfo, root, type);
    JSON_DUMP_STRING_FIELD(root, name);
    JSON_DUMP_STRING_FIELD(root, sourceLoc);
    return root;
}

cJSON* SVFLoopAndDomInfo::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = cJSON_CreateObject();

    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, reachableBBs);

#define JSON_DUMP_BB_MAP_FIELD(_field)                                         \
    do                                                                         \
    {                                                                          \
        cJSON* node##_field = cJSON_CreateObject();                            \
        for (const auto& BB2BBs : this->_field)                                \
        {                                                                      \
            cJSON* nodeBBs = cJSON_CreateArray();                              \
            for (const SVFBasicBlock* bb : BB2BBs.second)                      \
                cJSON_AddItemToArray(nodeBBs,                                  \
                                     cJSON_CreateStringReference(              \
                                         dumpInfo.getStrValueIndex(bb)));      \
            cJSON_AddItemToObjectCS(node##_field,                              \
                                    dumpInfo.getStrValueIndex(BB2BBs.first),   \
                                    nodeBBs);                                  \
        }                                                                      \
        cJSON_AddItemToObjectCS(root, #_field, node##_field);                  \
    } while (0)

    JSON_DUMP_BB_MAP_FIELD(dtBBsMap);
    JSON_DUMP_BB_MAP_FIELD(pdtBBsMap);
    JSON_DUMP_BB_MAP_FIELD(dfBBsMap);
    JSON_DUMP_BB_MAP_FIELD(bb2LoopMap);

#undef BB_MAP_FIELD_TO_JSON

    return root;
}

cJSON* SVFFunction::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFValue::toJson(dumpInfo);

    // isDecl
    JSON_DUMP_BOOL_FIELD(root, isDecl);
    // intrinsic
    JSON_DUMP_BOOL_FIELD(root, intrinsic);
    // addrTaken
    JSON_DUMP_BOOL_FIELD(root, addrTaken);
    // isUncalled
    JSON_DUMP_BOOL_FIELD(root, isUncalled);
    // isNotRet
    JSON_DUMP_BOOL_FIELD(root, isNotRet);
    // varArg
    JSON_DUMP_BOOL_FIELD(root, varArg);
    // funcType
    JSON_DUMP_TYPE_FIELD(dumpInfo, root, funcType);
    // loopAndDom
    cJSON* nodeLD = loopAndDom->toJson(dumpInfo);
    cJSON_AddItemToObjectCS(root, "loopAndDom", nodeLD);
    // realDefFun
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, realDefFun);
    // allBBs
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, allBBs);
    // owns allArgs
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, allArgs);

    return root;
}

cJSON* SVFBasicBlock::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFValue::toJson(dumpInfo);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, allInsts);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, succBBs);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, predBBs);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, fun);
    return root;
}

cJSON* SVFInstruction::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFValue::toJson(dumpInfo);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, bb);
    JSON_DUMP_BOOL_FIELD(root, terminator);
    JSON_DUMP_BOOL_FIELD(root, ret);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, succInsts);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, predInsts);
    return root;
}

cJSON* SVFCallInst::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFInstruction::toJson(dumpInfo);
    JSON_DUMP_VALUE_LIST_FIELD(dumpInfo, root, args);
    JSON_DUMP_BOOL_FIELD(root, varArg);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, calledVal);
    return root;
}

cJSON* SVFVirtualCallInst::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFCallInst::toJson(dumpInfo);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, vCallVtblPtr);
    JSON_DUMP_NUMBER_FIELD(root, virtualFunIdx);
    JSON_DUMP_STRING_FIELD(root, funNameOfVcall);
    return root;
}

cJSON* SVFConstant::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFValue::toJson(dumpInfo);
}

cJSON* SVFGlobalValue::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFConstant::toJson(dumpInfo);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, realDefGlobal);
    return root;
}

cJSON* SVFArgument::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFValue::toJson(dumpInfo);
    JSON_DUMP_VALUE_FIELD(dumpInfo, root, fun);
    JSON_DUMP_NUMBER_FIELD(root, argNo);
    JSON_DUMP_BOOL_FIELD(root, uncalled);
    return root;
}

cJSON* SVFConstantData::toJson(DumpInfo& dumpInfo) const
{
    return this->SVFConstant::toJson(dumpInfo);
}

cJSON* SVFConstantInt::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFConstantData::toJson(dumpInfo);
    JSON_DUMP_NUMBER_FIELD(root, zval);
    JSON_DUMP_NUMBER_FIELD(root, sval);
    return root;
}

cJSON* SVFConstantFP::toJson(DumpInfo& dumpInfo) const
{
    cJSON* root = this->SVFConstantData::toJson(dumpInfo);
    JSON_DUMP_NUMBER_FIELD(root, dval);
    return root;
}

cJSON* SVFConstantNullPtr::toJson(DumpInfo& dumpInfo) const
{
   return this->SVFConstantData::toJson(dumpInfo);
}

cJSON* SVFBlackHoleValue::toJson(DumpInfo& dumpInfo) const
{
   return this->SVFConstantData::toJson(dumpInfo);
}

cJSON* SVFOtherValue::toJson(DumpInfo& dumpInfo) const
{
   return this->SVFValue::toJson(dumpInfo);
}

cJSON* SVFMetadataAsValue::toJson(DumpInfo& dumpInfo) const
{
   return this->SVFOtherValue::toJson(dumpInfo);
}

/// Add field (index and offset) with its corresponding type
void StInfo::addFldWithType(u32_t fldIdx, const SVFType* type, u32_t elemIdx)
{
    fldIdxVec.push_back(fldIdx);
    elemIdxVec.push_back(elemIdx);
    fldIdx2TypeMap[fldIdx] = type;
}

///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
///  OriginalFieldType of b with field_idx 1 : Struct A
///  FlatternedFieldType of b with field_idx 1 : int
//{@
const SVFType* StInfo::getOriginalElemType(u32_t fldIdx) const
{
    Map<u32_t, const SVFType*>::const_iterator it = fldIdx2TypeMap.find(fldIdx);
    if(it!=fldIdx2TypeMap.end())
        return it->second;
    return nullptr;
}

const SVFLoopAndDomInfo::LoopBBs& SVFLoopAndDomInfo::getLoopInfo(const SVFBasicBlock* bb) const
{
    assert(hasLoopInfo(bb) && "loopinfo does not exit (bb not in a loop)");
    Map<const SVFBasicBlock*, LoopBBs>::const_iterator mapIter = bb2LoopMap.find(bb);
    return mapIter->second;
}

void SVFLoopAndDomInfo::getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const
{
    if (hasLoopInfo(bb))
    {
        const LoopBBs blocks = getLoopInfo(bb);
        if (!blocks.empty())
        {
            for (const SVFBasicBlock* block : blocks)
            {
                for (const SVFBasicBlock* succ : block->getSuccessors())
                {
                    if (std::find(blocks.begin(), blocks.end(), succ)==blocks.end())
                        exitbbs.push_back(succ);
                }
            }
        }
    }
}

bool SVFLoopAndDomInfo::dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
{
    if (bbKey == bbValue)
        return true;

    // An unreachable node is dominated by anything.
    if (isUnreachable(bbValue))
    {
        return true;
    }

    // And dominates nothing.
    if (isUnreachable(bbKey))
    {
        return false;
    }

    const Map<const SVFBasicBlock*,BBSet>& dtBBsMap = getDomTreeMap();
    Map<const SVFBasicBlock*,BBSet>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const BBSet & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }

    return false;
}

bool SVFLoopAndDomInfo::postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
{
    if (bbKey == bbValue)
        return true;

    // An unreachable node is dominated by anything.
    if (isUnreachable(bbValue))
    {
        return true;
    }

    // And dominates nothing.
    if (isUnreachable(bbKey))
    {
        return false;
    }

    const Map<const SVFBasicBlock*,BBSet>& dtBBsMap = getPostDomTreeMap();
    Map<const SVFBasicBlock*,BBSet>::const_iterator mapIter = dtBBsMap.find(bbKey);
    if (mapIter != dtBBsMap.end())
    {
        const BBSet & dtBBs = mapIter->second;
        if (dtBBs.find(bbValue) != dtBBs.end())
        {
            return true;
        }
    }
    return false;
}

bool SVFLoopAndDomInfo::isLoopHeader(const SVFBasicBlock* bb) const
{
    if (hasLoopInfo(bb))
    {
        const LoopBBs& blocks = getLoopInfo(bb);
        assert(!blocks.empty() && "no available loop info?");
        return blocks.front() == bb;
    }
    return false;
}

SVFFunction::SVFFunction(const std::string& f, const SVFType* ty,
                         const SVFFunctionType* ft, bool declare, bool intrin,
                         bool adt, bool varg, SVFLoopAndDomInfo* ld)
    : SVFValue(f, ty, SVFValue::SVFFunc), isDecl(declare), intrinsic(intrin),
      addrTaken(adt), isUncalled(false), isNotRet(false), varArg(varg),
      funcType(ft), loopAndDom(ld), realDefFun(nullptr)
{
}

SVFFunction::~SVFFunction()
{
    for(const SVFBasicBlock* bb : allBBs)
        delete bb;
    for(const SVFArgument* arg : allArgs)
        delete arg;
    delete loopAndDom;
}

u32_t SVFFunction::arg_size() const
{
    return allArgs.size();
}

const SVFArgument* SVFFunction::getArg(u32_t idx) const
{
    assert (idx < allArgs.size() && "getArg() out of range!");
    return allArgs[idx];
}

bool SVFFunction::isVarArg() const
{
    return varArg;
}

SVFBasicBlock::SVFBasicBlock(const std::string& b, const SVFType* ty, const SVFFunction* f):
    SVFValue(b, ty, SVFValue::SVFBB), fun(f)
{
}

SVFBasicBlock::~SVFBasicBlock()
{
    for(const SVFInstruction* inst : allInsts)
        delete inst;
}

/*!
 * Get position of a successor basic block
 */
u32_t SVFBasicBlock::getBBSuccessorPos(const SVFBasicBlock* Succ)
{
    u32_t i = 0;
    for (const SVFBasicBlock* SuccBB: succBBs)
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}

u32_t SVFBasicBlock::getBBSuccessorPos(const SVFBasicBlock* Succ) const
{
    u32_t i = 0;
    for (const SVFBasicBlock* SuccBB: succBBs)
    {
        if (SuccBB == Succ)
            return i;
        i++;
    }
    assert(false && "Didn't find succesor edge?");
    return 0;
}

const SVFInstruction* SVFBasicBlock::getTerminator() const
{
    if (allInsts.empty() || !allInsts.back()->isTerminator())
        return nullptr;
    return allInsts.back();
}

/*!
 * Return a position index from current bb to it successor bb
 */
u32_t SVFBasicBlock::getBBPredecessorPos(const SVFBasicBlock* succbb)
{
    u32_t pos = 0;
    for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
    {
        if(PredBB == this)
            return pos;
        ++pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}
u32_t SVFBasicBlock::getBBPredecessorPos(const SVFBasicBlock* succbb) const
{
    u32_t pos = 0;
    for (const SVFBasicBlock* PredBB : succbb->getPredecessors())
    {
        if(PredBB == this)
            return pos;
        ++pos;
    }
    assert(false && "Didn't find predecessor edge?");
    return pos;
}

SVFInstruction::SVFInstruction(const std::string& i, const SVFType* ty, const SVFBasicBlock* b,  bool tm, bool isRet, SVFValKind k):
    SVFValue(i, ty, k), bb(b), terminator(tm), ret(isRet)
{
}