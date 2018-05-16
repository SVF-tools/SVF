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

#include "MemoryModel/PAG.h"
#include "Util/AnalysisUtil.h"
#include "Util/GraphUtil.h"

#include <llvm/Support/DOTGraphTraits.h>	// for dot graph traits

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> HANDBLACKHOLE("blk", cl::init(false),
                                   cl::desc("Hanle blackhole edge"));


u64_t PAGEdge::callEdgeLabelCounter = 0;
PAGEdge::Inst2LabelMap PAGEdge::inst2LabelMap;

PAG* PAG::pag = NULL;


/*!
 * Add Address edge
 */
bool PAG::addAddrEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Addr))
        return false;
    else
        return addEdge(srcNode,dstNode, new AddrPE(srcNode, dstNode));
}

/*!
 * Add Copy edge
 */
bool PAG::addCopyEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Copy))
        return false;
    else
        return addEdge(srcNode,dstNode, new CopyPE(srcNode, dstNode));
}

/*!
 * Add Load edge
 */
bool PAG::addLoadEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Load))
        return false;
    else
        return addEdge(srcNode,dstNode, new LoadPE(srcNode, dstNode));
}

/*!
 * Add Store edge
 */
bool PAG::addStoreEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Store))
        return false;
    else
        return addEdge(srcNode,dstNode, new StorePE(srcNode, dstNode));
}

/*!
 * Add Call edge
 */
bool PAG::addCallEdge(NodeID src, NodeID dst, const llvm::Instruction* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasInterEdge(srcNode,dstNode, PAGEdge::Call, cs))
        return false;
    else
        return addEdge(srcNode,dstNode, new CallPE(srcNode, dstNode, cs));
}

/*!
 * Add Return edge
 */
bool PAG::addRetEdge(NodeID src, NodeID dst, const llvm::Instruction* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasInterEdge(srcNode,dstNode, PAGEdge::Ret, cs))
        return false;
    else
        return addEdge(srcNode,dstNode, new RetPE(srcNode, dstNode, cs));
}

/*!
 * Add blackhole/constant edge
 */
bool PAG::addBlackHoleAddrEdge(NodeID node) {
    if(HANDBLACKHOLE)
        return pag->addAddrEdge(pag->getBlackHoleNode(), node);
    else
        return pag->addCopyEdge(pag->getNullPtr(), node);
}

/*!
 * Add Thread fork edge for parameter passing from a spawner to its spawnees
 */
bool PAG::addThreadForkEdge(NodeID src, NodeID dst, const llvm::Instruction* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasInterEdge(srcNode,dstNode, PAGEdge::ThreadFork, cs))
        return false;
    else
        return addEdge(srcNode,dstNode, new TDForkPE(srcNode, dstNode, cs));
}

/*!
 * Add Thread fork edge for parameter passing from a spawnee back to its spawners
 */
bool PAG::addThreadJoinEdge(NodeID src, NodeID dst, const llvm::Instruction* cs) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasInterEdge(srcNode,dstNode, PAGEdge::ThreadJoin, cs))
        return false;
    else
        return addEdge(srcNode,dstNode, new TDJoinPE(srcNode, dstNode, cs));
}


/*!
 * Add Offset(Gep) edge
 * Find the base node id of src and connect base node to dst node
 * Create gep offset:  (offset + baseOff <nested struct gep size>)
 */
bool PAG::addGepEdge(NodeID src, NodeID dst, const LocationSet& ls, bool constGep) {

    PAGNode* node = getPAGNode(src);
    if (!constGep || node->hasIncomingVariantGepEdge()) {
        /// Since the offset from base to src is variant,
        /// the new gep edge being created is also a VariantGepPE edge.
        return addVariantGepEdge(src, dst);
    }
    else {
        return addNormalGepEdge(src, dst, ls);
    }
}

/*!
 * Add normal (Gep) edge
 */
bool PAG::addNormalGepEdge(NodeID src, NodeID dst, const LocationSet& ls) {
    const LocationSet& baseLS = getLocationSetFromBaseNode(src);
    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(baseNode, dstNode, PAGEdge::NormalGep))
        return false;
    else
        return addEdge(baseNode, dstNode, new NormalGepPE(baseNode, dstNode, ls+baseLS));
}

/*!
 * Add variant(Gep) edge
 * Find the base node id of src and connect base node to dst node
 */
