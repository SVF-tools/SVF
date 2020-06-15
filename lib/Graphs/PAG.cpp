//===- PAG.cpp -- Program assignment graph------------------------------------//
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
 * PAG.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/PAG.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/ICFGBuilder.h"

using namespace SVFUtil;

static llvm::cl::opt<bool> HANDBLACKHOLE("blk", llvm::cl::init(false),
                                   llvm::cl::desc("Hanle blackhole edge"));


u64_t PAGEdge::callEdgeLabelCounter = 0;
u64_t PAGEdge::storeEdgeLabelCounter = 0;
PAGEdge::Inst2LabelMap PAGEdge::inst2LabelMap;

PAG* PAG::pag = NULL;


PAG::PAG(bool buildFromFile) : fromFile(buildFromFile), totalPTAPAGEdge(0),nodeNumAfterPAGBuild(0) {
    symInfo = SymbolTableInfo::Symbolnfo();
    icfg = new ICFG();
	ICFGBuilder builder(icfg);
	builder.build(getModule());
}

/*!
 * Add Address edge
 */
AddrPE* PAG::addAddrPE(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::Addr))
        return SVFUtil::cast<AddrPE>(edge);
    else {
    	AddrPE* addrPE = new AddrPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, addrPE);
        return addrPE;
    }
}

/*!
 * Add Copy edge
 */
CopyPE* PAG::addCopyPE(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::Copy))
        return SVFUtil::cast<CopyPE>(edge);
    else {
    	CopyPE* copyPE = new CopyPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, copyPE);
        return copyPE;
    }
}

/*!
 * Add Compare edge
 */
CmpPE* PAG::addCmpPE(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::Cmp))
        return SVFUtil::cast<CmpPE>(edge);
    else {
    	CmpPE* cmp = new CmpPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, cmp);
        return cmp;
    }
}


/*!
 * Add Compare edge
 */
BinaryOPPE* PAG::addBinaryOPPE(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::BinaryOp))
        return SVFUtil::cast<BinaryOPPE>(edge);
    else {
    	BinaryOPPE* binaryOP = new BinaryOPPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, binaryOP);
        return binaryOP;
    }
}

/*!
 * Add Load edge
 */
LoadPE* PAG::addLoadPE(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::Load))
        return SVFUtil::cast<LoadPE>(edge);
    else {
    	LoadPE* loadPE = new LoadPE(srcNode, dstNode);
        addEdge(srcNode,dstNode, loadPE);
        return loadPE;
    }
}

/*!
 * Add Store edge
 * Note that two store instructions may share the same Store PAGEdge
 */
StorePE* PAG::addStorePE(NodeID src, NodeID dst, const IntraBlockNode* curVal) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(srcNode,dstNode, PAGEdge::Store))
        return SVFUtil::cast<StorePE>(edge);
    else {
    	StorePE* storePE = new StorePE(srcNode, dstNode, curVal);
        addEdge(srcNode,dstNode, storePE);
        return storePE;
    }
}

/*!
 * Add Call edge
 */
CallPE* PAG::addCallPE(NodeID src, NodeID dst, const CallBlockNode* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasInterEdge(srcNode,dstNode, PAGEdge::Call, cs))
        return SVFUtil::cast<CallPE>(edge);
    else {
    	CallPE* callPE = new CallPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, callPE);
        return callPE;
    }
}

/*!
 * Add Return edge
 */
RetPE* PAG::addRetPE(NodeID src, NodeID dst, const CallBlockNode* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasInterEdge(srcNode,dstNode, PAGEdge::Ret, cs))
        return SVFUtil::cast<RetPE>(edge);
    else {
    	RetPE* retPE = new RetPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, retPE);
        return retPE;
    }
}

/*!
 * Add blackhole/constant edge
 */
PAGEdge* PAG::addBlackHoleAddrPE(NodeID node) {
    if(HANDBLACKHOLE)
        return pag->addAddrPE(pag->getBlackHoleNode(), node);
    else
        return pag->addCopyPE(pag->getNullPtr(), node);
}

