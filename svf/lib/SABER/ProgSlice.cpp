//===- ProgSlice.cpp -- Program slicing--------------------------------------//
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
 * ProgSlice.cpp
 *
 *  Created on: Apr 5, 2014
 *      Author: Yulei Sui
 */

#include "SABER/ProgSlice.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Compute path conditions for nodes on the backward slice
 * path condition of each node is calculated starting from root node (source)
 * Given a SVFGNode n, its path condition C is allocated (path_i stands for one of m program paths reaches n)
 *
 * C = \bigvee Guard(path_i),  0 < i < m
 * Guard(path_i) = \bigwedge VFGGuard(x,y),  suppose (x,y) are two SVFGNode nodes on path_i
 */
bool ProgSlice::AllPathReachableSolve()
{
    const SVFGNode* source = getSource();
    VFWorkList worklist;
    worklist.push(source);
    /// mark source node conditions to be true
    setVFCond(source,getTrueCond());

    while(!worklist.empty())
    {
        const SVFGNode* node = worklist.pop();
        setCurSVFGNode(node);
        Condition cond = getVFCond(node);
        for(SVFGNode::const_iterator it = node->OutEdgeBegin(), eit = node->OutEdgeEnd(); it!=eit; ++it)
        {
            const SVFGEdge* edge = (*it);
            const SVFGNode* succ = edge->getDstNode();
            if(inBackwardSlice(succ))
            {
                Condition vfCond;
                const SVFBasicBlock* nodeBB = getSVFGNodeBB(node);
                const SVFBasicBlock* succBB = getSVFGNodeBB(succ);
                /// clean up the control flow conditions for next round guard computation
                clearCFCond();

                if(edge->isCallVFGEdge())
                {
                    vfCond = ComputeInterCallVFGGuard(nodeBB,succBB, getCallSite(edge)->getParent());
                }
                else if(edge->isRetVFGEdge())
                {
                    vfCond = ComputeInterRetVFGGuard(nodeBB,succBB, getRetSite(edge)->getParent());
                }
                else
                    vfCond = ComputeIntraVFGGuard(nodeBB,succBB);

                Condition succPathCond = condAnd(cond, vfCond);
                if(setVFCond(succ,  condOr(getVFCond(succ), succPathCond) ))
                    worklist.push(succ);
            }

            DBOUT(DSaber, outs() << " node (" << node->getId()  <<
                  ") --> " << "succ (" << succ->getId() << ") condition: " << getVFCond(succ) << "\n");
        }
    }

    return isSatisfiableForAll();
}

/*!
 * Solve by computing disjunction of conditions from all sinks (e.g., memory leak)
 */
bool ProgSlice::isSatisfiableForAll()
{

    Condition guard = getFalseCond();
    for(SVFGNodeSetIter it = sinksBegin(), eit = sinksEnd(); it!=eit; ++it)
    {
        guard = condOr(guard,getVFCond(*it));
    }
    setFinalCond(guard);

    return pathAllocator->isAllPathReachable(guard);
}

/*!
 * Solve by analysing each pair of sinks (e.g., double free)
 */
bool ProgSlice::isSatisfiableForPairs()
{

    for(SVFGNodeSetIter it = sinksBegin(), eit = sinksEnd(); it!=eit; ++it)
    {
        for(SVFGNodeSetIter sit = it, esit = sinksEnd(); sit!=esit; ++sit)
        {
            if(*it == *sit)
                continue;
            Condition guard = condAnd(getVFCond(*sit),getVFCond(*it));
            if(!isEquivalentBranchCond(guard, getFalseCond()))
            {
                setFinalCond(guard);
                return false;
            }
        }
    }

    return true;
}

const CallICFGNode* ProgSlice::getCallSite(const SVFGEdge* edge) const
{
    assert(edge->isCallVFGEdge() && "not a call svfg edge?");
    if(const CallDirSVFGEdge* callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
        return getSVFG()->getCallSite(callEdge->getCallSiteId());
    else
        return getSVFG()->getCallSite(SVFUtil::cast<CallIndSVFGEdge>(edge)->getCallSiteId());
}
const CallICFGNode* ProgSlice::getRetSite(const SVFGEdge* edge) const
{
    assert(edge->isRetVFGEdge() && "not a return svfg edge?");
    if(const RetDirSVFGEdge* callEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
        return getSVFG()->getCallSite(callEdge->getCallSiteId());
    else
        return getSVFG()->getCallSite(SVFUtil::cast<RetIndSVFGEdge>(edge)->getCallSiteId());
}

void ProgSlice::evalFinalCond2Event(GenericBug::EventStack &eventStack) const
{
    NodeBS elems = pathAllocator->exactCondElem(finalCond);
    for(NodeBS::iterator it = elems.begin(), eit = elems.end(); it!=eit; ++it)
    {
        const SVFInstruction* tinst = pathAllocator->getCondInst(*it);
        if(pathAllocator->isNegCond(*it))
            eventStack.push_back(SVFBugEvent(
                                     SVFBugEvent::Branch|((((u32_t)false) << 4) & BRANCHFLAGMASK), tinst));
        else
            eventStack.push_back(SVFBugEvent(
                                     SVFBugEvent::Branch|((((u32_t)true) << 4) & BRANCHFLAGMASK), tinst));
    }
}

/*!
 * Evaluate Atoms of a condition
 * TODO: for now we only evaluate one path, evaluate every single path
 *
 * Atom -- a propositional valirable: a, b, c
 * Literal -- an atom or its negation: a, ~a
 * Clause  -- A disjunction of some literals: a \vee b
 * CNF formula -- a conjunction of some clauses:  (a \vee b ) \wedge (c \vee d)
 */
std::string ProgSlice::evalFinalCond() const
{
    std::string str;
    std::stringstream rawstr(str);
    Set<std::string> locations;
    NodeBS elems = pathAllocator->exactCondElem(finalCond);

    for(NodeBS::iterator it = elems.begin(), eit = elems.end(); it!=eit; ++it)
    {
        const SVFInstruction* tinst = pathAllocator->getCondInst(*it);
        if(pathAllocator->isNegCond(*it))
            locations.insert(tinst->getSourceLoc()+"|False");
        else
            locations.insert(tinst->getSourceLoc()+"|True");
    }

    /// print leak path after eliminating duplicated element
    for(Set<std::string>::iterator iter = locations.begin(), eiter = locations.end();
            iter!=eiter; ++iter)
    {
        rawstr << "\t\t  --> (" << *iter << ") \n";
    }

    return rawstr.str();
}

void ProgSlice::destroy()
{
}
