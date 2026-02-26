//===- SparseDefUse.cpp -- Sparse Def-Use Table Implementation------------//
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
 * SparseDefUse.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: SVF Team
 *
 * Implementation of sparse def-use analysis for Abstract Interpretation.
 */

#include "AE/Svfexe/SparseDefUse.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

// Initialize static member
const SparseDefUse::ICFGNodeSet SparseDefUse::emptySet;

SparseDefUse::SparseDefUse(SVFIR* pag, PointerAnalysis* pta)
    : svfir(pag), pta(pta), built(false)
{
}

SparseDefUse::~SparseDefUse()
{
}

void SparseDefUse::build(ICFG* icfg)
{
    if (built)
        return;

    // Phase 1: Traverse ICFG and collect def/use information
    collectDefUseInfo(icfg);

    // Phase 2: Build address-taken variable use-def table
    buildAddressTakenTable();

    // Phase 3: Build top-level variable use-def table
    buildTopLevelTable();

    built = true;
}

void SparseDefUse::collectDefUseInfo(ICFG* icfg)
{
    // Traverse all ICFG nodes
    for (auto it = icfg->begin(); it != icfg->end(); ++it)
    {
        const ICFGNode* node = it->second;
        processICFGNode(node);
    }
}

void SparseDefUse::processICFGNode(const ICFGNode* node)
{
    // Get all SVF statements in this ICFG node
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        // Process different statement types
        if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            processStoreStmt(store, node);
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            processLoadStmt(load, node);
        }
        else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            processCopyStmt(copy, node);
        }
        else if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            processAddrStmt(addr, node);
        }
        else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            processGepStmt(gep, node);
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            processPhiStmt(phi, node);
        }
        else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            processSelectStmt(select, node);
        }
        else if (const BinaryOPStmt* binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            processBinaryOPStmt(binary, node);
        }
        else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            processCmpStmt(cmp, node);
        }
        else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            processCallPE(callPE, node);
        }
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            processRetPE(retPE, node);
        }
        // UnaryOPStmt, BranchStmt, etc. are handled similarly if needed
    }
}

void SparseDefUse::processStoreStmt(const StoreStmt* store, const ICFGNode* node)
{
    // store rhs -> *lhs
    // lhs is a pointer, pts(lhs) contains objects being DEFined
    NodeID ptrId = store->getLHSVarID();
    const PointsTo& pts = pta->getPts(ptrId);

    for (NodeID objId : pts)
    {
        objToDefs[objId].insert(node);
    }

    // rhs is a USE (the value being stored)
    NodeID rhsId = store->getRHSVarID();
    topLevelUses[rhsId].insert(node);
}

void SparseDefUse::processLoadStmt(const LoadStmt* load, const ICFGNode* node)
{
    // lhs = load *rhs
    // rhs is a pointer, pts(rhs) contains objects being USEd
    NodeID ptrId = load->getRHSVarID();
    const PointsTo& pts = pta->getPts(ptrId);

    for (NodeID objId : pts)
    {
        objToUses[objId].insert(node);
    }

    // lhs is DEFined (top-level, SSA single definition)
    NodeID lhsId = load->getLHSVarID();
    topLevelDef[lhsId] = node;
}

void SparseDefUse::processCopyStmt(const CopyStmt* copy, const ICFGNode* node)
{
    // lhs = rhs (includes type casts: ZEXT, SEXT, BITCAST, TRUNC, etc.)
    NodeID lhsId = copy->getLHSVarID();
    NodeID rhsId = copy->getRHSVarID();

    // lhs is DEFined
    topLevelDef[lhsId] = node;

    // rhs is USEd
    topLevelUses[rhsId].insert(node);
}

void SparseDefUse::processAddrStmt(const AddrStmt* addr, const ICFGNode* node)
{
    // lhs = &rhs (address of)
    NodeID lhsId = addr->getLHSVarID();

    // lhs is DEFined with the address
    topLevelDef[lhsId] = node;

    // rhs is the object whose address is taken - not really a "use" in the data flow sense
    // but we track it for completeness
}

void SparseDefUse::processGepStmt(const GepStmt* gep, const ICFGNode* node)
{
    // lhs = &(rhs + offset) - GEP computes address
    NodeID lhsId = gep->getLHSVarID();
    NodeID rhsId = gep->getRHSVarID();

    // lhs is DEFined
    topLevelDef[lhsId] = node;

    // rhs (base pointer) is USEd
    topLevelUses[rhsId].insert(node);
}

void SparseDefUse::processPhiStmt(const PhiStmt* phi, const ICFGNode* node)
{
    // result = phi(op1, op2, ...)
    NodeID resId = phi->getResID();

    // result is DEFined
    topLevelDef[resId] = node;

    // All operands are USEd
    for (u32_t i = 0; i < phi->getOpVarNum(); ++i)
    {
        NodeID opId = phi->getOpVarID(i);
        topLevelUses[opId].insert(node);
    }
}

void SparseDefUse::processSelectStmt(const SelectStmt* select, const ICFGNode* node)
{
    // result = condition ? trueVal : falseVal
    NodeID resId = select->getResID();

    // result is DEFined
    topLevelDef[resId] = node;

    // condition, trueVal, falseVal are USEd
    for (u32_t i = 0; i < select->getOpVarNum(); ++i)
    {
        NodeID opId = select->getOpVarID(i);
        topLevelUses[opId].insert(node);
    }
}

