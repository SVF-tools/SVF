//===----- CFLGraphBuilder.h -- CFL Graph Builder --------------//
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

#include "CFL/CFLGrammar.h"
#include "Graphs/CFLGraph.h"
namespace SVF
{

/*!
 * Build CFLGraph from memory graph or dot form

 */

class CFLGraphBuilder
{
protected:
    typedef CFLGrammar::Kind Kind;
    typedef CFLGrammar::Symbol Symbol;
    Map<std::string, Kind> label2KindMap;
    Map<Kind, std::string> kind2LabelMap;
    Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>> kind2AttrsMap;
    bool externMap;
    Kind current;

public:
    /// add attribute to kind2Attribute Map
    void addAttribute(CFLGrammar::Kind kind, CFLGrammar::Attribute attribute);

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
    CFLGraph* buildBigraph(GenericGraph<N,E>* graph, Kind startKind, GrammarBase *grammar)
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
            N* node = (*it).second;
            for(E* edge : node->getOutEdges())
            {
                CFLGrammar::Kind edgeLabel = edge->getEdgeKind();
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edge->getEdgeKind()];
                key.append("bar");
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), label2KindMap[key]);
            }
        }
        return cflGraph;
    }
    /// Build graph from file
    void build(std::string filename, CFLGraph* cflGraph);

    /// Build graph from Dot
    CFLGraph *buildFromDot(std::string filename, GrammarBase *grammar);

    Map<std::string, Kind>& getLabel2KindMap()
    {
        return this->label2KindMap;
    }

    Map<Kind, std::string>& getKind2LabelMap()
    {
        return this->kind2LabelMap;
    }

    Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>>& getKind2AttrsMap()
    {
        return this->kind2AttrsMap;
    }
};

class AliasCFLGraphBuilder : public CFLGraphBuilder
{
public:
    /// Build Bidirectional graph by copying nodes and edges from const graph inherited from GenericGraph
    CFLGraph* buildBigraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar);
};

}// SVF