/*!
 * Add Thread fork edge for parameter passing from a spawner to its spawnees
 */
TDForkPE* PAG::addThreadForkPE(NodeID src, NodeID dst, const CallBlockNode* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasInterEdge(srcNode,dstNode, PAGEdge::ThreadFork, cs))
        return SVFUtil::cast<TDForkPE>(edge);
    else {
    	TDForkPE* forkPE = new TDForkPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, forkPE);
        return forkPE;
    }
}

/*!
 * Add Thread fork edge for parameter passing from a spawnee back to its spawners
 */
TDJoinPE* PAG::addThreadJoinPE(NodeID src, NodeID dst, const CallBlockNode* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasInterEdge(srcNode,dstNode, PAGEdge::ThreadJoin, cs))
        return SVFUtil::cast<TDJoinPE>(edge);
    else{
    	TDJoinPE* joinPE = new TDJoinPE(srcNode, dstNode, cs);
        addEdge(srcNode,dstNode, joinPE);
        return joinPE;
    }
}


/*!
 * Add Offset(Gep) edge
 * Find the base node id of src and connect base node to dst node
 * Create gep offset:  (offset + baseOff <nested struct gep size>)
 */
GepPE* PAG::addGepPE(NodeID src, NodeID dst, const LocationSet& ls, bool constGep) {

    PAGNode* node = getPAGNode(src);
    if (!constGep || node->hasIncomingVariantGepEdge()) {
        /// Since the offset from base to src is variant,
        /// the new gep edge being created is also a VariantGepPE edge.
        return addVariantGepPE(src, dst);
    }
    else {
        return addNormalGepPE(src, dst, ls);
    }
}

/*!
 * Add normal (Gep) edge
 */
NormalGepPE* PAG::addNormalGepPE(NodeID src, NodeID dst, const LocationSet& ls) {
    const LocationSet& baseLS = getLocationSetFromBaseNode(src);
    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(baseNode, dstNode, PAGEdge::NormalGep))
        return SVFUtil::cast<NormalGepPE>(edge);
    else {
    	NormalGepPE* gepPE = new NormalGepPE(baseNode, dstNode, ls+baseLS);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}

/*!
 * Add variant(Gep) edge
 * Find the base node id of src and connect base node to dst node
 */
