//===- LLVMLoopAnalysis.cpp -- LoopAnalysis of SVF ------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
 * LLVMLoopAnalysis.cpp
 *
 *  Created on: 14, 06, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 */

#include "SVF-FE/LLVMLoopAnalysis.h"
#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "llvm/Analysis/LoopInfo.h"

#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Passes/PassBuilder.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Build llvm loops based on LoopInfo analysis
 * @param mod SVF module
 * @param llvmLoops output llvm loops
 */
void LLVMLoopAnalysis::buildLLVMLoops(SVFModule *mod, std::vector<const Loop *> &llvmLoops)
{
    llvm::DominatorTree DT = llvm::DominatorTree();
    std::vector<const Loop *> loop_stack;
    for (const auto &svfFunc: *mod)
    {
        // do not analyze external call
        if (SVFUtil::isExtCall(svfFunc)) continue;
        llvm::Function *func = svfFunc->getLLVMFun();
        DT.recalculate(*func);
        auto loopInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
        loopInfo->releaseMemory();
        loopInfo->analyze(DT);
        for (const auto &loop: *loopInfo)
        {
            loop_stack.push_back(loop);
        }
        // pre-order traversal on loop-subloop tree
        while (!loop_stack.empty())
        {
            const Loop *loop = loop_stack.back();
            loop_stack.pop_back();
            llvmLoops.push_back(loop);
            for (const auto &subloop: loop->getSubLoops())
            {
                loop_stack.push_back(subloop);
            }
        }
    }
}

/*!
 * We start from here
 * @param icfg ICFG
 */
void LLVMLoopAnalysis::build(ICFG *icfg)
{
    std::vector<const Loop *> llvmLoops;
    buildLLVMLoops(PAG::getPAG()->getModule(), llvmLoops);
    buildSVFLoops(icfg, llvmLoops);
}

/*!
 * Build SVF loops based on llvm loops
 * @param icfg ICFG
 * @param llvmLoops input llvm loops
 */
void LLVMLoopAnalysis::buildSVFLoops(ICFG *icfg, std::vector<const Loop *> &llvmLoops)
{
    for (const auto &llvmLoop: llvmLoops)
    {
        DBOUT(DPAGBuild, outs() << "loop name: " << llvmLoop->getName().data() << "\n");
        // count all node id in loop
        Set<ICFGNode *> loop_ids;
        Set<const ICFGNode *> nodes;
        for (const auto &BB: llvmLoop->getBlocks())
        {
            for (const auto &ins: *BB)
            {
                loop_ids.insert(icfg->getICFGNode(&ins));
                nodes.insert(icfg->getICFGNode(&ins));
            }
        }
        SVFLoop *svf_loop = new SVFLoop(nodes, Options::LoopBound);
        for (const auto &node: nodes)
        {
            icfg->addNodeToSVFLoop(node, svf_loop);
        }
        // mark loop header's first inst
        llvm::BasicBlock *header_blk = llvmLoop->getHeader();
        llvm::Instruction &in_ins = *header_blk->begin();
        ICFGNode *in_node = icfg->getICFGNode(&in_ins);
        for (const auto &edge: in_node->getInEdges())
        {
            if (loop_ids.find(edge->getSrcNode()) == loop_ids.end())
            {
                // entry edge
                svf_loop->addEntryICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  entry edge: " << edge->toString() << "\n");
            }
            else
            {
                // back edge
                svf_loop->addBackICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  back edge: " << edge->toString() << "\n");
            }
        }
        // handle in edge
        llvm::Instruction &br_ins = header_blk->back();
        ICFGNode *br_node = icfg->getICFGNode(&br_ins);
        for (const auto &edge: br_node->getOutEdges())
        {
            if (loop_ids.find(edge->getDstNode()) != loop_ids.end())
            {
                svf_loop->addInICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  in edge: " << edge->toString() << "\n");
            }
            else
            {
                continue;
            }
        }
        // mark loop end's first inst
        llvm::SmallVector<BasicBlock *, 8> ExitBlocks;
        llvmLoop->getExitBlocks(ExitBlocks);
        for (const auto& exit_blk: ExitBlocks)
        {
            assert(!exit_blk->empty() && "exit block is empty?");
            llvm::Instruction &out_ins = *exit_blk->begin();
            ICFGNode *out_node = icfg->getICFGNode(&out_ins);
            for (const auto &edge: out_node->getInEdges())
            {
                svf_loop->addOutICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  out edge: " << edge->toString() << "\n");
            }
        }
    }
}