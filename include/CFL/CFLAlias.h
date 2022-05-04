//===----- CFLAlias.h -- CFL Alias Analysis Client--------------//
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
 * CFLAlias.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */


#include "CFL/CFLSolver.h"
#include "CFL/CFGNormalizer.h"
#include "CFL/GrammarBuilder.h"
#include "MemoryModel/PointerAnalysis.h"
#include "Graphs/ConsG.h"
#include "Util/Options.h"

namespace SVF
{

class CFLAlias : public PointerAnalysis
{

public:
    CFLAlias(SVFIR* ir) : PointerAnalysis(ir, PointerAnalysis::CFLFICI_WPA), svfir(ir), graph(nullptr), grammar(nullptr), solver(nullptr)
    {
    }

    /// Destructor
    virtual ~CFLAlias()
    {
        delete graph;
        delete grammar;
        delete solver;
    }

    /// Start Analysis here (main part of pointer analysis).
    virtual void analyze()
    {
        PointerAnalysis::initialize();
        GrammarBuilder * gReader = new GrammarBuilder(Options::GrammarFilename);
        CFGNormalizer *normalizer = new CFGNormalizer();

        graph = new CFLGraph();
        if (Options::GraphIsFromDot == false)
        {
            // Maybe could put in SVFIR Class
            // In memory Graph does not have string type label
            Map<std::string, SVF::CFLGraph::Symbol> ConstMap =  {{"Addr",0}, {"Copy", 1},{"Store", 2},{"Load", 3},{"Gep", 4},{"Vgep", 5},{"Addrbar",6}, {"Copybar", 7},{"Storebar", 8},{"Loadbar", 9},{"Gepbar", 10},{"Vgepbar", 11}};
            ConstraintGraph *consCG = new ConstraintGraph(svfir);
            svfir->dump("SVFIR");
            // Can be put in build, copy from general graph
            graph->label2SymMap = ConstMap;
            graph->buildBigraph(consCG);
            graph->dump("PAG");
            delete consCG;
            GrammarBase *generalGrammar = gReader->build(&ConstMap);
            grammar = normalizer->normalize(generalGrammar);
            graph->setMap(&grammar->terminals, &grammar->nonterminals);
        }
        else
        {
            GrammarBase *generalGrammar = gReader->build();
            grammar = normalizer->normalize(generalGrammar);
            graph->setMap(&grammar->terminals, &grammar->nonterminals);
            graph->buildFromDot(Options::InputFilename);
        }
        graph->startSymbol = grammar->startSymbol;
        solver = new CFLSolver(graph, grammar);
        solver->solve();
        graph->dump("map");
        PointerAnalysis::finalize();
    }

    /// Interface exposed to users of our pointer analysis, given Value infos
    virtual AliasResult alias(const Value* v1, const Value* v2)
    {
        NodeID n1 = svfir->getValueNode(v1);
        NodeID n2 = svfir->getValueNode(v2);
        return alias(n1,n2);
    }

    /// Interface exposed to users of our pointer analysis, given PAGNodeID
    virtual AliasResult alias(NodeID node1, NodeID node2)
    {
        /// TODO:: Fix the edge label for a reachable alias relation if it is not 1;
        if(graph->hasEdge(graph->getGNode(node1), graph->getGNode(node2), graph->startSymbol))
            return AliasResult::MayAlias;
        else
            return AliasResult::NoAlias;
    }

    /// Get points-to targets of a pointer.
    virtual const PointsTo& getPts(NodeID ptr)
    {
        /// Check Outgoing edge Dst of ptr
        //PointsTo * ps;
        abort(); // to be implemented
    }

    /// Given an object, get all the nodes having whose pointsto contains the object
    virtual const NodeSet& getRevPts(NodeID nodeId)
    {
        /// Check Outgoing flowtobar edge dst of ptr
        abort(); // to be implemented
    }

private:
    SVFIR* svfir;
    CFLGraph* graph;
    CFLGrammar* grammar;
    CFLSolver *solver;
};

}
