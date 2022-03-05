/* -------------------- Alias.h ------------------ */
//
// Created by kisslune on 3/4/22.
//

#ifndef SVF_ALIAS_H
#define SVF_ALIAS_H

#include "CFLSolver.h"
#include "Graphs/ConsG.h"

namespace SVF
{

/*!
 *
 */
class AliasSolver : public CFLRSolver
{
public:
    void buildGraph();      // build graph from constraint graph

    // Context-free grammar operations
    //@{
    virtual std::set<Label> unaryDerivation(Label lbl);         // A ::= B
    virtual Label binaryDerivation(Label llbl, Label rlbl);     // A ::= B C
    //@}
};


}

#endif //SVF_ALIAS_H
