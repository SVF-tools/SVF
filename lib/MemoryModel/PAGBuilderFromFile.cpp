//===- PAGBuilderFromFile.cpp -- SVFIR builder-------------------------------//
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
using namespace SVF;
using namespace SVFUtil;
static u32_t gepNodeNumIndex = 100000;

/*
 * You can build a SVFIR from a file written by yourself
 *
 * The file should follow the format:
 * Node:  nodeID Nodetype
 * Edge:  nodeID edgetype NodeID Offset
 *
 * like:
5 o
6 v
7 v
8 v
9 v
5 addr 6 0
6 gep 7 4
7 copy 8 0
6 store 8 0
8 load 9 0
 */
SVFIR* PAGBuilderFromFile::build()
{

    string line;
    ifstream myfile(file.c_str());
    if (myfile.is_open())
    {
        while (myfile.good())
        {
            getline(myfile, line);

            u32_t token_count = 0;
            string tmps;
            istringstream ss(line);
            while (ss.good())
            {
                ss >> tmps;
                token_count++;
            }

            if (token_count == 0)
                continue;

            else if (token_count == 2)
            {
                NodeID nodeId;
                string nodetype;
                istringstream ss(line);
                ss >> nodeId;
                ss >> nodetype;
                outs() << "reading node :" << nodeId << "\n";
                if (nodetype == "v")
                    pag->addDummyValNode(nodeId);
                else if (nodetype == "o")
                {
                    const MemObj* mem = pag->addDummyMemObj(nodeId, nullptr);
                    pag->addFIObjNode(mem);
                }
                else
                    assert(false && "format not support, pls specify node type");
            }

            // do consider gep edge
            else if (token_count == 4)
            {
                NodeID nodeSrc;
                NodeID nodeDst;
                s32_t offsetOrCSId;
                string edge;
                istringstream ss(line);
                ss >> nodeSrc;
                ss >> edge;
                ss >> nodeDst;
                ss >> offsetOrCSId;
                outs() << "reading edge :" << nodeSrc << " " << edge << " "
                       << nodeDst << " offsetOrCSId=" << offsetOrCSId << " \n";
                addEdge(nodeSrc, nodeDst, offsetOrCSId, edge);
            }
            else
            {
                if (!line.empty())
                {
                    outs() << "format not supported, token count = "
                           << token_count << "\n";
                    assert(false && "format not supported");
                }
            }
        }
        myfile.close();
    }

    else
        outs() << "Unable to open file\n";

    /// new gep node's id from lower bound, nodeNum may not reflect the total nodes.
    u32_t lower_bound = gepNodeNumIndex;
    for(u32_t i = 0; i < lower_bound; i++)
        pag->incNodeNum();

    pag->setNodeNumAfterPAGBuild(pag->getTotalNodeNum());

    return pag;
}

/*!
 * Add SVFIR edge according to a file format
 */
void PAGBuilderFromFile::addEdge(NodeID srcID, NodeID dstID,
                                 s32_t offsetOrCSId, std::string edge)
{

    //check whether these two nodes available
    PAGNode* srcNode = pag->getGNode(srcID);
    PAGNode* dstNode = pag->getGNode(dstID);

    /// sanity check for SVFIR from txt
    assert(SVFUtil::isa<ValVar>(dstNode) && "dst not an value node?");
    if(edge=="addr")
        assert(SVFUtil::isa<ObjVar>(srcNode) && "src not an value node?");
    else
        assert(!SVFUtil::isa<ObjVar>(srcNode) && "src not an object node?");

    if (edge == "addr")
    {
        pag->addAddrStmt(srcID, dstID);
    }
    else if (edge == "copy")
        pag->addCopyStmt(srcID, dstID);
    else if (edge == "load")
        pag->addLoadStmt(srcID, dstID);
    else if (edge == "store")
        pag->addStoreStmt(srcID, dstID, nullptr);
    else if (edge == "gep")
        pag->addNormalGepStmt(srcID, dstID, LocationSet(offsetOrCSId));
    else if (edge == "variant-gep")
        pag->addVariantGepStmt(srcID, dstID, LocationSet(offsetOrCSId));
    else if (edge == "call")
        pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, nullptr, nullptr));
    else if (edge == "ret")
        pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, nullptr,nullptr));
    else if (edge == "cmp")
        pag->addCmpStmt(srcID, dstID, dstID, dstID);
    else if (edge == "binary-op")
        pag->addBinaryOPStmt(srcID, dstID, dstID, dstID);
    else if (edge == "unary-op")
        pag->addUnaryOPStmt(srcID, dstID, dstID);
    else if (edge == "phi")
        assert(false && "fix phi here!");
    else if (edge == "select")
        assert(false && "fix select here!");
    else if (edge == "branch"){
        assert(false && "fix successors here!");
        //pag->addBranchStmt(srcID, dstID, nullptr);
    }
    else
        assert(false && "format not support, can not create such edge");
}

