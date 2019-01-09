/*
 * IFDS.cpp
 *
 */

#include "WPA/IFDS.h"

using namespace std;
using namespace SVFUtil;

//constructor
IFDS::IFDS(ICFG *i) : icfg(i) {
    pta = AndersenWaveDiff::createAndersenWaveDiff(getPAG()->getModule());
    icfg->updateCallgraph(pta);
    icfg->getVFG()->updateCallGraph(pta);
    initialize();
    forwardTabulate();
    printRes();
}

/*initialization
    PathEdgeList = {(<EntryMain, 0> --> <EntryMain,0>)}
    WorkList = {(<EntryMain, 0> --> <EntryMain,0>)}
    SummaryEdgeList = {}
 */
void IFDS::initialize() {
    Datafact datafact = {};    // datafact = 0;
    assert(getProgEntryFunction(getPAG()->getModule())); // must have main function as program entry point
    mainEntryNode = icfg->getFunEntryICFGNode(getProgEntryFunction(getPAG()->getModule()));
    PathNode *mainEntryPN = new PathNode(mainEntryNode, datafact);
    PathEdge *startPE = new PathEdge(mainEntryPN, mainEntryPN);
    PathEdgeList.push_back(startPE);
    WorkList.push_back(startPE);
    SummaryEdgeList = {};
    ICFGDstNodeSet.insert(mainEntryNode);

    //initialize ICFGNodeToFacts
    for (ICFG::const_iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode *node = it->second;
        ICFGNodeToFacts[node] = {};
        SummaryICFGNodeToFacts[node] = {};
    }
}

void IFDS::forwardTabulate() {
    while (!WorkList.empty()) {
        PathEdge *e = WorkList.front();
        WorkList.pop_front();
        PathNode *srcPN = e->getSrcPathNode();
        const ICFGNode *sp = srcPN->getICFGNode();
        Datafact& d1 = srcPN->getDataFact();
        PathNode *dstPN = e->getDstPathNode();
        const ICFGNode *n = e->getDstPathNode()->getICFGNode();
        Datafact& d2 = dstPN->getDataFact();

        if (SVFUtil::isa<CallBlockNode>(n)) {
            const ICFGEdge::ICFGEdgeSetTy &outEdges = n->getOutEdges();
            for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
                    outEdges.end(); it != eit; ++it) {
                // if this is Call Edge
                if ((*it)->isCallCFGEdge()) {
                    ICFGNode *sp = (*it)->getDstNode();
                    Datafact& d = getCalleeDatafact(n, d2);
                    PathNode *newSrcPN = new PathNode(sp, d);
                    newSrcPN->setCaller(dstPN);   // set caller of sp
                    propagate(newSrcPN, sp, d);
                }
                // if it is CallToRetEdge
                else if ((*it)->isIntraCFGEdge()) {
                    Datafact& d = getCallToRetDatafact(d2);
                    ICFGNode *ret = (*it)->getDstNode();
                    propagate(srcPN, ret, d);
                    // add datafacts coming form SummaryEdges: find all <call, d1> --> <ret, any_fact> in SummaryEdgeList, add any_fact in retNode   ??
                    for (PathEdgeSet::iterator it = SummaryEdgeList.begin(), eit = SummaryEdgeList.end();
                         it != eit; ++it) {
                        if (((*it)->getSrcPathNode()->getICFGNode() == n)
                            && ((*it)->getSrcPathNode()->getDataFact() == dstPN->getDataFact())
                            && ((*it)->getDstPathNode()->getICFGNode() == ret)) {
                            propagate(srcPN, ret, (*it)->getDstPathNode()->getDataFact());
                        }
                    }
                }
            }
        } else if (SVFUtil::isa<FunExitBlockNode>(n)) {
            if(srcPN->getCaller()){
                const ICFGNode *caller = srcPN->getCaller()->getICFGNode();
                const ICFGEdge::ICFGEdgeSetTy &outEdges = caller->getOutEdges();
                for (ICFGEdge::ICFGEdgeSetTy::const_iterator it = outEdges.begin(), eit = outEdges.end(); it != eit; ++it){
                    if ((*it)->isCallCFGEdge()){
                        CallCFGEdge *callEdge = SVFUtil::dyn_cast<CallCFGEdge>(*it);
                        ICFGNode *ret = icfg->getRetICFGNode(icfg->getCallSite(callEdge->getCallSiteId()));
                        Datafact& d5 = transferFun(n, d2);
                        Datafact& d4 = srcPN->getCaller()->getDataFact();
                        if (isNotInSummaryEdgeList(caller, d4, ret, d5)) {
                            SEPropagate(caller, d4, ret, d5); // insert new summary edge into SummaryEdgeList
                            for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit = PathEdgeList.end();
                                 pit != epit; ++pit) {
                                if (((*pit)->getDstPathNode()->getICFGNode()->getId() == caller->getId())  //
                                    && ((*pit)->getDstPathNode()->getDataFact() == d4))
                                    propagate((*pit)->getSrcPathNode(), ret, d5);
                            }
                        }
                    }
                }
            }
        } else if (SVFUtil::isa<IntraBlockNode>(n)
                   || SVFUtil::isa<RetBlockNode>(n)
                   || SVFUtil::isa<FunEntryBlockNode>(n)) {

            Datafact& d = transferFun(n, d2);     //caculate datafact after execution of n
            const ICFGEdge::ICFGEdgeSetTy &outEdges = n->getOutEdges();
            for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
                    outEdges.end(); it != eit; ++it) {
                ICFGNode *succ = (*it)->getDstNode();
                propagate(srcPN, succ, d);
            }
        }
    }
}

