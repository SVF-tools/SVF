//
// Created by Jiawei Wang on 6/14/22.
//

#include "SVF-FE/LLVMLoopAnalysis.h"
#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "llvm/Analysis/LoopInfo.h"

#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Passes/PassBuilder.h"

using namespace SVF;
using namespace SVFUtil;

LLVMLoopAnalysis::LLVMLoopAnalysis() {
    black_lst = {
            "svf_assert", "bug_assert", "safe_assert", "isalnum", "isalpha",
            "isblank", "iscntrl", "isdigit", "isgraph", "islower", "isprint",
            "ispunct", "isspace", "isupper", "isxdigit", "strlen", "__strcpy_chk",
            "strcmp", "llvm.memset.p0i8.i64", "__memset_chk", "__memcpy_chk",
            "llvm.memcpy.p0i8.p0i8.i64", "llvm.memmove.p0i8.p0i8.i64",
            "scanf", "__isoc99_scanf", "nd", "llvm.objectsize.i64.p0i8",
            "nd_int", "malloc", "fscanf", "free"
    };
}

bool LLVMLoopAnalysis::buildLLVMLoops(SVFModule *mod) {
    llvm::DominatorTree DT = llvm::DominatorTree();
    std::vector<const Loop *> loop_stack;
    for (const auto &svfFunc: *mod) {
        if(SVFUtil::isExtCall(svfFunc)) continue;
        llvm::Function *func = svfFunc->getLLVMFun();
        if (black_lst.find(func->getName().str()) == black_lst.end()) {
            DT.recalculate(*func);
            auto loopInfo = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
            loopInfo->releaseMemory();
            loopInfo->analyze(DT);
            for (const auto &loop: *loopInfo) {
                loop_stack.push_back(loop);
            }
            while (!loop_stack.empty()) {
                const Loop *loop = loop_stack.back();
                llvmLoops.push_back(loop);
                for (const auto &subloop: loop->getSubLoops()) {
                    loop_stack.push_back(subloop);
                }
                loop_stack.pop_back();
            }
        } else {
            continue;
        }
    }
    return true;
}

void LLVMLoopAnalysis::build(SVFIR *svfir) {
    auto *module = svfir->getModule();
    auto *icfg = svfir->getICFG();
    buildLLVMLoops(module);
    buildSVFLoops(icfg);
}

void LLVMLoopAnalysis::buildSVFLoops(ICFG *icfg) {
    for (const auto &llvmLoop: llvmLoops) {
        DBOUT(DPAGBuild, outs() << "loop name: " << llvmLoop->getName().data() << "\n");
        // count all node id in loop
        Set<ICFGNode *> loop_ids;
        Set<const ICFGNode *> nodes;
        for (auto BB = llvmLoop->block_begin(); BB != llvmLoop->block_end(); ++BB) {
            for (auto ins = (*BB)->begin(); ins != (*BB)->end(); ++ins) {
                loop_ids.insert(icfg->getICFGNode(&*ins));
                nodes.insert(icfg->getICFGNode(&*ins));
            }
        }
        auto *svf_loop = new SVFLoop(nodes, 1);
        // TODO: temp set bound 1
        // mark loop header's first inst
        llvm::BasicBlock *header_blk = llvmLoop->getHeader();
        llvm::Instruction &in_ins = *header_blk->begin();
        ICFGNode *in_node = icfg->getICFGNode(&in_ins);
        for (auto edge_itr = in_node->InEdgeBegin(); edge_itr != in_node->InEdgeEnd(); ++edge_itr) {
            ICFGEdge *edge = *edge_itr;
            if (loop_ids.find(edge->getSrcNode()) == loop_ids.end()) {
                // entry edge
                svf_loop->addEntryICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  entry edge: " << edge->toString() << "\n");
            } else {
                // back edge
                svf_loop->addBackICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  back edge: " << edge->toString() << "\n");
            }
        }
        for (auto in_edge_itr = svf_loop->entryICFGEdgesBegin();
             in_edge_itr != svf_loop->entryICFGEdgesEnd(); ++in_edge_itr) {
            icfgEdgeToSVFLoop.insert({*in_edge_itr, svf_loop});
        }
        // handle in edge
        llvm::Instruction &br_ins = header_blk->back();
        ICFGNode *br_node = icfg->getICFGNode(&br_ins);
        for (auto edge_itr = br_node->OutEdgeBegin(); edge_itr != br_node->OutEdgeEnd(); ++edge_itr) {
            ICFGEdge *edge = *edge_itr;
            if (loop_ids.find(edge->getDstNode()) != loop_ids.end()) {
                svf_loop->addInICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  in edge: " << edge->toString() << "\n");
            } else {
                continue;
            }
        }
        // mark loop end's first inst
        llvm::SmallVector<BasicBlock *, 8> ExitBlocks;
        llvmLoop->getExitBlocks(ExitBlocks);
        for (llvm::BasicBlock *exit_blk: ExitBlocks) {
            llvm::Instruction &out_ins = *exit_blk->begin();
            ICFGNode *out_node = icfg->getICFGNode(&out_ins);
            for (auto edge_itr = out_node->InEdgeBegin(); edge_itr != out_node->InEdgeEnd(); ++edge_itr) {
                IntraCFGEdge *edge = SVFUtil::dyn_cast<IntraCFGEdge>(*edge_itr);
                svf_loop->addOutICFGEdge(edge);
                DBOUT(DPAGBuild, outs() << "  out edge: " << edge->toString() << "\n");
            }
        }
    }
}