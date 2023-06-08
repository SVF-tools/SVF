//===- SVFIR.cpp -- IR of SVF ---------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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
#include "SVFIR/SVFIR.h"

using namespace SVF;
using namespace SVFUtil;


std::unique_ptr<SVFIR> SVFIR::pag;

SVFIR::SVFIR(bool buildFromFile) : IRGraph(buildFromFile), svfModule(nullptr), icfg(nullptr), chgraph(nullptr)
{
}

/*!
 * Add Address edge
 */
AddrStmt* SVFIR::addAddrStmt(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Addr))
        return nullptr;
    else
    {
        AddrStmt* addrPE = new AddrStmt(srcNode, dstNode);
        addToStmt2TypeMap(addrPE);
        addEdge(srcNode,dstNode, addrPE);
        return addrPE;
    }
}

/*!
 * Add Copy edge
 */
CopyStmt* SVFIR::addCopyStmt(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Copy))
        return nullptr;
    else
    {
        CopyStmt* copyPE = new CopyStmt(srcNode, dstNode);
        addToStmt2TypeMap(copyPE);
        addEdge(srcNode,dstNode, copyPE);
        return copyPE;
    }
}

/*!
 * Add Phi statement
 */
PhiStmt* SVFIR::addPhiStmt(NodeID res, NodeID opnd, const ICFGNode* pred)
{
    SVFVar* opNode = getGNode(opnd);
    SVFVar* resNode = getGNode(res);
    PHINodeMap::iterator it = phiNodeMap.find(resNode);
    if(it == phiNodeMap.end())
    {
        PhiStmt* phi = new PhiStmt(resNode, {opNode}, {pred});
        addToStmt2TypeMap(phi);
        addEdge(opNode, resNode, phi);
        phiNodeMap[resNode] = phi;
        return phi;
    }
    else
    {
        it->second->addOpVar(opNode,pred);
        /// return null if we already added this PhiStmt
        return nullptr;
    }
}

/*!
 * Add Phi statement
 */
SelectStmt* SVFIR::addSelectStmt(NodeID res, NodeID op1, NodeID op2, NodeID cond)
{
    SVFVar* op1Node = getGNode(op1);
    SVFVar* op2Node = getGNode(op2);
    SVFVar* dstNode = getGNode(res);
    SVFVar* condNode = getGNode(cond);
    if(hasLabeledEdge(op1Node, dstNode, SVFStmt::Select, op2Node))
        return nullptr;
    else
    {
        std::vector<SVFVar*> opnds = {op1Node, op2Node};
        SelectStmt* select = new SelectStmt(dstNode, opnds, condNode);
        addToStmt2TypeMap(select);
        addEdge(op1Node, dstNode, select);
        return select;
    }
}

/*!
 * Add Compare edge
 */
CmpStmt* SVFIR::addCmpStmt(NodeID op1, NodeID op2, NodeID dst, u32_t predicate)
{
    SVFVar* op1Node = getGNode(op1);
    SVFVar* op2Node = getGNode(op2);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(op1Node, dstNode, SVFStmt::Cmp, op2Node))
        return nullptr;
    else
    {
        std::vector<SVFVar*> opnds = {op1Node, op2Node};
        CmpStmt* cmp = new CmpStmt(dstNode, opnds, predicate);
        addToStmt2TypeMap(cmp);
        addEdge(op1Node, dstNode, cmp);
        return cmp;
    }
}


/*!
 * Add Compare edge
 */
BinaryOPStmt* SVFIR::addBinaryOPStmt(NodeID op1, NodeID op2, NodeID dst, u32_t opcode)
{
    SVFVar* op1Node = getGNode(op1);
    SVFVar* op2Node = getGNode(op2);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(op1Node, dstNode, SVFStmt::BinaryOp, op2Node))
        return nullptr;
    else
    {
        std::vector<SVFVar*> opnds = {op1Node, op2Node};
        BinaryOPStmt* binaryOP = new BinaryOPStmt(dstNode, opnds, opcode);
        addToStmt2TypeMap(binaryOP);
        addEdge(op1Node,dstNode, binaryOP);
        return binaryOP;
    }
}

/*!
 * Add Unary edge
 */
