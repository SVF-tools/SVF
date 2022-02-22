//===----- CHGBuiler.cpp -- Class hierarchy graph builder ---------------------------//
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
 * CHGBuiler.cpp
 *
 *  Created on: Jun 4, 2021
 *      Author: Yulei Sui
 */

#include <set>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>   // setw() for formatting cout
#include <assert.h>
#include <stack>

#include "SVF-FE/CHGBuilder.h"
#include "Util/Options.h"
#include "SVF-FE/CPPUtil.h"
#include "MemoryModel/SymbolTableInfo.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"
#include "Util/SVFModule.h"
#include "MemoryModel/PTAStat.h"

using namespace SVF;
using namespace SVFUtil;
using namespace cppUtil;

const string pureVirtualFunName = "__cxa_pure_virtual";

const string ztiLabel = "_ZTI";


void CHGBuilder::buildCHG()
{

    double timeStart, timeEnd;
    timeStart = PTAStat::getClk(true);
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        Module *M = LLVMModuleSet::getLLVMModuleSet()->getModule(i);
        assert(M && "module not found?");
        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("construct CHGraph From module "
                + M->getName().str() + "...\n"));
        readInheritanceMetadataFromModule(*M);
        for (Module::const_global_iterator I = M->global_begin(), E = M->global_end(); I != E; ++I)
            buildCHGNodes(&(*I));
        for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F)
            buildCHGNodes(getDefFunForMultipleModule(&(*F)));
        for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F)
            buildCHGEdges(getDefFunForMultipleModule(&(*F)));

        analyzeVTables(*M);
    }

    DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("build Internal Maps ...\n"));
    buildInternalMaps();

    timeEnd = PTAStat::getClk(true);
    chg->buildingCHGTime = (timeEnd - timeStart) / TIMEINTERVAL;

    if (Options::DumpCHA)
        chg->dump("cha");
}

void CHGBuilder::buildCHGNodes(const GlobalValue *globalvalue)
{
    if (isValVtbl(globalvalue) && globalvalue->getNumOperands() > 0)
    {
        const ConstantStruct *vtblStruct = SVFUtil::dyn_cast<ConstantStruct>(globalvalue->getOperand(0));
        assert(vtblStruct && "Initializer of a vtable not a struct?");
        string className = getClassNameFromVtblObj(globalvalue);
        if (!chg->getNode(className))
            createNode(className);

        for (unsigned int ei = 0; ei < vtblStruct->getNumOperands(); ++ei)
        {
            const ConstantArray *vtbl = SVFUtil::dyn_cast<ConstantArray>(vtblStruct->getOperand(ei));
            assert(vtbl && "Element of initializer not an array?");
            for (u32_t i = 0; i < vtbl->getNumOperands(); ++i)
            {
                if(const ConstantExpr *ce = isCastConstantExpr(vtbl->getOperand(i)))
                {
                    const Value *bitcastValue = ce->getOperand(0);
                    if (const  Function* func = SVFUtil::dyn_cast<Function>(bitcastValue))
                    {
                        struct DemangledName dname = demangle(func->getName().str());
                        if (!chg->getNode(dname.className))
                            createNode(dname.className);
                    }
                }
            }
        }
    }
}

void CHGBuilder::buildCHGNodes(const SVFFunction* fun)
{
    const Function* F = fun->getLLVMFun();
    if (isConstructor(F) || isDestructor(F))
    {
        struct DemangledName dname = demangle(F->getName().str());
        DBOUT(DCHA, outs() << "\t build CHANode for class " + dname.className + "...\n");
        if (!chg->getNode(dname.className))
            createNode(dname.className);
    }
}

void CHGBuilder::buildCHGEdges(const SVFFunction* fun)
{
    const Function* F = fun->getLLVMFun();

    if (isConstructor(F) || isDestructor(F))
    {
        for (Function::const_iterator B = F->begin(), E = F->end(); B != E; ++B)
        {
            for (BasicBlock::const_iterator I = B->begin(), E = B->end(); I != E; ++I)
            {
                if (SVFUtil::isCallSite(&(*I)))
                {
                    CallSite cs = SVFUtil::getLLVMCallSite(&(*I));
                    connectInheritEdgeViaCall(fun, cs);
                }
                else if (const StoreInst *store = SVFUtil::dyn_cast<StoreInst>(&(*I)))
                {
                    connectInheritEdgeViaStore(fun, store);
                }
            }
        }
    }
}