void SparseDefUse::processBinaryOPStmt(const BinaryOPStmt* binary, const ICFGNode* node)
{
    // result = op1 binaryOp op2
    NodeID resId = binary->getResID();

    // result is DEFined
    topLevelDef[resId] = node;

    // Both operands are USEd
    for (u32_t i = 0; i < binary->getOpVarNum(); ++i)
    {
        NodeID opId = binary->getOpVarID(i);
        topLevelUses[opId].insert(node);
    }
}

void SparseDefUse::processCmpStmt(const CmpStmt* cmp, const ICFGNode* node)
{
    // result = op1 cmp op2
    NodeID resId = cmp->getResID();

    // result is DEFined
    topLevelDef[resId] = node;

    // Both operands are USEd
    for (u32_t i = 0; i < cmp->getOpVarNum(); ++i)
    {
        NodeID opId = cmp->getOpVarID(i);
        topLevelUses[opId].insert(node);
    }
}

void SparseDefUse::processCallPE(const CallPE* callPE, const ICFGNode* node)
{
    // Parameter passing from actual to formal: formal = actual
    NodeID formalId = callPE->getLHSVarID();  // formal param (DEF)
    NodeID actualId = callPE->getRHSVarID();  // actual arg (USE)

    // formal parameter is DEFined at the call site
    topLevelDef[formalId] = node;

    // actual argument is USEd
    topLevelUses[actualId].insert(node);
}

void SparseDefUse::processRetPE(const RetPE* retPE, const ICFGNode* node)
{
    // Return value passing: actual_ret = formal_ret
    NodeID actualRetId = retPE->getLHSVarID();  // actual return (DEF)
    NodeID formalRetId = retPE->getRHSVarID();  // formal return (USE)

    // actual return is DEFined
    topLevelDef[actualRetId] = node;

    // formal return is USEd
    topLevelUses[formalRetId].insert(node);
}

void SparseDefUse::buildAddressTakenTable()
{
    // Phase 2: For each object that has uses, map use nodes to def nodes
    for (const auto& objUse : objToUses)
    {
        NodeID objId = objUse.first;
        const ICFGNodeSet& useNodes = objUse.second;

        // Find the definition nodes for this object
        auto defIt = objToDefs.find(objId);
        if (defIt == objToDefs.end())
            continue;  // No definitions found (should be rare)

        const ICFGNodeSet& defNodes = defIt->second;

        // For each use node, record all possible def nodes
        for (const ICFGNode* useNode : useNodes)
        {
            UseDefKey key(objId, useNode);
            useDefTable[key] = defNodes;
        }
    }
}

void SparseDefUse::buildTopLevelTable()
{
    // Phase 3: For each top-level variable that has uses, map use nodes to def node
    for (const auto& varUse : topLevelUses)
    {
        NodeID varId = varUse.first;
        const ICFGNodeSet& useNodes = varUse.second;

        // Find the single definition node for this variable (SSA)
        auto defIt = topLevelDef.find(varId);
        if (defIt == topLevelDef.end())
            continue;  // No definition found (external/global initial value)

        const ICFGNode* defNode = defIt->second;

        // For each use node, record the single def node
        for (const ICFGNode* useNode : useNodes)
        {
            UseDefKey key(varId, useNode);
            useDefTable[key].insert(defNode);
        }
    }
}

const SparseDefUse::ICFGNodeSet& SparseDefUse::getDefNodes(NodeID varId, const ICFGNode* useNode) const
{
    UseDefKey key(varId, useNode);
    auto it = useDefTable.find(key);
    if (it != useDefTable.end())
        return it->second;
    return emptySet;
}

bool SparseDefUse::hasDefNodes(NodeID varId, const ICFGNode* useNode) const
{
    UseDefKey key(varId, useNode);
    auto it = useDefTable.find(key);
    return it != useDefTable.end() && !it->second.empty();
}

const SparseDefUse::ICFGNodeSet& SparseDefUse::getObjDefNodes(NodeID objId) const
{
    auto it = objToDefs.find(objId);
    if (it != objToDefs.end())
        return it->second;
    return emptySet;
}

const SparseDefUse::ICFGNodeSet& SparseDefUse::getObjUseNodes(NodeID objId) const
{
    auto it = objToUses.find(objId);
    if (it != objToUses.end())
        return it->second;
    return emptySet;
}

const ICFGNode* SparseDefUse::getTopLevelDefNode(NodeID varId) const
{
    auto it = topLevelDef.find(varId);
    if (it != topLevelDef.end())
        return it->second;
    return nullptr;
}

void SparseDefUse::printStats() const
{
    SVFUtil::outs() << "\n=== SparseDefUse Statistics ===\n";
    SVFUtil::outs() << "Use-Def Table entries: " << useDefTable.size() << "\n";
    SVFUtil::outs() << "Objects with definitions: " << objToDefs.size() << "\n";
    SVFUtil::outs() << "Objects with uses: " << objToUses.size() << "\n";
    SVFUtil::outs() << "Top-level vars with definitions: " << topLevelDef.size() << "\n";
    SVFUtil::outs() << "Top-level vars with uses: " << topLevelUses.size() << "\n";

    // Calculate average def nodes per use
    if (!useDefTable.empty())
    {
        size_t totalDefNodes = 0;
        for (const auto& entry : useDefTable)
        {
            totalDefNodes += entry.second.size();
        }
        double avgDefNodes = static_cast<double>(totalDefNodes) / useDefTable.size();
        SVFUtil::outs() << "Average def nodes per use: " << avgDefNodes << "\n";
    }

    SVFUtil::outs() << "================================\n\n";
}
