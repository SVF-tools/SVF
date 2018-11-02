/*
 * IFDS.cpp
 *
 */

// how to distinguish local variable and global variable?     PAGNode is global stack
//TODO 1. in TransferFun: remove dummy value
//TODO 3. in ForwardTabulate: realize CallNode and EndFuncNode

#include "WPA/IFDS.h"

using namespace std;

//constructor
IFDS::IFDS(ICFG *i) : icfg(i) {

    pta = AndersenWaveDiff::createAndersenWaveDiff(getPAG()->getModule());
    initialize();
    forwardTabulate();
    printRes();
}

/*initialization
    set PathEdgeList = {(<EntryMain, 0> --> <EntryMain,0>)}
    set WorkList = {(<EntryMain, 0> --> <EntryMain,0>)}
    set SummaryEdgeList = {}
 */
void IFDS::initialize() {

    Datafact datafact = {};    // datafact = 0;
    mainEntryNode = icfg->getFunEntryICFGNode(SVFUtil::getProgEntryFunction(getPAG()->getModule()));
    PathNode *mainEntryPN = new PathNode(mainEntryNode, datafact);
    PathEdge *startPE = new PathEdge(mainEntryPN, mainEntryPN);
    PathEdgeList.push_back(startPE);
    WorkList.push_back(startPE);
    SummaryEdgeList = {};

    //initialize ICFGNodeToFacts
    for (ICFG::const_iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        ICFGNodeToFacts[node] = {};
    }
}

void IFDS::forwardTabulate() {

    while (!WorkList.empty()) {
        PathEdge *e = WorkList.front();
        WorkList.pop_front();
        PathNode *srcPN = e->getSrcPathNode();
        PathNode *dstPN = e->getDstPathNode();
        const ICFGNode *n = e->getDstPathNode()->getICFGNode();

        if (SVFUtil::isa<CallBlockNode>(n)) {

            //code to add
        } else if (SVFUtil::isa<FunExitBlockNode>(n)) {
            //code to add
        } else if (SVFUtil::isa<IntraBlockNode>(n)
                   || SVFUtil::isa<RetBlockNode>(n)
                   || SVFUtil::isa<FunEntryBlockNode>(n)) {

            Datafact d = transferFun(dstPN);     //caculate datafact after execution of n
            const ICFGEdge::ICFGEdgeSetTy &outEdges = n->getOutEdges();
            for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
                    outEdges.end(); it != eit; ++it) {
                ICFGNode *succ = (*it)->getDstNode();
                propagate(srcPN, succ, d);
            }
        }
    }
}

// print ICFGNodes and theirs datafacts
void IFDS::printRes(){
    std::cout << "\n*******Possibly Uninitialized Variables*******\n";
    for (ICFGNodeToDataFactsMap::iterator it = ICFGNodeToFacts.begin(), eit = ICFGNodeToFacts.end(); it != eit; ++it){
        const ICFGNode* node = it->first;
        Facts facts = it->second;
        NodeID id = node->getId();
        std::cout << "ICFGNodeID:" <<id << ": PAGNodeSet: {";
        Datafact finalFact = {};
        for (Facts::iterator fit = facts.begin(), efit = facts.end(); fit != efit; ++fit){
            Datafact fact = (*fit);
            for(Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ++dit){
                finalFact.insert(*dit);
            }
        }
        for(Datafact::iterator dit = finalFact.begin(), edit = finalFact.end(); dit != edit; ++dit){
            std::cout << (*dit)->getId() << " ";
        }
        std::cout << "}\n";
    }
    std::cout << "-------------------------------------------------------";
}

//add new PathEdge into PathEdgeList and WorkList
void IFDS::propagate(PathNode *srcPN, ICFGNode *succ, Datafact d) {
    ICFGDstNodeSet = getDstICFGNodeSet();
    ICFGNodeSet::iterator it = ICFGDstNodeSet.find(succ);
    if (it == ICFGDstNodeSet.end()) {
        PathNode *newDstPN = new PathNode(succ, d);
        PathEdge *e = new PathEdge(srcPN, newDstPN);
        WorkList.push_back(e);
        PathEdgeList.push_back(e);
        ICFGNodeToFacts[succ].insert(d);
    } else {
        facts = ICFGNodeToFacts[succ];
        Facts::iterator it = facts.find(d);
        if (it == facts.end()) {
            PathNode *newDstPN = new PathNode(succ, d);
            PathEdge *e = new PathEdge(srcPN, newDstPN);
            WorkList.push_back(e);
            PathEdgeList.push_back(e);
            ICFGNodeToFacts[succ].insert(d);
        }
    }
}

//get all ICFGNode in all EndPathNode of PathEdgeList
IFDS::ICFGNodeSet& IFDS::getDstICFGNodeSet() {
    for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit =
            PathEdgeList.end(); pit != epit; ++pit) {
        const ICFGNode *n = (*pit)->getDstPathNode()->getICFGNode();
        ICFGDstNodeSet.insert(n);
    }
    return ICFGDstNodeSet;
}