//add new PathEdge into PathEdgeList and WorkList
void IFDS::propagate(PathNode *srcPN, ICFGNode *succ, Datafact& d) {
    if (ICFGDstNodeSet.find(succ) == ICFGDstNodeSet.end()) {
        PEPropagate(srcPN, succ, d);
        ICFGDstNodeSet.insert(succ);
    } else {
        if (ICFGNodeToFacts[succ].find(d) == ICFGNodeToFacts[succ].end()) {
            PEPropagate(srcPN, succ, d);
        }
    }
}

void IFDS:: PEPropagate(PathNode *srcPN, ICFGNode *succ, Datafact& d){
    PathNode *newDstPN = new PathNode(succ, d);
    PathEdge *e = new PathEdge(srcPN, newDstPN);
    WorkList.push_back(e);
    PathEdgeList.push_back(e);
    ICFGNodeToFacts[succ].insert(d);
}
// add new SummaryEdge
void IFDS:: SEPropagate(const ICFGNode *caller, Datafact& d4, ICFGNode *ret, Datafact& d5){
    PathNode *srcPN = new PathNode(caller, d4);
    PathNode *dstPN = new PathNode(ret, d5);
    PathEdge *e = new PathEdge(srcPN, dstPN);
    SummaryEdgeList.push_back(e);
    SummaryICFGNodeToFacts[caller].insert(d4);
    SummaryICFGNodeToFacts[ret].insert(d5);
}

bool IFDS::isNotInSummaryEdgeList(const ICFGNode *caller, Datafact& d4, ICFGNode *ret, Datafact& d5) {
    if (SummaryICFGDstNodeSet.find(ret) == SummaryICFGDstNodeSet.end()){
        SummaryICFGDstNodeSet.insert(ret);
        return true;
    }
    else if (SummaryICFGNodeToFacts[ret].find(d5) == SummaryICFGNodeToFacts[ret].end())
        return true;
    else if (SummaryICFGNodeToFacts[caller].find(d4) == SummaryICFGNodeToFacts[caller].end())
        return true;
    else
        return false;
}

bool IFDS::isInitialized(const PAGNode *pagNode, Datafact& datafact) {
    Datafact::iterator it = datafact.find(pagNode);
    if (it == datafact.end())
        return true;
    else
        return false;
}

