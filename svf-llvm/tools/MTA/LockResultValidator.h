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
    const Instruction* getPreviousMemoryAccessInst( const Instruction* I);

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
