//===----- CFLGraph.cpp -- Graph for context-free language reachability analysis --//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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

CFLGraph::Kind CFLGraph::getStartKind() const
{
    return this->startKind;
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
        dst->addIngoingEdge(edge);
        return edge;
    }
    else
    {
        delete edge;
        return nullptr;
    }
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
    SVF::ViewGraph(this, "CFL Graph");
}

namespace SVF
{
/*!
 * Write CFL graph into dot file for debugging
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
        std::stringstream rawstr(str);
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
        std::stringstream rawstr(str);
        if (edge->getEdgeKind() == ConstraintEdge::Addr)
        {
            rawstr << "color=green";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Copy)
        {
            rawstr << "color=black";
        }
        else if (edge->getEdgeKindWithMask() == ConstraintEdge::NormalGep)
        {
            rawstr << "color=purple,label=" << '"' << "Gep_" << edge->getEdgeAttri() << '"';
        }
        else if (edge->getEdgeKindWithMask() == ConstraintEdge::VariantGep)
        {
            rawstr << "color=purple,label=" << '"' << "VGep" << '"';
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Store)
        {
            rawstr << "color=blue";
        }
        else if (edge->getEdgeKind() == ConstraintEdge::Load)
        {
            rawstr << "color=red";
        }
        else if (edge->getEdgeKind() == graph->getStartKind())
        {
            rawstr << "color=Turquoise";
        }
        else
        {
            rawstr  << "style=invis";
        }
        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        CFLEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        std::string str;
        std::stringstream rawstr(str);
        rawstr << "Edge label: " << edge->getEdgeKind() << " ";
        return rawstr.str();
    }
};

}