IFDS::Datafact& IFDS::getCalleeDatafact(const ICFGNode *icfgNode, Datafact& fact) {     //TODO: no parameters?
    if (const CallBlockNode *node = SVFUtil::dyn_cast<CallBlockNode>(icfgNode)) {
        for (CallBlockNode::ActualParmVFGNodeVec::const_iterator it = node->getActualParms().begin(), eit = node->getActualParms().end();
             it != eit; ++it) {
            const PAGNode *actualParmNode = (*it)->getParam();
            if (actualParmNode->hasOutgoingEdges(PAGEdge::Call)) {   //Q1: only has one outgoing edge?
                for (PAGEdge::PAGEdgeSetTy::const_iterator pit = actualParmNode->getOutgoingEdgesBegin(
                        PAGEdge::Call), epit = actualParmNode->getOutgoingEdgesEnd(PAGEdge::Call);
                     pit != epit; ++pit) {
                    if (isInitialized(actualParmNode, fact))
                        fact.erase((*pit)->getDstNode());
                    else{
                        // x/a: replace x with a;
                        fact.insert((*pit)->getDstNode());
                        fact.erase(actualParmNode);
                    }
                }
            }
        }
    }
    delDatafact(fact, PAGNode::RetNode); //delete retNode in datafact
    return fact;
}

IFDS::Datafact& IFDS::getCallToRetDatafact(Datafact& fact) {   //TODO: handle global and heap
    for(Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ){   //erase global var
        if (const ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(*dit)) {
            if (objNode->getMemObj()->isGlobalObj())
                dit = fact.erase(dit);
            else
                dit++;
        }
        else
            dit++;
    }
    delDatafact(fact, PAGNode::RetNode);
    return fact;
}

//erase designated PAGNode
void IFDS::delDatafact(IFDS::Datafact& d, s32_t kind){
    for(Datafact::iterator dit = d.begin(), edit = d.end(); dit != edit; ){
        if((*dit)->getNodeKind() == kind)   //erase designated PAGNode
            dit = d.erase(dit);
        else
            dit++;
    }
}
// StmtNode(excludes cmp and binaryOp)
// Addr: srcNode is uninitialized, dstNode is initialiazed
// copy: dstNode depends on srcNode
// Store: dstNode->obj depends on srcNode
// Load: dstNode depends on scrNode->obj
// Gep : same as Copy

// PHINode: resNode depends on operands -> getPAGNode
// Cmp & Binary

