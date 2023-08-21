//===----- CFLGraphBuilder.h -- CFL Graph Builder --------------//
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
 * CFLGraphBuilder.h
 *
 *  Created on: May 22, 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFLGRAPHBUILDER_H_
#define INCLUDE_CFL_CFLGRAPHBUILDER_H_

#include "CFL/CFGrammar.h"
#include "Graphs/CFLGraph.h"
#include "Graphs/SVFG.h"

namespace SVF
{

enum class BuildDirection
{
    plain,
    bidirection,
};

/**
 * CFLGraphBuilder class is responsible for building CFL (Context-Free Language) graphs
 * from text files, dot files, json file or from memory graph.
 */
class CFLGraphBuilder
{
protected:
    /// Define Kind(Not contain attribute) and Symbol(May contain attribute) as types derived from CFGrammar
    /// to numerically represent label
    typedef CFGrammar::Kind Kind;
    typedef CFGrammar::Symbol Symbol;

    /// Maps to maintain mapping between labels and kinds
    Map<std::string, Kind> labelToKindMap;
    Map<Kind, std::string> kindToLabelMap;

    /// Map to maintain attributes associated with each kind
    Map<CFGrammar::Kind,  Set<CFGrammar::Attribute>> kindToAttrsMap;

    Kind current;
    CFLGraph *cflGraph;

    /// Method to add an attribute to a specific kind
    void addAttribute(CFGrammar::Kind kind, CFGrammar::Attribute attribute);

    /// build label and kind connect from the grammar
    void buildlabelToKindMap(GrammarBase *grammar);

    /// add src and dst node from file
    CFLNode* addGNode(u32_t NodeID);

    /// Method to build a CFL graph from a Text file
    CFLGraph* buildFromText(std::string fileName, GrammarBase *grammar, BuildDirection direction = BuildDirection::plain);

    /// Method to build a CFL graph from a Dot file
    CFLGraph *buildFromDot(std::string filename, GrammarBase *grammar, BuildDirection direction = BuildDirection::plain);

    /// Method to build a CFL graph from a Json file
    CFLGraph *buildFromJson(std::string filename, GrammarBase *grammar, BuildDirection direction = BuildDirection::plain);

public:
    /// Method to build a CFL graph by copying nodes and edges from any graph
    /// inherited from GenericGraph
    template<class N, class E>
    CFLGraph* build(GenericGraph<N,E>* graph, GrammarBase *grammar, BuildDirection direction = BuildDirection::plain);

    /// Method to build a CFL graph from external file
    CFLGraph* build(std::string fileName, GrammarBase *grammar, BuildDirection direction = BuildDirection::plain);

    /// @{
    /// Getter methods for accessing class variables

    /// Returns a reference to the map that associates string labels with their corresponding Kind
    Map<std::string, Kind>& getLabelToKindMap()
    {
        return this->labelToKindMap;
    }

    /// Returns a reference to the map that associates Kinds with their corresponding string labels
    Map<Kind, std::string>& getKindToLabelMap()
    {
        return this->kindToLabelMap;
    }

    /// Returns a reference to the map that associates Kinds with their corresponding attributes
    Map<CFGrammar::Kind,  Set<CFGrammar::Attribute>>& getKindToAttrsMap()
    {
        return this->kindToAttrsMap;
    }

    /// @}
};

/// AliasCFLGraphBuilder: a CFLGraphBuilder specialized for handling aliasing
class AliasCFLGraphBuilder : public CFLGraphBuilder
{
public:
    /// Builds a bidirectional CFL graph by copying nodes and edges from a const graph that inherits from GenericGraph
    CFLGraph* buildBigraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar);

    /// Builds a bidirectional CFL graph by copying nodes and edges from any graph that inherits from GenericGraph
    /// Transfers Load and Store to copy edge and address edge to construct PEG style CFLGraph
    CFLGraph* buildBiPEGgraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar, SVFIR* pag);

private:
    /// Connects VGep (Variable GEP)
    void connectVGep(CFLGraph *cflGraph,  ConstraintGraph *graph, ConstraintNode *src, ConstraintNode *dst, u32_t level, SVFIR* pag);

    /// Handles edges, with the exception of the GEP
    void addBiCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFGrammar::Kind label);

    /// Adds bidirectional GEP edges with attributes
    void addBiGepCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFGrammar::Attribute attri);
};

/// VFCFLGraphBuilder: a CFLGraphBuilder specialized for handling value-flow
class VFCFLGraphBuilder : public CFLGraphBuilder
{
public:
    /// Builds a bidirectional CFL graph by copying nodes and edges from a const graph that inherits from SVFG
    CFLGraph* buildBigraph(SVFG *graph, Kind startKind, GrammarBase *grammar);

    /// Builds a bidirectional CFL graph by copying nodes and edges from any graph that inherits from GenericGraph
    /// Transfers Load and Store to copy edge and address edge to construct PEG style CFLGraph
    CFLGraph* buildBiPEGgraph(ConstraintGraph *graph, Kind startKind, GrammarBase *grammar, SVFIR* pag);

private:
    /// Connects VGep (Variable GEP)
    void connectVGep(CFLGraph *cflGraph,  ConstraintGraph *graph, ConstraintNode *src, ConstraintNode *dst, u32_t level, SVFIR* pag);

    /// Handles edges, with the exception of the GEP
    void addBiCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFGrammar::Kind label);

    /// Adds bidirectional GEP edges with attributes
    void addBiGepCFLEdge(CFLGraph *cflGraph,  ConstraintNode* src, ConstraintNode* dst, CFGrammar::Attribute attri);
};


}// SVF

#endif /* INCLUDE_CFL_CFLGRAPHBUILDER_H_*/
