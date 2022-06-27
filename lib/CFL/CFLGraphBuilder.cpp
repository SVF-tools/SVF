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
/// add attribute to kind2Attribute Map
void CFLGraphBuilder::addAttribute(CFLGrammar::Kind kind, CFLGrammar::Attribute attribute)
{
    if(kind2AttrsMap.find(kind) == kind2AttrsMap.end())
    {
        Set<CFLGrammar::Attribute> attrs {attribute};
        kind2AttrsMap.insert(make_pair(kind, attrs));
    }
    else
    {
        if(kind2AttrsMap[kind].find(attribute) == kind2AttrsMap[kind].end())
        {
            kind2AttrsMap[kind].insert(attribute);
        }
    }
}


//// Build graph from file
void CFLGraphBuilder::build(std::string filename, CFLGraph* cflGraph)
{
}

CFLGraph * CFLGraphBuilder::buildFromDot(std::string fileName, GrammarBase *grammar)
{
    CFLGraph *cflGraph = new CFLGraph(grammar->getStartKind());
    externMap = true;
    for(auto pairV : grammar->getTerminals())
    {
        if(label2KindMap.find(pairV.first) == label2KindMap.end())
        {
            label2KindMap.insert(pairV);
        }
        if(kind2LabelMap.find(pairV.second) == kind2LabelMap.end())
        {
            kind2LabelMap.insert(make_pair(pairV.second, pairV.first));
        }
    }

    for(auto pairV : grammar->getNonterminals())
    {
        if(label2KindMap.find(pairV.first) == label2KindMap.end())
        {
            label2KindMap.insert(pairV);
        }
        if(kind2LabelMap.find(pairV.second) == kind2LabelMap.end())
        {
            kind2LabelMap.insert(make_pair(pairV.second, pairV.first));
        }
    }

    std::cout << "Building CFL Graph from dot file: " << fileName << "..\n";
    std::string lineString;
    std::ifstream inputFile(fileName);

    std::regex reg("Node(\\w+)\\s*->\\s*Node(\\w+)\\s*\\[.*label=(.*)\\]");

    std::cout << std::boolalpha;
    u32_t lineNum = 0 ;
    current = label2KindMap.size();

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
            if (externMap == false)
            {
                if (label2KindMap.find(matches.str(3)) != label2KindMap.end())
                {
                    cflGraph->addCFLEdge(src, dst, label2KindMap[matches.str(3)]);
                }
                else
                {
                    label2KindMap.insert({matches.str(3), current++});
                    cflGraph->addCFLEdge(src, dst, label2KindMap[matches.str(3)]);
                }
            }
            else
            {
                if (label2KindMap.find(matches.str(3)) != label2KindMap.end())
                {
                    cflGraph->addCFLEdge(src, dst, label2KindMap[matches.str(3)]);
                }
                else
                {
                    if(Options::FlexSymMap == true)
                    {
                        label2KindMap.insert({matches.str(3), current++});
                        cflGraph->addCFLEdge(src, dst, label2KindMap[matches.str(3)]);
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
    return cflGraph;
}

CFLGraph* AliasCFLGraphBuilder::buildBigraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar)
{
    CFLGraph *cflGraph = new CFLGraph(startKind);
    externMap = true;
    for(auto pairV : grammar->getTerminals())
    {
        if(label2KindMap.find(pairV.first) == label2KindMap.end())
        {
            label2KindMap.insert(pairV);
        }
        if(kind2LabelMap.find(pairV.second) == kind2LabelMap.end())
        {
            kind2LabelMap.insert(make_pair(pairV.second, pairV.first));
        }
    }
    for(auto pairV : grammar->getNonterminals())
    {
        if(label2KindMap.find(pairV.first) == label2KindMap.end())
        {
            label2KindMap.insert(pairV);
        }
        if(kind2LabelMap.find(pairV.second) == kind2LabelMap.end())
        {
            kind2LabelMap.insert(make_pair(pairV.second, pairV.first));
        }
    }
    for(auto it = graph->begin(); it!= graph->end(); it++)
    {
        CFLNode* node = new CFLNode((*it).first);
        cflGraph->addCFLNode((*it).first, node);
    }
    for(auto it = graph->begin(); it!= graph->end(); it++)
    {
        ConstraintNode* node = (*it).second;
        for(ConstraintEdge* edge : node->getOutEdges())
        {
            CFLGrammar::Kind edgeLabel = edge->getEdgeKind();
            // Need to get the offset from the Const Edge
            // The offset present edge is only from Normal Gep CG at moment
            if(NormalGepCGEdge::classof(edge))
            {
                NormalGepCGEdge *nGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge);
                CFLGrammar::Attribute attr =  nGepEdge->getConstantFieldIdx();
                addAttribute(edgeLabel, attr);
                edgeLabel = CFLGrammar::getAttributedKind(attr, edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edge->getEdgeKind()];
                key.append("bar");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(attr, label2KindMap[key]));
                addAttribute(label2KindMap[key], attr);
            }
            else
            {
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edge->getEdgeKind()];
                key.append("bar");
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), label2KindMap[key]);
            }
        }
    }
    return cflGraph;
}

} // end of SVF namespace