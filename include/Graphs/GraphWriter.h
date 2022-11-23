//===- Graphs/GraphWriter.h - Write graph to a .dot file --*- C++ -*-===//
//
// From the LLVM Project with some modifications, under the Apache License v2.0
// with LLVM Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a simple interface that can be used to print out generic
// LLVM graphs to ".dot" files.  "dot" is a tool that is part of the AT&T
// graphviz package (http://www.research.att.com/sw/tools/graphviz/) which can
// be used to turn the files output by this interface into a variety of
// different graphics formats.
//
// Graphs do not need to implement any interface past what is already required
// by the GraphTraits template, but they can choose to implement specializations
// of the DOTGraphTraits template if they want to customize the graphs output in
// any way.
//
//===----------------------------------------------------------------------===//

#ifndef GRAPHS_GRAPHWRITER_H
#define GRAPHS_GRAPHWRITER_H

#include "Graphs/GraphTraits.h"
#include "Graphs/DOTGraphTraits.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

namespace SVF
{

namespace DOT    // Private functions...
{

std::string EscapeStr(const std::string &Label);

} // end namespace DOT

namespace GraphProgram
{

enum Name
{
    DOT,
    FDP,
    NEATO,
    TWOPI,
    CIRCO
};

} // end namespace GraphProgram

template<typename GraphType>
class GraphWriter
{
    std::ofstream &O;
    const GraphType &G;

    using DOTTraits = DOTGraphTraits<GraphType>;
    using GTraits = GenericGraphTraits<GraphType>;
    using NodeRef = typename GTraits::NodeRef;
    using node_iterator = typename GTraits::nodes_iterator;
    using child_iterator = typename GTraits::ChildIteratorType;
    DOTTraits DTraits;

    static_assert(std::is_pointer<NodeRef>::value,
                  "FIXME: Currently GraphWriter requires the NodeRef type to be "
                  "a pointer.\nThe pointer usage should be moved to "
                  "DOTGraphTraits, and removed from GraphWriter itself.");

    // Writes the edge labels of the node to O and returns true if there are any
    // edge labels not equal to the empty string "".
    bool getEdgeSourceLabels(std::stringstream &O2, NodeRef Node)
    {
        child_iterator EI = GTraits::child_begin(Node);
        child_iterator EE = GTraits::child_end(Node);
        bool hasEdgeSourceLabels = false;

        for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i)
        {
            std::string label = DTraits.getEdgeSourceLabel(Node, EI);

            if (label.empty())
                continue;

            hasEdgeSourceLabels = true;

            if (i)
                O2 << "|";

            O2 << "<s" << i << ">" << DOT::EscapeStr(label);
        }

        if (EI != EE && hasEdgeSourceLabels)
            O2 << "|<s64>truncated...";

        return hasEdgeSourceLabels;
    }

public:
    GraphWriter(std::ofstream &o, const GraphType &g, bool SN) : O(o), G(g)
    {
        DTraits = DOTTraits(SN);
    }

    void writeGraph(const std::string &Title = "")
    {
        // Output the header for the graph...
        writeHeader(Title);

        // Emit all of the nodes in the graph...
        writeNodes();

        // Output any customizations on the graph
        DOTGraphTraits<GraphType>::addCustomGraphFeatures(G, *this);

        // Output the end of the graph
        writeFooter();
    }

    void writeHeader(const std::string &Title)
    {
        std::string GraphName(DTraits.getGraphName(G));

        if (!Title.empty())
            O << "digraph \"" << DOT::EscapeStr(Title) << "\" {\n";
        else if (!GraphName.empty())
            O << "digraph \"" << DOT::EscapeStr(GraphName) << "\" {\n";
        else
            O << "digraph unnamed {\n";

        if (DTraits.renderGraphFromBottomUp())
            O << "\trankdir=\"BT\";\n";

        if (!Title.empty())
            O << "\tlabel=\"" << DOT::EscapeStr(Title) << "\";\n";
        else if (!GraphName.empty())
            O << "\tlabel=\"" << DOT::EscapeStr(GraphName) << "\";\n";
        O << DTraits.getGraphProperties(G);
        O << "\n";
    }

    void writeFooter()
    {
        // Finish off the graph
        O << "}\n";
    }

    void writeNodes()
    {
        // Loop over the graph, printing it out...
        for (const auto Node : nodes<GraphType>(G))
            if (!isNodeHidden(Node))
                writeNode(Node);
    }

    bool isNodeHidden(NodeRef Node)
    {
        return DTraits.isNodeHidden(Node, G);
    }

