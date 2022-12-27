//===- DDAClient.h -- Clients of demand-driven analysis-------------//
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
 * @file: DDAClient.h
 * @author: yesen
 * @date: 4 Feb 2015
 *
 * LICENSE
 *
 */


#ifndef DDACLIENT_H_
#define DDACLIENT_H_

#include "SVFIR/SVFIR.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/SVFG.h"
#include "SVFIR/SVFValue.h"

namespace SVF
{

/**
 * General DDAClient which queries all top level pointers by default.
 */
class DDAClient
{
public:
    DDAClient(SVFModule* mod) : pag(nullptr), module(mod), curPtr(0), solveAll(true) {}

    virtual ~DDAClient() {}

    virtual inline void initialise(SVFModule*) {}

    /// Collect candidate pointers for query.
    virtual inline OrderedNodeSet& collectCandidateQueries(SVFIR* p)
    {
        setPAG(p);
        if (solveAll)
            candidateQueries = pag->getAllValidPtrs();
        else
        {
            for (OrderedNodeSet::iterator it = userInput.begin(), eit = userInput.end(); it != eit; ++it)
                addCandidate(*it);
        }
        return candidateQueries;
    }
    /// Get candidate queries
    inline const OrderedNodeSet& getCandidateQueries() const
    {
        return candidateQueries;
    }

    /// Call back used by DDAVFSolver.
    virtual inline void handleStatement(const SVFGNode*, NodeID) {}
    /// Set SVFIR graph.
    inline void setPAG(SVFIR* g)
    {
        pag = g;
    }
    /// Set the pointer being queried.
    void setCurrentQueryPtr(NodeID ptr)
    {
        curPtr = ptr;
    }
    /// Set pointer to be queried by DDA analysis.
    void setQuery(NodeID ptr)
    {
        userInput.insert(ptr);
        solveAll = false;
    }
    /// Get LLVM module
    inline SVFModule* getModule() const
    {
        return module;
    }
    virtual void answerQueries(PointerAnalysis* pta);

    virtual inline void performStat(PointerAnalysis*) {}

    virtual inline void collectWPANum(SVFModule*) {}
protected:
    void addCandidate(NodeID id)
    {
        if (pag->isValidTopLevelPtr(pag->getGNode(id)))
            candidateQueries.insert(id);
    }

    SVFIR*   pag;					///< SVFIR graph used by current DDA analysis
    SVFModule* module;		///< LLVM module
    NodeID curPtr;				///< current pointer being queried
    OrderedNodeSet candidateQueries;	///< store all candidate pointers to be queried

private:
    OrderedNodeSet userInput;           ///< User input queries
    bool solveAll;				///< TRUE if all top level pointers are being queried
};


/**
 * DDA client with function pointers as query candidates.
 */
class FunptrDDAClient : public DDAClient
{
private:
    typedef OrderedMap<NodeID,const CallICFGNode*> VTablePtrToCallSiteMap;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
public:
    FunptrDDAClient(SVFModule* module) : DDAClient(module) {}
    ~FunptrDDAClient() {}

    /// Only collect function pointers as query candidates.
    virtual OrderedNodeSet& collectCandidateQueries(SVFIR* p);
    virtual void performStat(PointerAnalysis* pta);
};



/**
 * DDA client with function pointers as query candidates.
 */
class AliasDDAClient : public DDAClient
{

public:
    typedef OrderedSet<const PAGNode*> PAGNodeSet;

    AliasDDAClient(SVFModule* module) : DDAClient(module) {}
    ~AliasDDAClient() {}

    /// Only collect function pointers as query candidates.
    virtual OrderedNodeSet& collectCandidateQueries(SVFIR* pag);

    virtual void performStat(PointerAnalysis* pta);

private:
    typedef OrderedMap<NodeID,const CallICFGNode*> VTablePtrToCallSiteMap;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
    PAGNodeSet loadSrcNodes;
    PAGNodeSet storeDstNodes;
    PAGNodeSet gepSrcNodes;
};

} // End namespace SVF

#endif /* DDACLIENT_H_ */