VariantGepPE* PAG::addVariantGepPE(NodeID src, NodeID dst) {

    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(PAGEdge* edge = hasIntraEdge(baseNode, dstNode, PAGEdge::VariantGep))
        return SVFUtil::cast<VariantGepPE>(edge);
    else {
    	VariantGepPE* gepPE = new VariantGepPE(baseNode, dstNode);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}



/*!
 * Add a temp field value node, this method can only invoked by getGepValNode
 */
NodeID PAG::addGepValNode(const Value* gepVal, const LocationSet& ls, NodeID i, const Type *type, u32_t fieldidx) {
	NodeID base = getBaseValNode(getValueNode(gepVal));
    //assert(findPAGNode(i) == false && "this node should not be created before");
	assert(0==GepValNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");
	GepValNodeMap[std::make_pair(base, ls)] = i;
    GepValPN *node = new GepValPN(gepVal, i, ls, type, fieldidx);
    return addValNode(gepVal, node, i);
}

/*!
 * Given an object node, find its field object node
 */
NodeID PAG::getGepObjNode(NodeID id, const LocationSet& ls) {
    PAGNode* node = pag->getPAGNode(id);
    if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(node))
        return getGepObjNode(gepNode->getMemObj(), gepNode->getLocationSet() + ls);
    else if (FIObjPN* baseNode = SVFUtil::dyn_cast<FIObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else if (DummyObjPN* baseNode = SVFUtil::dyn_cast<DummyObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else{
        assert(false && "new gep obj node kind?");
        return id;
    }
}

/*!
 * Get a field obj PAG node according to base mem obj and offset
 * To support flexible field sensitive analysis with regard to MaxFieldOffset
 * offset = offset % obj->getMaxFieldOffsetLimit() to create limited number of mem objects
 * maximum number of field object creation is obj->getMaxFieldOffsetLimit()
 */
NodeID PAG::getGepObjNode(const MemObj* obj, const LocationSet& ls) {
    NodeID base = getObjectNode(obj);

    /// if this obj is field-insensitive, just return the field-insensitive node.
    if (obj->isFieldInsensitive())
        return getFIObjNode(obj);

    LocationSet newLS = SymbolTableInfo::Symbolnfo()->getModulusOffset(obj,ls);

    NodeLocationSetMap::iterator iter = GepObjNodeMap.find(std::make_pair(base, newLS));
	if (iter == GepObjNodeMap.end())
		return addGepObjNode(obj, newLS);
	else
		return iter->second;

}

/*!
 * Add a field obj node, this method can only invoked by getGepObjNode
 */
NodeID PAG::addGepObjNode(const MemObj* obj, const LocationSet& ls) {
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    assert(0==GepObjNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");

    //for a gep id, base id is set at lower bits, and offset is set at higher bits
    //e.g. 1100050 denotes base=50 and offset=10
    // The offset is 10, not 11, because we add 1 to the offset to ensure that the
    // high bits are never 0. For example, we do not want the gep id to be 50 when 
    // the base is 50 and the offset is 0.
    NodeID gepMultiplier = pow(10, ceil(log10(
            getNodeNumAfterPAGBuild() > StInfo::getMaxFieldLimit() ?
            getNodeNumAfterPAGBuild() : StInfo::getMaxFieldLimit()
    )));
    NodeID gepId = (ls.getOffset() + 1) * gepMultiplier + base;
    GepObjNodeMap[std::make_pair(base, ls)] = gepId;
	GepObjPN *node = new GepObjPN(obj, gepId, ls);
    memToFieldsMap[base].set(gepId);
    return addObjNode(obj->getRefVal(), node, gepId);
}

/*!
 * Add a field-insensitive node, this method can only invoked by getFIGepObjNode
 */
NodeID PAG::addFIObjNode(const MemObj* obj)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    memToFieldsMap[base].set(obj->getSymId());
    FIObjPN *node = new FIObjPN(obj->getRefVal(), obj->getSymId(), obj);
    return addObjNode(obj->getRefVal(), node, obj->getSymId());
}


/*!
 * Return true if it is an intra-procedural edge
 */
PAGEdge* PAG::hasIntraEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind) {
    PAGEdge edge(src,dst,kind);
    PAGEdge::PAGEdgeSetTy::iterator it = PAGEdgeKindToSetMap[kind].find(&edge);
	if (it != PAGEdgeKindToSetMap[kind].end()) {
		return *it;
	}
	return NULL;
}

/*!
 * Return true if it is an inter-procedural edge
 */
PAGEdge* PAG::hasInterEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind, const ICFGNode* callInst) {
    PAGEdge edge(src,dst,PAGEdge::makeEdgeFlagWithCallInst(kind,callInst));
    PAGEdge::PAGEdgeSetTy::iterator it = PAGEdgeKindToSetMap[kind].find(&edge);
	if (it != PAGEdgeKindToSetMap[kind].end()) {
		return *it;
	}
	return NULL;
}


/*!
 * Add a PAG edge into edge map
 */
bool PAG::addEdge(PAGNode* src, PAGNode* dst, PAGEdge* edge) {

    DBOUT(DPAGBuild,
          outs() << "add edge from " << src->getId() << " kind :"
          << src->getNodeKind() << " to " << dst->getId()
          << " kind :" << dst->getNodeKind() << "\n");
    src->addOutEdge(edge);
    dst->addInEdge(edge);
    bool added = PAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge).second;
    assert(added && "duplicated edge, not added!!!");
	if (edge->isPTAEdge()){
	    totalPTAPAGEdge++;
		PTAPAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge);
	}
    return true;
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& PAG::getAllFieldsObjNode(const MemObj* obj) {
    NodeID base = getObjectNode(obj);
    return memToFieldsMap[base];
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& PAG::getAllFieldsObjNode(NodeID id) {
    const PAGNode* node = pag->getPAGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const ObjPN* obj = SVFUtil::cast<ObjPN>(node);
    return getAllFieldsObjNode(obj->getMemObj());
}

/*!
 * Get all fields object nodes of an object
 * If this object is collapsed into one field insensitive object
 * Then only return this field insensitive object
 */
NodeBS PAG::getFieldsAfterCollapse(NodeID id) {
    const PAGNode* node = pag->getPAGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const MemObj* mem = SVFUtil::cast<ObjPN>(node)->getMemObj();
    if(mem->isFieldInsensitive()) {
        NodeBS bs;
        bs.set(getFIObjNode(mem));
        return bs;
    }
    else
        return getAllFieldsObjNode(mem);
}

/*!
 * Get a base pointer given a pointer
 * Return the source node of its connected gep edge if this pointer has
 * Otherwise return the node id itself
 */
NodeID PAG::getBaseValNode(NodeID nodeId) {
    PAGNode* node  = getPAGNode(nodeId);
    if (node->hasIncomingEdges(PAGEdge::NormalGep) ||  node->hasIncomingEdges(PAGEdge::VariantGep)) {
        PAGEdge::PAGEdgeSetTy& ngeps = node->getIncomingEdges(PAGEdge::NormalGep);
        PAGEdge::PAGEdgeSetTy& vgeps = node->getIncomingEdges(PAGEdge::VariantGep);

        assert(((ngeps.size()+vgeps.size())==1) && "one node can only be connected by at most one gep edge!");

        PAGNode::iterator it;
        if(!ngeps.empty())
            it = ngeps.begin();
        else
            it = vgeps.begin();

        assert(SVFUtil::isa<GepPE>(*it) && "not a gep edge??");
        return (*it)->getSrcID();
    }
    else
        return nodeId;
}

/*!
 * Get a base PAGNode given a pointer
 * Return the source node of its connected normal gep edge
 * Otherwise return the node id itself
 * Size_t offset : gep offset
 */
LocationSet PAG::getLocationSetFromBaseNode(NodeID nodeId) {
    PAGNode* node  = getPAGNode(nodeId);
    PAGEdge::PAGEdgeSetTy& geps = node->getIncomingEdges(PAGEdge::NormalGep);
    /// if this node is already a base node
    if(geps.empty())
        return LocationSet(0);

    assert(geps.size()==1 && "one node can only be connected by at most one gep edge!");
    PAGNode::iterator it = geps.begin();
    const PAGEdge* edge = *it;
    assert(SVFUtil::isa<NormalGepPE>(edge) && "not a get edge??");
    const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
    return gepEdge->getLocationSet();
}

/*!
 * Clean up memory
 */
void PAG::destroy() {
    for (PAGEdge::PAGKindToEdgeSetMapTy::iterator I =
                PAGEdgeKindToSetMap.begin(), E = PAGEdgeKindToSetMap.end(); I != E;
            ++I) {
        for (PAGEdge::PAGEdgeSetTy::iterator edgeIt = I->second.begin(),
                endEdgeIt = I->second.end(); edgeIt != endEdgeIt; ++edgeIt) {
            delete *edgeIt;
        }
    }
    delete symInfo;
    symInfo = NULL;
}

/*!
 * Print this PAG graph including its nodes and edges
 */
void PAG::print() {

	outs() << "-------------------PAG------------------------------------\n";
	PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
			addrs.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
				<< "\n";
	}

	PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
			copys.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Copy --> " << (*iter)->getDstID()
				<< "\n";
	}

	PAGEdge::PAGEdgeSetTy& calls = pag->getEdgeSet(PAGEdge::Call);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = calls.begin(), eiter =
			calls.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Call --> " << (*iter)->getDstID()
				<< "\n";
	}

	PAGEdge::PAGEdgeSetTy& rets = pag->getEdgeSet(PAGEdge::Ret);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = rets.begin(), eiter =
			rets.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Ret --> " << (*iter)->getDstID()
				<< "\n";
	}

	PAGEdge::PAGEdgeSetTy& tdfks = pag->getEdgeSet(PAGEdge::ThreadFork);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = tdfks.begin(), eiter =
			tdfks.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- ThreadFork --> "
				<< (*iter)->getDstID() << "\n";
	}

	PAGEdge::PAGEdgeSetTy& tdjns = pag->getEdgeSet(PAGEdge::ThreadJoin);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = tdjns.begin(), eiter =
			tdjns.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- ThreadJoin --> "
				<< (*iter)->getDstID() << "\n";
	}

	PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
			ngeps.end(); iter != eiter; ++iter) {
		NormalGepPE* gep = SVFUtil::cast<NormalGepPE>(*iter);
		outs() << gep->getSrcID() << " -- NormalGep (" << gep->getOffset()
				<< ") --> " << gep->getDstID() << "\n";
	}

	PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
			vgeps.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- VariantGep --> "
				<< (*iter)->getDstID() << "\n";
	}

	PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Load);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
			loads.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
				<< "\n";
	}

	PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Store);
	for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
			stores.end(); iter != eiter; ++iter) {
		outs() << (*iter)->getSrcID() << " -- Store --> " << (*iter)->getDstID()
				<< "\n";
	}
	outs() << "----------------------------------------------------------\n";

}

