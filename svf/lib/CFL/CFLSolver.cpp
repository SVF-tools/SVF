//===----- CFLSolver.cpp -- Context-free language reachability solver--------------//
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
 * CFLSolver.cpp
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "CFL/CFLSolver.h"

using namespace SVF;

double CFLSolver::numOfChecks = 0;

void CFLSolver::initialize()
{
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
}

void CFLSolver::processCFLEdge(const CFLEdge* Y_edge)
{
    CFLNode* i = Y_edge->getSrcNode();
    CFLNode* j = Y_edge->getDstNode();

    /// For each production X -> Y
    ///     add X(i,j) if not exist to E and to worklist
    Symbol Y = Y_edge->getEdgeKind();
    if (grammar->hasProdsFromSingleRHS(Y))
        for(const Production& prod : grammar->getProdsFromSingleRHS(Y))
        {
            Symbol X = grammar->getLHSSymbol(prod);
            numOfChecks++;
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
            for(const CFLEdge* Z_edge : j->getOutEdgeWithTy(grammar->getSecondRHSSymbol(prod)))
            {
                CFLNode* k = Z_edge->getDstNode();
                numOfChecks++;
                if(const CFLEdge* newEdge = graph->addCFLEdge(i, k, X))
                {
                    pushIntoWorklist(newEdge);
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
            for(const CFLEdge* Z_edge : i->getInEdgeWithTy(grammar->getFirstRHSSymbol(prod)))
            {
                CFLNode* k = Z_edge->getSrcNode();
                numOfChecks++;
                if(const CFLEdge* newEdge = graph->addCFLEdge(k, j, X))
                {
                    pushIntoWorklist(newEdge);
                }
            }
        }
}


void CFLSolver::solve()
{
    /// initial worklist
    initialize();

    while(!isWorklistEmpty())
    {
        /// Select and remove an edge Y(i,j) from worklist
        const CFLEdge* Y_edge = popFromWorklist();
        processCFLEdge(Y_edge);
    }
}

void POCRSolver::buildCFLData()
{
    for (CFLEdge* edge: graph->getCFLEdges())
        addEdge(edge->getSrcID(), edge->getDstID(), edge->getEdgeKind());
}

void POCRSolver::processCFLEdge(const CFLEdge* Y_edge)
{
    CFLNode* i = Y_edge->getSrcNode();
    CFLNode* j = Y_edge->getDstNode();

    /// For each production X -> Y
    ///     add X(i,j) if not exist to E and to worklist
    Symbol Y = Y_edge->getEdgeKind();
    if (grammar->hasProdsFromSingleRHS(Y))
        for(const Production& prod : grammar->getProdsFromSingleRHS(Y))
        {
            Symbol X = grammar->getLHSSymbol(prod);
            numOfChecks++;
            if (addEdge(i->getId(), j->getId(), X))
            {
                const CFLEdge* newEdge = graph->addCFLEdge(Y_edge->getSrcNode(), Y_edge->getDstNode(), X);
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
            NodeBS diffDsts = addEdges(i->getId(), getSuccMap(j->getId())[grammar->getSecondRHSSymbol(prod)], X);
            numOfChecks += getSuccMap(j->getId())[grammar->getSecondRHSSymbol(prod)].count();
            for (NodeID diffDst: diffDsts)
            {
                const CFLEdge* newEdge = graph->addCFLEdge(i, graph->getGNode(diffDst), X);
                pushIntoWorklist(newEdge);
            }
        }

    /// For each production X -> Z Y
    /// Foreach incoming edge Z(k,i) to node i do
    ///     add X(k,j) if not exist to E and to worklist
    if(grammar->hasProdsFromSecondRHS(Y))
        for(const Production& prod : grammar->getProdsFromSecondRHS(Y))
        {
            Symbol X = grammar->getLHSSymbol(prod);
            NodeBS diffSrcs = addEdges(getPredMap(i->getId())[grammar->getFirstRHSSymbol(prod)], j->getId(), X);
            numOfChecks += getPredMap(i->getId())[grammar->getFirstRHSSymbol(prod)].count();
            for (NodeID diffSrc: diffSrcs)
            {
                const CFLEdge* newEdge = graph->addCFLEdge(graph->getGNode(diffSrc), j, X);
                pushIntoWorklist(newEdge);
            }
        }
}

void POCRSolver::initialize()
{
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
        for(auto IDMap : getSuccMap())
        {
            Symbol X = grammar->getLHSSymbol(prod);
            if (addEdge(IDMap.first, IDMap.first, X))
            {
                CFLNode* i = graph->getGNode(IDMap.first);
                const CFLEdge* newEdge = graph->addCFLEdge(i, i, X);
                pushIntoWorklist(newEdge);
            }
        }
    }
}