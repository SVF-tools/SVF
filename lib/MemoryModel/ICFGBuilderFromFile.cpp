#include "MemoryModel/ICFGBuilderFromFile.h"
#include <fstream>	// for ICFGBuilderFromFile
#include <string>	// for ICFGBuilderFromFile
#include <sstream>	// for ICFGBuilderFromFile
#include "llvm/Support/JSON.h"

using namespace std;
using namespace SVFUtil;

ICFG* ICFGBuilderFromFile::build(){
    icfg = new ICFG();
    ifstream myfile(file.c_str());
	if(myfile.is_open()){
        //read the json stream from file
        std::stringstream jsonStringStream;
		while(myfile >> jsonStringStream.rdbuf());

        //parse the json json data 
        llvm::json::Value root_value = llvm::json::parse(jsonStringStream.str()).get();
		llvm::json::Array* root_array = root_value.getAsArray();
       
       //add all node 
        for(llvm::json::Array::const_iterator it = root_array->begin(); 
		it!=root_array->end();it++){
            llvm::json::Value ICFG_Node_obj_val = *it;
			llvm::json::Object* ICFG_Node_obj = ICFG_Node_obj_val.getAsObject();
			string node_type = ICFG_Node_obj->get("Node Type")->getAsString()->str();
            NodeID nodeID = ICFG_Node_obj->get("ICFG_ID")->getAsInteger().getValue();
            addNode(nodeID,node_type);
        }
        
        //add all edges
        for(llvm::json::Array::const_iterator it = root_array->begin(); 
		it!=root_array->end();it++){
            llvm::json::Value ICFG_Node_obj_val = *it;
			llvm::json::Object* ICFG_Node_obj = ICFG_Node_obj_val.getAsObject();
			llvm::json::Array* icfg_edges_array = ICFG_Node_obj->get("ICFGEdges")->getAsArray();
            for(llvm::json::Array::const_iterator eit = icfg_edges_array->begin();
            eit!=icfg_edges_array->end();eit++){
                llvm::json::Value icfgEdge_value = *eit;
				llvm::json::Object* icfg_edge_obj = icfgEdge_value.getAsObject();
                addEdge(icfg_edge_obj);
            }
        }
    }
	return icfg;
}

void ICFGBuilderFromFile::addNode(NodeID nodeId, std::string nodeType){
    if(!icfg->hasICFGNode(nodeId)){
        if(nodeType=="IntraBlock"){
            ICFGNode* node = new ICFGNode(nodeId,ICFGNode::IntraBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding IntraBlock node....\n";
        }
        else if(nodeType=="FunEntryBlock"){
            ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunEntryBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunEntryBlock node....\n";
        }
        else if(nodeType=="FunExitBlock"){
            ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunExitBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunExitBlock node....\n";
        }
        else if(nodeType=="FunCallBlock"){
            ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunCallBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunCallBlock node....\n";
        }
        else if(nodeType=="FunRetBlock"){
            ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunCallBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunRetBlock node....\n";
        }
    }
}

void ICFGBuilderFromFile::addEdge(llvm::json::Object* edge_obj){
    NodeID srcNodeID = edge_obj->get("ICFGEdgeSrcID")->getAsInteger().getValue();
    NodeID dstNodeID = edge_obj->get("ICFGEdgeDstID")->getAsInteger().getValue();
    ICFGNode* srcICFGNode = icfg->getICFGNode(srcNodeID);
    ICFGNode* dstICFGNode = icfg->getICFGNode(dstNodeID);
    std::string icfg_edge_type = edge_obj->get("ICFG Edge Type")->getAsString()->str();
    if(icfg_edge_type=="IntraCFGEdge"){
        icfg->addIntraEdge(srcICFGNode,dstICFGNode);
    }
    else if(icfg_edge_type=="RetCFGEdge"){
        CallSiteID csID = edge_obj->get("csID")->getAsInteger().getValue();
        icfg->addRetEdge(srcICFGNode,dstICFGNode,csID);
    }
    else if(icfg_edge_type=="CallCFGEdge"){
        CallSiteID csID = edge_obj->get("csID")->getAsInteger().getValue();
        icfg->addCallEdge(srcICFGNode,dstICFGNode,csID);
    }
}
