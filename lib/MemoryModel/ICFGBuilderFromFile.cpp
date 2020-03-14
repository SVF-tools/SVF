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
            if(node_type=="IntraBlock"){
                llvm::json::Array* pagEdges = ICFG_Node_obj->get("PAG Edges")->getAsArray();
                addNode(nodeID,node_type,pagEdges);
            }
            addNode(nodeID,node_type,NULL);
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

void ICFGBuilderFromFile::addNode(NodeID nodeId, std::string nodeType,llvm::json::Array* pagEdges){
    const std::string *value = new std::string("");
    if(!icfg->hasICFGNode(nodeId)){
        if(nodeType=="IntraBlock"){
            IntraBlockNode *node = new IntraBlockNode(nodeId,value);
            for(llvm::json::Array::const_iterator it = pagEdges->begin(); it!=pagEdges->end();it++){
                llvm::json::Value pagEdge_value = *it;
                llvm::json::Object* pagEdge = pagEdge_value.getAsObject();
                NodeID source_node = pagEdge->get("Source Node")->getAsInteger().getValue();
				NodeID destination_node = pagEdge->get("Destination Node")->getAsInteger().getValue();
				string source_node_type = pagEdge->get("Source Type")->getAsString()->str();
				string destination_node_type = pagEdge->get("Destination Type")->getAsString()->str();
				string edge_type = pagEdge->get("Edge Type")->getAsString()->str();
				llvm::json::Value* offset_value = pagEdge->get("offset");
				string offset;
				if(offset_value!=NULL){
					offset = offset_value->getAsString()->str();
				}
                PAGNode* srcNode = pag.getPAGNode(source_node);
                PAGNode* dstNode = pag.getPAGNode(destination_node);
                PAGEdge* edge = NULL;
                if (edge_type == "Addr"){
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Addr);
                }
                else if (edge_type == "Copy")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Copy);
                else if (edge_type == "Load")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Load);
                else if (edge_type == "Store")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Store);
                else if (edge_type == "NormalGep")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::NormalGep);
                else if (edge_type == "VariantGep")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::VariantGep);
                else if (edge_type == "Call")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Call);
                else if (edge_type == "Ret")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Ret);
                else if (edge_type == "Cmp")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::Cmp);
                else if (edge_type == "BinaryOp")
                    edge = pag.getIntraPAGEdge(srcNode,dstNode,PAGEdge::BinaryOp);
                else
                    assert(false && "format not support, can not create such edge");
                //add pag edge to the node
                if(edge!=NULL)
                    node->addPAGEdge(edge);
            }
            icfg->addGNode(nodeId,node);
            outs()<<"adding IntraBlock node....\n";
        }
        else if(nodeType=="FunEntryBlock"){
            FunEntryBlockNode *node = new FunEntryBlockNode(nodeId,value);
            // ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunEntryBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunEntryBlock node....\n";
        }
        else if(nodeType=="FunExitBlock"){
            FunExitBlockNode *node = new FunExitBlockNode(nodeId,value);
            // ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunExitBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunExitBlock node....\n";
        }
        else if(nodeType=="FunCallBlock"){
            CallBlockNode *node = new CallBlockNode(nodeId,*(new CallSite()));
            // ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunCallBlock);
            icfg->addGNode(nodeId,node);
            outs()<<"adding FunCallBlock node....\n";
        }
        else if(nodeType=="FunRetBlock"){
            RetBlockNode *node = new RetBlockNode(nodeId,*(new CallSite()));
            // ICFGNode* node = new ICFGNode(nodeId,ICFGNode::FunCallBlock);
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
