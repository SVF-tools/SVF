/*
 * LOCKResultValidator.h
 *
 *  Created on: 24/07/2021
 */



#ifndef LOCKRESULTVALIDATOR_H_
#define LOCKRESULTVALIDATOR_H_

#include "MTA/LockAnalysis.h"
#include "SVF-LLVM/LLVMUtil.h"

/* Validate the result of lock analysis */

namespace SVF
{
class LockResultValidator
{
public:
    typedef Set<std::string> CxtLockSetStr;
    typedef Map<const SVFInstruction*, CxtLockSetStr> CxtStmtToCxtLockS;

    typedef unsigned LOCK_FLAG;

    LockResultValidator(LockAnalysis* la) : _la(la)
    {
        _mod = _la->getTCT()->getSVFModule();
    }

    ~LockResultValidator() {}

    void analyze();

    inline SVFModule* getModule() const
    {
        return _mod;
    }
private:
    // Get CallICFGNode
    inline CallICFGNode* getCBN(const SVFInstruction* inst)
    {
        return _la->getTCT()->getCallICFGNode(inst);
    }
    const Instruction* getPreviousMemoryAccessInst( const Instruction* I)
    {
        I = I->getPrevNode();
        while (I)
        {
            if (SVFUtil::isa<LoadInst>(I) || SVFUtil::isa<StoreInst>(I))
                return I;
            SVFFunction* callee = nullptr;

            if(LLVMUtil::isCallSite(I))
            {
                PTACallGraph::FunctionSet callees;
                const SVFInstruction* svfInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(I);
                _la->getTCT()->getThreadCallGraph()->getCallees(getCBN(svfInst), callees);

                for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(),
                        ecit = callees.end(); cit!=ecit; cit++)
                {
                    if(*cit != nullptr)
                    {
                        callee = const_cast<SVFFunction*> (*cit);
                        break;
                    }

                }
            }

            if (callee)
            {
                if (callee->getName().find("llvm.memset") != std::string::npos)
                    return I;
            }
            I = I->getPrevNode();
        }
        return nullptr;
    }

    inline bool inFilter(const std::string& name)
    {
        return filterFun.find(name) != filterFun.end();
    }

    inline bool match(const std::string& lockName, CxtLockSetStr LS)
    {
        return LS.find(lockName) != LS.end();
    }

    Set<std::string> &split(const std::string &s, char delim, Set<std::string> &elems);
    Set<std::string> split(const std::string &s, char delim);

    std::string getOutput(const char* scenario, LOCK_FLAG analysisRes);

    Set<std::string> getStringArg(const Instruction* inst, unsigned int arg_num);

    bool collectLockTargets();
    LOCK_FLAG validateStmtInLock();

    CxtStmtToCxtLockS instToCxtLockSet;
    LockAnalysis::CxtStmtToCxtLockSet cxtStmtToCxtLockSet;
    LockAnalysis::CxtLockSet cxtLockSet;

    LockAnalysis* _la;
    SVFModule* _mod;

    static const LOCK_FLAG LOCK_TRUE = 0x01;
    static const LOCK_FLAG LOCK_IMPRECISE = 0x02;
    static const LOCK_FLAG LOCK_UNSOUND = 0x04;

    static constexpr char const *LOCK = "LOCK";

    Set<std::string> filterFun = {"LOCK", "INTERLEV_ACCESS", "PAUSE", "CXT_THREAD", "TCT_ACCESS"};
};
}	// End namespace SVF
#endif /* LOCKRESULTVALIDATOR_H_ */
