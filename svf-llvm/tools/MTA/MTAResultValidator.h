/*
 * MTAResultValidator.h
 *
 *  Created on: 29/06/2015
 *      Author: Peng Di and Ding Ye
 */

#ifndef MTARESULTVALIDATOR_H_
#define MTARESULTVALIDATOR_H_

#include "MTA/MHP.h"
#include "SVF-LLVM/LLVMUtil.h"

/*!
 * Validate the result of context-sensitive analysis, including context-sensitive
 * thread detection and thread interleaving.
 */
namespace SVF
{
typedef unsigned NodeID;

class MTAResultValidator
{

public:
    typedef int INTERLEV_FLAG;
    MTAResultValidator(MHP* mh) :
        mhp(mh)
    {
        tcg = mhp->getThreadCallGraph();
        tdAPI = tcg->getThreadAPI();
        mod = mhp->getTCT()->getSVFModule();
    }
    // Destructor
    ~MTAResultValidator()
    {
    }

    // Analysis
    void analyze();
    inline SVFModule* getModule() const
    {
        return mod;
    }
protected:

    /*
     * Assistant functions
     */

    // Split string
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
    std::vector<std::string> split(const std::string &s, char delim);

    // Get special arguments of given call sites
    NodeID getIntArg(const Instruction* inst, unsigned int arg_num);
    std::vector<std::string> getStringArg(const Instruction* inst, unsigned int arg_num);
    CallStrCxt getCxtArg(const Instruction* inst, unsigned int arg_num);

    /*
     * Get the previous LoadInst or StoreInst from Instruction "I" in the
     * same BasicBlock. Return nullptr if none exists.
     */
    const Instruction* getPreviousMemoryAccessInst(const Instruction* I);

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
     * The collected targets are stored in csnumToInstMap that maps label "cs1" to its Callsite.
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

    typedef Map<NodeID, const SVFInstruction*> csnumToInst;
    typedef Map<NodeID, CallStrCxt> vthdToCxtMap;
    typedef Map<NodeID, NodeID> vthdTorthdMap;
    typedef Map<NodeID, NodeID> rthdTovthdMap;

    typedef Map<NodeID, Set<NodeID>> rthdToChildrenMap;

    MHP::InstToThreadStmtSetMap instToTSMap; // Map a instruction to CxtThreadStmtSet
    MHP::ThreadStmtToThreadInterleav threadStmtToInterLeaving; /// Map a statement to its thread interleavings

    static constexpr char const *CXT_THREAD = "CXT_THREAD";
    static constexpr char const *INTERLEV_ACCESS = "INTERLEV_ACCESS";
    static constexpr char const *TCT_ACCESS = "TCT_ACCESS";

    ThreadAPI* tdAPI;
    ThreadCallGraph* tcg;
    MHP* mhp;
    vthdToCxtMap vthdToCxt;
    vthdTorthdMap vthdTorthd;
    rthdTovthdMap rthdTovthd;
    csnumToInst	csnumToInstMap;
    rthdToChildrenMap rthdToChildren;
    SVFModule* mod;
    /// Constant INTERLEV_FLAG values
    //@{
    static const INTERLEV_FLAG INTERLEV_TRUE = 0x01;
    static const INTERLEV_FLAG INTERLEV_IMPRECISE = 0x02;
    static const INTERLEV_FLAG INTERLEV_UNSOUND = 0x04;
    //@}
};



/*!
 * \brief Validate the result of concurrent analysis.
 *
 * The properties to validate of two memory accesses include
 * one or more of the following four:
 * (1) they may accesses aliases;
 * (2) they may happen in parallel;
 * (3) they are protected by common lock(s);
 * (4) they may cause a data race error.
 * The ground truth are specified by the "RC_ACCESS" function in the target program.
 *
 * Users may utilize this result validator to validate their analysis with
 * one or more of the four properties, by inheriting the RCResultValidator class.
 * The corresponding virtual function of the desired property should be overridden.
 */
class RaceResultValidator
{
public:
    typedef int RC_FLAG;

    /*!
     * Data structure for recording access pairs for the validation.
     */
    class AccessPair
    {
    public:
        /// Constructor
        AccessPair(const Instruction* I1, const Instruction* I2,
                   const RC_FLAG flags) :
            I1(I1), I2(I2), flags(flags)
        {
        }

