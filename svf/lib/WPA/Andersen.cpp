//===- Andersen.cpp -- Field-sensitive Andersen's analysis-------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * Andersen.cpp
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "WPA/Andersen.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"


using namespace SVF;
using namespace SVFUtil;


Size_t AndersenBase::numOfProcessedAddr = 0;
Size_t AndersenBase::numOfProcessedCopy = 0;
Size_t AndersenBase::numOfProcessedGep = 0;
Size_t AndersenBase::numOfProcessedLoad = 0;
Size_t AndersenBase::numOfProcessedStore = 0;
Size_t AndersenBase::numOfSfrs = 0;
Size_t AndersenBase::numOfFieldExpand = 0;

Size_t AndersenBase::numOfSCCDetection = 0;
double AndersenBase::timeOfSCCDetection = 0;
double AndersenBase::timeOfSCCMerges = 0;
double AndersenBase::timeOfCollapse = 0;

Size_t AndersenBase::AveragePointsToSetSize = 0;
Size_t AndersenBase::MaxPointsToSetSize = 0;
double AndersenBase::timeOfProcessCopyGep = 0;
double AndersenBase::timeOfProcessLoadStore = 0;
double AndersenBase::timeOfUpdateCallGraph = 0;

/*!
 * Initilize analysis
 */
void AndersenBase::initialize()
{
    /// Build PAG
    PointerAnalysis::initialize();
    /// Build Constraint Graph
    consCG = new ConstraintGraph(pag);
    setGraph(consCG);
	if (Options::ConsCGDotGraph)
		consCG->dump("consCG_initial");
}

/*!
 * Finalize analysis
 */
void AndersenBase::finalize()
{
    /// dump constraint graph if PAGDotGraph flag is enabled
	if (Options::ConsCGDotGraph)
		consCG->dump("consCG_final");

	if (Options::PrintCGGraph)
		consCG->print();
    BVDataPTAImpl::finalize();
}

/*!
 * Andersen analysis
 */
void AndersenBase::analyze()
{
    /// Initialization for the Solver
    initialize();

    bool readResultsFromFile = false;
    if(!Options::ReadAnder.empty()) {
        readResultsFromFile = this->readFromFile(Options::ReadAnder);
        // Finalize the analysis
        PointerAnalysis::finalize();
    }

    if(!readResultsFromFile)
    {
        // Start solving constraints
        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Start Solving Constraints\n"));

        bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::AnderTimeLimit);

        initWorklist();
        do
        {
            numOfIteration++;
//            if (numOfIteration > 4) break;
            if (0 == numOfIteration % iterationForPrintStat)
                printStat();

            reanalyze = false;

            solveWorklist();

            if (updateCallGraph(getIndirectCallsites()))
                reanalyze = true;

            /*
            llvm::errs() << "Did one iteration\n";
            llvm::errs() << "Blacklisted edges and edges in constraint graph:\n";
            for (auto& tup: consCG->getBlackListEdges()) {
                NodeID src = std::get<0>(tup);
                NodeID dst = std::get<1>(tup);
                llvm::errs() << "blacklist edge details: " << src << " : " << dst << "\n";
                if (consCG->hasConstraintNode(src) && consCG->hasConstraintNode(dst)) {
                    ConstraintNode* srcNode = consCG->getConstraintNode(src);
                    ConstraintNode* dstNode = consCG->getConstraintNode(dst);
                    llvm::errs() << "constraint node found\n";
                    if (consCG->hasEdge(srcNode, dstNode, ConstraintEdge::Copy)) {
                        llvm::errs() << "----> blacklisted edge " << src << " " << dst << " reappeared!!!\n";
                    }
                }
            }
            */
        }
        while (reanalyze);

        // Analysis is finished, reset the alarm if we set it.
        SVFUtil::stopAnalysisLimitTimer(limitTimerSet);

        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Finish Solving Constraints\n"));

        // Finalize the analysis
        finalize();
    }

    if (!Options::WriteAnder.empty())
        this->writeToFile(Options::WriteAnder);
}

void AndersenBase::cleanConsCG(NodeID id) {
    consCG->resetSubs(consCG->getRep(id));
    for (NodeID sub: consCG->getSubs(id))
        consCG->resetRep(sub);
    consCG->resetSubs(id);
    consCG->resetRep(id);
}

void AndersenBase::normalizePointsTo()
{
    PAG::MemObjToFieldsMap &memToFieldsMap = pag->getMemToFieldsMap();
    PAG::NodeLocationSetMap &GepObjNodeMap = pag->getGepObjNodeMap();

    // clear GepObjNodeMap/memToFieldsMap/nodeToSubsMap/nodeToRepMap
    // for redundant gepnodes and remove those nodes from pag
    for (NodeID n: redundantGepNodes) {
        NodeID base = pag->getBaseObjNode(n);
        GepObjPN *gepNode = SVFUtil::dyn_cast<GepObjPN>(pag->getPAGNode(n));
        assert(gepNode && "Not a gep node in redundantGepNodes set");
        const LocationSet ls = gepNode->getLocationSet();
        GepObjNodeMap.erase(std::make_pair(base, ls));
        memToFieldsMap[base].reset(n);
        cleanConsCG(n);

        pag->removeGNode(gepNode);
    }
}

/*!
 * Initilize analysis
 */
void Andersen::initialize()
{
    resetData();
    setDiffOpt(Options::PtsDiff);
    setPWCOpt(Options::MergePWC);
    AndersenBase::initialize();
    /// Initialize worklist
    processAllAddr();
}

/*!
 * Finalize analysis
 */
void Andersen::finalize()
{
    /// sanitize field insensitive obj
    /// TODO: Fields has been collapsed during Andersen::collapseField().
    //	sanitizePts();
	AndersenBase::finalize();
}

/*!
 * Start constraint solving
 */
void Andersen::processNode(NodeID nodeId)
{
    // sub nodes do not need to be processed
    if (sccRepNode(nodeId) != nodeId)
        return;

    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    double insertStart = stat->getClk();
    handleLoadStore(node);
    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;

    double propStart = stat->getClk();
    handleCopyGep(node);
    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Process copy and gep edges
 */
void Andersen::handleCopyGep(ConstraintNode* node)
{
    NodeID nodeId = node->getId();
    computeDiffPts(nodeId);

    if (!getDiffPts(nodeId).empty())
    {
        for (ConstraintEdge* edge : node->getCopyOutEdges())
            processCopy(nodeId, edge);
        for (ConstraintEdge* edge : node->getGepOutEdges())
        {
            if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(edge))
                processGep(nodeId, gepEdge);
        }
    }
}

