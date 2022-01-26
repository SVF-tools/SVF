//===- SrcSnkDDA.cpp -- Source-sink analyzer --------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SrcSnkDDA.cpp
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */


#include "Util/Options.h"
#include "SABER/SrcSnkDDA.h"
#include "Graphs/SVFGStat.h"
#include "SVF-FE/SVFIRBuilder.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

using namespace SVF;
using namespace SVFUtil;

/// Initialize analysis
void SrcSnkDDA::initialize(SVFModule* module)
{
	SVFIRBuilder builder;
	SVFIR* pag = builder.build(module);

    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    if(Options::SABERFULLSVFG)
        svfg =  memSSA.buildFullSVFG(ander);
    else
        svfg =  memSSA.buildPTROnlySVFG(ander);
    setGraph(memSSA.getSVFG());
    ptaCallGraph = ander->getPTACallGraph();
    //AndersenWaveDiff::releaseAndersenWaveDiff();
    /// allocate control-flow graph branch conditions
    getPathAllocator()->allocate(getPAG()->getModule());

    initSrcs();
    initSnks();
}

void SrcSnkDDA::analyze(SVFModule* module)
{

    initialize(module);

    ContextCond::setMaxCxtLen(Options::CxtLimit);

    for (SVFGNodeSetIter iter = sourcesBegin(), eiter = sourcesEnd();
            iter != eiter; ++iter)
    {
        setCurSlice(*iter);

        DBOUT(DGENERAL, outs() << "Analysing slice:" << (*iter)->getId() << ")\n");
        ContextCond cxt;
        DPIm item((*iter)->getId(),cxt);
        forwardTraverse(item);

        /// do not consider there is bug when reaching a global SVFGNode
        /// if we touch a global, then we assume the client uses this memory until the program exits.
        if (getCurSlice()->isReachGlobal())
        {
            DBOUT(DSaber, outs() << "Forward analysis reaches globals for slice:" << (*iter)->getId() << ")\n");
        }
        else
        {
            DBOUT(DSaber, outs() << "Forward process for slice:" << (*iter)->getId() << " (size = " << getCurSlice()->getForwardSliceSize() << ")\n");

            for (SVFGNodeSetIter sit = getCurSlice()->sinksBegin(), esit =
                        getCurSlice()->sinksEnd(); sit != esit; ++sit)
            {
                ContextCond cxt;
                DPIm item((*sit)->getId(),cxt);
                backwardTraverse(item);
            }

            DBOUT(DSaber, outs() << "Backward process for slice:" << (*iter)->getId() << " (size = " << getCurSlice()->getBackwardSliceSize() << ")\n");

            if(Options::DumpSlice)
                annotateSlice(_curSlice);

            if(_curSlice->AllPathReachableSolve())
                _curSlice->setAllReachable();

            DBOUT(DSaber, outs() << "Guard computation for slice:" << (*iter)->getId() << ")\n");
        }

        reportBug(getCurSlice());
    }

    finalize();
}


/*!
 * determine whether a SVFGNode n is in a allocation wrapper function,
 * if so, return all SVFGNodes which receive the value of node n
 */
bool SrcSnkDDA::isInAWrapper(const SVFGNode* src, CallSiteSet& csIdSet)
{

    bool reachFunExit = false;

    WorkList worklist;
    worklist.push(src);
    SVFGNodeBS visited;
    u32_t step = 0;
    while (!worklist.empty())
    {
        const SVFGNode* node  = worklist.pop();

        if(visited.test(node->getId())==0)
            visited.set(node->getId());
        else
            continue;
        // reaching maximum steps when traversing on SVFG to identify a memory allocation wrapper
        if (step++ > Options::MaxStepInWrapper)
            return false;

        for (SVFGNode::const_iterator it = node->OutEdgeBegin(), eit =
                    node->OutEdgeEnd(); it != eit; ++it)
        {
            const SVFGEdge* edge = (*it);
            //assert(edge->isDirectVFGEdge() && "the edge should always be direct VF");
            // if this is a call edge
            if(edge->isCallDirectVFGEdge())
            {
                return false;
            }
            // if this is a return edge
            else if(edge->isRetDirectVFGEdge())
            {
                reachFunExit = true;
                csIdSet.insert(getSVFG()->getCallSite(SVFUtil::cast<RetDirSVFGEdge>(edge)->getCallSiteId()));
            }
            // (1) an intra direct edge, we will keep tracking
            // (2) an intra indirect edge, we only track if the succ SVFGNode is a load, which means we only track one level store-load pair .
            // (3) do not track for all other interprocedural edges.
            else
            {
                const SVFGNode* succ = edge->getDstNode();
                if(SVFUtil::isa<IntraDirSVFGEdge>(edge)){
                    if (SVFUtil::isa<CopySVFGNode>(succ) || SVFUtil::isa<GepSVFGNode>(succ)
                        || SVFUtil::isa<PHISVFGNode>(succ) || SVFUtil::isa<FormalRetSVFGNode>(succ)
                        || SVFUtil::isa<ActualRetSVFGNode>(succ) || SVFUtil::isa<StoreSVFGNode>(succ))
                    {
                        worklist.push(succ);
                    }
                }
                else if(SVFUtil::isa<IntraIndSVFGEdge>(edge))
                {
                    if(SVFUtil::isa<LoadSVFGNode>(succ))
                    {
                        worklist.push(succ);
                    }
                }
                else
                    return false;
            }
        }
    }
    if(reachFunExit)
        return true;
    else
        return false;
}


