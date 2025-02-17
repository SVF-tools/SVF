#include "Graphs/BasicBlockG.h"
#include "Graphs/ICFGNode.h"

using namespace SVF;
const std::string BasicBlockEdge::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "BasicBlockEdge: " << getSrcNode()->toString() << " -> " << getDstNode()->getName();
    return rawstr.str();
}

const std::string SVFBasicBlock::toString() const
{
    std::string str;
    std::stringstream rawstr(str);
    rawstr << "----------"<< "SVFBasicBlock: " << getName() <<"----------\n";
    for (const ICFGNode* icfgNode : allICFGNodes)
    {
        rawstr << icfgNode->toString();
    }
    rawstr << "\n----------------------------------------\n";
    return rawstr.str();
}
