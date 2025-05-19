#include "Graphs/BasicBlockG.h"
#include "Graphs/ICFGNode.h"
#include "SVFIR/GraphDBClient.h"

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

std::string SVFBasicBlock::toDBString() const
{
    const std::string queryStatement ="CREATE (n:SVFBasicBlock {id:'" + std::to_string(getId())+":" + std::to_string(getFunction()->getId()) + "'" +
    ", fun_obj_var_id: " + std::to_string(getFunction()->getId()) +
    ", bb_name:'" + getName() +"'" +
    ", sscc_bb_ids:'" + GraphDBClient::getInstance().extractNodesIds(getSuccBBs()) + "'" +
    ", pred_bb_ids:'" + GraphDBClient::getInstance().extractNodesIds(getPredBBs()) + "'" +
    ", all_icfg_nodes_ids:'" + GraphDBClient::getInstance().extractNodesIds(getICFGNodeList()) + "'" +
    + "})";
    return queryStatement;
}

std::string BasicBlockEdge::toDBString() const
{
    const std::string queryStatement =
    "MATCH (n:SVFBasicBlock {id:'"+std::to_string(getSrcID())+":"+std::to_string(getSrcNode()->getFunction()->getId())+"'}), (m:SVFBasicBlock{id:'"+std::to_string(getDstID())+":"+std::to_string(getDstNode()->getFunction()->getId())+
    "'}) WHERE n.id = '" +std::to_string(getSrcID())+":" + std::to_string(getSrcNode()->getFunction()->getId())+ "'"+
    " AND m.id = '" +std::to_string(getDstID())+":" + std::to_string(getDstNode()->getFunction()->getId())+ "'"+
    " CREATE (n)-[r:BasicBlockEdge{}]->(m)";
    return queryStatement;
}