void CHGBuilder::buildInternalMaps()
{
    buildClassNameToAncestorsDescendantsMap();
    buildVirtualFunctionToIDMap();
    buildCSToCHAVtblsAndVfnsMap();
}

void CHGBuilder::connectInheritEdgeViaCall(const SVFFunction* callerfun, CallSite cs)
{
    if (getCallee(cs) == nullptr)
        return;

    const Function* callee = getCallee(cs)->getLLVMFun();
    const Function* caller = callerfun->getLLVMFun();

    struct DemangledName dname = demangle(caller->getName().str());
    if ((isConstructor(caller) && isConstructor(callee)) || (isDestructor(caller) && isDestructor(callee)))
    {
        if (cs.arg_size() < 1 || (cs.arg_size() < 2 && cs.paramHasAttr(0, llvm::Attribute::StructRet)))
            return;
        const Value *csThisPtr = getVCallThisPtr(cs);
        //const Argument *consThisPtr = getConstructorThisPtr(caller);
        //bool samePtr = isSameThisPtrInConstructor(consThisPtr, csThisPtr);
        bool samePtrTrue = true;
        if (csThisPtr != nullptr && samePtrTrue)
        {
            struct DemangledName basename = demangle(callee->getName().str());
            if (!SVFUtil::isCallSite(csThisPtr)  &&
                    basename.className.size() > 0)
            {
                chg->addEdge(dname.className, basename.className, CHEdge::INHERITANCE);
            }
        }
    }
}

void CHGBuilder::connectInheritEdgeViaStore(const SVFFunction* caller, const StoreInst* storeInst)
{
    struct DemangledName dname = demangle(caller->getName());
    if (const ConstantExpr *ce = SVFUtil::dyn_cast<ConstantExpr>(storeInst->getValueOperand()))
    {
        if (ce->getOpcode() == Instruction::BitCast)
        {
            const Value *bitcastval = ce->getOperand(0);
            if (const ConstantExpr *bcce = SVFUtil::dyn_cast<ConstantExpr>(bitcastval))
            {
                if (bcce->getOpcode() == Instruction::GetElementPtr)
                {
                    const Value *gepval = bcce->getOperand(0);
                    if (isValVtbl(gepval))
                    {
                        string vtblClassName = getClassNameFromVtblObj(gepval);
                        if (vtblClassName.size() > 0 && dname.className.compare(vtblClassName) != 0)
                        {
                            chg->addEdge(dname.className, vtblClassName, CHEdge::INHERITANCE);
                        }
                    }
                }
            }
        }
    }
}

void CHGBuilder::readInheritanceMetadataFromModule(const Module &M)
{
    for (Module::const_named_metadata_iterator mdit = M.named_metadata_begin(),
            mdeit = M.named_metadata_end(); mdit != mdeit; ++mdit)
    {
        const NamedMDNode *md = &*mdit;
        string mdname = md->getName().str();
        if (mdname.compare(0, 15, "__cxx_bases_of_") != 0)
            continue;
        string className = mdname.substr(15);
        for (NamedMDNode::const_op_iterator opit = md->op_begin(),
                opeit = md->op_end(); opit != opeit; ++opit)
        {
            const MDNode *N = *opit;
            const MDString &mdstr = SVFUtil::cast<MDString>(N->getOperand(0));
            string baseName = mdstr.getString().str();
            chg->addEdge(className, baseName, CHEdge::INHERITANCE);
        }
    }
}

CHNode *CHGBuilder::createNode(const std::string className)
{
    assert(!chg->getNode(className) && "this node should never be created before!");
    CHNode * node = new CHNode(className, chg->classNum++);
    chg->classNameToNodeMap[className] = node;
    chg->addGNode(node->getId(), node);
    if (className.size() > 0 && className[className.size() - 1] == '>')
    {
        string templateName = getBeforeBrackets(className);
        CHNode* templateNode = chg->getNode(templateName);
        if (!templateNode)
        {
            DBOUT(DCHA, outs() << "\t Create Template CHANode " + templateName + " for class " + className + "...\n");
            templateNode = createNode(templateName);
            templateNode->setTemplate();
        }
        chg->addEdge(className, templateName, CHEdge::INSTANTCE);
        chg->addInstances(templateName,node);
    }
    return node;
}