/*!
 * Process load and store edges
 */
void Andersen::handleLoadStore(ConstraintNode *node)
{
    NodeID nodeId = node->getId();
    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter =
                getPts(nodeId).end(); piter != epiter; ++piter)
    {
        NodeID ptd = *piter;
        // handle load
        for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(),
                eit = node->outgoingLoadsEnd(); it != eit; ++it)
        {
            if (processLoad(ptd, *it))
                pushIntoWorklist(ptd);
        }

        // handle store
        for (ConstraintNode::const_iterator it = node->incomingStoresBegin(),
                eit = node->incomingStoresEnd(); it != eit; ++it)
        {
            if (processStore(ptd, *it))
                pushIntoWorklist((*it)->getSrcID());
        }
    }
}

/*!
 * Process address edges
 */
void Andersen::processAllAddr()
{
    for (ConstraintGraph::const_iterator nodeIt = consCG->begin(), nodeEit = consCG->end(); nodeIt != nodeEit; nodeIt++)
    {
        ConstraintNode * cgNode = nodeIt->second;
        for (ConstraintNode::const_iterator it = cgNode->incomingAddrsBegin(), eit = cgNode->incomingAddrsEnd();
                it != eit; ++it)
            processAddr(SVFUtil::cast<AddrCGEdge>(*it));
    }
}

/*!
 * Process address edges
 */
void Andersen::processAddr(const AddrCGEdge* addr)
{
    numOfProcessedAddr++;

    NodeID dst = addr->getDstID();
    NodeID src = addr->getSrcID();
    if(addPts(dst,src))
        pushIntoWorklist(dst);
}

/*!
 * Process load edges
 *	src --load--> dst,
 *	node \in pts(src) ==>  node--copy-->dst
 */
bool Andersen::processLoad(NodeID node, const ConstraintEdge* load)
{
    /// TODO: New copy edges are also added for black hole obj node to
    ///       make gcc in spec 2000 pass the flow-sensitive analysis.
    ///       Try to handle black hole obj in an appropiate way.
//	if (pag->isBlkObjOrConstantObj(node) || isNonPointerObj(node))
    if (pag->isConstantObj(node) || isNonPointerObj(node))
        return false;

    numOfProcessedLoad++;

    NodeID dst = load->getDstID();
    return addCopyEdge(node, dst, 1, const_cast<ConstraintEdge*>(load));
}

/*!
 * Process store edges
 *	src --store--> dst,
 *	node \in pts(dst) ==>  src--copy-->node
 */
bool Andersen::processStore(NodeID node, const ConstraintEdge* store)
{
    /// TODO: New copy edges are also added for black hole obj node to
    ///       make gcc in spec 2000 pass the flow-sensitive analysis.
    ///       Try to handle black hole obj in an appropiate way
//	if (pag->isBlkObjOrConstantObj(node) || isNonPointerObj(node))
    if (pag->isConstantObj(node) || isNonPointerObj(node))
        return false;

    numOfProcessedStore++;

    NodeID src = store->getSrcID();
    return addCopyEdge(src, node, 1, const_cast<ConstraintEdge*>(store));
}

/*!
 * Process copy edges
 *	src --copy--> dst,
 *	union pts(dst) with pts(src)
 */
bool Andersen::processCopy(NodeID node, const ConstraintEdge* edge)
{
    numOfProcessedCopy++;

    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    const PointsTo& srcPts = getDiffPts(node);



    bool changed = unionPts(dst, srcPts);
    if (changed)
        pushIntoWorklist(dst);
    return changed;
}

/*!
 * Process gep edges
 *	src --gep--> dst,
 *	for each srcPtdNode \in pts(src) ==> add fieldSrcPtdNode into tmpDstPts
 *		union pts(dst) with tmpDstPts
 */
bool Andersen::processGep(NodeID, const GepCGEdge* edge)
{
    const PointsTo& srcPts = getDiffPts(edge->getSrcID());
    return processGepPts(srcPts, edge);
}

bool Andersen::canApplyPAInvariant(VariantGepCGEdge* vgepCGEdge, NodeID obj) {
    if (vgepCGEdge->isStructTy() && consCG->isArrayTy(obj)) {
        llvm::errs() << "Can't apply invariant: " << *(vgepCGEdge->getLLVMValue()) << "\n";
        return false;
    } else {
        return true;
    }
}
/*!
 * Compute points-to for gep edges
 */
