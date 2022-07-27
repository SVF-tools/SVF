//===----- CFLSolver.cpp -- Context-free language reachability solver--------------//
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
 * CFLSolver.cpp
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "CFL/CFLSolver.h"

using namespace SVF;

void CFLSolver::solve()
{
    /// initial worklist
    for(auto it = graph->begin(); it!= graph->end(); it++)
    {
        for(const CFLEdge* edge : (*it).second->getOutEdges())
        {
            pushIntoWorklist(edge);
        }
    }

    /// Foreach production X -> epsilon
    ///     add X(i,i) if not exist to E and to worklist
    for(const Production& prod : grammar->getEpsilonProds())
    {
        for(auto it = graph->begin(); it!= graph->end(); it++)
        {
            Symbol X = grammar->getLHSSymbol(prod);
            CFLNode* i = (*it).second;
            if(const CFLEdge* edge = graph->addCFLEdge(i, i, X))
            {
                pushIntoWorklist(edge);
            }
        }
    }

    while(!isWorklistEmpty())
    {
        /// Select and remove an edge Y(i,j) from worklist
        const CFLEdge* Y_edge = popFromWorklist();
        CFLNode* i = Y_edge->getSrcNode();
        CFLNode* j = Y_edge->getDstNode();

        /// For each production X -> Y
        ///     add X(i,j) if not exist to E and to worklist
        Symbol Y = Y_edge->getEdgeKind();
        if (grammar->hasProdsFromSingleRHS(Y))
            for(const Production& prod : grammar->getProdsFromSingleRHS(Y))
            {
                Symbol X = grammar->getLHSSymbol(prod);
                if(const CFLEdge* newEdge = graph->addCFLEdge(i, j, X))
                {
                    pushIntoWorklist(newEdge);
                }
            }

        /// For each production X -> Y Z
        /// Foreach outgoing edge Z(j,k) from node j do
        ///     add X(i,k) if not exist to E and to worklist
        if (grammar->hasProdsFromFirstRHS(Y))
            for(const Production& prod : grammar->getProdsFromFirstRHS(Y))
            {
                Symbol X = grammar->getLHSSymbol(prod);
                for(const CFLEdge* Z_edge : j->getOutEdges())
                {
                    if (Z_edge->getEdgeKind() == grammar->getSecondRHSSymbol(prod))
                    {
                        CFLNode* k = Z_edge->getDstNode();
                        if(const CFLEdge* newEdge = graph->addCFLEdge(i, k, X))
                        {
                            pushIntoWorklist(newEdge);
                        }
                    }
                }
            }

        /// For each production X -> Z Y
        /// Foreach incoming edge Z(k,i) to node i do
        ///     add X(k,j) if not exist to E and to worklist
        if(grammar->hasProdsFromSecondRHS(Y))
            for(const Production& prod : grammar->getProdsFromSecondRHS(Y))
            {
                Symbol X = grammar->getLHSSymbol(prod);
                for(const CFLEdge* Z_edge : i->getInEdges())
                {
                    if(Z_edge->getEdgeKind() == grammar->getFirstRHSSymbol(prod))
                    {
                        CFLNode* k = Z_edge->getSrcNode();
                        if(const CFLEdge* newEdge = graph->addCFLEdge(k, j, X))
                        {
                            pushIntoWorklist(newEdge);
                        }
                    }
                }
            }
    }
}