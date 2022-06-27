//===- VFG.cpp -- Sparse value-flow graph-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * VFG.cpp
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei Sui
 */


#include <Graphs/SVFGNode.h>
#include "Util/Options.h"
#include "Graphs/VFG.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

const std::string VFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VFGNode ID: " << getId() << " ";
    return rawstr.str();
}

const std::string StmtVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "StmtVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}

const NodeBS LoadVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getPAGDstNodeID());
    return nb;
}

const std::string LoadVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "LoadVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}

const NodeBS StoreVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    for (auto edge: getOutEdges())
    {
        if (IndirectSVFGEdge *iedge = SVFUtil::dyn_cast<IndirectSVFGEdge>(edge))
        {
            nb |= iedge->getPointsTo();
        }
    }
    return nb;
}

const std::string StoreVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "StoreVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}

const NodeBS CopyVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getPAGDstNodeID());
    return nb;
}

const std::string CopyVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CopyVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}

const NodeBS CmpVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRes()->getId());
    return nb;
}

const std::string CmpVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CmpVFGNode ID: " << getId() << " ";
    rawstr << "PAGEdge: [" << res->getId() << " = cmp(";
    for(CmpVFGNode::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << it->second->getId() << ", ";
    rawstr << ")]\n";
    if(res->hasValue())
    {
        rawstr << " " << value2String(res->getValue());
    }
    return rawstr.str();
}

const NodeBS BinaryOPVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRes()->getId());
    return nb;
}

const std::string BinaryOPVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BinaryOPVFGNode ID: " << getId() << " ";
    rawstr << "PAGEdge: [" << res->getId() << " = Binary(";
    for(BinaryOPVFGNode::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << it->second->getId() << ", ";
    rawstr << ")]\t";
    if(res->hasValue())
    {
        rawstr << " " << value2String(res->getValue());
    }
    return rawstr.str();
}

const NodeBS UnaryOPVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRes()->getId());
    return nb;
}

const std::string UnaryOPVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "UnaryOPVFGNode ID: " << getId() << " ";
    rawstr << "PAGEdge: [" << res->getId() << " = Unary(";
    for(UnaryOPVFGNode::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << it->second->getId() << ", ";
    rawstr << ")]\t";
    if(res->hasValue())
    {
        rawstr << " " << value2String(res->getValue());
    }
    return rawstr.str();
}

const NodeBS BranchVFGNode::getDefSVFVars() const
{
    return NodeBS();
}

const std::string BranchVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BranchVFGNode ID: " << getId() << " ";
    rawstr << "PAGEdge: [" << brstmt->toString() << "\t";
    return rawstr.str();
}

const NodeBS GepVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getPAGDstNodeID());
    return nb;
}

const std::string GepVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}

const NodeBS PHIVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRes()->getId());
    return nb;
}

const std::string PHIVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "PHIVFGNode ID: " << getId() << " ";
    rawstr << "PAGNode: [" << res->getId() << " = PHI(";
    for(PHIVFGNode::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << it->second->getId() << ", ";
    rawstr << ")]\t";
    if(res->hasValue())
    {
        rawstr << " " << value2String(res->getValue());
    }
    return rawstr.str();
}


const std::string IntraPHIVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraPHIVFGNode ID: " << getId() << " ";
    rawstr << "PAGNode: [" << res->getId() << " = PHI(";
    for(PHIVFGNode::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << it->second->getId() << ", ";
    rawstr << ")]\t";
    if(res->hasValue())
    {
        rawstr << " " << value2String(res->getValue());
    }
    return rawstr.str();
}

const NodeBS AddrVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getPAGDstNodeID());
    return nb;
}

const std::string AddrVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "AddrVFGNode ID: " << getId() << " ";
    rawstr << getPAGEdge()->toString();
    return rawstr.str();
}


const std::string ArgumentVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ArgumentVFGNode ID: " << getId() << " ";
    rawstr << param->toString();
    return rawstr.str();
}

const NodeBS ActualParmVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getParam()->getId());
    return nb;
}

const std::string ActualParmVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ActualParmVFGNode ID: " << getId() << " ";
    rawstr << "CS[" << getSourceLoc(getCallSite()->getCallSite()) << "]";
    rawstr << param->toString();
    return rawstr.str();
}

const NodeBS FormalParmVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getParam()->getId());
    return nb;
}

const std::string FormalParmVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FormalParmVFGNode ID: " << getId() << " ";
    rawstr << "Fun[" << getFun()->getName() << "]";
    rawstr << param->toString();
    return rawstr.str();
}

const NodeBS ActualRetVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRev()->getId());
    return nb;
}

const std::string ActualRetVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ActualRetVFGNode ID: " << getId() << " ";
    rawstr << "CS[" << getSourceLoc(getCallSite()->getCallSite()) << "]";
    rawstr << param->toString();
    return rawstr.str();
}


const NodeBS FormalRetVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getRet()->getId());
    return nb;
}