bool Andersen::processGepPts(const PointsTo& pts, const GepCGEdge* edge)
{
    numOfProcessedGep++;

    PointsTo tmpDstPts;
    if (const VariantGepCGEdge* vgepCGEdge = SVFUtil::dyn_cast<VariantGepCGEdge>(edge))
    {
        // If a pointer is connected by a variant gep edge,
        // then set this memory object to be field insensitive,
        // unless the object is a black hole/constant.
        llvm::Value* llvmValue = vgepCGEdge->getLLVMValue();
        for (NodeID o : pts)
        {
            if (consCG->isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            } 
            
            PAGNode* objNode = pag->getPAGNode(o);
            
            if (Options::InvariantVGEP /*&& !mustCollapse*/) {
                // First of all, we believe that variable indices
                // when the type is a complex type, are most definitely accessing 
                // an element in the array.
                // GetElementPtrInst* vgep = vgepCGEdge->getLLVMValue();

                // For the rest, we add the invariant

                if (consCG->isStructTy(o)) {
                    // We assume these don't happen
                    // We will add the invariant later
                    pag->addPtdForVarGep(vgepCGEdge->getLLVMValue(), o);
                    PAGNode* node = pag->getPAGNode(o);
                    continue;
                } else if (consCG->isArrayTy(o)) {
                    LocationSet ls(0);
                    NodeID fieldSrcPtdNode = consCG->getGepObjNode(o, ls);
                    tmpDstPts.set(fieldSrcPtdNode);
                } else {
                    /*
                    LocationSet ls(0);
                    NodeID fieldSrcPtdNode = consCG->getGepObjNode(o, ls);
                    tmpDstPts.set(fieldSrcPtdNode);
                    */
                    // Add the field-insensitive node into pts.
                    NodeID baseId = consCG->getFIObjNode(o);
                    tmpDstPts.set(baseId);

                }
            } else {

                if (!isFieldInsensitive(o))
                {
                    PAGNode* ptdNode = pag->getPAGNode(o);
                    setObjFieldInsensitive(o);
                    consCG->addNodeToBeCollapsed(consCG->getBaseObjNode(o));
                }

                // Add the field-insensitive node into pts.
                NodeID baseId = consCG->getFIObjNode(o);
                tmpDstPts.set(baseId);
            }
        }
    }
    else if (const NormalGepCGEdge* normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
    {
        // TODO: after the node is set to field insensitive, handling invariant
        // gep edge may lose precision because offsets here are ignored, and the
        // base object is always returned.

        PAGNode* srcNode = pag->getPAGNode(normalGepEdge->getSrcID());


        for (NodeID o : pts)
        {
						/*
            if (Options::PreventCollapseExplosion) {
                //llvm::errs() << "Doing GEP for nodeid: " << srcNode->getId() << "\n";
                if (srcNode->getDiffPtd().test(o)) {
                    PAGNode* obj = pag->getPAGNode(o);
                    llvm::errs() << "Must prevent explosion for :" << *obj << "\n";
                }
            }
						*/
            if (consCG->isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            if (!matchType(edge->getSrcID(), o, normalGepEdge)) continue;


            NodeID fieldSrcPtdNode = consCG->getGepObjNode(o, normalGepEdge->getLocationSet());

            tmpDstPts.set(fieldSrcPtdNode);
            addTypeForGepObjNode(fieldSrcPtdNode, normalGepEdge);
        }
    }
    else
    {
        assert(false && "Andersen::processGepPts: New type GEP edge type?");
    }

    NodeID dstId = edge->getDstID();

    if (Options::LogAll) {
        llvm::errs() << "$$ ------------\n";
        NodeID src = edge->getSrcID();
        NodeID dst = edge->getDstID();

        llvm::errs() << "$$ Solving gep edge between: " << src << " and " << dst << "\n";

        PAGNode* srcNode = pag->getPAGNode(src);
        PAGNode* dstNode = pag->getPAGNode(dst);

        if (srcNode->hasValue()) {
            const Value* srcVal = srcNode->getValue();
            llvm::errs() << "$$ Src value: " << *srcVal << " : " << SVFUtil::getSourceLoc(srcVal) << "\n";
        }

        if (dstNode->hasValue()) {
            const Value* dstVal = dstNode->getValue();
            llvm::errs() << "$$ Dst value: " << *dstVal << " : " << SVFUtil::getSourceLoc(dstVal) << "\n";
        }

        if (edge->getLLVMValue()) {
            Value* gepVal = edge->getLLVMValue();
            if (Instruction* inst = SVFUtil::dyn_cast<Instruction>(gepVal)) {
                llvm::errs() << "$$ Processing gep edge: [PRIMARY] : " << edge->getEdgeID() << " : "  << *inst << " : " << inst->getFunction()->getName() << "\n";
            } else {
                llvm::errs() << "$$ Processing gep edge: [PRIMARY] : " << edge->getEdgeID() << " : " << *gepVal << "\n";
            }
        } else {
            llvm::errs() << "$$ Processing gep edge: [PRIMARY] : " << edge->getEdgeID() << " : NO VALUE!\n";
        }
        for (NodeBS::iterator nodeIt = tmpDstPts.begin(); nodeIt != tmpDstPts.end(); nodeIt++) {
            NodeID ptd = *nodeIt;
            PAGNode* pagNode = pag->getPAGNode(ptd);
            int idx = -1;
            if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(pagNode)) {
                idx = gepNode->getLocationSet().getOffset();
            }
            if (pagNode->hasValue()) {
                Value* ptdValue = const_cast<Value*>(pagNode->getValue());
                if (Function* f = SVFUtil::dyn_cast<Function>(ptdValue)) {
                    llvm::errs() << "$$ PTD : " << ptd << " Function : " << f->getName() << " : " << SVFUtil::getSourceLoc(f) << "\n";
                } else if (Instruction* I = SVFUtil::dyn_cast<Instruction>(ptdValue)) {

                    llvm::errs() << "$$ PTD : " << ptd << " Stack object: " << *I << " : " << idx << " : " << I->getFunction()->getName() << " : " << SVFUtil::getSourceLoc(I) << "\n";
                } else if (GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(ptdValue)) {
                    llvm::errs() << "$$ PTD : " << ptd << " Global variable: " << *v << " : " << idx << " : " << *v << " : " << SVFUtil::getSourceLoc(v) << "\n";
                }
            } else {
                llvm::errs() << "$$ PTD : " << ptd << "PTD Dummy node: " << ptd << " : " << idx << "\n";
            }
            
        }
        llvm::errs() << "$$ ------------\n";
    }

    if (unionPts(dstId, tmpDstPts))
    {
        pushIntoWorklist(dstId);
        return true;
    }

    return false;
}

/**
 * Detect and collapse PWC nodes produced by processing gep edges, under the constraint of field limit.
 */
inline void Andersen::collapsePWCNode(NodeID nodeId)
{
    // If a node is a PWC node, collapse all its points-to tarsget.
    // collapseNodePts() may change the points-to set of the nodes which have been processed
    // before, in this case, we may need to re-do the analysis.
    if (mergePWC() && consCG->isPWCNode(nodeId) && collapseNodePts(nodeId))
        reanalyze = true;
}

inline void Andersen::collapseFields()
{
    while (consCG->hasNodesToBeCollapsed())
    {
        //llvm::errs() << "Collapsing fields\n";
        NodeID node = consCG->getNextCollapseNode();
        /*
        if (consCG->isStructTy(node)) {
            llvm::errs() << "Collapsing struct type object\n";
        } else if (consCG->isArrayTy(node)) {
            llvm::errs() << "Collapsing array type object\n";
        }
        */
        // collapseField() may change the points-to set of the nodes which have been processed
        // before, in this case, we may need to re-do the analysis.
        if (collapseField(node))
            reanalyze = true;
    }
}

/*
 * Merge constraint graph nodes based on SCC cycle detected.
 */
void Andersen::mergeSccCycle()
{
    NodeStack revTopoOrder;
    NodeStack & topoOrder = getSCCDetector()->topoNodeStack();
    while (!topoOrder.empty())
    {
        NodeID repNodeId = topoOrder.top();
        topoOrder.pop();
        revTopoOrder.push(repNodeId);
        const NodeBS& subNodes = getSCCDetector()->subNodes(repNodeId);
        // merge sub nodes to rep node
        mergeSccNodes(repNodeId, subNodes);
    }

    // restore the topological order for later solving.
    while (!revTopoOrder.empty())
    {
        NodeID nodeId = revTopoOrder.top();
        revTopoOrder.pop();
        topoOrder.push(nodeId);
    }
}

/*
bool Andersen::addInvariant(ConstraintEdge* edge) {
    ConstraintEdge* srcEdge = edge->getSourceEdge();
    if (LoadCGEdge* loadEdge = dyn_cast<LoadCGEdge>(srcEdge)) {
        NodeID ptdID = edge->getSrcID();
        PAGNode* ptdNode = pag->getPAGNode(ptdID);
        if (SVFUtil::isa<GepObjPN>(ptdNode)) {
            return false;
        } else if(isFieldInsensitive(ptdID)) {
            return false;
        }

        if (!ptdNode->hasValue()) {
            llvm::errs() << "ptd doesn't have value\n";
            return false;
        }
        Value* ptdValue = const_cast<Value*>(ptdNode->getValue());
        if (SVFUtil::isa<CallInst>(ptdValue)) {
            return true;
        }
        if (instrumentInvariant(loadEdge->getLLVMValue(), ptdValue)) {
            return true;
        }

    } else if (StoreCGEdge* storeEdge = dyn_cast<StoreCGEdge>(srcEdge)) {
        NodeID ptdID = edge->getDstID();
        PAGNode* ptdNode = pag->getPAGNode(ptdID);
        if (SVFUtil::isa<GepObjPN>(ptdNode)) {
            return false;
        } else if(isFieldInsensitive(ptdID)) {
                return false;
        }
        if (!ptdNode->hasValue()) {
            llvm::errs() << "ptd doesn't have value\n";
            return false;
        }
        Value* ptdValue = const_cast<Value*>(ptdNode->getValue());
        if (SVFUtil::isa<CallInst>(ptdValue)) {
            return true;
        }
        if (instrumentInvariant(storeEdge->getLLVMValue(), ptdValue)) {
            return true;
        }

    }
    return false;
}
*/

void Andersen::addCycleInvariants(CycleID pwcID, PAG::PWCList* gepNodesInSCC) {
    pag->addPWCInvariants(pwcID, gepNodesInSCC);
}

void Andersen::handlePointersAsPA(std::set<const llvm::Value*>* gepsInPWC) {
    // TODO pick a vgep outside of a loop if possible
    for (const llvm::Value* gep: *gepsInPWC) {
        pag->addPtdForVarGep(gep, -1); 
        break;
    }
}

/**
 * Union points-to of subscc nodes into its rep nodes
 * Move incoming/outgoing direct edges of sub node to rep node
 */
void Andersen::mergeSccNodes(NodeID repNodeId, const NodeBS& subNodes)
{
    std::vector<ConstraintEdge*> criticalGepEdges;
    std::vector<PAGNode*> subPAGNodes;
    bool treatAsPAInv = false;
    bool inLoop = false;
    bool hasStructType = false;


    // Check if there's going to be a PWC
    // If yes, then don't collapse the fields
		// We still merge the node to the rep because we can't have cycles
		// in AndersenWaveDiff algorithm.

    std::set<const Function*> cycleFuncSet;
    for (NodeBS::iterator nodeIt = subNodes.begin(); nodeIt != subNodes.end(); nodeIt++)
    {
        NodeID subNodeId = *nodeIt;
        PAGNode* pagNode = pag->getPAGNode(subNodeId);
        if (Options::DumpCycle) {
            if (subNodes.count() > 1) {
                if (pagNode->hasValue()) {
                    const Value* v = pagNode->getValue();
                    if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(v)) {
                        llvm::errs() << "Node: " << *inst << " in function: " << inst->getParent()->getParent()->getName() << "\n";
                        cycleFuncSet.insert(inst->getParent()->getParent());
                    } else {
                        llvm::errs() << "Node: " << *(pagNode->getValue()) << "\n";
                    }
                }
            }
        }

        subPAGNodes.push_back(pagNode);

        if (subNodeId != repNodeId) {
            mergeNodeToRep(subNodeId, repNodeId, criticalGepEdges);
        }
    }

    std::set<const llvm::Value*>* gepNodesInSCC = new std::set<const llvm::Value*>();
    if (criticalGepEdges.size() > 0) {
        if (Options::InvariantPWC) {
            // is a PWC
            for (PAGNode* pagNode: subPAGNodes) {
                if (pagNode->hasValue()) {
                    const Value* pagVal = pagNode->getValue();
                    if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(pagVal)) {
                        BasicBlock* bb = const_cast<BasicBlock*>(inst->getParent());
                        Function* func = bb->getParent();
                        // TODO handle the loop correctly   
												// I think this is the case where the PWC manifests as
												// p = p + k
												/*
                        if (svfLoopInfo->bbIsLoop(bb)) {
                            inLoop = true;
                        }
												*/
                        if (const GetElementPtrInst* gepVal = SVFUtil::dyn_cast<GetElementPtrInst>(inst)) {
                            Type* gepPtrTy = gepVal->getPointerOperand()->getType();
                            while (SVFUtil::isa<PointerType>(gepPtrTy)) {
                                PointerType* pty = SVFUtil::dyn_cast<PointerType>(gepPtrTy);
                                gepPtrTy = pty->getPointerElementType();
                            }
                            hasStructType = SVFUtil::isa<StructType>(gepPtrTy);

                            gepNodesInSCC->insert(gepVal);
                        }
                    }

                }
            }
            treatAsPAInv = inLoop && !hasStructType;
            if (!treatAsPAInv) {
                pwcCycleId++;
                addCycleInvariants(pwcCycleId, gepNodesInSCC);
            } else {
                // We assume these can never point to structs, so no need to do a field sensitive analysis
                handlePointersAsPA(gepNodesInSCC);
            }
        } else {
            consCG->setPWCNode(repNodeId);
        }
    }

    delete(gepNodesInSCC);
}

