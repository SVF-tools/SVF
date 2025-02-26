#include "SVF-LLVM/SVFLLVMValue.h"
#include "Util/SVFUtil.h"
#include "Graphs/GenericGraph.h"

using namespace SVF;
using namespace SVFUtil;

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