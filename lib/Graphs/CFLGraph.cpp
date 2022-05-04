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

#include "Util/Options.h"
#include "Graphs/CFLGraph.h"
#include "Util/SVFUtil.h"

using namespace SVF;

//// Build graph from file
void CFLGraph::build(std::string filename)
{

}

void CFLGraph::addCFLNode(NodeID id, CFLNode* node)
{
    addGNode(id, node);
}

const CFLEdge* CFLGraph::addCFLEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label)
{
    CFLEdge* edge = new CFLEdge(src,dst,label);
    if(cflEdgeSet.insert(edge).second)
    {
        src->addOutgoingEdge(edge);
        dst->addIncomingEdge(edge);
        return edge;
    }
    else
        return nullptr;
}

const CFLEdge* CFLGraph::hasEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label)
{
    CFLEdge edge(src,dst,label);
    auto it = cflEdgeSet.find(&edge);
    if(it !=cflEdgeSet.end())
        return *it;
    else
        return nullptr;
}

void CFLGraph::dump(const std::string& filename)
{
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

void CFLGraph::view()
{
    llvm::ViewGraph(this, "CFL Graph");
}

void CFLGraph::buildFromDot(std::string fileName)
{
    std::cout << "Building CFL Graph from dot file: " << fileName << "..\n";
    std::string lineString;
    std::ifstream inputFile(fileName);

    std::regex reg("Node(\\w+)\\s*->\\s*Node(\\w+)\\s*\\[.*label=(.*)\\]");

    std::cout << std::boolalpha;
    u32_t lineNum = 0 ;

    while (getline(inputFile, lineString))
    {
        lineNum += 1;
        std::smatch matches;
        if (std::regex_search(lineString, matches, reg))
        {
            CFLNode *src, *dst;
            if (hasGNode(std::stoul(matches.str(1), nullptr, 16))==false)
            {
                src = new CFLNode(std::stoul(matches.str(1), nullptr, 16));
                addCFLNode(src->getId(), src);
            }
            else
            {
                src = getGNode(std::stoul(matches.str(1), nullptr, 16));
            }
            if (hasGNode(std::stoul(matches.str(2), nullptr, 16))==false)
            {
                dst = new CFLNode(std::stoul(matches.str(2), nullptr, 16));
                addCFLNode(dst->getId(), dst);
            }
            else
            {
                dst = getGNode(std::stoul(matches.str(2), nullptr, 16));
            }
            if (externMap == false)
            {
                if (label2SymMap.find(matches.str(3)) != label2SymMap.end())
                {
                    addCFLEdge(src, dst, label2SymMap[matches.str(3)]);
                }
                else
                {
                    label2SymMap.insert({matches.str(3), current++});
                    addCFLEdge(src, dst, label2SymMap[matches.str(3)]);
                }
            }
            else
            {
                if (label2SymMap.find(matches.str(3)) != label2SymMap.end())
                {
                    addCFLEdge(src, dst, label2SymMap[matches.str(3)]);
                }
                else
                {
                    if(Options::FlexSymMap == true)
                    {
                        label2SymMap.insert({matches.str(3), current++});
                        addCFLEdge(src, dst, label2SymMap[matches.str(3)]);
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

void CFLGraph::setMap(Map<std::string, Symbol>* terminals, Map<std::string, Symbol>* nonterminals)
{
    externMap = true;
    for(auto pairV : *terminals)
    {
        if(label2SymMap.find(pairV.first) == label2SymMap.end())
        {
            label2SymMap.insert(pairV);
        }
    }

    for(auto pairV : *nonterminals)
    {
        if(label2SymMap.find(pairV.first) == label2SymMap.end())
        {
            label2SymMap.insert(pairV);
        }
    }
    current = label2SymMap.size();
}



namespace llvm
{
/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<CFLGraph*> : public DefaultDOTGraphTraits
{

    typedef CFLNode NodeType;

    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(CFLGraph*)
    {
        return "CFL Reachability Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(CFLNode *node, CFLGraph*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "Node ID: " << node->getId() << " ";
        return rawstr.str();
    }

    static std::string getNodeAttributes(CFLNode *node, CFLGraph*)
    {
        return "shape=box";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(CFLNode*, EdgeIter EI, CFLGraph* graph)
    {
        CFLEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        std::string str;
        raw_string_ostream rawstr(str);
        std::string key = "";
        for (auto &i : graph->label2SymMap)
        {
            if (i.second == edge->getEdgeKind())
            {
                key = i.first;
            }
        }
        rawstr << "label=" << '"' <<key << '"';
        if( graph->label2SymMap[key] == graph->startSymbol)
        {
            rawstr << ',' << "color=red";
        }

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        CFLEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "Edge label: " << edge->getEdgeKind() << " ";
        return rawstr.str();
    }
};

}