const std::string FormalRetVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FormalRetVFGNode ID: " << getId() << " ";
    rawstr << "Fun[" << getFun()->getName() << "]";
    rawstr << param->toString();
    return rawstr.str();
}


const std::string InterPHIVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    if(isFormalParmPHI())
        rawstr << "FormalParmPHI ID: " << getId() << " PAGNode ID: " << res->getId() << "\n" << value2String(res->getValue());
    else
        rawstr << "ActualRetPHI ID: " << getId() << " PAGNode ID: " << res->getId() << "\n" << value2String(res->getValue());
    return rawstr.str();
}

const NodeBS NullPtrVFGNode::getDefSVFVars() const
{
    NodeBS nb;
    nb.set(getPAGNode()->getId());
    return nb;
}

const std::string NullPtrVFGNode::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "NullPtrVFGNode ID: " << getId();
    rawstr << " PAGNode ID: " << node->getId() << "\n";
    return rawstr.str();
}


const std::string VFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VFGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string DirectSVFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "DirectVFGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string IntraDirSVFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraDirSVFGEdge: [" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string CallDirSVFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallDirSVFGEdge CallSite ID: " << getCallSiteId() << " [";
    rawstr << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string RetDirSVFGEdge::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetDirSVFGEdge CallSite ID: " << getCallSiteId() << " [";
    rawstr << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}



FormalRetVFGNode::FormalRetVFGNode(NodeID id, const PAGNode* n, const SVFFunction* f) :
    ArgumentVFGNode(id, n, FRet), fun(f)
{
}

PHIVFGNode::PHIVFGNode(NodeID id, const PAGNode* r,VFGNodeK k): VFGNode(id, k), res(r)
{

}

/*!
 * Constructor
 *  * Build VFG
 * 1) build VFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect VFG edges
 *    between two statements (PAGEdges)
 */
VFG::VFG(PTACallGraph* cg, VFGK k): totalVFGNode(0), callgraph(cg), pag(SVFIR::getPAG()), kind(k)
{

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate VFG Top Level Node\n"));
    addVFGNodes();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Direct Edge\n"));
    connectDirectVFGEdges();
}

/*!
 * Memory has been cleaned up at GenericGraph
 */
void VFG::destroy()
{
    pag = nullptr;
}


/*!
 * Create VFG nodes for top level pointers
 */
void VFG::addVFGNodes()
{

    // initialize dummy definition  null pointers in order to uniform the construction
    // to be noted for black hole pointer it has already has address edge connected,
    // and its definition will be set when processing addr SVFIR edge.
    addNullPtrVFGNode(pag->getGNode(pag->getNullPtr()));

    // initialize address nodes
    SVFStmt::SVFStmtSetTy& addrs = getPAGEdgeSet(SVFStmt::Addr);
    for (SVFStmt::SVFStmtSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter)
    {
        addAddrVFGNode(SVFUtil::cast<AddrStmt>(*iter));
    }

    // initialize copy nodes
    SVFStmt::SVFStmtSetTy& copys = getPAGEdgeSet(SVFStmt::Copy);
    for (SVFStmt::SVFStmtSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter)
    {
        const CopyStmt* edge = SVFUtil::cast<CopyStmt>(*iter);
        assert(!isPhiCopyEdge(edge) && "Copy edges can not be a PhiNode (or from PhiNode)");
        addCopyVFGNode(edge);
    }

    // initialize gep nodes
    SVFStmt::SVFStmtSetTy& ngeps = getPAGEdgeSet(SVFStmt::Gep);
    for (SVFStmt::SVFStmtSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter)
    {
        addGepVFGNode(SVFUtil::cast<GepStmt>(*iter));
    }

    // initialize load nodes
    SVFStmt::SVFStmtSetTy& loads = getPAGEdgeSet(SVFStmt::Load);
    for (SVFStmt::SVFStmtSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter)
    {
        addLoadVFGNode(SVFUtil::cast<LoadStmt>(*iter));
    }

    // initialize store nodes
    SVFStmt::SVFStmtSetTy& stores = getPAGEdgeSet(SVFStmt::Store);
    for (SVFStmt::SVFStmtSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        addStoreVFGNode(SVFUtil::cast<StoreStmt>(*iter));
    }

    SVFStmt::SVFStmtSetTy& forks = getPAGEdgeSet(SVFStmt::ThreadFork);
    for (SVFStmt::SVFStmtSetTy::iterator iter = forks.begin(), eiter =
                forks.end(); iter != eiter; ++iter)
    {
        TDForkPE* forkedge = SVFUtil::cast<TDForkPE>(*iter);
        addActualParmVFGNode(forkedge->getRHSVar(),forkedge->getCallSite());
    }

    // initialize actual parameter nodes
    for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(), eit = pag->getCallSiteArgsMap().end(); it !=eit; ++it)
    {

        for(SVFIR::SVFVarList::iterator pit = it->second.begin(), epit = it->second.end(); pit!=epit; ++pit)
        {
            const PAGNode* pagNode = *pit;
            if (isInterestedPAGNode(pagNode))
                addActualParmVFGNode(pagNode,it->first);
        }
    }

    // initialize actual return nodes (callsite return)
    for(SVFIR::CSToRetMap::iterator it = pag->getCallSiteRets().begin(), eit = pag->getCallSiteRets().end(); it !=eit; ++it)
    {

        /// for external function we do not create acutalRet VFGNode
        /// they are in the formal of AddrVFGNode if the external function returns an allocated memory
        /// if fun has body, it may also exist in isExtCall, e.g., xmalloc() in bzip2, spec2000.
        if(isInterestedPAGNode(it->second) == false || hasDef(it->second))
            continue;

        addActualRetVFGNode(it->second,it->first->getCallICFGNode());
    }

    // initialize formal parameter nodes
    for(SVFIR::FunToArgsListMap::iterator it = pag->getFunArgsMap().begin(), eit = pag->getFunArgsMap().end(); it !=eit; ++it)
    {
        const SVFFunction* func = it->first;

        for(SVFIR::SVFVarList::iterator pit = it->second.begin(), epit = it->second.end(); pit!=epit; ++pit)
        {
            const PAGNode* param = *pit;
            if (isInterestedPAGNode(param) == false || hasBlackHoleConstObjAddrAsDef(param))
                continue;

            CallPESet callPEs;
            if (param->hasIncomingEdges(SVFStmt::Call))
            {
                for (SVFStmt::SVFStmtSetTy::const_iterator cit = param->getIncomingEdgesBegin(SVFStmt::Call), ecit =
                            param->getIncomingEdgesEnd(SVFStmt::Call); cit != ecit; ++cit)
                {
                    CallPE* callPE = SVFUtil::cast<CallPE>(*cit);
                    if (isInterestedPAGNode(callPE->getRHSVar()))
                        callPEs.insert(callPE);
                }
            }
            addFormalParmVFGNode(param,func,callPEs);
        }

        if (func->getLLVMFun()->getFunctionType()->isVarArg())
        {
            const PAGNode* varParam = pag->getGNode(pag->getVarargNode(func));
            if (isInterestedPAGNode(varParam) == false || hasBlackHoleConstObjAddrAsDef(varParam))
                continue;

            CallPESet callPEs;
            if (varParam->hasIncomingEdges(SVFStmt::Call))
            {
                for(SVFStmt::SVFStmtSetTy::const_iterator cit = varParam->getIncomingEdgesBegin(SVFStmt::Call),
                        ecit = varParam->getIncomingEdgesEnd(SVFStmt::Call); cit!=ecit; ++cit)
                {
                    CallPE* callPE = SVFUtil::cast<CallPE>(*cit);
                    if(isInterestedPAGNode(callPE->getRHSVar()))
                        callPEs.insert(callPE);
                }
            }
            addFormalParmVFGNode(varParam,func,callPEs);
        }
    }

    // initialize formal return nodes (callee return)
    for (SVFIR::FunToRetMap::iterator it = pag->getFunRets().begin(), eit = pag->getFunRets().end(); it != eit; ++it)
    {
        const SVFFunction* func = it->first;

        const PAGNode* uniqueFunRetNode = it->second;

        RetPESet retPEs;
        if (uniqueFunRetNode->hasOutgoingEdges(SVFStmt::Ret))
        {
            for (SVFStmt::SVFStmtSetTy::const_iterator cit = uniqueFunRetNode->getOutgoingEdgesBegin(SVFStmt::Ret),
                    ecit = uniqueFunRetNode->getOutgoingEdgesEnd(SVFStmt::Ret);
                    cit != ecit; ++cit)
            {
                const RetPE* retPE = SVFUtil::cast<RetPE>(*cit);
                if (isInterestedPAGNode(retPE->getLHSVar()))
                    retPEs.insert(retPE);
            }
        }

        if(isInterestedPAGNode(uniqueFunRetNode))
            addFormalRetVFGNode(uniqueFunRetNode, func, retPEs);
    }

    // initialize llvm phi nodes (phi of top level pointers)
    SVFStmt::SVFStmtSetTy& phis = getPAGEdgeSet(SVFStmt::Phi);
    for (SVFStmt::SVFStmtSetTy::iterator iter = phis.begin(), eiter =
                phis.end(); iter != eiter; ++iter)
    {
        const PhiStmt* edge = SVFUtil::cast<PhiStmt>(*iter);
        if(isInterestedPAGNode(edge->getRes()))
            addIntraPHIVFGNode(edge);
    }
    // initialize select statement
    SVFStmt::SVFStmtSetTy& selects = getPAGEdgeSet(SVFStmt::Select);
    for (SVFStmt::SVFStmtSetTy::iterator iter = selects.begin(), eiter =
                selects.end(); iter != eiter; ++iter)
    {
        const MultiOpndStmt* edge = SVFUtil::cast<MultiOpndStmt>(*iter);
        if(isInterestedPAGNode(edge->getRes()))
            addIntraPHIVFGNode(edge);
    }
    // initialize llvm binary nodes (binary operators)
    SVFStmt::SVFStmtSetTy& binaryops = getPAGEdgeSet(SVFStmt::BinaryOp);
    for (SVFStmt::SVFStmtSetTy::iterator iter = binaryops.begin(), eiter =
                binaryops.end(); iter != eiter; ++iter)
    {
        const BinaryOPStmt* edge = SVFUtil::cast<BinaryOPStmt>(*iter);
        if(isInterestedPAGNode(edge->getRes()))
            addBinaryOPVFGNode(edge);
    }
    // initialize llvm unary nodes (unary operators)
    SVFStmt::SVFStmtSetTy& unaryops = getPAGEdgeSet(SVFStmt::UnaryOp);
    for (SVFStmt::SVFStmtSetTy::iterator iter = unaryops.begin(), eiter =
                unaryops.end(); iter != eiter; ++iter)
    {
        const UnaryOPStmt* edge = SVFUtil::cast<UnaryOPStmt>(*iter);
        if(isInterestedPAGNode(edge->getRes()))
            addUnaryOPVFGNode(edge);
    }
    // initialize llvm unary nodes (unary operators)
    SVFStmt::SVFStmtSetTy& brs = getPAGEdgeSet(SVFStmt::Branch);
    for (SVFStmt::SVFStmtSetTy::iterator iter = brs.begin(), eiter =
                brs.end(); iter != eiter; ++iter)
    {
        const BranchStmt* edge = SVFUtil::cast<BranchStmt>(*iter);
        if(isInterestedPAGNode(edge->getBranchInst()))
            addBranchVFGNode(edge);
    }
    // initialize llvm cmp nodes (comparision)
    SVFStmt::SVFStmtSetTy& cmps = getPAGEdgeSet(SVFStmt::Cmp);
    for (SVFStmt::SVFStmtSetTy::iterator iter = cmps.begin(), eiter =
                cmps.end(); iter != eiter; ++iter)
    {
        const CmpStmt* edge = SVFUtil::cast<CmpStmt>(*iter);
        if(isInterestedPAGNode(edge->getRes()))
            addCmpVFGNode(edge);
    }
}

