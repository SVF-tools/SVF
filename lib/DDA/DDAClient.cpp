/*
 * @file: DDAClient.cpp
 * @author: yesen
 * @date: 16 Feb 2015
 *
 * LICENSE
 *
 */


#include "DDA/DDAClient.h"
#include "DDA/FlowDDA.h"
#include <iostream>
#include <iomanip>	// for std::setw

using namespace SVFUtil;

static llvm::cl::opt<bool> SingleLoad("single-load", llvm::cl::init(true),
                                llvm::cl::desc("Count load pointer with same source operand as one query"));

static llvm::cl::opt<bool> DumpFree("dump-free", llvm::cl::init(false),
                              llvm::cl::desc("Dump use after free locations"));

static llvm::cl::opt<bool> DumpUninitVar("dump-uninit-var", llvm::cl::init(false),
                                   llvm::cl::desc("Dump uninitialised variables"));

static llvm::cl::opt<bool> DumpUninitPtr("dump-uninit-ptr", llvm::cl::init(false),
                                   llvm::cl::desc("Dump uninitialised pointers"));

static llvm::cl::opt<bool> DumpSUPts("dump-su-pts", llvm::cl::init(false),
                               llvm::cl::desc("Dump strong updates store"));

static llvm::cl::opt<bool> DumpSUStore("dump-su-store", llvm::cl::init(false),
                                 llvm::cl::desc("Dump strong updates store"));

static llvm::cl::opt<bool> MallocOnly("malloc-only", llvm::cl::init(true),
                                llvm::cl::desc("Only add tainted objects for malloc"));

static llvm::cl::opt<bool> TaintUninitHeap("uninit-heap", llvm::cl::init(true),
                                     llvm::cl::desc("detect uninitialized heap variables"));

static llvm::cl::opt<bool> TaintUninitStack("uninit-stack", llvm::cl::init(true),
                                      llvm::cl::desc("detect uninitialized stack variables"));

void DDAClient::answerQueries(PointerAnalysis* pta) {

    collectCandidateQueries(pta->getPAG());

    u32_t count = 0;
    for (NodeSet::iterator nIter = candidateQueries.begin();
            nIter != candidateQueries.end(); ++nIter,++count) {
        PAGNode* node = pta->getPAG()->getPAGNode(*nIter);
        if(pta->getPAG()->isValidTopLevelPtr(node)) {
            DBOUT(DGENERAL,outs() << "\n@@Computing PointsTo for :" << node->getId() <<
                  " [" << count + 1<< "/" << candidateQueries.size() << "]" << " \n");
            DBOUT(DDDA,outs() << "\n@@Computing PointsTo for :" << node->getId() <<
                  " [" << count + 1<< "/" << candidateQueries.size() << "]" << " \n");
            setCurrentQueryPtr(node->getId());
            pta->computeDDAPts(node->getId());
        }
    }
}

void FunptrDDAClient::performStat(PointerAnalysis* pta) {

    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pta->getModule());
    u32_t totalCallsites = 0;
    u32_t morePreciseCallsites = 0;
    u32_t zeroTargetCallsites = 0;
    u32_t oneTargetCallsites = 0;
    u32_t twoTargetCallsites = 0;
    u32_t moreThanTwoCallsites = 0;

    for (VTablePtrToCallSiteMap::iterator nIter = vtableToCallSiteMap.begin();
            nIter != vtableToCallSiteMap.end(); ++nIter) {
        NodeID vtptr = nIter->first;
        const PointsTo& ddaPts = pta->getPts(vtptr);
        const PointsTo& anderPts = ander->getPts(vtptr);

        PTACallGraph* callgraph = ander->getPTACallGraph();
        if(!callgraph->hasIndCSCallees(nIter->second)) {
            //outs() << "virtual callsite has no callee" << *(nIter->second.getInstruction()) << "\n";
            continue;
        }

        const PTACallGraph::FunctionSet& callees = callgraph->getIndCSCallees(nIter->second);
        totalCallsites++;
        if(callees.size() == 0)
            zeroTargetCallsites++;
        else if(callees.size() == 1)
            oneTargetCallsites++;
        else if(callees.size() == 2)
            twoTargetCallsites++;
        else
            moreThanTwoCallsites++;

        if(ddaPts.count() >= anderPts.count() || ddaPts.empty())
            continue;

        std::set<const Function*> ander_vfns;
        std::set<const Function*> dda_vfns;
        ander->getVFnsFromPts(nIter->second,anderPts, ander_vfns);
        pta->getVFnsFromPts(nIter->second,ddaPts, dda_vfns);

        ++morePreciseCallsites;
        outs() << "============more precise callsite =================\n";
        outs() << *(nIter->second).getInstruction() << "\n";
        outs() << getSourceLoc((nIter->second).getInstruction()) << "\n";
        outs() << "\n";
        outs() << "------ander pts or vtable num---(" << anderPts.count()  << ")--\n";
        outs() << "------DDA vfn num---(" << ander_vfns.size() << ")--\n";
        //ander->dumpPts(vtptr, anderPts);
        outs() << "------DDA pts or vtable num---(" << ddaPts.count() << ")--\n";
        outs() << "------DDA vfn num---(" << dda_vfns.size() << ")--\n";
        //pta->dumpPts(vtptr, ddaPts);
        outs() << "-------------------------\n";
        outs() << "\n";
        outs() << "=================================================\n";
    }

    outs() << "=================================================\n";
    outs() << "Total virtual callsites: " << vtableToCallSiteMap.size() << "\n";
    outs() << "Total analyzed virtual callsites: " << totalCallsites << "\n";
    outs() << "Indirect call map size: " << ander->getPTACallGraph()->getIndCallMap().size() << "\n";
    outs() << "Precise callsites: " << morePreciseCallsites << "\n";
    outs() << "Zero target callsites: " << zeroTargetCallsites << "\n";
    outs() << "One target callsites: " << oneTargetCallsites << "\n";
    outs() << "Two target callsites: " << twoTargetCallsites << "\n";
    outs() << "More than two target callsites: " << moreThanTwoCallsites << "\n";
    outs() << "=================================================\n";
}

