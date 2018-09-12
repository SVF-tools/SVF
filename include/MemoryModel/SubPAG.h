//===- SubPAG.h -- Program assignment graph-----------------------------------//

/*
 * SubPAG.h
 *
 *  Created on: Aug 20, 2018
 *      Author: Mohamad Barbar
 */

#ifndef SUBPAG_H_
#define SUBPAG_H_

class SubPAG : public PAG {
    // Nodes in the SubPAG which call edges should connect to.
    // argNodes[0] is arg 0, argNodes[1] is arg 1, ...
    std::vector<PAGNode *> argNodes;

};

#endif  /* SUBPAG_H_ */
