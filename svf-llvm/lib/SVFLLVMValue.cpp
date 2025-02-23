#include "SVF-LLVM/SVFLLVMValue.h"
#include "Util/SVFUtil.h"
#include "Graphs/GenericGraph.h"

using namespace SVF;
using namespace SVFUtil;

SVFFunction::SVFFunction(const SVFType* ty, const SVFFunctionType* ft,
                         bool declare, bool intrinsic, bool adt, bool varg,
                         SVFLoopAndDomInfo* ld)
    : SVFLLVMValue(ty, SVFLLVMValue::SVFFunc), isDecl(declare), intrinsic(intrinsic),
      addrTaken(adt), isUncalled(false), isNotRet(false), varArg(varg),
      funcType(ft), loopAndDom(ld), realDefFun(nullptr), exitBlock(nullptr)
{
}

SVFFunction::~SVFFunction()
{
    delete loopAndDom;
    delete bbGraph;
}

u32_t SVFFunction::arg_size() const
{
    return allArgs.size();
}

const ArgValVar* SVFFunction::getArg(u32_t idx) const
{
    assert (idx < allArgs.size() && "getArg() out of range!");
    return allArgs[idx];
}

bool SVFFunction::isVarArg() const
{
    return varArg;
}

const SVFBasicBlock *SVFFunction::getExitBB() const
{
    assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
    assert(exitBlock && "must have an exitBlock");
    return exitBlock;
}

void SVFFunction::setExitBlock(SVFBasicBlock *bb)
{
    assert(!exitBlock && "have already set exit Basicblock!");
    exitBlock = bb;
}



__attribute__((weak))
std::string SVFLLVMValue::toString() const
{
    assert("SVFValue::toString should be implemented or supported by fronted" && false);
    abort();
}

__attribute__((weak))
const std::string SVFValue::valueOnlyToString() const
{
    assert("SVFBaseNode::valueOnlyToString should be implemented or supported by fronted" && false);
    abort();
}