/*!
 * Add def-use edges for top level pointers
 */
VFGEdge* VFG::addIntraDirectVFEdge(NodeID srcId, NodeID dstId)
{
    VFGNode* srcNode = getVFGNode(srcId);
    VFGNode* dstNode = getVFGNode(dstId);
    checkIntraEdgeParents(srcNode, dstNode);
    if(VFGEdge* edge = hasIntraVFGEdge(srcNode,dstNode, VFGEdge::IntraDirectVF))
    {
        assert(edge->isDirectVFGEdge() && "this should be a direct value flow edge!");
        return nullptr;
    }
    else
    {
        if(srcNode!=dstNode)
        {
            IntraDirSVFGEdge* directEdge = new IntraDirSVFGEdge(srcNode,dstNode);
            return (addVFGEdge(directEdge) ? directEdge : nullptr);
        }
        else
            return nullptr;
    }
}

/*!
 * Add interprocedural call edges for top level pointers
 */
VFGEdge* VFG::addCallEdge(NodeID srcId, NodeID dstId, CallSiteID csId)
{
    VFGNode* srcNode = getVFGNode(srcId);
    VFGNode* dstNode = getVFGNode(dstId);
    if(VFGEdge* edge = hasInterVFGEdge(srcNode,dstNode, VFGEdge::CallDirVF,csId))
    {
        assert(edge->isCallDirectVFGEdge() && "this should be a direct value flow edge!");
        return nullptr;
    }
    else
    {
        CallDirSVFGEdge* callEdge = new CallDirSVFGEdge(srcNode,dstNode,csId);
        return (addVFGEdge(callEdge) ? callEdge : nullptr);
    }
}

