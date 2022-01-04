//===- SVFIR.cpp -- IR of SVF ---------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SVFIR.cpp
 *
 *  Created on: 31, 12, 2021
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "MemoryModel/SVFIR.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/ICFGBuilder.h"

using namespace SVF;
using namespace SVFUtil;


SVFIR* SVFIR::pag = nullptr;

SVFIR::SVFIR(bool buildFromFile) : IRGraph(buildFromFile)
{
    icfg = new ICFG();
    ICFGBuilder builder(icfg);
    builder.build(getModule());
}

/*!
 * Add Address edge
 */
AddrPE* SVFIR::addAddrPE(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Addr))
        return SVFUtil::cast<AddrPE>(edge);
    else
    {
        AddrPE* addrPE = new AddrPE(srcNode, dstNode);
        addToStmt2TypeMap(addrPE);
        addEdge(srcNode,dstNode, addrPE);
        return addrPE;
    }
}

/*!
 * Add Copy edge
 */
CopyPE* SVFIR::addCopyPE(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Copy))
        return SVFUtil::cast<CopyPE>(edge);
    else
    {
        CopyPE* copyPE = new CopyPE(srcNode, dstNode);
        addToStmt2TypeMap(copyPE);
        addEdge(srcNode,dstNode, copyPE);
        return copyPE;
    }
}

/*!
 * Add Phi statement 
 */
PhiPE* SVFIR::addPhiNode(NodeID res, NodeID opnd)
{
    SVFVar* opNode = getGNode(opnd);
    SVFVar* resNode = getGNode(res);
    PHINodeMap::iterator it = phiNodeMap.find(resNode);
    if(it == phiNodeMap.end()){
        PhiPE* phi = new PhiPE(resNode, {opNode});
        addToStmt2TypeMap(phi);
        phiNodeMap[resNode] = phi;
        return phi;
    }
    else{
        it->second->addOpVar(opNode);
        return it->second;
    }
}

/*!
 * Add Compare edge
 */
CmpPE* SVFIR::addCmpPE(NodeID op1, NodeID op2, NodeID dst)
{
    SVFVar* op1Node = getGNode(op1);
    SVFVar* op2Node = getGNode(op2);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(op1Node, dstNode, SVFStmt::Cmp, op2Node))
        return SVFUtil::cast<CmpPE>(edge);
    else
    {
        std::vector<SVFVar*> opnds = {op1Node, op2Node};
        CmpPE* cmp = new CmpPE(dstNode, opnds);
        addToStmt2TypeMap(cmp);
        addEdge(op1Node, dstNode, cmp);
        return cmp;
    }
}


/*!
 * Add Compare edge
 */
BinaryOPPE* SVFIR::addBinaryOPPE(NodeID op1, NodeID op2, NodeID dst)
{
    SVFVar* op1Node = getGNode(op1);
    SVFVar* op2Node = getGNode(op2);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(op1Node, dstNode, SVFStmt::BinaryOp, op2Node))
        return SVFUtil::cast<BinaryOPPE>(edge);
    else
    {
        std::vector<SVFVar*> opnds = {op1Node, op2Node};
        BinaryOPPE* binaryOP = new BinaryOPPE(dstNode, opnds);
        addToStmt2TypeMap(binaryOP);
        addEdge(op1Node,dstNode, binaryOP);
        return binaryOP;
    }
}

/*!
 * Add Unary edge
 */
UnaryOPPE* SVFIR::addUnaryOPPE(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(srcNode,dstNode, SVFStmt::UnaryOp))
        return SVFUtil::cast<UnaryOPPE>(edge);
    else
    {
        UnaryOPPE* unaryOP = new UnaryOPPE(srcNode, dstNode);
        addToStmt2TypeMap(unaryOP);
        addEdge(srcNode,dstNode, unaryOP);
        return unaryOP;
    }
}

/*!
 * Add Load edge
 */
LoadPE* SVFIR::addLoadPE(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Load))
        return SVFUtil::cast<LoadPE>(edge);
    else
    {
        LoadPE* loadPE = new LoadPE(srcNode, dstNode);
        addToStmt2TypeMap(loadPE);
        addEdge(srcNode,dstNode, loadPE);
        return loadPE;
    }
}

/*!
 * Add Store edge
 * Note that two store instructions may share the same Store SVFStmt
 */
StorePE* SVFIR::addStorePE(NodeID src, NodeID dst, const IntraBlockNode* curVal)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(srcNode,dstNode, SVFStmt::Store, curVal))
        return SVFUtil::cast<StorePE>(edge);
    else
    {
        StorePE* storePE = new StorePE(srcNode, dstNode, curVal);
        addToStmt2TypeMap(storePE);
        addEdge(srcNode,dstNode, storePE);
        return storePE;
    }
}

