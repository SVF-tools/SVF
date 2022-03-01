//===- ICFGStat.h ----------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFGStat.h
 *
 *  Created on: 12Sep.,2018
 *      Author: yulei
 */

#ifndef INCLUDE_UTIL_ICFGSTAT_H_
#define INCLUDE_UTIL_ICFGSTAT_H_

#include "MemoryModel/PTAStat.h"
#include "Graphs/ICFG.h"
#include <iomanip>

namespace SVF
{

class ICFGStat : public PTAStat
{

private:
    ICFG *icfg;
    int numOfNodes;
    int numOfCallNodes;
    int numOfRetNodes;
    int numOfEntryNodes;
    int numOfExitNodes;
    int numOfIntraNodes;

    int numOfEdges;
    int numOfCallEdges;
    int numOfRetEdges;
    int numOfIntraEdges;

public:
    typedef Set<const ICFGNode *> ICFGNodeSet;

    ICFGStat(ICFG *cfg) : PTAStat(nullptr), icfg(cfg)
    {
        numOfNodes = 0;
        numOfCallNodes = 0;
        numOfRetNodes = 0;
        numOfEntryNodes = 0;
        numOfExitNodes = 0;
        numOfIntraNodes = 0;

        numOfEdges = 0;
        numOfCallEdges = 0;
        numOfRetEdges = 0;
        numOfIntraEdges = 0;

    }

    void performStat()
    {

        countStat();

        PTNumStatMap["ICFGNode"] = numOfNodes;
        PTNumStatMap["IntraICFGNode"] = numOfIntraNodes;
        PTNumStatMap["CallICFGNode"] = numOfCallNodes;
        PTNumStatMap["RetICFGNode"] = numOfRetNodes;
        PTNumStatMap["FunEntryICFGNode"] = numOfEntryNodes;
        PTNumStatMap["FunExitICFGNode"] = numOfExitNodes;

        PTNumStatMap["ICFGEdge"] = numOfEdges;
        PTNumStatMap["CallCFGEdge"] = numOfCallEdges;
        PTNumStatMap["RetCFGEdge"] = numOfRetEdges;
        PTNumStatMap["IntraCFGEdge"] = numOfIntraEdges;

        printStat("ICFG Stat");
    }

    void performStatforIFDS()
    {

        countStat();
        PTNumStatMap["ICFGNode(N)"] = numOfNodes;
        PTNumStatMap["CallICFGNode(Call)"] = numOfCallNodes;
        PTNumStatMap["ICFGEdge(E)"] = numOfEdges;
        printStat("IFDS Stat");
    }

    void countStat()
    {
        ICFG::ICFGNodeIDToNodeMapTy::iterator it = icfg->begin();
        ICFG::ICFGNodeIDToNodeMapTy::iterator eit = icfg->end();
        for (; it != eit; ++it)
        {
            numOfNodes++;

            ICFGNode *node = it->second;

            if (SVFUtil::isa<IntraICFGNode>(node))
                numOfIntraNodes++;
            else if (SVFUtil::isa<CallICFGNode>(node))
                numOfCallNodes++;
            else if (SVFUtil::isa<RetICFGNode>(node))
                numOfRetNodes++;
            else if (SVFUtil::isa<FunEntryICFGNode>(node))
                numOfEntryNodes++;
            else if (SVFUtil::isa<FunExitICFGNode>(node))
                numOfExitNodes++;


            ICFGEdge::ICFGEdgeSetTy::iterator edgeIt =
                it->second->OutEdgeBegin();
            ICFGEdge::ICFGEdgeSetTy::iterator edgeEit =
                it->second->OutEdgeEnd();
            for (; edgeIt != edgeEit; ++edgeIt)
            {
                const ICFGEdge *edge = *edgeIt;
                numOfEdges++;
                if (edge->isCallCFGEdge())
                    numOfCallEdges++;
                else if (edge->isRetCFGEdge())
                    numOfRetEdges++;
                else if (edge->isIntraCFGEdge())
                    numOfIntraEdges++;
            }
        }
    }

    void printStat(string statname)
    {

        SVFUtil::outs() << "\n************ " << statname << " ***************\n";
        SVFUtil::outs().flags(std::ios::left);
        unsigned field_width = 20;
        for(NUMStatMap::iterator it = PTNumStatMap.begin(), eit = PTNumStatMap.end(); it!=eit; ++it)
        {
            // format out put with width 20 space
            SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
        }
        PTNumStatMap.clear();
        SVFUtil::outs().flush();
    }
};

} // End namespace SVF

#endif /* INCLUDE_UTIL_ICFGSTAT_H_ */