/*!
 * Add interprocedural return edges for top level pointers
 */
VFGEdge* VFG::addRetEdge(NodeID srcId, NodeID dstId, CallSiteID csId)
{
    VFGNode* srcNode = getVFGNode(srcId);
    VFGNode* dstNode = getVFGNode(dstId);
    if(VFGEdge* edge = hasInterVFGEdge(srcNode,dstNode, VFGEdge::RetDirVF,csId))
    {
        assert(edge->isRetDirectVFGEdge() && "this should be a direct value flow edge!");
        return nullptr;
    }
    else
    {
        RetDirSVFGEdge* retEdge = new RetDirSVFGEdge(srcNode,dstNode,csId);
        return (addVFGEdge(retEdge) ? retEdge : nullptr);
    }
}


/*!
 * Connect def-use chains for direct value-flow, (value-flow of top level pointers)
 */
void VFG::connectDirectVFGEdges()
{

    for(iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        NodeID nodeId = it->first;
        VFGNode* node = it->second;

        if(StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node))
        {
            /// do not handle AddrSVFG node, as it is already the source of a definition
            if(SVFUtil::isa<AddrVFGNode>(stmtNode))
                continue;
            /// for all other cases, like copy/gep/load/ret, connect the RHS pointer to its def
            if (stmtNode->getPAGSrcNode()->isConstantData() == false)
                addIntraDirectVFEdge(getDef(stmtNode->getPAGSrcNode()), nodeId);

            /// for store, connect the RHS/LHS pointer to its def
            if(SVFUtil::isa<StoreVFGNode>(stmtNode) && (stmtNode->getPAGDstNode()->isConstantData() == false))
            {
                addIntraDirectVFEdge(getDef(stmtNode->getPAGDstNode()), nodeId);
            }

        }
        else if(PHIVFGNode* phiNode = SVFUtil::dyn_cast<PHIVFGNode>(node))
        {
            for (PHIVFGNode::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd(); it != eit; it++)
            {
                if (it->second->isConstantData() == false)
                    addIntraDirectVFEdge(getDef(it->second), nodeId);
            }
        }
        else if(BinaryOPVFGNode* binaryNode = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        {
            for (BinaryOPVFGNode::OPVers::const_iterator it = binaryNode->opVerBegin(), eit = binaryNode->opVerEnd(); it != eit; it++)
            {
                if (it->second->isConstantData() == false)
                    addIntraDirectVFEdge(getDef(it->second), nodeId);
            }
        }
        else if(UnaryOPVFGNode* unaryNode = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        {
            for (UnaryOPVFGNode::OPVers::const_iterator it = unaryNode->opVerBegin(), eit = unaryNode->opVerEnd(); it != eit; it++)
            {
                if (it->second->isConstantData() == false)
                    addIntraDirectVFEdge(getDef(it->second), nodeId);
            }
        }
        else if(CmpVFGNode* cmpNode = SVFUtil::dyn_cast<CmpVFGNode>(node))
        {
            for (CmpVFGNode::OPVers::const_iterator it = cmpNode->opVerBegin(), eit = cmpNode->opVerEnd(); it != eit; it++)
            {
                if (it->second->isConstantData() == false)
                    addIntraDirectVFEdge(getDef(it->second), nodeId);
            }
        }
        else if(ActualParmVFGNode* actualParm = SVFUtil::dyn_cast<ActualParmVFGNode>(node))
        {
            if (actualParm->getParam()->isConstantData() == false)
                addIntraDirectVFEdge(getDef(actualParm->getParam()), nodeId);
        }
        else if(FormalParmVFGNode* formalParm = SVFUtil::dyn_cast<FormalParmVFGNode>(node))
        {
            for(CallPESet::const_iterator it = formalParm->callPEBegin(), eit = formalParm->callPEEnd();
                    it!=eit; ++it)
            {
                const CallICFGNode* cs = (*it)->getCallSite();
                ActualParmVFGNode* acutalParm = getActualParmVFGNode((*it)->getRHSVar(),cs);
                addInterEdgeFromAPToFP(acutalParm,formalParm,getCallSiteID(cs, formalParm->getFun()));
            }
        }
        else if(FormalRetVFGNode* calleeRet = SVFUtil::dyn_cast<FormalRetVFGNode>(node))
        {
            /// connect formal ret to its definition node
            addIntraDirectVFEdge(getDef(calleeRet->getRet()), nodeId);

            /// connect formal ret to actual ret
            for(RetPESet::const_iterator it = calleeRet->retPEBegin(), eit = calleeRet->retPEEnd(); it!=eit; ++it)
            {
                ActualRetVFGNode* callsiteRev = getActualRetVFGNode((*it)->getLHSVar());
                const CallICFGNode* retBlockNode = (*it)->getCallSite();
                CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(retBlockNode->getCallSite());
                addInterEdgeFromFRToAR(calleeRet,callsiteRev, getCallSiteID(callBlockNode, calleeRet->getFun()));
            }
        }
        /// Do not process FormalRetVFGNode, as they are connected by copy within callee
        /// We assume one procedure only has unique return
    }

    /// connect direct value-flow edges (parameter passing) for thread fork/join
    if(Options::EnableThreadCallGraph)
    {
        /// add fork edge
        SVFStmt::SVFStmtSetTy& forks = getPAGEdgeSet(SVFStmt::ThreadFork);
        for (SVFStmt::SVFStmtSetTy::iterator iter = forks.begin(), eiter =
                    forks.end(); iter != eiter; ++iter)
        {
            TDForkPE* forkedge = SVFUtil::cast<TDForkPE>(*iter);
            ActualParmVFGNode* acutalParm = getActualParmVFGNode(forkedge->getRHSVar(),forkedge->getCallSite());
            FormalParmVFGNode* formalParm = getFormalParmVFGNode(forkedge->getLHSVar());
            addInterEdgeFromAPToFP(acutalParm,formalParm,getCallSiteID(forkedge->getCallSite(), formalParm->getFun()));
        }
        /// add join edge
        SVFStmt::SVFStmtSetTy& joins = getPAGEdgeSet(SVFStmt::ThreadJoin);
        for (SVFStmt::SVFStmtSetTy::iterator iter = joins.begin(), eiter =
                    joins.end(); iter != eiter; ++iter)
        {
            TDJoinPE* joinedge = SVFUtil::cast<TDJoinPE>(*iter);
            NodeID callsiteRev = getDef(joinedge->getLHSVar());
            FormalRetVFGNode* calleeRet = getFormalRetVFGNode(joinedge->getRHSVar());
            addRetEdge(calleeRet->getId(),callsiteRev, getCallSiteID(joinedge->getCallSite(), calleeRet->getFun()));
        }
    }
}

/*!
 * Whether we has an intra VFG edge
 */
VFGEdge* VFG::hasIntraVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind)
{
    VFGEdge edge(src,dst,kind);
    VFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    VFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}


