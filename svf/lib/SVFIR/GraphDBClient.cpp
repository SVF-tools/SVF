#include "SVFIR/GraphDBClient.h"
#include "SVFIR/SVFVariables.h"

using namespace SVF;

bool GraphDBClient::loadSchema(lgraph::RpcClient* connection,
                               const std::string& filepath,
                               const std::string& dbname)
{
    if (nullptr != connection)
    {
        SVFUtil::outs() << "load schema from file:" << filepath << "\n";
        std::string result;
        bool ret = connection->ImportSchemaFromFile(result, filepath, dbname);
        if (ret)
        {
            SVFUtil::outs() << dbname<< "schema load successfully:" << result << "\n";
        }
        else
        {
            SVFUtil::outs() << dbname<< "schema load failed:" << result << "\n";
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
        if (ret)
        {
            SVFUtil::outs()
                << "Create Graph callGraph successfully:" << result << "\n";
        }
        else
        {
            SVFUtil::outs()
                << "Failed to create Graph callGraph:" << result << "\n";
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
        if (ret)
        {
            SVFUtil::outs() << "ICFG edge added: " << result << "\n";
        }
        else
        {
            SVFUtil::outs() << "Failed to add ICFG edge to db " << dbname << " "
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
        if (ret)
        {
            SVFUtil::outs()<< "ICFG node added: " << result << "\n";
        }
        else
        {
            SVFUtil::outs() << "Failed to add icfg node to db " << dbname << " "
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
        std::string is_reachable_str = node->isReachableFromProgEntry() ? "true" : "false";
        const std::string queryStatement ="CREATE (n:CallGraphNode {id: " + std::to_string(node->getId()) +
                                 ", fun_obj_var_id: " + std::to_string(node->getFunction()->getId()) +
                                 ", is_reachable_from_prog_entry: " + is_reachable_str + "})";
        // SVFUtil::outs()<<"CallGraph Node Insert Query:"<<queryStatement<<"\n";
        std::string result;
        bool ret = connection->CallCypher(result, queryStatement, dbname);
        if (ret)
        {
            SVFUtil::outs()<< "CallGraph node added: " << result << "\n";
        }
        else
        {
            SVFUtil::outs() << "Failed to add callGraph node to db " << dbname << " "
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
        if (ret)
        {
            SVFUtil::outs() << "CallGraph edge added: " << result << "\n";
        }
        else
        {
            SVFUtil::outs() << "Failed to add callgraph edge to db " << dbname << " "
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
    ", fp_nodes:'" + extractNodesIds(node->getFormalParms()) +"'})";
    return queryStatement;
}

std::string GraphDBClient::getFunExitICFGNodeInsertStmt(const FunExitICFGNode* node) {
    std::string formalRetId = "";
    if (node->getFormalRet() == nullptr)
    {
        formalRetId = "";
    } else {
        formalRetId = ",formal_ret_node_id:" + std::to_string(node->getFormalRet()->getId());
    }
    const std::string queryStatement ="CREATE (n:FunExitICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ", fun_obj_var_id:" + std::to_string(node->getFun()->getId()) + 
    formalRetId + "})";
    return queryStatement;
}

std::string GraphDBClient::getCallICFGNodeInsertStmt(const CallICFGNode* node) {
    std::string fun_name_of_v_call = "";
    std::string vtab_ptr_node_id = "";
    std::string virtual_fun_idx = "0";
    std::string is_vir_call_inst = node->isVirtualCall() ? "true" : "false";
    std::string virtualFunAppendix = "";
    if (node->isVirtualCall())
    {
        fun_name_of_v_call = ", fun_name_of_v_call: '"+node->getFunNameOfVirtualCall()+"'";
        vtab_ptr_node_id = ", vtab_ptr_node_id:" + std::to_string(node->getVtablePtr()->getId());
        virtual_fun_idx = ", virtual_fun_idx:" + std::to_string(node->getFunIdxInVtable());
        virtualFunAppendix = vtab_ptr_node_id+virtual_fun_idx+fun_name_of_v_call;
    }
    std::string called_fun_obj_var_id = "";
    if (node->getCalledFunction() != nullptr)
    {
        called_fun_obj_var_id = ", called_fun_obj_var_id:" + std::to_string(node->getCalledFunction()->getId());
    }
    const std::string queryStatement ="CREATE (n:CallICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    ", ret_icfg_node_id: " + std::to_string(node->getRetICFGNode()->getId()) +
    ", bb_id: " + std::to_string(node->getBB()->getId()) +
    ", svf_type:'" + node->getType()->toString() +
    "', ap_nodes:'" + extractNodesIds(node->getActualParms()) +"'"+
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
    }
    const std::string queryStatement ="CREATE (n:RetICFGNode {id: " + std::to_string(node->getId()) +
    ", kind: " + std::to_string(node->getNodeKind()) +
    actual_ret_node_id+
    ", call_block_node_id: " + std::to_string(node->getCallICFGNode()->getId()) + "})";
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
            if (ret)
            {
                SVFUtil::outs()<< "SVFType node added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add SVFType node to db " << dbname << " "
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
            if (ret)
            {
                SVFUtil::outs()<< "StInfo node added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add StInfo node to db " << dbname << " "
                                << result << "\n";
            }
        }
    }

}

std::string GraphDBClient::getSVFPointerTypeNodeInsertStmt(const SVFPointerType* node)
{
    std::string is_single_val_ty = node->isSingleValueType() ? "true" : "false";
    const std::string queryStatement ="CREATE (n:SVFPointerTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:SVFIntegerTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:SVFFunctionTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:SVFStructTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:SVFArrayTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:SVFOtherTypeNode {type_name:'" + node->toString() +
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
    const std::string queryStatement ="CREATE (n:StInfoNode {id:" + std::to_string(node->getStinfoId()) +
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
            if (ret)
            {
                SVFUtil::outs() << "BB edge added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add BB edge to db " << dbname
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
            if (ret)
            {
                SVFUtil::outs() << "BB node added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add BB node to db " << dbname
                                << " " << result << "\n";
            }
        }
    }
}

std::string GraphDBClient::getBBNodeInsertStmt(const SVFBasicBlock* node)
{
    const std::string queryStatement ="CREATE (n:SVFBasicBlock {id:'" + std::to_string(node->getId())+":" + std::to_string(node->getFunction()->getId()) + "'" +
    ", fun_obj_var_id: " + std::to_string(node->getFunction()->getId()) +
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
            if (ret)
            {
                SVFUtil::outs() << "PAG edge added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add PAG edge to db " << dbname
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
            if (ret)
            {
                SVFUtil::outs() << "PAG node added: " << result << "\n";
            }
            else
            {
                SVFUtil::outs() << "Failed to add PAG node to db " << dbname
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
    std::string bb_id_str = "";
    if (nullptr != edge->getBB())
    {
        bb_id_str += ", bb_id:" + std::to_string(edge->getBB()->getId());
    }

    std::string icfg_node_id_str = "";
    if (nullptr != edge->getICFGNode())
    {
        icfg_node_id_str += ", icfg_node_id:" + std::to_string(edge->getICFGNode()->getId());
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
    ", multi_opnd_label_counter:" + std::to_string(*(edge->getMultiOpndLabelCounter()));
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

    if (nullptr != edge->getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(edge->getFunEntryICFGNode()->getId());
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

    if (nullptr != edge->getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(edge->getFunExitICFGNode()->getId());
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

    if (nullptr != edge->getFunEntryICFGNode())
    {
        funEntryICFGNodeStr +=  ", fun_entry_icfg_node_id:" + std::to_string(edge->getFunEntryICFGNode()->getId());
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

    if (nullptr != edge->getFunExitICFGNode())
    {
        funExitICFGNodeStr +=  ", fun_exit_icfg_node_id:" + std::to_string(edge->getFunExitICFGNode()->getId());
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
