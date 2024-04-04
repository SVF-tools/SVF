//===- svf-ex.cpp -- A driver example of SVF-------------------------------------//
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
//===-----------------------------------------------------------------------===//

/*
 // A driver program of SVF including usages of SVF APIs
 //
 // Author: Yulei Sui,
 */

#include "AE/Svfexe/SVFIR2AbsState.h"
#include "Graphs/SVFG.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

using namespace llvm;
using namespace std;
using namespace SVF;

/*!
 * An example to query alias results of two SVF values
 */
SVF::AliasResult aliasQuery(PointerAnalysis* pta, const SVFValue* v1, const SVFValue* v2)
{
    return pta->alias(v1, v2);
}

/*!
 * An example to print points-to set of an SVF value
 */
std::string printPts(PointerAnalysis* pta, const SVFValue* svfval)
{

    std::string str;
    raw_string_ostream rawstr(str);

    NodeID pNodeId = pta->getPAG()->getValueNode(svfval);
    const PointsTo& pts = pta->getPts(pNodeId);
    for (PointsTo::iterator ii = pts.begin(), ie = pts.end();
            ii != ie; ii++)
    {
        rawstr << " " << *ii << " ";
        PAGNode* targetObj = pta->getPAG()->getGNode(*ii);
        if(targetObj->hasValue())
        {
            rawstr << "(" << targetObj->getValue()->toString() << ")\t ";
        }
    }

    return rawstr.str();

}

/*!
 * An example to query/collect all SVFStmt from a ICFGNode (iNode)
 */
void traverseOnSVFStmt(const ICFGNode* node)
{
    SVFIR2AbsState* svfir2ExeState = new SVFIR2AbsState(SVFIR::getPAG());
    for (const SVFStmt* stmt: node->getSVFStmts())
    {
        if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            svfir2ExeState->translateAddr(addr);
        }
        else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            svfir2ExeState->translateBinary(binary);
        }
        else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            svfir2ExeState->translateCmp(cmp);
        }
        else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            svfir2ExeState->translateLoad(load);
        }
        else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            svfir2ExeState->translateStore(store);
        }
        else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            svfir2ExeState->translateCopy(copy);
        }
        else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            if (gep->isConstantOffset())
            {
                gep->accumulateConstantByteOffset();
                gep->accumulateConstantOffset();
            }
            svfir2ExeState->translateGep(gep);
        }
        else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            svfir2ExeState->translateSelect(select);
        }
        else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            svfir2ExeState->translatePhi(phi);
        }
        else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            // To handle Call Edge
            svfir2ExeState->translateCall(callPE);
        }
        else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            svfir2ExeState->translateRet(retPE);
        }
        else
            assert(false && "implement this part");
    }
}


/*!
 * An example to query/collect all successor nodes from a ICFGNode (iNode) along control-flow graph (ICFG)
 */
void traverseOnICFG(ICFG* icfg, const ICFGNode* iNode)
{
    FIFOWorkList<const ICFGNode*> worklist;
    Set<const ICFGNode*> visited;
    worklist.push(iNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const ICFGNode* vNode = worklist.pop();
        for (ICFGNode::const_iterator it = vNode->OutEdgeBegin(), eit =
                    vNode->OutEdgeEnd(); it != eit; ++it)
        {
            ICFGEdge* edge = *it;
            ICFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }
}

void dummyVisit(const VFGNode* node)
{

}
/*!
 * An example to query/collect all the uses of a definition of a value along value-flow graph (VFG)
 */
void traverseOnVFG(const SVFG* vfg, const SVFValue* svfval)
{
    SVFIR* pag = SVFIR::getPAG();
    PAGNode* pNode = pag->getGNode(pag->getValueNode(svfval));
    if (!vfg->hasDefSVFGNode(pNode))
        return;
    const VFGNode* vNode = vfg->getDefSVFGNode(pNode);
    FIFOWorkList<const VFGNode*> worklist;
    Set<const VFGNode*> visited;
    worklist.push(vNode);

    /// Traverse along VFG
    while (!worklist.empty())
    {
        const VFGNode* vNode = worklist.pop();
        for (VFGNode::const_iterator it = vNode->OutEdgeBegin(), eit =
                    vNode->OutEdgeEnd(); it != eit; ++it)
        {
            VFGEdge* edge = *it;
            VFGNode* succNode = edge->getDstNode();
            if (visited.find(succNode) == visited.end())
            {
                visited.insert(succNode);
                worklist.push(succNode);
            }
        }
    }

    /// Collect all LLVM Values
    for(Set<const VFGNode*>::const_iterator it = visited.begin(), eit = visited.end(); it!=eit; ++it)
    {
        const VFGNode* node = *it;
        dummyVisit(node);
        /// can only query VFGNode involving top-level pointers (starting with % or @ in LLVM IR)
        /// PAGNode* pNode = vfg->getLHSTopLevPtr(node);
        /// Value* val = pNode->getValue();
    }
}

int main(int argc, char ** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "Whole Program Points-to Analysis", "[options] <input-bitcode...>"
                    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);

    /// Build Program Assignment Graph (SVFIR)
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    /// Create Andersen's pointer analysis
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);


    /// Call Graph
    PTACallGraph* callgraph = ander->getPTACallGraph();

    /// ICFG
    ICFG* icfg = pag->getICFG();

    /// Value-Flow Graph (VFG)
    VFG* vfg = new VFG(callgraph);

    /// Sparse value-flow graph (SVFG)
    SVFGBuilder svfBuilder;
    SVFG* svfg = svfBuilder.buildFullSVFG(ander);

    /// Collect uses of an LLVM Value
    if (Options::PTSPrint())
    {
        for (const auto& it : *svfg)
        {
            const SVFGNode* node = it.second;
            if (node->getValue())
            {
                traverseOnVFG(svfg, node->getValue());
                /// Print points-to information
                printPts(ander, node->getValue());
                for (const SVFGEdge* edge : node->getOutEdges())
                {
                    const SVFGNode* node2 = edge->getDstNode();
                    if (node2->getValue())
                        aliasQuery(ander, node->getValue(), node2->getValue());
                }
            }
        }
    }

    /// Collect all successor nodes on ICFG
    if (Options::PTSPrint())
    {
        for (const auto& it : *icfg)
        {
            const ICFGNode* node = it.second;
            traverseOnICFG(icfg, node);
        }
    }

    // clean up memory
    delete vfg;
    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();

    LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
    llvm::llvm_shutdown();
    return 0;
}

