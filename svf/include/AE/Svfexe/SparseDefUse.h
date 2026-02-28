//===- SparseDefUse.h -- Sparse Def-Use Table for Abstract Interpretation-//
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
 * SparseDefUse.h
 *
 *  Created on: Feb 9, 2026
 *      Author: SVF Team
 *
 * This file implements sparse def-use analysis for Abstract Interpretation.
 * It builds a Use-Def table that maps each (VarID, UseNode) pair to the set
 * of possible definition nodes, enabling sparse propagation.
 *
 * Key concepts:
 * - Top-level variables: SSA form, single definition point
 * - Address-taken variables: accessed via pointers, use Andersen's pts for def-use
 */

#ifndef INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_
#define INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_

#include "SVFIR/SVFIR.h"
#include "Graphs/ICFG.h"
#include "MemoryModel/PointerAnalysis.h"
#include "WPA/Andersen.h"

namespace SVF
{

/// Key for the Use-Def table: (VarID, UseNode)
struct UseDefKey
{
    NodeID varId;
    const ICFGNode* useNode;

    UseDefKey(NodeID v, const ICFGNode* n) : varId(v), useNode(n) {}

    bool operator==(const UseDefKey& other) const
    {
        return varId == other.varId && useNode == other.useNode;
    }
};

/// Hash function for UseDefKey
struct UseDefKeyHash
{
    size_t operator()(const UseDefKey& key) const
    {
        return std::hash<NodeID>()(key.varId) ^
               (std::hash<const ICFGNode*>()(key.useNode) << 1);
    }
};

/// SparseDefUse builds and maintains a Use-Def table for sparse abstract interpretation.
/// It enables the analysis to skip nodes without relevant data dependencies.
class SparseDefUse
{
public:
    /// Type definitions
    using ICFGNodeSet = Set<const ICFGNode*>;
    using UseDefTable = Map<UseDefKey, ICFGNodeSet, UseDefKeyHash>;
    using ObjToNodesMap = Map<NodeID, ICFGNodeSet>;
    using VarToNodeMap = Map<NodeID, const ICFGNode*>;
    using VarToNodesMap = Map<NodeID, ICFGNodeSet>;

    /// Constructor
    /// @param pag The SVFIR to analyze
    /// @param pta The pointer analysis result (Andersen)
    SparseDefUse(SVFIR* pag, PointerAnalysis* pta);

    /// Destructor
    virtual ~SparseDefUse();

    /// Build the def-use table by traversing the ICFG
    /// @param icfg The ICFG to traverse
    void build(ICFG* icfg);

    /// Get definition nodes for a variable at a use node
    /// @param varId The variable ID
    /// @param useNode The ICFG node where the variable is used
    /// @return Set of ICFG nodes that may define the variable
    const ICFGNodeSet& getDefNodes(NodeID varId, const ICFGNode* useNode) const;

    /// Check if a variable has any definition reaching a use node
    /// @param varId The variable ID
    /// @param useNode The ICFG node where the variable is used
    /// @return true if there are definition nodes
    bool hasDefNodes(NodeID varId, const ICFGNode* useNode) const;

    /// Get all nodes that may define an object (address-taken variable)
    /// @param objId The object variable ID
    /// @return Set of ICFG nodes that may define the object
    const ICFGNodeSet& getObjDefNodes(NodeID objId) const;

    /// Get all nodes that may use an object (address-taken variable)
    /// @param objId The object variable ID
    /// @return Set of ICFG nodes that may use the object
    const ICFGNodeSet& getObjUseNodes(NodeID objId) const;

    /// Get the single definition node for a top-level variable
    /// @param varId The variable ID
    /// @return The ICFG node that defines the variable, or nullptr if not found
    const ICFGNode* getTopLevelDefNode(NodeID varId) const;

    /// Check if the def-use table has been built
    /// @return true if build() has been called
    bool isBuilt() const
    {
        return built;
    }

    /// Print statistics about the def-use table
    void printStats() const;

protected:
    /// Phase 1: Traverse ICFG and collect def/use information
    void collectDefUseInfo(ICFG* icfg);

    /// Process a single ICFG node to collect def/use info
    void processICFGNode(const ICFGNode* node);

    /// Process different statement types
    void processStoreStmt(const StoreStmt* store, const ICFGNode* node);
    void processLoadStmt(const LoadStmt* load, const ICFGNode* node);
    void processCopyStmt(const CopyStmt* copy, const ICFGNode* node);
    void processAddrStmt(const AddrStmt* addr, const ICFGNode* node);
    void processGepStmt(const GepStmt* gep, const ICFGNode* node);
    void processPhiStmt(const PhiStmt* phi, const ICFGNode* node);
    void processSelectStmt(const SelectStmt* select, const ICFGNode* node);
    void processBinaryOPStmt(const BinaryOPStmt* binary, const ICFGNode* node);
    void processCmpStmt(const CmpStmt* cmp, const ICFGNode* node);
    void processCallPE(const CallPE* callPE, const ICFGNode* node);
    void processRetPE(const RetPE* retPE, const ICFGNode* node);

    /// Phase 2: Build address-taken variable use-def table
    void buildAddressTakenTable();

    /// Phase 3: Build top-level variable use-def table
    void buildTopLevelTable();

private:
    /// The SVFIR to analyze
    SVFIR* svfir;

    /// Pointer analysis result
    PointerAnalysis* pta;

    /// The final Use-Def table
    UseDefTable useDefTable;

    /// Intermediate structures for address-taken variables
    ObjToNodesMap objToDefs;    // Object -> nodes that DEF it
    ObjToNodesMap objToUses;    // Object -> nodes that USE it

    /// Intermediate structures for top-level variables
    VarToNodeMap topLevelDef;   // Variable -> single def node (SSA)
    VarToNodesMap topLevelUses; // Variable -> nodes that use it

    /// Empty set for returning when no def nodes found
    static const ICFGNodeSet emptySet;

    /// Flag indicating if table has been built
    bool built;
};

} // End namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEDEFUSE_H_ */