UnaryOPStmt* SVFIR::addUnaryOPStmt(NodeID src, NodeID dst, u32_t opcode)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(srcNode,dstNode, SVFStmt::UnaryOp))
        return nullptr;
    else
    {
        UnaryOPStmt* unaryOP = new UnaryOPStmt(srcNode, dstNode, opcode);
        addToStmt2TypeMap(unaryOP);
        addEdge(srcNode,dstNode, unaryOP);
        return unaryOP;
    }
}

/*
* Add BranchStmt
*/
BranchStmt* SVFIR::addBranchStmt(NodeID br, NodeID cond, const BranchStmt::SuccAndCondPairVec&  succs)
{
    SVFVar* brNode = getGNode(br);
    SVFVar* condNode = getGNode(cond);
    if(hasNonlabeledEdge(condNode,brNode, SVFStmt::Branch))
        return nullptr;
    else
    {
        BranchStmt* branch = new BranchStmt(brNode, condNode, succs);
        addToStmt2TypeMap(branch);
        addEdge(condNode,brNode, branch);
        return branch;
    }
}

/*!
 * Add Load edge
 */
LoadStmt* SVFIR::addLoadStmt(NodeID src, NodeID dst)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(srcNode,dstNode, SVFStmt::Load))
        return nullptr;
    else
    {
        LoadStmt* loadPE = new LoadStmt(srcNode, dstNode);
        addToStmt2TypeMap(loadPE);
        addEdge(srcNode,dstNode, loadPE);
        return loadPE;
    }
}

/*!
 * Add Store edge
 * Note that two store instructions may share the same Store SVFStmt
 */
StoreStmt* SVFIR::addStoreStmt(NodeID src, NodeID dst, const IntraICFGNode* curVal)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(srcNode,dstNode, SVFStmt::Store, curVal))
        return nullptr;
    else
    {
        StoreStmt* storePE = new StoreStmt(srcNode, dstNode, curVal);
        addToStmt2TypeMap(storePE);
        addEdge(srcNode,dstNode, storePE);
        return storePE;
    }
}

/*!
 * Add Call edge
 */
CallPE* SVFIR::addCallPE(NodeID src, NodeID dst, const CallICFGNode* cs, const FunEntryICFGNode* entry)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(srcNode,dstNode, SVFStmt::Call, cs))
        return nullptr;
    else
    {
        CallPE* callPE = new CallPE(srcNode, dstNode, cs,entry);
        addToStmt2TypeMap(callPE);
        addEdge(srcNode,dstNode, callPE);
        return callPE;
    }
}

/*!
 * Add Return edge
 */
RetPE* SVFIR::addRetPE(NodeID src, NodeID dst, const CallICFGNode* cs, const FunExitICFGNode* exit)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(srcNode,dstNode, SVFStmt::Ret, cs))
        return nullptr;
    else
    {
        RetPE* retPE = new RetPE(srcNode, dstNode, cs, exit);
        addToStmt2TypeMap(retPE);
        addEdge(srcNode,dstNode, retPE);
        return retPE;
    }
}

/*!
 * Add blackhole/constant edge
 */
SVFStmt* SVFIR::addBlackHoleAddrStmt(NodeID node)
{
    if(Options::HandBlackHole())
        return pag->addAddrStmt(pag->getBlackHoleNode(), node);
    else
        return pag->addCopyStmt(pag->getNullPtr(), node);
}

/*!
 * Add Thread fork edge for parameter passing from a spawner to its spawnees
 */
TDForkPE* SVFIR::addThreadForkPE(NodeID src, NodeID dst, const CallICFGNode* cs, const FunEntryICFGNode* entry)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(srcNode,dstNode, SVFStmt::ThreadFork, cs))
        return nullptr;
    else
    {
        TDForkPE* forkPE = new TDForkPE(srcNode, dstNode, cs, entry);
        addToStmt2TypeMap(forkPE);
        addEdge(srcNode,dstNode, forkPE);
        return forkPE;
    }
}

/*!
 * Add Thread fork edge for parameter passing from a spawnee back to its spawners
 */