/**
 * Collapse node's points-to set. Change all points-to elements into field-insensitive.
 */
bool Andersen::collapseNodePts(NodeID nodeId)
{
    //llvm::errs() << "Collapsing node pts\n";
    bool changed = false;
    const PointsTo& nodePts = getPts(nodeId);
    /// Points to set may be changed during collapse, so use a clone instead.
    PointsTo ptsClone = nodePts;
    for (PointsTo::iterator ptsIt = ptsClone.begin(), ptsEit = ptsClone.end(); ptsIt != ptsEit; ptsIt++)
    {
        if (isFieldInsensitive(*ptsIt))
            continue;

        if (collapseField(*ptsIt))
            changed = true;
    }
    return changed;
}

/**
 * Collapse field. make struct with the same base as nodeId become field-insensitive.
 */
bool Andersen::collapseField(NodeID nodeId)
{
    /// Black hole doesn't have structures, no collapse is needed.
    /// In later versions, instead of using base node to represent the struct,
    /// we'll create new field-insensitive node. To avoid creating a new "black hole"
    /// node, do not collapse field for black hole node.
    if (consCG->isBlkObjOrConstantObj(nodeId))
        return false;

    bool changed = false;

    double start = stat->getClk();

    // set base node field-insensitive.
    setObjFieldInsensitive(nodeId);

    // replace all occurrences of each field with the field-insensitive node
    NodeID baseId = consCG->getFIObjNode(nodeId);
    NodeID baseRepNodeId = consCG->sccRepNode(baseId);
    NodeBS & allFields = consCG->getAllFieldsObjNode(baseId);
    std::vector<ConstraintEdge*> criticalGepEdges;
    for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end(); fieldIt != fieldEit; fieldIt++)
    {
        NodeID fieldId = *fieldIt;
        if (fieldId != baseId)
        {
            // use the reverse pts of this field node to find all pointers point to it
            const NodeSet revPts = getRevPts(fieldId);
            for (const NodeID o : revPts)
            {
                // change the points-to target from field to base node
                clearPts(o, fieldId);
                addPts(o, baseId);
                pushIntoWorklist(o);

                changed = true;
            }
            // merge field node into base node, including edges and pts.
            NodeID fieldRepNodeId = consCG->sccRepNode(fieldId);
            if (fieldRepNodeId != baseRepNodeId)
                mergeNodeToRep(fieldRepNodeId, baseRepNodeId, criticalGepEdges);
            /// TODO: I don't think we care about criticalGepEdges here.
            /// We're any way turning the object into field sensitive
            /// This isn't a PWC situation

            // collect each gep node whose base node has been set as field-insensitive
            redundantGepNodes.set(fieldId);
        }
    }

    if (consCG->isPWCNode(baseRepNodeId))
        if (collapseNodePts(baseRepNodeId))
            changed = true;

    double end = stat->getClk();
    timeOfCollapse += (end - start) / TIMEINTERVAL;

    return changed;
}

