#include "SVFIR/GraphDBClient.h"
#include "SVFIR/SVFVariables.h"

using namespace SVF;

Map<int,FunObjVar*> id2funObjVarsMap;
Set<SVFBasicBlock*> basicBlocks;
Map<int, RetICFGNode*> id2RetICFGNodeMap;
Map<int, CallPE*> id2CallPEMap;
Map<int, RetPE*> id2RetPEMap;
Map<CallCFGEdge*, std::string> callCFGEdge2CallPEStrMap;
Map<RetCFGEdge*, int> retCFGEdge2RetPEStrMap;

bool GraphDBClient::loadSchema(lgraph::RpcClient* connection,
                               const std::string& filepath,
                               const std::string& dbname)
{
    if (nullptr != connection)
    {
        SVFUtil::outs() << "load schema from file:" << filepath << "\n";
        std::string result;
        bool ret = connection->ImportSchemaFromFile(result, filepath, dbname);
        if (!ret)
        {
            SVFUtil::outs() << dbname<< "Warining: Schema load failed:" << result << "\n";
        }
        return ret;
    }
    return false;
}

// create a new graph name CallGraph in db
bool GraphDBClient::createSubGraph(lgraph::RpcClient* connection, const std::string& graphname)
{
     ///TODO: graph name should be configurable
    if (nullptr != connection)
    {
        std::string result;
        connection->CallCypher(result, "CALL dbms.graph.deleteGraph('"+graphname+"')");
        bool ret = connection->CallCypherToLeader(result, "CALL dbms.graph.createGraph('"+graphname+"')");
        if (!ret)
        {
            SVFUtil::outs()
                << "Warining: Failed to create Graph callGraph:" << result << "\n";
        }
    }
    return false;
}

bool GraphDBClient::addICFGEdge2db(lgraph::RpcClient* connection,
                                   const ICFGEdge* edge,
                                   const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement;
        if(SVFUtil::isa<IntraCFGEdge>(edge))
        {
            queryStatement = getIntraCFGEdgeStmt(SVFUtil::cast<IntraCFGEdge>(edge));
        }
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            queryStatement = getCallCFGEdgeStmt(SVFUtil::cast<CallCFGEdge>(edge));
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            queryStatement = getRetCFGEdgeStmt(SVFUtil::cast<RetCFGEdge>(edge));
        }
        else 
        {
            assert("unknown icfg edge type?");
            return false;
        }
        // SVFUtil::outs() << "ICFGEdge Query Statement:" << queryStatement << "\n";
        std::string result;
        if (queryStatement.empty())
        {
            return false;
        }
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add ICFG edge to db " << dbname << " "
                            << result << "\n";
        }
        return ret;

    }
    return false;
}

bool GraphDBClient::addICFGNode2db(lgraph::RpcClient* connection,
                                   const ICFGNode* node,
                                   const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement;
        if(SVFUtil::isa<GlobalICFGNode>(node))
        {
           queryStatement = getGlobalICFGNodeInsertStmt(SVFUtil::cast<GlobalICFGNode>(node)); 
        }
        else if (SVFUtil::isa<IntraICFGNode>(node))
        {
            queryStatement = getIntraICFGNodeInsertStmt(SVFUtil::cast<IntraICFGNode>(node));
        }
        else if (SVFUtil::isa<FunEntryICFGNode>(node))
        {
            queryStatement = getFunEntryICFGNodeInsertStmt(SVFUtil::cast<FunEntryICFGNode>(node));
        }
        else if (SVFUtil::isa<FunExitICFGNode>(node))
        {
            queryStatement = getFunExitICFGNodeInsertStmt(SVFUtil::cast<FunExitICFGNode>(node));
        }
        else if (SVFUtil::isa<CallICFGNode>(node))
        {
            queryStatement = getCallICFGNodeInsertStmt(SVFUtil::cast<CallICFGNode>(node));
        }
        else if (SVFUtil::isa<RetICFGNode>(node))
        {
            queryStatement = getRetICFGNodeInsertStmt(SVFUtil::cast<RetICFGNode>(node));
        }
        else 
        {
            assert("unknown icfg node type?");
            return false;
        }

        // SVFUtil::outs()<<"ICFGNode Insert Query:"<<queryStatement<<"\n";
        std::string result;
        if (queryStatement.empty())
        {
            return false;
        }
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add icfg node to db " << dbname << " "
                            << result << "\n";
        }
        return ret;
    }
    return false;
}

bool GraphDBClient::addCallGraphNode2db(lgraph::RpcClient* connection,
                                        const CallGraphNode* node,
                                        const std::string& dbname)
{
    if (nullptr != connection)
    {
        const std::string queryStatement ="CREATE (n:CallGraphNode {id: " + std::to_string(node->getId()) +
                                 ", fun_obj_var_id: " + std::to_string(node->getFunction()->getId()) +"})";
        // SVFUtil::outs()<<"CallGraph Node Insert Query:"<<queryStatement<<"\n";
        std::string result;
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add callGraph node to db " << dbname << " "
                            << result << "\n";
        }
        return ret;
    }
    return false;
}

bool GraphDBClient::addCallGraphEdge2db(lgraph::RpcClient* connection,
                                        const CallGraphEdge* edge,
                                        const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string indirectCallIds = "";
        Set<const CallICFGNode*> indirectCall = edge->getIndirectCalls();
        if (indirectCall.size() > 0)
        {
            indirectCallIds = extractNodesIds(indirectCall);
        }

        std::string directCallIds = "";
        Set<const CallICFGNode*> directCall = edge->getDirectCalls();
        if (directCall.size() > 0)
        {
            directCallIds = extractNodesIds(directCall);
        }

        const std::string queryStatement =
            "MATCH (n:CallGraphNode{id:"+std::to_string(edge->getSrcNode()->getId())+"}), (m:CallGraphNode{id:"+std::to_string(edge->getDstNode()->getId()) + "}) WHERE n.id = " +
            std::to_string(edge->getSrcNode()->getId()) +
            " AND m.id = " + std::to_string(edge->getDstNode()->getId()) +
            " CREATE (n)-[r:CallGraphEdge{csid:" + std::to_string(edge->getCallSiteID()) +
            ", kind:" + std::to_string(edge->getEdgeKind()) +
            ", direct_call_set:'" + directCallIds + "', indirect_call_set:'" +
            indirectCallIds + "'}]->(m)";
        // SVFUtil::outs() << "Call Graph Edge Insert Query:" << queryStatement << "\n";
        std::string result;
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add callgraph edge to db " << dbname << " "
                            << result << "\n";
        }
        return ret;
    }
    return false;
}

// pasre the directcallsIds/indirectcallsIds string to vector
std::vector<int> GraphDBClient::stringToIds(const std::string& str)
{
    std::vector<int> ids;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ','))
    {
        ids.push_back(std::stoi(token));
    }
    return ids;
}

std::string GraphDBClient::getGlobalICFGNodeInsertStmt(const GlobalICFGNode* node) {
    const std::string queryStatement ="CREATE (n:GlobalICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) + "})";
    return queryStatement;
}

std::string GraphDBClient::getIntraICFGNodeInsertStmt(const IntraICFGNode* node) {
    const std::string queryStatement ="CREATE (n:IntraICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ", is_return: " + (node->isRetInst() ? "true" : "false") +
    ", fun_obj_var_id:" + std::to_string(node->getFun()->getId()) +
    ", bb_id:" + std::to_string(node->getBB()->getId()) + "})";
    return queryStatement;
}

std::string GraphDBClient::getInterICFGNodeInsertStmt(const InterICFGNode* node) {
    const std::string queryStatement ="CREATE (n:InterICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) + "})";
    return queryStatement;
}

std::string GraphDBClient::getFunEntryICFGNodeInsertStmt(const FunEntryICFGNode* node) {
    const std::string queryStatement ="CREATE (n:FunEntryICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ", fun_obj_var_id:" + std::to_string(node->getFun()->getId()) + 
    ", bb_id:" + std::to_string(node->getBB()->getId()) +
    ", fp_nodes:'" + extractNodesIds(node->getFormalParms()) +"'})";
    return queryStatement;
}

std::string GraphDBClient::getFunExitICFGNodeInsertStmt(const FunExitICFGNode* node) {
    std::string formalRetId = "";
    if (nullptr == node->getFormalRet())
    {
        formalRetId = ",formal_ret_node_id:-1";
    } else {
        formalRetId = ",formal_ret_node_id:" + std::to_string(node->getFormalRet()->getId());
    }
    const std::string queryStatement ="CREATE (n:FunExitICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ", fun_obj_var_id:" + std::to_string(node->getFun()->getId()) + 
    ", bb_id:" + std::to_string(node->getBB()->getId()) +
    formalRetId + "})";
    return queryStatement;
}

std::string GraphDBClient::getCallICFGNodeInsertStmt(const CallICFGNode* node) {
    std::string fun_name_of_v_call = "";
    std::string vtab_ptr_node_id = "";
    std::string virtual_fun_idx = "";
    std::string is_vir_call_inst = node->isVirtualCall() ? "true" : "false";
    std::string virtualFunAppendix = "";
    if (node->isVirtualCall())
    {
        fun_name_of_v_call = ", fun_name_of_v_call: '"+node->getFunNameOfVirtualCall()+"'";
        vtab_ptr_node_id = ", vtab_ptr_node_id:" + std::to_string(node->getVtablePtr()->getId());
        virtual_fun_idx = ", virtual_fun_idx:" + std::to_string(node->getFunIdxInVtable());
        virtualFunAppendix = vtab_ptr_node_id+virtual_fun_idx+fun_name_of_v_call;
    }
    else 
    {
        vtab_ptr_node_id = ", vtab_ptr_node_id:-1";
        virtual_fun_idx = ", virtual_fun_idx:-1";
        virtualFunAppendix = vtab_ptr_node_id+virtual_fun_idx;
    }
    std::string called_fun_obj_var_id = "";
    if (node->getCalledFunction() != nullptr)
    {
        called_fun_obj_var_id = ", called_fun_obj_var_id:" + std::to_string(node->getCalledFunction()->getId());
    }
    else 
    {
        called_fun_obj_var_id = ", called_fun_obj_var_id: -1";
    }
    std::string ret_icfg_node_id = "";
    if (node->getRetICFGNode() != nullptr)
    {
        ret_icfg_node_id = ", ret_icfg_node_id: " + std::to_string(node->getRetICFGNode()->getId());
    }
    else 
    {
        ret_icfg_node_id = ", ret_icfg_node_id: -1";
    }
    const std::string queryStatement ="CREATE (n:CallICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ret_icfg_node_id +
    ", bb_id: " + std::to_string(node->getBB()->getId()) +
    ", fun_obj_var_id: " + std::to_string(node->getFun()->getId()) +
    ", svf_type:'" + node->getType()->toString() + "'" +
    ", ap_nodes:'" + extractNodesIds(node->getActualParms()) +"'" +
    called_fun_obj_var_id +
    ", is_vararg: " + (node->isVarArg() ? "true" : "false") +
    ", is_vir_call_inst: " + (node->isVirtualCall() ? "true" : "false") +
    virtualFunAppendix+"})";
    return queryStatement;
}

std::string GraphDBClient::getRetICFGNodeInsertStmt(const RetICFGNode* node) {
    std::string actual_ret_node_id="";
    if (node->getActualRet() != nullptr)
    {
        actual_ret_node_id = ", actual_ret_node_id: " + std::to_string(node->getActualRet()->getId()) ;
    } else {
        actual_ret_node_id = ", actual_ret_node_id: -1";
    }
    const std::string queryStatement ="CREATE (n:RetICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    actual_ret_node_id+
    ", call_block_node_id: " + std::to_string(node->getCallICFGNode()->getId()) +
    ", bb_id: " + std::to_string(node->getBB()->getId()) +
    ", fun_obj_var_id: " + std::to_string(node->getFun()->getId()) +
    ", svf_type:'" + node->getType()->toString() + "'"+"})";
    return queryStatement;
}

std::string GraphDBClient::getICFGNodeKindString(const ICFGNode* node)
{
    if(SVFUtil::isa<GlobalICFGNode>(node))
    {
        return "GlobalICFGNode";
    }
    else if (SVFUtil::isa<FunEntryICFGNode>(node))
    {
        return "FunEntryICFGNode";
    }
    else if (SVFUtil::isa<FunExitICFGNode>(node))
    {
        return "FunExitICFGNode";
    }
    else if (SVFUtil::isa<CallICFGNode>(node))
    {
        return "CallICFGNode";
    }
    else if (SVFUtil::isa<RetICFGNode>(node))
    {
        return "RetICFGNode";
    }
    else if (SVFUtil::isa<InterICFGNode>(node))
    {
        return "InterICFGNode";
    }
    else if (SVFUtil::isa<IntraICFGNode>(node))
    {
        return "IntraICFGNode";
    }
    else 
    {
        assert("unknown icfg node type?");
        return "";
    }
}

std::string GraphDBClient::getIntraCFGEdgeStmt(const IntraCFGEdge* edge) {
    std::string srcKind = getICFGNodeKindString(edge->getSrcNode());
    std::string dstKind = getICFGNodeKindString(edge->getDstNode());
    std::string condition = "";
    if (edge->getCondition() != nullptr)
    {
        condition = ", condition_var_id:"+ std::to_string(edge->getCondition()->getId()) +
                    ", branch_cond_val:" + std::to_string(edge->getSuccessorCondValue());
    } else {
        condition = ", condition_var_id:-1, branch_cond_val:-1";
    }
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(edge->getDstNode()->getId()) +
        " CREATE (n)-[r:IntraCFGEdge{kind:" + std::to_string(edge->getEdgeKind()) +
        condition +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::getCallCFGEdgeStmt(const CallCFGEdge* edge) {
    std::string srcKind = getICFGNodeKindString(edge->getSrcNode());
    std::string dstKind = getICFGNodeKindString(edge->getDstNode());
    const std::string queryStatement =
    "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(edge->getDstNode()->getId()) +
        " CREATE (n)-[r:CallCFGEdge{kind:" + std::to_string(edge->getEdgeKind()) +
        ", call_pe_ids:'"+ extractEdgesIds(edge->getCallPEs()) +
        "'}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::getRetCFGEdgeStmt(const RetCFGEdge* edge) {
    std::string srcKind = getICFGNodeKindString(edge->getSrcNode());
    std::string dstKind = getICFGNodeKindString(edge->getDstNode());
    std::string ret_pe_id ="";
    if (edge->getRetPE() != nullptr)
    {
        ret_pe_id = ", ret_pe_id:"+ std::to_string(edge->getRetPE()->getEdgeID());
    } else {
        ret_pe_id = ", ret_pe_id:-1";
    }
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(edge->getDstNode()->getId()) +
        " CREATE (n)-[r:RetCFGEdge{kind:" + std::to_string(edge->getEdgeKind()) +
        ret_pe_id+
        "}]->(m)";
    return queryStatement;
}

void GraphDBClient::insertCHG2db(const CHGraph* chg)
{
    std::string chgNodePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/PAGNodeSchema.json";
    std::string chgEdgePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/CHGEdgeSchema.json";
    // add all CHG Node & Edge to DB
    if (nullptr != connection)
    {
        // create a new graph name CHG in db
        createSubGraph(connection, "CHG");
        // load schema for CHG
        loadSchema(connection, chgEdgePath.c_str(), "CHG");
        loadSchema(connection, chgNodePath.c_str(), "CHG");
        std::vector<const CHEdge*> edges;
        for (auto it = chg->begin(); it != chg->end(); ++it)
        {
            CHNode* node = it->second;
            insertCHNode2db(connection, node, "CHG");
            for (auto edgeIter = node->OutEdgeBegin();
                 edgeIter != node->OutEdgeEnd(); ++edgeIter)
            {
                CHEdge* edge = *edgeIter;
                edges.push_back(edge);
            }
        }
        for (const auto& edge : edges)
        {
            insertCHEdge2db(connection, edge, "CHG");
        }
    }
}

void GraphDBClient::insertCHNode2db(lgraph::RpcClient* connection, const CHNode* node, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement;
        if (SVFUtil::isa<CHNode>(node))
        {
            queryStatement = getCHNodeInsertStmt(SVFUtil::cast<CHNode>(node));
        }
        else
        {
            assert("unknown CHG node type?");
            return ;
        }
        // SVFUtil::outs()<<"CHNode Insert Query:"<<queryStatement<<"\n";
        std::string result;
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add CHG node to db " << dbname << " "
                            << result << "\n";
        }
    }
}

std::string GraphDBClient::getCHNodeInsertStmt(const CHNode* node)
{
    const std::string queryStatement ="CREATE (n:CHNode {class_name:'" + node->getName() + "'" +
    ", vtable_id: " + std::to_string(node->getVTable()->getId()) +
    ", flags:'" + std::to_string(node->getFlags()) +"'" +
    ", virtual_function_vectors:'" + extractFuncVectors2String(node->getVirtualFunctionVectors()) + "'" +
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getCHEdgeInsertStmt(const CHEdge* edge)
{
    const std::string queryStatement = 
        "MATCH (n:CHNode{class_name:'"+edge->getSrcNode()->getName()+"'}), (m:CHNode{class_name:'"+edge->getDstNode()->getName()+"'}) WHERE n.class_name = '" +
        edge->getSrcNode()->getName() +
        "' AND m.class_name = '" + edge->getDstNode()->getName() +
        "' CREATE (n)-[r:CHEdge{edge_type:" + std::to_string(edge->getEdgeType()) +
        "}]->(m)";
    return queryStatement;
}

void GraphDBClient::insertCHEdge2db(lgraph::RpcClient* connection, const CHEdge* edge, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement;
        if (SVFUtil::isa<CHEdge>(edge))
        {
            queryStatement = getCHEdgeInsertStmt(SVFUtil::cast<CHEdge>(edge));
        }
        else
        {
            assert("unknown CHG edge type?");
            return ;
        }
        // SVFUtil::outs()<<"CHEdge Insert Query:"<<queryStatement<<"\n";
        std::string result;
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (!ret)
        {
            SVFUtil::outs() << "Warining: Failed to add CHG edge to db " << dbname << " "
                            << result << "\n";
        }
    }
}

void GraphDBClient::insertICFG2db(const ICFG* icfg)
{
    // add all ICFG Node & Edge to DB
    if (nullptr != connection)
    {
        // create a new graph name ICFG in db
        createSubGraph(connection, "ICFG");
        // load schema for CallGraph
        std::string ICFGNodePath =
            SVF_ROOT "/svf/include/Graphs/DBSchema/ICFGNodeSchema.json";
        std::string ICFGEdgePath =
            SVF_ROOT "/svf/include/Graphs/DBSchema/ICFGEdgeSchema.json";
        loadSchema(connection, ICFGNodePath.c_str(), "ICFG");
        loadSchema(connection, ICFGEdgePath.c_str(), "ICFG");
        std::vector<const ICFGEdge*> edges;
        for (auto it = icfg->begin(); it != icfg->end(); ++it)
        {
            ICFGNode* node = it->second;
            addICFGNode2db(connection, node, "ICFG");
            for (auto edgeIter = node->OutEdgeBegin();
                 edgeIter != node->OutEdgeEnd(); ++edgeIter)
            {
                ICFGEdge* edge = *edgeIter;
                edges.push_back(edge);
            }
        }
        for (auto edge : edges)
        {
            addICFGEdge2db(connection, edge, "ICFG");
        }
    }
}