        /// Class member access
        //@{
        inline bool isFlaged(const RC_FLAG flag) const
        {
            return flags & flag;
        }
        inline const Instruction* getInstruction1() const
        {
            return I1;
        }
        inline const Instruction* getInstruction2() const
        {
            return I2;
        }
        //@}

    private:
        const Instruction* I1;
        const Instruction* I2;
        RC_FLAG flags;
    };

    /// Destructor
    virtual ~RaceResultValidator()
    {
        release();
    }

    /// Initialization
    void init(SVFModule* M)
    {
        this->M = M;
        selectedValidationScenarios = RC_MHP | RC_ALIASES | RC_PROTECTED | RC_RACE;
        collectValidationTargets();
    }

    /// Analysis
    void analyze()
    {
        validateAll();
    }

    /// Release resource
    void release()
    {
    }

    /// Check if the input program has validation target
    inline bool hasValidationTarget() const
    {
        return !accessPairs.empty();
    }

protected:
    /// Interface to the specific validation properties.
    /// Override one or more to implement your own analysis.
    //@{
    virtual bool mayAccessAliases(const Instruction* I1,
                                  const Instruction* I2)
    {
        selectedValidationScenarios &= ~RC_ALIASES;
        return true;
    }
    virtual bool mayHappenInParallel(const Instruction* I1,
                                     const Instruction* I2)
    {
        selectedValidationScenarios &= ~RC_MHP;
        return true;
    }
    virtual bool protectedByCommonLocks(const Instruction* I1,
                                        const Instruction* I2)
    {
        selectedValidationScenarios &= ~RC_PROTECTED;
        return true;
    }
    virtual bool mayHaveDataRace(const Instruction* I1,
                                 const Instruction* I2)
    {
        selectedValidationScenarios &= ~RC_RACE;
        return true;
    }
    //@}

    /*!
     * Collect the targets for validations.
     * The targets should be memory access Instructions in pairs.
     * The collected targets are stored in the member variable "accessPairs".
     */
    void collectValidationTargets();

    /// Perform validation for all targets.
    void validateAll();

    /// Get the validation result string of a single validation scenario.
    inline std::string getOutput(const char *scenario,
                                 bool analysisRes, bool expectedRes)
    {
        std::string ret(scenario);
        ret += "\t";
        if (expectedRes)
            ret += " T: ";
        else
            ret += " F: ";
        if (analysisRes == expectedRes)
            ret += SVFUtil::sucMsg("SUCCESS");
        else
            ret += SVFUtil::errMsg("FAILURE");
        return ret;
    }

private:
    SVFModule* M;
    std::vector<AccessPair> accessPairs;
    RC_FLAG selectedValidationScenarios;

    /*!
     * Comparison function to sort the validation targets in ascending order of
     * the validation id (i.e., the 1st argument of RC_ACCESS function call).
     */
    static bool compare(const CallBase* CI1, const CallBase* CI2)
    {
        const Value *V1 = CI1->getArgOperand(0);
        const Value *V2 = CI2->getArgOperand(0);
        const ConstantInt* C1 = SVFUtil::dyn_cast<ConstantInt>(V1);
        const ConstantInt* C2 = SVFUtil::dyn_cast<ConstantInt>(V2);
        assert(0 != C1 && 0 != C2);
        return C1->getZExtValue() < C2->getZExtValue();
    }

    /*!
     * Get the previous LoadInst or StoreInst from Instruction "I" in the
     * same BasicBlock.
     * Return nullptr if none exists.
     */
    const Instruction* getPreviousMemoryAccessInst(
        const Instruction* I);

    /// Constant RC_FLAG values
    //@{
    static const RC_FLAG RC_MHP = 0x01;
    static const RC_FLAG RC_ALIASES = 0x02;
    static const RC_FLAG RC_PROTECTED = 0x04;
    static const RC_FLAG RC_RACE = 0x10;
    //@}

    /// The name of the function which is used to specify the ground truth
    /// of the validation properties in the target program.
    static constexpr char const *RC_ACCESS = "RC_ACCESS";
};
}	// namespace SVF end
#endif /* MTARESULTVALIDATOR_H_ */