/*!
 * Whether we has an thread VFG edge
 */
VFGEdge* VFG::hasThreadVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind)
{
    VFGEdge edge(src,dst,kind);
    VFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    VFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * Whether we has an inter VFG edge
 */
VFGEdge* VFG::hasInterVFGEdge(VFGNode* src, VFGNode* dst, VFGEdge::VFGEdgeK kind,CallSiteID csId)
{
    VFGEdge edge(src,dst,VFGEdge::makeEdgeFlagWithInvokeID(kind,csId));
    VFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    VFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}


/*!
 * Return the corresponding VFGEdge
 */
VFGEdge* VFG::getIntraVFGEdge(const VFGNode* src, const VFGNode* dst, VFGEdge::VFGEdgeK kind)
{
    return hasIntraVFGEdge(const_cast<VFGNode*>(src),const_cast<VFGNode*>(dst),kind);
}


/*!
 * Dump VFG
 */
void VFG::dump(const std::string& file, bool simple)
{
    GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
}

/*!
 * View VFG from the debugger.
 */
void VFG::view()
{
    llvm::ViewGraph(this, "Value Flow Graph");
}


void VFG::updateCallGraph(PointerAnalysis* pta)
{
    VFGEdgeSetTy vfEdgesAtIndCallSite;
    PointerAnalysis::CallEdgeMap::const_iterator iter = pta->getIndCallMap().begin();
    PointerAnalysis::CallEdgeMap::const_iterator eiter = pta->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallICFGNode* newcs = iter->first;
        assert(newcs->isIndirectCall() && "this is not an indirect call?");
        const PointerAnalysis::FunctionSet & functions = iter->second;
        for (PointerAnalysis::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction*  func = *func_iter;
            connectCallerAndCallee(newcs, func, vfEdgesAtIndCallSite);
        }
    }
}