void GraphDBClient::insertCallGraph2db(const CallGraph* callGraph)
{

    std::string callGraphNodePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/CallGraphNodeSchema.json";
    std::string callGraphEdgePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/CallGraphEdgeSchema.json";
    // add all CallGraph Node & Edge to DB
    if (nullptr != connection)
    {
        // create a new graph name CallGraph in db
        createSubGraph(connection, "CallGraph");
        // load schema for CallGraph
        SVF::GraphDBClient::getInstance().loadSchema(
            connection,
            callGraphEdgePath,
            "CallGraph");
        SVF::GraphDBClient::getInstance().loadSchema(
            connection,
            callGraphNodePath,
            "CallGraph");
        std::vector<const CallGraphEdge*> edges;
        for (const auto& item : *callGraph)
        {
            const CallGraphNode* node = item.second;
            SVF::GraphDBClient::getInstance().addCallGraphNode2db(
                connection, node, "CallGraph");
            for (CallGraphEdge::CallGraphEdgeSet::iterator iter =
                     node->OutEdgeBegin();
                 iter != node->OutEdgeEnd(); ++iter)
            {
                const CallGraphEdge* edge = *iter;
                edges.push_back(edge);
            }
        }
        for (const auto& edge : edges)
        {
            SVF::GraphDBClient::getInstance().addCallGraphEdge2db(connection, edge, "CallGraph");
        }

        } else {
        SVFUtil::outs() << "No DB connection, skip inserting CallGraph to DB\n";
        }
}

void GraphDBClient::insertSVFTypeNodeSet2db(const Set<const SVFType*>* types, const Set<const StInfo*>* stInfos, std::string& dbname)
{
    if (nullptr != connection)
    {
        // create a new graph name SVFType in db
        createSubGraph(connection, "SVFType");
        // load schema for SVFType
        loadSchema(connection, SVF_ROOT "/svf/include/Graphs/DBSchema/SVFTypeNodeSchema.json", "SVFType");
        
        // load & insert each svftype node to db
        for (const auto& ty : *types)
        {
            std::string queryStatement;
            if (SVFUtil::isa<SVFPointerType>(ty))
            {
                queryStatement = getSVFPointerTypeNodeInsertStmt(SVFUtil::cast<SVFPointerType>(ty));
            } 
            else if (SVFUtil::isa<SVFIntegerType>(ty))
            {
                queryStatement = getSVFIntegerTypeNodeInsertStmt(SVFUtil::cast<SVFIntegerType>(ty));
            }
            else if (SVFUtil::isa<SVFFunctionType>(ty))
            {
                queryStatement = getSVFFunctionTypeNodeInsertStmt(SVFUtil::cast<SVFFunctionType>(ty));
            }
            else if (SVFUtil::isa<SVFStructType>(ty))
            {
                queryStatement = getSVFSturctTypeNodeInsertStmt(SVFUtil::cast<SVFStructType>(ty));  
            }
            else if (SVFUtil::isa<SVFArrayType>(ty))
            {
                queryStatement = getSVFArrayTypeNodeInsertStmt(SVFUtil::cast<SVFArrayType>(ty));
            }
            else if (SVFUtil::isa<SVFOtherType>(ty))
            {
                queryStatement = getSVFOtherTypeNodeInsertStmt(SVFUtil::cast<SVFOtherType>(ty));   
            }
            else 
            {
                assert("unknown SVF type?");
                return ;
            }
    
            // SVFUtil::outs()<<"SVFType Insert Query:"<<queryStatement<<"\n";
            std::string result;
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add SVFType node to db " << dbname << " "
                                << result << "\n";
            }
        
        }

        // load & insert each stinfo node to db
        for(const auto& stInfo : *stInfos)
        {
            // insert stinfo node to db
            std::string queryStatement = getStInfoNodeInsertStmt(stInfo);
            // SVFUtil::outs()<<"StInfo Insert Query:"<<queryStatement<<"\n";
            std::string result;
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add StInfo node to db " << dbname << " "
                                << result << "\n";
            }
        }
    }

}

std::string GraphDBClient::getSVFPointerTypeNodeInsertStmt(const SVFPointerType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFPointerType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) + "})";
    return queryStatement;
}

std::string GraphDBClient::getSVFIntegerTypeNodeInsertStmt(const SVFIntegerType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFIntegerType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) +
    ", single_and_width:" + std::to_string(node->getSignAndWidth()) + "})";
    return queryStatement;
}

std::string GraphDBClient::getSVFFunctionTypeNodeInsertStmt(const SVFFunctionType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFFunctionType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) +
    ", params_types_vec:'" + extractSVFTypes(node->getParamTypes()) +
    "', ret_ty_node_name:'" + node->getReturnType()->toString() + "'})";
    return queryStatement;
}

std::string GraphDBClient::getSVFSturctTypeNodeInsertStmt(const SVFStructType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFStructType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", stinfo_node_id:" + std::to_string(node->getTypeInfo()->getStinfoId()) +
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) +
    ", struct_name:'" + node->getName() + "'})";
    return queryStatement;
}

std::string GraphDBClient::getSVFArrayTypeNodeInsertStmt(const SVFArrayType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFArrayType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", stinfo_node_id:" + std::to_string(node->getTypeInfo()->getStinfoId()) +
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) +
    ", num_of_element:" + std::to_string(node->getNumOfElement()) + 
    ", type_of_element_node_type_name:'" + node->getTypeOfElement()->toString() + "'})";
    return queryStatement;
}

std::string GraphDBClient::getSVFOtherTypeNodeInsertStmt(const SVFOtherType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFOtherType {type_name:'" + node->toString() +
    "', svf_i8_type_name:'" + node->getSVFInt8Type()->toString() +
    "', svf_ptr_type_name:'" + node->getSVFPtrType()->toString() + 
    "', kind:" + std::to_string(node->getKind()) + 
    ", is_single_val_ty:" + is_single_val_ty + 
    ", byte_size:" + std::to_string(node->getByteSize()) +
    ", repr:'" + node->getRepr() + "'})";
    return queryStatement;
}

std::string GraphDBClient::getStInfoNodeInsertStmt(const StInfo* node)
{
    const std::string queryStatement ="CREATE (n:StInfo {id:" + std::to_string(node->getStinfoId()) +
    ", fld_idx_vec:'" + extractIdxs(node->getFlattenedFieldIdxVec()) +
    "', elem_idx_vec:'" + extractIdxs(node->getFlattenedElemIdxVec()) + 
    "', finfo_types:'" + extractSVFTypes(node->getFlattenFieldTypes()) + 
    "', flatten_element_types:'" + extractSVFTypes(node->getFlattenElementTypes()) + 
    "', fld_idx_2_type_map:'" + extractFldIdx2TypeMap(node->getFldIdx2TypeMap()) +
    "', stride:" + std::to_string(node->getStride()) +
    ", num_of_flatten_elements:" + std::to_string(node->getNumOfFlattenElements()) +
    ", num_of_flatten_fields:" + std::to_string(node->getNumOfFlattenFields()) + "})";
    return queryStatement;
}

void GraphDBClient::insertBasicBlockGraph2db(const BasicBlockGraph* bbGraph)
{
    if (nullptr != connection)
    {
        std::vector<const BasicBlockEdge*> edges;
        for (auto& bb: *bbGraph)
        {
            SVFBasicBlock* node = bb.second;
            insertBBNode2db(connection, node, "BasicBlockGraph");
            for (auto iter = node->OutEdgeBegin(); iter != node->OutEdgeEnd(); ++iter)
            {
                edges.push_back(*iter);
            }
        }
        for (const BasicBlockEdge* edge : edges)
        {
            insertBBEdge2db(connection, edge, "BasicBlockGraph");
        }
    }
}

void GraphDBClient::insertBBEdge2db(lgraph::RpcClient* connection, const BasicBlockEdge* edge, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement = getBBEdgeInsertStmt(edge);
        // SVFUtil::outs()<<"BBEdge Insert Query:"<<queryStatement<<"\n";
        std::string result;
        if (!queryStatement.empty())
        {
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add BB edge to db " << dbname
                                << " " << result << "\n";
            }
        }
    }
}

void GraphDBClient::insertBBNode2db(lgraph::RpcClient* connection, const SVFBasicBlock* node, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement = getBBNodeInsertStmt(node);
        // SVFUtil::outs()<<"BBNode Insert Query:"<<queryStatement<<"\n";
        std::string result;
        if (!queryStatement.empty())
        {
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add BB node to db " << dbname
                                << " " << result << "\n";
            }
        }
    }
}

std::string GraphDBClient::getBBNodeInsertStmt(const SVFBasicBlock* node)
{
    const std::string queryStatement ="CREATE (n:SVFBasicBlock {id:'" + std::to_string(node->getId())+":" + std::to_string(node->getFunction()->getId()) + "'" +
    ", fun_obj_var_id: " + std::to_string(node->getFunction()->getId()) +
    ", bb_name:'" + node->getName() +"'" +
    ", sscc_bb_ids:'" + extractNodesIds(node->getSuccBBs()) + "'" +
    ", pred_bb_ids:'" + extractNodesIds(node->getPredBBs()) + "'" +
    ", all_icfg_nodes_ids:'" + extractNodesIds(node->getICFGNodeList()) + "'" +
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getBBEdgeInsertStmt(const BasicBlockEdge* edge)
{
    const std::string queryStatement =
    "MATCH (n:SVFBasicBlock {id:'"+std::to_string(edge->getSrcID())+":"+std::to_string(edge->getSrcNode()->getFunction()->getId())+"'}), (m:SVFBasicBlock{id:'"+std::to_string(edge->getDstID())+":"+std::to_string(edge->getDstNode()->getFunction()->getId())+
    "'}) WHERE n.id = '" +std::to_string(edge->getSrcID())+":" + std::to_string(edge->getSrcNode()->getFunction()->getId())+ "'"+
    " AND m.id = '" +std::to_string(edge->getDstID())+":" + std::to_string(edge->getDstNode()->getFunction()->getId())+ "'"+
    " CREATE (n)-[r:BasicBlockEdge{}]->(m)";
    return queryStatement;
}

void GraphDBClient::insertPAG2db(const PAG* pag)
{
    std::string pagNodePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/PAGNodeSchema.json";
    std::string pagEdgePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/PAGEdgeSchema.json";
    std::string bbNodePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/BasicBlockNodeSchema.json";
    std::string bbEdgePath =
        SVF_ROOT "/svf/include/Graphs/DBSchema/BasicBlockEdgeSchema.json";

    // add all PAG Node & Edge to DB
    if (nullptr != connection)
    {
        // create a new graph name PAG in db
        createSubGraph(connection, "PAG");
        // create a new graph name BasicBlockGraph in db
        createSubGraph(connection, "BasicBlockGraph");
        // load schema for PAG
        SVF::GraphDBClient::getInstance().loadSchema(connection, pagEdgePath,
                                                     "PAG");
        SVF::GraphDBClient::getInstance().loadSchema(connection, pagNodePath,
                                                     "PAG");
        // load schema for PAG
        SVF::GraphDBClient::getInstance().loadSchema(connection, bbEdgePath,
                                                     "BasicBlockGraph");
        SVF::GraphDBClient::getInstance().loadSchema(connection, bbNodePath,
                                                     "BasicBlockGraph");

        std::vector<const SVFStmt*> edges;
        for (auto it = pag->begin(); it != pag->end(); ++it)
        {
            SVFVar* node = it->second;
            insertPAGNode2db(connection, node, "PAG");
            for (auto edgeIter = node->OutEdgeBegin();
                 edgeIter != node->OutEdgeEnd(); ++edgeIter)
            {
                SVFStmt* edge = *edgeIter;
                edges.push_back(edge);
            }
        }
        for (auto edge : edges)
        {
            insertPAGEdge2db(connection, edge, "PAG");
        }
    }
    else
    {
        SVFUtil::outs() << "No DB connection, skip inserting CallGraph to DB\n";
    }
}

void GraphDBClient::insertPAGEdge2db(lgraph::RpcClient* connection, const SVFStmt* edge, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement = getPAGEdgeInsertStmt(edge);
        // SVFUtil::outs()<<"PAGEdge Insert Query:"<<queryStatement<<"\n";
        std::string result;
        if (!queryStatement.empty())
        {
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add PAG edge to db " << dbname
                                << " " << result << "\n";
            }
        }
    }
}

std::string GraphDBClient::getPAGEdgeInsertStmt(const SVFStmt* edge)
{
    std::string queryStatement = "";
    if(SVFUtil::isa<TDForkPE>(edge))
    {
        queryStatement = generateTDForkPEEdgeInsertStmt(SVFUtil::cast<TDForkPE>(edge));
    } 
    else if(SVFUtil::isa<TDJoinPE>(edge))
    {
        queryStatement = generateTDJoinPEEdgeInsertStmt(SVFUtil::cast<TDJoinPE>(edge));
    }
    else if(SVFUtil::isa<CallPE>(edge))
    {
        queryStatement = generateCallPEEdgeInsertStmt(SVFUtil::cast<CallPE>(edge));
    }
    else if(SVFUtil::isa<RetPE>(edge))
    {
        queryStatement = generateRetPEEdgeInsertStmt(SVFUtil::cast<RetPE>(edge));
    }
    else if(SVFUtil::isa<GepStmt>(edge))
    {
        queryStatement = generateGepStmtEdgeInsertStmt(SVFUtil::cast<GepStmt>(edge));
    }
    else if(SVFUtil::isa<LoadStmt>(edge))
    {
        queryStatement = generateLoadStmtEdgeInsertStmt(SVFUtil::cast<LoadStmt>(edge));
    }
    else if(SVFUtil::isa<StoreStmt>(edge))
    {
        queryStatement = generateStoreStmtEdgeInsertStmt(SVFUtil::cast<StoreStmt>(edge));
    }
    else if(SVFUtil::isa<CopyStmt>(edge))
    {
        queryStatement = generateCopyStmtEdgeInsertStmt(SVFUtil::cast<CopyStmt>(edge));
    }
    else if(SVFUtil::isa<AddrStmt>(edge))
    {
        queryStatement = generateAddrStmtEdgeInsertStmt(SVFUtil::cast<AddrStmt>(edge));
    }
    else if(SVFUtil::isa<AssignStmt>(edge))
    {
        queryStatement = generateAssignStmtEdgeInsertStmt(SVFUtil::cast<AssignStmt>(edge));
    }
    else if(SVFUtil::isa<PhiStmt>(edge))
    {
        queryStatement = generatePhiStmtEdgeInsertStmt(SVFUtil::cast<PhiStmt>(edge));
    }
    else if(SVFUtil::isa<SelectStmt>(edge))
    {
        queryStatement = generateSelectStmtEndgeInsertStmt(SVFUtil::cast<SelectStmt>(edge));
    }
    else if(SVFUtil::isa<CmpStmt>(edge))
    {
        queryStatement = generateCmpStmtEdgeInsertStmt(SVFUtil::cast<CmpStmt>(edge));
    }
    else if(SVFUtil::isa<BinaryOPStmt>(edge))
    {
        queryStatement = generateBinaryOPStmtEdgeInsertStmt(SVFUtil::cast<BinaryOPStmt>(edge));
    }
    else if(SVFUtil::isa<MultiOpndStmt>(edge))
    {
        queryStatement = generateMultiOpndStmtEdgeInsertStmt(SVFUtil::cast<MultiOpndStmt>(edge));
    }
    else if(SVFUtil::isa<UnaryOPStmt>(edge))
    {
        queryStatement = genereateUnaryOPStmtEdgeInsertStmt(SVFUtil::cast<UnaryOPStmt>(edge));
    }
    else if(SVFUtil::isa<BranchStmt>(edge))
    {
        queryStatement = generateBranchStmtEdgeInsertStmt(SVFUtil::cast<BranchStmt>(edge));
    }
    else if(SVFUtil::isa<SVFStmt>(edge))
    {
        queryStatement = generateSVFStmtEdgeInsertStmt(SVFUtil::cast<SVFStmt>(edge));
    }
    else
    {
        assert("unknown SVFStmt type?");
    }
    return queryStatement;
}

void GraphDBClient::insertPAGNode2db(lgraph::RpcClient* connection, const SVFVar* node, const std::string& dbname)
{
    if (nullptr != connection)
    {
        std::string queryStatement = getPAGNodeInsertStmt(node);
        // SVFUtil::outs()<<"PAGNode Insert Query:"<<queryStatement<<"\n";
        std::string result;
        if (!queryStatement.empty())
        {
            bool ret = connection->CallCypher(result, queryStatement, dbname);
            if (!ret)
            {
                SVFUtil::outs() << "Warining: Failed to add PAG node to db " << dbname
                                << " " << result << "\n";
            }
        }
    }
}

std::string GraphDBClient::getPAGNodeInsertStmt(const SVFVar* node)
{
    std::string queryStatement = "";
    if(SVFUtil::isa<ConstNullPtrValVar>(node))
    {
        queryStatement = getConstNullPtrValVarNodeInsertStmt(SVFUtil::cast<ConstNullPtrValVar>(node));
    }
    else if(SVFUtil::isa<ConstIntValVar>(node))
    {
        queryStatement = getConstIntValVarNodeInsertStmt(SVFUtil::cast<ConstIntValVar>(node));
    }
    else if(SVFUtil::isa<ConstFPValVar>(node))
    {
        queryStatement = getConstFPValVarNodeInsertStmt(SVFUtil::cast<ConstFPValVar>(node));
    }
    else if(SVFUtil::isa<BlackHoleValVar>(node))
    {
        queryStatement = getBlackHoleValvarNodeInsertStmt(SVFUtil::cast<BlackHoleValVar>(node));
    }
    else if(SVFUtil::isa<ConstDataValVar>(node))
    {
        queryStatement = getConstDataValVarNodeInsertStmt(SVFUtil::cast<ConstDataValVar>(node));
    }
    else if(SVFUtil::isa<RetValPN>(node))
    {
        queryStatement = getRetValPNNodeInsertStmt(SVFUtil::cast<RetValPN>(node));
    }
    else if(SVFUtil::isa<VarArgValPN>(node))
    {
        queryStatement = getVarArgValPNNodeInsertStmt(SVFUtil::cast<VarArgValPN>(node));
    }
    else if(SVFUtil::isa<DummyValVar>(node))
    {
        queryStatement = getDummyValVarNodeInsertStmt(SVFUtil::cast<DummyValVar>(node));
    }
    else if(SVFUtil::isa<ConstAggValVar>(node))
    {
        queryStatement = getConstAggValVarNodeInsertStmt(SVFUtil::cast<ConstAggValVar>(node));
    }
    else if(SVFUtil::isa<GlobalValVar>(node))
    {
        queryStatement = getGlobalValVarNodeInsertStmt(SVFUtil::cast<GlobalValVar>(node));
    }
    else if(SVFUtil::isa<FunValVar>(node))
    {
        queryStatement = getFunValVarNodeInsertStmt(SVFUtil::cast<FunValVar>(node));
    }
    else if(SVFUtil::isa<GepValVar>(node))
    {
        queryStatement = getGepValVarNodeInsertStmt(SVFUtil::cast<GepValVar>(node));
    }
    else if(SVFUtil::isa<ArgValVar>(node))
    {
        queryStatement = getArgValVarNodeInsertStmt(SVFUtil::cast<ArgValVar>(node));
    }
    else if(SVFUtil::isa<ValVar>(node))
    {
        queryStatement = getValVarNodeInsertStmt(SVFUtil::cast<ValVar>(node));
    }
    else if(SVFUtil::isa<ConstNullPtrObjVar>(node))
    {
        queryStatement = getConstNullPtrObjVarNodeInsertStmt(SVFUtil::cast<ConstNullPtrObjVar>(node));
    }
    else if(SVFUtil::isa<ConstIntObjVar>(node))
    {
        queryStatement = getConstIntObjVarNodeInsertStmt(SVFUtil::cast<ConstIntObjVar>(node));
    }
    else if(SVFUtil::isa<ConstFPObjVar>(node))
    {
        queryStatement = getConstFPObjVarNodeInsertStmt(SVFUtil::cast<ConstFPObjVar>(node));
    }
    else if(SVFUtil::isa<ConstDataObjVar>(node))
    {
        queryStatement = getConstDataObjVarNodeInsertStmt(SVFUtil::cast<ConstDataObjVar>(node));
    }
    else if(SVFUtil::isa<DummyObjVar>(node))
    {
        queryStatement = getDummyObjVarNodeInsertStmt(SVFUtil::cast<DummyObjVar>(node));
    }
    else if(SVFUtil::isa<ConstAggObjVar>(node))
    {
        queryStatement = getConstAggObjVarNodeInsertStmt(SVFUtil::cast<ConstAggObjVar>(node));
    }
    else if(SVFUtil::isa<GlobalObjVar>(node))
    {
        queryStatement = getGlobalObjVarNodeInsertStmt(SVFUtil::cast<GlobalObjVar>(node));
    }
    else if(SVFUtil::isa<FunObjVar>(node))
    {
        const FunObjVar* funObjVar = SVFUtil::cast<FunObjVar>(node);
        queryStatement = getFunObjVarNodeInsertStmt(funObjVar);
        if ( nullptr != funObjVar->getBasicBlockGraph())
        {
            insertBasicBlockGraph2db(funObjVar->getBasicBlockGraph());
        }
    }
    else if(SVFUtil::isa<StackObjVar>(node))
    {
        queryStatement = getStackObjVarNodeInsertStmt(SVFUtil::cast<StackObjVar>(node));
    }
    else if(SVFUtil::isa<HeapObjVar>(node))
    {
        queryStatement = getHeapObjVarNodeInsertStmt(SVFUtil::cast<HeapObjVar>(node));
    } 
    else if(SVFUtil::isa<BaseObjVar>(node))
    {
        queryStatement = getBaseObjNodeInsertStmt(SVFUtil::cast<BaseObjVar>(node));
    }
    else if(SVFUtil::isa<GepObjVar>(node))
    {
        queryStatement = getGepObjVarNodeInsertStmt(SVFUtil::cast<GepObjVar>(node));
    }
    else if(SVFUtil::isa<ObjVar>(node))
    {
        queryStatement = getObjVarNodeInsertStmt(SVFUtil::cast<ObjVar>(node));
    }
    else
    {
        assert("unknown SVFVar type?");
    }
    return queryStatement;
}

