//===- SubPAG.h -- Program assignment graph-----------------------------------//

/*
 * SubPAG.h
 *
 *  Created on: Aug 20, 2018
 *      Author: Mohamad Barbar
 */

#ifndef SUBPAG_H_
#define SUBPAG_H_

#include "PAG.h"

class PAG;

class SubPAG : public PAG {
private:
    /// Name of the function this sub PAG represents.
    std::string functionName;
    /// Nodes in the SubPAG which call edges should connect to.
    /// argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    std::vector<PAGNode *> argNodes;

public:
    SubPAG(std::string functionName) : PAG(true), functionName(functionName), argNodes() {
    }

    ~SubPAG() {
    }

    std::vector<PAGNode *> &getArgNodes() { return argNodes; }
    std::string getFunctionName() { return functionName; }
};

#endif  /* SUBPAG_H_ */
