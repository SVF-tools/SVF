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

class SubPAG : public PAG {
private:
    /// Name of the function this sub PAG represents.
    std::string functionName;
    /// Nodes in the SubPAG which call edges should connect to.
    /// argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    std::map<int, PAGNode *> argNodes;
    PAGNode *returnNode;

public:
    SubPAG(std::string functionName) : PAG(true), functionName(functionName),
	                                   argNodes(), returnNode(NULL) {
    }

    ~SubPAG() {
    }

    std::map<int, PAGNode *> &getArgNodes() { return argNodes; }
    std::string getFunctionName() { return functionName; }
    PAGNode *getReturnNode() { return returnNode; }
    void setReturnNode(PAGNode *returnNode) { this->returnNode = returnNode; }
};

#endif  /* SUBPAG_H_ */
