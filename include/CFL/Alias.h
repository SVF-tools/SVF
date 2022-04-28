/* -------------------- Alias.h ------------------ */
//
// Created by kisslune on 3/4/22.
//

#ifndef SVF_ALIAS_H
#define SVF_ALIAS_H

#include "CFLSolver.h"


namespace SVF
{

/*!
 *
 */
class AliasSolver : public CFLRSolver
{
public:
    enum aliasLabels {
        fault,      // a fault label
        epsilon,
        a,
        abar,
        d,
        dbar,
        f,
        fbar,
        A,
        Abar,
        F,
        DV,     // dbar V
        FV,     // fbar V
        M,
        V
    };

    void buildGraph();      // build graph from constraint graph

    // Context-free grammar operations
    //@{
    virtual Label unaryDerivation(Label lbl);         // A ::= B
    virtual Label binaryDerivation(Label llbl, Label rlbl);     // A ::= B C
    //@}
};


}

#endif //SVF_ALIAS_H