/*
 * If this is a dummy node or node does not have incoming edges we assume it is not a pointer here
 */
bool PAG::isValidPointer(NodeID nodeId) const {
    PAGNode* node = pag->getPAGNode(nodeId);
    if ((node->getInEdges().empty() && node->getOutEdges().empty()))
        return false;
    return node->isPointer();
}

bool PAG::isValidTopLevelPtr(const PAGNode* node) {
    if (node->isTopLevelPtr()) {
        if (isValidPointer(node->getId()) && node->hasValue()) {
            if (SVFUtil::ArgInNoCallerFunction(node->getValue()))
                return false;
            return true;
        }
    }
    return false;
}
/*!
 * PAGEdge constructor
 */
PAGEdge::PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k) :
    GenericPAGEdgeTy(s,d,k),value(NULL),basicBlock(NULL),icfgNode(NULL) {
    edgeId = PAG::getPAG()->getTotalEdgeNum();
    PAG::getPAG()->incEdgeNum();
}

/*!
 * Whether src and dst nodes are both pointer type
 */
bool PAGEdge::isPTAEdge() const {
	return getSrcNode()->isPointer() && getDstNode()->isPointer();
}

/*!
 * PAGNode constructor
 */
PAGNode::PAGNode(const Value* val, NodeID i, PNODEK k) :
    GenericPAGNodeTy(i,k), value(val) {

    assert( ValNode <= k && k <= CloneDummyObjNode && "new PAG node kind?");

    switch (k) {
    case ValNode:
    case GepValNode: {
        assert(val != NULL && "value is NULL for ValPN or GepValNode");
        isTLPointer = val->getType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case RetNode: {
        assert(val != NULL && "value is NULL for RetNode");
        isTLPointer = SVFUtil::cast<Function>(val)->getReturnType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case VarargNode:
    case DummyValNode: {
        isTLPointer = true;
        isATPointer = false;
        break;
    }

    case ObjNode:
    case GepObjNode:
    case FIObjNode:
    case DummyObjNode:
    case CloneGepObjNode:
    case CloneFIObjNode:
    case CloneDummyObjNode: {
        isTLPointer = false;
        isATPointer = true;
        break;
    }
    }
}

/*!
 * Dump this PAG
 */
void PAG::dump(std::string name) {
    GraphPrinter::WriteGraphToFile(outs(), name, this);
}


/*!
 * Whether to handle blackhole edge
 */
void PAG::handleBlackHole(bool b) {
    HANDBLACKHOLE = b;
}

namespace llvm {
/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<PAG*> : public DefaultDOTGraphTraits {

    typedef PAGNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(PAG *graph) {
        return graph->getGraphName();
    }

    /// Return label of a VFG node with two display mode
    /// Either you can choose to display the name of the value or the whole instruction
    static std::string getNodeLabel(PAGNode *node, PAG *graph) {
        bool briefDisplay = true;
        bool nameDisplay = true;
        std::string str;
        raw_string_ostream rawstr(str);
        // print function info
        if (node->getFunction())
            rawstr << "[" << node->getFunction()->getName() << "] ";

        if (briefDisplay) {
            if (SVFUtil::isa<ValPN>(node)) {
                if (nameDisplay)
                    rawstr << node->getId() << ":" << node->getValueName();
                else
                    rawstr << node->getId();
            } else
                rawstr << node->getId();
        } else {
            // print the whole value
            if (!SVFUtil::isa<DummyValPN>(node) && !SVFUtil::isa<DummyObjPN>(node))
                rawstr << *node->getValue();
            else
                rawstr << "";

        }

        return rawstr.str();

    }

    static std::string getNodeAttributes(PAGNode *node, PAG *pag) {
        if (SVFUtil::isa<ValPN>(node)) {
            if(SVFUtil::isa<GepValPN>(node))
                return "shape=hexagon";
            else if (SVFUtil::isa<DummyValPN>(node))
                return "shape=diamond";
            else
                return "shape=circle";
        } else if (SVFUtil::isa<ObjPN>(node)) {
            if(SVFUtil::isa<GepObjPN>(node))
                return "shape=doubleoctagon";
            else if(SVFUtil::isa<FIObjPN>(node))
                return "shape=septagon";
            else if (SVFUtil::isa<DummyObjPN>(node))
                return "shape=Mcircle";
            else
                return "shape=doublecircle";
        } else if (SVFUtil::isa<RetPN>(node)) {
            return "shape=Mrecord";
        } else if (SVFUtil::isa<VarArgPN>(node)) {
            return "shape=octagon";
        } else {
            assert(0 && "no such kind node!!");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(PAGNode *node, EdgeIter EI, PAG *pag) {
        const PAGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<AddrPE>(edge)) {
            return "color=green";
        } else if (SVFUtil::isa<CopyPE>(edge)) {
            return "color=black";
        } else if (SVFUtil::isa<GepPE>(edge)) {
            return "color=purple";
        } else if (SVFUtil::isa<StorePE>(edge)) {
            return "color=blue";
        } else if (SVFUtil::isa<LoadPE>(edge)) {
            return "color=red";
        } else if (SVFUtil::isa<CmpPE>(edge)) {
            return "color=grey";
        } else if (SVFUtil::isa<BinaryOPPE>(edge)) {
            return "color=grey";
        } else if (SVFUtil::isa<TDForkPE>(edge)) {
            return "color=Turquoise";
        } else if (SVFUtil::isa<TDJoinPE>(edge)) {
            return "color=Turquoise";
        } else if (SVFUtil::isa<CallPE>(edge)) {
            return "color=black,style=dashed";
        } else if (SVFUtil::isa<RetPE>(edge)) {
            return "color=black,style=dotted";
        }
        else {
            assert(0 && "No such kind edge!!");
        }
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(PAGNode *node, EdgeIter EI) {
        const PAGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if(const CallPE* calledge = SVFUtil::dyn_cast<CallPE>(edge)) {
            const Instruction* callInst= calledge->getCallSite()->getCallSite().getInstruction();
            return SVFUtil::getSourceLoc(callInst);
        }
        else if(const RetPE* retedge = SVFUtil::dyn_cast<RetPE>(edge)) {
            const Instruction* callInst= retedge->getCallSite()->getCallSite().getInstruction();
            return SVFUtil::getSourceLoc(callInst);
        }
        return "";
    }
};
}
