/*
 * MTAResultValidator.h
 *
 *  Created on: 29/06/2015
 *      Author: Peng Di
 */

#ifndef MTARESULTVALIDATOR_H_
#define MTARESULTVALIDATOR_H_

#include <MemoryModel/PointerAnalysis.h>
#include "MTA/TCT.h"
#include "MTA/MHP.h"
#include "Util/AnalysisUtil.h"
#include "Util/ThreadCallGraph.h"

/*!
 * Validate the result of context-sensitive analysis, including context-sensitive
 * thread detection and thread interleaving.
 */
class MTAResultValidator {

public:
    typedef int INTERLEV_FLAG;
    MTAResultValidator(MHP* mh) :
        mhp(mh) {
        tcg = mhp->getThreadCallGraph();
        tdAPI = tcg->getThreadAPI();
        mod = tcg->getModule();
    }
    // Destructor
    ~MTAResultValidator() {
    }

    // Analysis
    void analyze();

protected:

    /*
     * Assistant functions
     */

    // Split string
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
    std::vector<std::string> split(const std::string &s, char delim);

    // Get special arguments of given call sites
    NodeID getIntArg(const llvm::Instruction* inst, unsigned int arg_num);
    std::vector<std::string> getStringArg(const llvm::Instruction* inst, unsigned int arg_num);
    CallStrCxt getCxtArg(const llvm::Instruction* inst, unsigned int arg_num);

    /*
     * Get the previous LoadInst or StoreInst from Instruction "I" in the
     * same BasicBlock. Return NULL if none exists.
     */
    const llvm::Instruction *getPreviousMemoryAccessInst(const llvm::Instruction *I);

    // Compare two cxts
    bool matchCxt(const CallStrCxt cxt1, const CallStrCxt cxt2) const;

    // Dump calling context information
    void dumpCxt(const CallStrCxt& cxt) const;

    void dumpInterlev(NodeBS& lev);

    // Get the validation result string of a single validation scenario.
    inline std::string getOutput(const char *scenario, bool analysisRes);
    inline std::string getOutputforInterlevAnalysis(const char *scenario, INTERLEV_FLAG analysisRes);

    /*
     * Collect the callsite targets for validations.
     * The targets are labeled by "cs1:", "cs2:"... that are the names of its basic blocks.
     * The collected targets are stored in csnumToInstMap that maps label "cs1" to CallInst.
     */
    bool collectCallsiteTargets();

    /*
     * Collect the CxtThread targets for validations.
     * The collected targets are stored in vthdToCxt that maps vthd to cxt.
     */
    bool collectCxtThreadTargets();

    /*
     * Collect TCT targets for validations.
     * The collected targets are stored in rthdToChildren.
     */
    bool collectTCTTargets();

    /*
     * Collect the thread interleaving targets for validations.
     * The collected targets are stored in instToTSMap and threadStmtToInterLeaving.
     */
    bool collectInterleavingTargets();

    /*
     * Perform validation for Cxtthread.
     * If correct, the validator maps given thread vthd to static CxtThread rthd stored in vthdTorthd.
     */
    bool validateCxtThread();

    /*
     * Perform validation for TCT.
     */
    bool validateTCT();

    /*
     * Perform validation for thread interleaving.
     */
    INTERLEV_FLAG validateInterleaving();

private:

    std::map<NodeID, const llvm::CallInst*> csnumToInstMap;
    std::map<NodeID, CallStrCxt> vthdToCxt;
    std::map<NodeID, NodeID> vthdTorthd;
    std::map<NodeID, NodeID> rthdTovthd;

    std::map<NodeID, std::set<NodeID>> rthdToChildren;

    MHP::InstToThreadStmtSetMap instToTSMap; // Map a instruction to CxtThreadStmtSet
    MHP::ThreadStmtToThreadInterleav threadStmtToInterLeaving; /// Map a statement to its thread interleavings

    static constexpr char const *CXT_THREAD = "CXT_THREAD";
    static constexpr char const *INTERLEV_ACCESS = "INTERLEV_ACCESS";
    static constexpr char const *TCT_ACCESS = "TCT_ACCESS";

    llvm::Module* mod;
    ThreadAPI* tdAPI;
    ThreadCallGraph* tcg;
    MHP* mhp;

    /// Constant INTERLEV_FLAG values
    //@{
    static const INTERLEV_FLAG INTERLEV_TRUE = 0x01;
    static const INTERLEV_FLAG INTERLEV_IMPRECISE = 0x02;
    static const INTERLEV_FLAG INTERLEV_UNSOUND = 0x04;
    //@}
};

#endif /* MTARESULTVALIDATOR_H_ */