/*
 * build the following two maps:
 * classNameToDescendantsMap
 * chg->classNameToAncestorsMap
 */
void CHGBuilder::buildClassNameToAncestorsDescendantsMap()
{

    for (CHGraph::const_iterator it = chg->begin(), eit = chg->end();
            it != eit; ++it)
    {
        const CHNode *node = it->second;
        WorkList worklist;
        CHNodeSetTy visitedNodes;
        worklist.push(node);
        while (!worklist.empty())
        {
            const CHNode *curnode = worklist.pop();
            if (visitedNodes.find(curnode) == visitedNodes.end())
            {
                for (CHEdge::CHEdgeSetTy::const_iterator it =
                            curnode->getOutEdges().begin(), eit =
                            curnode->getOutEdges().end(); it != eit; ++it)
                {
                    if ((*it)->getEdgeType() == CHEdge::INHERITANCE)
                    {
                        CHNode *succnode = (*it)->getDstNode();
                        chg->classNameToAncestorsMap[node->getName()].insert(succnode);
                        chg->classNameToDescendantsMap[succnode->getName()].insert(node);
                        worklist.push(succnode);
                    }
                }
                visitedNodes.insert(curnode);
            }
        }
    }
}


const CHGraph::CHNodeSetTy& CHGBuilder::getInstancesAndDescendants(const string className)
{

    CHGraph::NameToCHNodesMap::const_iterator it = chg->classNameToInstAndDescsMap.find(className);
    if (it != chg->classNameToInstAndDescsMap.end())
    {
        return it->second;
    }
    else
    {
        chg->classNameToInstAndDescsMap[className] = chg->getDescendants(className);
        if (chg->getNode(className)->isTemplate())
        {
            const CHNodeSetTy& instances = chg->getInstances(className);
            for (CHNodeSetTy::const_iterator it = instances.begin(), eit = instances.end(); it != eit; ++it)
            {
                const CHNode *node = *it;
                chg->classNameToInstAndDescsMap[className].insert(node);
                const CHNodeSetTy& instance_descendants = chg->getDescendants(node->getName());
                for (CHNodeSetTy::const_iterator dit =
                            instance_descendants.begin(), deit =
                            instance_descendants.end(); dit != deit; ++dit)
                {
                    chg->classNameToInstAndDescsMap[className].insert(*dit);
                }
            }
        }
        return chg->classNameToInstAndDescsMap[className];
    }
}




/*
 * do the following things:
 * 1. initialize virtualFunctions for each class
 * 2. mark multi-inheritance classes
 * 3. mark pure abstract classes
 *
 * Layout of VTables:
 *
 * 1. single inheritance
 * class A {...};
 * class B: public A {...};
 * B's vtable: {i8 *null, _ZTI1B, ...}
 *
 * 2. normal multiple inheritance
 * class A {...};
 * class B {...};
 * class C: public A, public B {...};
 * C's vtable: {i8 *null, _ZTI1C, ..., inttoptr xxx, _ZTI1C, ...}
 * "inttoptr xxx" servers as a delimiter for dividing virtual methods inherited
 * from "class A" and "class B"
 *
 * 3. virtual diamond inheritance
 * class A {...};
 * class B: public virtual A {...};
 * class C: public virtual A {...};
 * class D: public B, public C {...};
 * D's vtable: {i8 *null, _ZTI1C, ..., inttoptr xxx, _ZTI1C, i8 *null, ...}
 * there will several "i8 *null" following "inttoptr xxx, _ZTI1C,", and the
 * number of "i8 *null" is the same as the number of virtual methods in
 * "class A"
 */
