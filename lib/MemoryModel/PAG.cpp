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
#include "Util/SVFUtil.h"
#include "llvm/Support/JSON.h"

using namespace SVFUtil;

static llvm::cl::opt<bool> HANDBLACKHOLE("blk", llvm::cl::init(false),
                                   llvm::cl::desc("Hanle blackhole edge"));


u64_t PAGEdge::callEdgeLabelCounter = 0;
u64_t PAGEdge::storeEdgeLabelCounter = 0;
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
 * Add Compare edge
 */
bool PAG::addCmpEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Cmp))
        return false;
    else
        return addEdge(srcNode,dstNode, new CmpPE(srcNode, dstNode));
}


/*!
 * Add Compare edge
 */
bool PAG::addBinaryOPEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::BinaryOp))
        return false;
    else
        return addEdge(srcNode,dstNode, new BinaryOPPE(srcNode, dstNode));
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
 * Note that two store instructions may share the same Store PAGEdge
 */
bool PAG::addStoreEdge(NodeID src, NodeID dst) {
    PAGNode* srcNode = getPAGNode(src);
    PAGNode* dstNode = getPAGNode(dst);
    if(hasIntraEdge(srcNode,dstNode, PAGEdge::Store))
        return false;
    else
        return addEdge(srcNode,dstNode, new StorePE(srcNode, dstNode, curVal));
}

/*!
 * Add Call edge
 */
