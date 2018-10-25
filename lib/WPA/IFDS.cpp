/*
 * IFDS.cpp
 *
 */

// how to distinguish local variable and global variable?     PAGNode is global stack

#include "WPA/IFDS.h"     //parameter and return cannot use object

static llvm::cl::opt<bool> IFDS("-ifds", llvm::cl::init(false),
                                    llvm::cl::desc("uninitialized variable analysis"));     // ??

//constructor
IFDS::IFDS(ICFG* i): icfg(i){
    initialize();
    forwardTabulate();
    merge();
}

/*initialization
    set PathEdgeList = {(<EntryMain, 0> --> <EntryMain,0>)}
    set WorkList = {(<EntryMain, 0> --> <EntryMain,0>)}
    set SummaryEdgeList = {}
 */
void IFDS::initialize(){

    Datafact* datafact = { };
    mainEntryNode = icfg->getFunEntryICFGNode(SVFUtil::getProgEntryFunction(getPAG()->getModule()));
    PathNode* mainEntryPN = new PathNode(mainEntryNode, datafact);
    PathEdge* startPE = new PathEdge(mainEntryPN, mainEntryPN);
    PathEdgeList.push_back(startPE);
    WorkList.push_back(startPE);
    SummaryEdgeList = {};
}

void IFDS::forwardTabulate() {

	while (!WorkList.empty()) {
		PathEdge* e = WorkList.front();
		WorkList.pop_front();
		PathNode* srcPN = e->getSrcPathNode();
		PathNode* dstPN = e->getDstPathNode();
		ICFGNode* n = e->getDstPathNode()->getICFGNode();

		if (SVFUtil::isa<CallBlockNode>(n)) {
			//code to add
		} else if (SVFUtil::isa<FunExitBlockNode>(n)) {
			//code to add
		} else if (SVFUtil::isa<IntraBlockNode>(n)
				|| SVFUtil::isa<RetBlockNode>(n)
				|| SVFUtil::isa<FunEntryBlockNode>(n)) {

			Datafact* d = transferFun(dstPN);     //caculate datafact after execution of n
			const ICFGEdge::ICFGEdgeSetTy& outEdges = n->getOutEdges();
			for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
					outEdges.end(); it != eit; ++it) {
				ICFGNode* succ = (*it)->getDstNode();
				propagate(srcPN, succ, d);
			}

		}
	}
}

//add new PathEdge into PathEdgeList and WorkList
void IFDS::propagate(PathNode* srcPN, ICFGNode* succ, Datafact* d) {
	ICFGDstNodeSet = getDstICFGNodeSet();
	ICFGNodeSet::iterator it = ICFGDstNodeSet.find(succ);
	if (it == ICFGDstNodeSet.end()) {
		PathNode* newDstPN = new PathNode(succ, d);
		PathEdge* e = new PathEdge(srcPN, newDstPN);
		WorkList.push_back(e);
		PathEdgeList.push_back(e);
	} else {
		Facts facts = getDstICFGNodeFacts(succ);
		Facts::iterator it = facts.find(d);
		if (it == facts.end()) {
			PathNode* newDstPN = new PathNode(succ, d);
			PathEdge* e = new PathEdge(srcPN, newDstPN);
			WorkList.push_back(e);
			PathEdgeList.push_back(e);
		}
	}
}

//get all ICFGNode in all EndPathNode of PathEdgeList
IFDS::ICFGNodeSet& IFDS::getDstICFGNodeSet() {
	for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit =
			PathEdgeList.end(); pit != epit; ++pit) {
		ICFGNode* n = (*pit)->getDstPathNode()->getICFGNode();
		ICFGDstNodeSet.insert(n);
	}
	return ICFGDstNodeSet;
}

//get all datafacts of a given ICFGNode in all EndPathNode of PathEdgeList
IFDS::Facts IFDS::getDstICFGNodeFacts(ICFGNode* node) {
	Facts facts = { };
	for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit =
			PathEdgeList.end(); pit != epit; ++pit) {
		ICFGNode* n = (*pit)->getDstPathNode()->getICFGNode();
		if (n == node) {
			Datafact* nodeFact = (*pit)->getDstPathNode()->getDataFact();
			facts.insert(nodeFact);
		}
	}
	return facts;
}

