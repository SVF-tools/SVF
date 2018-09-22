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

using namespace std;
using namespace SVFUtil;

/*
 * You can build a PAG from a file written by yourself
 *
 * The file should follow the format:
 * Node:  nodeID Nodetype [0|1|2|...|ret]
 * Edge:  nodeID edgetype NodeID Offset
 *
 * like:
 * 1 o
 * 2 v
 * 3 v
 * 4 v
 * 1 addr 2 0
 * 1 addr 3 0
 * 3 gep 4 4
 */
PAG* PAGBuilderFromFile::build() {
    // We do this to avoid setting off adding the current BB and value in
    // addEdge (as they don't exist).
    std::string oldPagFromTXT = SVFModule::pagFileName();
    SVFModule::setPagFromTXT("tmp");

    string line;
    ifstream myfile(file.c_str());
    if (myfile.is_open()) {
        while (myfile.good()) {
            getline(myfile, line);

            Size_t token_count = 0;
            string tmps;
            istringstream ss(line);
            while (ss.good()) {
                ss >> tmps;
                token_count++;
            }

            if (token_count == 0) {
                continue;
            }

            if (token_count == 2) {
                NodeID nodeId;
                string nodetype;
                istringstream ss(line);
                ss >> nodeId;
                ss >> nodetype;
                outs() << "reading node :" << nodeId << "\n";
                if (nodetype == "v") {
                    pag->addDummyValNode(nodeId);
                } else if (nodetype == "o") {
                    const MemObj* mem = pag->addDummyMemObj(nodeId);
                    mem->getTypeInfo()->setFlag(ObjTypeInfo::HEAP_OBJ);
                    mem->getTypeInfo()->setFlag(ObjTypeInfo::HASPTR_OBJ);
                    pag->addFIObjNode(mem);
                } else {
                    assert(false && "format not support, pls specify node type");
                }
            } else if (token_count == 4) {
                // do consider gep edge
                NodeID nodeSrc;
                NodeID nodeDst;
                Size_t offsetOrCSId;
                string edge;
                istringstream ss(line);
                ss >> nodeSrc;
                ss >> edge;
                ss >> nodeDst;
                ss >> offsetOrCSId;
                outs() << "reading edge :" << nodeSrc << " " << edge << " "
                       << nodeDst << " offsetOrCSId=" << offsetOrCSId << " \n";
                addEdge(nodeSrc, nodeDst, offsetOrCSId, edge);
            } else {
                if (!line.empty()) {
                    outs() << "format not supported, token count = "
                            << token_count << "\n";
                    assert(false && "format not supported");
                }
            }
        }
        myfile.close();
    } else outs() << "Unable to open file\n";

    /// new gep node's id from lower bound, nodeNum may not reflect the total nodes.
    u32_t lower_bound = 1000;
    for(u32_t i = 0; i < lower_bound; i++)
        pag->incNodeNum();

    SVFModule::setPagFromTXT(oldPagFromTXT);

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

    if (edge == "addr"){
        pag->addAddrEdge(srcID, dstID);
    }
    else if (edge == "copy")
        pag->addCopyEdge(srcID, dstID);
    else if (edge == "load")
        pag->addLoadEdge(srcID, dstID);
    else if (edge == "store")
        pag->addStoreEdge(srcID, dstID);
    else if (edge == "gep")
        pag->addNormalGepEdge(srcID, dstID, LocationSet(offsetOrCSId));
    else if (edge == "variant-gep")
        pag->addVariantGepEdge(srcID, dstID);
    else if (edge == "call")
        pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, NULL));
    else if (edge == "ret")
        pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, NULL));
    else
        assert(false && "format not support, can not create such edge");
}

