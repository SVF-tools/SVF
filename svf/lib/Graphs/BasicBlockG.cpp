#include "Graphs/BasicBlockG.h"

using namespace SVF;
const std::string BasicBlockEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "BasicBlockEdge: " << getSrcNode()->toString() << " -> " << getDstNode()->getName();
    return rawstr.str();
}

__attribute__((weak))
const std::string SVFBasicBlock::toString() const
{
    assert("SVFBasicBlock::toString should be implemented or supported by fronted" && false);
    abort();
}