// same node having different datafacts, merge facts together:  <n,d1>, <n,d2>  become <n, (d1,d2)>
IFDS::ICFGNodeToDataFactsMap &IFDS::merge() {
	for (ICFGNodeSet::iterator nit = ICFGDstNodeSet.begin(), enit =
			ICFGDstNodeSet.end(); nit != enit; ++nit) {
		Facts facts = getDstICFGNodeFacts(*nit);
		UniniVarSolution[*nit] = facts;
	}
	return UniniVarSolution;
}

bool IFDS::isInitialized(PAGNode* pagNode, Datafact* datafact){
	Datafact::iterator it = datafact->find(pagNode);
		if (it == datafact->end())
			return true;
		else
			return false;
}

// StmtNode(excludes cmp and binaryOp)
// Addr: srcNode is uninitialized, dstNode is initialiazed
// copy: dstNode depends on srcNode
// Store: dstNode->obj depends on srcNode     getPTS
// Load: dstNode depends on scrNode->obj
// Gep : same as Copy

// PHINode: resNode depends on operands -> getPAGNode

IFDS::Datafact* IFDS::transferFun(PathNode* pathNode) {
	ICFGNode* icfgNode = pathNode->getICFGNode();
	Datafact* fact = pathNode->getDataFact();
	//execution of mainEntryNode: add all PAGNode into fact
	if (icfgNode == mainEntryNode){
		for (PAG::const_iterator it = (icfg->getPAG())->begin(), eit = icfg->getPAG()->end(); it != eit ; ++it){
			PAGNode* node = it->second;
			fact->insert(node);
		}
		return fact;
	}

	if (IntraBlockNode *node = SVFUtil::dyn_cast<IntraBlockNode>(icfgNode)){
		IntraBlockNode::StmtOrPHIVFGNodeVec &nodes = node->getVFGNodes();
		for (IntraBlockNode::StmtOrPHIVFGNodeVec::const_iterator it = nodes.begin(), eit = nodes.end(); it != eit; ++ it){
			const VFGNode* node = *it;
			if (const StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node)){
				PAGNode* dstPagNode = stmtNode->getPAGDstNode();
				PAGNode* srcPagNode = stmtNode->getPAGSrcNode();
				// Addr: srcNode is uninitialized, dstNode is initialiazed
				if (const AddrVFGNode* addrNode = SVFUtil::dyn_cast<AddrVFGNode>(stmtNode)){
					fact->insert(srcPagNode);
					fact->erase(dstPagNode);
				}
				// Copy: dstNode depends on srcNode
				else if (const CopyVFGNode* copyNode = SVFUtil::dyn_cast<CopyVFGNode>(stmtNode)){
					if (isInitialized(srcPagNode,fact))
						fact->erase(dstPagNode);
					else
						fact->insert(dstPagNode);
				}
				// Gep: same as Copy
				else if (const GepVFGNode* copyNode = SVFUtil::dyn_cast<GepVFGNode>(stmtNode)){
					if (isInitialized(srcPagNode,fact))
						fact->erase(dstPagNode);
					else
						fact->insert(dstPagNode);
				}
				// code to add ..
			}
			//code to add ..
		}
	}else if (FunEntryBlockNode *node = SVFUtil::dyn_cast<FunEntryBlockNode>(icfgNode)){
		//code to add ..
	}else if (FunExitBlockNode *node = SVFUtil::dyn_cast<FunExitBlockNode>(icfgNode)){
		//code to add ..
	}else if (CallBlockNode *node = SVFUtil::dyn_cast<CallBlockNode>(icfgNode)){
		//code to add ..
	}else if (RetBlockNode *node = SVFUtil::dyn_cast<RetBlockNode>(icfgNode)){
		//code to add ..
	}

	return fact;
}
