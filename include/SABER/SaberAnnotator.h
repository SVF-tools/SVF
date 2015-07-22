//===- SaberAnnotator.h -- Annotating LLVM IR---------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * SaberAnnotator.h
 *
 *  Created on: May 4, 2014
 *      Author: Yulei Sui
 */

#ifndef SABERANNOTATOR_H_
#define SABERANNOTATOR_H_

#include "Util/BasicTypes.h"
#include "Util/Annotator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>

class ProgSlice;
class SVFGNode;
/*!
 * Saber annotation
 */
class SaberAnnotator : public Annotator {

private:
    const ProgSlice* _curSlice;
public:
    /// Constructor
    SaberAnnotator(ProgSlice* slice): _curSlice(slice) {

    }
    /// Destructor
    virtual ~SaberAnnotator() {

    }
    /// Annotation
    //@{
    void annotateSource();
    void annotateSinks();
    void annotateFeasibleBranch(const llvm::BranchInst *brInst, u32_t succPos);
    void annotateInfeasibleBranch(const llvm::BranchInst *brInst, u32_t succPos);

    void annotateSwitch(llvm::SwitchInst *brInst, u32_t succPos);
    //@}
};


#endif /* SABERANNOTATOR_H_ */
