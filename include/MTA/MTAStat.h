/*
 * MTAStat.h
 *
 *  Created on: Jun 23, 2015
 *      Author: Yulei Sui, Peng Di
 */

#ifndef MTASTAT_H_
#define MTASTAT_H_

#include "Util/PTAStat.h"

class ThreadCallGraph;
class TCT;
class MHP;
class LockAnalysis;
class MTAAnnotator;
/*!
 * Statistics for MTA
 */
class MTAStat : public PTAStat {

public:
    typedef std::set<const llvm::Instruction*> InstSet;

    /// Constructor
    MTAStat():PTAStat(NULL),TCTTime(0),MHPTime(0),FSMPTATime(0),AnnotationTime(0) {
    }
    /// Statistics for thread call graph
    void performThreadCallGraphStat(ThreadCallGraph* tcg);
    /// Statistics for thread creation tree
    void performTCTStat(TCT* tct);
    /// Statistics for MHP statement pairs
    void performMHPPairStat(MHP* mhp, LockAnalysis* lsa);
    /// Statistics for annotation
    void performAnnotationStat(MTAAnnotator* anno);

    double TCTTime;
    double MHPTime;
    double FSMPTATime;
    double AnnotationTime;
};


#endif /* MTASTAT_H_ */