/*!
 * Add Call edge
 */
CallPE* SVFIR::addCallPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(srcNode,dstNode, SVFStmt::Call, cs))
        return SVFUtil::cast<CallPE>(edge);
    else
    {
        CallPE* callPE = new CallPE(srcNode, dstNode, cs);
        addToStmt2TypeMap(callPE);
        addEdge(srcNode,dstNode, callPE);
        return callPE;
    }
}

/*!
 * Add Return edge
 */
RetPE* SVFIR::addRetPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(srcNode,dstNode, SVFStmt::Ret, cs))
        return SVFUtil::cast<RetPE>(edge);
    else
    {
        RetPE* retPE = new RetPE(srcNode, dstNode, cs);
        addToStmt2TypeMap(retPE);
        addEdge(srcNode,dstNode, retPE);
        return retPE;
    }
}

/*!
 * Add blackhole/constant edge
 */
SVFStmt* SVFIR::addBlackHoleAddrPE(NodeID node)
{
    if(Options::HandBlackHole)
        return pag->addAddrPE(pag->getBlackHoleNode(), node);
    else
        return pag->addCopyPE(pag->getNullPtr(), node);
}

/*!
 * Add Thread fork edge for parameter passing from a spawner to its spawnees
 */
TDForkPE* SVFIR::addThreadForkPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(srcNode,dstNode, SVFStmt::ThreadFork, cs))
        return SVFUtil::cast<TDForkPE>(edge);
    else
    {
        TDForkPE* forkPE = new TDForkPE(srcNode, dstNode, cs);
        addToStmt2TypeMap(forkPE);
        addEdge(srcNode,dstNode, forkPE);
        return forkPE;
    }
}

/*!
 * Add Thread fork edge for parameter passing from a spawnee back to its spawners
 */
TDJoinPE* SVFIR::addThreadJoinPE(NodeID src, NodeID dst, const CallBlockNode* cs)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasLabeledEdge(srcNode,dstNode, SVFStmt::ThreadJoin, cs))
        return SVFUtil::cast<TDJoinPE>(edge);
    else
    {
        TDJoinPE* joinPE = new TDJoinPE(srcNode, dstNode, cs);
        addToStmt2TypeMap(joinPE);
        addEdge(srcNode,dstNode, joinPE);
        return joinPE;
    }
}


/*!
 * Add Offset(Gep) edge
 * Find the base node id of src and connect base node to dst node
 * Create gep offset:  (offset + baseOff <nested struct gep size>)
 */
GepPE* SVFIR::addGepPE(NodeID src, NodeID dst, const LocationSet& ls, bool constGep)
{

    SVFVar* node = getGNode(src);
    if (!constGep || node->hasIncomingVariantGepEdge())
    {
        /// Since the offset from base to src is variant,
        /// the new gep edge being created is also a VariantGepPE edge.
        return addVariantGepPE(src, dst);
    }
    else
    {
        return addNormalGepPE(src, dst, ls);
    }
}

/*!
 * Add normal (Gep) edge
 */
NormalGepPE* SVFIR::addNormalGepPE(NodeID src, NodeID dst, const LocationSet& ls)
{
    const LocationSet& baseLS = getLocationSetFromBaseNode(src);
    SVFVar* baseNode = getGNode(getBaseValNode(src));
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(baseNode, dstNode, SVFStmt::NormalGep))
        return SVFUtil::cast<NormalGepPE>(edge);
    else
    {
        NormalGepPE* gepPE = new NormalGepPE(baseNode, dstNode, ls+baseLS);
        addToStmt2TypeMap(gepPE);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}

/*!
 * Add variant(Gep) edge
 * Find the base node id of src and connect base node to dst node
 */