void CHGBuilder::analyzeVTables(const Module &M)
{
    for (Module::const_global_iterator I = M.global_begin(),
            E = M.global_end(); I != E; ++I)
    {
        const GlobalValue *globalvalue = SVFUtil::dyn_cast<const GlobalValue>(&(*I));
        if (isValVtbl(globalvalue) && globalvalue->getNumOperands() > 0)
        {
            const ConstantStruct *vtblStruct =
                SVFUtil::dyn_cast<ConstantStruct>(globalvalue->getOperand(0));
            assert(vtblStruct && "Initializer of a vtable not a struct?");

            string vtblClassName = getClassNameFromVtblObj(globalvalue);
            CHNode *node = chg->getNode(vtblClassName);
            assert(node && "node not found?");

            node->setVTable(globalvalue);

            for (unsigned int ei = 0; ei < vtblStruct->getNumOperands(); ++ei)
            {
                const ConstantArray *vtbl =
                    SVFUtil::dyn_cast<ConstantArray>(vtblStruct->getOperand(ei));
                assert(vtbl && "Element of initializer not an array?");

                /*
                 * items in vtables can be classified into 3 categories:
                 * 1. i8* null
                 * 2. i8* inttoptr xxx
                 * 3. i8* bitcast xxx
                 */
                bool pure_abstract = true;
                u32_t i = 0;
                while (i < vtbl->getNumOperands())
                {
                    CHNode::FuncVector virtualFunctions;
                    bool is_virtual = false; // virtual inheritance
                    int null_ptr_num = 0;
                    for (; i < vtbl->getNumOperands(); ++i)
                    {
                        if (SVFUtil::isa<ConstantPointerNull>(vtbl->getOperand(i)))
                        {
                            if (i > 0 && !SVFUtil::isa<ConstantPointerNull>(vtbl->getOperand(i-1)))
                            {
                                const ConstantExpr *ce =
                                    SVFUtil::dyn_cast<ConstantExpr>(vtbl->getOperand(i-1));
                                if (ce->getOpcode() == Instruction::BitCast)
                                {
                                    const Value *bitcastValue = ce->getOperand(0);
                                    string bitcastValueName = bitcastValue->getName().str();
                                    if (bitcastValueName.compare(0, ztiLabel.size(), ztiLabel) == 0)
                                    {
                                        is_virtual = true;
                                        null_ptr_num = 1;
                                        while (i+null_ptr_num < vtbl->getNumOperands())
                                        {
                                            if (SVFUtil::isa<ConstantPointerNull>(vtbl->getOperand(i+null_ptr_num)))
                                                null_ptr_num++;
                                            else
                                                break;
                                        }
                                    }
                                }
                            }
                            continue;
                        }
                        const ConstantExpr *ce =
                            SVFUtil::dyn_cast<ConstantExpr>(vtbl->getOperand(i));
                        assert(ce != nullptr && "item in vtable not constantexp or null");
                        u32_t opcode = ce->getOpcode();
                        assert(opcode == Instruction::IntToPtr ||
                               opcode == Instruction::BitCast);
                        assert(ce->getNumOperands() == 1 &&
                               "inttptr or bitcast operand num not 1");
                        if (opcode == Instruction::IntToPtr)
                        {
                            node->setMultiInheritance();
                            ++i;
                            break;
                        }
                        if (opcode == Instruction::BitCast)
                        {
                            const Value *bitcastValue = ce->getOperand(0);
                            string bitcastValueName = bitcastValue->getName().str();
                            /*
                             * value in bitcast:
                             * _ZTIXXX
                             * Function
                             * GlobalAlias (alias to other function)
                             */
                            assert(SVFUtil::isa<Function>(bitcastValue) ||
                                   SVFUtil::isa<GlobalValue>(bitcastValue));
                            if (const Function* f = SVFUtil::dyn_cast<Function>(bitcastValue))
                            {
                                const SVFFunction* func = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(f);
                                addFuncToFuncVector(virtualFunctions, func);
                                if (func->getName().compare(pureVirtualFunName) == 0)
                                {
                                    pure_abstract &= true;
                                }
                                else
                                {
                                    pure_abstract &= false;
                                }
                                struct DemangledName dname = demangle(func->getName());
                                if (dname.className.size() > 0 &&
                                        vtblClassName.compare(dname.className) != 0)
                                {
                                    chg->addEdge(vtblClassName, dname.className, CHEdge::INHERITANCE);
                                }
                            }
                            else
                            {
                                if (const GlobalAlias *alias =
                                            SVFUtil::dyn_cast<GlobalAlias>(bitcastValue))
                                {
                                    const Constant *aliasValue = alias->getAliasee();
                                    if (const Function* aliasFunc =
                                                SVFUtil::dyn_cast<Function>(aliasValue))
                                    {
                                        const SVFFunction* func = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(aliasFunc);
                                        addFuncToFuncVector(virtualFunctions, func);
                                    }
                                    else if (const ConstantExpr *aliasconst =
                                                 SVFUtil::dyn_cast<ConstantExpr>(aliasValue))
                                    {
                                        u32_t aliasopcode = aliasconst->getOpcode();
                                        assert(aliasopcode == Instruction::BitCast &&
                                               "aliased constantexpr in vtable not a bitcast");
                                        const Function* aliasbitcastfunc =
                                            SVFUtil::dyn_cast<Function>(aliasconst->getOperand(0));
                                        assert(aliasbitcastfunc &&
                                               "aliased bitcast in vtable not a function");
                                        const SVFFunction* func = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(aliasbitcastfunc);
                                        addFuncToFuncVector(virtualFunctions, func);
                                    }
                                    else
                                    {
                                        assert(false && "alias not function or bitcast");
                                    }

                                    pure_abstract &= false;
                                }
                                else if (bitcastValueName.compare(0, ztiLabel.size(),
                                                                  ztiLabel) == 0)
                                {
                                }
                                else
                                {
                                    assert("what else can be in bitcast of a vtable?");
                                }
                            }
                        }
                    }
                    if (is_virtual && virtualFunctions.size() > 0)
                    {
                        for (int i = 0; i < null_ptr_num; ++i)
                        {
                            const SVFFunction* fun = virtualFunctions[i];
                            virtualFunctions.insert(virtualFunctions.begin(), fun);
                        }
                    }
                    if (virtualFunctions.size() > 0)
                        node->addVirtualFunctionVector(virtualFunctions);
                }
                if (pure_abstract == true)
                {
                    node->setPureAbstract();
                }
            }
        }
    }
}