/*!
 * SCC detection on constraint graph
 */
NodeStack& Andersen::SCCDetect()
{
    numOfSCCDetection++;

    double sccStart = stat->getClk();
    WPAConstraintSolver::SCCDetect();
    double sccEnd = stat->getClk();

    timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    double mergeStart = stat->getClk();

    mergeSccCycle();

    double mergeEnd = stat->getClk();

    timeOfSCCMerges +=  (mergeEnd - mergeStart)/TIMEINTERVAL;

    return getSCCDetector()->topoNodeStack();
}

/*!
 * Update call graph for the input indirect callsites
 */
bool Andersen::updateCallGraph(const CallSiteToFunPtrMap& callsites)
{

    double cgUpdateStart = stat->getClk();

    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    NodePairSet cpySrcNodes;	/// nodes as a src of a generated new copy edge
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it )
    {
        CallSite cs = SVFUtil::getLLVMCallSite(it->first->getCallSite());
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            connectCaller2CalleeParams(cs,*cit,cpySrcNodes);
        }
    }
    for(NodePairSet::iterator it = cpySrcNodes.begin(), eit = cpySrcNodes.end(); it!=eit; ++it)
    {
        pushIntoWorklist(it->first);
    }

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}

std::tuple<ConstraintEdge*, Instruction*, Value*> Andersen::pickCycleEdgeToBreak(std::set<ConstraintEdge*>& cycleEdges) {
    ConstraintEdge* candidateEdge = nullptr;
    for (ConstraintEdge* candidateEdge: cycleEdges) {
        PAGNode* srcNode = pag->getPAGNode(candidateEdge->getSrcID());
        PAGNode* dstNode = pag->getPAGNode(candidateEdge->getDstID());
        Type* srcTy = nullptr;
        Type* dstTy = nullptr;

        if (srcNode) {
            srcTy = const_cast<Type*>(srcNode->getType());
        }
        if (dstNode) {
            dstTy = const_cast<Type*>(dstNode->getType());
        }

        if (srcTy == dstTy)
            continue;

        /*
        if (!Options::KaliBreakNullTypeEdges) {
            if (!srcTy || !dstTy) {
                //llvm::errs() << "srcTy = null? " << srcTy << " dstTy = null? " << dstTy << "\n";
                continue;
            }
        }
        */

        if (!candidateEdge->getSourceEdge()) {
            continue;
        }

        if (candidateEdge->getDerivedWeight() > 0) {
            ConstraintEdge* srcEdge = candidateEdge->getSourceEdge();

            LoadInst* loadMemInst = SVFUtil::dyn_cast<LoadInst>(srcEdge->getLLVMValue());
            StoreInst* storeMemInst = SVFUtil::dyn_cast<StoreInst>(srcEdge->getLLVMValue());

            // We don't handle any edge other than load and store
            if (!loadMemInst && !storeMemInst) {
                continue;
            }

            PAGNode* tgtPtdNode = nullptr;
            if (LoadCGEdge* loadEdge = SVFUtil::dyn_cast<LoadCGEdge>(srcEdge)) {
                NodeID ptdID = candidateEdge->getSrcID();
                tgtPtdNode = pag->getPAGNode(ptdID);
                
            } else if (StoreCGEdge* storeEdge = SVFUtil::dyn_cast<StoreCGEdge>(srcEdge)) {
                NodeID ptdID = candidateEdge->getDstID();
                tgtPtdNode = pag->getPAGNode(ptdID);
            }

            // TODO: Sometimes getting a ValPN here... why? 
            if (!SVFUtil::isa<ObjPN>(tgtPtdNode)) {
                continue;
            }

            if (SVFUtil::isa<GepObjPN>(tgtPtdNode)) {
                continue;
            } else if(isFieldInsensitive(tgtPtdNode->getId())) {
                continue;
            }

            if (!tgtPtdNode->hasValue()) {
                continue;
            }
            Value* tgtValue = const_cast<Value*>(tgtPtdNode->getValue());

            NodeID valID = (pag->hasValueNode(tgtValue)? pag->getValueNode(tgtValue): -1);
            
            if (SVFUtil::isa<CallInst>(tgtValue)) {
                continue;
            }

            Instruction* memInst = nullptr;
            // Don't pick obvious edges. These are the ones that were created
            // by the C -> IR transformations
            if (loadMemInst) {
                if (loadMemInst->getPointerOperand() == tgtValue) {
                    continue;
                }
                memInst = loadMemInst;
            } else if (storeMemInst) {
                if (storeMemInst->getPointerOperand() == tgtValue) {
                    continue;
                }
                memInst = storeMemInst;
            } else {
                assert(false && "What else?");
            }

            //llvm::errs() << "Returning edge " << candidateEdge->getSrcID() << " : " << candidateEdge->getDstID() << "\n";
            return std::make_tuple(candidateEdge, memInst, tgtValue);

        }
    }
    return std::make_tuple(nullptr, nullptr, nullptr);
}