/**
 * Connect actual params/return to formal params/return for top-level variables.
 * Also connect indirect actual in/out and formal in/out.
 */
void VFG::connectCallerAndCallee(const CallICFGNode* callBlockNode, const SVFFunction* callee, VFGEdgeSetTy& edges)
{
    SVFIR * pag = SVFIR::getPAG();
    ICFG * icfg = pag->getICFG();
    CallSiteID csId = getCallSiteID(callBlockNode, callee);
    RetICFGNode* retBlockNode = icfg->getRetICFGNode(callBlockNode->getCallSite());
    // connect actual and formal param
    if (pag->hasCallSiteArgsMap(callBlockNode) && pag->hasFunArgsList(callee))
    {
        const SVFIR::SVFVarList& csArgList = pag->getCallSiteArgsList(callBlockNode);
        const SVFIR::SVFVarList& funArgList = pag->getFunArgsList(callee);
        SVFIR::SVFVarList::const_iterator csArgIt = csArgList.begin(), csArgEit = csArgList.end();
        SVFIR::SVFVarList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        for (; funArgIt != funArgEit && csArgIt != csArgEit; funArgIt++, csArgIt++)
        {
            const PAGNode *cs_arg = *csArgIt;
            const PAGNode *fun_arg = *funArgIt;
            if (fun_arg->isPointer() && cs_arg->isPointer())
                connectAParamAndFParam(cs_arg, fun_arg, callBlockNode, csId, edges);
        }
        assert(funArgIt == funArgEit && "function has more arguments than call site");
        if (callee->getLLVMFun()->isVarArg())
        {
            NodeID varFunArg = pag->getVarargNode(callee);
            const PAGNode* varFunArgNode = pag->getGNode(varFunArg);
            if (varFunArgNode->isPointer())
            {
                for (; csArgIt != csArgEit; csArgIt++)
                {
                    const PAGNode *cs_arg = *csArgIt;
                    if (cs_arg->isPointer())
                        connectAParamAndFParam(cs_arg, varFunArgNode, callBlockNode, csId, edges);
                }
            }
        }
    }

    // connect actual return and formal return
    if (pag->funHasRet(callee) && pag->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = pag->getFunRet(callee);
        if (cs_return->isPointer() && fun_return->isPointer())
            connectFRetAndARet(fun_return, cs_return, csId, edges);
    }
}

