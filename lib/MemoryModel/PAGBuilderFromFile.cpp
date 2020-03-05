//===- PAGBuilderFromFile.cpp -- PAG builder-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * PAGBuilderFromFile.cpp
 *
 *  Created on: 20 Sep. 2018
 *      Author: 136884
 */

#include "MemoryModel/PAGBuilderFromFile.h"
#include <fstream>	// for PAGBuilderFromFile
#include <string>	// for PAGBuilderFromFile
#include <sstream>	// for PAGBuilderFromFile
#include "llvm/Support/JSON.h"

using namespace std;
using namespace SVFUtil;
static u32_t gepNodeNumIndex = 100000;

void PAGBuilderFromFile::addNode(NodeID ID, string node_type){
	if(node_type=="DummyValNode"){
		pag->addDummyValNode(ID);
	}
	if(node_type=="FIObjNode"){
		const MemObj* mem = pag->addDummyMemObj(ID, NULL);
		pag->addFIObjNode(mem);
	}
	if(node_type == "DummyObjNode"){
		pag->addDummyObjNode(ID);
	}
	// else
	// 	assert(false && "format not support, pls specify node type");
	
}

PAG* PAGBuilderFromFile::build() {

	ifstream myfile(file.c_str());
	if(myfile.is_open()){
		std::stringstream jsonStringStream;
		while(myfile >> jsonStringStream.rdbuf());
		llvm::json::Value root_value = llvm::json::parse(jsonStringStream.str()).get();
		llvm::json::Object* root_obj;
		root_obj = root_value.getAsObject();

		//get the Instructions array
		llvm::json::Value* root_array_value;
		root_array_value= root_obj->get("Instructions");
		llvm::json::Array* root_array;
		root_array = root_array_value->getAsArray();

		//get the global edge array
		llvm::json::Value* global_edges_value;
		global_edges_value = root_obj->get("global edge");
		llvm::json::Array* global_edges_array;
		global_edges_array = global_edges_value->getAsArray();

		for(llvm::json::Array::const_iterator it = root_array->begin(); 
		it!=root_array->end();it++){
			llvm::json::Value currentInst_value = *it;
			llvm::json::Object* currentInst_obj;
			currentInst_obj = currentInst_value.getAsObject();
			llvm::json::Value* edges_array_value;
			edges_array_value = currentInst_obj->get("edges");
			llvm::json::Array* edges_array;
			edges_array = edges_array_value->getAsArray();
			for(llvm::json::Array::const_iterator eit = edges_array->begin();
			eit!=edges_array->end(); eit++){
				llvm::json::Value edge_value = *eit;
				llvm::json::Object* edge_obj;
				edge_obj = edge_value.getAsObject();
				NodeID source_node;
				source_node = edge_obj->get("source node")->getAsInteger().getValue();
				NodeID destination_node;
				destination_node = edge_obj->get("destination node")->getAsInteger().getValue();
				string source_node_type;
				source_node_type = edge_obj->get("source node type")->getAsString()->str();
				string destination_node_type;
				destination_node_type = edge_obj->get("destination node type")->getAsString()->str();
				string edge_type;
				edge_type = edge_obj->get("edge type")->getAsString()->str();
				llvm::json::Value* offset_value = edge_obj->get("offset");
				string offset;
				if(offset_value!=NULL){
					offset = offset_value->getAsString()->str();
				}
				//add new node
				if(!pag->hasGNode(source_node)){
					addNode(source_node,source_node_type);
				}else if(!pag->hasGNode(destination_node)){
					addNode(destination_node,destination_node_type);
				}else{
					addEdge(source_node, destination_node, std::stol(offset), edge_type);
				}
			}
		}
		for(llvm::json::Array::const_iterator it = global_edges_array->begin();
		it!=global_edges_array->end();it++){
			llvm::json::Object global_edge_obj = *it->getAsObject();
			NodeID source_node;
			source_node = global_edge_obj.get("source node")->getAsInteger().getValue();
			NodeID destination_node;
			destination_node = global_edge_obj.get("destination node")->getAsInteger().getValue();
			string source_node_type;
			source_node_type = global_edge_obj.get("source node type")->getAsString()->str();
			string destination_node_type;
			destination_node_type = global_edge_obj.get("destination node type")->getAsString()->str();
			string edge_type;
			edge_type = global_edge_obj.get("edge type")->getAsString()->str();
			llvm::json::Value* offset_value = global_edge_obj.get("offset");
			string offset;
			if(offset_value!=NULL)
				offset = offset_value->getAsString()->str();
			//add new node
			if(!pag->hasGNode(source_node)){
				addNode(source_node,source_node_type);
			}else if(!pag->hasGNode(destination_node)){
				addNode(destination_node,destination_node_type);
			}else{
				addEdge(source_node, destination_node, std::stol(offset), edge_type);
			}
		}
		myfile.close();
	}else{
		outs() << "Unable to open file\n";
	}

	/// new gep node's id from lower bound, nodeNum may not reflect the total nodes.
	u32_t lower_bound = gepNodeNumIndex;
	for(u32_t i = 0; i < lower_bound; i++)
		pag->incNodeNum();

    pag->setNodeNumAfterPAGBuild(pag->getTotalNodeNum());

	return pag;
}