VariantGepPE* SVFIR::addVariantGepPE(NodeID src, NodeID dst)
{

    SVFVar* baseNode = getGNode(getBaseValNode(src));
    SVFVar* dstNode = getGNode(dst);
    if(SVFStmt* edge = hasNonlabeledEdge(baseNode, dstNode, SVFStmt::VariantGep))
        return SVFUtil::cast<VariantGepPE>(edge);
    else
    {
        VariantGepPE* gepPE = new VariantGepPE(baseNode, dstNode);
        addToStmt2TypeMap(gepPE);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}



/*!
 * Add a temp field value node, this method can only invoked by getGepValNode
 * due to constaint expression, curInst is used to distinguish different instructions (e.g., memorycpy) when creating GepValPN.
 */
NodeID SVFIR::addGepValNode(const Value* curInst,const Value* gepVal, const LocationSet& ls, NodeID i, const Type *type, u32_t fieldidx)
{
    NodeID base = getBaseValNode(getValueNode(gepVal));
    //assert(findPAGNode(i) == false && "this node should not be created before");
    assert(0==GepValNodeMap[curInst].count(std::make_pair(base, ls))
           && "this node should not be created before");
    GepValNodeMap[curInst][std::make_pair(base, ls)] = i;
    GepValPN *node = new GepValPN(gepVal, i, ls, type, fieldidx);
    return addValNode(gepVal, node, i);
}

/*!
 * Given an object node, find its field object node
 */
NodeID SVFIR::getGepObjNode(NodeID id, const LocationSet& ls)
{
    SVFVar* node = pag->getGNode(id);
    if (GepObjPN* gepNode = SVFUtil::dyn_cast<GepObjPN>(node))
        return getGepObjNode(gepNode->getMemObj(), gepNode->getLocationSet() + ls);
    else if (FIObjPN* baseNode = SVFUtil::dyn_cast<FIObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else if (DummyObjPN* baseNode = SVFUtil::dyn_cast<DummyObjPN>(node))
        return getGepObjNode(baseNode->getMemObj(), ls);
    else
    {
        assert(false && "new gep obj node kind?");
        return id;
    }
}

/*!
 * Get a field obj SVFIR node according to base mem obj and offset
 * To support flexible field sensitive analysis with regard to MaxFieldOffset
 * offset = offset % obj->getMaxFieldOffsetLimit() to create limited number of mem objects
 * maximum number of field object creation is obj->getMaxFieldOffsetLimit()
 */
NodeID SVFIR::getGepObjNode(const MemObj* obj, const LocationSet& ls)
{
    NodeID base = obj->getId();

    /// if this obj is field-insensitive, just return the field-insensitive node.
    if (obj->isFieldInsensitive())
        return getFIObjNode(obj);

    LocationSet newLS = SymbolTableInfo::SymbolInfo()->getModulusOffset(obj,ls);

    // Base and first field are the same memory location.
    if (Options::FirstFieldEqBase && newLS.getOffset() == 0) return base;

    NodeLocationSetMap::iterator iter = GepObjNodeMap.find(std::make_pair(base, newLS));
    if (iter == GepObjNodeMap.end())
        return addGepObjNode(obj, newLS);
    else
        return iter->second;

}

/*!
 * Add a field obj node, this method can only invoked by getGepObjNode
 */
NodeID SVFIR::addGepObjNode(const MemObj* obj, const LocationSet& ls)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = obj->getId();
    assert(0==GepObjNodeMap.count(std::make_pair(base, ls))
           && "this node should not be created before");

    NodeID gepId = NodeIDAllocator::get()->allocateGepObjectId(base, ls.getOffset(), StInfo::getMaxFieldLimit());
    GepObjNodeMap[std::make_pair(base, ls)] = gepId;
    GepObjPN *node = new GepObjPN(obj, gepId, ls);
    memToFieldsMap[base].set(gepId);
    return addObjNode(obj->getValue(), node, gepId);
}

/*!
 * Add a field-insensitive node, this method can only invoked by getFIGepObjNode
 */
NodeID SVFIR::addFIObjNode(const MemObj* obj)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = obj->getId();
    memToFieldsMap[base].set(obj->getId());
    FIObjPN *node = new FIObjPN(obj->getValue(), obj->getId(), obj);
    return addObjNode(obj->getValue(), node, obj->getId());
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& SVFIR::getAllFieldsObjNode(const MemObj* obj)
{
    NodeID base = obj->getId();
    return memToFieldsMap[base];
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& SVFIR::getAllFieldsObjNode(NodeID id)
{
    const SVFVar* node = pag->getGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const ObjPN* obj = SVFUtil::cast<ObjPN>(node);
    return getAllFieldsObjNode(obj->getMemObj());
}

/*!
 * Get all fields object nodes of an object
 * If this object is collapsed into one field insensitive object
 * Then only return this field insensitive object
 */
NodeBS SVFIR::getFieldsAfterCollapse(NodeID id)
{
    const SVFVar* node = pag->getGNode(id);
    assert(SVFUtil::isa<ObjPN>(node) && "need an object node");
    const MemObj* mem = SVFUtil::cast<ObjPN>(node)->getMemObj();
    if(mem->isFieldInsensitive())
    {
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
NodeID SVFIR::getBaseValNode(NodeID nodeId)
{
    SVFVar* node  = getGNode(nodeId);
    if (node->hasIncomingEdges(SVFStmt::NormalGep) ||  node->hasIncomingEdges(SVFStmt::VariantGep))
    {
        SVFStmt::PAGEdgeSetTy& ngeps = node->getIncomingEdges(SVFStmt::NormalGep);
        SVFStmt::PAGEdgeSetTy& vgeps = node->getIncomingEdges(SVFStmt::VariantGep);

        assert(((ngeps.size()+vgeps.size())==1) && "one node can only be connected by at most one gep edge!");

        SVFVar::iterator it;
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
 * Get a base SVFVar given a pointer
 * Return the source node of its connected normal gep edge
 * Otherwise return the node id itself
 * Size_t offset : gep offset
 */
LocationSet SVFIR::getLocationSetFromBaseNode(NodeID nodeId)
{
    SVFVar* node  = getGNode(nodeId);
    SVFStmt::PAGEdgeSetTy& geps = node->getIncomingEdges(SVFStmt::NormalGep);
    /// if this node is already a base node
    if(geps.empty())
        return LocationSet(0);

    assert(geps.size()==1 && "one node can only be connected by at most one gep edge!");
    SVFVar::iterator it = geps.begin();
    const SVFStmt* edge = *it;
    assert(SVFUtil::isa<NormalGepPE>(edge) && "not a get edge??");
    const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
    return gepEdge->getLocationSet();
}

/*!
 * Clean up memory
 */
void SVFIR::destroy()
{
    delete icfg;
    icfg = nullptr;
}

/*!
 * Print this SVFIR graph including its nodes and edges
 */
void SVFIR::print()
{

    outs() << "-------------------SVFIR------------------------------------\n";
    SVFStmt::PAGEdgeSetTy& addrs = pag->getEdgeSet(SVFStmt::Addr);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::PAGEdgeSetTy& copys = pag->getEdgeSet(SVFStmt::Copy);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Copy --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::PAGEdgeSetTy& calls = pag->getEdgeSet(SVFStmt::Call);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = calls.begin(), eiter =
                calls.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Call --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::PAGEdgeSetTy& rets = pag->getEdgeSet(SVFStmt::Ret);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = rets.begin(), eiter =
                rets.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Ret --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::PAGEdgeSetTy& tdfks = pag->getEdgeSet(SVFStmt::ThreadFork);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = tdfks.begin(), eiter =
                tdfks.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadFork --> "
               << (*iter)->getDstID() << "\n";
    }

    SVFStmt::PAGEdgeSetTy& tdjns = pag->getEdgeSet(SVFStmt::ThreadJoin);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = tdjns.begin(), eiter =
                tdjns.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadJoin --> "
               << (*iter)->getDstID() << "\n";
    }

    SVFStmt::PAGEdgeSetTy& ngeps = pag->getEdgeSet(SVFStmt::NormalGep);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter)
    {
        NormalGepPE* gep = SVFUtil::cast<NormalGepPE>(*iter);
        outs() << gep->getSrcID() << " -- NormalGep (" << gep->getOffset()
               << ") --> " << gep->getDstID() << "\n";
    }

    SVFStmt::PAGEdgeSetTy& vgeps = pag->getEdgeSet(SVFStmt::VariantGep);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
                vgeps.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- VariantGep --> "
               << (*iter)->getDstID() << "\n";
    }

    SVFStmt::PAGEdgeSetTy& loads = pag->getEdgeSet(SVFStmt::Load);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::PAGEdgeSetTy& stores = pag->getEdgeSet(SVFStmt::Store);
    for (SVFStmt::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Store --> " << (*iter)->getDstID()
               << "\n";
    }
    outs() << "----------------------------------------------------------\n";

}

    /// Initialize candidate pointers
void SVFIR::initialiseCandidatePointers()
{
    // collect candidate pointers for demand-driven analysis
    for (iterator nIter = begin(); nIter != end(); ++nIter)
    {
        NodeID nodeId = nIter->first;
        // do not compute points-to for isolated node
        if (isValidPointer(nodeId) == false)
            continue;
        candidatePointers.insert(nodeId);
    }
}

/*
 * If this is a dummy node or node does not have incoming edges we assume it is not a pointer here
 */
bool SVFIR::isValidPointer(NodeID nodeId) const
{
    SVFVar* node = pag->getGNode(nodeId);
    if ((node->getInEdges().empty() && node->getOutEdges().empty()))
        return false;
    return node->isPointer();
}

bool SVFIR::isValidTopLevelPtr(const SVFVar* node)
{
    if (node->isTopLevelPtr())
    {
        if (isValidPointer(node->getId()) && node->hasValue())
        {
            if (SVFUtil::ArgInNoCallerFunction(node->getValue()))
                return false;
            return true;
        }
    }
    return false;
}

/*!
 * Whether to handle blackhole edge
 */
void SVFIR::handleBlackHole(bool b)
{
    Options::HandBlackHole = b;
}



