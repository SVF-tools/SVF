/*
 * SICFG.h
 *
 */

#ifndef INCLUDE_UTIL_SICFG_H_
#define INCLUDE_UTIL_SICFG_H_

#include "Util/ICFG.h"
#include "Util/VFG.h"

class SICFG : public ICFG{

private:
	VFG* vfg;

public:
	SICFG(PTACallGraph* cg): ICFG(cg){
		vfg = new VFG(cg);
	}

    /// Add VFGStmtNode into IntraBlockNode
    void handleIntraStmt(IntraBlockNode* instICFGNode, const Instruction* inst){
		if (!SVFUtil::isNonInstricCallSite(inst)) {
			PAG::PAGEdgeList& pagEdgeList = pag->getInstPAGEdgeList(inst);
			for (PAG::PAGEdgeList::const_iterator bit = pagEdgeList.begin(),
					ebit = pagEdgeList.end(); bit != ebit; ++bit) {
				if (!isPhiCopyEdge(*bit)) {
					StmtVFGNode* stmt = vfg->getStmtVFGNode(*bit);
					instICFGNode->addStmtVFGNode(stmt);
				}
			}
		}
    }

    /// Add VFGStmtNode into IntraBlockNode
    void handleArgument(InterBlockNode* instICFGNode){

    }
};



#endif /* INCLUDE_UTIL_SICFG_H_ */