std::string GraphDBClient::getSVFVarNodeFieldsStmt(const SVFVar* node)
{
    std::string fieldsStr = "";
    fieldsStr += "id: " + std::to_string(node->getId()) + 
    ", svf_type_name:'"+node->getType()->toString() +
    "', in_edge_kind_to_set_map:'" + pagEdgeToSetMapTyToString(node->getInEdgeKindToSetMap()) +
    "', out_edge_kind_to_set_map:'" + pagEdgeToSetMapTyToString(node->getOutEdgeKindToSetMap()) +
    "'";
    return fieldsStr;
}

std::string GraphDBClient::getValVarNodeFieldsStmt(const ValVar* node)
{
    std::string fieldsStr = getSVFVarNodeFieldsStmt(node);
    if ( nullptr != node->getICFGNode())
    {
        fieldsStr += ", icfg_node_id:" + std::to_string(node->getICFGNode()->getId());
    }
    else
    {
        fieldsStr += ", icfg_node_id:-1";
    }
    return fieldsStr;
}

std::string GraphDBClient::getValVarNodeInsertStmt(const ValVar* node)
{
    const std::string queryStatement ="CREATE (n:ValVar {"+
    getValVarNodeFieldsStmt(node)+
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstDataValVarNodeFieldsStmt(const ConstDataValVar* node)
{
    return getValVarNodeFieldsStmt(node);
}


std::string GraphDBClient::getConstDataValVarNodeInsertStmt(const ConstDataValVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstDataValVar {"+
    getConstDataValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getBlackHoleValvarNodeInsertStmt(const BlackHoleValVar* node)
{
    const std::string queryStatement ="CREATE (n:BlackHoleValVar {"+
    getConstDataValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstFPValVarNodeInsertStmt(const ConstFPValVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstFPValVar {"+
    getConstDataValVarNodeFieldsStmt(node) 
    +", kind:" + std::to_string(node->getNodeKind())
    +", dval:"+ std::to_string(node->getFPValue())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstIntValVarNodeInsertStmt(const ConstIntValVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstIntValVar {"+
    getConstDataValVarNodeFieldsStmt(node) 
    +", kind:" + std::to_string(node->getNodeKind())
    +", zval:'"+ std::to_string(node->getZExtValue()) + "'"
    +", sval:"+ std::to_string(node->getSExtValue())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstNullPtrValVarNodeInsertStmt(const ConstNullPtrValVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstNullPtrValVar {"+
    getConstDataValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getRetValPNNodeInsertStmt(const RetValPN* node)
{
    const std::string queryStatement ="CREATE (n:RetValPN {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    +", call_graph_node_id:"+std::to_string(node->getCallGraphNode()->getId())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getVarArgValPNNodeInsertStmt(const VarArgValPN* node)
{
    const std::string queryStatement ="CREATE (n:VarArgValPN {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    +", call_graph_node_id:"+std::to_string(node->getFunction()->getId())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getDummyValVarNodeInsertStmt(const DummyValVar* node)
{
    const std::string queryStatement ="CREATE (n:DummyValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstAggValVarNodeInsertStmt(const ConstAggValVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstAggValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getGlobalValVarNodeInsertStmt(const GlobalValVar* node)
{
    const std::string queryStatement ="CREATE (n:GlobalValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getFunValVarNodeInsertStmt(const FunValVar* node)
{
    const std::string queryStatement ="CREATE (n:FunValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", fun_obj_var_node_id:" + std::to_string(node->getFunction()->getId())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getGepValVarNodeInsertStmt(const GepValVar* node)
{std::ostringstream accessPathFieldsStr;
    accessPathFieldsStr << "";

    if (nullptr != node->getAccessPath().gepSrcPointeeType())
    {
        accessPathFieldsStr << ", ap_gep_pointee_type_name:'"<<node->getAccessPath().gepSrcPointeeType()->toString()<<"'";
    }
    if (!node->getAccessPath().getIdxOperandPairVec().empty())
    {
        accessPathFieldsStr <<", ap_idx_operand_pairs:'"<< IdxOperandPairsToString(&node->getAccessPath().getIdxOperandPairVec())<<"'";
    }
    const std::string queryStatement ="CREATE (n:GepValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", base_val_id:" + std::to_string(node->getBaseNode()->getId())
    + ", gep_val_svf_type_name:'"+node->getType()->toString()+"'"
    + ", ap_fld_idx:"+std::to_string(node->getConstantFieldIdx())
    + accessPathFieldsStr.str()
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getArgValVarNodeInsertStmt(const ArgValVar* node)
{
    const std::string queryStatement ="CREATE (n:ArgValVar {"+
    getValVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", cg_node_id:" + std::to_string(node->getParent()->getId())
    + ", arg_no:" + std::to_string(node->getArgNo())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getObjVarNodeFieldsStmt(const ObjVar* node)
{
    return getSVFVarNodeFieldsStmt(node);
}

std::string GraphDBClient::getObjVarNodeInsertStmt(const ObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ObjVar {"+
    getObjVarNodeFieldsStmt(node)
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getBaseObjVarNodeFieldsStmt(const BaseObjVar* node)
{
    std::string fieldsStr;
    std::string icfgIDstr = "";
    if ( nullptr != node->getICFGNode())
    {
        icfgIDstr = ", icfg_node_id:" + std::to_string(node->getICFGNode()->getId());
    }
    else 
    {
        icfgIDstr = ", icfg_node_id:-1";
    }
    std::string objTypeInfo_byteSize_str = "";
    if (node->isConstantByteSize())
    {
        objTypeInfo_byteSize_str += ", obj_type_info_byte_size:" + std::to_string(node->getByteSizeOfObj());
    }
    fieldsStr += getObjVarNodeFieldsStmt(node) +
    icfgIDstr + 
    ", obj_type_info_type_name:'" + node->getTypeInfo()->getType()->toString() + "'" + 
    ", obj_type_info_flags:" + std::to_string(node->getTypeInfo()->getFlag()) +
    ", obj_type_info_max_offset_limit:" + std::to_string(node->getMaxFieldOffsetLimit()) + 
    ", obj_type_info_elem_num:" + std::to_string(node->getNumOfElements()) +
    objTypeInfo_byteSize_str;
    return fieldsStr;
}

std::string GraphDBClient::getBaseObjNodeInsertStmt(const BaseObjVar* node)
{
    const std::string queryStatement ="CREATE (n:BaseObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getGepObjVarNodeInsertStmt(const GepObjVar* node)
{
    const std::string queryStatement ="CREATE (n:BaseObjVar {"+
    getObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", base_obj_var_node_id:" + std::to_string(node->getBaseObj()->getId())
    + ", app_offset:" + std::to_string(node->getConstantFieldIdx())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getHeapObjVarNodeInsertStmt(const HeapObjVar* node)
{
    const std::string queryStatement ="CREATE (n:HeapObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getStackObjVarNodeInsertStmt(const StackObjVar* node)
{
    const std::string queryStatement ="CREATE (n:StackObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstDataObjVarNodeFieldsStmt(const ConstDataObjVar* node)
{
    return getBaseObjVarNodeFieldsStmt(node);
}

std::string GraphDBClient::getConstDataObjVarNodeInsertStmt(const ConstDataObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstDataObjVar {"+
    getConstDataObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstNullPtrObjVarNodeInsertStmt(const ConstNullPtrObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstNullPtrObjVar {"+
    getConstDataObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstIntObjVarNodeInsertStmt(const ConstIntObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstIntObjVar {"+
    getConstDataObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", zval:'" + std::to_string(node->getZExtValue()) + "'"
    + ", sval:" + std::to_string(node->getSExtValue())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstFPObjVarNodeInsertStmt(const ConstFPObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstFPObjVar {"+
    getConstDataObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", dval:" + std::to_string(node->getFPValue())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getDummyObjVarNodeInsertStmt(const DummyObjVar* node)
{
    const std::string queryStatement ="CREATE (n:DummyObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getConstAggObjVarNodeInsertStmt(const ConstAggObjVar* node)
{
    const std::string queryStatement ="CREATE (n:ConstAggObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getGlobalObjVarNodeInsertStmt(const GlobalObjVar* node)
{
    const std::string queryStatement ="CREATE (n:GlobalObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + "})";
    return queryStatement;
}

std::string GraphDBClient::getFunObjVarNodeInsertStmt(const FunObjVar* node)
{
    /// TODO: add bbGraph to bbGraph_tuGraph DB
    std::ostringstream exitBBStr;
    if (node->hasBasicBlock() && nullptr != node->getExitBB())
    {
        exitBBStr << ", exit_bb_id:" << std::to_string(node->getExitBB()->getId());
    } 
    else 
    {
        exitBBStr << ", exit_bb_id:-1";
    }
    const std::string queryStatement ="CREATE (n:FunObjVar {"+
    getBaseObjVarNodeFieldsStmt(node) 
    + ", kind:" + std::to_string(node->getNodeKind())
    + ", is_decl:" + (node->isDeclaration()? "true" : "false")
    + ", intrinsic:" + (node->isIntrinsic()? "true" : "false")
    + ", is_addr_taken:" + (node->hasAddressTaken()? "true" : "false")
    + ", is_uncalled:" + (node->isUncalledFunction()? "true" : "false")
    + ", is_not_ret:" + (node->hasReturn()? "true" : "false")
    + ", sup_var_arg:" + (node->isVarArg()? "true" : "false")
    + ", fun_type_name:'" + node->getFunctionType()->toString() + "'"
    + ", real_def_fun_node_id:" + std::to_string(node->getDefFunForMultipleModule()->getId())
    // + ", bb_graph_id:" + std::to_string(node->getBasicBlockGraph()->getFunObjVarId())
    + exitBBStr.str()
    + ", all_args_node_ids:'" + extractNodesIds(node->getArgs()) + "'"
    + ", reachable_bbs:'" + extractNodesIds(node->getReachableBBs()) + "'"
    + ", dt_bbs_map:'" + extractBBsMapWithSet2String(&(node->getDomTreeMap())) + "'"
    + ", pdt_bbs_map:'" + extractBBsMapWithSet2String(&(node->getLoopAndDomInfo()->getPostDomTreeMap())) + "'"
    + ", df_bbs_map:'" + extractBBsMapWithSet2String(&(node->getDomFrontierMap())) + "'"
    + ", bb2_loop_map:'" + extractBBsMapWithSet2String(&(node->getLoopAndDomInfo()->getBB2LoopMap())) + "'"
    + ", bb2_p_dom_level:'" + extractLabelMap2String(&(node->getLoopAndDomInfo()->getBBPDomLevel())) + "'"
    + ", bb2_pi_dom:'" + extractBBsMap2String(&(node->getLoopAndDomInfo()->getBB2PIdom())) + "'"
    + "})";
    return queryStatement;
}

std::string GraphDBClient::generateSVFStmtEdgeFieldsStmt(const SVFStmt* edge)
{
    std::string valueStr = "";
    if (nullptr != edge->getValue())
    {
        valueStr += ", svf_var_node_id:"+ std::to_string(edge->getValue()->getId());
    }
    else
    {
        valueStr += ", svf_var_node_id:-1";
    }
    std::string bb_id_str = "";
    if (nullptr != edge->getBB())
    {
        bb_id_str += ", bb_id:'" + std::to_string(edge->getBB()->getParent()->getId()) + ":" + std::to_string(edge->getBB()->getId())+"'";
    }
    else 
    {
        bb_id_str += ", bb_id:''";
    }

    std::string icfg_node_id_str = "";
    if (nullptr != edge->getICFGNode())
    {
        icfg_node_id_str += ", icfg_node_id:" + std::to_string(edge->getICFGNode()->getId());
    }
    else 
    {
        icfg_node_id_str += ", icfg_node_id:-1";
    }

    std::string inst2_label_map = "";
    if (nullptr != edge->getInst2LabelMap() && !edge->getInst2LabelMap()->empty())
    {
        inst2_label_map += ", inst2_label_map:'"+ extractLabelMap2String(edge->getInst2LabelMap()) +"'";
    }

    std::string var2_label_map = "";
    if (nullptr != edge->getVar2LabelMap() && !edge->getVar2LabelMap()->empty())
    {
        var2_label_map += ", var2_label_map:'"+ extractLabelMap2String(edge->getVar2LabelMap()) +"'";
    }
    std::string fieldsStr = "";
    fieldsStr += "edge_id: " + std::to_string(edge->getEdgeID()) + 
    valueStr +
    bb_id_str +
    icfg_node_id_str +
    inst2_label_map +
    var2_label_map +
    ", call_edge_label_counter:" + std::to_string(*(edge->getCallEdgeLabelCounter())) +
    ", store_edge_label_counter:" + std::to_string(*(edge->getStoreEdgeLabelCounter())) +
    ", multi_opnd_label_counter:" + std::to_string(*(edge->getMultiOpndLabelCounter())) +
    ", edge_flag:" + std::to_string(edge->getEdgeKindWithoutMask());
    return fieldsStr;
}

std::string GraphDBClient::generateSVFStmtEdgeInsertStmt(const SVFStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getSrcNode());
    std::string dstKind = getPAGNodeKindString(edge->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(edge->getDstNode()->getId()) +
        " CREATE (n)-[r:SVFStmt{"+
        generateSVFStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateAssignStmtFieldsStmt(const AssignStmt* edge)
{
    return generateSVFStmtEdgeFieldsStmt(edge);
}

std::string GraphDBClient::generateAssignStmtEdgeInsertStmt(const AssignStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:AssignStmt{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateAddrStmtEdgeInsertStmt(const AddrStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:AddrStmt{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", arr_size:'" + extractNodesIds(edge->getArrSize()) +"'"+
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateCopyStmtEdgeInsertStmt(const CopyStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:CopyStmt{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", copy_kind:" + std::to_string(edge->getCopyKind()) +
        "}]->(m)";
    return queryStatement;   
}

std::string GraphDBClient::generateStoreStmtEdgeInsertStmt(const StoreStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:StoreStmt{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateLoadStmtEdgeInsertStmt(const LoadStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:LoadStmt{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateGepStmtEdgeInsertStmt(const GepStmt* edge)
{
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    std::ostringstream accessPathStr;
    accessPathStr << "";
    if (!edge->isVariantFieldGep())
    {
        accessPathStr << ", ap_fld_idx:"
                      << std::to_string(edge->getConstantStructFldIdx());
    }
    else
    {
        accessPathStr << ", ap_fld_idx:-1";
    }

    if (nullptr != edge->getAccessPath().gepSrcPointeeType())
    {
        accessPathStr << ", ap_gep_pointee_type_name:'"
                      << edge->getAccessPath().gepSrcPointeeType()->toString()
                      << "'";
    }
    if (!edge->getAccessPath().getIdxOperandPairVec().empty())
    {
        accessPathStr << ", ap_idx_operand_pairs:'"
                      << IdxOperandPairsToString(
                             &edge->getAccessPath().getIdxOperandPairVec())
                      << "'";
    }

    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:GepStmt{" + generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        accessPathStr.str() +
        ", variant_field:" + (edge->isVariantFieldGep()? "true" : "false") +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateCallPEEdgeInsertStmt(const CallPE* edge)
{
    std::string callInstStr = "";
    std::string funEntryICFGNodeStr = "";
    if (nullptr != edge->getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(edge->getCallInst()->getId());
    }
    else
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != edge->getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(edge->getFunEntryICFGNode()->getId());
    }
    else 
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:-1";
    }
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:CallPE{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        callInstStr +
        funEntryICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateRetPEEdgeInsertStmt(const RetPE* edge)
{
    std::string callInstStr = "";
    std::string funExitICFGNodeStr = "";
    if (nullptr != edge->getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(edge->getCallInst()->getId());
    }
    else 
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != edge->getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(edge->getFunExitICFGNode()->getId());
    }
    else 
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:-1";
    }
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:RetPE{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        callInstStr +
        funExitICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateTDForkPEEdgeInsertStmt(const TDForkPE* edge)
{
    std::string callInstStr = "";
    std::string funEntryICFGNodeStr = "";
    if (nullptr != edge->getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(edge->getCallInst()->getId());
    }
    else 
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != edge->getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(edge->getFunEntryICFGNode()->getId());
    }
    else 
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:-1";
    }
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:TDForkPE{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        callInstStr +
        funEntryICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateTDJoinPEEdgeInsertStmt(const TDJoinPE* edge)
{
    std::string callInstStr = "";
    std::string funExitICFGNodeStr = "";
    if (nullptr != edge->getCallInst()) 
    {
        callInstStr +=  ", call_icfg_node_id:" + std::to_string(edge->getCallInst()->getId());
    }
    else
    {
        callInstStr +=  ", call_icfg_node_id:-1";
    }

    if (nullptr != edge->getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(edge->getFunExitICFGNode()->getId());
    }
    else 
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:-1";
    }
    std::string srcKind = getPAGNodeKindString(edge->getRHSVar());
    std::string dstKind = getPAGNodeKindString(edge->getLHSVar());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(edge->getRHSVar()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(edge->getLHSVar()->getId())+"}) WHERE n.id = " +
        std::to_string(edge->getRHSVar()->getId()) +
        " AND m.id = " + std::to_string(edge->getLHSVar()->getId()) +
        " CREATE (n)-[r:TDJoinPE{"+
        generateAssignStmtFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        callInstStr +
        funExitICFGNodeStr +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateMultiOpndStmtEdgeFieldsStmt(const MultiOpndStmt* edge)
{
    std::string stmt = generateSVFStmtEdgeFieldsStmt(edge);
    if (! edge->getOpndVars().empty())
    {
        stmt += ", op_var_node_ids:'" + extractNodesIds(edge->getOpndVars())+"'";
    }
    else 
    {
        stmt += ", op_var_node_ids:''";
    }
    return stmt;
}

std::string GraphDBClient::generateMultiOpndStmtEdgeInsertStmt(const MultiOpndStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:MultiOpndStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generatePhiStmtEdgeInsertStmt(const PhiStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:PhiStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", op_icfg_nodes_ids:'" + extractNodesIds(*(edge->getOpICFGNodeVec())) + "'"+
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateSelectStmtEndgeInsertStmt(const SelectStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
       "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:SelectStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", condition_svf_var_node_id:" + std::to_string(edge->getCondition()->getId()) + 
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateCmpStmtEdgeInsertStmt(const CmpStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:CmpStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", predicate:" + std::to_string(edge->getPredicate()) + 
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateBinaryOPStmtEdgeInsertStmt(const BinaryOPStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
       "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:BinaryOPStmt{"+
        generateMultiOpndStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", op_code:" + std::to_string(edge->getOpcode()) + 
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::genereateUnaryOPStmtEdgeInsertStmt(const UnaryOPStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:UnaryOPStmt{"+
        generateSVFStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", op_code:" + std::to_string(edge->getOpcode()) + 
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::generateBranchStmtEdgeInsertStmt(const BranchStmt* edge)
{
    const SVFStmt* stmt = SVFUtil::cast<SVFStmt>(edge); 
    std::string srcKind = getPAGNodeKindString(stmt->getSrcNode());
    std::string dstKind = getPAGNodeKindString(stmt->getDstNode());
    const std::string queryStatement =
        "MATCH (n:"+srcKind+"{id:"+std::to_string(stmt->getSrcNode()->getId())+"}), (m:"+dstKind+"{id:"+std::to_string(stmt->getDstNode()->getId())+"}) WHERE n.id = " +
        std::to_string(stmt->getSrcNode()->getId()) +
        " AND m.id = " + std::to_string(stmt->getDstNode()->getId()) +
        " CREATE (n)-[r:BranchStmt{"+
        generateSVFStmtEdgeFieldsStmt(edge) +
        ", kind:" + std::to_string(edge->getEdgeKind()) +
        ", successors:'" + extractSuccessorsPairSet2String(&(edge->getSuccessors())) + "'"+ 
        ", condition_svf_var_node_id:" + std::to_string(edge->getCondition()->getId()) +
        ", br_inst_svf_var_node_id:" + std::to_string(edge->getBranchInst()->getId()) +
        "}]->(m)";
    return queryStatement;
}

std::string GraphDBClient::getPAGNodeKindString(const SVFVar* node)
{
    if(SVFUtil::isa<ConstNullPtrValVar>(node))
    {
        return "ConstNullPtrValVar";
    }
    else if(SVFUtil::isa<ConstIntValVar>(node))
    {
        return "ConstIntValVar";
    }
    else if(SVFUtil::isa<ConstFPValVar>(node))
    {
        return "ConstFPValVar";
    }
    else if(SVFUtil::isa<BlackHoleValVar>(node))
    {
        return "BlackHoleValVar";
    }
    else if(SVFUtil::isa<ConstDataValVar>(node))
    {
        return "ConstDataValVar";
    }
    else if(SVFUtil::isa<RetValPN>(node))
    {
        return "RetValPN";
    }
    else if(SVFUtil::isa<VarArgValPN>(node))
    {
        return "VarArgValPN";
    }
    else if(SVFUtil::isa<DummyValVar>(node))
    {
        return "DummyValVar";
    }
    else if(SVFUtil::isa<ConstAggValVar>(node))
    {
        return "ConstAggValVar";
    }
    else if(SVFUtil::isa<GlobalValVar>(node))
    {
        return "GlobalValVar";
    }
    else if(SVFUtil::isa<FunValVar>(node))
    {
        return "FunValVar";
    }
    else if(SVFUtil::isa<GepValVar>(node))
    {
        return "GepValVar";
    }
    else if(SVFUtil::isa<ArgValVar>(node))
    {
        return "ArgValVar";
    }
    else if(SVFUtil::isa<ValVar>(node))
    {
        return "ValVar";
    }
    else if(SVFUtil::isa<ConstNullPtrObjVar>(node))
    {
        return "ConstNullPtrObjVar";
    }
    else if(SVFUtil::isa<ConstIntObjVar>(node))
    {
        return "ConstIntObjVar";
    }
    else if(SVFUtil::isa<ConstFPObjVar>(node))
    {
        return "ConstFPObjVar";
    }
    else if(SVFUtil::isa<ConstDataObjVar>(node))
    {
        return "ConstDataObjVar";
    }
    else if(SVFUtil::isa<DummyObjVar>(node))
    {
        return "DummyObjVar";
    }
    else if(SVFUtil::isa<ConstAggObjVar>(node))
    {
        return "ConstAggObjVar";
    }
    else if(SVFUtil::isa<GlobalObjVar>(node))
    {
        return "GlobalObjVar";
    }
    else if(SVFUtil::isa<FunObjVar>(node))
    {
        return "FunObjVar";
    }
    else if(SVFUtil::isa<StackObjVar>(node))
    {
        return "StackObjVar";
    }
    else if(SVFUtil::isa<HeapObjVar>(node))
    {
        return "HeapObjVar";
    } 
    else if(SVFUtil::isa<BaseObjVar>(node))
    {
        return "BaseObjVar";
    }
    else if(SVFUtil::isa<GepObjVar>(node))
    {
        return "GepObjVar";
    }
    else if(SVFUtil::isa<ObjVar>(node))
    {
        return "ObjVar";
    }
   
        assert("unknown SVFVar node type?");
        return "SVFVar";
    
}

void GraphDBClient::readSVFTypesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Build SVF types from DB....\n";
    addSVFTypeNodeFromDB(connection, dbname, pag);
}

void GraphDBClient::addSVFTypeNodeFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    // parse all SVFType
    std::string queryStatement = "MATCH (node) WHERE NOT 'StInfo' IN labels(node) return node";

    Map<std::string, SVFType*> svfTypeMap;
    Map<int, StInfo*> stInfoMap;
    // Map<SVFType::SVFTyKind, Set<SVFType*>> svfTypeKind2SVFTypesMap;
    Map<SVFType*, std::pair<std::string, std::string>> svfi8AndPtrTypeMap;
    Map<std::string, Set<SVFFunctionType*>> functionRetTypeSetMap;
    Map<SVFFunctionType*, std::vector<std::string>> functionParamsTypeSetMap;
    Map<int,Set<SVFType*>> stInfoId2SVFTypeMap;
    Map<std::string, Set<SVFArrayType*>> elementTyepsMap;
    
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    cJSON* node;
    if (nullptr != root)
    {
        cJSON_ArrayForEach(node, root)
        {
            cJSON* data = cJSON_GetObjectItem(node, "node");
            if (!data)
                continue;

            cJSON* properties = cJSON_GetObjectItem(data, "properties");
            if (!properties)
                continue;

            std::string label = cJSON_GetObjectItem(data, "label")->valuestring;

            SVFType* type = nullptr;
            std::string i8Type =
                cJSON_GetObjectItem(properties, "svf_i8_type_name")
                    ->valuestring;
            std::string ptrType =
                cJSON_GetObjectItem(properties, "svf_ptr_type_name")
                    ->valuestring;
            bool svt = cJSON_IsTrue(
                cJSON_GetObjectItem(properties, "is_single_val_ty"));
            int byteSize =
                cJSON_GetObjectItem(properties, "byte_size")->valueint;
            std::string typeNameString =
                cJSON_GetObjectItem(properties, "type_name")->valuestring;

            if (label == "SVFPointerType")
            {
                type = new SVFPointerType(byteSize, svt);
            }
            else if (label == "SVFIntegerType")
            {
                cJSON* single_and_width_Json =
                    cJSON_GetObjectItem(properties, "single_and_width");
                short single_and_width =
                    (short)cJSON_GetNumberValue(single_and_width_Json);
                type = new SVFIntegerType(byteSize, svt, single_and_width);
            }
            else if (label == "SVFFunctionType")
            {
                SVFFunctionType* funType = new SVFFunctionType(svt, byteSize);
                type = funType;
                std::string retTypeName =
                    cJSON_GetObjectItem(properties, "ret_ty_node_name")
                        ->valuestring;
                auto it = svfTypeMap.find(retTypeName);
                if (it != svfTypeMap.end())
                {
                    funType->setReturnType(it->second);
                }
                else
                {
                    functionRetTypeSetMap[retTypeName].insert(funType);
                }
                std::string paramsTypes =
                    cJSON_GetObjectItem(properties, "params_types_vec")
                        ->valuestring;
                if (!paramsTypes.empty())
                {
                    functionParamsTypeSetMap[funType] =
                        parseSVFTypes(paramsTypes);
                }
            }
            else if (label == "SVFOtherType")
            {
                std::string repr =
                    cJSON_GetObjectItem(properties, "repr")->valuestring;
                type = new SVFOtherType(svt, byteSize, repr);
            }
            else if (label == "SVFStructType")
            {
                std::string name =
                    cJSON_GetObjectItem(properties, "struct_name")->valuestring;
                type = new SVFStructType(svt, byteSize, name);
                int stInfoID =
                    cJSON_GetObjectItem(properties, "stinfo_node_id")->valueint;
                auto it = stInfoMap.find(stInfoID);
                if (it != stInfoMap.end())
                {
                    type->setTypeInfo(it->second);
                }
                else
                {
                    stInfoId2SVFTypeMap[stInfoID].insert(type);
                }
            }
            else if (label == "SVFArrayType")
            {
                int numOfElement =
                    cJSON_GetObjectItem(properties, "num_of_element")->valueint;
                SVFArrayType* arrayType =
                    new SVFArrayType(svt, byteSize, numOfElement);
                type = arrayType;
                int stInfoID =
                    cJSON_GetObjectItem(properties, "stinfo_node_id")->valueint;
                auto stInfoIter = stInfoMap.find(stInfoID);
                if (stInfoIter != stInfoMap.end())
                {
                    type->setTypeInfo(stInfoIter->second);
                }
                else
                {
                    stInfoId2SVFTypeMap[stInfoID].insert(type);
                }
                std::string typeOfElementName =
                    cJSON_GetObjectItem(properties,
                                        "type_of_element_node_type_name")
                        ->valuestring;
                auto tyepIter = svfTypeMap.find(typeOfElementName);
                if (tyepIter != svfTypeMap.end())
                {
                    arrayType->setTypeOfElement(tyepIter->second);
                }
                else
                {
                    elementTyepsMap[typeOfElementName].insert(arrayType);
                }
            }
            svfTypeMap.emplace(typeNameString, type);
            // svfTypeKind2SVFTypesMap[type->getSVFTyKind()].insert(type);
            svfi8AndPtrTypeMap[type] = std::make_pair(i8Type, ptrType);
        }
        cJSON_Delete(root);
    }

    // parse all StInfo
    queryStatement = "MATCH (node:StInfo) return node";
    root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON_ArrayForEach(node, root)
        {
            cJSON* data = cJSON_GetObjectItem(node, "node");
            if (!data)
                continue;

            cJSON* properties = cJSON_GetObjectItem(data, "properties");
            if (!properties)
                continue;

            u32_t id = static_cast<u32_t>(
                cJSON_GetObjectItem(properties, "id")->valueint);
            std::string fld_idx_vec =
                cJSON_GetObjectItem(properties, "fld_idx_vec")->valuestring;
            std::vector<u32_t> fldIdxVec =
                parseElements2Container<std::vector<u32_t>>(fld_idx_vec);

            std::string elem_idx_vec =
                cJSON_GetObjectItem(properties, "elem_idx_vec")->valuestring;
            std::vector<u32_t> elemIdxVec =
                parseElements2Container<std::vector<u32_t>>(elem_idx_vec);

            std::string fld_idx_2_type_map =
                cJSON_GetObjectItem(properties, "fld_idx_2_type_map")
                    ->valuestring;
            Map<u32_t, const SVFType*> fldIdx2TypeMap =
                parseStringToFldIdx2TypeMap<Map<u32_t, const SVFType*>>(
                    fld_idx_2_type_map, svfTypeMap);

            std::string finfo_types =
                cJSON_GetObjectItem(properties, "finfo_types")->valuestring;
            std::vector<const SVFType*> finfo =
                parseElementsToSVFTypeContainer<std::vector<const SVFType*>>(
                    finfo_types, svfTypeMap);

            u32_t stride = static_cast<u32_t>(
                cJSON_GetObjectItem(properties, "stride")->valueint);
            u32_t num_of_flatten_elements = static_cast<u32_t>(
                cJSON_GetObjectItem(properties, "num_of_flatten_elements")
                    ->valueint);
            u32_t num_of_flatten_fields = static_cast<u32_t>(
                cJSON_GetObjectItem(properties, "num_of_flatten_fields")
                    ->valueint);
            std::string flatten_element_types =
                cJSON_GetObjectItem(properties, "flatten_element_types")
                    ->valuestring;
            std::vector<const SVFType*> flattenElementTypes =
                parseElementsToSVFTypeContainer<std::vector<const SVFType*>>(
                    flatten_element_types, svfTypeMap);
            StInfo* stInfo =
                new StInfo(id, fldIdxVec, elemIdxVec, fldIdx2TypeMap, finfo,
                           stride, num_of_flatten_elements,
                           num_of_flatten_fields, flattenElementTypes);
            stInfoMap[id] = stInfo;
        }
        cJSON_Delete(root);
    }

    for (auto& [retTypeName, types]:functionRetTypeSetMap)
    {
        auto retTypeIter = svfTypeMap.find(retTypeName);
        if (retTypeIter != svfTypeMap.end())
        {
            for (auto& type: types)
            {
                type->setReturnType(retTypeIter->second);
            }
        }
        else
        {
            SVFUtil::outs()
                << "Warning3: No matching SVFType found for type: " << retTypeName << "\n";
        }
    }
    Set<const SVFType*> ori = pag->getSVFTypes();

    for (auto& [funType, paramsVec]:functionParamsTypeSetMap)
    {
        for (const std::string& param : paramsVec)
        {
            auto paramTypeIter = svfTypeMap.find(param);
            if (paramTypeIter != svfTypeMap.end())
            {
                funType->addParamType(paramTypeIter->second);
            } 
            else
            {
                SVFUtil::outs()<<"Warning2: No matching SVFType found for type: "
                              << param << "\n";
            }
        }
    }

    for (auto&[stInfoId, types] : stInfoId2SVFTypeMap)
    {
        auto stInfoIter = stInfoMap.find(stInfoId);
        if (stInfoIter != stInfoMap.end())
        {
            for (SVFType* type: types)
            {
                type->setTypeInfo(stInfoIter->second);
                if (stInfoIter->second->getNumOfFlattenFields() > pag->maxStSize)
                {
                    pag->maxStSize = stInfoIter->second->getNumOfFlattenFields();
                    pag->maxStruct = type;
                }
            }
        }
        else
        {
            SVFUtil::outs()<<"Warning: No matching StInfo found for id: "
            << stInfoId << "\n";
        }
    }

    for (auto& [elementTypesName, arrayTypes]:elementTyepsMap)
    {
        auto elementTypeIter = svfTypeMap.find(elementTypesName);
        if (elementTypeIter != svfTypeMap.end())
        {
            for (SVFArrayType* type : arrayTypes)
            {
                type->setTypeOfElement(elementTypeIter->second);
            }
        }
        else 
        {
            SVFUtil::outs()<<"Warning1: No matching SVFType found for type: "
            << elementTypesName << "\n";
        }
    }

    for (auto& [svfType, pair]:svfi8AndPtrTypeMap)
    {
        std::string svfi8Type = pair.first;
        std::string svfptrType = pair.second;
        auto i8Type = svfTypeMap.find(svfi8Type);
        auto ptrType = svfTypeMap.find(svfptrType);
        if (i8Type!=svfTypeMap.end())
        {
            svfType->setSVFInt8Type(i8Type->second);
        }
        if (ptrType!= svfTypeMap.end())
        {
            svfType->setSVFPtrType(ptrType->second);
        }
    }
    for (auto& [typeName, type] : svfTypeMap)
    {
        pag->addTypeInfo(type);
    }
    for (auto& [id, stInfo]: stInfoMap)
    {
        pag->addStInfo(stInfo);
    }
}

void GraphDBClient::updateRetPE4RetCFGEdge()
{
    if (retCFGEdge2RetPEStrMap.size() > 0)
    {
        for (auto& [edge, id] : retCFGEdge2RetPEStrMap)
        {
            if (nullptr != edge && id != -1)
            {
                auto it = id2RetPEMap.find(id);
                if (it != id2RetPEMap.end())
                {
                    RetPE* retPE = it->second;
                    edge->addRetPE(retPE);
                }
                else
                {
                    SVFUtil::outs() << "Warning[updateRetPE4RetCFGEdge]: No matching RetPE found for id: " << id << "\n";
                }
            }
        }
    }
}

void GraphDBClient::updateCallPEs4CallCFGEdge()
{
    if (callCFGEdge2CallPEStrMap.size() > 0)
    {
        for (auto& [edge, ids] : callCFGEdge2CallPEStrMap)
        {
            if (nullptr != edge && !ids.empty())
            {
                std::vector<int> idVec = parseElements2Container<std::vector<int>>(ids);
                for (int id : idVec)
                {
                    auto it = id2CallPEMap.find(id);
                    if (it != id2CallPEMap.end())
                    {
                        CallPE* callPE = it->second;
                        edge->addCallPE(callPE);
                    }
                    else
                    {
                        SVFUtil::outs() << "Warning[updateCallPEs4CallCFGEdge]: No matching CallPE found for id: " << id << "\n";
                    }
                }
            }
        }
    }
}

void GraphDBClient::loadSVFPAGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Loading SVF PAG edges from DB....\n";
    readPAGEdgesFromDB(connection, dbname, "AddrStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "CopyStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "StoreStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "LoadStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "GepStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "CallPE", pag);
    readPAGEdgesFromDB(connection, dbname, "RetPE", pag);
    readPAGEdgesFromDB(connection, dbname, "PhiStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "SelectStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "CmpStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "BinaryOPStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "UnaryOPStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "BranchStmt", pag);
    readPAGEdgesFromDB(connection, dbname, "TDForkPE", pag);
    readPAGEdgesFromDB(connection, dbname, "RetPETDJoinPE", pag);
    
    updateCallPEs4CallCFGEdge();
    updateRetPE4RetCFGEdge();
    SVFUtil::outs()<< "Loading SVF PAG edges from DB done....\n";

}

void GraphDBClient::readPAGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string edgeType, SVFIR* pag)
{
    int skip = 0;
    int limit = 1000;
    while (true)
    {
        // SVFUtil::outs() << "Loading SVF PAG edges from DB, edge type: " << edgeType << ", skip: " << skip << ", limit: " << limit << "\n";
        std::string queryStatement =  "MATCH ()-[edge:"+edgeType+"]->() RETURN edge SKIP "+std::to_string(skip)+" LIMIT "+std::to_string(limit);
        cJSON* root = queryFromDB(connection, dbname, queryStatement);
        if ( nullptr == root)
        {
            break;
        }
        else
        {
            cJSON* edge;
            cJSON_ArrayForEach(edge, root)
            {
                cJSON* data = cJSON_GetObjectItem(edge, "edge");
                if (!data)
                    continue;
                cJSON* properties = cJSON_GetObjectItem(data, "properties");
                if (!properties)
                    continue;
    
                // parse src SVFVar & dst SVFVar
                int src_id = cJSON_GetObjectItem(data,"src")->valueint;
                int dst_id = cJSON_GetObjectItem(data,"dst")->valueint;
                SVFVar* srcNode = pag->getGNode(src_id);
                SVFVar* dstNode = pag->getGNode(dst_id);
                if (nullptr == srcNode)
                {
                    SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching src SVFVar found for id: " << src_id << "\n";
                    continue;
                }
                if( nullptr == dstNode)
                {
                    SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching dst SVFVar found for id: " << dst_id << "\n";
                    continue;
                }
    
                int edge_id = cJSON_GetObjectItem(properties,"edge_id")->valueint;
                int svf_var_node_id = cJSON_GetObjectItem(properties,"svf_var_node_id")->valueint; 
                SVFVar* value = nullptr;
                if (svf_var_node_id != -1)
                {
                    value = pag->getGNode(svf_var_node_id);
                }
                int icfg_node_id = cJSON_GetObjectItem(properties,"icfg_node_id")->valueint;
                ICFGNode* icfgNode = nullptr;
                if (icfg_node_id != -1)
                {
                    icfgNode = pag->getICFG()->getICFGNode(icfg_node_id);
                }
    
                std::string bb_id = cJSON_GetObjectItem(properties,"bb_id")->valuestring;
                SVFBasicBlock* bb = nullptr;
                if (!bb_id.empty())
                {
                    std::pair<int, int> pair = parseBBIdPair(bb_id);
                    if (pair.first != -1 && pair.second != -1)
                    {
                        FunObjVar* fun = SVFUtil::dyn_cast<FunObjVar>(pag->getGNode(pair.first));
                        if (nullptr != fun)
                        {
                            bb = fun->getBasicBlockGraph()->getGNode(pair.second);
                            if (nullptr == bb)
                            {
                                SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching BB found for id: " << bb_id << "\n";
                                continue;
                            }
                        }
                    }
                }
    
                int call_edge_label_counter = cJSON_GetObjectItem(properties,"call_edge_label_counter")->valueint; 
                int store_edge_label_counter = cJSON_GetObjectItem(properties,"store_edge_label_counter")->valueint; 
                int multi_opnd_label_counter = cJSON_GetObjectItem(properties,"multi_opnd_label_counter")->valueint; 
                s64_t edgeFlag = static_cast<u64_t>(cJSON_GetObjectItem(properties,"edge_flag")->valueint);
    
                SVFStmt* stmt = nullptr;
    
                if (edgeType == "AddrStmt")
                {
                    stmt = new AddrStmt(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode);
                    std::string arr_size = cJSON_GetObjectItem(properties,"arr_size")->valuestring;
                    AddrStmt* addrStmt = SVFUtil::cast<AddrStmt>(stmt);
                    if (!arr_size.empty())
                    {
                        Set<int> arrSizeVec = parseElements2Container<Set<int>>(arr_size);
                        for (int varId : arrSizeVec)
                        {
                            SVFVar* var = pag->getGNode(varId);
                            if (nullptr != var)
                            {
                                addrStmt->addArrSize(var);
                            }
                            else 
                            {
                                SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching SVFVar found for id: " << varId << " when processing AddrStmt:"<<edge_id<<"\n";
                            }
                        }
                    }
                    pag->addAddrStmt(addrStmt);
                }
                else if (edgeType == "CopyStmt")
                {
                    int copy_kind = cJSON_GetObjectItem(properties,"copy_kind")->valueint; 
                    stmt = new CopyStmt(srcNode, dstNode, edgeFlag, edge_id, value, copy_kind, icfgNode );
                    CopyStmt* copyStmt = SVFUtil::cast<CopyStmt>(stmt);
                    pag->addCopyStmt(copyStmt);
                }
                else if (edgeType == "StoreStmt")
                {
                    stmt = new StoreStmt(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode);
                    StoreStmt* storeStmt = SVFUtil::cast<StoreStmt>(stmt);
                    pag->addStoreStmt(storeStmt, srcNode, dstNode);
                }
                else if (edgeType == "LoadStmt")
                {
                    stmt = new LoadStmt(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode);
                    LoadStmt* loadStmt = SVFUtil::cast<LoadStmt>(stmt);
                    pag->addLoadStmt(loadStmt);
                }
                else if (edgeType == "GepStmt")
                {
                    s64_t fldIdx = cJSON_GetObjectItem(properties, "ap_fld_idx")->valueint;
                    if (fldIdx == -1)
                    {
                        fldIdx = 0;
                    }
                    bool variant_field = cJSON_IsTrue(cJSON_GetObjectItem(properties,"variant_field"));
                    cJSON* ap_gep_pointee_type_name_node = cJSON_GetObjectItem(properties, "ap_gep_pointee_type_name");
                    const SVFType* gepPointeeType = nullptr;
                    std::string ap_gep_pointee_type_name = "";
                    if (nullptr != ap_gep_pointee_type_name_node && nullptr != ap_gep_pointee_type_name_node->valuestring)
                    {
                        ap_gep_pointee_type_name = ap_gep_pointee_type_name_node->valuestring;
                        if (!ap_gep_pointee_type_name.empty())
                        {
                            gepPointeeType = pag->getSVFType(ap_gep_pointee_type_name);
                        }
                    }
                    AccessPath* ap = nullptr;
                    if (nullptr != gepPointeeType)
                    {
                        ap = new AccessPath(fldIdx, gepPointeeType);
                    }
                    else
                    {
                        ap = new AccessPath(fldIdx);
                        if (!ap_gep_pointee_type_name.empty())
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching SVFType found for ap_gep_pointee_type_name: " << ap_gep_pointee_type_name << " when updating GepStmt:"<<edge_id<< "\n";
                    }
                    cJSON* ap_idx_operand_pairs_node = cJSON_GetObjectItem(properties, "ap_idx_operand_pairs");
                    std::string ap_idx_operand_pairs = "";
                    if (nullptr != ap_idx_operand_pairs_node && nullptr != ap_idx_operand_pairs_node->valuestring)
                    {
                        ap_idx_operand_pairs = ap_idx_operand_pairs_node->valuestring;   
                    }
                    parseAPIdxOperandPairsString(ap_idx_operand_pairs, pag, ap);
                    
                    stmt = new GepStmt(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode, *ap, variant_field);
                    GepStmt* gepStmt = SVFUtil::cast<GepStmt>(stmt);
                    pag->addGepStmt(gepStmt);
                }
                else if (edgeType == "CallPE")
                {
                    int call_icfg_node_id = cJSON_GetObjectItem(properties,"call_icfg_node_id")->valueint;
                    int fun_entry_icfg_node_id = cJSON_GetObjectItem(properties,"fun_entry_icfg_node_id")->valueint;
                    const CallICFGNode* callICFGNode = nullptr;
                    const FunEntryICFGNode* funEntryICFGNode = nullptr;
                    if (call_icfg_node_id != -1)
                    {
                        callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(call_icfg_node_id));
                        if (nullptr == callICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching CallICFGNode found for id: " << call_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    if (fun_entry_icfg_node_id != -1)
                    {
                        funEntryICFGNode = SVFUtil::dyn_cast<FunEntryICFGNode>(pag->getICFG()->getGNode(fun_entry_icfg_node_id));
                        if (nullptr == funEntryICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching FunEntryICFGNode found for id: " << fun_entry_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    stmt = new CallPE(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode, callICFGNode, funEntryICFGNode);
                    CallPE* callPE = SVFUtil::cast<CallPE>(stmt);
                    pag->addCallPE(callPE, srcNode, dstNode);
                    id2CallPEMap[edge_id] = callPE;
                }
                else if (edgeType == "TDForkPE")
                {
                    int call_icfg_node_id = cJSON_GetObjectItem(properties,"call_icfg_node_id")->valueint;
                    int fun_entry_icfg_node_id = cJSON_GetObjectItem(properties,"fun_entry_icfg_node_id")->valueint;
                    const CallICFGNode* callICFGNode = nullptr;
                    const FunEntryICFGNode* funEntryICFGNode = nullptr;
                    if (call_icfg_node_id != -1)
                    {
                        callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(call_icfg_node_id));
                        if (nullptr == callICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching CallICFGNode found for id: " << call_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    if (fun_entry_icfg_node_id != -1)
                    {
                        funEntryICFGNode = SVFUtil::dyn_cast<FunEntryICFGNode>(pag->getICFG()->getGNode(fun_entry_icfg_node_id));
                        if (nullptr == funEntryICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching FunEntryICFGNode found for id: " << fun_entry_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    stmt = new TDForkPE(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode, callICFGNode, funEntryICFGNode);
                    TDForkPE* forkPE = SVFUtil::cast<TDForkPE>(stmt);
                    pag->addCallPE(forkPE, srcNode, dstNode);
                    id2CallPEMap[edge_id] = forkPE;
                }
                else if (edgeType == "RetPE")
                {
                    int call_icfg_node_id = cJSON_GetObjectItem(properties,"call_icfg_node_id")->valueint;
                    int fun_exit_icfg_node_id = cJSON_GetObjectItem(properties,"fun_exit_icfg_node_id")->valueint;
                    const CallICFGNode* callICFGNode = nullptr;
                    const FunExitICFGNode* funExitICFGNode = nullptr;
                    if (call_icfg_node_id != -1)
                    {
                        callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(call_icfg_node_id));
                        if (nullptr == callICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching CallICFGNode found for id: " << call_icfg_node_id << "\n";
                            continue;  
                        }
                    }
                    if (fun_exit_icfg_node_id != -1)
                    {
                        funExitICFGNode = SVFUtil::dyn_cast<FunExitICFGNode>(pag->getICFG()->getGNode(fun_exit_icfg_node_id));
                        if (nullptr == funExitICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching FunExitICFGNode found for id: " << fun_exit_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    stmt = new RetPE(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode,callICFGNode, funExitICFGNode);
                    RetPE* retPE = SVFUtil::cast<RetPE>(stmt);
                    pag->addRetPE(retPE, srcNode, dstNode);
                    id2RetPEMap[edge_id] = retPE;
                }
                else if (edgeType == "RetPETDJoinPE")
                {
                    int call_icfg_node_id = cJSON_GetObjectItem(properties,"call_icfg_node_id")->valueint;
                    int fun_exit_icfg_node_id = cJSON_GetObjectItem(properties,"fun_exit_icfg_node_id")->valueint;
                    const CallICFGNode* callICFGNode = nullptr;
                    const FunExitICFGNode* funExitICFGNode = nullptr;
                    if (call_icfg_node_id != -1)
                    {
                        callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(call_icfg_node_id));
                        if (nullptr == callICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching CallICFGNode found for id: " << call_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    if (fun_exit_icfg_node_id != -1)
                    {
                        funExitICFGNode = SVFUtil::dyn_cast<FunExitICFGNode>(pag->getICFG()->getGNode(fun_exit_icfg_node_id));
                        if (nullptr == funExitICFGNode)
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching FunExitICFGNode found for id: " << fun_exit_icfg_node_id << "\n";
                            continue;
                        }
                    }
                    stmt = new TDJoinPE(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode, callICFGNode, funExitICFGNode);
                    TDJoinPE* joinPE = SVFUtil::cast<TDJoinPE>(stmt);
                    pag->addRetPE(joinPE, srcNode, dstNode);
                    id2RetPEMap[edge_id] = joinPE;
                }
                else if (edgeType == "PhiStmt")
                {
                    std::vector<SVFVar*> opVarNodes;
                    std::string op_var_node_ids = cJSON_GetObjectItem(properties, "op_var_node_ids")->valuestring;
                    parseOpVarString(op_var_node_ids, pag, opVarNodes);
                    stmt = new PhiStmt(srcNode, dstNode, edgeFlag, edge_id, value, icfgNode, opVarNodes);
                    PhiStmt* phiStmt = SVFUtil::cast<PhiStmt>(stmt);
                    std::string op_icfg_nodes_ids = cJSON_GetObjectItem(properties, "op_icfg_nodes_ids")->valuestring;
                    if (!op_icfg_nodes_ids.empty())
                    {
                        std::vector<int> opICFGNodeIds = parseElements2Container<std::vector<int>>(op_icfg_nodes_ids);
                        std::vector<const ICFGNode*> opICFGNodes;
                        for (int icfgNodeId : opICFGNodeIds)
                        {
                            ICFGNode* opICFGNode = pag->getICFG()->getGNode(icfgNodeId);
                            if (nullptr != opICFGNode)
                            {
                                opICFGNodes.push_back(opICFGNode);
                            }
                            else
                            {
                                SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching ICFGNode found for id: " << icfgNodeId << "\n";
                            }
                        }
                        phiStmt->setOpICFGNodeVec(opICFGNodes);
                    }  
                    pag->addPhiStmt(phiStmt, srcNode, dstNode);
                }
                else if (edgeType == "SelectStmt")
                {
                    std::vector<SVFVar*> opVarNodes;
                    std::string op_var_node_ids = cJSON_GetObjectItem(properties, "op_var_node_ids")->valuestring;
                    parseOpVarString(op_var_node_ids, pag, opVarNodes);
                    int condition_svf_var_node_id = cJSON_GetObjectItem(properties, "condition_svf_var_node_id")->valueint;
                    SVFVar* condition = pag->getGNode(condition_svf_var_node_id);
                    stmt = new SelectStmt(srcNode, dstNode, edgeFlag, edge_id, condition, value, icfgNode, opVarNodes);
                    SelectStmt* selectStmt = SVFUtil::cast<SelectStmt>(stmt);
                    pag->addSelectStmt(selectStmt, srcNode, dstNode);
                }
                else if (edgeType == "CmpStmt")
                {
                    std::vector<SVFVar*> opVarNodes;
                    std::string op_var_node_ids = cJSON_GetObjectItem(properties, "op_var_node_ids")->valuestring;
                    parseOpVarString(op_var_node_ids, pag, opVarNodes);
                    u32_t predicate = cJSON_GetObjectItem(properties, "predicate")->valueint;
                    stmt = new CmpStmt(srcNode, dstNode, edgeFlag, edge_id, value, predicate, icfgNode, opVarNodes);
                    CmpStmt* cmpStmt = SVFUtil::cast<CmpStmt>(stmt);
                    pag->addCmpStmt(cmpStmt, srcNode, dstNode);
                }
                else if (edgeType == "BinaryOPStmt")
                {
                    std::vector<SVFVar*> opVarNodes;
                    std::string op_var_node_ids = cJSON_GetObjectItem(properties, "op_var_node_ids")->valuestring;
                    parseOpVarString(op_var_node_ids, pag, opVarNodes);
                    u32_t op_code = cJSON_GetObjectItem(properties, "op_code")->valueint;
                    stmt = new BinaryOPStmt(srcNode, dstNode, edgeFlag, edge_id, value, op_code, icfgNode, opVarNodes);
                    BinaryOPStmt* binaryOpStmt = SVFUtil::cast<BinaryOPStmt>(stmt);
                    pag->addBinaryOPStmt(binaryOpStmt, srcNode, dstNode);
                }
                else if (edgeType == "UnaryOPStmt")
                {
                    u32_t op_code = cJSON_GetObjectItem(properties, "op_code")->valueint;
                    stmt = new UnaryOPStmt(srcNode, dstNode, edgeFlag, edge_id, value, op_code, icfgNode);
                    UnaryOPStmt* unaryOpStmt = SVFUtil::cast<UnaryOPStmt>(stmt);
                    pag->addUnaryOPStmt(unaryOpStmt, srcNode, dstNode);
                }
                else if (edgeType == "BranchStmt")
                {
                    int condition_svf_var_node_id = cJSON_GetObjectItem(properties, "condition_svf_var_node_id")->valueint;
                    int br_inst_svf_var_node_id = cJSON_GetObjectItem(properties, "br_inst_svf_var_node_id")->valueint;
                    const SVFVar* condition = pag->getGNode(condition_svf_var_node_id);
                    const SVFVar* brInst = pag->getGNode(br_inst_svf_var_node_id);
                    if (condition == nullptr)
                    {
                        SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching condition SVFVar found for id: " << condition_svf_var_node_id << "\n";
                        continue;
                    }
                    if (brInst == nullptr)
                    {
                        SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching brInst SVFVar found for id: " << br_inst_svf_var_node_id << "\n";
                        continue;
                    }
                    std::string successorsStr = cJSON_GetObjectItem(properties, "successors")->valuestring;
                    std::vector<std::pair<int, s32_t>> successorsIdVec = parseSuccessorsPairSetFromString(successorsStr);
                    std::vector<std::pair<const ICFGNode*, s32_t>> successors;
                    for (auto& pair : successorsIdVec)
                    {
                        const ICFGNode* succ = pag->getICFG()->getGNode(pair.first);
                        if (nullptr != succ)
                        {
                            successors.push_back(std::make_pair(succ, pair.second));
                        }
                        else 
                        {
                            SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching ICFGNode found for id: " << pair.first << "\n";
                        }
                    }
                    stmt = new BranchStmt(srcNode, dstNode, edgeFlag, edge_id, value, successors, condition, brInst, icfgNode);
                    BranchStmt* branchStmt = SVFUtil::cast<BranchStmt>(stmt);
                    pag->addBranchStmt(branchStmt, srcNode, dstNode);
                }
                stmt->setBasicBlock(bb);
                stmt->setCallEdgeLabelCounter(static_cast<u64_t>(call_edge_label_counter));
                stmt->setStoreEdgeLabelCounter(static_cast<u64_t>(store_edge_label_counter));
                stmt->setMultiOpndLabelCounter(static_cast<u64_t>(multi_opnd_label_counter));
                std::string inst2_label_map = cJSON_GetObjectItem(properties,"inst2_label_map")->valuestring;
                std::string var2_label_map = cJSON_GetObjectItem(properties,"var2_label_map")->valuestring; 
                Map<int, u32_t> inst2_label_map_ids = parseLabelMapFromString<Map<int, u32_t>>(inst2_label_map);
                Map<int, u32_t> var2_label_map_ids = parseLabelMapFromString<Map<int, u32_t>>(var2_label_map);
                if (!inst2_label_map_ids.empty())
                {
                    for (auto& [id, label] : inst2_label_map_ids)
                    {
                        const ICFGNode* icfgNode = nullptr;
                        if (id != -1)
                        {
                            icfgNode = pag->getICFG()->getGNode(id);
                            if (nullptr == icfgNode)
                            {
                                SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching ICFGNode found for id: " << id << " when parsing inst2_label_map_ids\n";
                            }
                        }
                        stmt->addInst2Labeled(icfgNode, label);
                    }
                }
                if (!var2_label_map_ids.empty())
                {
                    for (auto& [id, label] : var2_label_map_ids)
                    {
                        const SVFVar* var = nullptr;
                        if (id != -1)
                        {
                            var = pag->getGNode(id);
                            if (nullptr == var)
                            {
                                SVFUtil::outs() << "Warning: [readPAGEdgesFromDB] No matching SVFVar found for id: " << id << " when parsing var2_label_map_ids\n";
                            }
                        }
                        stmt->addVar2Labeled(var, label);
                    }
                }
                skip += 1;
            }
            cJSON_Delete(root);
        }
    }
}

void GraphDBClient::parseOpVarString(std::string& op_var_node_ids, SVFIR* pag, std::vector<SVFVar*>& opVarNodes)
{
    if (!op_var_node_ids.empty())
    {
        std::vector<int> opVarNodeIds = parseElements2Container<std::vector<int>>(op_var_node_ids);
        for (int varId : opVarNodeIds)
        {
            SVFVar* var = pag->getGNode(varId);
            if (nullptr != var)
            {
                opVarNodes.push_back(var);
            }
            else
            {
                SVFUtil::outs() << "Warning: [parseOpVarString] No matching SVFVar found for id: " << varId << "\n";
            }
        }
    }
}

void GraphDBClient::initialSVFPAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Initial SVF PAG nodes from DB....\n";
    readPAGNodesFromDB(connection, dbname, "ValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ArgValVar", pag);
    readPAGNodesFromDB(connection, dbname, "GepValVar", pag);
    readPAGNodesFromDB(connection, dbname, "BaseObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "GepObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "HeapObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "StackObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "FunObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "FunValVar", pag);
    readPAGNodesFromDB(connection, dbname, "GlobalValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstAggValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstDataValVar", pag);
    readPAGNodesFromDB(connection, dbname, "BlackHoleValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstFPValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstIntValVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstNullPtrValVar", pag);
    readPAGNodesFromDB(connection, dbname, "GlobalObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstAggObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstDataObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstFPObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstIntObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "ConstNullPtrObjVar", pag);
    readPAGNodesFromDB(connection, dbname, "RetValPN", pag);
    readPAGNodesFromDB(connection, dbname, "VarArgValPN", pag);
    readPAGNodesFromDB(connection, dbname, "DummyValVar", pag);
    readPAGNodesFromDB(connection, dbname, "DummyObjVar", pag);
}

void GraphDBClient::updatePAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Updating SVF PAG nodes from DB....\n";
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ArgValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "GepValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "BaseObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "GepObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "HeapObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "StackObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "FunObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "FunValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "GlobalValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstAggValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstDataValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "BlackHoleValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstFPValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstIntValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstNullPtrValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "GlobalObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstAggObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstDataObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstFPObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstIntObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "ConstNullPtrObjVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "RetValPN", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "VarArgValPN", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "DummyValVar", pag);
    updateSVFPAGNodesAttributesFromDB(connection, dbname, "DummyObjVar", pag);
}

void GraphDBClient::updateSVFValVarAtrributes(cJSON* properties, ValVar* var, SVFIR* pag)
{
    int icfg_node_id = cJSON_GetObjectItem(properties, "icfg_node_id")->valueint;
    if (icfg_node_id != -1)
    {
        ICFGNode* icfgNode = pag->getICFG()->getGNode(icfg_node_id);
        if (nullptr != icfgNode)
        {
            var->updateSVFValVar(icfgNode);
        }
        else
        {
            SVFUtil::outs() << "Warning: [updateSVFValVarAtrributes] No matching ICFGNode found for id: " << icfg_node_id << " when update SVFVar:"<<var->getId()<<"\n";
        }
    }
}

void GraphDBClient::updateSVFBaseObjVarAtrributes(cJSON* properties, BaseObjVar* var, SVFIR* pag)
{
    int icfg_node_id = cJSON_GetObjectItem(properties, "icfg_node_id")->valueint;
    if (icfg_node_id != -1)
    {
        ICFGNode* icfgNode = pag->getICFG()->getGNode(icfg_node_id);
        if (nullptr != icfgNode)
        {
            var->setICFGNode(icfgNode);
        }
        else
        {
            SVFUtil::outs() << "Warning: [updateSVFValVarAtrributes] No matching ICFGNode found for id: " << icfg_node_id << " when update SVFVar:"<<var->getId()<<"\n";
        }
    }
}

void GraphDBClient::updateFunObjVarAttributes(cJSON* properties, FunObjVar* var, SVFIR* pag)
{
    int real_def_fun_node_id = cJSON_GetObjectItem(properties, "real_def_fun_node_id")->valueint;
    const FunObjVar* realDefFunNode = id2funObjVarsMap[real_def_fun_node_id];
    if (nullptr != realDefFunNode)
    {
        var->setRelDefFun(realDefFunNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching FunObjVar found for id: " << real_def_fun_node_id <<" when updating FunObjVar:"<<var->getId()<< "\n";
    }
    
    int exit_bb_id = cJSON_GetObjectItem(properties, "exit_bb_id")->valueint;
    if (exit_bb_id != -1)
    {
        SVFBasicBlock* exitBB = var->getBasicBlockGraph()->getGNode(exit_bb_id);
        if (nullptr != exitBB)
        {
            var->updateExitBlock(exitBB);
        }
        else
        {
            SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << exit_bb_id <<" when updating FunObjVar:"<<var->getId()<< "\n";
        }
    }

    SVFLoopAndDomInfo* loopAndDom = new SVFLoopAndDomInfo();
    var->setLoopAndDomInfo(loopAndDom);

    std::string reachable_bbs = cJSON_GetObjectItem(properties, "reachable_bbs")->valuestring;
    std::string dt_bbs_map = cJSON_GetObjectItem(properties, "dt_bbs_map")->valuestring;
    std::string pdt_bbs_map = cJSON_GetObjectItem(properties, "pdt_bbs_map")->valuestring;
    std::string df_bbs_map = cJSON_GetObjectItem(properties, "df_bbs_map")->valuestring;
    std::string bb2_loop_map = cJSON_GetObjectItem(properties, "bb2_loop_map")->valuestring;
    std::string bb2_p_dom_level = cJSON_GetObjectItem(properties, "bb2_p_dom_level")->valuestring;
    std::string bb2_pi_dom = cJSON_GetObjectItem(properties, "bb2_pi_dom")->valuestring;

    if (!reachable_bbs.empty())
    {
        std::vector<int> BBListVec = parseElements2Container<std::vector<int>>(reachable_bbs);
        std::vector<const SVFBasicBlock*> reachableBBs;
        for (auto& bbId : BBListVec)
        {
            SVFBasicBlock* bb = var->getBasicBlockGraph()->getGNode(bbId);
            if (nullptr != bb)
            {
                reachableBBs.push_back(bb);
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }
        loopAndDom->setReachableBBs(reachableBBs);
    }

    if (!dt_bbs_map.empty())
    {
        Map<const SVFBasicBlock*, Set<const SVFBasicBlock*>> dtBBsMap;

        Map<int, Set<int>> dt_bbs_map_ids = parseBBsMapFromString<Map<int, Set<int>>>(dt_bbs_map);
        for (auto& [bbId, bbSetIds] : dt_bbs_map_ids)
        {
            SVFBasicBlock* bb = var->getBasicBlockGraph()->getGNode(bbId);
            if (nullptr != bb)
            {
                Set<const SVFBasicBlock*> dtBBSet;
                for (auto& bbSetId : bbSetIds)
                {
                    SVFBasicBlock* bbSet = var->getBasicBlockGraph()->getGNode(bbSetId);
                    if (nullptr != bbSet)
                    {
                        dtBBSet.insert(bbSet);
                    }
                    else
                    {
                        SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbSetId <<" when updating FunObjVar:"<<var->getId()<< "\n";
                    }
                }
                dtBBsMap[bb] = dtBBSet;
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }
        loopAndDom->setDomTreeMap(dtBBsMap);
    }

    if (!pdt_bbs_map.empty())
    {
        Map<const SVFBasicBlock*, Set<const SVFBasicBlock*>> pdtBBsMap;

        Map<int, Set<int>> pdt_bbs_map_ids = parseBBsMapFromString<Map<int, Set<int>>>(pdt_bbs_map);
        for (auto& [bbId, bbSetIds] : pdt_bbs_map_ids)
        {
            SVFBasicBlock* bb = var->getBasicBlockGraph()->getGNode(bbId);
            if (nullptr != bb)
            {
                Set<const SVFBasicBlock*> pdtBBSet;
                for (auto& bbSetId : bbSetIds)
                {
                    SVFBasicBlock* bbSet = var->getBasicBlockGraph()->getGNode(bbSetId);
                    if (nullptr != bbSet)
                    {
                        pdtBBSet.insert(bbSet);
                    }
                    else
                    {
                        SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbSetId <<" when updating FunObjVar:"<<var->getId()<< "\n";
                    }
                }
                pdtBBsMap[bb] = pdtBBSet;
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }
        loopAndDom->setPostDomTreeMap(pdtBBsMap);
    }

    if (!df_bbs_map.empty())
    {
        Map<const SVFBasicBlock*, Set<const SVFBasicBlock*>> dfBBsMap;

        Map<int, Set<int>> df_bbs_map_ids = parseBBsMapFromString<Map<int, Set<int>>>(df_bbs_map);
        for (auto& [bbId, bbSetIds] : df_bbs_map_ids)
        {
            SVFBasicBlock* bb = var->getBasicBlockGraph()->getGNode(bbId);
            if (nullptr != bb)
            {
                Set<const SVFBasicBlock*> dfBBSet;
                for (auto& bbSetId : bbSetIds)
                {
                    SVFBasicBlock* bbSet = var->getBasicBlockGraph()->getGNode(bbSetId);
                    if (nullptr != bbSet)
                    {
                        dfBBSet.insert(bbSet);
                    }
                    else
                    {
                        SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbSetId <<" when updating FunObjVar:"<<var->getId()<< "\n";
                    }
                }
                dfBBsMap[bb] = dfBBSet;
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }
        loopAndDom->setDomFrontierMap(dfBBsMap);
    }

    if (!bb2_loop_map.empty())
    {
        Map<const SVFBasicBlock*, std::vector<const SVFBasicBlock*>> bb2LoopMap;

        Map<int, std::vector<int>> bb2_loop_map_ids = parseBBsMapFromString<Map<int, std::vector<int>>>(bb2_loop_map);
        for (auto& [bbId, bbSetIds] : bb2_loop_map_ids)
        {
            SVFBasicBlock* bb = var->getBasicBlockGraph()->getGNode(bbId);
            if (nullptr != bb)
            {
                std::vector<const SVFBasicBlock*> loopBBs;
                for (auto& bbSetId : bbSetIds)
                {
                    SVFBasicBlock* bbSet = var->getBasicBlockGraph()->getGNode(bbSetId);
                    if (nullptr != bbSet)
                    {
                        loopBBs.push_back(bbSet);
                    }
                    else
                    {
                        SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbSetId <<" when updating FunObjVar:"<<var->getId()<< "\n";
                    }
                }
                bb2LoopMap[bb] = loopBBs;
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }
        loopAndDom->setBB2LoopMap(bb2LoopMap);
    }
    
    if (!bb2_p_dom_level.empty())
    {
        Map<const SVFBasicBlock*, u32_t> bb2PdomLevel;
        Map<int, u32_t> bb2_p_dom_level_ids = parseLabelMapFromString<Map<int, u32_t>>(bb2_p_dom_level);
        for (auto& [bbId, value] : bb2_p_dom_level_ids)
        {
            SVFBasicBlock* bb = nullptr;
            if (bbId != -1)
            {
                bb = var->getBasicBlockGraph()->getGNode(bbId);
            }
            if (nullptr != bb)
            {
                bb2PdomLevel[bb] = value;
            }
            else
            {
                SVFUtil::outs() << "Warning: [updateFunObjVarAttributes] No matching BasicBlock found for id: " << bbId <<" when updating FunObjVar:"<<var->getId()<< "\n";
            }
        }

        loopAndDom->setBB2PdomLevel(bb2PdomLevel);
    }

    if (!bb2_pi_dom.empty())
    {
        Map<const SVFBasicBlock*, const SVFBasicBlock*> bb2PiDom;
        
        Map<int , int> bb2_pi_dom_ids = parseBB2PiMapFromString<Map<int, int>>(bb2_pi_dom);
        for (auto& [key,value] : bb2_pi_dom_ids)
        {
            SVFBasicBlock* keyBB = nullptr;
            SVFBasicBlock* valueBB = nullptr;
            if (key != -1)
            {
                keyBB = var->getBasicBlockGraph()->getGNode(key);
            }
            if (value != -1)
            {
                valueBB = var->getBasicBlockGraph()->getGNode(value);
            }
            bb2PiDom[keyBB] = valueBB;
        }
        loopAndDom->setBB2PIdom(bb2PiDom);
    }
}
void GraphDBClient::updateGepValVarAttributes(cJSON* properties, GepValVar* var, SVFIR* pag)
{
    int base_val_id = cJSON_GetObjectItem(properties, "base_val_id")->valueint;
    ValVar* baseVal = SVFUtil::dyn_cast<ValVar>(pag->getGNode(base_val_id));
    if (nullptr != baseVal)
    {
        var->setBaseNode(baseVal);
    }
    else
    {
        SVFUtil::outs() << "Warning: [updateGepValVarAttributes] No matching ValVar found for id: "
                        << base_val_id << " when updating GepValVar:" << var->getId()
                        << "\n";
    }
    s64_t fldIdx = cJSON_GetObjectItem(properties, "ap_fld_idx")->valueint;
    cJSON* ap_gep_pointee_type_name_node = cJSON_GetObjectItem(properties, "ap_gep_pointee_type_name");
    std::string ap_gep_pointee_type_name = "";
    if (nullptr != ap_gep_pointee_type_name_node && nullptr != ap_gep_pointee_type_name_node->valuestring)
    {
        ap_gep_pointee_type_name = ap_gep_pointee_type_name_node->valuestring;
    }
    const SVFType* gepPointeeType = nullptr;
    if (!ap_gep_pointee_type_name.empty())
    {
        gepPointeeType = pag->getSVFType(ap_gep_pointee_type_name);
    }
    AccessPath* ap = nullptr;
    if (nullptr != gepPointeeType)
    {
        ap = new AccessPath(fldIdx, gepPointeeType);
    }
    else
    {
        ap = new AccessPath(fldIdx);
        if (!ap_gep_pointee_type_name.empty())
            SVFUtil::outs() << "Warning: [updateGepValVarAttributes] No matching SVFType found for ap_gep_pointee_type_name: " << ap_gep_pointee_type_name << " when updating GepValVar:"<<var->getId()<< "\n";
    }

    cJSON* ap_idx_operand_pairs_node = cJSON_GetObjectItem(properties, "ap_idx_operand_pairs");
    std::string ap_idx_operand_pairs = "";
    if (nullptr != ap_idx_operand_pairs_node && nullptr != ap_idx_operand_pairs_node->valuestring)
    {
        ap_idx_operand_pairs = ap_idx_operand_pairs_node->valuestring;   
    }
    parseAPIdxOperandPairsString(ap_idx_operand_pairs, pag, ap);
    var->setAccessPath(ap);
}

void GraphDBClient::parseAPIdxOperandPairsString(const std::string& ap_idx_operand_pairs, SVFIR* pag, AccessPath* ap)
{
    if (!ap_idx_operand_pairs.empty())
    {
        std::vector<std::pair<int, std::string>> pairVec = parseIdxOperandPairsString(ap_idx_operand_pairs);
        for(auto& pair : pairVec)
        {
            int varId = pair.first;
            std::string typeName = pair.second;
            const SVFType* type;
            if (typeName != "NULL")
            {
                type = pag->getSVFType(typeName);
                if (nullptr == type)
                {
                    SVFUtil::outs() << "Warning: [parseAPIdxOperandPairsString] No matching SVFType found for type: " << typeName << " when parsing IdxOperandPair\n";
                }
            }
            else 
            {
                type = nullptr;
            }
            const SVFVar* var = pag->getGNode(varId);
            if (nullptr != var)
            {
                std::pair<const SVFVar*, const SVFType*> pair = std::make_pair(var, type);
                ap->addIdxOperandPair(pair);
            }
            else
            {
                SVFUtil::outs() << "Warning: [parseAPIdxOperandPairsString] No matching ValVar found for id: " << varId <<" when parsing IdxOperandPair \n";
            }
        }
    }
}

void GraphDBClient::updateSVFPAGNodesAttributesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, SVFIR* pag)
{
    std::string result;
    std::string queryStatement = " MATCH (node:"+nodeType+") RETURN node";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* node;
        cJSON_ArrayForEach(node, root)
        {
            cJSON* data = cJSON_GetObjectItem(node, "node");
            if (!data)
                continue;
            cJSON* properties = cJSON_GetObjectItem(data, "properties");
            if (!properties)
                continue;
            int id = cJSON_GetObjectItem(properties,"id")->valueint;
            if (nodeType == "ConstNullPtrValVar")
            {
                ConstNullPtrValVar* var = SVFUtil::dyn_cast<ConstNullPtrValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstNullPtrValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstIntValVar")
            {
                ConstIntValVar* var = SVFUtil::dyn_cast<ConstIntValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstIntValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstFPValVar")
            {
                ConstFPValVar* var = SVFUtil::dyn_cast<ConstFPValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstFPValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ArgValVar")
            {
                ArgValVar* var = SVFUtil::dyn_cast<ArgValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ArgValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
                int cg_node_id = cJSON_GetObjectItem(properties, "cg_node_id")->valueint;
                FunObjVar* cgNode = id2funObjVarsMap[cg_node_id];
                if (nullptr != cgNode)
                {
                    var->addCGNode(cgNode);
                }
                else
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunObjVar found for id: " << cg_node_id <<" when updating ArgValVar:"<< id << "\n";
                }
            }
            else if (nodeType == "BlackHoleValVar")
            {
                BlackHoleValVar* var = SVFUtil::dyn_cast<BlackHoleValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching BlackHoleValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstDataValVar")
            {
                ConstDataValVar* var = SVFUtil::dyn_cast<ConstDataValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstDataValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "RetValPN")
            {
                RetValPN* var = SVFUtil::dyn_cast<RetValPN>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching RetValPN found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
                int call_graph_node_id = cJSON_GetObjectItem(properties, "call_graph_node_id")->valueint;
                FunObjVar* callGraphNode = id2funObjVarsMap[call_graph_node_id];
                if (nullptr != callGraphNode)
                {
                    var->setCallGraphNode(callGraphNode);
                }
                else
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunObjVar found for id: " << call_graph_node_id <<" when updating RetValPN:"<<id<< "\n";
                }
            }
            else if (nodeType == "VarArgValPN")
            {
                VarArgValPN* var = SVFUtil::dyn_cast<VarArgValPN>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching VarArgValPN found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
                int call_graph_node_id = cJSON_GetObjectItem(properties, "call_graph_node_id")->valueint;
                FunObjVar* callGraphNode = id2funObjVarsMap[call_graph_node_id];
                if (nullptr != callGraphNode)
                {
                    var->setCallGraphNode(callGraphNode);
                }
                else
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunObjVar found for id: " << call_graph_node_id <<" when updating VarArgValPN:"<<id<< "\n";
                }
            }
            else if (nodeType == "DummyValVar")
            {
                DummyValVar* var = SVFUtil::dyn_cast<DummyValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching DummyValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstAggValVar")
            {
                ConstAggValVar* var = SVFUtil::dyn_cast<ConstAggValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstAggValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "GlobalValVar")
            {
                GlobalValVar* var = SVFUtil::dyn_cast<GlobalValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching GlobalValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "FunValVar")
            {
                FunValVar* var = SVFUtil::dyn_cast<FunValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
                int fun_obj_var_node_id = cJSON_GetObjectItem(properties, "fun_obj_var_node_id")->valueint;
                FunObjVar* funObjVar = id2funObjVarsMap[fun_obj_var_node_id];
                if (nullptr != funObjVar)
                {
                    var->setFunction(funObjVar);
                }
                else
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunObjVar found for id: " << fun_obj_var_node_id <<" when updating FunValVar:"<<id<< "\n";
                }
            }
            else if (nodeType == "GepValVar")
            {
                GepValVar* var = SVFUtil::dyn_cast<GepValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching GepValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
                updateGepValVarAttributes(properties, var, pag);

            }
            else if (nodeType == "ValVar")
            {
                ValVar* var = SVFUtil::dyn_cast<ValVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ValVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFValVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstNullPtrObjVar")
            {
                ConstNullPtrObjVar* var = SVFUtil::dyn_cast<ConstNullPtrObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstNullPtrObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstIntObjVar")
            {
                ConstIntObjVar* var = SVFUtil::dyn_cast<ConstIntObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstIntObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstFPObjVar")
            {
                ConstFPObjVar* var = SVFUtil::dyn_cast<ConstFPObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstFPObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstDataObjVar")
            {
                ConstDataObjVar* var = SVFUtil::dyn_cast<ConstDataObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstDataObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "DummyObjVar")
            {
                DummyObjVar* var = SVFUtil::dyn_cast<DummyObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching DummyObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "ConstAggObjVar")
            {
                ConstAggObjVar* var = SVFUtil::dyn_cast<ConstAggObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching ConstAggObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "GlobalObjVar")
            {
                GlobalObjVar* var = SVFUtil::dyn_cast<GlobalObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching GlobalObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "FunObjVar")
            {
                FunObjVar* var = SVFUtil::dyn_cast<FunObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching FunObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
                updateFunObjVarAttributes(properties, var, pag);
            }
            else if (nodeType == "StackObjVar")
            {
                StackObjVar* var = SVFUtil::dyn_cast<StackObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching StackObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "HeapObjVar")
            {
                HeapObjVar* var = SVFUtil::dyn_cast<HeapObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching HeapObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
            else if (nodeType == "BaseObjVar")
            {
                BaseObjVar* var = SVFUtil::dyn_cast<BaseObjVar>(pag->getGNode(id));
                if (var == nullptr)
                {
                    SVFUtil::outs() << "Warning: [updateSVFPAGNodesAttributesFromDB] No matching BaseObjVar found for id: " << id << "\n";
                    continue;
                }
                updateSVFBaseObjVarAtrributes(properties, var, pag);
            }
        }
        cJSON_Delete(root);
    }
}


void GraphDBClient::readPAGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, SVFIR* pag)
{
    std::string result;
    std::string queryStatement = " MATCH (node:"+nodeType+") RETURN node";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* node;
        cJSON_ArrayForEach(node, root)
        {
            cJSON* data = cJSON_GetObjectItem(node, "node");
            if (!data)
                continue;
            cJSON* properties = cJSON_GetObjectItem(data, "properties");
            if (!properties)
                continue;
            int id = cJSON_GetObjectItem(properties,"id")->valueint;
            std::string svfTypeName = cJSON_GetObjectItem(properties, "svf_type_name")->valuestring;
            const SVFType* type = pag->getSVFType(svfTypeName);
            if (type == nullptr)
            {
                SVFUtil::outs() << "Warning: [readPAGNodesFromDB] No matching SVFType found for type: " << svfTypeName << "for PAGNode:"<<id<<"\n";
            }

            if (nodeType == "ConstNullPtrValVar")
            {
                ConstNullPtrValVar* var = new ConstNullPtrValVar(id, type, ValVar::ConstNullptrValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ConstIntValVar")
            {
                u64_t zval = std::stoull(cJSON_GetObjectItem(properties, "zval")->valuestring);
                s64_t sval = cJSON_GetObjectItem(properties, "sval")->valueint;
                ConstIntValVar* var = new ConstIntValVar(id, sval, zval, type, ValVar::ConstIntValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ConstFPValVar")
            {
                double dval = cJSON_GetObjectItem(properties, "dval")->valuedouble;
                ConstFPValVar* var = new ConstFPValVar(id, dval, type, ValVar::ConstFPValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ArgValVar")
            {
                u32_t arg_no = static_cast<u32_t>(cJSON_GetObjectItem(properties, "arg_no")->valueint);
                ArgValVar* var = new ArgValVar(id, type,arg_no, ValVar::ArgValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "BlackHoleValVar")
            {
                BlackHoleValVar* var = new BlackHoleValVar(id, type, ValVar::BlackHoleValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ConstDataValVar")
            {
                ConstDataValVar* var = new ConstDataValVar(id, type, ValVar::ConstDataValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "RetValPN")
            {
                RetValPN* var = new RetValPN(id, type, ValVar::RetValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "VarArgValPN")
            {
                VarArgValPN* var = new VarArgValPN(id, type, ValVar::VarargValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "DummyValVar")
            {
                DummyValVar* var = new DummyValVar(id, type, ValVar::DummyValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ConstAggValVar")
            {
                ConstAggValVar* var = new ConstAggValVar(id, type, ValVar::ConstAggValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "GlobalValVar")
            {
                GlobalValVar* var = new GlobalValVar(id, type, ValVar::GlobalValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "FunValVar")
            {
                FunValVar* var = new FunValVar(id, type, ValVar::FunValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "GepValVar")
            {
                std::string gep_val_svf_type_name = cJSON_GetObjectItem(properties, "gep_val_svf_type_name")->valuestring;
                const SVFType* gepValType = pag->getSVFType(gep_val_svf_type_name);
                GepValVar* var = new GepValVar(id, type, gepValType, ValVar::GepValNode);
                pag->addInitValNode(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ValVar")
            {
                ValVar* var = new ValVar(id, type, ValVar::ValNode);
                pag->addValNodeFromDB(var);
                NodeIDAllocator::get()->increaseNumOfValues();
            }
            else if (nodeType == "ConstNullPtrObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                ConstNullPtrObjVar* var = new ConstNullPtrObjVar(id, type, objTypeInfo, ObjVar::ConstNullptrObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "ConstIntObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                u64_t zval = std::stoull(cJSON_GetObjectItem(properties, "zval")->valuestring);
                s64_t sval = cJSON_GetObjectItem(properties, "sval")->valueint;
                ConstIntObjVar* var = new ConstIntObjVar(id, sval, zval, type, objTypeInfo, ObjVar::ConstIntObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "ConstFPObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                float dval = (float)(cJSON_GetObjectItem(properties, "dval")->valuedouble);
                ConstFPObjVar* var = new ConstFPObjVar(id, dval, type, objTypeInfo, ObjVar::ConstFPObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "ConstDataObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                ConstDataObjVar* var = new ConstDataObjVar(id, type, objTypeInfo, ObjVar::ConstDataObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "DummyObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                DummyObjVar* var = new DummyObjVar(id, type, objTypeInfo, ObjVar::DummyObjNode);
                pag->addDummyObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "ConstAggObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                ConstAggObjVar* var = new ConstAggObjVar(id, type, objTypeInfo, ObjVar::ConstAggObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "GlobalObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                GlobalObjVar* var = new GlobalObjVar(id, type, objTypeInfo, ObjVar::GlobalObjNode);
                pag->addBaseObjNode(var); 
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "FunObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                bool is_decl = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_decl"));
                bool intrinsic = cJSON_IsTrue(cJSON_GetObjectItem(properties, "intrinsic"));
                bool is_addr_taken = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_addr_taken"));
                bool is_uncalled = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_uncalled"));
                bool is_not_return = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_not_return"));
                bool sup_var_arg = cJSON_IsTrue(cJSON_GetObjectItem(properties, "sup_var_arg"));
                std::string fun_type_name = cJSON_GetObjectItem(properties, "fun_type_name")->valuestring;
                const SVFFunctionType* funcType = SVFUtil::dyn_cast<SVFFunctionType>(pag->getSVFType(fun_type_name));
                FunObjVar* var = new FunObjVar(id, type, objTypeInfo, is_decl, intrinsic, is_addr_taken, is_uncalled, is_not_return, sup_var_arg, funcType, ObjVar::FunObjNode);
                std::string all_args_node_ids = cJSON_GetObjectItem(properties, "all_args_node_ids")->valuestring;
                if (!all_args_node_ids.empty())
                {
                    std::vector<int> all_args_node_ids_vec = parseElements2Container<std::vector<int>>(all_args_node_ids);
                    for (int arg_id : all_args_node_ids_vec)
                    {
                        ArgValVar* arg = SVFUtil::dyn_cast<ArgValVar>(pag->getGNode(arg_id));
                        if (arg != nullptr)
                        {
                            var->addArgument(arg);
                        } 
                        else 
                        {
                            SVFUtil::outs() << "Warning: [readPAGNodesFromDB] No matching ArgValVar found for id: " << arg_id << "\n";
                        }
                    }
                }
                pag->addBaseObjNode(var);
                id2funObjVarsMap[id] = var;  
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();              
            }
            else if (nodeType == "StackObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                StackObjVar* var = new StackObjVar(id, type, objTypeInfo, ObjVar::StackObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "HeapObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                HeapObjVar* var = new HeapObjVar(id, type, objTypeInfo, ObjVar::HeapObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "BaseObjVar")
            {
                ObjTypeInfo* objTypeInfo = parseObjTypeInfoFromDB(properties, pag);
                BaseObjVar* var = new BaseObjVar(id, type, objTypeInfo, ObjVar::BaseObjNode);
                pag->addBaseObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "GepObjVar")
            {
                s64_t app_offset = cJSON_GetObjectItem(properties, "app_offset")->valueint;
                int base_obj_var_node_id = cJSON_GetObjectItem(properties, "base_obj_var_node_id")->valueint;
                const BaseObjVar* baseObj = pag->getBaseObject(base_obj_var_node_id);
                GepObjVar* var = new GepObjVar(id, type, app_offset, baseObj, ObjVar::GepObjNode);
                pag->addGepObjNode(var);
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
            else if (nodeType == "ObjVar")
            {
                ObjVar* var = new ObjVar(id, type, ObjVar::ObjNode);
                pag->addObjNodeFromDB(SVFUtil::cast<ObjVar>(var));
                NodeIDAllocator::get()->increaseNumOfObjAndNodes();
            }
        }
        cJSON_Delete(root);
    }
}

ObjTypeInfo* GraphDBClient::parseObjTypeInfoFromDB(cJSON* properties, SVFIR* pag)
{
    std::string obj_type_info_type_name = cJSON_GetObjectItem(properties, "obj_type_info_type_name")->valuestring;
    const SVFType* objTypeInfoType = pag->getSVFType(obj_type_info_type_name);
    int obj_type_info_flags = cJSON_GetObjectItem(properties, "obj_type_info_flags")->valueint;
    int obj_type_info_max_offset_limit = cJSON_GetObjectItem(properties, "obj_type_info_max_offset_limit")->valueint;
    int obj_type_info_elem_num = cJSON_GetObjectItem(properties, "obj_type_info_elem_num")->valueint;
    int obj_type_info_byte_size = cJSON_GetObjectItem(properties, "obj_type_info_byte_size")->valueint;
    ObjTypeInfo* objTypeInfo = new ObjTypeInfo(objTypeInfoType, obj_type_info_flags, obj_type_info_max_offset_limit, obj_type_info_elem_num, obj_type_info_byte_size);
    if (nullptr != objTypeInfo)
        return objTypeInfo;
    return nullptr;
}

cJSON* GraphDBClient::queryFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string queryStatement)
{
    // parse all SVFType
    std::string result;
    if (!connection->CallCypher(result, queryStatement, dbname))
    {
        SVFUtil::outs() << queryStatement<< "\n";
        SVFUtil::outs() << "Failed to query from DB:" << result << "\n";
        return nullptr;
    } 
    cJSON* root = cJSON_Parse(result.c_str());
    if (!root || !cJSON_IsArray(root))
    {
        SVFUtil::outs() << "Invalid JSON format: "<<queryStatement<<"\n";
        cJSON_Delete(root);
        return nullptr;
    }
    // TODO: need to fix: all graph should support pagination query not only the PAG
    if (dbname == "PAG" && result=="[]")
    {
        // SVFUtil::outs() << "No data found for query: " << queryStatement << "\n";
        cJSON_Delete(root);
        return nullptr;
    }

    return root;
}

void GraphDBClient::readBasicBlockGraphFromDB(lgraph::RpcClient* connection, const std::string& dbname)
{
    SVFUtil::outs()<< "Build BasicBlockGraph from DB....\n";
    for (auto& item : id2funObjVarsMap)
    {
        FunObjVar* funObjVar = item.second;
        readBasicBlockNodesFromDB(connection, dbname, funObjVar);
    }

    for (auto& item : id2funObjVarsMap)
    {
        FunObjVar* funObjVar = item.second;
        readBasicBlockEdgesFromDB(connection, dbname, funObjVar);
    }
}

void GraphDBClient::readBasicBlockNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, FunObjVar* funObjVar)
{
    NodeID id = funObjVar->getId();
    std::string queryStatement ="MATCH (node) where node.fun_obj_var_id = " + std::to_string(id) +" RETURN node";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* node;
        BasicBlockGraph* bbGraph = new BasicBlockGraph();
        funObjVar->setBasicBlockGraph(bbGraph);
        cJSON_ArrayForEach(node, root)
        {
            cJSON* data = cJSON_GetObjectItem(node, "node");
            if (!data)
                continue;
            cJSON* properties = cJSON_GetObjectItem(data, "properties");
            if (!properties)
                continue;
            std::string id = cJSON_GetObjectItem(properties, "id")->valuestring;
            std::string bb_name =
                cJSON_GetObjectItem(properties, "bb_name")->valuestring;
            int bbId = parseBBId(id);
            SVFBasicBlock* bb = new SVFBasicBlock(bbId, funObjVar);
            bb->setName(bb_name);
            bbGraph->addBasicBlock(bb);
            basicBlocks.insert(bb);
        }
        cJSON_Delete(root);
    }
}

void GraphDBClient::readBasicBlockEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, FunObjVar* funObjVar)
{
    BasicBlockGraph* bbGraph = funObjVar->getBasicBlockGraph();
    if (nullptr != bbGraph)
    {
        for (auto& pair: *bbGraph)
        {
            SVFBasicBlock* bb = pair.second;
            std::string queryStatement = "MATCH (node{id:'"+std::to_string(bb->getId())+":"+std::to_string(bb->getFunction()->getId())+"'}) RETURN node.pred_bb_ids, node.sscc_bb_ids";
            cJSON* root = queryFromDB(connection, dbname, queryStatement);
            if (nullptr != root)
            {
                cJSON* item;
                cJSON_ArrayForEach(item, root)
                {
                    if (!item)
                        continue;
                    std::string pred_bb_ids = cJSON_GetObjectItem(item, "node.pred_bb_ids")->valuestring;
                    std::string sscc_bb_ids = cJSON_GetObjectItem(item, "node.sscc_bb_ids")->valuestring;
                    if (!pred_bb_ids.empty())
                    {
                        std::vector<int> predBBIds = parseElements2Container<std::vector<int>>(pred_bb_ids);
                        for (int predBBId : predBBIds)
                        {

                            SVFBasicBlock* predBB = bbGraph->getGNode(predBBId);
                            if (nullptr != predBB)
                            {
                                bb->addPredBasicBlock(predBB);
                            }

                        }
                    }
                    if (!sscc_bb_ids.empty())
                    {
                        std::vector<int> ssccBBIds = parseElements2Container<std::vector<int>>(sscc_bb_ids);
                        for (int ssccBBId : ssccBBIds)
                        {
                            SVFBasicBlock* ssccBB = bbGraph->getGNode(ssccBBId);
                            if (nullptr != ssccBB)
                            {
                                bb->addSuccBasicBlock(ssccBB);
                            }

                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
    }
}

ICFG* GraphDBClient::buildICFGFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Build ICFG from DB....\n";
    DBOUT(DGENERAL, outs() << pasMsg("\t Building ICFG From DB ...\n"));
    ICFG* icfg = new ICFG();
    // read & add all the ICFG nodes from DB
    readICFGNodesFromDB(connection, dbname, "GlobalICFGNode", icfg, pag);
    readICFGNodesFromDB(connection, dbname, "FunEntryICFGNode", icfg, pag);
    readICFGNodesFromDB(connection, dbname, "FunExitICFGNode", icfg, pag);
    readICFGNodesFromDB(connection, dbname, "IntraICFGNode", icfg, pag);
    // need to parse the RetICFGNode first before parsing the CallICFGNode
    readICFGNodesFromDB(connection, dbname, "RetICFGNode", icfg, pag);
    readICFGNodesFromDB(connection, dbname, "CallICFGNode", icfg, pag);

    // read & add all the ICFG edges from DB
    readICFGEdgesFromDB(connection, dbname, "IntraCFGEdge", icfg, pag);
    readICFGEdgesFromDB(connection, dbname, "CallCFGEdge", icfg, pag);
    readICFGEdgesFromDB(connection, dbname, "RetCFGEdge", icfg, pag);

    return icfg;
}

void GraphDBClient::readICFGNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string nodeType, ICFG* icfg, SVFIR* pag)
{
    std::string queryStatement = " MATCH (node:"+nodeType+") RETURN node";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* node;
        cJSON_ArrayForEach(node, root)
        {
            ICFGNode* icfgNode = nullptr;
            if (nodeType == "GlobalICFGNode")
            {
                icfgNode = parseGlobalICFGNodeFromDBResult(node);
                if (nullptr != icfgNode)
                {
                    icfg->addGlobalICFGNode(SVFUtil::cast<GlobalICFGNode>(icfgNode));
                }
            }
            else if (nodeType == "IntraICFGNode")
            {
                icfgNode = parseIntraICFGNodeFromDBResult(node, pag);
                if (nullptr != icfgNode)
                {
                    icfg->addIntraICFGNode(SVFUtil::cast<IntraICFGNode>(icfgNode));
                }
            }
            else if (nodeType == "FunEntryICFGNode")
            {
                icfgNode = parseFunEntryICFGNodeFromDBResult(node, pag);
                if (nullptr != icfgNode)
                {
                    icfg->addFunEntryICFGNode(SVFUtil::cast<FunEntryICFGNode>(icfgNode));
                }
            }
            else if (nodeType == "FunExitICFGNode")
            {
                icfgNode = parseFunExitICFGNodeFromDBResult(node, pag);
                if (nullptr != icfgNode)
                {
                    icfg->addFunExitICFGNode(SVFUtil::cast<FunExitICFGNode>(icfgNode));
                }
            }
            else if (nodeType == "RetICFGNode")
            {
                icfgNode = parseRetICFGNodeFromDBResult(node, pag);
                if (nullptr != icfgNode)
                {
                    icfg->addRetICFGNode(SVFUtil::cast<RetICFGNode>(icfgNode));
                    id2RetICFGNodeMap[icfgNode->getId()] = SVFUtil::cast<RetICFGNode>(icfgNode);
                }
            }
            else if (nodeType == "CallICFGNode")
            {
                icfgNode = parseCallICFGNodeFromDBResult(node, pag);
                if (nullptr != icfgNode)
                {
                    icfg->addCallICFGNode(SVFUtil::cast<CallICFGNode>(icfgNode));
                }
            }
            
            if (nullptr == icfgNode)
            {
                SVFUtil::outs()<< "Failed to create "<< nodeType<< " from db query result\n";
            }
        }
        cJSON_Delete(root);
    }
}

ICFGNode* GraphDBClient::parseGlobalICFGNodeFromDBResult(const cJSON* node)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    GlobalICFGNode* icfgNode;
    int id = cJSON_GetObjectItem(properties,"id")->valueint;

    icfgNode = new GlobalICFGNode(id);
    return icfgNode;
}

ICFGNode* GraphDBClient::parseFunEntryICFGNodeFromDBResult(const cJSON* node, SVFIR* pag)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    FunEntryICFGNode* icfgNode;
    int id = cJSON_GetObjectItem(properties,"id")->valueint;
    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint; 
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseFunEntryICFGNodeFromDBResult] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
    }

    // parse FunEntryICFGNode bb
    int bb_id = cJSON_GetObjectItem(properties, "bb_id")->valueint;
    SVFBasicBlock* bb = funObjVar->getBasicBlockGraph()->getGNode(bb_id);

    icfgNode = new FunEntryICFGNode(id, funObjVar, bb);
    std::string fpNodesStr = cJSON_GetObjectItem(properties, "fp_nodes")->valuestring;
    std::vector<u32_t> fpNodesIdVec = parseElements2Container<std::vector<u32_t>>(fpNodesStr);
    for (auto fpNodeId: fpNodesIdVec)
    {
        SVFVar* fpNode = pag->getGNode(fpNodeId);
        if (nullptr != fpNode)
        {
            pag->addFunArgs(SVFUtil::cast<FunEntryICFGNode>(icfgNode), funObjVar, fpNode);
        }
        else 
        {
            SVFUtil::outs() << "Warning: [parseFunEntryICFGNodeFromDBResult] No matching fpNode SVFVar found for id: " << fpNodeId << "\n";
        }
    }

    if (nullptr != bb)
    {
        bb->addICFGNode(icfgNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseFunEntryICFGNodeFromDBResult] No matching BasicBlock found for id: " << bb_id << "\n";
    }


    return icfgNode;
}

ICFGNode* GraphDBClient::parseFunExitICFGNodeFromDBResult(const cJSON* node, SVFIR* pag)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    FunExitICFGNode* icfgNode;
    int id = cJSON_GetObjectItem(properties,"id")->valueint;

    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint;
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseFunExitICFGNodeFromDBResult] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
    }

    // parse FunExitICFGNode bb
    int bb_id = cJSON_GetObjectItem(properties, "bb_id")->valueint;
    SVFBasicBlock* bb = funObjVar->getBasicBlockGraph()->getGNode(bb_id);

    icfgNode = new FunExitICFGNode(id, funObjVar, bb);
    int formal_ret_node_id = cJSON_GetObjectItem(properties, "formal_ret_node_id")->valueint;
    if (formal_ret_node_id != -1)
    {
        SVFVar* formalRet = pag->getGNode(formal_ret_node_id);
        if (nullptr != formalRet)
        {
            pag->addFunRet(SVFUtil::cast<FunExitICFGNode>(icfgNode), funObjVar, formalRet);
        }
        else
        {
            SVFUtil::outs() << "Warning: [parseFunExitICFGNodeFromDBResult] No matching formalRet SVFVar found for id: " << formal_ret_node_id << "\n";
        }
    }

    if (nullptr != bb)
    {
        bb->addICFGNode(icfgNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseFunExitICFGNodeFromDBResult] No matching BasicBlock found for id: " << bb_id << "\n";
    }

    return icfgNode;
}

ICFGNode* GraphDBClient::parseIntraICFGNodeFromDBResult(const cJSON* node, SVFIR* pag)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    IntraICFGNode* icfgNode;
    int id = cJSON_GetObjectItem(properties, "id")->valueint;
    // parse intraICFGNode funObjVar
    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint;
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseIntraICFGNodeFromDBResult] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
    }

    // parse intraICFGNode bb
    int bb_id = cJSON_GetObjectItem(properties, "bb_id")->valueint;
    SVFBasicBlock* bb = funObjVar->getBasicBlockGraph()->getGNode(bb_id);

    // parse isRet 
    bool is_return = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_return"));

    
    icfgNode = new IntraICFGNode(id, bb, funObjVar, is_return);

    // add this ICFGNode to its BasicBlock
    if (nullptr != bb)
    {
        bb->addICFGNode(icfgNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseIntraICFGNodeFromDBResult] No matching BasicBlock found for id: " << bb_id << "\n";
    }
    return icfgNode;
}

ICFGNode* GraphDBClient::parseRetICFGNodeFromDBResult(const cJSON* node, SVFIR* pag)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;
    
    RetICFGNode* icfgNode;
    // parse retICFGNode id
    int id = cJSON_GetObjectItem(properties, "id")->valueint;

    // parse retICFGNode funObjVar
    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint;
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseRetICFGNodeFromDBResult] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
    }

    // parse retICFGNode bb
    int bb_id = cJSON_GetObjectItem(properties, "bb_id")->valueint;
    SVFBasicBlock* bb = funObjVar->getBasicBlockGraph()->getGNode(bb_id);

    // parse retICFGNode svfType
    std::string svfTypeName = cJSON_GetObjectItem(properties, "svf_type")->valuestring;
    const SVFType* type = pag->getSVFType(svfTypeName);
    if (nullptr == type)
    {
        SVFUtil::outs() << "Warning: [parseRetICFGNodeFromDBResult] No matching SVFType found for: " << svfTypeName << "\n";
    }

    // create RetICFGNode Instance 
    icfgNode = new RetICFGNode(id, type, bb, funObjVar);

    // parse & add actualRet for RetICFGNode
    int actual_ret_node_id = cJSON_GetObjectItem(properties, "actual_ret_node_id")->valueint;
    if (actual_ret_node_id != -1)
    {
        SVFVar* actualRet = pag->getGNode(actual_ret_node_id);
        if (nullptr != actualRet)
        {
            pag->addCallSiteRets(SVFUtil::cast<RetICFGNode>(icfgNode), actualRet);
        }
        else
        {
            SVFUtil::outs() << "Warning: [parseRetICFGNodeFromDBResult] No matching actualRet SVFVar found for id: " << actual_ret_node_id << "\n";
        }
    }

    // add this ICFGNode to its BasicBlock
    if (nullptr != bb)
    {
        bb->addICFGNode(icfgNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseRetICFGNodeFromDBResult] No matching BasicBlock found for id: " << bb_id << "\n";
    }
    return icfgNode;
}

ICFGNode* GraphDBClient::parseCallICFGNodeFromDBResult(const cJSON* node, SVFIR* pag)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;
    
    CallICFGNode* icfgNode;

    // parse CallICFGNode id
    int id = cJSON_GetObjectItem(properties, "id")->valueint;

    // parse CallICFGNode funObjVar
    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint;
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
    }

    // parse CallICFGNode bb
    int bb_id = cJSON_GetObjectItem(properties, "bb_id")->valueint;
    SVFBasicBlock* bb = funObjVar->getBasicBlockGraph()->getGNode(bb_id);

    // parse CallICFGNode svfType
    std::string svfTypeName = cJSON_GetObjectItem(properties, "svf_type")->valuestring;
    const SVFType* type = pag->getSVFType(svfTypeName);
    if (nullptr == type)
    {
        SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching SVFType found for: " << svfTypeName << "\n";
    }

    // parse CallICFGNode calledFunObjVar
    int called_fun_obj_var_id = cJSON_GetObjectItem(properties, "called_fun_obj_var_id")->valueint;
    FunObjVar* calledFunc = nullptr;
    if (called_fun_obj_var_id != -1)
    {
        auto calledFuncIt = id2funObjVarsMap.find(called_fun_obj_var_id);
        if (calledFuncIt != id2funObjVarsMap.end())
        {
            calledFunc = calledFuncIt->second;
        }
        else
        {
            SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching calledFunObjVar found for id: " << called_fun_obj_var_id << "\n";
        }
    }

    bool is_vararg = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_vararg"));
    bool is_vir_call_inst = cJSON_IsTrue(cJSON_GetObjectItem(properties, "is_vir_call_inst"));

    // parse CallICFGNode retICFGNode
    int ret_icfg_node_id = cJSON_GetObjectItem(properties, "ret_icfg_node_id")->valueint;
    RetICFGNode* retICFGNode = nullptr;
    if (ret_icfg_node_id != -1)
    {
        auto retICFGNodeIt = id2RetICFGNodeMap.find(ret_icfg_node_id);
        if (retICFGNodeIt != id2RetICFGNodeMap.end())
        {
            retICFGNode = retICFGNodeIt->second;
        }
        else
        {
            SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching RetICFGNode found for id: " << ret_icfg_node_id << "\n";
        }
    }
    
    std::string fun_name_of_v_call = "";
    s32_t virtualFunIdx = 0;
    SVFVar* vtabPtr = nullptr;
    if (is_vir_call_inst)
    {
        int virtual_fun_idx = cJSON_GetObjectItem(properties, "virtual_fun_idx")->valueint;
        virtualFunIdx = static_cast<s32_t>(virtual_fun_idx);
        int vtab_ptr_node_id = cJSON_GetObjectItem(properties, "vtab_ptr_node_id")->valueint;
        vtabPtr = pag->getGNode(vtab_ptr_node_id);
        fun_name_of_v_call = cJSON_GetObjectItem(properties, "fun_name_of_v_call")->valuestring;
    }
     
    // create CallICFGNode Instance
    icfgNode = new CallICFGNode(id, bb, type, funObjVar, calledFunc, retICFGNode,
        is_vararg, is_vir_call_inst, virtualFunIdx, vtabPtr, fun_name_of_v_call);
    
    // parse CallICFGNode APNodes
    std::string ap_nodes = cJSON_GetObjectItem(properties, "ap_nodes")->valuestring;
    if (!ap_nodes.empty() && ap_nodes!= "[]")
    {
        std::vector<u32_t> apNodesIdVec = parseElements2Container<std::vector<u32_t>>(ap_nodes);
        if (apNodesIdVec.size() > 0)
        {
            for (auto apNodeId: apNodesIdVec)
            {
                SVFVar* apNode = pag->getGNode(apNodeId);
                if (nullptr != apNode)
                {
                    pag->addCallSiteArgs(SVFUtil::cast<CallICFGNode>(icfgNode), SVFUtil::cast<ValVar>(apNode));
                }
                else
                {
                    SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching APNode ValVar found for id: " << apNodeId << "\n";
                }
            }
        }
    }

    if (retICFGNode != nullptr)
    {
        retICFGNode->addCallBlockNode(icfgNode);
    }

    // add this ICFGNode to its BasicBlock
    if (nullptr != bb)
    {
        bb->addICFGNode(icfgNode);
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseCallICFGNodeFromDBResult] No matching BasicBlock found for id: " << bb_id << "\n";
    }
    
    return icfgNode;
}

void GraphDBClient::readICFGEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, std::string edgeType, ICFG* icfg, SVFIR* pag)
{
    std::string queryStatement =  "MATCH ()-[edge:"+edgeType+"]->() RETURN edge";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* edge;
        cJSON_ArrayForEach(edge, root)
        {
            ICFGEdge* icfgEdge = nullptr;
            if (edgeType == "IntraCFGEdge")
            {
                icfgEdge = parseIntraCFGEdgeFromDBResult(edge, pag, icfg);
            }
            else if (edgeType == "CallCFGEdge")
            {
                icfgEdge = parseCallCFGEdgeFromDBResult(edge, pag, icfg);
            }
            else if (edgeType == "RetCFGEdge")
            {
                icfgEdge = parseRetCFGEdgeFromDBResult(edge, pag, icfg);
            }
            if (nullptr != icfgEdge)
            {
                icfg->addICFGEdge(icfgEdge);
            }
            else 
            {
                SVFUtil::outs()<< "Failed to create "<< edgeType << " from db query result\n";
            }
        }
        cJSON_Delete(root);
    }
}

ICFGEdge* GraphDBClient::parseIntraCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg)
{
    cJSON* data = cJSON_GetObjectItem(edge, "edge");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    IntraCFGEdge* icfgEdge;

    // parse srcICFGNode & dstICFGNode
    int src_id = cJSON_GetObjectItem(data,"src")->valueint;
    int dst_id = cJSON_GetObjectItem(data,"dst")->valueint; 
    ICFGNode* src = icfg->getICFGNode(src_id);
    ICFGNode* dst = icfg->getICFGNode(dst_id);

    if (src == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseIntraCFGEdgeFromDBResult] No matching src ICFGNode found for id: " << src_id << "\n";
        return nullptr;
    }
    if (dst == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseIntraCFGEdgeFromDBResult] No matching dst ICFGNode found for id: " << dst_id << "\n";
        return nullptr;
    }

    // create IntraCFGEdge Instance
    icfgEdge = new IntraCFGEdge(src, dst);
   
    // parse branchCondVal & conditionalVar
    int condition_var_id = cJSON_GetObjectItem(properties, "condition_var_id")->valueint;
    int branch_cond_val = cJSON_GetObjectItem(properties, "branch_cond_val")->valueint;
    s64_t branchCondVal = 0;
    SVFVar* conditionVar;
    if (condition_var_id != -1 && branch_cond_val != -1)
    {
        branchCondVal = static_cast<s64_t>(branch_cond_val);
        conditionVar = pag->getGNode(condition_var_id);
        if (nullptr == conditionVar)
        {
            SVFUtil::outs() << "Warning: [parseIntraCFGEdgeFromDBResult] No matching conditionVar found for id: " << condition_var_id << "\n";
        }
        icfgEdge->setConditionVar(conditionVar);
        icfgEdge->setBranchCondVal(branchCondVal);
    }

    return icfgEdge;
}

ICFGEdge* GraphDBClient::parseCallCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg)
{
    cJSON* data = cJSON_GetObjectItem(edge, "edge");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    CallCFGEdge* icfgEdge;
    // parse srcICFGNode & dstICFGNode
    int src_id = cJSON_GetObjectItem(data,"src")->valueint;
    int dst_id = cJSON_GetObjectItem(data,"dst")->valueint; 
    ICFGNode* src = icfg->getICFGNode(src_id);
    ICFGNode* dst = icfg->getICFGNode(dst_id);
    if (src == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseCallCFGEdgeFromDBResult] No matching src ICFGNode found for id: " << src_id << "\n";
        return nullptr;
    }
    if (dst == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseCallCFGEdgeFromDBResult] No matching dst ICFGNode found for id: " << dst_id << "\n";
        return nullptr;
    }

    // create CallCFGEdge Instance
    icfgEdge = new CallCFGEdge(src, dst);
    std::string call_pe_ids = cJSON_GetObjectItem(properties, "call_pe_ids")->valuestring;
    if (!call_pe_ids.empty())
    {
        callCFGEdge2CallPEStrMap[icfgEdge] = call_pe_ids;
    }
    return icfgEdge;
}

ICFGEdge* GraphDBClient::parseRetCFGEdgeFromDBResult(const cJSON* edge, SVFIR* pag, ICFG* icfg)
{
    cJSON* data = cJSON_GetObjectItem(edge, "edge");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;
    
    RetCFGEdge* icfgEdge;
    // parse srcICFGNode & dstICFGNode
    int src_id = cJSON_GetObjectItem(data,"src")->valueint;
    int dst_id = cJSON_GetObjectItem(data,"dst")->valueint; 
    ICFGNode* src = icfg->getICFGNode(src_id);
    ICFGNode* dst = icfg->getICFGNode(dst_id);
    if (src == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseRetCFGEdgeFromDBResult] No matching src ICFGNode found for id: " << src_id << "\n";
        return nullptr;
    }
    if (dst == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseRetCFGEdgeFromDBResult] No matching dst ICFGNode found for id: " << dst_id << "\n";
        return nullptr;
    }

    // create RetCFGEdge Instance
    icfgEdge = new RetCFGEdge(src, dst);
    int ret_pe_id = cJSON_GetObjectItem(properties, "ret_pe_id")->valueint;
    if (ret_pe_id != -1)
    {
        retCFGEdge2RetPEStrMap[icfgEdge] = ret_pe_id;
    }
    return icfgEdge;
}

CallGraph* GraphDBClient::buildCallGraphFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag)
{
    SVFUtil::outs()<< "Build CallGraph from DB....\n";
    DBOUT(DGENERAL, outs() << pasMsg("\t Building CallGraph From DB ...\n"));
    CallGraph* callGraph = new CallGraph();
    readCallGraphNodesFromDB(connection, dbname, callGraph);
    readCallGraphEdgesFromDB(connection, dbname, pag, callGraph);

    return callGraph;
}

void GraphDBClient::readCallGraphNodesFromDB(lgraph::RpcClient* connection, const std::string& dbname, CallGraph* callGraph)
{
    std::string queryStatement = " MATCH (node:CallGraphNode) RETURN node";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* node;
        cJSON_ArrayForEach(node, root)
        {
            CallGraphNode* cgNode = nullptr;
            cgNode = parseCallGraphNodeFromDB(node);
            if (nullptr != cgNode)
            {
                callGraph->addCallGraphNodeFromDB(cgNode);
            }
        }
        cJSON_Delete(root);
    }
}

void GraphDBClient::readCallGraphEdgesFromDB(lgraph::RpcClient* connection, const std::string& dbname, SVFIR* pag, CallGraph* callGraph)
{
    std::string queryStatement = "MATCH ()-[edge]->() RETURN edge";
    cJSON* root = queryFromDB(connection, dbname, queryStatement);
    if (nullptr != root)
    {
        cJSON* edge;
        cJSON_ArrayForEach(edge, root)
        {
            CallGraphEdge* cgEdge = nullptr;
            cgEdge = parseCallGraphEdgeFromDB(edge, pag, callGraph);
            if (nullptr != cgEdge)
            {
                if (cgEdge->isDirectCallEdge())
                {
                    callGraph->addDirectCallGraphEdge(cgEdge);
                }
                if (cgEdge->isIndirectCallEdge())
                {
                    callGraph->addIndirectCallGraphEdge(cgEdge);
                }
            }
        }
        cJSON_Delete(root);
    }
}

CallGraphNode* GraphDBClient::parseCallGraphNodeFromDB(const cJSON* node)
{
    cJSON* data = cJSON_GetObjectItem(node, "node");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;
    
    int id = cJSON_GetObjectItem(properties,"id")->valueint;

    // parse funObjVar 
    int fun_obj_var_id = cJSON_GetObjectItem(properties, "fun_obj_var_id")->valueint;
    FunObjVar* funObjVar = nullptr;
    auto funObjVarIt = id2funObjVarsMap.find(fun_obj_var_id);
    if (funObjVarIt != id2funObjVarsMap.end())
    {
        funObjVar = funObjVarIt->second;
    }
    else
    {
        SVFUtil::outs() << "Warning: [parseCallGraphNodeFromDB] No matching FunObjVar found for id: " << fun_obj_var_id << "\n";
        return nullptr;
    }
    CallGraphNode* cgNode;

    // create callGraph node instance 
    cgNode = new CallGraphNode(id, funObjVar);

    return cgNode;
}

CallGraphEdge* GraphDBClient::parseCallGraphEdgeFromDB(const cJSON* edge, SVFIR* pag, CallGraph* callGraph)
{
    CallGraphEdge* cgEdge = nullptr;
    cJSON* data = cJSON_GetObjectItem(edge, "edge");
    if (!data)
        return nullptr;

    cJSON* properties = cJSON_GetObjectItem(data, "properties");
    if (!properties)
        return nullptr;

    int src_id = cJSON_GetObjectItem(data,"src")->valueint;
    int dst_id = cJSON_GetObjectItem(data,"dst")->valueint;
    int csid = cJSON_GetObjectItem(properties,"csid")->valueint;
    std::string direct_call_set = cJSON_GetObjectItem(properties,"direct_call_set")->valuestring;
    std::string indirect_call_set = cJSON_GetObjectItem(properties, "indirect_call_set")->valuestring;

    int kind = cJSON_GetObjectItem(properties, "kind")->valueint;

    CallGraphNode* srcNode = callGraph->getGNode(src_id);
    CallGraphNode* dstNode = callGraph->getGNode(dst_id);
    if (srcNode == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseCallGraphEdgeFromDB] No matching src CallGraphNode found for id: " << src_id << "\n";
        return nullptr;
    }
    if (dstNode == nullptr)
    {
        SVFUtil::outs() << "Warning: [parseCallGraphEdgeFromDB] No matching dst CallGraphNode found for id: " << dst_id << "\n";
        return nullptr;
    }

    // create CallGraphEdge Instance 
    cgEdge = new CallGraphEdge(srcNode, dstNode, static_cast<CallGraphEdge::CEDGEK>(kind), csid);
    Set<int> direct_call_set_ids;
    if (!direct_call_set.empty())
    {
        direct_call_set_ids = parseElements2Container<Set<int>>(direct_call_set);
        for (int directCallId : direct_call_set_ids)
        {
            CallICFGNode* node = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(directCallId));
            callGraph->addCallSite(node, node->getFun(), cgEdge->getCallSiteID());
            cgEdge->addDirectCallSite(node);
            pag->addCallSite(node);
            callGraph->callinstToCallGraphEdgesMap[node].insert(cgEdge);
        }
    }

    Set<int> indirect_call_set_ids;
    if (!indirect_call_set.empty())
    {
        indirect_call_set_ids = parseElements2Container<Set<int>>(indirect_call_set);
        for (int indirectCallId : indirect_call_set_ids)
        {
            CallICFGNode* node = SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getGNode(indirectCallId));
            callGraph->numOfResolvedIndCallEdge++;
            callGraph->addCallSite(node, node->getFun(), cgEdge->getCallSiteID());
            cgEdge->addInDirectCallSite(node);
            pag->addCallSite(node);
            callGraph->callinstToCallGraphEdgesMap[node].insert(cgEdge);
        }
    }

    return cgEdge;
}