IFDS::Datafact& IFDS::transferFun(const ICFGNode *icfgNode, Datafact& fact) {
    if (const IntraBlockNode *node = SVFUtil::dyn_cast<IntraBlockNode>(icfgNode)) {
        for (IntraBlockNode::StmtOrPHIVFGNodeVec::const_iterator it = node->vNodeBegin(), eit = node->vNodeEnd();
             it != eit; ++it) {
            const VFGNode *node = *it;
            if (const StmtVFGNode *stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node)) {
                PAGNode *dstPagNode = stmtNode->getPAGDstNode();
                PAGNode *srcPagNode = stmtNode->getPAGSrcNode();
                // Addr: srcNode is uninitialized, dstNode is initialiazed
                if (const AddrVFGNode *addrNode = SVFUtil::dyn_cast<AddrVFGNode>(stmtNode)) {
                    fact.erase(dstPagNode);
                    if(dstPagNode->isConstantData())
                        fact.erase(srcPagNode);
                    else
                        fact.insert(srcPagNode);
                    if (ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(srcPagNode))
                        if (objNode->getMemObj()->isFunction())
                            fact.erase(srcPagNode);
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
                else if (const StoreVFGNode *storeNode = SVFUtil::dyn_cast<StoreVFGNode>(stmtNode)) {
                    PointsTo &PTset = IFDS::getPts(dstPagNode->getId());
                    if (isInitialized(srcPagNode, fact)) {
                        for (PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it) {
                            PAGNode *node = getPAG()->getPAGNode(*it);
                            fact.erase(node);
                        }
                    } else {
                        for (PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it) {
                            PAGNode *node = getPAG()->getPAGNode(*it);
                            fact.insert(node);
                        }
                    }
                }
                // Load：Load: dstNode depends on scrNode->obj
                // if all obj are initialized, dstPagNode is initialized, otherwise dstPagNode is Unini
                else if (const LoadVFGNode *loadNode = SVFUtil::dyn_cast<LoadVFGNode>(stmtNode)) {
                    PointsTo PTset = IFDS::getPts(srcPagNode->getId());
                    u32_t sum = 0;
                    for (PointsTo::iterator it = PTset.begin(), eit = PTset.end(); it != eit; ++it) {
                        PAGNode *node = getPAG()->getPAGNode(*it);
                        sum += isInitialized(node, fact);
                    }
                    if (sum == PTset.count())
                        fact.erase(dstPagNode);
                    else
                        fact.insert(dstPagNode);
                }
            }
            //Compare:
            else if (const CmpVFGNode *cmpNode = SVFUtil::dyn_cast<CmpVFGNode>(node)){
                const PAGNode *resCmpNode = cmpNode->getRes();
                u32_t sum = 0;
                for(CmpVFGNode::OPVers::const_iterator it = cmpNode->opVerBegin(), eit = cmpNode->opVerEnd(); it != eit; ++it){
                    const PAGNode *opNode = it->second;
                    sum += isInitialized(opNode, fact);
                }
                if (sum == cmpNode->getOpVerNum())
                    fact.erase(resCmpNode);
                else
                    fact.insert(resCmpNode);
            }
            //BinaryOp:
            else if (const BinaryOPVFGNode *biOpNode = SVFUtil::dyn_cast<BinaryOPVFGNode>(node)){
                const PAGNode *resBiOpNode = biOpNode->getRes();
                u32_t sum = 0;
                for(BinaryOPVFGNode::OPVers::const_iterator it = biOpNode->opVerBegin(), eit = biOpNode->opVerEnd(); it != eit; ++it){
                    const PAGNode *opNode = it->second;
                    sum += isInitialized(opNode, fact);
                }
                if (sum == biOpNode->getOpVerNum())
                    fact.erase(resBiOpNode);
                else
                    fact.insert(resBiOpNode);
            }
        }
    } else if (const FunExitBlockNode *node = SVFUtil::dyn_cast<FunExitBlockNode>(icfgNode)) {
        for(Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ){
            if((*dit)->getFunction() && (*dit)->getNodeKind()!= PAGNode::RetNode)   //erase non global vars except retNode
                dit = fact.erase(dit);
            else
                dit++;
        }
    }
    else if (const RetBlockNode *node = SVFUtil::dyn_cast<RetBlockNode>(icfgNode)) {
        if (node->getActualRet()) {
            const PAGNode *actualRet = node->getActualRet()->getRev();// if there is a return statement
            if (!isExtCall(node->getCallSite())) {
                u32_t sum = 0;
                // all FormalRetNodes are initialized --> actualRetNode is initialized
                for (PAGEdge::PAGEdgeSetTy::iterator it = actualRet->getIncomingEdgesBegin(
                        PAGEdge::Ret), eit = actualRet->getIncomingEdgesEnd(PAGEdge::Ret); it != eit; ++it)
                    sum += isInitialized((*it)->getSrcNode(), fact);
                if (sum == std::distance(actualRet->getIncomingEdgesBegin(PAGEdge::Ret),
                                         actualRet->getIncomingEdgesEnd(PAGEdge::Ret)))
                    fact.erase(actualRet);
                else
                    fact.insert(actualRet);
            } else // isExtCall()
                fact.erase(actualRet);
        }
    }

    return fact;
}