    void writeNode(NodeRef Node)
    {
        std::string NodeAttributes = DTraits.getNodeAttributes(Node, G);

        O << "\tNode" << static_cast<const void*>(Node) << " [shape=record,";
        if (!NodeAttributes.empty()) O << NodeAttributes << ",";
        O << "label=\"{";

        if (!DTraits.renderGraphFromBottomUp())
        {
            O << DOT::EscapeStr(DTraits.getNodeLabel(Node, G));

            // If we should include the address of the node in the label, do so now.
            std::string Id = DTraits.getNodeIdentifierLabel(Node, G);
            if (!Id.empty())
                O << "|" << DOT::EscapeStr(Id);

            std::string NodeDesc = DTraits.getNodeDescription(Node, G);
            if (!NodeDesc.empty())
                O << "|" << DOT::EscapeStr(NodeDesc);
        }

        std::string edgeSourceLabels;
        std::stringstream EdgeSourceLabels(edgeSourceLabels);
        bool hasEdgeSourceLabels = getEdgeSourceLabels(EdgeSourceLabels, Node);

        if (hasEdgeSourceLabels)
        {
            if (!DTraits.renderGraphFromBottomUp()) O << "|";

            O << "{" << EdgeSourceLabels.str() << "}";

            if (DTraits.renderGraphFromBottomUp()) O << "|";
        }

        if (DTraits.renderGraphFromBottomUp())
        {
            O << DOT::EscapeStr(DTraits.getNodeLabel(Node, G));

            // If we should include the address of the node in the label, do so now.
            std::string Id = DTraits.getNodeIdentifierLabel(Node, G);
            if (!Id.empty())
                O << "|" << DOT::EscapeStr(Id);

            std::string NodeDesc = DTraits.getNodeDescription(Node, G);
            if (!NodeDesc.empty())
                O << "|" << DOT::EscapeStr(NodeDesc);
        }

        if (DTraits.hasEdgeDestLabels())
        {
            O << "|{";

            unsigned i = 0, e = DTraits.numEdgeDestLabels(Node);
            for (; i != e && i != 64; ++i)
            {
                if (i) O << "|";
                O << "<d" << i << ">"
                  << DOT::EscapeStr(DTraits.getEdgeDestLabel(Node, i));
            }

            if (i != e)
                O << "|<d64>truncated...";
            O << "}";
        }

        O << "}\"];\n";   // Finish printing the "node" line

        // Output all of the edges now
        child_iterator EI = GTraits::child_begin(Node);
        child_iterator EE = GTraits::child_end(Node);
        for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i)
            if (!DTraits.isNodeHidden(*EI, G))
                writeEdge(Node, i, EI);
        for (; EI != EE; ++EI)
            if (!DTraits.isNodeHidden(*EI, G))
                writeEdge(Node, 64, EI);
    }

    void writeEdge(NodeRef Node, unsigned edgeidx, child_iterator EI)
    {
        if (NodeRef TargetNode = *EI)
        {
            int DestPort = -1;
            if (DTraits.edgeTargetsEdgeSource(Node, EI))
            {
                child_iterator TargetIt = DTraits.getEdgeTarget(Node, EI);

                // Figure out which edge this targets...
                unsigned Offset =
                    (unsigned)std::distance(GTraits::child_begin(TargetNode), TargetIt);
                DestPort = static_cast<int>(Offset);
            }

            if (DTraits.getEdgeSourceLabel(Node, EI).empty())
                edgeidx = -1;

            emitEdge(static_cast<const void*>(Node), edgeidx,
                     static_cast<const void*>(TargetNode), DestPort,
                     DTraits.getEdgeAttributes(Node, EI, G));
        }
    }

    /// emitSimpleNode - Outputs a simple (non-record) node
    void emitSimpleNode(const void *ID, const std::string &Attr,
                        const std::string &Label, unsigned NumEdgeSources = 0,
                        const std::vector<std::string> *EdgeSourceLabels = nullptr)
    {
        O << "\tNode" << ID << "[ ";
        if (!Attr.empty())
            O << Attr << ",";
        O << " label =\"";
        if (NumEdgeSources) O << "{";
        O << DOT::EscapeStr(Label);
        if (NumEdgeSources)
        {
            O << "|{";

            for (unsigned i = 0; i != NumEdgeSources; ++i)
            {
                if (i) O << "|";
                O << "<s" << i << ">";
                if (EdgeSourceLabels) O << DOT::EscapeStr((*EdgeSourceLabels)[i]);
            }
            O << "}}";
        }
        O << "\"];\n";
    }

    /// emitEdge - Output an edge from a simple node into the graph...
    void emitEdge(const void *SrcNodeID, int SrcNodePort,
                  const void *DestNodeID, int DestNodePort,
                  const std::string &Attrs)
    {
        if (SrcNodePort  > 64) return;             // Eminating from truncated part?
        if (DestNodePort > 64) DestNodePort = 64;  // Targeting the truncated part?

        O << "\tNode" << SrcNodeID;
        if (SrcNodePort >= 0)
            O << ":s" << SrcNodePort;
        O << " -> Node" << DestNodeID;
        if (DestNodePort >= 0 && DTraits.hasEdgeDestLabels())
            O << ":d" << DestNodePort;

        if (!Attrs.empty())
            O << "[" << Attrs << "]";
        O << ";\n";
    }

    /// getOStream - Get the raw output stream into the graph file. Useful to
    /// write fancy things using addCustomGraphFeatures().
    std::ofstream &getOStream()
    {
        return O;
    }
};

template<typename GraphType>
std::ofstream &WriteGraph(std::ofstream &O, const GraphType &G,
                          bool ShortNames = false)
{
    // Start the graph emission process...
    GraphWriter<GraphType> W(O, G, ShortNames);

    // Emit the graph.
    W.writeGraph("");

    return O;
}

/// Writes graph into a provided @c Filename.
/// If @c Filename is empty, generates a random one.
/// \return The resulting filename, or an empty string if writing
/// failed.
template <typename GraphType>
std::string WriteGraph(const GraphType &G,
                       bool ShortNames = false,
                       std::string Filename = "")
{

    std::ofstream O(Filename);

    if (O.fail())
    {
        std::cerr << "error opening file '" << Filename << "' for writing!\n";
        O.close();
        return "";
    }

    SVF::WriteGraph(O, G, ShortNames);
    O.close();

    std::cerr << " done. \n";

    return Filename;
}

/// ViewGraph - Emit a dot graph, run 'dot', run gv on the postscript file,
/// then cleanup.  For use from the debugger.
///
template<typename GraphType>
void ViewGraph(const GraphType &G,const std::string& name,
               bool ShortNames = false,
               GraphProgram::Name Program = GraphProgram::DOT)
{
    SVF::WriteGraph(G, ShortNames);
}

} // end namespace llvm

#endif // LLVM_SUPPORT_GRAPHWRITER_H