void CHGBuilder::buildVirtualFunctionToIDMap()
{
    /*
     * 1. Divide classes into groups
     * 2. Get all virtual functions in a group
     * 3. Assign consecutive IDs to virtual functions that have
     * the same name (after demangling) in a group
     */
    CHGraph::CHNodeSetTy visitedNodes;
    for (CHGraph::const_iterator nit = chg->begin(),
            neit = chg->end(); nit != neit; ++nit)
    {
        CHNode *node = nit->second;
        if (visitedNodes.find(node) != visitedNodes.end())
            continue;

        string className = node->getName();

        /*
         * get all the classes in a specific group
         */
        CHGraph::CHNodeSetTy group;
        stack<const CHNode*> nodeStack;
        nodeStack.push(node);
        while (!nodeStack.empty())
        {
            const CHNode *curnode = nodeStack.top();
            nodeStack.pop();
            group.insert(curnode);
            if (visitedNodes.find(curnode) != visitedNodes.end())
                continue;
            for (CHEdge::CHEdgeSetTy::const_iterator it = curnode->getOutEdges().begin(),
                    eit = curnode->getOutEdges().end(); it != eit; ++it)
            {
                CHNode *tmpnode = (*it)->getDstNode();
                nodeStack.push(tmpnode);
                group.insert(tmpnode);
            }
            for (CHEdge::CHEdgeSetTy::const_iterator it = curnode->getInEdges().begin(),
                    eit = curnode->getInEdges().end(); it != eit; ++it)
            {
                CHNode *tmpnode = (*it)->getSrcNode();
                nodeStack.push(tmpnode);
                group.insert(tmpnode);
            }
            visitedNodes.insert(curnode);
        }

        /*
         * get all virtual functions in a specific group
         */
        set<const SVFFunction*> virtualFunctions;
        for (CHGraph::CHNodeSetTy::iterator it = group.begin(),
                eit = group.end(); it != eit; ++it)
        {
            const vector<CHNode::FuncVector> &vecs = (*it)->getVirtualFunctionVectors();
            for (vector<CHNode::FuncVector>::const_iterator vit = vecs.begin(),
                    veit = vecs.end(); vit != veit; ++vit)
            {
                for (vector<const SVFFunction*>::const_iterator fit = (*vit).begin(),
                        feit = (*vit).end(); fit != feit; ++fit)
                {
                    virtualFunctions.insert(*fit);
                }
            }
        }

        /*
         * build a set of pairs of demangled function name and function in a
         * specific group, items in the set will be sort by the first item of the
         * pair, so all the virtual functions in a group will be sorted by the
         * demangled function name
         * <f, A::f>
         * <f, B::f>
         * <g, A::g>
         * <g, B::g>
         * <g, C::g>
         * <~A, A::~A>
         * <~B, B::~B>
         * <~C, C::~C>
         * ...
         */
        set<pair<string, const SVFFunction*> > fNameSet;
        for (set<const SVFFunction*>::iterator fit = virtualFunctions.begin(),
                feit = virtualFunctions.end(); fit != feit; ++fit)
        {
            const SVFFunction* f = *fit;
            struct DemangledName dname = demangle(f->getName());
            fNameSet.insert(pair<string, const SVFFunction*>(dname.funcName, f));
        }
        for (set<pair<string, const SVFFunction*>>::iterator it = fNameSet.begin(),
                eit = fNameSet.end(); it != eit; ++it)
        {
            chg->virtualFunctionToIDMap[it->second] = chg->vfID++;
        }
    }
}