// print ICFGNodes and theirs datafacts
void IFDS::printRes() {
    std::cout << "\n*******Possibly Uninitialized Variables*******\n";
    for (ICFGNodeToDataFactsMap::iterator it = ICFGNodeToFacts.begin(), eit = ICFGNodeToFacts.end(); it != eit; ++it) {
        const ICFGNode *node = it->first;
        Facts facts = it->second;
        NodeID id = node->getId();
        std::cout << "ICFGNodeID:" << id << ": PAGNodeSet: {";
        Datafact finalFact = {};
        for (Facts::iterator fit = facts.begin(), efit = facts.end(); fit != efit; ++fit) {
            Datafact fact = (*fit);
            for (Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ++dit) {
                finalFact.insert(*dit);
            }
        }
        if (finalFact.size() > 1){
            for (Datafact::iterator dit = finalFact.begin(), edit = --finalFact.end(); dit != edit; ++dit)
                std::cout << (*dit)->getId() << " ";
            std::cout << (*finalFact.rbegin())->getId() << "}\n";   //print last element without " "
        }
        else if (finalFact.size() == 1)
            std::cout << (*finalFact.begin())->getId() << "}\n";
        else
            std::cout << "}\n";
    }
    printPathEdgeList();
    printSummaryEdgeList();
    validateTests("checkInit");
    validateTests("checkUninit");
    std::cout << "-------------------------------------------------------" << std::endl;
}
void IFDS::printPathEdgeList() {
    std::cout << "\n***********PathEdge**************\n";
    for (PathEdgeSet::const_iterator it = PathEdgeList.begin(), eit = PathEdgeList.end(); it != eit; ++it){
        NodeID srcID = (*it)->getSrcPathNode()->getICFGNode()->getId();
        NodeID dstID = (*it)->getDstPathNode()->getICFGNode()->getId();
        Datafact srcFact = (*it)->getSrcPathNode()->getDataFact();
        Datafact dstFact = (*it)->getDstPathNode()->getDataFact();

        cout << "[ICFGNodeID:" << srcID << ",(";
        for (Datafact::const_iterator it = srcFact.begin(), eit = srcFact.end(); it != eit; it++){
            std::cout << (*it)->getId() << " ";
        }
        if (!srcFact.empty())
            cout << "\b";
        cout << ")] --> [ICFGNodeID:" << dstID << ",(";
        for (Datafact::const_iterator it = dstFact.begin(), eit = dstFact.end(); it != eit; it++){
            std::cout << (*it)->getId() << " ";
        }
        if (!dstFact.empty())
            cout << "\b";
        cout << ")]\n";
    }
}
void IFDS::printSummaryEdgeList() {
    std::cout << "\n***********SummaryEdge**************\n";
    for (PathEdgeSet::const_iterator it = SummaryEdgeList.begin(), eit = SummaryEdgeList.end(); it != eit; ++it){
        NodeID srcID = (*it)->getSrcPathNode()->getICFGNode()->getId();
        NodeID dstID = (*it)->getDstPathNode()->getICFGNode()->getId();
        Datafact srcFact = (*it)->getSrcPathNode()->getDataFact();
        Datafact dstFact = (*it)->getDstPathNode()->getDataFact();

        cout << "[ICFGNodeID:" << srcID << ",(";
        for (Datafact::const_iterator it = srcFact.begin(), eit = srcFact.end(); it != eit; it++){
            std::cout << (*it)->getId() << " ";
        }
        if (!srcFact.empty())
            cout << "\b";
        cout << ")] --> [ICFGNodeID:" << dstID << ",(";
        for (Datafact::const_iterator it = dstFact.begin(), eit = dstFact.end(); it != eit; it++){
            std::cout << (*it)->getId() << " ";
        }
        if (!dstFact.empty())
            cout << "\b";
        cout << ")]\n";
    }
}
void IFDS::validateTests(const char *fun) {
    for (u32_t i = 0; i < icfg->getPAG()->getModule().getModuleNum(); ++i) {
        Module *module = icfg->getPAG()->getModule().getModule(i);
        if (Function *checkFun = module->getFunction(fun)) {
            for (Value::user_iterator i = checkFun->user_begin(), e =
                    checkFun->user_end(); i != e; ++i)
                if (SVFUtil::isa<CallInst>(*i) || SVFUtil::isa<InvokeInst>(*i)) {
                    CallSite cs(*i);
                    assert(cs.getNumArgOperands() == 1 && "arguments should one pointer!!");
                    Value *v1 = cs.getArgOperand(0);
                    NodeID ptr = icfg->getPAG()->getValueNode(v1);
                    PointsTo &pts = pta->getPts(ptr);
                    for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit; ++it) {
                        const PAGNode *objNode = icfg->getPAG()->getPAGNode(*it);
                        NodeID objNodeId = objNode->getId();
                        const CallBlockNode *callnode = icfg->getCallICFGNode(cs);
                        const Facts &facts = ICFGNodeToFacts[callnode];

                        bool initialize = true;
                        for (Facts::const_iterator fit = facts.begin(), efit = facts.end(); fit != efit; ++fit) {
                            const Datafact &fact = (*fit);
                            if (fact.count(objNode))
                                initialize = false;
                        }
                        if (strcmp(fun, "checkInit") == 0) {
                            if (initialize)
                                std::cout << sucMsg("SUCCESS: ") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                            else
                                std::cout << errMsg("FAIL: ") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                        } else if (strcmp(fun, "checkUninit") == 0) {
                            if (initialize)
                                std::cout << errMsg("FAIL: ") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                            else
                                std::cout << sucMsg("SUCCESS: ") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                        }
                    }
                }
        }
    }
}