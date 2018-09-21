//===- ExternalPAG.h -- Program assignment graph-----------------------------------//

/*
 * ExternalPAG.h
 *
 *  Created on: Aug 20, 2018
 *      Author: Mohamad Barbar
 */

#ifndef EXTERNALPAG_H_
#define EXTERNALPAG_H_

#include "PAG.h"

/// Represents the PAG of a function loaded externally (i.e. from file).
/// It's purpose is to be attached to the main PAG (almost) seamlessly.
class ExternalPAG : public PAG {
private:
    /// Name of the function this external PAG represents.
    std::string functionName;
    /// Nodes in the ExternalPAG which call edges should connect to.
    /// argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    std::map<int, PAGNode *> argNodes;
    PAGNode *returnNode;

public:
    ExternalPAG(std::string functionName) : PAG(true), functionName(functionName),
	                                   argNodes(), returnNode(NULL) {
    }

    ~ExternalPAG() {
    }

    std::map<int, PAGNode *> &getArgNodes() { return argNodes; }
    std::string getFunctionName() { return functionName; }
    PAGNode *getReturnNode() { return returnNode; }
    void setReturnNode(PAGNode *returnNode) { this->returnNode = returnNode; }
};

#endif  /* EXTERNALPAG_H_ */