void Andersen::instrumentInvariant(Instruction* memoryInst, Value* target) {
    // Some types
    /*
    LLVMContext& C = target->getContext();
    Type* voidPtrTy = PointerType::get(Type::getInt8Ty(C), 0);
    IntegerType* i64Ty = IntegerType::get(C, 64);

    // We need to check the pointer operand of the memory instruction
    // does not point to target

    // So first we record the last value of the address of the target (for
    // stack targets), or the returned address for mallocs etc


    Module* mod = memoryInst->getParent()->getParent()->getParent();
    int id = -1;
    bool targetRecorded = false;

    if (valueToKaliIdMap.find(target) == valueToKaliIdMap.end()) {
        // Give this guy a static id
        id = kaliInvariantId++;
        valueToKaliIdMap[target] = id;
        kaliIdToValueMap[id] = target;
    } else {
        id = valueToKaliIdMap[target];
        targetRecorded = true;
    }

    // Get the global map
    GlobalVariable* kaliMapGVar = mod->getGlobalVariable("kaliMap");
    assert(kaliMapGVar && "can't find KaliMap");

    // Get the index into the kaliMap
    Constant* zero = ConstantInt::get(i64Ty, 0);
    Constant* idConstant = ConstantInt::get(i64Ty, id);
    std::vector<Value*> idxVec;
    idxVec.push_back(zero);
    idxVec.push_back(idConstant);
    llvm::ArrayRef<Value*> idxs(idxVec);

    if (!targetRecorded) {
        if (!SVFUtil::isa<GlobalVariable>(target)) {
            // TODO: Can also be a GEP
            // Need to assess this carefully for both the source and target
            assert(SVFUtil::isa<AllocaInst>(target) || SVFUtil::isa<CallInst>(target) || SVFUtil::isa<LoadInst>(target) || SVFUtil::isa<GetElementPtrInst>(target) && "what else can it be?");
            Instruction* targetInst = SVFUtil::dyn_cast<Instruction>(target); 

            // Now instrument the stack allocation to store the address into the
            // the global kaliMap
            IRBuilder builder(targetInst->getNextNode());

            // Get the index into the kaliMap
            Value* gepIndexMapEntry = builder.CreateGEP(kaliMapGVar, idxs);
            //Value* gepIndexAddress = builder.CreateGEP(gepIndexMapEntry, idConstant);

            // Cast the address to void*
            Value* voidPtrAddress = builder.CreateBitCast(targetInst, voidPtrTy);

            // Do the store now!
            StoreInst* storeInst = builder.CreateStore(voidPtrAddress, gepIndexMapEntry);
        } else {
            GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(target);

            Function* mainFunction = mod->getFunction("main");
            Instruction* inst = mainFunction->getEntryBlock().getFirstNonPHIOrDbg();
            IRBuilder builder(inst);

            Value* gepIndexMapEntry = builder.CreateGEP(kaliMapGVar, idxs);
            //Value* gepIndexAddress = builder.CreateGEP(gepIndexMapEntry, idConstant);

            // Cast the address to void*
            Value* voidPtrAddress = builder.CreateBitCast(target, voidPtrTy);

            // Do the store now!
            StoreInst* storeInst = builder.CreateStore(voidPtrAddress, gepIndexMapEntry);
            inst->getParent()->dump();
        }
    }


    //llvm::errs() << "Target = " << *target << "\n";
    // IN case of a load, the memory instruction does not need to be
    // instrumented any further
    LoadInst* ldPtrInst = nullptr;
    if (LoadInst* ldInst = SVFUtil::dyn_cast<LoadInst>(memoryInst)) {
        ldPtrInst = ldInst;
    } else if (StoreInst* storeInst = SVFUtil::dyn_cast<StoreInst>(memoryInst)) {
        Value* ptr = storeInst->getPointerOperand();
        Instruction* storePtrInst = SVFUtil::dyn_cast<Instruction>(ptr);
        assert(storePtrInst && "store pointer not inst?");
        IRBuilder builder1(storePtrInst->getNextNode());
        // llvm::errs() << "Adding instrumentation after store inst: " << *storePtrInst << " in function: " << storePtrInst->getParent()->getParent()->getName() << "\n";
        Type* loadTy = ptr->getType()->getPointerElementType();
        assert(loadTy && "Should be of pointer type");
        ldPtrInst = builder1.CreateLoad(loadTy, storePtrInst);
    } else {
        assert(false && "what is this instruction?");
    }

    IRBuilder builder(ldPtrInst->getNextNode());
    // Cast it to void*
    Value* bitCastInst = builder.CreateBitCast(ldPtrInst, voidPtrTy); // <== 1 
    
    // Load the target value from the kaliMap

    Value* gepIndexMapEntry = builder.CreateGEP(kaliMapGVar, idxs);
    Value* gepIndexAddress = builder.CreateGEP(gepIndexMapEntry, idConstant);

    LoadInst* targetLoadVal = builder.CreateLoad(voidPtrTy, gepIndexAddress); // <=== 2

    // Compare 1 and 2
    Value* cmp = builder.CreateICmpEQ(bitCastInst, targetLoadVal);
    Instruction* cmpInst = SVFUtil::dyn_cast<Instruction>(cmp);
    assert(cmpInst && "Cmp must be instruction");

    // Ah, now split the current basic block
    BasicBlock* headBB = ldPtrInst->getParent();
    Instruction* termInst = llvm::SplitBlockAndInsertIfThen(cmpInst, cmpInst->getNextNode(), false);

    // Insert call to the switch view function
    Function* switchViewFn = mod->getFunction("switch_view");
    IRBuilder switcherBuilder(termInst);
    switcherBuilder.CreateCall(switchViewFn->getFunctionType(), switchViewFn);
    */
    
}

