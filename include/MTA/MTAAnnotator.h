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
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>

/*!
 * MTA annotation
 */
class MTAAnnotator: public Annotator {

public:
    typedef std::set<const llvm::Instruction*> InstSet;
    /// Constructor
    MTAAnnotator(): mhp(NULL),lsa(NULL) {
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
    virtual ~MTAAnnotator() {

    }
    /// Annotation
    //@{
    void annotateDRCheck(llvm::Instruction* inst);
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
    void collectLoadStoreInst(llvm::Module* mod);

    /// Get operand of store and load
    const llvm::Value* getStoreOperand(const llvm::Instruction* inst);
    const llvm::Value* getLoadOperand(const llvm::Instruction* inst);


    /// Check if Function "F" is memset
    inline bool isMemset(const llvm::Instruction *I) {
        const llvm::Function* F =analysisUtil::getCallee(I);
        return F && F->getName().find("llvm.memset") != llvm::StringRef::npos;
    }

    /// Check if Function "F" is memcpy
    inline bool isMemcpy(const llvm::Instruction *I) {
        const llvm::Function* F =analysisUtil::getCallee(I);
        return F && ExtAPI::EFT_L_A0__A0R_A1R == ExtAPI::getExtAPI()->get_type(F);
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

#endif /* MTAANNOTATOR_H_ */
