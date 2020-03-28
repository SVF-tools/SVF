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


/**
 * General DDAClient which queries all top level pointers by default.
 */
class DDAClient {
public:
    DDAClient(SVFModule* mod) : pag(NULL), module(mod), curPtr(0), solveAll(true) {}

    virtual ~DDAClient() {}

    virtual inline void initialise(SVFModule* module) {}

    /// Collect candidate pointers for query.
    virtual inline NodeSet& collectCandidateQueries(PAG* p) {
        setPAG(p);
        if (solveAll)
            candidateQueries = pag->getAllValidPtrs();
        else {
            for (NodeSet::iterator it = userInput.begin(), eit = userInput.end(); it != eit; ++it)
                addCandidate(*it);
        }
        return candidateQueries;
    }
    /// Get candidate queries
    inline const NodeSet& getCandidateQueries() const {
        return candidateQueries;
    }

    /// Call back used by DDAVFSolver.
    virtual inline void handleStatement(const SVFGNode* stmt, NodeID var) {}
    /// Set PAG graph.
    inline void setPAG(PAG* g) {
        pag = g;
    }
    /// Set the pointer being queried.
    void setCurrentQueryPtr(NodeID ptr) {
        curPtr = ptr;
    }
    /// Set pointer to be queried by DDA analysis.
    void setQuery(NodeID ptr) {
        userInput.insert(ptr);
        solveAll = false;
    }
    /// Get LLVM module
    inline SVFModule* getModule() const {
        return module;
    }
    virtual void answerQueries(PointerAnalysis* pta);

    virtual inline void performStat(PointerAnalysis* pta) {}

    virtual inline void collectWPANum(SVFModule* mod) {}
protected:
    void addCandidate(NodeID id) {
        if (pag->isValidTopLevelPtr(pag->getPAGNode(id)))
            candidateQueries.insert(id);
    }

    PAG*   pag;					///< PAG graph used by current DDA analysis
    SVFModule* module;		///< LLVM module
    NodeID curPtr;				///< current pointer being queried
    NodeSet candidateQueries;	///< store all candidate pointers to be queried

private:
    NodeSet userInput;           ///< User input queries
    bool solveAll;				///< TRUE if all top level pointers are being queried
};


/**
 * DDA client with function pointers as query candidates.
 */
class FunptrDDAClient : public DDAClient {
private:
    typedef std::map<NodeID,const CallBlockNode*> VTablePtrToCallSiteMap;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
public:
    FunptrDDAClient(SVFModule* module) : DDAClient(module) {}
    ~FunptrDDAClient() {}

    /// Only collect function pointers as query candidates.
    virtual NodeSet& collectCandidateQueries(PAG* p);
    virtual void performStat(PointerAnalysis* pta);
};



/**
 * DDA client with function pointers as query candidates.
 */
class AliasDDAClient : public DDAClient {

public:
    typedef std::set<const PAGNode*> PAGNodeSet;

    AliasDDAClient(SVFModule* module) : DDAClient(module) {}
    ~AliasDDAClient() {}

    /// Only collect function pointers as query candidates.
    virtual NodeSet& collectCandidateQueries(PAG* pag);

    virtual void performStat(PointerAnalysis* pta);

private:
    typedef std::map<NodeID,const CallBlockNode*> VTablePtrToCallSiteMap;
    VTablePtrToCallSiteMap vtableToCallSiteMap;
    PAGNodeSet loadSrcNodes;
    PAGNodeSet storeDstNodes;
    PAGNodeSet gepSrcNodes;
};


#endif /* DDACLIENT_H_ */
