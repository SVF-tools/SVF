//===----- CFLSolver.h -- Context-free language reachability solver--------------//
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
 * CFLSolver.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "Graphs/CFLGraph.h"
#include "CFL/CFLGrammar.h"
#include "Util/WorkList.h"


namespace SVF
{

class CFLSolver
{

public:
    /// Define worklist
    typedef FIFOWorkList<const CFLEdge*> WorkList;
    typedef CFLGrammar::Production Production;
    typedef CFLGrammar::Symbol Symbol;

    CFLSolver(CFLGraph* _graph, CFLGrammar* _grammar): graph(_graph), grammar(_grammar)
    {
    }

    ~CFLSolver()
    {
        delete graph;
        delete grammar;
    }

    /// Start solving
    void solve();

    /// Return CFL Graph
    inline const CFLGraph* getGraph() const
    {
        return graph;
    }

    /// Return CFL Grammar
    inline const CFLGrammar* getGrammar() const
    {
        return grammar;
    }
    inline bool pushIntoWorklist(const CFLEdge* item)
    {
        return worklist.push(item);
    }
    inline bool isWorklistEmpty()
    {
        return worklist.empty();
    }

protected:
    /// Worklist operations
    //@{
    inline const CFLEdge* popFromWorklist()
    {
        return worklist.pop();
    }

    inline bool isInWorklist(const CFLEdge* item)
    {
        return worklist.find(item);
    }
    //@}

private:
    CFLGraph* graph;
    CFLGrammar* grammar;
    /// Worklist for resolution
    WorkList worklist;

};

}