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
private:
    typedef CFLGrammar::Kind Kind;
    typedef CFLGrammar::Symbol Symbol;
    Map<std::string, Kind> label2KindMap;
    Map<Kind, std::string> kind2LabelMap;
    Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>> kind2AttrsMap;
    bool externMap;
    Symbol current;

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
    CFLGraph* buildBigraph(GenericGraph<N,E>* graph, Kind startKind)
    {
        CFLGraph *cflGraph = new CFLGraph(startKind);
        Map<std::string, SVF::CFLGraph::Symbol> ConstMap =  {{"Addr",0}, {"Copy", 1},{"Store", 2},{"Load", 3},{"Gep_i", 4},{"Vgep", 5},{"Addrbar",6}, {"Copybar", 7},{"Storebar", 8},{"Loadbar", 9},{"Gepbar_i", 10},{"Vgepbar", 11}};
        externMap = true;
        for(auto pairV : ConstMap)
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
                    key.pop_back();
                    key.pop_back();    // _i standsfor attribute variable should place at last
                    key.append("bar_i");   // for example Gep_i should be Gepbar_i, not Gep_ibar
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
}// SVF