void CHGBuilder::buildCSToCHAVtblsAndVfnsMap()
{

    for (SymbolTableInfo::CallSiteSet::const_iterator it =
                SymbolTableInfo::SymbolInfo()->getCallSiteSet().begin(), eit =
                SymbolTableInfo::SymbolInfo()->getCallSiteSet().end(); it != eit; ++it)
    {
        CallSite cs = *it;
        if (!cppUtil::isVirtualCallSite(cs))
            continue;
        VTableSet vtbls;
        const CHNodeSetTy& chClasses = getCSClasses(cs);
        for (CHNodeSetTy::const_iterator it = chClasses.begin(), eit = chClasses.end(); it != eit; ++it)
        {
            const CHNode *child = *it;
            const GlobalValue *vtbl = child->getVTable();
            if (vtbl != nullptr)
            {
                vtbls.insert(vtbl);
            }
        }
        if (vtbls.size() > 0)
        {
            chg->csToCHAVtblsMap[cs] = vtbls;
            VFunSet virtualFunctions;
            chg->getVFnsFromVtbls(cs, vtbls, virtualFunctions);
            if (virtualFunctions.size() > 0)
                chg->csToCHAVFnsMap[cs] = virtualFunctions;
        }
    }
}

const CHGraph::CHNodeSetTy& CHGBuilder::getCSClasses(CallSite cs)
{
    assert(isVirtualCallSite(cs) && "not virtual callsite!");

    CHGraph::CallSiteToCHNodesMap::const_iterator it = chg->csToClassesMap.find(cs);
    if (it != chg->csToClassesMap.end())
    {
        return it->second;
    }
    else
    {
        string thisPtrClassName = getClassNameOfThisPtr(cs);
        if (const CHNode* thisNode = chg->getNode(thisPtrClassName))
        {
            const CHGraph::CHNodeSetTy& instAndDesces = getInstancesAndDescendants(thisPtrClassName);
            chg->csToClassesMap[cs].insert(thisNode);
            for (CHGraph::CHNodeSetTy::const_iterator it = instAndDesces.begin(), eit = instAndDesces.end(); it != eit; ++it)
                chg->csToClassesMap[cs].insert(*it);
        }
        return chg->csToClassesMap[cs];
    }
}

void CHGBuilder::addFuncToFuncVector(CHNode::FuncVector &v, const SVFFunction *f) {
    const auto *lf = f->getLLVMFun();
    if (isCPPThunkFunction(lf)) {
        if(const auto *tf = getThunkTarget(lf))
            v.push_back(chg->svfMod->getSVFFunction(tf));
    } else {
        v.push_back(f);
    }
}