//build pag from icfg file
PAG* PAGBuilderFromFile::buildFromICFG(){
	ifstream myfile(file.c_str());
	if(myfile.is_open()){
		std::stringstream jsonStringStream;
		while(myfile >> jsonStringStream.rdbuf());
		llvm::json::Value root_value = llvm::json::parse(jsonStringStream.str()).get();
		llvm::json::Array* root_array = root_value.getAsArray();
		for(llvm::json::Array::const_iterator it = root_array->begin(); 
		it!=root_array->end();it++){
			llvm::json::Value ICFG_Node_obj_val = *it;
			llvm::json::Object* ICFG_Node_obj = ICFG_Node_obj_val.getAsObject();
			string node_type = ICFG_Node_obj->get("Node Type")->getAsString()->str();
			if(node_type == "IntraBlock"){
				llvm::json::Array* pag_edges_array = ICFG_Node_obj->get("PAG Edges")->getAsArray();
				for(llvm::json::Array::const_iterator eit = pag_edges_array->begin();
				eit!=pag_edges_array->end();eit++){
					llvm::json::Value edge_value = *eit;
					llvm::json::Object* edge_obj = edge_value.getAsObject();
					NodeID source_node = edge_obj->get("Source Node")->getAsInteger().getValue();
					NodeID destination_node = edge_obj->get("Destination Node")->getAsInteger().getValue();
					string source_node_type = edge_obj->get("Source Type")->getAsString()->str();
					string destination_node_type = edge_obj->get("Destination Type")->getAsString()->str();
					string edge_type = edge_obj->get("Edge Type")->getAsString()->str();
					llvm::json::Value* offset_value = edge_obj->get("offset");
					string offset;
					if(offset_value!=NULL){
						offset = offset_value->getAsString()->str();
					}
					//add new node
					if(!pag->hasGNode(source_node)){
						addNode(source_node,source_node_type);
					}else if(!pag->hasGNode(destination_node)){
						addNode(destination_node,destination_node_type);
					}else{
						addEdge(source_node, destination_node, std::stol(offset), edge_type);
					}
				}
			}
		}
		myfile.close();
	}else{
		outs() << "Unable to open file\n";
	}

	/// new gep node's id from lower bound, nodeNum may not reflect the total nodes.
	u32_t lower_bound = gepNodeNumIndex;
	for(u32_t i = 0; i < lower_bound; i++)
		pag->incNodeNum();

    pag->setNodeNumAfterPAGBuild(pag->getTotalNodeNum());

	return pag;
}

/*!
 * Add PAG edge according to a file format
 */
void PAGBuilderFromFile::addEdge(NodeID srcID, NodeID dstID,
                                 Size_t offsetOrCSId, std::string edge) {

    //check whether these two nodes available
    PAGNode* srcNode = pag->getPAGNode(srcID);
    PAGNode* dstNode = pag->getPAGNode(dstID);

    /// sanity check for PAG from txt
	assert(SVFUtil::isa<ValPN>(dstNode) && "dst not an value node?");
    if(edge=="addr")
    		assert(SVFUtil::isa<ObjPN>(srcNode) && "src not an value node?");
    else
		assert(!SVFUtil::isa<ObjPN>(srcNode) && "src not an object node?");

    if (edge == "Addr"){
        pag->addAddrEdge(srcID, dstID);
    }
    else if (edge == "Copy")
        pag->addCopyEdge(srcID, dstID);
    else if (edge == "Load")
        pag->addLoadEdge(srcID, dstID);
    else if (edge == "Store")
        pag->addStoreEdge(srcID, dstID);
    else if (edge == "NormalGep")
        pag->addNormalGepEdge(srcID, dstID, LocationSet(offsetOrCSId));
    else if (edge == "VariantGep")
        pag->addVariantGepEdge(srcID, dstID);
    else if (edge == "Call")
        pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, NULL));
    else if (edge == "Ret")
        pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, NULL));
    else if (edge == "Cmp")
        pag->addCmpEdge(srcID, dstID);
    else if (edge == "BinaryOp")
        pag->addBinaryOPEdge(srcID, dstID);
    else
        assert(false && "format not support, can not create such edge");
}