bool PAG::addVariantGepEdge(NodeID src, NodeID dst) {

    PAGNode* baseNode = getPAGNode(getBaseValNode(src));
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(baseNode, dstNode, PAGEdge::VariantGep))
        return false;
    else
        return addEdge(baseNode, dstNode, new VariantGepPE(baseNode, dstNode));
}

/*!
 * Add global black hole address edge
 */
bool PAG::addGlobalBlackHoleAddrEdge(NodeID node, const ConstantExpr *int2Ptrce) {
    const llvm::Value* cval = curVal;
    const llvm::BasicBlock* cbb = curBB;
    setCurrentLocation(int2Ptrce,NULL);
    bool added = addBlackHoleAddrEdge(node);
    setCurrentLocation(cval,cbb);
    return added;
}

/*!
 * Add black hole Address edge for formal params
 */
bool PAG::addFormalParamBlackHoleAddrEdge(NodeID node, const llvm::Argument *arg)
{
    const llvm::Value* cval = curVal;
    const llvm::BasicBlock* cbb = curBB;
    setCurrentLocation(arg,&(arg->getParent()->getEntryBlock()));
    bool added = addBlackHoleAddrEdge(node);
    setCurrentLocation(cval,cbb);
    return added;
}


/*!
 * Add a temp field value node according to base value and offset
 * this node is after the initial node method, it is out of scope of symInfo table
 */
NodeID PAG::getGepValNode(const llvm::Value* val, const LocationSet& ls, const Type *baseType, u32_t fieldidx) {
    NodeID base = getBaseValNode(getValueNode(val));
    NodeLocationSetMap::iterator iter = GepValNodeMap.find(std::make_pair(base, ls));
    if (iter == GepValNodeMap.end()) {
        /*
         * getGepValNode can only be called from two places:
         * 1. PAGBuilder::addComplexConsForExt to handle external calls
         * 2. PAGBuilder::getGlobalVarField to initialize global variable
         * so curVal can only be
         * 1. Instruction
         * 2. GlobalVariable
         */
        assert((isa<Instruction>(curVal) || isa<GlobalVariable>(curVal)) && "curVal not an instruction or a globalvariable?");
        const std::vector<FieldInfo> &fieldinfo = symInfo->getFlattenFieldInfoVec(baseType);
        const Type *type = fieldinfo[fieldidx].getFlattenElemTy();
        NodeID gepNode= addGepValNode(val,ls,nodeNum,type,fieldidx);
        addGepEdge(base, gepNode, ls, true);
        return gepNode;
    } else
        return iter->second;
}

/*!
 * Add a temp field value node, this method can only invoked by getGepValNode
 */
