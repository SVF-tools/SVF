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
#include "SVFIR/SVFValue.h"

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
                    if(Options::FlexSymMap() == true)
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

CFLGraph* AliasCFLGraphBuilder::buildBiPEGgraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar, SVFIR* pag)
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
            /// Process Store
            if (edge->getEdgeKind() == ConstraintEdge::Store)
            {
                if (pag->isNullPtr(edge->getSrcID()))
                    continue;
                /// Check Dst of Store Dereference Node
                ConstraintNode* Dst = edge->getDstNode();
                ConstraintNode* DerefNode = nullptr;
                CFLNode* CFLDerefNode = nullptr;
                for (ConstraintEdge* DstInEdge : Dst->getInEdges())
                {
                    if (DstInEdge->getEdgeKind() == ConstraintEdge::Addr)
                    {
                        DerefNode = DstInEdge->getSrcNode();
                        CFLDerefNode = cflGraph->getGNode(DstInEdge->getSrcID());
                        break;
                    }
                }
                if (DerefNode == nullptr)
                {

                    NodeID refId = pag->addDummyValNode();
                    CFLDerefNode = new CFLNode(refId);
                    cflGraph->addCFLNode(refId, CFLDerefNode);
                    /// Add Addr Edge
                    cflGraph->addCFLEdge(CFLDerefNode, cflGraph->getGNode(edge->getDstID()), ConstraintEdge::Addr);
                    std::string key = kind2LabelMap[ConstraintEdge::Addr];
                    key.append("bar");
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), CFLDerefNode, label2KindMap[key]);
                }
                /// Add Copy Edge
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(CFLDerefNode->getId()), ConstraintEdge::Copy);
                std::string key = kind2LabelMap[ConstraintEdge::Copy];
                key.append("bar");
                cflGraph->addCFLEdge(cflGraph->getGNode(CFLDerefNode->getId()), cflGraph->getGNode(edge->getSrcID()), label2KindMap[key]);
            }
            /// Process Load
            else if ( edge->getEdgeKind() == ConstraintEdge::Load)
            {
                /// Check Src of Load Dereference Node
                ConstraintNode* Src = edge->getSrcNode();
                ConstraintNode* DerefNode = nullptr;
                CFLNode* CFLDerefNode = nullptr;
                for (ConstraintEdge* SrcInEdge : Src->getInEdges())
                {
                    if (SrcInEdge->getEdgeKind() == ConstraintEdge::Addr)
                    {
                        DerefNode = SrcInEdge->getSrcNode();
                        CFLDerefNode = cflGraph->getGNode(SrcInEdge->getSrcID());
                    }
                }
                if (DerefNode == nullptr)
                {
                    NodeID refId = pag->addDummyValNode();
                    CFLDerefNode = new CFLNode(refId);
                    cflGraph->addCFLNode(refId, CFLDerefNode);
                    /// Add Addr Edge
                    cflGraph->addCFLEdge(CFLDerefNode, cflGraph->getGNode(edge->getSrcID()), ConstraintEdge::Addr);
                    std::string key = kind2LabelMap[ConstraintEdge::Addr];
                    key.append("bar");
                    cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), CFLDerefNode, label2KindMap[key]);
                }
                /// Add Copy Edge
                cflGraph->addCFLEdge(cflGraph->getGNode(CFLDerefNode->getId()), cflGraph->getGNode(edge->getDstID()),  ConstraintEdge::Copy);
                std::string key = kind2LabelMap[ConstraintEdge::Copy];
                key.append("bar");
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(CFLDerefNode->getId()),  label2KindMap[key]);
            }
            else if ( edge->getEdgeKind() == ConstraintEdge::VariantGep)
            {
                /// Handle VGep normalize to Normal Gep by connecting all geps' srcs to vgep dest
                /// Example: In Test Case: Ctest field-ptr-arith-varIdx.c.bc
                /// BFS Search the 8 LEVEL up to find the ValueNode, and the number of level search is arbitary
                /// the more the level search the more valueNode and the Vgep Dst will possivble connect
                connectVGep(cflGraph, graph,  edge->getSrcNode(), edge->getDstNode(), 8, pag);
            }
            else
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
    }
    return cflGraph;
}

void AliasCFLGraphBuilder::AliasCFLGraphBuilder::connectVGep(CFLGraph *cflGraph,  ConstraintGraph *graph, ConstraintNode *src, ConstraintNode *dst, u32_t level, SVFIR* pag)
{
    if (level == 0) return;
    level -= 1;
    for (auto eit = src->getAddrInEdges().begin(); eit != src->getAddrInEdges().end(); eit++)
    {
        const MemObj* mem = pag->getBaseObj((*eit)->getSrcID());
        for (u32_t maxField = 0 ; maxField < mem->getNumOfElements(); maxField++)
        {
            addBiGepCFLEdge(cflGraph, (*eit)->getDstNode(), dst, maxField);
        }
    }
    for (auto eit = src->getInEdges().begin(); eit != src->getInEdges().end() && level != 0; eit++)
    {
        connectVGep(cflGraph, graph, (*eit)->getSrcNode(), dst, level, pag);
    }
    return;
}

