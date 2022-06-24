/*
 * MTAAnnotator.h
 *
 *  Created on: May 4, 2014
 *      Author: Peng Di
 */

#ifndef MTAANNOTATOR_H_
#define MTAANNOTATOR_H_

#include "Util/BasicTypes.h"
#include "Util/Annotator.h"
#include "MTA/MHP.h"

namespace SVF
{

/*!
 * MTA annotation
 */
class MTAAnnotator: public Annotator
{

public:
    typedef Set<const Instruction*> InstSet;
    /// Constructor
    MTAAnnotator(): mhp(nullptr),lsa(nullptr)
    {
        numOfAllSt = 0;
        numOfAllLd = 0;
        numOfNonLocalSt = 0;
        numOfNonLocalLd = 0;
        numOfAliasSt = 0;
        numOfAliasLd = 0;
        numOfMHPSt = 0;
        numOfMHPLd = 0;
        numOfAnnotatedSt = 0;
        numOfAnnotatedLd = 0;
    }
    /// Destructor
    virtual ~MTAAnnotator()
    {

    }
    /// Annotation
    //@{
    void annotateDRCheck(Instruction* inst);
    //@}

    /// Initialize
    void initialize(MHP* mhp, LockAnalysis* lsa);

    /// Prune candidate instructions that are thread local
    void pruneThreadLocal(PointerAnalysis*pta);

    /// Prune candidate instructions that non-mhp and non-alias with others
    void pruneAliasMHP(PointerAnalysis* pta);

    /// Perform annotation
    void performAnnotate();

    /// Collect all load and store instruction
    void collectLoadStoreInst(SVFModule* mod);

    /// Get operand of store and load
    const Value* getStoreOperand(const Instruction* inst);
    const Value* getLoadOperand(const Instruction* inst);


    /// Check if Function "F" is memset
    inline bool isMemset(const Instruction *I)
    {
        const Function* F =SVFUtil::getCallee(I)->getLLVMFun();
        return F && F->getName().find("llvm.memset") != std::string::npos;
    }

    /// Check if Function "F" is memcpy
    inline bool isMemcpy(const Instruction *I)
    {
        const SVFFunction* F =SVFUtil::getCallee(I);
        return F && F->getName().find("llvm.memcpy") != std::string::npos;
    }

private:
    MHP* mhp;
    LockAnalysis* lsa;
    InstSet loadset;
    InstSet storeset;


//	RCMemoryPartitioning mp;

public:

    u32_t numOfAllSt;
    u32_t numOfAllLd;
    u32_t numOfNonLocalSt;
    u32_t numOfNonLocalLd;
    u32_t numOfAliasSt;
    u32_t numOfAliasLd;
    u32_t numOfMHPSt;
    u32_t numOfMHPLd;
    u32_t numOfAnnotatedSt;
    u32_t numOfAnnotatedLd;

    /// Constant INTERLEV_FLAG values
    //@{
    static const u32_t ANNO_MHP= 0x04;
    static const u32_t ANNO_ALIAS= 0x02;
    static const u32_t ANNO_LOCAL= 0x01;
    //@}
};

} // End namespace SVF

#endif /* MTAANNOTATOR_H_ */
