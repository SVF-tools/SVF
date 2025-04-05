//===- SVFIRBuilder.cpp -- SVFIR builder-----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SVFIRBuilder.cpp
 *
 *  Created on: Nov 1, 2013
 *      Author: Yulei Sui
 */

#include "SVF-LLVM/SVFIRBuilder.h"
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/CHGBuilder.h"
#include "SVF-LLVM/CppUtil.h"
#include "SVF-LLVM/LLVMLoopAnalysis.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SymbolTableBuilder.h"
#include "SVFIR/PAGBuilderFromFile.h"
#include "Util/CallGraphBuilder.h"
#include "Graphs/CallGraph.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;


/*!
 * Start building SVFIR here
 */
SVFIR* SVFIRBuilder::build()
{
    double startTime = SVFStat::getClk(true);

    DBOUT(DGENERAL, outs() << pasMsg("\t Building SVFIR ...\n"));

    // We read SVFIR from a user-defined txt instead of parsing SVFIR from LLVM IR
    if (SVFIR::pagReadFromTXT())
    {
        PAGBuilderFromFile fileBuilder(SVFIR::pagFileName());
        return fileBuilder.build();
    }

    // If the SVFIR has been built before, then we return the unique SVFIR of the program
    if(pag->getNodeNumAfterPAGBuild() > 1)
        return pag;


    createFunObjVars();

    /// build icfg
    ICFGBuilder icfgbuilder;
    pag->icfg = icfgbuilder.build();

    /// initial external library information
    /// initial SVFIR nodes
    initialiseNodes();
    /// initial SVFIR edges:
    ///// handle globals
    visitGlobal();
    ///// collect exception vals in the program



    /// build callgraph
    CallGraphBuilder callGraphBuilder;
    std::vector<const FunObjVar*> funset;
    for (const auto& item: llvmModuleSet()->getFunctionSet())
    {
        funset.push_back(llvmModuleSet()->getFunObjVar(item));
    }
    pag->callGraph = callGraphBuilder.buildSVFIRCallGraph(funset);

    CHGraph* chg = new CHGraph();
    CHGBuilder chgbuilder(chg);
    chgbuilder.buildCHG();
    pag->setCHG(chg);

    /// handle functions
    for (Module& M : llvmModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            const Function& fun = *F;
            const FunObjVar* svffun = llvmModuleSet()->getFunObjVar(&fun);
            /// collect return node of function fun
            if(!fun.isDeclaration())
            {
                /// Return SVFIR node will not be created for function which can not
                /// reach the return instruction due to call to abort(), exit(),
                /// etc. In 176.gcc of SPEC 2000, function build_objc_string() from
                /// c-lang.c shows an example when fun.doesNotReturn() evaluates
                /// to TRUE because of abort().
                if (fun.doesNotReturn() == false &&
                        fun.getReturnType()->isVoidTy() == false)
                {
                    pag->addFunRet(svffun,
                                   pag->getGNode(pag->getReturnNode(svffun)));
                }

                /// To be noted, we do not record arguments which are in declared function without body
                /// TODO: what about external functions with SVFIR imported by commandline?
                for (Function::const_arg_iterator I = fun.arg_begin(), E = fun.arg_end();
                        I != E; ++I)
                {
                    setCurrentLocation(&*I,&fun.getEntryBlock());
                    NodeID argValNodeId = llvmModuleSet()->getValueNode(&*I);
                    // if this is the function does not have caller (e.g. main)
                    // or a dead function, shall we create a black hole address edge for it?
                    // it is (1) too conservative, and (2) make FormalParmVFGNode defined at blackhole address PAGEdge.
                    // if(SVFUtil::ArgInNoCallerFunction(&*I)) {
                    //    if(I->getType()->isPointerTy())
                    //        addBlackHoleAddrEdge(argValNodeId);
                    //}
                    pag->addFunArgs(svffun,pag->getGNode(argValNodeId));
                }
            }
            for (Function::const_iterator bit = fun.begin(), ebit = fun.end();
                    bit != ebit; ++bit)
            {
                const BasicBlock& bb = *bit;
                for (BasicBlock::const_iterator it = bb.begin(), eit = bb.end();
                        it != eit; ++it)
                {
                    const Instruction& inst = *it;
                    setCurrentLocation(&inst,&bb);
                    visit(const_cast<Instruction&>(inst));
                }
            }
        }
    }

    sanityCheck();

    pag->initialiseCandidatePointers();

    pag->setNodeNumAfterPAGBuild(pag->getTotalNodeNum());

    // dump SVFIR
    if (Options::PAGDotGraph())
        pag->dump("svfir_initial");

    // print to command line of the SVFIR graph
    if (Options::PAGPrint())
        pag->print();

    // dump ICFG
    if (Options::DumpICFG())
        pag->getICFG()->dump("icfg_initial");

    if (Options::LoopAnalysis())
    {
        LLVMLoopAnalysis loopAnalysis;
        loopAnalysis.build(pag->getICFG());
    }

    // dump SVFIR as JSON
    if (!Options::DumpJson().empty())
    {
        assert(false && "please implement SVFIRWriter::writeJsonToPath");
    }

    double endTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingSVFIR = (endTime - startTime) / TIMEINTERVAL;

    return pag;
}

void SVFIRBuilder::initFunObjVar()
{
    for (Module& mod : llvmModuleSet()->getLLVMModules())
    {
        /// Function
        for (const Function& f : mod.functions())
        {
            FunObjVar* svffun = const_cast<FunObjVar*>(llvmModuleSet()->getFunObjVar(&f));
            initSVFBasicBlock(&f);

            if (!LLVMUtil::isExtCall(&f))
            {
                initDomTree(svffun, &f);
            }
            /// set realDefFun for all functions
            const Function *realfun = llvmModuleSet()->getRealDefFun(&f);
            svffun->setRelDefFun(realfun == nullptr ? nullptr : llvmModuleSet()->getFunObjVar(realfun));
        }
    }

    // Store annotations of functions in extapi.bc
    for (const auto& pair : llvmModuleSet()->ExtFun2Annotations)
    {
        ExtAPI::getExtAPI()->setExtFuncAnnotations(llvmModuleSet()->getFunObjVar(pair.first), pair.second);
    }

}

void SVFIRBuilder::initSVFBasicBlock(const Function* func)
{
    FunObjVar *svfFun = const_cast<FunObjVar *>(llvmModuleSet()->getFunObjVar(func));
    for (Function::const_iterator bit = func->begin(), ebit = func->end(); bit != ebit; ++bit)
    {
        const BasicBlock* bb = &*bit;
        SVFBasicBlock* svfbb = llvmModuleSet()->getSVFBasicBlock(bb);
        for (succ_const_iterator succ_it = succ_begin(bb); succ_it != succ_end(bb); succ_it++)
        {
            const SVFBasicBlock* svf_scc_bb = llvmModuleSet()->getSVFBasicBlock(*succ_it);
            svfbb->addSuccBasicBlock(svf_scc_bb);
        }
        for (const_pred_iterator pred_it = pred_begin(bb); pred_it != pred_end(bb); pred_it++)
        {
            const SVFBasicBlock* svf_pred_bb = llvmModuleSet()->getSVFBasicBlock(*pred_it);
            svfbb->addPredBasicBlock(svf_pred_bb);
        }

        /// set exit block: exit basic block must have no successors and have a return instruction
        if (svfbb->getSuccessors().empty())
        {
            if (LLVMUtil::basicBlockHasRetInst(bb))
            {
                assert((LLVMUtil::functionDoesNotRet(func) ||
                        SVFUtil::isa<ReturnInst>(bb->back())) &&
                       "last inst must be return inst");
                svfFun->setExitBlock(svfbb);
            }
        }
    }
    // For no return functions, we set the last block as exit BB
    // This ensures that each function that has definition must have an exit BB
    if (svfFun->hasBasicBlock() && svfFun->exitBlock == nullptr)
    {
        SVFBasicBlock* retBB = const_cast<SVFBasicBlock*>(svfFun->back());
        assert((LLVMUtil::functionDoesNotRet(func) ||
                SVFUtil::isa<ReturnInst>(&func->back().back())) &&
               "last inst must be return inst");
        svfFun->setExitBlock(retBB);
    }
}