NodeID PAG::addGepValNode(const llvm::Value* val, const LocationSet& ls, NodeID i, const llvm::Type *type, u32_t fieldidx) {
    NodeID base = getBaseValNode(getValueNode(val));
    //assert(findPAGNode(i) == false && "this node should not be created before");
    assert(0==GepValNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");
    GepValNodeMap[std::make_pair(base, ls)] = i;
    GepValPN *node = new GepValPN(val, i, ls, type, fieldidx);
    return addValNode(val, node, i);
}

/*!
 * Given an object node, find its field object node
 */
NodeID PAG::getGepObjNode(NodeID id, const LocationSet& ls) {
    PAGNode* node = pag->getPAGNode(id);
    if (GepObjPN* gepNode = dyn_cast<GepObjPN>(node))
        return getGepObjNode(gepNode->getMemObj(), gepNode->getLocationSet() + ls);
    else if (FIObjPN* baseNode = dyn_cast<FIObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else {
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

    LocationSet newLS = SymbolTableInfo::Symbolnfo()->getModulusOffset(obj->getTypeInfo(),ls);

    NodeLocationSetMap::iterator iter = GepObjNodeMap.find(std::make_pair(base, newLS));
    if (iter == GepObjNodeMap.end()) {
        NodeID gepNode= addGepObjNode(obj,newLS,nodeNum);
        return gepNode;
    } else
        return iter->second;

}

/*!
 * Add a field obj node, this method can only invoked by getGepObjNode
 */
NodeID PAG::addGepObjNode(const MemObj* obj, const LocationSet& ls, NodeID i) {
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    assert(0==GepObjNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");
    GepObjNodeMap[std::make_pair(base, ls)] = i;
    GepObjPN *node = new GepObjPN(obj->getRefVal(), i, obj, ls);
    memToFieldsMap[base].set(i);
    return addObjNode(obj->getRefVal(), node, i);
}

/*!
 * Add a field-insensitive node, this method can only invoked by getFIGepObjNode
 */
NodeID PAG::addFIObjNode(const MemObj* obj, NodeID i)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = getObjectNode(obj);
    memToFieldsMap[base].set(i);
    FIObjPN *node = new FIObjPN(obj->getRefVal(), i, obj);
    return addObjNode(obj->getRefVal(), node, i);
}


/*!
 * Return true if it is an intra-procedural edge
 */
bool PAG::hasIntraEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind) {
    PAGEdge edge(src,dst,kind);
    return PAGEdgeKindToSetMap[kind].find(&edge) != PAGEdgeKindToSetMap[kind].end();
}

/*!
 * Return true if it is an inter-procedural edge
 */
bool PAG::hasInterEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind, const llvm::Instruction* callInst) {
    PAGEdge edge(src,dst,PAGEdge::makeEdgeFlagWithCallInst(kind,callInst));
    return PAGEdgeKindToSetMap[kind].find(&edge) != PAGEdgeKindToSetMap[kind].end();
}

/*
 * curVal   <-------->  PAGEdge
 * Instruction          Any Edge
 * Argument             CopyEdge  (PAG::addFormalParamBlackHoleAddrEdge)
 * ConstantExpr         CopyEdge  (Int2PtrConstantExpr   CastConstantExpr  PAGBuilder::processCE)
 *                      GepEdge   (GepConstantExpr   PAGBuilder::processCE)
 * ConstantPointerNull  CopyEdge  (3-->2 NullPtr-->BlkPtr PAG::addNullPtrNode)
 *  				    AddrEdge  (0-->2 BlkObj-->BlkPtr PAG::addNullPtrNode)
 * GlobalVariable       AddrEdge  (PAGBuilder::visitGlobal)
 *                      GepEdge   (PAGBuilder::getGlobalVarField)
 * Function             AddrEdge  (PAGBuilder::visitGlobal)
 * Constant             StoreEdge (PAGBuilder::InitialGlobal)
 */
void PAG::setCurrentBBAndValueForPAGEdge(PAGEdge* edge) {
    assert(curVal && "current Val is NULL?");
    edge->setBB(curBB);
    edge->setValue(curVal);
    if (const Instruction *curInst = dyn_cast<Instruction>(curVal)) {
        assert(curBB && "instruction does not have a basic block??");
        inst2PAGEdgesMap[curInst].push_back(edge);
    } else if (isa<Argument>(curVal)) {
        assert(curBB && (&curBB->getParent()->getEntryBlock() == curBB));
        funToEntryPAGEdges[curBB->getParent()].insert(edge);
    } else if (isa<ConstantExpr>(curVal)) {
        if (!curBB)
            globPAGEdgesSet.insert(edge);
    } else if (isa<ConstantPointerNull>(curVal)) {
        assert((edge->getSrcID() == NullPtr && edge->getDstID() == BlkPtr) ||
               (edge->getSrcID() == BlackHole && edge->getDstID() == BlkPtr));
        globPAGEdgesSet.insert(edge);
    } else if (isa<GlobalVariable>(curVal) ||
               isa<Function>(curVal) ||
               isa<Constant>(curVal)) {
        globPAGEdgesSet.insert(edge);
    } else {
        assert(false && "what else value can we have?");
    }
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
    setCurrentBBAndValueForPAGEdge(edge);
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
    assert(llvm::isa<ObjPN>(node) && "need an object node");
    const ObjPN* obj = llvm::cast<ObjPN>(node);
    return getAllFieldsObjNode(obj->getMemObj());
}

/*!
 * Get all fields object nodes of an object
 * If this object is collapsed into one field insensitive object
 * Then only return this field insensitive object
 */
NodeBS PAG::getFieldsAfterCollapse(NodeID id) {
    const PAGNode* node = pag->getPAGNode(id);
    assert(llvm::isa<ObjPN>(node) && "need an object node");
    const MemObj* mem = llvm::cast<ObjPN>(node)->getMemObj();
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

        assert(isa<GepPE>(*it) && "not a gep edge??");
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
    assert(isa<NormalGepPE>(edge) && "not a get edge??");
    const NormalGepPE* gepEdge = cast<NormalGepPE>(edge);
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
    for (iterator I = begin(), E = end(); I != E; ++I) {
        PAGNode* node = I->second;
        if (!isa<DummyValPN>(node) && !isa<DummyObjPN>(node)) {
            outs() << "node " << node->getId() << " " << *(node->getValue())
                   << "\n";
            outs() << "\t InEdge: { ";
            for (PAGNode::iterator iter = node->getInEdges().begin();
                    iter != node->getInEdges().end(); ++iter) {
                outs() << (*iter)->getSrcID() << " ";
                if (NormalGepPE* edge = dyn_cast<NormalGepPE>(*iter))
                    outs() << " offset=" << edge->getOffset() << " ";
                else if (isa<VariantGepPE>(*iter))
                    outs() << " offset=variant";
            }
            outs() << "}\t";
            outs() << "\t OutEdge: { ";
            for (PAGNode::iterator iter = node->getOutEdges().begin();
                    iter != node->getOutEdges().end(); ++iter) {
                outs() << (*iter)->getDstID() << " ";
                if (NormalGepPE* edge = dyn_cast<NormalGepPE>(*iter))
                    outs() << " offset=" << edge->getOffset() << " ";
                else if (isa<VariantGepPE>(*iter))
                    outs() << " offset=variant";
            }
            outs() << "}\n";
        }
        outs() << "\n";
    }

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

/*!
 * PAGEdge constructor
 */
PAGEdge::PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k) :
    GenericPAGEdgeTy(s,d,k),value(NULL),basicBlock(NULL) {
    edgeId = PAG::getPAG()->getTotalEdgeNum();
    PAG::getPAG()->incEdgeNum();
}

/*!
 * PAGNode constructor
 */
PAGNode::PAGNode(const llvm::Value* val, NodeID i, PNODEK k) :
    GenericPAGNodeTy(i,k), value(val) {

    assert( ValNode <= k && k<= DummyObjNode && "new PAG node kind?");

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
        isTLPointer = llvm::cast<llvm::Function>(val)->getReturnType()->isPointerTy();
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
    case DummyObjNode: {
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
    GraphPrinter::WriteGraphToFile(llvm::outs(), name, this);
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

        if (briefDisplay) {
            if (isa<ValPN>(node)) {
                if (nameDisplay)
                    rawstr << node->getId() << ":" << node->getValueName();
                else
                    rawstr << node->getId();
            } else
                rawstr << node->getId();
        } else {
            // print the whole value
            if (!isa<DummyValPN>(node) && !isa<DummyObjPN>(node))
                rawstr << *node->getValue();
            else
                rawstr << "";

        }

        return rawstr.str();

    }

    static std::string getNodeAttributes(PAGNode *node, PAG *pag) {
        if (isa<ValPN>(node)) {
            if(isa<GepValPN>(node))
                return "shape=hexagon";
            else if (isa<DummyValPN>(node))
                return "shape=diamond";
            else
                return "shape=circle";
        } else if (isa<ObjPN>(node)) {
            if(isa<GepObjPN>(node))
                return "shape=doubleoctagon";
            else if(isa<FIObjPN>(node))
                return "shape=septagon";
            else if (isa<DummyObjPN>(node))
                return "shape=Mcircle";
            else
                return "shape=doublecircle";
        } else if (isa<RetPN>(node)) {
            return "shape=Mrecord";
        } else if (isa<VarArgPN>(node)) {
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
        if (isa<AddrPE>(edge)) {
            return "color=green";
        } else if (isa<CopyPE>(edge)) {
            return "color=black";
        } else if (isa<GepPE>(edge)) {
            return "color=purple";
        } else if (isa<StorePE>(edge)) {
            return "color=blue";
        } else if (isa<LoadPE>(edge)) {
            return "color=red";
        } else if (isa<TDForkPE>(edge)) {
            return "color=Turquoise";
        } else if (isa<TDJoinPE>(edge)) {
            return "color=Turquoise";
        } else if (isa<CallPE>(edge)) {
            return "color=black,style=dashed";
        } else if (isa<RetPE>(edge)) {
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
        if(const CallPE* calledge = dyn_cast<CallPE>(edge)) {
            const llvm::Instruction* callInst= calledge->getCallInst();
            return analysisUtil::getSourceLoc(callInst);
        }
        else if(const RetPE* retedge = dyn_cast<RetPE>(edge)) {
            const llvm::Instruction* callInst= retedge->getCallInst();
            return analysisUtil::getSourceLoc(callInst);
        }
        return "";
    }
};
}