void Andersen::heapAllocatorViaIndCall(CallSite cs, NodePairSet &cpySrcNodes)
{
    assert(SVFUtil::getCallee(cs) == nullptr && "not an indirect callsite?");
    RetBlockNode* retBlockNode = pag->getICFG()->getRetBlockNode(cs.getInstruction());
    const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
    NodeID srcret;
    // It matches it with the call-site
    CallSite2DummyValPN::const_iterator it = callsite2DummyValPN.find(cs);

    if(it != callsite2DummyValPN.end())
    {
        srcret = sccRepNode(it->second);
    }
    else
    {
        NodeID valNode = pag->addDummyValNode();
        NodeID objNode = pag->addDummyObjNode(cs.getType());
        addPts(valNode,objNode);
        callsite2DummyValPN.insert(std::make_pair(cs,valNode));
        consCG->addConstraintNode(new ConstraintNode(valNode),valNode);
        consCG->addConstraintNode(new ConstraintNode(objNode),objNode);
        srcret = valNode;
        // Get the Call type
        CallBase* callBase = cs.getInstruction();
        CallInst* heapCall = SVFUtil::dyn_cast<CallInst>(callBase);

        assert(heapCall && "Must be an instruction");
        if (heapCall->hasMetadata("annotation")) {
            MDNode* mdNode = heapCall->getMetadata("annotation");
            MDString* tyAnnotStr = (MDString*)mdNode->getOperand(0).get();
            if (tyAnnotStr->getString() == "ArrayType") {
                consCG->addArrayIndHeapCall(objNode);
            } else if (tyAnnotStr->getString() == "StructType") {
                consCG->addStructIndHeapCall(objNode);
            }
        }
    }

    NodeID dstrec = sccRepNode(cs_return->getId());
    if(addCopyEdge(srcret, dstrec))
        cpySrcNodes.insert(std::make_pair(srcret,dstrec));
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */
void Andersen::connectCaller2CalleeParams(CallSite cs, const SVFFunction* F, NodePairSet &cpySrcNodes)
{
    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite " << *cs.getInstruction() << " to callee " << *F << "\n");

    CallBlockNode* callBlockNode = pag->getICFG()->getCallBlockNode(cs.getInstruction());
    RetBlockNode* retBlockNode = pag->getICFG()->getRetBlockNode(cs.getInstruction());

		if (matchArgs(callBlockNode, F) == false) {
			return;
		}


    if(SVFUtil::isHeapAllocExtFunViaRet(F) && pag->callsiteHasRet(retBlockNode))
    {
        heapAllocatorViaIndCall(cs,cpySrcNodes);
    }

    if (pag->funHasRet(F) && pag->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = pag->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer())
        {
            NodeID dstrec = sccRepNode(cs_return->getId());
            NodeID srcret = sccRepNode(fun_return->getId());
            if(addCopyEdge(srcret, dstrec))
            {
                cpySrcNodes.insert(std::make_pair(srcret,dstrec));
            }
        }
        else
        {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (pag->hasCallSiteArgsMap(callBlockNode) && pag->hasFunArgsList(F))
    {

        // connect actual and formal param
        const PAG::PAGNodeList& csArgList = pag->getCallSiteArgsList(callBlockNode);
        const PAG::PAGNodeList& funArgList = pag->getFunArgsList(F);
        //Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        PAG::PAGNodeList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        PAG::PAGNodeList::const_iterator csArgIt  = csArgList.begin(), csArgEit = csArgList.end();
        for (; funArgIt != funArgEit && csArgIt != csArgEit; ++csArgIt, ++funArgIt)
        {
            //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
            if (csArgIt  == csArgEit)
            {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode *cs_arg = *csArgIt ;
            const PAGNode *fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer())
            {
                DBOUT(DAndersen, outs() << "process actual parm  " << cs_arg->toString() << " \n");
                NodeID srcAA = sccRepNode(cs_arg->getId());
                NodeID dstFA = sccRepNode(fun_arg->getId());
                if(addCopyEdge(srcAA, dstFA))
                {
                    cpySrcNodes.insert(std::make_pair(srcAA,dstFA));
                }
            }
        }

        //Any remaining actual args must be varargs.
        if (F->isVarArg())
        {
            NodeID vaF = sccRepNode(pag->getVarargNode(F));
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt)
            {
                const PAGNode *cs_arg = *csArgIt;
                if (cs_arg->isPointer())
                {
                    NodeID vnAA = sccRepNode(cs_arg->getId());
                    if (addCopyEdge(vnAA,vaF))
                    {
                        cpySrcNodes.insert(std::make_pair(vnAA,vaF));
                    }
                }
            }
        }
        if(csArgIt != csArgEit)
        {
            writeWrnMsg("too many args to non-vararg func.");
            writeWrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");
        }
    }
}

/*!
 * merge nodeId to newRepId. Return true if the newRepId is a PWC node
 */
bool Andersen::mergeSrcToTgt(NodeID nodeId, NodeID newRepId, std::vector<ConstraintEdge*>& criticalGepEdges)
{

    if(nodeId==newRepId)
        return false;

		/*
    if (Options::PreventCollapseExplosion) {
        const PointsTo& repPtd = getPts(newRepId); // rep node's points-to set
        const PointsTo& nodePtd = getPts(nodeId);  // the mergee node's points to set
        PointsTo diffPtd = repPtd;
        diffPtd.intersectWithComplement(nodePtd);  // subtract to get the new points-to nodes
        llvm::errs() << "Merging " << nodeId << " to rep " << newRepId << " with diffptd " << diffPtd.count () << "\n";
        PAGNode* repNode = pag->getPAGNode(newRepId);
        PointsTo gepDiffPtd;
        for (NodeID o: diffPtd) {
            PAGNode* obj = pag->getPAGNode(o);
            if (GepObjPN* gep = SVFUtil::dyn_cast<GepObjPN>(obj)) {
                gepDiffPtd.set(o);
            }
        }

        repNode->updateDiffPtd(gepDiffPtd);
    }
		*/

    /// union pts of node to rep
    updatePropaPts(newRepId, nodeId);
    unionPts(newRepId,nodeId);

    if (Options::LogAll) {
        llvm::errs() << "$$ --------\n";
        llvm::errs() << "$$ Merging " << nodeId << " to rep " << newRepId << "\n";
        const PointsTo& ptd = getPts(newRepId);
        PAGNode* mergeeNode = pag->getPAGNode(nodeId);
        PAGNode* repNode = pag->getPAGNode(newRepId);

        int idx = -1;
        if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(mergeeNode)) {
            idx = gepNode->getLocationSet().getOffset();
        }

        if (mergeeNode->hasValue()) {
            Value* ptrValue = const_cast<Value*>(mergeeNode->getValue());
            if (Function* f = SVFUtil::dyn_cast<Function>(ptrValue)) {
                llvm::errs() << "$$ MERGEE : " << nodeId << " Function : " << f->getName() << " : " << SVFUtil::getSourceLoc(f) << "\n";
            } else if (Instruction* I = SVFUtil::dyn_cast<Instruction>(ptrValue)) {
                llvm::errs() << "$$ MERGEE : " << nodeId << " Stack object: " << *I << " : " << idx << " : " << I->getFunction()->getName() << SVFUtil::getSourceLoc(I) << "\n";

            } else if (GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(ptrValue)) {
                llvm::errs() << "$$ MERGEE : " << nodeId << " Global variable: " << *v << " : " << idx << " : " << *v << " : " << SVFUtil::getSourceLoc(v) << "\n";
            }
        } else {
            llvm::errs() << "$$ MERGEE : " << nodeId << "MERGEE Dummy node: " << idx << "\n";
        }

        if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(repNode)) {
            idx = gepNode->getLocationSet().getOffset();
        }
        if (repNode->hasValue()) {
            Value* ptrValue = const_cast<Value*>(repNode->getValue());
            if (Function* f = SVFUtil::dyn_cast<Function>(ptrValue)) {
                llvm::errs() << "$$ REP: " << newRepId << " Function : " << f->getName() << "\n";
            } else if (Instruction* I = SVFUtil::dyn_cast<Instruction>(ptrValue)) {
                llvm::errs() << "$$ REP: " << newRepId << " Stack object: " << *I << " : " << idx << " : " << I->getFunction()->getName() << " : " << SVFUtil::getSourceLoc(I) << "\n";
            } else if (GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(ptrValue)) {
                llvm::errs() << "$$ REP: " << newRepId << " Global variable: " << *v << " : " << idx << " : " << *v << " : " << SVFUtil::getSourceLoc(v) << "\n";
            }
        } else {
            llvm::errs() << "$$ REP: " << newRepId << "REP Dummy node: " << idx << "\n";
        }
        llvm::errs() << "$$ --------\n";
    }

    /// move the edges from node to rep, and remove the node
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = consCG->moveEdgesToRepNode(node, consCG->getConstraintNode(newRepId), criticalGepEdges);

    /// set rep and sub relations
    updateNodeRepAndSubs(node->getId(),newRepId);

    consCG->removeConstraintNode(node);

    return gepInsideScc;
}


