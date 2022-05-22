//===----- CFLGraphBuilder.h -- CFL Graph Builder--------------//
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
 * CFLGraphBuilder.h
 *
 *  Created on: May 22, 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLGraphBuilder.h"
#include "Util/Options.h"
#include "Util/BasicTypes.h"

namespace SVF
{
    /// Build graph by copying nodes and edges from any graph inherited from GenericGraph
    template<class N, class E>
    void build(GenericGraph<N,E>* graph, CFLGraph* cflGraph)
    {
        for(auto it = graph->begin(); it!= graph->end(); it++)
        {
            CFLNode* node = new CFLNode((*it).first);
            cflGraph->addCFLNode((*it).first, node);
        }
        for(auto it = graph->begin(); it!= graph->end(); it++)
        {
            N* node = (*it).second;
            for(E* edge : node->getOutEdges())
            {
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edge->getEdgeKind());
            }
        }
    }

    /// Build Bidirectional graph by copying nodes and edges from any graph inherited from GenericGraph
    template<class N, class E>
    void buildBigraph(GenericGraph<N,E>* graph, CFLGraph* cflGraph)
    {
        for(auto it = graph->begin(); it!= graph->end(); it++)
        {
            CFLNode* node = new CFLNode((*it).first);
            cflGraph->addCFLNode((*it).first, node);
        }
        for(auto it = graph->begin(); it!= graph->end(); it++)
        {
            N* node = (*it).second;
            for(E* edge : node->getOutEdges())
            {
                CFLGrammar::Kind edgeLabel = edge->getEdgeKind();
                // Need to get the offset from the Const Edge
                // The offset present edge is only from Normal Gep CG at moment
                if(NormalGepCGEdge::classof(edge))
                {
                    NormalGepCGEdge *nGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge);
                    CFLGrammar::Attribute offset =  nGepEdge->getConstantFieldIdx();
                    cflGraph->addAttribute(edgeLabel, offset);
                    edgeLabel = CFLGrammar::getAttributedKind(offset, edgeLabel);
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                    std::string key = cflGraph->sym2LabelMap[edge->getEdgeKind()];
                    key.pop_back();
                    key.pop_back();    // _i standsfor attribute variable should place at last
                    key.append("bar_i");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(offset, cflGraph->label2SymMap[key]));
                    cflGraph->addAttribute(cflGraph->label2SymMap[key], offset);
                }
                else 
                {
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                    std::string key = cflGraph->sym2LabelMap[edge->getEdgeKind()];
                    key.append("bar");    
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), cflGraph->label2SymMap[key]);
                }  
            }   
        }
    }
    //// Build graph from file
    void CFLGraphBuilder::build(std::string filename, CFLGraph* cflGraph)
    {
    }

    void CFLGraphBuilder::buildFromDot(std::string fileName, CFLGraph* cflGraph)
{
    std::cout << "Building CFL Graph from dot file: " << fileName << "..\n";
    std::string lineString;
    std::ifstream inputFile(fileName);

    std::regex reg("Node(\\w+)\\s*->\\s*Node(\\w+)\\s*\\[.*label=(.*)\\]");

    std::cout << std::boolalpha;
    u32_t lineNum = 0 ;
    cflGraph->current = cflGraph->label2SymMap.size();

    while (getline(inputFile, lineString))
    {
        lineNum += 1;
        std::smatch matches;
        if (std::regex_search(lineString, matches, reg))
        {
            CFLNode *src, *dst;
            if (cflGraph->hasGNode(std::stoul(matches.str(1), nullptr, 16))==false)
            {
                src = new CFLNode(std::stoul(matches.str(1), nullptr, 16));
                cflGraph->addCFLNode(src->getId(), src);
            }
            else
            {
                src = cflGraph->getGNode(std::stoul(matches.str(1), nullptr, 16));
            }
            if (cflGraph->hasGNode(std::stoul(matches.str(2), nullptr, 16))==false)
            {
                dst = new CFLNode(std::stoul(matches.str(2), nullptr, 16));
                cflGraph->addCFLNode(dst->getId(), dst);
            }
            else
            {
                dst = cflGraph->getGNode(std::stoul(matches.str(2), nullptr, 16));
            }
            if (cflGraph->externMap == false)
            {
                if (cflGraph->label2SymMap.find(matches.str(3)) != cflGraph->label2SymMap.end())
                {
                    cflGraph->addCFLEdge(src, dst, cflGraph->label2SymMap[matches.str(3)]);
                }
                else
                {
                    cflGraph->label2SymMap.insert({matches.str(3), cflGraph->current++});
                    cflGraph->addCFLEdge(src, dst, cflGraph->label2SymMap[matches.str(3)]);
                }
            }
            else
            {
                if (cflGraph->label2SymMap.find(matches.str(3)) != cflGraph->label2SymMap.end())
                {
                    cflGraph->addCFLEdge(src, dst, cflGraph->label2SymMap[matches.str(3)]);
                }
                else
                {
                    if(Options::FlexSymMap == true)
                    {
                        cflGraph->label2SymMap.insert({matches.str(3), cflGraph->current++});
                        cflGraph->addCFLEdge(src, dst, cflGraph->label2SymMap[matches.str(3)]);
                    }
                    else
                    {
                        std::string msg = "In line " + std::to_string(lineNum) + " sym can not find in grammar, please correct the input dot or set --flexsymmap.";
                        SVFUtil::errMsg(msg);
                        std::cout << msg;
                        abort();
                    }
                }
            }
        }
    }
    inputFile.close();
}

}