void SVFIRBuilder::initDomTree(FunObjVar* svffun, const Function* fun)
{
    if (fun->isDeclaration())
        return;
    //process and stored dt & df
    DominanceFrontier df;
    DominatorTree& dt = llvmModuleSet()->getDomTree(fun);
    df.analyze(dt);
    LoopInfo loopInfo = LoopInfo(dt);
    PostDominatorTree pdt = PostDominatorTree(const_cast<Function&>(*fun));
    SVFLoopAndDomInfo* ld = svffun->getLoopAndDomInfo();

    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> & dfBBsMap = ld->getDomFrontierMap();
    for (DominanceFrontierBase::const_iterator dfIter = df.begin(), eDfIter = df.end(); dfIter != eDfIter; dfIter++)
    {
        const BasicBlock* keyBB = dfIter->first;
        const std::set<BasicBlock* >& domSet = dfIter->second;
        Set<const SVFBasicBlock*>& valueBasicBlocks = dfBBsMap[llvmModuleSet()->getSVFBasicBlock(keyBB)];
        for (const BasicBlock* bbValue:domSet)
        {
            valueBasicBlocks.insert(llvmModuleSet()->getSVFBasicBlock(bbValue));
        }
    }
    std::vector<const SVFBasicBlock*> reachableBBs;
    LLVMUtil::getFunReachableBBs(fun, reachableBBs);
    ld->setReachableBBs(reachableBBs);

    for (Function::const_iterator bit = fun->begin(), beit = fun->end(); bit!=beit; ++bit)
    {
        const BasicBlock &bb = *bit;
        SVFBasicBlock* svfBB = llvmModuleSet()->getSVFBasicBlock(&bb);
        if (DomTreeNode* dtNode = dt.getNode(&bb))
        {
            SVFLoopAndDomInfo::BBSet& bbSet = ld->getDomTreeMap()[svfBB];
            for (const auto domBB : *dtNode)
            {
                const auto* domSVFBB = llvmModuleSet()->getSVFBasicBlock(domBB->getBlock());
                bbSet.insert(domSVFBB);
            }
        }

        if (DomTreeNode* pdtNode = pdt.getNode(&bb))
        {
            u32_t level = pdtNode->getLevel();
            ld->getBBPDomLevel()[svfBB] = level;
            BasicBlock* idomBB = pdtNode->getIDom()->getBlock();
            const SVFBasicBlock* idom = idomBB == NULL ? NULL: llvmModuleSet()->getSVFBasicBlock(idomBB);
            ld->getBB2PIdom()[svfBB] = idom;

            SVFLoopAndDomInfo::BBSet& bbSet = ld->getPostDomTreeMap()[svfBB];
            for (const auto domBB : *pdtNode)
            {
                const auto* domSVFBB = llvmModuleSet()->getSVFBasicBlock(domBB->getBlock());
                bbSet.insert(domSVFBB);
            }
        }

        if (const Loop* loop = loopInfo.getLoopFor(&bb))
        {
            for (const BasicBlock* loopBlock : loop->getBlocks())
            {
                const SVFBasicBlock* loopbb = llvmModuleSet()->getSVFBasicBlock(loopBlock);
                ld->addToBB2LoopMap(svfBB, loopbb);
            }
        }
    }
}

void SVFIRBuilder::createFunObjVars()
{
    std::vector<FunObjVar*> funset;
    // Iterate over all object symbols in the symbol table
    for (const auto* fun: llvmModuleSet()->getFunctionSet())
    {
        u32_t id = llvmModuleSet()->objSyms()[fun];
        // Debug output for adding object node
        DBOUT(DPAGBuild, outs() << "add obj node " << id << "\n");

        // Check if the value is a function and add a function object node
        pag->addFunObjNode(id, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(fun->getType()), nullptr);
        llvmModuleSet()->LLVMFun2FunObjVar[fun] = cast<FunObjVar>(pag->getGNode(id));

        FunObjVar *funObjVar = SVFUtil::cast<FunObjVar>(pag->getGNode(id));
        funset.push_back(funObjVar);

        funObjVar->initFunObjVar(fun->isDeclaration(), LLVMUtil::isIntrinsicFun(fun), fun->hasAddressTaken(),
                                 LLVMUtil::isUncalledFunction(fun), LLVMUtil::functionDoesNotRet(fun), fun->isVarArg(),
                                 SVFUtil::cast<SVFFunctionType>(llvmModuleSet()->getSVFType(fun->getFunctionType())),
                                 new SVFLoopAndDomInfo, nullptr, nullptr,
                                 {}, nullptr);
        BasicBlockGraph* bbGraph = new BasicBlockGraph();
        funObjVar->setBasicBlockGraph(bbGraph);


        for (const BasicBlock& bb : *fun)
        {
            llvmModuleSet()->addBasicBlock(funObjVar, &bb);
        }

        /// set fun in bb
        for (auto& bb: *funObjVar->bbGraph)
        {
            bb.second->setFun(funObjVar);
        }
        llvmModuleSet()->addToSVFVar2LLVMValueMap(fun, pag->getGNode(id));
    }

    initFunObjVar();
}