/*
 * Merge a node to its rep node based on SCC detection
 */
void Andersen::mergeNodeToRep(NodeID nodeId,NodeID newRepId, std::vector<ConstraintEdge*>& criticalGepEdges)
{
    //llvm::errs() << "Merging node: " << nodeId << " to newRepId " << newRepId << "\n";
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = mergeSrcToTgt(nodeId,newRepId, criticalGepEdges);
    //llvm::errs() << "Found critical Gep inside SCC\n";
    /// We do this in mergeSccNodes
    /// 1. if find gep edges inside SCC cycle, the rep node will become a PWC node and
    /// its pts should be collapsed later.
    /// 2. if the node to be merged is already a PWC node, the rep node will also become
    /// a PWC node as it will have a self-cycle gep edge.
    if (!Options::InvariantPWC) {
        if (gepInsideScc || node->isPWCNode())
            consCG->setPWCNode(newRepId);
    }
		
		// Record that these points-to objects are somewhat sketchy
		pag->getImpactedByCollapseSet() |= getPts(nodeId);
}

/*
 * Updates subnodes of its rep, and rep node of its subs
 */
void Andersen::updateNodeRepAndSubs(NodeID nodeId, NodeID newRepId)
{
    consCG->setRep(nodeId,newRepId);
    NodeBS repSubs;
    repSubs.set(nodeId);
    /// update nodeToRepMap, for each subs of current node updates its rep to newRepId
    //  update nodeToSubsMap, union its subs with its rep Subs
    NodeBS& nodeSubs = consCG->sccSubNodes(nodeId);
    for(NodeBS::iterator sit = nodeSubs.begin(), esit = nodeSubs.end(); sit!=esit; ++sit)
    {
        NodeID subId = *sit;
        consCG->setRep(subId,newRepId);
    }
    repSubs |= nodeSubs;
    consCG->setSubs(newRepId,repSubs);
    consCG->resetSubs(nodeId);
}

/*!
 * Print pag nodes' pts by an ascending order
 */
void Andersen::dumpTopLevelPtsTo()
{
    for (OrderedNodeSet::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter)
    {
        const PAGNode* node = getPAG()->getPAGNode(*nIter);
        if (getPAG()->isValidTopLevelPtr(node))
        {
            const PointsTo& pts = this->getPts(node->getId());
            outs() << "\nNodeID " << node->getId() << " ";

            if (pts.empty())
            {
                outs() << "\t\tPointsTo: {empty}\n\n";
            }
            else
            {
                outs() << "\t\tPointsTo: { ";

                multiset<Size_t> line;
                for (PointsTo::iterator it = pts.begin(), eit = pts.end();
                        it != eit; ++it)
                {
                    line.insert(*it);
                }
                for (multiset<Size_t>::const_iterator it = line.begin(); it != line.end(); ++it)
                    outs() << *it << " ";
                outs() << "}\n\n";
            }
        }
    }

    outs().flush();
}
