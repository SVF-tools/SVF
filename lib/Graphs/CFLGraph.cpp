//===----- CFLGraph.cpp -- Graph for context-free language reachability analysis --//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFLGraph.cpp
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "Graphs/CFLGraph.h"

using namespace SVF;


void CFLGraph::addCFLNode(NodeID id, CFLNode* node){
    addGNode(id, node);
}

const CFLEdge* CFLGraph::addCFLEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label){
    CFLEdge* edge = new CFLEdge(src,dst,label);
    if(cflEdgeSet.insert(edge).second)
        return edge;
    else
        return nullptr;
}

const CFLEdge* CFLGraph::hasEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label){
    CFLEdge edge(src,dst,label);
    auto it = cflEdgeSet.find(&edge);
    if(it !=cflEdgeSet.end())
        return *it;
    else
        return nullptr;
}


void CFLGraph::build(std::string filename){

}