void SVFIRBuilder::initialiseBaseObjVars()
{
    // Iterate over all object symbols in the symbol table
    for (LLVMModuleSet::ValueToIDMapTy::iterator iter =
                llvmModuleSet()->objSyms().begin(); iter != llvmModuleSet()->objSyms().end();
            ++iter)
    {
        // Debug output for adding object node
        DBOUT(DPAGBuild, outs() << "add obj node " << iter->second << "\n");

        // Skip blackhole and constant symbols
        if(iter->second == pag->blackholeSymID() || iter->second == pag->constantSymID())
            continue;

        // Get the LLVM value corresponding to the symbol
        const Value* llvmValue = iter->first;

        const ICFGNode* icfgNode = nullptr;
        if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(llvmValue))
        {
            if(llvmModuleSet()->hasICFGNode(inst))
                icfgNode = llvmModuleSet()->getICFGNode(inst);
        }

        // Check if the value is a function and add a function object node
        if (SVFUtil::dyn_cast<Function>(llvmValue))
        {
            // already one
        }
        // Check if the value is a heap object and add a heap object node
        else if (LLVMUtil::isHeapObj(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addHeapObjNode(iter->second, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        // Check if the value is an alloca instruction and add a stack object node
        else if (LLVMUtil::isStackObj(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addStackObjNode(iter->second, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (auto fpValue = SVFUtil::dyn_cast<ConstantFP>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addConstantFPObjNode(iter->second, pag->getObjTypeInfo(id),  LLVMUtil::getDoubleValue(fpValue), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (auto intValue = SVFUtil::dyn_cast<ConstantInt>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addConstantIntObjNode(iter->second, pag->getObjTypeInfo(id), LLVMUtil::getIntegerValue(intValue), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (SVFUtil::isa<ConstantPointerNull>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addConstantNullPtrObjNode(iter->second, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (SVFUtil::isa<GlobalValue>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addGlobalObjNode(iter->second,
                                  pag->getObjTypeInfo(id),
                                  llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (SVFUtil::isa<ConstantData, MetadataAsValue, BlockAddress>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addConstantDataObjNode(iter->second, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        else if (SVFUtil::isa<ConstantAggregate>(llvmValue))
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addConstantAggObjNode(iter->second, pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        // Add a generic object node for other types of values
        else
        {
            NodeID id = llvmModuleSet()->getObjectNode(iter->first);
            pag->addObjNode(iter->second,
                            pag->getObjTypeInfo(id), llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        llvmModuleSet()->addToSVFVar2LLVMValueMap(llvmValue, pag->getGNode(iter->second));
    }

}

void SVFIRBuilder::initialiseValVars()
{
    // Iterate over all value symbols in the symbol table
    for (LLVMModuleSet::ValueToIDMapTy::iterator iter =
                llvmModuleSet()->valSyms().begin(); iter != llvmModuleSet()->valSyms().end();
            ++iter)
    {
        // Debug output for adding value node
        DBOUT(DPAGBuild, outs() << "add val node " << iter->second << "\n");

        // Skip blackhole and null pointer symbols
        if(iter->second == pag->blkPtrSymID() || iter->second == pag->nullPtrSymID())
            continue;

        const ICFGNode* icfgNode = nullptr;
        auto llvmValue = iter->first;
        if (const Instruction* inst =
                    SVFUtil::dyn_cast<Instruction>(llvmValue))
        {
            if (llvmModuleSet()->hasICFGNode(inst))
            {
                icfgNode = llvmModuleSet()->getICFGNode(inst);
            }
        }

        // Check if the value is a function and get its call graph node
        if (const Function* func = SVFUtil::dyn_cast<Function>(llvmValue))
        {
            // add value node representing the function
            pag->addFunValNode(iter->second, icfgNode, llvmModuleSet()->getFunObjVar(func), llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (auto argval = SVFUtil::dyn_cast<Argument>(llvmValue))
        {
            pag->addArgValNode(
                iter->second, argval->getArgNo(), icfgNode,
                llvmModuleSet()->getFunObjVar(argval->getParent()),llvmModuleSet()->getSVFType(llvmValue->getType()));
            if (!argval->hasName())
                pag->getGNode(iter->second)->setName("arg_" + std::to_string(argval->getArgNo()));
        }
        else if (auto fpValue = SVFUtil::dyn_cast<ConstantFP>(llvmValue))
        {
            pag->addConstantFPValNode(iter->second, LLVMUtil::getDoubleValue(fpValue), icfgNode, llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (auto intValue = SVFUtil::dyn_cast<ConstantInt>(llvmValue))
        {
            pag->addConstantIntValNode(iter->second, LLVMUtil::getIntegerValue(intValue), icfgNode, llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (SVFUtil::isa<ConstantPointerNull>(llvmValue))
        {
            pag->addConstantNullPtrValNode(iter->second, icfgNode, llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (SVFUtil::isa<GlobalValue>(llvmValue))
        {
            pag->addGlobalValNode(iter->second, icfgNode,
                                  llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (SVFUtil::isa<ConstantData, MetadataAsValue, BlockAddress>(llvmValue))
        {
            pag->addConstantDataValNode(iter->second, icfgNode, llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else if (SVFUtil::isa<ConstantAggregate>(llvmValue))
        {
            pag->addConstantAggValNode(iter->second, icfgNode, llvmModuleSet()->getSVFType(llvmValue->getType()));
        }
        else
        {
            // Add value node to PAG
            pag->addValNode(iter->second, llvmModuleSet()->getSVFType(llvmValue->getType()), icfgNode);
        }
        llvmModuleSet()->addToSVFVar2LLVMValueMap(llvmValue,
                pag->getGNode(iter->second));
    }
}


/*
 * Initial all the nodes from symbol table
 */
void SVFIRBuilder::initialiseNodes()
{
    DBOUT(DPAGBuild, outs() << "Initialise SVFIR Nodes ...\n");


    pag->addBlackholeObjNode();
    pag->addConstantObjNode();
    pag->addBlackholePtrNode();
    addNullPtrNode();

    initialiseBaseObjVars();
    initialiseValVars();

    for (LLVMModuleSet::FunToIDMapTy::iterator iter =
                llvmModuleSet()->retSyms().begin(); iter != llvmModuleSet()->retSyms().end();
            ++iter)
    {
        const Value* llvmValue = iter->first;
        const ICFGNode* icfgNode = nullptr;
        if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(llvmValue))
        {
            if(llvmModuleSet()->hasICFGNode(inst))
                icfgNode = llvmModuleSet()->getICFGNode(inst);
        }
        DBOUT(DPAGBuild, outs() << "add ret node " << iter->second << "\n");
        pag->addRetNode(iter->second,
                        llvmModuleSet()->getFunObjVar(SVFUtil::cast<Function>(llvmValue)),
                        llvmModuleSet()->getSVFType(iter->first->getType()), icfgNode);
        llvmModuleSet()->addToSVFVar2LLVMValueMap(llvmValue, pag->getGNode(iter->second));
        const FunObjVar* funObjVar = llvmModuleSet()->getFunObjVar(SVFUtil::cast<Function>(llvmValue));
        pag->returnFunObjSymMap[funObjVar] = iter->second;
    }

    for (LLVMModuleSet::FunToIDMapTy::iterator iter =
                llvmModuleSet()->varargSyms().begin();
            iter != llvmModuleSet()->varargSyms().end(); ++iter)
    {
        const Value* llvmValue = iter->first;

        const ICFGNode *icfgNode = nullptr;
        if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(llvmValue))
        {
            if (llvmModuleSet()->hasICFGNode(inst))
                icfgNode = llvmModuleSet()->getICFGNode(inst);
        }
        DBOUT(DPAGBuild, outs() << "add vararg node " << iter->second << "\n");
        pag->addVarargNode(iter->second,
                           llvmModuleSet()->getFunObjVar(SVFUtil::cast<Function>(llvmValue)),
                           llvmModuleSet()->getSVFType(iter->first->getType()), icfgNode);
        llvmModuleSet()->addToSVFVar2LLVMValueMap(llvmValue, pag->getGNode(iter->second));
        const FunObjVar* funObjVar = llvmModuleSet()->getFunObjVar(SVFUtil::cast<Function>(llvmValue));
        pag->varargFunObjSymMap[funObjVar] = iter->second;
    }

    /// add address edges for constant nodes.
    for (LLVMModuleSet::ValueToIDMapTy::iterator iter =
                llvmModuleSet()->objSyms().begin(); iter != llvmModuleSet()->objSyms().end(); ++iter)
    {
        DBOUT(DPAGBuild, outs() << "add address edges for constant node " << iter->second << "\n");
        const Value* val = iter->first;
        if (isConstantObjSym(val))
        {
            NodeID ptr = llvmModuleSet()->getValueNode(val);
            if(ptr!= pag->getBlkPtr() && ptr!= pag->getNullPtr())
            {
                setCurrentLocation(val, (SVFBasicBlock*) nullptr);
                addAddrEdge(iter->second, ptr);
            }
        }
    }

    assert(pag->getTotalNodeNum() >= pag->getTotalSymNum()
           && "not all node have been initialized!!!");

    /// add argvalvar for svffunctions
    for (auto& fun: llvmModuleSet()->getFunctionSet())
    {
        for (const Argument& arg : fun->args())
        {
            const_cast<FunObjVar*>(llvmModuleSet()->getFunObjVar(fun))->addArgument(SVFUtil::cast<ArgValVar>(
                        pag->getGNode(llvmModuleSet()->getValueNode(&arg))));
        }
    }

}

/*
    https://github.com/SVF-tools/SVF/issues/524
    Handling single value types, for constant index, including pointer, integer, etc
    e.g. field_idx = getelementptr i8, %i8* %p, i64 -4
    We can obtain the field index by inferring the byteoffset if %p is casted from a pointer to a struct
    For another example, the following can be an array access.
    e.g. field_idx = getelementptr i8, %struct_type %p, i64 1

*/
u32_t SVFIRBuilder::inferFieldIdxFromByteOffset(const llvm::GEPOperator* gepOp, DataLayout *dl, AccessPath& ap, APOffset idx)
{
    return 0;
}

/*!
 * Return the object node offset according to GEP insn (V).
 * Given a gep edge p = q + i, if "i" is a constant then we return its offset size
 * otherwise if "i" is a variable determined by runtime, then it is a variant offset
 * Return TRUE if the offset of this GEP insn is a constant.
 */
bool SVFIRBuilder::computeGepOffset(const User *V, AccessPath& ap)
{
    assert(V);

    const llvm::GEPOperator *gepOp = SVFUtil::dyn_cast<const llvm::GEPOperator>(V);
    DataLayout * dataLayout = getDataLayout(llvmModuleSet()->getMainLLVMModule());
    llvm::APInt byteOffset(dataLayout->getIndexSizeInBits(gepOp->getPointerAddressSpace()),0,true);
    if(gepOp && dataLayout && gepOp->accumulateConstantOffset(*dataLayout,byteOffset))
    {
        //s32_t bo = byteOffset.getSExtValue();
    }

    bool isConst = true;

    bool prevPtrOperand = false;
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi)
    {
        const Type* gepTy = *gi;
        const SVFType* svfGepTy = llvmModuleSet()->getSVFType(gepTy);

        assert((prevPtrOperand && svfGepTy->isPointerTy()) == false &&
               "Expect no more than one gep operand to be of a pointer type");
        if(!prevPtrOperand && svfGepTy->isPointerTy()) prevPtrOperand = true;
        const Value* offsetVal = gi.getOperand();
        assert(gepTy != offsetVal->getType() && "iteration and operand have the same type?");
        ap.addOffsetVarAndGepTypePair(getPAG()->getGNode(llvmModuleSet()->getValueNode(offsetVal)), svfGepTy);

        //The int value of the current index operand
        const ConstantInt* op = SVFUtil::dyn_cast<ConstantInt>(offsetVal);

        // if Options::ModelConsts() is disabled. We will treat whole array as one,
        // but we can distinguish different field of an array of struct, e.g. s[1].f1 is different from s[0].f2
        if(const ArrayType* arrTy = SVFUtil::dyn_cast<ArrayType>(gepTy))
        {
            if(!op || (arrTy->getArrayNumElements() <= (u32_t)LLVMUtil::getIntegerValue(op).first))
                continue;
            APOffset idx = (u32_t)LLVMUtil::getIntegerValue(op).first;
            u32_t offset = pag->getFlattenedElemIdx(llvmModuleSet()->getSVFType(arrTy), idx);
            ap.setFldIdx(ap.getConstantStructFldIdx() + offset);
        }
        else if (const StructType *ST = SVFUtil::dyn_cast<StructType>(gepTy))
        {
            assert(op && "non-const offset accessing a struct");
            //The actual index
            APOffset idx = (u32_t)LLVMUtil::getIntegerValue(op).first;
            u32_t offset = pag->getFlattenedElemIdx(llvmModuleSet()->getSVFType(ST), idx);
            ap.setFldIdx(ap.getConstantStructFldIdx() + offset);
        }
        else if (gepTy->isSingleValueType())
        {
            // If it's a non-constant offset access
            // If its point-to target is struct or array, it's likely an array accessing (%result = gep %struct.A* %a, i32 %non-const-index)
            // If its point-to target is single value (pointer arithmetic), then it's a variant gep (%result = gep i8* %p, i32 %non-const-index)
            if(!op && gepTy->isPointerTy() && gepOp->getSourceElementType()->isSingleValueType())
            {
                isConst = false;
            }

            // The actual index
            //s32_t idx = op->getSExtValue();

            // For pointer arithmetic we ignore the byte offset
            // consider using inferFieldIdxFromByteOffset(geopOp,dataLayout,ap,idx)?
            // ap.setFldIdx(ap.getConstantFieldIdx() + inferFieldIdxFromByteOffset(geopOp,idx));
        }
    }
    return isConst;
}

/*!
 * Handle constant expression, and connect the gep edge
 */
void SVFIRBuilder::processCE(const Value* val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* gepce = isGepConstantExpr(ref))
        {
            DBOUT(DPAGBuild, outs() << "handle gep constant expression " << llvmModuleSet()->getSVFValue(ref)->toString() << "\n");
            const Constant* opnd = gepce->getOperand(0);
            // handle recursive constant express case (gep (bitcast (gep X 1)) 1)
            processCE(opnd);
            auto &GEPOp = llvm::cast<llvm::GEPOperator>(*gepce);
            Type *pType = GEPOp.getSourceElementType();
            AccessPath ap(0, llvmModuleSet()->getSVFType(pType));
            bool constGep = computeGepOffset(gepce, ap);
            // must invoke pag methods here, otherwise it will be a dead recursion cycle
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(gepce, (SVFBasicBlock*) nullptr);
            /*
             * The gep edge created are like constexpr (same edge may appear at multiple callsites)
             * so bb/inst of this edge may be rewritten several times, we treat it as global here.
             */
            addGepEdge(llvmModuleSet()->getValueNode(opnd), llvmModuleSet()->getValueNode(gepce), ap, constGep);
            setCurrentLocation(cval, cbb);
        }
        else if (const ConstantExpr* castce = isCastConstantExpr(ref))
        {
            DBOUT(DPAGBuild, outs() << "handle cast constant expression " << llvmModuleSet()->getSVFValue(ref)->toString() << "\n");
            const Constant* opnd = castce->getOperand(0);
            processCE(opnd);
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(castce, (SVFBasicBlock*) nullptr);
            addCopyEdge(llvmModuleSet()->getValueNode(opnd), llvmModuleSet()->getValueNode(castce), CopyStmt::BITCAST);
            setCurrentLocation(cval, cbb);
        }
        else if (const ConstantExpr* selectce = isSelectConstantExpr(ref))
        {
            DBOUT(DPAGBuild, outs() << "handle select constant expression " << llvmModuleSet()->getSVFValue(ref)->toString() << "\n");
            const Constant* src1 = selectce->getOperand(1);
            const Constant* src2 = selectce->getOperand(2);
            processCE(src1);
            processCE(src2);
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(selectce, (SVFBasicBlock*) nullptr);
            NodeID cond = llvmModuleSet()->getValueNode(selectce->getOperand(0));
            NodeID nsrc1 = llvmModuleSet()->getValueNode(src1);
            NodeID nsrc2 = llvmModuleSet()->getValueNode(src2);
            NodeID nres = llvmModuleSet()->getValueNode(selectce);
            addSelectStmt(nres,nsrc1, nsrc2, cond);
            setCurrentLocation(cval, cbb);
        }
        // if we meet a int2ptr, then it points-to black hole
        else if (const ConstantExpr* int2Ptrce = isInt2PtrConstantExpr(ref))
        {
            const Constant* opnd = int2Ptrce->getOperand(0);
            processCE(opnd);
            const SVFBasicBlock* cbb = getCurrentBB();
            const Value* cval = getCurrentValue();
            setCurrentLocation(int2Ptrce, (SVFBasicBlock*) nullptr);
            addCopyEdge(llvmModuleSet()->getValueNode(opnd), llvmModuleSet()->getValueNode(int2Ptrce), CopyStmt::INTTOPTR);
            setCurrentLocation(cval, cbb);
        }
        else if (const ConstantExpr* ptr2Intce = isPtr2IntConstantExpr(ref))
        {
            const Constant* opnd = ptr2Intce->getOperand(0);
            processCE(opnd);
            const SVFBasicBlock* cbb = getCurrentBB();
            const Value* cval = getCurrentValue();
            setCurrentLocation(ptr2Intce, (SVFBasicBlock*) nullptr);
            addCopyEdge(llvmModuleSet()->getValueNode(opnd), llvmModuleSet()->getValueNode(ptr2Intce), CopyStmt::PTRTOINT);
            setCurrentLocation(cval, cbb);
        }
        else if(isTruncConstantExpr(ref) || isCmpConstantExpr(ref))
        {
            // we don't handle trunc and cmp instruction for now
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, (SVFBasicBlock*) nullptr);
            NodeID dst = llvmModuleSet()->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (isBinaryConstantExpr(ref))
        {
            // we don't handle binary constant expression like add(x,y) now
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, (SVFBasicBlock*) nullptr);
            NodeID dst = llvmModuleSet()->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (isUnaryConstantExpr(ref))
        {
            // we don't handle unary constant expression like fneg(x) now
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, (SVFBasicBlock*) nullptr);
            NodeID dst = llvmModuleSet()->getValueNode(ref);
            addBlackHoleAddrEdge(dst);
            setCurrentLocation(cval, cbb);
        }
        else if (SVFUtil::isa<ConstantAggregate>(ref))
        {
            // we don't handle constant aggregate like constant vectors
        }
        else if (SVFUtil::isa<BlockAddress>(ref))
        {
            // blockaddress instruction (e.g. i8* blockaddress(@run_vm, %182))
            // is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and SVFIRBuilder.cpp:183-194
            const Value* cval = getCurrentValue();
            const SVFBasicBlock* cbb = getCurrentBB();
            setCurrentLocation(ref, (SVFBasicBlock*) nullptr);
            NodeID dst = llvmModuleSet()->getValueNode(ref);
            addAddrEdge(pag->getConstantNode(), dst);
            setCurrentLocation(cval, cbb);
        }
        else
        {
            if(SVFUtil::isa<ConstantExpr>(val))
                assert(false && "we don't handle all other constant expression for now!");
        }
    }
}
/*!
 * Get the field of the global variable node
 * FIXME:Here we only get the field that actually used in the program
 * We ignore the initialization of global variable field that not used in the program
 */
NodeID SVFIRBuilder::getGlobalVarField(const GlobalVariable *gvar, u32_t offset, SVFType* tpy)
{

    // if the global variable do not have any field needs to be initialized
    if (offset == 0 && gvar->getInitializer()->getType()->isSingleValueType())
    {
        return getValueNode(gvar);
    }
    /// if we did not find the constant expression in the program,
    /// then we need to create a gep node for this field
    else
    {
        return getGepValVar(gvar, AccessPath(offset), tpy);
    }
}

/*For global variable initialization
 * Give a simple global variable
 * int x = 10;     // store 10 x  (constant, non pointer)                                      |
 * int *y = &x;    // store x y   (pointer type)
 * Given a struct
 * struct Z { int s; int *t;};
 * Global initialization:
 * struct Z z = {10,&x}; // store x z.t  (struct type)
 * struct Z *m = &z;       // store z m  (pointer type)
 * struct Z n = {10,&z.s}; // store z.s n ,  &z.s constant expression (constant expression)
 */
void SVFIRBuilder::InitialGlobal(const GlobalVariable *gvar, Constant *C,
                                 u32_t offset)
{
    DBOUT(DPAGBuild, outs() << "global " << llvmModuleSet()->getSVFValue(gvar)->toString() << " constant initializer: " << llvmModuleSet()->getSVFValue(C)->toString() << "\n");
    if (C->getType()->isSingleValueType())
    {
        NodeID src = getValueNode(C);
        // get the field value if it is available, otherwise we create a dummy field node.
        setCurrentLocation(gvar, (SVFBasicBlock*) nullptr);
        NodeID field = getGlobalVarField(gvar, offset, llvmModuleSet()->getSVFType(C->getType()));

        if (SVFUtil::isa<GlobalVariable, Function>(C))
        {
            setCurrentLocation(C, (SVFBasicBlock*) nullptr);
            addStoreEdge(src, field);
        }
        else if (SVFUtil::isa<ConstantExpr>(C))
        {
            // add gep edge of C1 itself is a constant expression
            processCE(C);
            setCurrentLocation(C, (SVFBasicBlock*) nullptr);
            addStoreEdge(src, field);
        }
        else if (SVFUtil::isa<BlockAddress>(C))
        {
            // blockaddress instruction (e.g. i8* blockaddress(@run_vm, %182))
            // is treated as constant data object for now, see LLVMUtil.h:397, SymbolTableInfo.cpp:674 and SVFIRBuilder.cpp:183-194
            processCE(C);
            setCurrentLocation(C, (SVFBasicBlock*) nullptr);
            addAddrEdge(pag->getConstantNode(), src);
        }
        else
        {
            setCurrentLocation(C, (SVFBasicBlock*) nullptr);
            addStoreEdge(src, field);
            /// src should not point to anything yet
            if (C->getType()->isPtrOrPtrVectorTy() && src != pag->getNullPtr())
                addCopyEdge(pag->getNullPtr(), src, CopyStmt::COPYVAL);
        }
    }
    else if (SVFUtil::isa<ConstantArray, ConstantStruct>(C))
    {
        if(cppUtil::isValVtbl(gvar) && !Options::VtableInSVFIR())
            return;
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            u32_t off = pag->getFlattenedElemIdx(llvmModuleSet()->getSVFType(C->getType()), i);
            InitialGlobal(gvar, SVFUtil::cast<Constant>(C->getOperand(i)), offset + off);
        }
    }
    else if(ConstantData* data = SVFUtil::dyn_cast<ConstantData>(C))
    {
        if(Options::ModelConsts())
        {
            if(ConstantDataSequential* seq = SVFUtil::dyn_cast<ConstantDataSequential>(data))
            {
                for(u32_t i = 0; i < seq->getNumElements(); i++)
                {
                    u32_t off = pag->getFlattenedElemIdx(llvmModuleSet()->getSVFType(C->getType()), i);
                    Constant* ct = seq->getElementAsConstant(i);
                    InitialGlobal(gvar, ct, offset + off);
                }
            }
            else
            {
                assert((SVFUtil::isa<ConstantAggregateZero, UndefValue>(data)) && "Single value type data should have been handled!");
            }
        }
    }
    else
    {
        //TODO:assert(SVFUtil::isa<ConstantVector>(C),"what else do we have");
    }
}

/*!
 *  Visit global variables for building SVFIR
 */
void SVFIRBuilder::visitGlobal()
{

    /// initialize global variable
    for (Module &M : llvmModuleSet()->getLLVMModules())
    {
        for (Module::global_iterator I = M.global_begin(), E = M.global_end(); I != E; ++I)
        {
            GlobalVariable *gvar = &*I;
            NodeID idx = getValueNode(gvar);
            NodeID obj = getObjectNode(gvar);

            setCurrentLocation(gvar, (SVFBasicBlock*) nullptr);
            addAddrEdge(obj, idx);

            if (gvar->hasInitializer())
            {
                Constant *C = gvar->getInitializer();
                DBOUT(DPAGBuild, outs() << "add global var node " << llvmModuleSet()->getSVFValue(gvar)->toString() << "\n");
                InitialGlobal(gvar, C, 0);
            }
        }


        /// initialize global functions
        for (Module::const_iterator I = M.begin(), E = M.end(); I != E; ++I)
        {
            const Function* fun = &*I;
            NodeID idx = getValueNode(fun);
            NodeID obj = getObjectNode(fun);

            DBOUT(DPAGBuild, outs() << "add global function node " << fun->getName().str() << "\n");
            setCurrentLocation(fun, (SVFBasicBlock*) nullptr);
            addAddrEdge(obj, idx);
        }

        // Handle global aliases (due to linkage of multiple bc files), e.g., @x = internal alias @y. We need to add a copy from y to x.
        for (Module::alias_iterator I = M.alias_begin(), E = M.alias_end(); I != E; I++)
        {
            const GlobalAlias* alias = &*I;
            NodeID dst = llvmModuleSet()->getValueNode(alias);
            NodeID src = llvmModuleSet()->getValueNode(alias->getAliasee());
            processCE(alias->getAliasee());
            setCurrentLocation(alias, (SVFBasicBlock*) nullptr);
            addCopyEdge(src, dst, CopyStmt::COPYVAL);
        }
    }
}

/*!
 * Visit alloca instructions
 * Add edge V (dst) <-- O (src), V here is a value node on SVFIR, O is object node on SVFIR
 */
void SVFIRBuilder::visitAllocaInst(AllocaInst &inst)
{

    // AllocaInst should always be a pointer type
    assert(SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process alloca  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");
    NodeID dst = getValueNode(&inst);

    NodeID src = getObjectNode(&inst);

    addAddrWithStackArraySz(src, dst, inst);

}

/*!
 * Visit phi instructions
 */
void SVFIRBuilder::visitPHINode(PHINode &inst)
{

    DBOUT(DPAGBuild, outs() << "process phi " << llvmModuleSet()->getSVFValue(&inst)->toString() << "  \n");

    NodeID dst = getValueNode(&inst);

    for (u32_t i = 0; i < inst.getNumIncomingValues(); ++i)
    {
        const Value* val = inst.getIncomingValue(i);
        const Instruction* incomingInst = SVFUtil::dyn_cast<Instruction>(val);
        bool matched = (incomingInst == nullptr ||
                        incomingInst->getFunction() == inst.getFunction());
        (void) matched; // Suppress warning of unused variable under release build
        assert(matched && "incomingInst's Function incorrect");
        const Instruction* predInst = &inst.getIncomingBlock(i)->back();
        const ICFGNode* icfgNode = llvmModuleSet()->getICFGNode(predInst);
        NodeID src = getValueNode(val);
        addPhiStmt(dst,src,icfgNode);
    }
}

/*
 * Visit load instructions
 */
void SVFIRBuilder::visitLoadInst(LoadInst &inst)
{
    DBOUT(DPAGBuild, outs() << "process load  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");

    NodeID dst = getValueNode(&inst);

    NodeID src = getValueNode(inst.getPointerOperand());

    addLoadEdge(src, dst);
}

/*!
 * Visit store instructions
 */
void SVFIRBuilder::visitStoreInst(StoreInst &inst)
{
    // StoreInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process store " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");

    NodeID dst = getValueNode(inst.getPointerOperand());

    NodeID src = getValueNode(inst.getValueOperand());

    addStoreEdge(src, dst);

}

/*!
 * Visit getelementptr instructions
 */
void SVFIRBuilder::visitGetElementPtrInst(GetElementPtrInst &inst)
{

    NodeID dst = getValueNode(&inst);
    // GetElementPtrInst should always be a pointer or a vector contains pointers
    // for now we don't handle vector type here
    if(SVFUtil::isa<VectorType>(inst.getType()))
    {
        addBlackHoleAddrEdge(dst);
        return;
    }

    assert(SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process gep  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");

    NodeID src = getValueNode(inst.getPointerOperand());

    AccessPath ap(0, llvmModuleSet()->getSVFType(inst.getSourceElementType()));
    bool constGep = computeGepOffset(&inst, ap);
    addGepEdge(src, dst, ap, constGep);
}

/*
 * Visit cast instructions
 */
void SVFIRBuilder::visitCastInst(CastInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process cast  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");
    NodeID dst = getValueNode(&inst);

    const Value* opnd = inst.getOperand(0);
    NodeID src = getValueNode(opnd);
    addCopyEdge(src, dst, getCopyKind(&inst));
}

/*!
 * Visit Binary Operator
 */
void SVFIRBuilder::visitBinaryOperator(BinaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 2 && "not two operands for BinaryOperator?");
    Value* op1 = inst.getOperand(0);
    NodeID op1Node = getValueNode(op1);
    Value* op2 = inst.getOperand(1);
    NodeID op2Node = getValueNode(op2);
    u32_t opcode = inst.getOpcode();
    addBinaryOPEdge(op1Node, op2Node, dst, opcode);
}

/*!
 * Visit Unary Operator
 */
void SVFIRBuilder::visitUnaryOperator(UnaryOperator &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 1 && "not one operand for Unary instruction?");
    Value* opnd = inst.getOperand(0);
    NodeID src = getValueNode(opnd);
    u32_t opcode = inst.getOpcode();
    addUnaryOPEdge(src, dst, opcode);
}

/*!
 * Visit compare instruction
 */
void SVFIRBuilder::visitCmpInst(CmpInst &inst)
{
    NodeID dst = getValueNode(&inst);
    assert(inst.getNumOperands() == 2 && "not two operands for compare instruction?");
    Value* op1 = inst.getOperand(0);
    NodeID op1Node = getValueNode(op1);
    Value* op2 = inst.getOperand(1);
    NodeID op2Node = getValueNode(op2);
    u32_t predicate = inst.getPredicate();
    addCmpEdge(op1Node, op2Node, dst, predicate);
}


/*!
 * Visit select instructions
 */
void SVFIRBuilder::visitSelectInst(SelectInst &inst)
{

    DBOUT(DPAGBuild, outs() << "process select  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");

    NodeID dst = getValueNode(&inst);
    NodeID src1 = getValueNode(inst.getTrueValue());
    NodeID src2 = getValueNode(inst.getFalseValue());
    NodeID cond = getValueNode(inst.getCondition());
    /// Two operands have same incoming basic block, both are the current BB
    addSelectStmt(dst,src1,src2, cond);
}

void SVFIRBuilder::visitCallInst(CallInst &i)
{
    visitCallSite(&i);
}

void SVFIRBuilder::visitInvokeInst(InvokeInst &i)
{
    visitCallSite(&i);
}

void SVFIRBuilder::visitCallBrInst(CallBrInst &i)
{
    visitCallSite(&i);
}

/*
 * Visit callsites
 */
void SVFIRBuilder::visitCallSite(CallBase* cs)
{

    // skip llvm intrinsics
    if(isIntrinsicInst(cs))
        return;

    DBOUT(DPAGBuild,
          outs() << "process callsite " << svfcall->valueOnlyToString() << "\n");


    CallICFGNode* callBlockNode = llvmModuleSet()->getCallICFGNode(cs);
    RetICFGNode* retBlockNode = llvmModuleSet()->getRetICFGNode(cs);

    pag->addCallSite(callBlockNode);

    /// Collect callsite arguments and returns
    for (u32_t i = 0; i < cs->arg_size(); i++)
        pag->addCallSiteArgs(
            callBlockNode,
            SVFUtil::cast<ValVar>(pag->getGNode(getValueNode(cs->getArgOperand(i)))));

    if(!cs->getType()->isVoidTy())
        pag->addCallSiteRets(retBlockNode,pag->getGNode(getValueNode(cs)));

    if (callBlockNode->isVirtualCall())
    {
        const Value* value = cppUtil::getVCallVtblPtr(cs);
        callBlockNode->setVtablePtr(pag->getGNode(getValueNode(value)));
    }
    if (const Function *callee = LLVMUtil::getCallee(cs))
    {
        if (LLVMUtil::isExtCall(callee))
        {
            handleExtCall(cs, callee);
        }
        else
        {
            handleDirectCall(cs, callee);
        }
    }
    else
    {
        //If the callee was not identified as a function (null F), this is indirect.
        handleIndCall(cs);
    }
}

/*!
 * Visit return instructions of a function
 */
void SVFIRBuilder::visitReturnInst(ReturnInst &inst)
{

    // ReturnInst itself should always not be a pointer type
    assert(!SVFUtil::isa<PointerType>(inst.getType()));

    DBOUT(DPAGBuild, outs() << "process return  " << llvmModuleSet()->getSVFValue(&inst)->toString() << " \n");

    if(Value* src = inst.getReturnValue())
    {
        const FunObjVar *F = llvmModuleSet()->getFunObjVar(inst.getParent()->getParent());

        NodeID rnF = getReturnNode(F);
        NodeID vnS = getValueNode(src);
        const ICFGNode* icfgNode = llvmModuleSet()->getICFGNode(&inst);
        //vnS may be null if src is a null ptr
        addPhiStmt(rnF,vnS,icfgNode);
    }
}


/*!
 * visit extract value instructions for structures in registers
 * TODO: for now we just assume the pointer after extraction points to blackhole
 * for example %24 = extractvalue { i32, %struct.s_hash* } %call34, 0
 * %24 is a pointer points to first field of a register value %call34
 * however we can not create %call34 as an memory object, as it is register value.
 * Is that necessary treat extract value as getelementptr instruction later to get more precise results?
 */
void SVFIRBuilder::visitExtractValueInst(ExtractValueInst  &inst)
{
    NodeID dst = getValueNode(&inst);
    addBlackHoleAddrEdge(dst);
}

/*!
 * The �extractelement� instruction extracts a single scalar element from a vector at a specified index.
 * TODO: for now we just assume the pointer after extraction points to blackhole
 * The first operand of an �extractelement� instruction is a value of vector type.
 * The second operand is an index indicating the position from which to extract the element.
 *
 * <result> = extractelement <4 x i32> %vec, i32 0    ; yields i32
 */
void SVFIRBuilder::visitExtractElementInst(ExtractElementInst &inst)
{
    NodeID dst = getValueNode(&inst);
    addBlackHoleAddrEdge(dst);
}

/*!
 * Branch and switch instructions are treated as UnaryOP
 * br %cmp label %if.then, label %if.else
 */
void SVFIRBuilder::visitBranchInst(BranchInst &inst)
{
    NodeID brinst = getValueNode(&inst);
    NodeID cond;
    if (inst.isConditional())
        cond = getValueNode(inst.getCondition());
    else
        cond = pag->getNullPtr();

    assert(inst.getNumSuccessors() <= 2 && "if/else has more than two branches?");

    BranchStmt::SuccAndCondPairVec successors;
    std::vector<const Instruction*> nextInsts;
    LLVMUtil::getNextInsts(&inst, nextInsts);
    u32_t branchID = 0;
    for (const Instruction* succInst : nextInsts)
    {
        assert(branchID <= 1 && "if/else has more than two branches?");
        const ICFGNode* icfgNode = llvmModuleSet()->getICFGNode(succInst);
        successors.push_back(std::make_pair(icfgNode, 1-branchID));
        branchID++;
    }
    addBranchStmt(brinst, cond, successors);
    /// set conditional svf var
    if (inst.isConditional())
    {
        for (auto& edge : llvmModuleSet()->getICFGNode(&inst)->getOutEdges())
        {
            if (IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
            {
                intraEdge->setConditionVar(pag->getGNode(cond));
            }
        }
    }
}


/**
 * See more: https://github.com/SVF-tools/SVF/pull/1191
 *
 * Given the code:
 *
 * switch (a) {
 *   case 0: printf("0\n"); break;
 *   case 1:
 *   case 2:
 *   case 3: printf("a >=1 && a <= 3\n"); break;
 *   case 4:
 *   case 6:
 *   case 7:  printf("a >= 4 && a <=7\n"); break;
 *   default: printf("a < 0 || a > 7"); break;
 * }
 *
 * Generate the IR:
 *
 * switch i32 %0, label %sw.default [
 *  i32 0, label %sw.bb
 *  i32 1, label %sw.bb1
 *  i32 2, label %sw.bb1
 *  i32 3, label %sw.bb1
 *  i32 4, label %sw.bb3
 *  i32 6, label %sw.bb3
 *  i32 7, label %sw.bb3
 * ]
 *
 * We can get every case basic block and related case value:
 * [
 *   {%sw.default, -1},
 *   {%sw.bb, 0},
 *   {%sw.bb1, 1},
 *   {%sw.bb1, 2},
 *   {%sw.bb1, 3},
 *   {%sw.bb3, 4},
 *   {%sw.bb3, 6},
 *   {%sw.bb3, 7},
 * ]
 * Note: default case value is nullptr
 */
/// For larger number, we preserve case value just -1 now
/// see more: https://github.com/SVF-tools/SVF/pull/992

/// The following implementation follows ICFGBuilder::processFunBody
void SVFIRBuilder::visitSwitchInst(SwitchInst &inst)
{
    NodeID brinst = getValueNode(&inst);
    NodeID cond = getValueNode(inst.getCondition());

    BranchStmt::SuccAndCondPairVec successors;
    std::vector<const Instruction*> nextInsts;
    LLVMUtil::getNextInsts(&inst, nextInsts);
    for (const Instruction* succInst : nextInsts)
    {
        /// branch condition value
        const ConstantInt* condVal = inst.findCaseDest(const_cast<BasicBlock*>(succInst->getParent()));
        /// default case is set to -1;
        s64_t val = -1;
        if (condVal && condVal->getBitWidth() <= 64)
            val = (u32_t)LLVMUtil::getIntegerValue(condVal).first;
        const ICFGNode* icfgNode = llvmModuleSet()->getICFGNode(succInst);
        successors.push_back(std::make_pair(icfgNode, val));
    }
    addBranchStmt(brinst, cond, successors);
    /// set conditional svf var
    for (auto& edge : llvmModuleSet()->getICFGNode(&inst)->getOutEdges())
    {
        if (IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
        {
            intraEdge->setConditionVar(pag->getGNode(cond));
        }
    }
}


///   %ap = alloca %struct.va_list
///  %ap2 = bitcast %struct.va_list* %ap to i8*
/// ; Read a single integer argument from %ap2
/// %tmp = va_arg i8* %ap2, i32 (VAArgInst)
/// TODO: for now, create a copy edge from %ap2 to %tmp, we assume here %tmp should point to the n-th argument of the var_args
void SVFIRBuilder::visitVAArgInst(VAArgInst &inst)
{
    NodeID dst = getValueNode(&inst);
    Value* opnd = inst.getPointerOperand();
    NodeID src = getValueNode(opnd);
    addCopyEdge(src, dst, CopyStmt::COPYVAL);
}

/// <result> = freeze ty <val>
/// If <val> is undef or poison, ‘freeze’ returns an arbitrary, but fixed value of type `ty`
/// Otherwise, this instruction is a no-op and returns the input <val>
/// For now, we assume <val> is never a poison or undef.
void SVFIRBuilder::visitFreezeInst(FreezeInst &inst)
{
    NodeID dst = getValueNode(&inst);
    for (u32_t i = 0; i < inst.getNumOperands(); i++)
    {
        Value* opnd = inst.getOperand(i);
        NodeID src = getValueNode(opnd);
        addCopyEdge(src, dst, CopyStmt::COPYVAL);
    }
}


/*!
 * Add the constraints for a direct, non-external call.
 */
void SVFIRBuilder::handleDirectCall(CallBase* cs, const Function *F)
{

    assert(F);
    CallICFGNode* callICFGNode = llvmModuleSet()->getCallICFGNode(cs);
    const FunObjVar* svffun = llvmModuleSet()->getFunObjVar(F);
    DBOUT(DPAGBuild,
          outs() << "handle direct call " << LLVMUtil::dumpValue(cs) << " callee " << F->getName().str() << "\n");

    //Only handle the ret.val. if it's used as a ptr.
    NodeID dstrec = getValueNode(cs);
    //Does it actually return a ptr?
    if (!cs->getType()->isVoidTy())
    {
        NodeID srcret = getReturnNode(svffun);
        FunExitICFGNode* exitICFGNode = pag->getICFG()->getFunExitICFGNode(svffun);
        addRetEdge(srcret, dstrec,callICFGNode, exitICFGNode);
    }
    //Iterators for the actual and formal parameters
    u32_t itA = 0, ieA = cs->arg_size();
    Function::const_arg_iterator itF = F->arg_begin(), ieF = F->arg_end();
    //Go through the fixed parameters.
    DBOUT(DPAGBuild, outs() << "      args:");
    for (; itF != ieF; ++itA, ++itF)
    {
        //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
        if (itA == ieA)
        {
            DBOUT(DPAGBuild, outs() << " !! not enough args\n");
            break;
        }
        const Value* AA = cs->getArgOperand(itA), *FA = &*itF; //current actual/formal arg

        DBOUT(DPAGBuild, outs() << "process actual parm  " << llvmModuleSet()->getSVFValue(AA)->toString() << " \n");

        NodeID dstFA = getValueNode(FA);
        NodeID srcAA = getValueNode(AA);
        FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(svffun);
        addCallEdge(srcAA, dstFA, callICFGNode, entry);
    }
    //Any remaining actual args must be varargs.
    if (F->isVarArg())
    {
        NodeID vaF = getVarargNode(svffun);
        DBOUT(DPAGBuild, outs() << "\n      varargs:");
        for (; itA != ieA; ++itA)
        {
            const Value* AA = cs->getArgOperand(itA);
            NodeID vnAA = getValueNode(AA);
            FunEntryICFGNode* entry = pag->getICFG()->getFunEntryICFGNode(svffun);
            addCallEdge(vnAA,vaF, callICFGNode,entry);
        }
    }
    if(itA != ieA)
    {
        /// FIXME: this assertion should be placed for correct checking except
        /// bug program like 188.ammp, 300.twolf
        writeWrnMsg("too many args to non-vararg func.");
        writeWrnMsg("(" + callICFGNode->getSourceLoc() + ")");

    }
}

/*!
 * Example 1:

    %0 = getelementptr inbounds %struct.outer, %struct.inner %base, i32 0, i32 0
    call void @llvm.memcpy(ptr %inner, ptr %0, i64 24, i1 false)
    The base value for %0 is %base.
    Note: the %base is recognized as the base value if the offset (field index) is 0

 * Example 2:
 *     https://github.com/SVF-tools/SVF/issues/1650
       https://github.com/SVF-tools/SVF/pull/1652

    @i1 = dso_local global %struct.inner { i32 0, ptr @f1, ptr @f2 }
    @n1 = dso_local global %struct.outer { i32 0, ptr @i1 }

    %inner = alloca %struct.inner
    %0 = load ptr, ptr getelementptr inbounds (%struct.outer, ptr @n1, i32 0, i32 1)
    call void @llvm.memcpy(ptr %inner, ptr %0, i64 24, i1 false)

    The base value for %0 is @i1

  * Example 3:
  *
    @conststruct = internal global <{ [40 x i8], [4 x i8], [4 x i8], [2512 x i8] }>
        <{ [40 x i8] undef, [4 x i8] zeroinitializer, [4 x i8] undef, [2512 x i8] zeroinitializer }>, align 8

    %0 = load ptr, ptr getelementptr inbounds (<{ [40 x i8], [4 x i8], [4 x i8], [2512 x i8] }>,
         ptr @conststruct, i64 0, i32 0, i64 16)

    The base value for %0 is still %0
 */
const Value* SVFIRBuilder::getBaseValueForExtArg(const Value* V)
{
    const Value*  value = stripAllCasts(V);
    assert(value && "null ptr?");
    if(const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(value))
    {
        APOffset totalidx = 0;
        for (bridge_gep_iterator gi = bridge_gep_begin(gep), ge = bridge_gep_end(gep); gi != ge; ++gi)
        {
            if(const ConstantInt* op = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand()))
                totalidx += LLVMUtil::getIntegerValue(op).first;
        }
        if(totalidx == 0 && !SVFUtil::isa<StructType>(value->getType()))
            value = gep->getPointerOperand();
    }
    else if (const LoadInst* load = SVFUtil::dyn_cast<LoadInst>(value))
    {
        const Value* loadP = load->getPointerOperand();
        if (const GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(loadP))
        {
            APOffset totalidx = 0;
            for (bridge_gep_iterator gi = bridge_gep_begin(gep), ge = bridge_gep_end(gep); gi != ge; ++gi)
            {
                if(const ConstantInt* op = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand()))
                    totalidx += LLVMUtil::getIntegerValue(op).first;
            }
            const Value * pointer_operand = gep->getPointerOperand();
            if (auto *glob = SVFUtil::dyn_cast<GlobalVariable>(pointer_operand))
            {
                if (glob->hasInitializer())
                {
                    if (auto *initializer = SVFUtil::dyn_cast<
                                            ConstantStruct>(glob->getInitializer()))
                    {
                        /*
                            *@conststruct = internal global <{ [40 x i8], [4 x i8], [4 x i8], [2512 x i8] }>
                                <{ [40 x i8] undef, [4 x i8] zeroinitializer, [4 x i8] undef, [2512 x i8] zeroinitializer }>, align 8

                            %0 = load ptr, ptr getelementptr inbounds (<{ [40 x i8], [4 x i8], [4 x i8], [2512 x i8] }>,
                                    ptr @conststruct, i64 0, i32 0, i64 16)
                            in this case, totalidx is 16 while initializer->getNumOperands() is 4, so we return value as the base
                         */
                        if (totalidx >= initializer->getNumOperands()) return value;
                        auto *ptrField = initializer->getOperand(totalidx);
                        if (auto *ptrValue = SVFUtil::dyn_cast<llvm::GlobalVariable>(ptrField))
                        {
                            return ptrValue;
                        }
                    }
                }
            }
        }
    }

    return value;
}

/*!
 * Indirect call is resolved on-the-fly during pointer analysis
 */
void SVFIRBuilder::handleIndCall(CallBase* cs)
{
    const CallICFGNode* cbn = llvmModuleSet()->getCallICFGNode(cs);
    pag->addIndirectCallsites(cbn,llvmModuleSet()->getValueNode(cs->getCalledOperand()));
}

void SVFIRBuilder::updateCallGraph(CallGraph* callgraph)
{
    CallGraph::CallEdgeMap::const_iterator iter = callgraph->getIndCallMap().begin();
    CallGraph::CallEdgeMap::const_iterator eiter = callgraph->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallICFGNode* callBlock = iter->first;
        const CallBase* callbase = SVFUtil::cast<CallBase>(llvmModuleSet()->getLLVMValue(callBlock));
        assert(callBlock->isIndirectCall() && "this is not an indirect call?");
        const CallGraph::FunctionSet& functions = iter->second;
        for (CallGraph::FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const Function* callee = SVFUtil::cast<Function>(llvmModuleSet()->getLLVMValue(*func_iter));

            if (isExtCall(*func_iter))
            {
                setCurrentLocation(callee, callee->empty() ? nullptr : &callee->getEntryBlock());
                handleExtCall(callbase, callee);
            }
            else
            {
                setCurrentLocation(llvmModuleSet()->getLLVMValue(callBlock), callBlock->getBB());
                handleDirectCall(const_cast<CallBase*>(callbase), callee);
            }
        }
    }

    // dump SVFIR
    if (Options::PAGDotGraph())
        pag->dump("svfir_final");
}

/*
 * TODO: more sanity checks might be needed here
 */
void SVFIRBuilder::sanityCheck()
{
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
        (void) pag->getGNode(nIter->first);
        //TODO::
        // (1)  every source(root) node of a pag tree should be object node
        //       if a node has no incoming edge, but has outgoing edges
        //       then it has to be an object node.
        // (2)  make sure every variable should be initialized
        //      otherwise it causes the a null pointer, the aliasing relation may not be captured
        //      when loading a pointer value should make sure
        //      some value has been store into this pointer before
        //      q = load p, some value should stored into p first like store w p;
        // (3)  make sure PAGNode should not have a const expr value (pointer should have unique def)
        // (4)  look closely into addComplexConsForExt, make sure program locations(e.g.,inst bb)
        //      are set correctly for dummy gepval node
        // (5)  reduce unnecessary copy edge (const casts) and ensure correctness.
    }
}


/*!
 * Add a temp field value node according to base value and offset
 * this node is after the initial node method, it is out of scope of symInfo table
 */
NodeID SVFIRBuilder::getGepValVar(const Value* val, const AccessPath& ap, const SVFType* elementType)
{
    NodeID base = getValueNode(val);
    NodeID gepval = pag->getGepValVar(llvmModuleSet()->getValueNode(curVal), base, ap);
    if (gepval==UINT_MAX)
    {
        assert(((int) UINT_MAX)==-1 && "maximum limit of unsigned int is not -1?");
        /*
         * getGepValVar can only be called from two places:
         * 1. SVFIRBuilder::addComplexConsForExt to handle external calls
         * 2. SVFIRBuilder::getGlobalVarField to initialize global variable
         * so curVal can only be
         * 1. Instruction
         * 2. GlobalVariable
         */
        assert(
            (SVFUtil::isa<Instruction>(curVal) || SVFUtil::isa<GlobalVariable>(curVal)) && "curVal not an instruction or a globalvariable?");

        // We assume every GepValNode and its GepEdge to the baseNode are unique across the whole program
        // We preserve the current BB information to restore it after creating the gepNode
        const Value* cval = getCurrentValue();
        const SVFBasicBlock* cbb = getCurrentBB();
        setCurrentLocation(curVal, (SVFBasicBlock*) nullptr);
        LLVMModuleSet* llvmmodule = llvmModuleSet();
        const ICFGNode* node = nullptr;
        if (const Instruction* inst = SVFUtil::dyn_cast<Instruction>(curVal))
            if (llvmmodule->hasICFGNode(inst))
            {
                node = llvmmodule->getICFGNode(inst);
            }
        NodeID gepNode = pag->addGepValNode(llvmModuleSet()->getValueNode(curVal), cast<ValVar>(pag->getGNode(getValueNode(val))), ap,
                                            NodeIDAllocator::get()->allocateValueId(),
                                            llvmmodule->getSVFType(PointerType::getUnqual(llvmmodule->getContext())), node);
        addGepEdge(base, gepNode, ap, true);
        setCurrentLocation(cval, cbb);
        return gepNode;
    }
    else
        return gepval;
}


/*
 * curVal   <-------->  PAGEdge
 * Instruction          Any Edge
 * Argument             CopyEdge  (SVFIR::addFormalParamBlackHoleAddrEdge)
 * ConstantExpr         CopyEdge  (Int2PtrConstantExpr   CastConstantExpr  SVFIRBuilder::processCE)
 *                      GepEdge   (GepConstantExpr   SVFIRBuilder::processCE)
 * ConstantPointerNull  CopyEdge  (3-->2 NullPtr-->BlkPtr SVFIR::addNullPtrNode)
 *  				    AddrEdge  (0-->2 BlkObj-->BlkPtr SVFIR::addNullPtrNode)
 * GlobalVariable       AddrEdge  (SVFIRBuilder::visitGlobal)
 *                      GepEdge   (SVFIRBuilder::getGlobalVarField)
 * Function             AddrEdge  (SVFIRBuilder::visitGlobal)
 * Constant             StoreEdge (SVFIRBuilder::InitialGlobal)
 */
void SVFIRBuilder::setCurrentBBAndValueForPAGEdge(PAGEdge* edge)
{
    if (SVFIR::pagReadFromTXT())
        return;

    assert(curVal && "current Val is nullptr?");
    edge->setBB(curBB!=nullptr ? curBB : nullptr);
    edge->setValue(pag->getGNode(llvmModuleSet()->getValueNode(curVal)));
    ICFGNode* icfgNode = pag->getICFG()->getGlobalICFGNode();
    LLVMModuleSet* llvmMS = llvmModuleSet();
    if (const Instruction* curInst = SVFUtil::dyn_cast<Instruction>(curVal))
    {
        const FunObjVar* srcFun = edge->getSrcNode()->getFunction();
        const FunObjVar* dstFun = edge->getDstNode()->getFunction();
        if(srcFun!=nullptr && !SVFUtil::isa<RetPE>(edge) && !SVFUtil::isa<FunValVar>(edge->getSrcNode()) && !SVFUtil::isa<FunObjVar>(edge->getSrcNode()))
        {
            assert(srcFun==llvmMS->getFunObjVar(curInst->getFunction()) && "SrcNode of the PAGEdge not in the same function?");
        }
        if(dstFun!=nullptr && !SVFUtil::isa<CallPE>(edge) && !SVFUtil::isa<RetValPN>(edge->getDstNode()))
        {
            assert(dstFun==llvmMS->getFunObjVar(curInst->getFunction()) && "DstNode of the PAGEdge not in the same function?");
        }

        /// We assume every GepValVar and its GepStmt are unique across whole program
        if (!(SVFUtil::isa<GepStmt>(edge) && SVFUtil::isa<GepValVar>(edge->getDstNode())))
            assert(curBB && "instruction does not have a basic block??");

        /// We will have one unique function exit ICFGNode for all returns
        if(SVFUtil::isa<ReturnInst>(curInst))
        {
            icfgNode = pag->getICFG()->getFunExitICFGNode(llvmMS->getFunObjVar(curInst->getFunction()));
        }
        else
        {
            if(SVFUtil::isa<RetPE>(edge))
                icfgNode = llvmMS->getRetICFGNode(SVFUtil::cast<Instruction>(curInst));
            else
                icfgNode = llvmMS->getICFGNode(SVFUtil::cast<Instruction>(curInst));
        }
    }
    else if (const Argument* arg = SVFUtil::dyn_cast<Argument>(curVal))
    {
        assert(curBB && (curBB->getParent()->getEntryBlock() == curBB));
        icfgNode = pag->getICFG()->getFunEntryICFGNode(
                       llvmModuleSet()->getFunObjVar(SVFUtil::cast<Function>(arg->getParent())));
    }
    else if (SVFUtil::isa<Constant>(curVal) ||
             SVFUtil::isa<Function>(curVal) ||
             SVFUtil::isa<MetadataAsValue>(curVal))
    {
        if (!curBB)
            pag->addGlobalPAGEdge(edge);
        else
        {
            icfgNode = const_cast<ICFGNode*>(curBB->front());
        }
    }
    else
    {
        assert(false && "what else value can we have?");
    }

    pag->addToSVFStmtList(icfgNode,edge);
    icfgNode->addSVFStmt(edge);
    if(const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(edge))
    {
        CallICFGNode* callNode = const_cast<CallICFGNode*>(callPE->getCallSite());
        FunEntryICFGNode* entryNode = const_cast<FunEntryICFGNode*>(callPE->getFunEntryICFGNode());
        if(ICFGEdge* edge = pag->getICFG()->hasInterICFGEdge(callNode,entryNode, ICFGEdge::CallCF))
            SVFUtil::cast<CallCFGEdge>(edge)->addCallPE(callPE);
    }
    else if(const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(edge))
    {
        RetICFGNode* retNode = const_cast<RetICFGNode*>(retPE->getCallSite()->getRetICFGNode());
        FunExitICFGNode* exitNode = const_cast<FunExitICFGNode*>(retPE->getFunExitICFGNode());
        if(ICFGEdge* edge = pag->getICFG()->hasInterICFGEdge(exitNode, retNode, ICFGEdge::RetCF))
            SVFUtil::cast<RetCFGEdge>(edge)->addRetPE(retPE);
    }
}


/*!
 * Get a base SVFVar given a pointer
 * Return the source node of its connected normal gep edge
 * Otherwise return the node id itself
 * s32_t offset : gep offset
 */
AccessPath SVFIRBuilder::getAccessPathFromBaseNode(NodeID nodeId)
{
    SVFVar* node  = pag->getGNode(nodeId);
    SVFStmt::SVFStmtSetTy& geps = node->getIncomingEdges(SVFStmt::Gep);
    /// if this node is already a base node
    if(geps.empty())
        return AccessPath(0);

    assert(geps.size()==1 && "one node can only be connected by at most one gep edge!");
    SVFVar::iterator it = geps.begin();
    const GepStmt* gepEdge = SVFUtil::cast<GepStmt>(*it);
    if(gepEdge->isVariantFieldGep())
        return AccessPath(0);
    else
        return gepEdge->getAccessPath();
}
