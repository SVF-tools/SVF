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

#include "Graphs/PAG.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/SVFG.h"
#include "Util/BasicTypes.h"

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
    virtual inline OrderedNodeSet& collectCandidateQueries(PAG* p)
    {
        setPAG(p);
        if (solveAll)
            candidateQueries = pag->getAllValidPtrs();
        else
        {
            for (const auto &it: userInput)
                addCandidate(it);
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
    /// Set PAG graph.
    inline void setPAG(PAG* g)
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
        if (pag->isValidTopLevelPtr(pag->getPAGNode(id)))
            candidateQueries.insert(id);
    }

    PAG*   pag;					///< PAG graph used by current DDA analysis
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
    using VTablePtrToCallSiteMap = OrderedMap<NodeID, const CallBlockNode *>;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
public:
    FunptrDDAClient(SVFModule* module) : DDAClient(module) {}
    ~FunptrDDAClient() {}

    /// Only collect function pointers as query candidates.
    OrderedNodeSet& collectCandidateQueries(PAG* p) override;
    void performStat(PointerAnalysis* pta) override;
};



/**
 * DDA client with function pointers as query candidates.
 */
class AliasDDAClient : public DDAClient
{

public:
    using PAGNodeSet = OrderedSet<const PAGNode *>;

    AliasDDAClient(SVFModule* module) : DDAClient(module) {}
    ~AliasDDAClient() {}

    /// Only collect function pointers as query candidates.
    OrderedNodeSet& collectCandidateQueries(PAG* pag) override;

    void performStat(PointerAnalysis* pta) override;

private:
    using VTablePtrToCallSiteMap = OrderedMap<NodeID, const CallBlockNode *>;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
    PAGNodeSet loadSrcNodes;
    PAGNodeSet storeDstNodes;
    PAGNodeSet gepSrcNodes;
};

} // End namespace SVF

#endif /* DDACLIENT_H_ */
