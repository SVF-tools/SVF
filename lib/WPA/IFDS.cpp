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
    initialize();
    forwardTabulate();
    printRes();
    validateTests("checkInit");
    validateTests("checkUninit");
}

/*initialization
    PathEdgeList = {(<EntryMain, 0> --> <EntryMain,0>)}
    WorkList = {(<EntryMain, 0> --> <EntryMain,0>)}
    SummaryEdgeList = {}
 */
void IFDS::initialize() {

    Datafact datafact = {};    // datafact = 0;
    mainEntryNode = icfg->getFunEntryICFGNode(
            SVFUtil::getProgEntryFunction(getPAG()->getModule()));    //getProgEntryFunction needs to be public, edited
    PathNode *mainEntryPN = new PathNode(mainEntryNode, datafact);
    PathEdge *startPE = new PathEdge(mainEntryPN, mainEntryPN);
    PathEdgeList.push_back(startPE);
    WorkList.push_back(startPE);
    SummaryEdgeList = {};

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
        Datafact d1 = srcPN->getDataFact();
        PathNode *dstPN = e->getDstPathNode();
        const ICFGNode *n = e->getDstPathNode()->getICFGNode();

        if (SVFUtil::isa<CallBlockNode>(n)) {
            const ICFGEdge::ICFGEdgeSetTy &outEdges = n->getOutEdges();
            for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
                    outEdges.end(); it != eit; ++it) {
                // if this is Call Edge
                if ((*it)->isCallCFGEdge()) {
                    ICFGNode *sp = (*it)->getDstNode();
                    Datafact d = transferFun(dstPN);
                    PathNode *newSrcPN = new PathNode(sp, d);
                    propagate(newSrcPN, sp, d);
                }
                    // if it is CallToRetEdge
                else if ((*it)->isIntraCFGEdge()) {
                    Datafact d = transferFun(dstPN);
                    ICFGNode *ret = (*it)->getDstNode();
                    ICFGNodeToFacts[ret].insert(d);   //reduntant
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
            const ICFGEdge::ICFGEdgeSetTy &inEdges = sp->getInEdges();
            // for each caller
            for (ICFGEdge::ICFGEdgeSetTy::iterator it = inEdges.begin(), eit =
                    inEdges.end(); it != eit; ++it) {
                assert((*it)->isCallCFGEdge());     // only can be call edges
                ICFGNode *caller = (*it)->getSrcNode();

                CallCFGEdge *callEdge = SVFUtil::dyn_cast<CallCFGEdge>(*it); //downcast to CallCFGEdge  --> getCallSiteID
                ICFGNode *ret = icfg->getRetICFGNode(icfg->getCallSite(callEdge->getCallSiteId()));
                Datafact d5 = transferFun(dstPN);    // to do: add transfer function for exit node (only care about global var)
                ICFGNodeToFacts[ret].insert(d5);

                Datafact d4 = getCallerDatafact(srcPN, caller); // deduce d4 from d1 (backward direction!)
                if (isNotInSummaryEdgeList(caller, d4, ret, d5)) {    //d1 should be d4 ??
                    // insert new summary edge into SummaryEdgeList
                    PathNode *srcPN = new PathNode(caller, d4);
                    PathNode *dstPN = new PathNode(ret, d5);
                    PathEdge *e = new PathEdge(srcPN, dstPN);
                    SummaryEdgeList.push_back(e);
                    SummaryICFGNodeToFacts[caller].insert(d4);
                    SummaryICFGNodeToFacts[ret].insert(d5);

                    for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit = PathEdgeList.end();
                         pit != epit; ++pit) {
                        if (((*pit)->getDstPathNode()->getICFGNode() == caller)
                            && ((*pit)->getDstPathNode()->getDataFact() == d4))
                            propagate((*pit)->getSrcPathNode(), ret, d5);
                    }
                }
            }
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

bool IFDS::isNotInSummaryEdgeList(ICFGNode *n1, Datafact d1, ICFGNode *n2, Datafact d2) {
    SummaryICFGDstNodeSet = getSummaryDstICFGNodeSet();
    facts = SummaryICFGNodeToFacts[n1];
    facts2 = SummaryICFGNodeToFacts[n2];

    ICFGNodeSet::iterator it = SummaryICFGDstNodeSet.find(n2);
    Facts::iterator fit = facts.find(d1);
    Facts::iterator fit2 = facts2.find(d2);

    if (it == SummaryICFGDstNodeSet.end())
        return true;
    else if (fit2 == facts2.end())
        return true;
    else if (fit == facts.end())
        return true;
    else
        return false;
}

//get all ICFGNode in all EndPathNode of PathEdgeList
IFDS::ICFGNodeSet &IFDS::getDstICFGNodeSet() {
    for (PathEdgeSet::iterator pit = PathEdgeList.begin(), epit =
            PathEdgeList.end(); pit != epit; ++pit) {
        const ICFGNode *n = (*pit)->getDstPathNode()->getICFGNode();
        ICFGDstNodeSet.insert(n);
    }
    return ICFGDstNodeSet;
}

IFDS::ICFGNodeSet &IFDS::getSummaryDstICFGNodeSet() {
    for (PathEdgeSet::iterator pit = SummaryEdgeList.begin(), epit =
            SummaryEdgeList.end(); pit != epit; ++pit) {
        const ICFGNode *n = (*pit)->getDstPathNode()->getICFGNode();
        SummaryICFGDstNodeSet.insert(n);
    }
    return SummaryICFGDstNodeSet;
}

bool IFDS::isInitialized(const PAGNode *pagNode, Datafact datafact) {
    Datafact::iterator it = datafact.find(pagNode);
    if (it == datafact.end())
        return true;
    else
        return false;
}

//get d4 from d1
IFDS::Datafact IFDS::getCallerDatafact(PathNode *srcPN, ICFGNode *caller) {
    const ICFGNode *sp = srcPN->getICFGNode();
    Datafact d1 = srcPN->getDataFact();

    const CallBlockNode *node = SVFUtil::dyn_cast<CallBlockNode>(caller);

    for (CallBlockNode::ActualParmVFGNodeVec::const_iterator it = node->getActualParms().begin(), eit = node->getActualParms().end();
         it != eit; ++it) {
        const PAGNode *actualParmNode = (*it)->getParam();
        if (actualParmNode->hasOutgoingEdges(PAGEdge::Call)) {
            for (PAGEdge::PAGEdgeSetTy::const_iterator pit = actualParmNode->getOutgoingEdgesBegin(
                    PAGEdge::Call), epit = actualParmNode->getOutgoingEdgesEnd(PAGEdge::Call);
                 pit != epit; ++pit) {

                if (isInitialized((*pit)->getDstNode(), d1))
                    d1.erase(actualParmNode);
                else
                    d1.insert(actualParmNode);
            }
        }
    }
    return d1;
}

// StmtNode(excludes cmp and binaryOp)
// Addr: srcNode is uninitialized, dstNode is initialiazed
// copy: dstNode depends on srcNode
// Store: dstNode->obj depends on srcNode
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
            if (node->hasIncomingEdge() || node->hasOutgoingEdge()) {
                bool constant = false;
                if (node->isConstantData())
                    constant = true;
                if (ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(node))
                    if (objNode->getMemObj()->isGlobalObj() || objNode->getMemObj()->isFunction())
                        constant = true;
                PAGEdge::PAGEdgeSetTy edges = node->getIncomingEdges(PAGEdge::Addr);
                for (PAGEdge::PAGEdgeSetTy::iterator it = edges.begin(), eit = edges.end(); it != eit; ++it) {
                    PAGEdge *e = *it;
                    if (ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(e->getSrcNode()))
                        if (objNode->getMemObj()->isGlobalObj() || objNode->getMemObj()->isFunction())
                            constant = true;
                }
                if (DummyValPN *dummyValNode = SVFUtil::dyn_cast<DummyValPN>(node))
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
        }
    // backward direction to deduce d4 from d1 ??
    } else if (const FunEntryBlockNode *node = SVFUtil::dyn_cast<FunEntryBlockNode>(icfgNode)) {


        //code to add ..
    } else if (const FunExitBlockNode *node = SVFUtil::dyn_cast<FunExitBlockNode>(icfgNode)) {

        for (Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ++dit) {
            // erase local variable from datafact
            if (const ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(*dit))
                if (!objNode->getMemObj()->isGlobalObj())
                    fact.erase(*dit);
        }

    } else if (const CallBlockNode *node = SVFUtil::dyn_cast<CallBlockNode>(icfgNode)) {
        const ICFGEdge::ICFGEdgeSetTy &outEdges = node->getOutEdges();
        for (ICFGEdge::ICFGEdgeSetTy::iterator it = outEdges.begin(), eit =
                outEdges.end(); it != eit; ++it) {
            // if is Call Edge
            if ((*it)->isCallCFGEdge()) {
                for (CallBlockNode::ActualParmVFGNodeVec::const_iterator it = node->getActualParms().begin(), eit = node->getActualParms().end();
                     it != eit; ++it) {
                    const PAGNode *actualParmNode = (*it)->getParam();
                    if (actualParmNode->hasOutgoingEdges(PAGEdge::Call)) {
                        for (PAGEdge::PAGEdgeSetTy::const_iterator pit = actualParmNode->getOutgoingEdgesBegin(
                                PAGEdge::Call), epit = actualParmNode->getOutgoingEdgesEnd(PAGEdge::Call);
                             pit != epit; ++pit) {
                            if (isInitialized(actualParmNode, fact))
                                fact.erase((*pit)->getDstNode());
                            else
                                fact.insert((*pit)->getDstNode());
                        }
                    }
                }
            }
                // if is call-to-ret edge
            else if ((*it)->isIntraCFGEdge()) {
                // erase global variable from datafact
                for (Datafact::iterator dit = fact.begin(), edit = fact.end(); dit != edit; ++dit) {
                    if (const ObjPN *objNode = SVFUtil::dyn_cast<ObjPN>(*dit))
                        if (objNode->getMemObj()->isGlobalObj())
                            fact.erase(*dit);
                }
            }
        }
    } else if (const RetBlockNode *node = SVFUtil::dyn_cast<RetBlockNode>(icfgNode)) {
        //code to add ..
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
        for (Datafact::iterator dit = finalFact.begin(), edit = finalFact.end(); dit != edit; ++dit) {
            std::cout << (*dit)->getId() << " ";
        }
        std::cout << "}\n";
    }
    printPathEdgeList();
    printSummaryEdgeList();
    std::cout << "-------------------------------------------------------";
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
                                outs() << sucMsg("SUCCESS :") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                            else
                                errs() << errMsg("FAIL :") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                        } else if (strcmp(fun, "checkUninit") == 0) {
                            if (initialize)
                                outs() << errMsg("FAIL :") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                            else
                                errs() << sucMsg("SUCCESS :") << fun << " check <CFGId:" << callnode->getId()
                                       << ", objId:" << objNodeId << "> at ("
                                       << getSourceLoc(*i) << ")\n";
                        }
                    }
                }
        }
    }
}