bool IFDS::isInitialized(const PAGNode *pagNode, Datafact datafact) {
    Datafact::iterator it = datafact.find(pagNode);
    if (it == datafact.end())
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

IFDS::Datafact IFDS::transferFun(PathNode *pathNode) {
    const ICFGNode *icfgNode = pathNode->getICFGNode();
    Datafact fact = pathNode->getDataFact();

    //execution of mainEntryNode: add all PAGNode into fact (except constant value PAGNode)
    if (icfgNode == mainEntryNode) {
        for (PAG::const_iterator it = (icfg->getPAG())->begin(), eit = icfg->getPAG()->end(); it != eit; ++it) {
            PAGNode *node = it->second;
            if (node->hasIncomingEdge() || node->hasOutgoingEdge()){
                bool constant = false;
                if(node->isConstantData())
                    constant = true;
                if (ObjPN* objNode = SVFUtil::dyn_cast<ObjPN>(node))
                    if (objNode->getMemObj()->isGlobalObj()||objNode->getMemObj()->isFunction())
                        constant = true;
                PAGEdge::PAGEdgeSetTy edges= node->getIncomingEdges(PAGEdge::Addr);
                for(PAGEdge::PAGEdgeSetTy::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it){
                    PAGEdge* e = *it;
                    if(ObjPN* objNode = SVFUtil::dyn_cast<ObjPN>(e->getSrcNode()))
                        if (objNode->getMemObj()->isGlobalObj()||objNode->getMemObj()->isFunction())
                            constant = true;
                }
                if (DummyValPN* dummyValNode = SVFUtil::dyn_cast<DummyValPN>(node))
                    constant = true;
                if (!constant)
                    fact.insert(node);
            }
        }
        return fact;
    }

    if (const IntraBlockNode *node = SVFUtil::dyn_cast<IntraBlockNode>(icfgNode)) {
        for (IntraBlockNode::StmtOrPHIVFGNodeVec::const_iterator it = node->vNodeBegin(), eit = node->vNodeEnd();
             it != eit; ++it) {
            const VFGNode *node = *it;
            if (const StmtVFGNode *stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node)) {
                PAGNode *dstPagNode = stmtNode->getPAGDstNode();
                PAGNode *srcPagNode = stmtNode->getPAGSrcNode();
                // Addr: srcNode is uninitialized, dstNode is initialiazed
                if (const AddrVFGNode *addrNode = SVFUtil::dyn_cast<AddrVFGNode>(stmtNode)) {
                    fact.insert(srcPagNode);
                    fact.erase(dstPagNode);
                }
                // Copy: dstNode depends on srcNode
                else if (const CopyVFGNode *copyNode = SVFUtil::dyn_cast<CopyVFGNode>(stmtNode)) {
                    if (isInitialized(srcPagNode, fact))
                        fact.erase(dstPagNode);
                    else
                        fact.insert(dstPagNode);
                }
                // Gep: same as Copy
                else if (const GepVFGNode *copyNode = SVFUtil::dyn_cast<GepVFGNode>(stmtNode)) {
                    if (isInitialized(srcPagNode, fact))
                        fact.erase(dstPagNode);
                    else
                        fact.insert(dstPagNode);
                }
                // Store：dstNode->obj depends on srcNode
                else if (const StoreVFGNode* storeNode = SVFUtil::dyn_cast<StoreVFGNode>(stmtNode)){
                    PointsTo& PTset = IFDS::getPts(dstPagNode->getId());
                    if (isInitialized(srcPagNode, fact)){
                        for(PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it) {
                            PAGNode *node = getPAG()->getPAGNode(*it);
                            fact.erase(node);
                        }
                    }else {
                        for(PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it){
                            PAGNode* node = getPAG()->getPAGNode(*it);
                            fact.insert(node);
                        }
                    }
                }
                // Load：Load: dstNode depends on scrNode->obj
                // if all obj are initialized, dstPagNode is initialized, otherwise dstPagNode is Unini
                else if (const LoadVFGNode* loadNode = SVFUtil::dyn_cast<LoadVFGNode>(stmtNode)){
                    PointsTo PTset = IFDS::getPts(srcPagNode->getId());
                    u32_t sum = 0;
                    for(PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it){
                        PAGNode *node = getPAG()->getPAGNode(*it);
                        sum += isInitialized(node, fact);
                    }
                    if (sum == PTset.count())
                        fact.erase(dstPagNode);
                    else
                        fact.insert(dstPagNode);
                }
            }
            //code to add ..
        }
    } else if (const FunEntryBlockNode *node = SVFUtil::dyn_cast<FunEntryBlockNode>(icfgNode)) {
        //code to add ..
    } else if (const FunExitBlockNode *node = SVFUtil::dyn_cast<FunExitBlockNode>(icfgNode)) {
        //code to add ..
    } else if (const CallBlockNode *node = SVFUtil::dyn_cast<CallBlockNode>(icfgNode)) {
        //code to add ..
    } else if (const RetBlockNode *node = SVFUtil::dyn_cast<RetBlockNode>(icfgNode)) {
        //code to add ..
    }
    return fact;
}