TDJoinPE* SVFIR::addThreadJoinPE(NodeID src, NodeID dst, const CallICFGNode* cs, const FunExitICFGNode* exit)
{
    SVFVar* srcNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasLabeledEdge(srcNode,dstNode, SVFStmt::ThreadJoin, cs))
        return nullptr;
    else
    {
        TDJoinPE* joinPE = new TDJoinPE(srcNode, dstNode, cs, exit);
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
GepStmt* SVFIR::addGepStmt(NodeID src, NodeID dst, const AccessPath& ap, bool constGep)
{

    SVFVar* node = getGNode(src);
    if (!constGep || node->hasIncomingVariantGepEdge())
    {
        /// Since the offset from base to src is variant,
        /// the new gep edge being created is also a Variant GepStmt edge.
        return addVariantGepStmt(src, dst, ap);
    }
    else
    {
        return addNormalGepStmt(src, dst, ap);
    }
}

/*!
 * Add normal (Gep) edge
 */
GepStmt* SVFIR::addNormalGepStmt(NodeID src, NodeID dst, const AccessPath& ap)
{
    SVFVar* baseNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(baseNode, dstNode, SVFStmt::Gep))
        return nullptr;
    else
    {
        GepStmt* gepPE = new GepStmt(baseNode, dstNode, ap);
        addToStmt2TypeMap(gepPE);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}

/*!
 * Add variant(Gep) edge
 * Find the base node id of src and connect base node to dst node
 */
GepStmt* SVFIR::addVariantGepStmt(NodeID src, NodeID dst, const AccessPath& ap)
{
    SVFVar* baseNode = getGNode(src);
    SVFVar* dstNode = getGNode(dst);
    if(hasNonlabeledEdge(baseNode, dstNode, SVFStmt::Gep))
        return nullptr;
    else
    {
        GepStmt* gepPE = new GepStmt(baseNode, dstNode, ap, true);
        addToStmt2TypeMap(gepPE);
        addEdge(baseNode, dstNode, gepPE);
        return gepPE;
    }
}



/*!
 * Add a temp field value node, this method can only invoked by getGepValVar
 * due to constaint expression, curInst is used to distinguish different instructions (e.g., memorycpy) when creating GepValVar.
 */
NodeID SVFIR::addGepValNode(const SVFValue* curInst,const SVFValue* gepVal, const AccessPath& ap, NodeID i, const SVFType* type)
{
    NodeID base = getBaseValVar(getValueNode(gepVal));
    //assert(findPAGNode(i) == false && "this node should not be created before");
    assert(0==GepValObjMap[curInst].count(std::make_pair(base, ap))
           && "this node should not be created before");
    GepValObjMap[curInst][std::make_pair(base, ap)] = i;
    GepValVar *node = new GepValVar(gepVal, i, ap, type);
    return addValNode(gepVal, node, i);
}

/*!
 * Given an object node, find its field object node
 */
NodeID SVFIR::getGepObjVar(NodeID id, const APOffset& apOffset)
{
    SVFVar* node = pag->getGNode(id);
    if (GepObjVar* gepNode = SVFUtil::dyn_cast<GepObjVar>(node))
        return getGepObjVar(gepNode->getMemObj(), gepNode->getConstantFieldIdx() + apOffset);
    else if (FIObjVar* baseNode = SVFUtil::dyn_cast<FIObjVar>(node))
        return getGepObjVar(baseNode->getMemObj(), apOffset);
    else if (DummyObjVar* baseNode = SVFUtil::dyn_cast<DummyObjVar>(node))
        return getGepObjVar(baseNode->getMemObj(), apOffset);
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
NodeID SVFIR::getGepObjVar(const MemObj* obj, const APOffset& apOffset)
{
    NodeID base = obj->getId();

    /// if this obj is field-insensitive, just return the field-insensitive node.
    if (obj->isFieldInsensitive())
        return getFIObjVar(obj);

    APOffset newLS = pag->getSymbolInfo()->getModulusOffset(obj, apOffset);

    // Base and first field are the same memory location.
    if (Options::FirstFieldEqBase() && newLS == 0) return base;

    NodeOffsetMap::iterator iter = GepObjVarMap.find(std::make_pair(base, newLS));
    if (iter == GepObjVarMap.end())
        return addGepObjNode(obj, newLS);
    else
        return iter->second;

}

/*!
 * Add a field obj node, this method can only invoked by getGepObjVar
 */
NodeID SVFIR::addGepObjNode(const MemObj* obj, const APOffset& apOffset)
{
    //assert(findPAGNode(i) == false && "this node should not be created before");
    NodeID base = obj->getId();
    assert(0==GepObjVarMap.count(std::make_pair(base, apOffset))
           && "this node should not be created before");

    NodeID gepId = NodeIDAllocator::get()->allocateGepObjectId(base, apOffset, Options::MaxFieldLimit());
    GepObjVarMap[std::make_pair(base, apOffset)] = gepId;
    GepObjVar *node = new GepObjVar(obj, gepId, apOffset);
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
    FIObjVar *node = new FIObjVar(obj->getValue(), obj->getId(), obj);
    return addObjNode(obj->getValue(), node, obj->getId());
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& SVFIR::getAllFieldsObjVars(const MemObj* obj)
{
    NodeID base = obj->getId();
    return memToFieldsMap[base];
}

/*!
 * Get all fields object nodes of an object
 */
NodeBS& SVFIR::getAllFieldsObjVars(NodeID id)
{
    const SVFVar* node = pag->getGNode(id);
    assert(SVFUtil::isa<ObjVar>(node) && "need an object node");
    const ObjVar* obj = SVFUtil::cast<ObjVar>(node);
    return getAllFieldsObjVars(obj->getMemObj());
}

/*!
 * Get all fields object nodes of an object
 * If this object is collapsed into one field insensitive object
 * Then only return this field insensitive object
 */
NodeBS SVFIR::getFieldsAfterCollapse(NodeID id)
{
    const SVFVar* node = pag->getGNode(id);
    assert(SVFUtil::isa<ObjVar>(node) && "need an object node");
    const MemObj* mem = SVFUtil::cast<ObjVar>(node)->getMemObj();
    if(mem->isFieldInsensitive())
    {
        NodeBS bs;
        bs.set(getFIObjVar(mem));
        return bs;
    }
    else
        return getAllFieldsObjVars(mem);
}

/*!
 * Get a base pointer given a pointer
 * Return the source node of its connected gep edge if this pointer has
 * Otherwise return the node id itself
 */
NodeID SVFIR::getBaseValVar(NodeID nodeId)
{
    SVFVar* node  = getGNode(nodeId);
    if (node->hasIncomingEdges(SVFStmt::Gep))
    {
        SVFStmt::SVFStmtSetTy& geps = node->getIncomingEdges(SVFStmt::Gep);

        assert((geps.size()==1) && "one node can only be connected by at most one gep edge!");

        SVFVar::iterator it = geps.begin();

        assert(SVFUtil::isa<GepStmt>(*it) && "not a gep edge??");
        return (*it)->getSrcID();
    }
    else
        return nodeId;
}

/*!
 * It is used to create a dummy GepValVar during global initiailzation.
 */
NodeID SVFIR::getGepValVar(const SVFValue* curInst, NodeID base, const AccessPath& ap) const
{
    GepValueVarMap::const_iterator iter = GepValObjMap.find(curInst);
    if(iter==GepValObjMap.end())
    {
        return UINT_MAX;
    }
    else
    {
        NodeAccessPathMap::const_iterator lit =
            iter->second.find(std::make_pair(base, ap));
        if (lit == iter->second.end())
            return UINT_MAX;
        else
            return lit->second;
    }
}


/*!
 * Clean up memory
 */
void SVFIR::destroy()
{
    delete icfg;
    icfg = nullptr;
    delete chgraph;
    chgraph = nullptr;
    SVFModule::releaseSVFModule();
    svfModule = nullptr;
}

/*!
 * Print this SVFIR graph including its nodes and edges
 */
void SVFIR::print()
{

    outs() << "-------------------SVFIR------------------------------------\n";
    SVFStmt::SVFStmtSetTy& addrs = pag->getSVFStmtSet(SVFStmt::Addr);
    for (SVFStmt::SVFStmtSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Addr --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::SVFStmtSetTy& copys = pag->getSVFStmtSet(SVFStmt::Copy);
    for (SVFStmt::SVFStmtSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Copy --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::SVFStmtSetTy& calls = pag->getSVFStmtSet(SVFStmt::Call);
    for (SVFStmt::SVFStmtSetTy::iterator iter = calls.begin(), eiter =
                calls.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Call --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::SVFStmtSetTy& rets = pag->getSVFStmtSet(SVFStmt::Ret);
    for (SVFStmt::SVFStmtSetTy::iterator iter = rets.begin(), eiter =
                rets.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Ret --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::SVFStmtSetTy& tdfks = pag->getSVFStmtSet(SVFStmt::ThreadFork);
    for (SVFStmt::SVFStmtSetTy::iterator iter = tdfks.begin(), eiter =
                tdfks.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadFork --> "
               << (*iter)->getDstID() << "\n";
    }

    SVFStmt::SVFStmtSetTy& tdjns = pag->getSVFStmtSet(SVFStmt::ThreadJoin);
    for (SVFStmt::SVFStmtSetTy::iterator iter = tdjns.begin(), eiter =
                tdjns.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- ThreadJoin --> "
               << (*iter)->getDstID() << "\n";
    }

    SVFStmt::SVFStmtSetTy& ngeps = pag->getSVFStmtSet(SVFStmt::Gep);
    for (SVFStmt::SVFStmtSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter)
    {
        GepStmt* gep = SVFUtil::cast<GepStmt>(*iter);
        if(gep->isVariantFieldGep())
            outs() << (*iter)->getSrcID() << " -- VariantGep --> "
                   << (*iter)->getDstID() << "\n";
        else
            outs() << gep->getRHSVarID() << " -- Gep (" << gep->getConstantFieldIdx()
                   << ") --> " << gep->getLHSVarID() << "\n";
    }

    SVFStmt::SVFStmtSetTy& loads = pag->getSVFStmtSet(SVFStmt::Load);
    for (SVFStmt::SVFStmtSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        outs() << (*iter)->getSrcID() << " -- Load --> " << (*iter)->getDstID()
               << "\n";
    }

    SVFStmt::SVFStmtSetTy& stores = pag->getSVFStmtSet(SVFStmt::Store);
    for (SVFStmt::SVFStmtSetTy::iterator iter = stores.begin(), eiter =
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
/*!
 * Return true if FIObjVar can point to any object
 * Or a field GepObjVar can point to any object.
 */
bool SVFIR::isNonPointerObj(NodeID id) const
{
    SVFVar* node = getGNode(id);
    if (const FIObjVar* fiNode = SVFUtil::dyn_cast<FIObjVar>(node))
    {
        return (fiNode->getMemObj()->hasPtrObj()==false);
    }
    else if (const GepObjVar* gepNode = SVFUtil::dyn_cast<GepObjVar>(node))
    {
        return (gepNode->getMemObj()->isNonPtrFieldObj(gepNode->getConstantFieldIdx()));
    }
    else if (const DummyObjVar* dummyNode = SVFUtil::dyn_cast<DummyObjVar>(node))
    {
        return (dummyNode->getMemObj()->hasPtrObj()==false);
    }
    else
    {
        assert(false && "expecting a object node");
        abort();
    }
}
/*
 * If this is a dummy node or node does not have incoming edges and outcoming edges we assume it is not a pointer here.
 * However, if it is a pointer and it is an argument of a function definition, we assume it is a pointer here.
 */
bool SVFIR::isValidPointer(NodeID nodeId) const
{
    SVFVar* node = pag->getGNode(nodeId);

    if (node->hasValue() && node->isPointer())
    {
        if(const SVFArgument* arg = SVFUtil::dyn_cast<SVFArgument>(node->getValue()))
        {
            if (!(arg->getParent()->isDeclaration()))
                return true;
        }
    }

    if ((node->getInEdges().empty() && node->getOutEdges().empty()))
        return false;
    return node->isPointer();
}

bool SVFIR::isValidTopLevelPtr(const SVFVar* node)
{
    if (SVFUtil::isa<ValVar>(node))
    {
        if (isValidPointer(node->getId()) && node->hasValue())
        {
            return !SVFUtil::isArgOfUncalledFunction(node->getValue());
        }
    }
    return false;
}

/*!
 * Whether to handle blackhole edge
 */
void SVFIR::handleBlackHole(bool b)
{
    Options::HandBlackHole.setValue(b);
}



