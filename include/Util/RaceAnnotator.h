/*
 * RaceAnnotator.h
 *
 *  Created on: May 4, 2014
 *      Author: Peng Di
 */

#ifndef MTAANNOTATOR_H_
#define MTAANNOTATOR_H_

#include "Util/BasicTypes.h"
#include "Util/Annotator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>

/*!
 * MTA annotation
 */
class RaceAnnotator: public Annotator {

public:
    /// Constructor
    RaceAnnotator() {
    }
    /// Destructor
    virtual ~RaceAnnotator() {
    }
    /// Annotation
    //@{
    void annotateDRCheck(llvm::Instruction* inst);
    //@}

};

#endif /* MTAANNOTATOR_H_ */
