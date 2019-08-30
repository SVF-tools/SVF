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
    void annotateDRCheck(Instruction* inst);
    //@}

};

#endif /* MTAANNOTATOR_H_ */