/*!
 * Given a VFG node, return its left hand side top level pointer
 */
const PAGNode* VFG::getLHSTopLevPtr(const VFGNode* node) const
{

    if(const AddrVFGNode* addr = SVFUtil::dyn_cast<AddrVFGNode>(node))
        return addr->getPAGDstNode();
    else if(const CopyVFGNode* copy = SVFUtil::dyn_cast<CopyVFGNode>(node))
        return copy->getPAGDstNode();
    else if(const GepVFGNode* gep = SVFUtil::dyn_cast<GepVFGNode>(node))
        return gep->getPAGDstNode();
    else if(const LoadVFGNode* load = SVFUtil::dyn_cast<LoadVFGNode>(node))
        return load->getPAGDstNode();
    else if(const PHIVFGNode* phi = SVFUtil::dyn_cast<PHIVFGNode>(node))
        return phi->getRes();
    else if(const CmpVFGNode* cmp = SVFUtil::dyn_cast<CmpVFGNode>(node))
        return cmp->getRes();
    else if(const BinaryOPVFGNode* bop = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        return bop->getRes();
    else if(const UnaryOPVFGNode* uop = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        return uop->getRes();
    else if(const ActualParmVFGNode* ap = SVFUtil::dyn_cast<ActualParmVFGNode>(node))
        return ap->getParam();
    else if(const FormalParmVFGNode*fp = SVFUtil::dyn_cast<FormalParmVFGNode>(node))
        return fp->getParam();
    else if(const ActualRetVFGNode* ar = SVFUtil::dyn_cast<ActualRetVFGNode>(node))
        return ar->getRev();
    else if(const FormalRetVFGNode* fr = SVFUtil::dyn_cast<FormalRetVFGNode>(node))
        return fr->getRet();
    else if(const NullPtrVFGNode* nullVFG = SVFUtil::dyn_cast<NullPtrVFGNode>(node))
        return nullVFG->getPAGNode();
    else
        assert(false && "unexpected node kind!");
    return nullptr;
}

/*!
 * Whether this is an function entry VFGNode (formal parameter, formal In)
 */
const SVFFunction* VFG::isFunEntryVFGNode(const VFGNode* node) const
{
    if(const FormalParmVFGNode* fp = SVFUtil::dyn_cast<FormalParmVFGNode>(node))
    {
        return fp->getFun();
    }
    else if(const InterPHIVFGNode* phi = SVFUtil::dyn_cast<InterPHIVFGNode>(node))
    {
        if(phi->isFormalParmPHI())
            return phi->getFun();
    }
    return nullptr;
}


const Value* StmtVFGNode::getValue() const
{
    return getPAGEdge()->getValue();
}

const Value* CmpVFGNode::getValue() const
{
    return getRes()->getValue();
}

const Value* BinaryOPVFGNode::getValue() const
{
    return getRes()->getValue();
}

const Value* PHIVFGNode::getValue() const
{
    return getRes()->getValue();
}

const Value* ArgumentVFGNode::getValue() const
{
    return param->hasValue() ? param->getValue() : nullptr;
}

/*!
 * GraphTraits specialization
 */
namespace llvm
{
template<>
struct DOTGraphTraits<VFG*> : public DOTGraphTraits<SVFIR*>
{

    typedef VFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVFIR*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(VFG*)
    {
        return "VFG";
    }

    std::string getNodeLabel(NodeType *node, VFG *graph)
    {
        if (isSimple())
            return getSimpleNodeLabel(node, graph);
        else
            return getCompleteNodeLabel(node, graph);
    }

    /// Return label of a VFG node without MemSSA information
    static std::string getSimpleNodeLabel(NodeType *node, VFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        if(StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node))
        {
            rawstr << stmtNode->toString();
        }
        else if(PHIVFGNode* tphi = SVFUtil::dyn_cast<PHIVFGNode>(node))
        {
            rawstr << tphi->toString();
        }
        else if(FormalParmVFGNode* fp = SVFUtil::dyn_cast<FormalParmVFGNode>(node))
        {
            rawstr << fp->toString();
        }
        else if(ActualParmVFGNode* ap = SVFUtil::dyn_cast<ActualParmVFGNode>(node))
        {
            rawstr << ap->toString();
        }
        else if (ActualRetVFGNode* ar = SVFUtil::dyn_cast<ActualRetVFGNode>(node))
        {
            rawstr << ar->toString();
        }
        else if (FormalRetVFGNode* fr = SVFUtil::dyn_cast<FormalRetVFGNode>(node))
        {
            rawstr << fr->toString();
        }
        else if(SVFUtil::isa<NullPtrVFGNode>(node))
        {
            rawstr << "NullPtr";
        }
        else if(BinaryOPVFGNode* bop = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        {
            rawstr << bop->toString();
        }
        else if(UnaryOPVFGNode* uop = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        {
            rawstr << uop->toString();
        }
        else if(CmpVFGNode* cmp = SVFUtil::dyn_cast<CmpVFGNode>(node))
        {
            rawstr << cmp->toString();;
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    /// Return label of a VFG node with MemSSA information
    static std::string getCompleteNodeLabel(NodeType *node, VFG*)
    {

        std::string str;
        raw_string_ostream rawstr(str);
        if(StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node))
        {
            rawstr << stmtNode->toString();
        }
        else if(BinaryOPVFGNode* bop = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        {
            rawstr << bop->toString();
        }
        else if(UnaryOPVFGNode* uop = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        {
            rawstr << uop->toString();
        }
        else if(CmpVFGNode* cmp = SVFUtil::dyn_cast<CmpVFGNode>(node))
        {
            rawstr << cmp->toString();
        }
        else if(PHIVFGNode* phi = SVFUtil::dyn_cast<PHIVFGNode>(node))
        {
            rawstr << phi->toString();
        }
        else if(FormalParmVFGNode* fp = SVFUtil::dyn_cast<FormalParmVFGNode>(node))
        {
            rawstr << fp->toString();
        }
        else if(ActualParmVFGNode* ap = SVFUtil::dyn_cast<ActualParmVFGNode>(node))
        {
            rawstr << ap->toString();
        }
        else if(NullPtrVFGNode* nptr = SVFUtil::dyn_cast<NullPtrVFGNode>(node))
        {
            rawstr << nptr->toString();
        }
        else if (ActualRetVFGNode* ar = SVFUtil::dyn_cast<ActualRetVFGNode>(node))
        {
            rawstr << ar->toString();
        }
        else if (FormalRetVFGNode* fr = SVFUtil::dyn_cast<FormalRetVFGNode>(node))
        {
            rawstr << fr->toString();
        }
        else if (MRSVFGNode* mr = SVFUtil::dyn_cast<MRSVFGNode>(node))
        {
            rawstr << mr->toString();
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, VFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);

        if(StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node))
        {
            const PAGEdge* edge = stmtNode->getPAGEdge();
            if (SVFUtil::isa<AddrStmt>(edge))
            {
                rawstr <<  "color=green";
            }
            else if (SVFUtil::isa<CopyStmt>(edge))
            {
                rawstr <<  "color=black";
            }
            else if (SVFUtil::isa<RetPE>(edge))
            {
                rawstr <<  "color=black,style=dotted";
            }
            else if (SVFUtil::isa<GepStmt>(edge))
            {
                rawstr <<  "color=purple";
            }
            else if (SVFUtil::isa<StoreStmt>(edge))
            {
                rawstr <<  "color=blue";
            }
            else if (SVFUtil::isa<LoadStmt>(edge))
            {
                rawstr << "color=red";
            }
            else
            {
                assert(0 && "No such kind edge!!");
            }
            rawstr <<  "";
        }
        else if (SVFUtil::isa<CmpVFGNode>(node))
        {
            rawstr << "color=grey";
        }
        else if (SVFUtil::isa<BinaryOPVFGNode>(node))
        {
            rawstr << "color=grey";
        }
        else if (SVFUtil::isa<UnaryOPVFGNode>(node))
        {
            rawstr << "color=grey";
        }
        else if(SVFUtil::isa<PHIVFGNode>(node))
        {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<NullPtrVFGNode>(node))
        {
            rawstr <<  "color=grey";
        }
        else if(SVFUtil::isa<FormalParmVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<ActualParmVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<ActualRetVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<FormalRetVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<MRSVFGNode>(node))
        {
            rawstr <<  "color=orange,penwidth=2";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, VFG*)
    {
        VFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<DirectSVFGEdge>(edge))
        {
            if (SVFUtil::isa<CallDirSVFGEdge>(edge))
                return "style=solid,color=red";
            else if (SVFUtil::isa<RetDirSVFGEdge>(edge))
                return "style=solid,color=blue";
            else
                return "style=solid";
        }
        else if (SVFUtil::isa<IndirectSVFGEdge>(edge))
        {
            if (SVFUtil::isa<CallIndSVFGEdge>(edge))
                return "style=dashed,color=red";
            else if (SVFUtil::isa<RetIndSVFGEdge>(edge))
                return "style=dashed,color=blue";
            else
                return "style=dashed";
        }
        else
        {
            assert(false && "what else edge we have?");
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        VFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallDirSVFGEdge* dirCall = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetDirSVFGEdge* dirRet = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();

        return rawstr.str();
    }
};
} // End namespace llvm