bool PAG::addCallEdge(NodeID src, NodeID dst, const Instruction* cs) {
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
bool PAG::addRetEdge(NodeID src, NodeID dst, const Instruction* cs) {
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
bool PAG::addThreadForkEdge(NodeID src, NodeID dst, const Instruction* cs) {
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
bool PAG::addThreadJoinEdge(NodeID src, NodeID dst, const Instruction* cs) {
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
    const Value* cval = curVal;
    const BasicBlock* cbb = curBB;
    setCurrentLocation(int2Ptrce,NULL);
    bool added = addBlackHoleAddrEdge(node);
    setCurrentLocation(cval,cbb);
    return added;
}

/*!
 * Add black hole Address edge for formal params
 */
bool PAG::addFormalParamBlackHoleAddrEdge(NodeID node, const Argument *arg)
{
    const Value* cval = curVal;
    const BasicBlock* cbb = curBB;
    setCurrentLocation(arg,&(arg->getParent()->getEntryBlock()));
    bool added = addBlackHoleAddrEdge(node);
    setCurrentLocation(cval,cbb);
    return added;
}

void PAG::printInst2JsonFile(const std::string& filename){
    outs() << "write symbols to '" << filename << "'...";

    std::error_code err;
    ToolOutputFile F(filename.c_str(), err, llvm::sys::fs::F_None);
    if (err) {
        outs() << "  error opening file for writing!\n";
        F.os().clear_error();
        return;
    }
    
    llvm::json::Array root_array;
    for(Inst2PAGEdgesMap::const_iterator it = inst2PAGEdgesMap.begin();
     it!=inst2PAGEdgesMap.end(); it++){
        llvm::json::Object currentInst;
        char hex_char[30];
        const Instruction* inst = it->first;
		const PAGEdgeList& pagEdgeList = it->second;
        std::sprintf(hex_char,"%p",inst);
        currentInst["current Inst"] =std::string(hex_char);

        llvm::json::Array next_inst_array;
        std::vector<const Instruction*> instList;
		getNextInsts(inst,instList);
        for (std::vector<const Instruction*>::const_iterator nit = instList.begin();
        nit!=instList.end(); ++nit){
            std::sprintf(hex_char,"%p",*nit);
            next_inst_array.push_back(std::string(hex_char));
		}
        llvm::json::Value next_inst_value = llvm::json::Array{next_inst_array};
        currentInst["next Inst"] = next_inst_value;

        llvm::json::Array edges_array;
                for (PAGEdgeList::const_iterator bit = pagEdgeList.begin(),
                ebit = pagEdgeList.end(); bit != ebit; ++bit) {
            const PAGEdge* edge = *bit;
            llvm::json::Object edge_obj;
            edge_obj["source node"] = edge->getSrcID();
            edge_obj["destination node"] = edge->getDstID();
            edge_obj["source node type"] = getNodeKindValue(edge->getSrcNode()->getNodeKind());
            edge_obj["destination node type"] = getNodeKindValue(edge->getDstNode()->getNodeKind());
            edge_obj["edge type"] = getEdgeKindValue(edge->getEdgeKind());
            if(edge->getEdgeKind()==PAGEdge::NormalGep){
                const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
                edge_obj["offset"] = gepEdge->getOffset();
            }
            llvm::json::Value edge_value = llvm::json::Object{edge_obj};
            edges_array.push_back(edge_value);
        }
        
        llvm::json::Value edges_array_value = llvm::json::Array{edges_array};
        currentInst["edges"] = edges_array_value;

        llvm::json::Value currentInst_value = llvm::json::Object{currentInst};
        root_array.push_back(currentInst_value);    
    }

    llvm::json::Value root_array_value = llvm::json::Array{root_array};
    llvm::json::Object root;
    root["Instructions"] = root_array_value;
    llvm::json::Array global_edge_array;
    for(PAGEdgeSet::const_iterator it = globPAGEdgesSet.begin(); it!=globPAGEdgesSet.end(); it++){
        const PAGEdge* edge = *it;
        llvm::json::Object global_edge_obj;
        global_edge_obj["source node"] = edge->getSrcID();
        global_edge_obj["destination node"]= edge->getDstID();
        global_edge_obj["source node type"] = getNodeKindValue(edge->getSrcNode()->getNodeKind());
        global_edge_obj["destination node type"] = getNodeKindValue(edge->getDstNode()->getNodeKind());
        global_edge_obj["edge type"] = getEdgeKindValue(edge->getEdgeKind());
        if(edge->getEdgeKind()==PAGEdge::NormalGep){
                const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
                global_edge_obj["offset"] = gepEdge->getOffset();
        }
        llvm::json::Value edge_value = llvm::json::Object{global_edge_obj};
        global_edge_array.push_back(edge_value);
	}
    llvm::json::Value global_edge_array_value = llvm::json::Array{global_edge_array};
    root["global edge"] = global_edge_array_value;
    llvm::json::Value root_value = llvm::json::Object{root};

    llvm::json::operator<<(F.os(),root_value);

    F.os().close();
    if (!F.os().has_error()) {
        outs() << "\n";
        F.keep();
        return;
    }

}

std::string PAG::getNodeKindValue(int kind){
    switch (kind){
        case (PAGNode::ValNode):
            return "ValNode";
            break;
        case PAGNode::ObjNode:
            return "ObjNode";
            break;
        case PAGNode::RetNode:
            return "RetNode";
            break;
        case PAGNode::VarargNode:
            return "VarargNode";
            break;
        case PAGNode::GepValNode:
            return "GepValNode";
            break;
        case PAGNode::GepObjNode:
            return "GepObjNode";
            break;
        case PAGNode::FIObjNode:
            return "FIObjNode";
            break;
        case PAGNode::DummyValNode:
            return "DummyValNode";
            break;
        case PAGNode::DummyObjNode:
            return "DummyObjNode";
            break;
    }
    return "";
}

std::string PAG::getEdgeKindValue(int kind){
    switch(kind){
        case (PAGEdge::Addr):
            return "Addr";
            break;
        case (PAGEdge::Copy):
            return "Copy";
            break;
        case (PAGEdge::Store):
            return "Store";
            break;
        case (PAGEdge::Load):
            return "Load";
            break;
        case (PAGEdge::Call):
            return "Call";
            break;
        case (PAGEdge::Ret):
            return "Ret";
            break;
        case (PAGEdge::NormalGep):
            return "NormalGep";
            break;
        case (PAGEdge::VariantGep):
            return "VariantGep";
            break;
        case (PAGEdge::ThreadFork):
            return "ThreadFork";
            break;
        case (PAGEdge::ThreadJoin):
            return "ThreadJoin";
            break;
        case (PAGEdge::Cmp):
            return "Cmp";
            break;
        case (PAGEdge::BinaryOp):
            return "BinaryOp";
            break;
    }
    return "";
}


/*!
 * Add a temp field value node according to base value and offset
 * this node is after the initial node method, it is out of scope of symInfo table
 */
NodeID PAG::getGepValNode(const Value* val, const LocationSet& ls, const Type *baseType, u32_t fieldidx) {
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
        assert((SVFUtil::isa<Instruction>(curVal) || SVFUtil::isa<GlobalVariable>(curVal)) && "curVal not an instruction or a globalvariable?");
        const std::vector<FieldInfo> &fieldinfo = symInfo->getFlattenFieldInfoVec(baseType);
        const Type *type = fieldinfo[fieldidx].getFlattenElemTy();

        // We assume every GepValNode and its GepEdge to the baseNode are unique across the whole program
        // We preserve the current BB information to restore it after creating the gepNode
        const Value* cval = pag->getCurrentValue();
        const BasicBlock* cbb = pag->getCurrentBB();
        pag->setCurrentLocation(curVal, NULL);
        NodeID gepNode= addGepValNode(val,ls,nodeNum,type,fieldidx);
        addGepEdge(base, gepNode, ls, true);
        pag->setCurrentLocation(cval, cbb);
        return gepNode;
    } else
        return iter->second;
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
    //e.g. 1100050 denotes base=50 and offset=11
    NodeID gepMultiplier = pow(10, ceil(log10(
            getNodeNumAfterPAGBuild() > SymbolTableInfo::getMaxFieldLimit() ?
            getNodeNumAfterPAGBuild() : SymbolTableInfo::getMaxFieldLimit()
    )));
    NodeID gepId = ls.getOffset() * gepMultiplier + base;
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
bool PAG::hasIntraEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind) {
    PAGEdge edge(src,dst,kind);
    return PAGEdgeKindToSetMap[kind].find(&edge) != PAGEdgeKindToSetMap[kind].end();
}

/*!
 * Return true if it is an inter-procedural edge
 */
bool PAG::hasInterEdge(PAGNode* src, PAGNode* dst, PAGEdge::PEDGEK kind, const Instruction* callInst) {
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
    if (const Instruction *curInst = SVFUtil::dyn_cast<Instruction>(curVal)) {
 	/// We assume every GepValPN and its GepPE are unique across whole program
	if(!(SVFUtil::isa<GepPE>(edge) && SVFUtil::isa<GepValPN>(edge->getDstNode())))
		assert(curBB && "instruction does not have a basic block??");
		addToInstPAGEdgeList(curInst,edge);
    } else if (SVFUtil::isa<Argument>(curVal)) {
        assert(curBB && (&curBB->getParent()->getEntryBlock() == curBB));
    } else if (SVFUtil::isa<ConstantExpr>(curVal)) {
        if (!curBB)
            globPAGEdgesSet.insert(edge);
    } else if (SVFUtil::isa<GlobalVariable>(curVal) ||
               SVFUtil::isa<Function>(curVal) ||
			   SVFUtil::isa<Constant>(curVal)) {
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
	if (edge->isPTAEdge()){
	    totalPTAPAGEdge++;
		PTAPAGEdgeKindToSetMap[edge->getEdgeKind()].insert(edge);
	}
	if (!SVFModule::pagReadFromTXT())
		// setCurrentBBAndValueForPAGEdge(edge);
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

/*!
 * PAGEdge constructor
 */
PAGEdge::PAGEdge(PAGNode* s, PAGNode* d, GEdgeFlag k) :
    GenericPAGEdgeTy(s,d,k),value(NULL),basicBlock(NULL) {
    edgeId = PAG::getPAG()->getTotalEdgeNum();
    PAG::getPAG()->incEdgeNum();
}

/*!
 * Whether src and dst nodes are both pointer type
 */
bool PAGEdge::isPTAEdge() const {
	return getSrcNode()->isPointer() && getDstNode()->isPointer();
}

PAGNode::PAGNode(NodeID i,PNODEK k,const char* str_val):
    GenericPAGNodeTy(i,k), str_value(str_val){
     assert(ValNode <= k && k<= DummyObjNode && "new PAG node kind?");
    switch (k) {
    case ValNode:
    case GepValNode: {
        assert(str_val != NULL && "value is NULL for ValPN or GepValNode");
        isATPointer = false;
        break;
    }

    case RetNode: {
        assert(str_val != NULL && "value is NULL for RetNode");
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
 * PAGNode constructor
 */
PAGNode::PAGNode(const Value* val, NodeID i, PNODEK k) :
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
            const Instruction* callInst= calledge->getCallInst();
            return SVFUtil::getSourceLoc(callInst);
        }
        else if(const RetPE* retedge = SVFUtil::dyn_cast<RetPE>(edge)) {
            const Instruction* callInst= retedge->getCallInst();
            return SVFUtil::getSourceLoc(callInst);
        }
        return "";
    }
};
}