/*!
 * Propagate information forward by matching context
 */
void SrcSnkDDA::FWProcessOutgoingEdge(const DPIm& item, SVFGEdge* edge)
{
    DBOUT(DSaber,outs() << "\n##processing source: " << getCurSlice()->getSource()->getId() <<" forward propagate from (" << edge->getSrcID());

    // for indirect SVFGEdge, the propagation should follow the def-use chains
    // points-to on the edge indicate whether the object of source node can be propagated

    const SVFGNode* dstNode = edge->getDstNode();
    DPIm newItem(dstNode->getId(),item.getContexts());

    /// handle globals here
    if(isGlobalSVFGNode(dstNode) || getCurSlice()->isReachGlobal())
    {
        getCurSlice()->setReachGlobal();
        return;
    }


    /// perform context sensitive reachability
    // push context for calling
    if (edge->isCallVFGEdge())
    {
        CallSiteID csId = 0;
        if(const CallDirSVFGEdge* callEdge = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
            csId = callEdge->getCallSiteId();
        else
            csId = SVFUtil::cast<CallIndSVFGEdge>(edge)->getCallSiteId();

        newItem.pushContext(csId);
        DBOUT(DSaber, outs() << " push cxt [" << csId << "] ");
    }
    // match context for return
    else if (edge->isRetVFGEdge())
    {
        CallSiteID csId = 0;
        if(const RetDirSVFGEdge* callEdge = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
            csId = callEdge->getCallSiteId();
        else
            csId = SVFUtil::cast<RetIndSVFGEdge>(edge)->getCallSiteId();

        if (newItem.matchContext(csId) == false)
        {
            DBOUT(DSaber, outs() << "-|-\n");
            return;
        }
        DBOUT(DSaber, outs() << " pop cxt [" << csId << "] ");
    }

    /// whether this dstNode has been visited or not
    if(forwardVisited(dstNode,newItem))
    {
        DBOUT(DSaber,outs() << " node "<< dstNode->getId() <<" has been visited\n");
        return;
    }
    else
        addForwardVisited(dstNode, newItem);

    if(pushIntoWorklist(newItem))
        DBOUT(DSaber,outs() << " --> " << edge->getDstID() << ", cxt size: " << newItem.getContexts().cxtSize() <<")\n");

}

/*!
 * Propagate information backward without matching context, as forward analysis already did it
 */
void SrcSnkDDA::BWProcessIncomingEdge(const DPIm&, SVFGEdge* edge)
{
    DBOUT(DSaber,outs() << "backward propagate from (" << edge->getDstID() << " --> " << edge->getSrcID() << ")\n");
    const SVFGNode* srcNode = edge->getSrcNode();
    if(backwardVisited(srcNode))
        return;
    else
        addBackwardVisited(srcNode);

    ContextCond cxt;
    DPIm newItem(srcNode->getId(), cxt);
    pushIntoWorklist(newItem);
}

/// Set current slice
void SrcSnkDDA::setCurSlice(const SVFGNode* src)
{
    if(_curSlice!=nullptr)
    {
        delete _curSlice;
        _curSlice = nullptr;
        clearVisitedMap();
    }

    _curSlice = new ProgSlice(src,getPathAllocator(), getSVFG());
}

void SrcSnkDDA::annotateSlice(ProgSlice* slice)
{
    getSVFG()->getStat()->addToSources(slice->getSource());
    for(SVFGNodeSetIter it = slice->sinksBegin(), eit = slice->sinksEnd(); it!=eit; ++it )
        getSVFG()->getStat()->addToSinks(*it);
    for(SVFGNodeSetIter it = slice->forwardSliceBegin(), eit = slice->forwardSliceEnd(); it!=eit; ++it )
        getSVFG()->getStat()->addToForwardSlice(*it);
    for(SVFGNodeSetIter it = slice->backwardSliceBegin(), eit = slice->backwardSliceEnd(); it!=eit; ++it )
        getSVFG()->getStat()->addToBackwardSlice(*it);
}

void SrcSnkDDA::dumpSlices()
{

    if(Options::DumpSlice)
        const_cast<SVFG*>(getSVFG())->dump("Slice",true);
}

void SrcSnkDDA::printBDDStat()
{

    outs() << "BDD Mem usage: " << getPathAllocator()->getMemUsage() << "\n";
    outs() << "BDD Number: " << getPathAllocator()->getCondNum() << "\n";
}