void AliasCFLGraphBuilder::addBiCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFLGrammar::Kind label)
{
    cflGraph->addCFLEdge(cflGraph->getGNode(src->getId()), cflGraph->getGNode(dst->getId()), label);
    std::string key = kind2LabelMap[label];
    key.append("bar");
    cflGraph->addCFLEdge(cflGraph->getGNode(dst->getId()), cflGraph->getGNode(src->getId()), label2KindMap[key]);
    return;
}

void AliasCFLGraphBuilder::addBiGepCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFLGrammar::Attribute attri)
{
    CFLEdge::GEdgeFlag edgeLabel = CFLGrammar::getAttributedKind(attri, ConstraintEdge::NormalGep);
    cflGraph->addCFLEdge(cflGraph->getGNode(src->getId()), cflGraph->getGNode(dst->getId()), edgeLabel);
    std::string key = kind2LabelMap[ConstraintEdge::NormalGep];
    key.append("bar");
    cflGraph->addCFLEdge(cflGraph->getGNode(dst->getId()), cflGraph->getGNode(src->getId()), CFLGrammar::getAttributedKind(attri, label2KindMap[key]));
    return;
}

CFLGraph* VFCFLGraphBuilder::buildBigraph(SVFG *graph, Kind startKind, GrammarBase *grammar)
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
        VFGNode* node = (*it).second;
        for(VFGEdge* edge : node->getOutEdges())
        {
            CFLGrammar::Kind edgeLabel;
            // Get 'a' edge : IntraDirectVF || IntraIndirectVF
            if (edge->getEdgeKind() == VFGEdge::IntraDirectVF || edge->getEdgeKind() == VFGEdge::IntraIndirectVF || edge->getEdgeKind() == VFGEdge::TheadMHPIndirectVF )
            {
                edgeLabel = 0;
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edge->getEdgeKind()];
                key.append("bar");
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), label2KindMap[key]);
            }
            // Get 'call' edge : CallDirVF || CallIndVF
            else if ( edge->getEdgeKind() == VFGEdge::CallDirVF )
            {
                edgeLabel = 1;
                CallDirSVFGEdge *attributedEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge);
                CFLGrammar::Attribute attr =  attributedEdge->getCallSiteId();
                addAttribute(edgeLabel, attr);
                edgeLabel = CFLGrammar::getAttributedKind(attr, edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edgeLabel];
                key.append("bar");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(attr, label2KindMap[key]));
                addAttribute(label2KindMap[key], attr);
            }
            // Get 'call' edge : CallIndVF
            else if ( edge->getEdgeKind() == VFGEdge::CallIndVF )
            {
                edgeLabel = 1;
                CallIndSVFGEdge *attributedEdge = SVFUtil::dyn_cast<CallIndSVFGEdge>(edge);
                CFLGrammar::Attribute attr =  attributedEdge->getCallSiteId();
                addAttribute(edgeLabel, attr);
                edgeLabel = CFLGrammar::getAttributedKind(attr, edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edgeLabel];
                key.append("bar");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(attr, label2KindMap[key]));
                addAttribute(label2KindMap[key], attr);
            }
            // Get 'ret' edge : RetDirVF
            else if ( edge->getEdgeKind() == VFGEdge::RetDirVF )
            {
                edgeLabel = 2;
                RetDirSVFGEdge *attributedEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge);
                CFLGrammar::Attribute attr =  attributedEdge->getCallSiteId();
                addAttribute(edgeLabel, attr);
                edgeLabel = CFLGrammar::getAttributedKind(attr, edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edgeLabel];
                key.append("bar");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(attr, label2KindMap[key]));
                addAttribute(label2KindMap[key], attr);
            }
            // Get 'ret' edge : RetIndVF
            else if ( edge->getEdgeKind() == VFGEdge::RetIndVF )
            {
                edgeLabel = 2;
                RetIndSVFGEdge *attributedEdge = SVFUtil::dyn_cast<RetIndSVFGEdge>(edge);
                CFLGrammar::Attribute attr =  attributedEdge->getCallSiteId();
                addAttribute(edgeLabel, attr);
                edgeLabel = CFLGrammar::getAttributedKind(attr, edgeLabel);
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getSrcID()), cflGraph->getGNode(edge->getDstID()), edgeLabel);
                std::string key = kind2LabelMap[edgeLabel];
                key.append("bar");   // for example Gep_i should be Gepbar_i, not Gep_ibar
                cflGraph->addCFLEdge(cflGraph->getGNode(edge->getDstID()), cflGraph->getGNode(edge->getSrcID()), CFLGrammar::getAttributedKind(attr, label2KindMap[key]));
                addAttribute(label2KindMap[key], attr);
            }
        }
    }
    return cflGraph;
}

CFLGraph* VFCFLGraphBuilder::buildBiPEGgraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar, SVFIR* pag)
{
    CFLGraph *cflGraph = new CFLGraph(startKind);
    return cflGraph;
}


} // end of SVF namespace
