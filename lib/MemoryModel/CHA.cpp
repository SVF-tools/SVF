//===----- CHA.cpp  Base class of pointer analyses ---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * CHA.cpp
 *
 *  Created on: Apr 13, 2016
 *      Author: Xiaokang Fan
 */

#include <set>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>   // setw() for formatting cout
#include "Util/CPPUtil.h"
#include <assert.h>
#include <stack>
#include "MemoryModel/CHA.h"
#include "MemoryModel/MemModel.h"
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/DebugInfo.h> // for debuginfo like DILocation
#include <llvm/Support/DOTGraphTraits.h>	// for dot graph traits
#include <llvm/IR/InstIterator.h> // for inst_iterator
#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"
#include "Util/SVFModule.h"

using namespace llvm;
using namespace cppUtil;
using namespace std;

static cl::opt<bool> dumpCHA("dump-cha", cl::init(false), cl::desc("dump the class hierarchy graph"));

const string pureVirtualFunName = "__cxa_pure_virtual";

const string ztiLabel = "_ZTI";

static bool hasEdge(const CHNode *src, const CHNode *dst,
                    CHEdge::CHEDGETYPE et) {
    for (CHEdge::CHEdgeSetTy::const_iterator it = src->getOutEdges().begin(),
            eit = src->getOutEdges().end(); it != eit; ++it) {
        CHNode *node = (*it)->getDstNode();
        CHEdge::CHEDGETYPE edgeType = (*it)->getEdgeType();
        if (node == dst && edgeType == et)
            return true;
    }
    return false;
}

void CHNode::getVirtualFunctions(u32_t idx, FuncVector &virtualFunctions) const {
    for (vector<FuncVector>::const_iterator it = virtualFunctionVectors.begin(),
            eit = virtualFunctionVectors.end(); it != eit; ++it) {
        if ((*it).size() > idx)
            virtualFunctions.push_back((*it)[idx]);
    }
}

CHGraph::~CHGraph() {
    for (CHGraph::iterator it = this->begin(), eit = this->end(); it != eit; ++it)
        delete it->second;
}

void CHGraph::buildCHG() {

	double timeStart, timeEnd;
	timeStart = CLOCK_IN_MS();
	for (u32_t i = 0; i < svfMod.getModuleNum(); ++i) {
		Module *M = svfMod.getModule(i);
		assert(M && "module not found?");
		DBOUT(DGENERAL, outs() << analysisUtil::pasMsg("construct CHGraph From module "
										+ M->getName().str() + "...\n"));
		readInheritanceMetadataFromModule(*M);
		for (Module::const_global_iterator I = M->global_begin(), E = M->global_end(); I != E; ++I)
			buildCHGNodes(&(*I));
		for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F)
			buildCHGNodes(&(*F));
		for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F)
			buildCHGEdges(&(*F));

		analyzeVTables(*M);
	}

	DBOUT(DGENERAL, outs() << analysisUtil::pasMsg("build Internal Maps ...\n"));
	buildInternalMaps();

	timeEnd = CLOCK_IN_MS();
	buildingCHGTime = (timeEnd - timeStart) / TIMEINTERVAL;

	if (dumpCHA)
		dump("cha");
}

void CHGraph::buildCHGNodes(const GlobalValue *globalvalue) {
	if (isValVtbl(globalvalue) && globalvalue->getNumOperands() > 0) {
		const ConstantStruct *vtblStruct = llvm::dyn_cast<ConstantStruct>(globalvalue->getOperand(0));
		assert(vtblStruct && "Initializer of a vtable not a struct?");
		string className = getClassNameFromVtblObj(globalvalue);
		if (!getNode(className))
			createNode(className);

        for (int ei = 0; ei < vtblStruct->getNumOperands(); ++ei) {
            const ConstantArray *vtbl = llvm::dyn_cast<ConstantArray>(vtblStruct->getOperand(ei));
            assert(vtbl && "Element of initializer not an array?");
			for (u32_t i = 0; i < vtbl->getNumOperands(); ++i) {
				if(const ConstantExpr *ce = analysisUtil::isCastConstantExpr(vtbl->getOperand(i))){
					const Value *bitcastValue = ce->getOperand(0);
					if (const Function *func = llvm::dyn_cast<Function>(bitcastValue)) {
						struct DemangledName dname = demangle(func->getName().str());
						if (!getNode(dname.className))
							createNode(dname.className);
					}
				}
			}
        }
	}
}

void CHGraph::buildCHGNodes(const Function *F) {
	if (isConstructor(F) || isDestructor(F)) {
		struct DemangledName dname = demangle(F->getName().str());
		DBOUT(DCHA, outs() << "\t build CHANode for class " + dname.className + "...\n");
		if (!getNode(dname.className))
			createNode(dname.className);
	}
}

void CHGraph::buildCHGEdges(const Function *F) {
	if (isConstructor(F) || isDestructor(F)) {
		for (Function::const_iterator B = F->begin(), E = F->end(); B != E; ++B) {
			for (BasicBlock::const_iterator I = B->begin(), E = B->end(); I != E; ++I) {
				if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
					CallSite cs = analysisUtil::getLLVMCallSite(&(*I));
					connectInheritEdgeViaCall(F, cs);
				} else if (const StoreInst *store = dyn_cast<StoreInst>(I)) {
					connectInheritEdgeViaStore(F, store);
				}
			}
		}
	}
}


void CHGraph::buildInternalMaps() {
    buildClassNameToAncestorsDescendantsMap();
    buildVirtualFunctionToIDMap();
    buildCSToCHAVtblsAndVfnsMap();
}

void CHGraph::connectInheritEdgeViaCall(const Function* caller, CallSite cs){
    const Function *callee = analysisUtil::getCallee(cs);
    if (callee == NULL)
        return;

    struct DemangledName dname = demangle(caller->getName().str());
    if ((isConstructor(caller) && isConstructor(callee)) || (isDestructor(caller) && isDestructor(callee))) {
        if (cs.arg_size() < 1 || (cs.arg_size() < 2 && cs.paramHasAttr(0, Attribute::StructRet)))
            return;
        const Value *csThisPtr = getVCallThisPtr(cs);
        const Argument *consThisPtr = getConstructorThisPtr(caller);
        bool samePtr = true; // isSameThisPtrInConstructor(consThisPtr,csThisPtr);
        if (csThisPtr != NULL && samePtr) {
            struct DemangledName basename = demangle(callee->getName().str());
            if (!isa<CallInst>(csThisPtr) && !isa<InvokeInst>(csThisPtr) &&
                    basename.className.size() > 0) {
                addEdge(dname.className, basename.className, CHEdge::INHERITANCE);
            }
        }
    }
}

void CHGraph::connectInheritEdgeViaStore(const Function* caller, const llvm::StoreInst* storeInst){
    struct DemangledName dname = demangle(caller->getName().str());
    if (const ConstantExpr *ce = dyn_cast<ConstantExpr>(storeInst->getValueOperand())) {
        if (ce->getOpcode() == Instruction::BitCast) {
            const Value *bitcastval = ce->getOperand(0);
            if (const ConstantExpr *bcce = dyn_cast<ConstantExpr>(bitcastval)) {
                if (bcce->getOpcode() == Instruction::GetElementPtr) {
                    const Value *gepval = bcce->getOperand(0);
                    if (isValVtbl(gepval)) {
                        string vtblClassName = getClassNameFromVtblObj(gepval);
                        if (vtblClassName.size() > 0 && dname.className.compare(vtblClassName) != 0) {
                            addEdge(dname.className, vtblClassName, CHEdge::INHERITANCE);
                        }
                    }
                }
            }
        }
    }
}

void CHGraph::readInheritanceMetadataFromModule(const Module &M) {
    for (Module::const_named_metadata_iterator mdit = M.named_metadata_begin(),
            mdeit = M.named_metadata_end(); mdit != mdeit; ++mdit) {
        const NamedMDNode *md = &*mdit;
        string mdname = md->getName().str();
        if (mdname.compare(0, 15, "__cxx_bases_of_") != 0)
            continue;
        string className = mdname.substr(15);
        for (NamedMDNode::const_op_iterator opit = md->op_begin(),
                opeit = md->op_end(); opit != opeit; ++opit) {
            const MDNode *N = *opit;
            MDString *mdstr = cast<MDString>(N->getOperand(0));
            string baseName = mdstr->getString().str();
            addEdge(className, baseName, CHEdge::INHERITANCE);
        }
    }
}

void CHGraph::addEdge(const string className, const string baseClassName,
                      CHEdge::CHEDGETYPE edgeType) {
    CHNode *srcNode = getNode(className);
    CHNode *dstNode = getNode(baseClassName);
    assert(srcNode && dstNode && "node not found?");

    if (!hasEdge(srcNode, dstNode, edgeType)) {
        CHEdge *edge = new CHEdge(srcNode, dstNode, edgeType);
        srcNode->addOutgoingEdge(edge);
        dstNode->addIncomingEdge(edge);
    }
}

CHNode *CHGraph::getNode(const string name) const {
    for (CHGraph::const_iterator it = this->begin(), eit = this->end();
            it != eit; ++it) {
        CHNode *node = it->second;
        if (node->getName() == name)
            return node;
    }
    return NULL;
}


CHNode *CHGraph::createNode(const std::string className) {
	assert(!getNode(className) && "this node should never be created before!");
	CHNode * node = new CHNode(className, classNum++);
	addGNode(node->getId(), node);
	if (className.size() > 0 && className[className.size() - 1] == '>') {
		string templateName = getBeforeBrackets(className);
		CHNode* templateNode = getNode(templateName);
		if (!templateNode) {
			DBOUT(DCHA, outs() << "\t Create Template CHANode " + templateName + " for class " + className + "...\n");
			templateNode = createNode(templateName);
			templateNode->setTemplate();
		}
		addEdge(className, templateName, CHEdge::INSTANTCE);
		addInstances(templateName,node);
	}
	return node;
}

/*
 * build the following two maps:
 * classNameToDescendantsMap
 * classNameToAncestorsMap
 */
void CHGraph::buildClassNameToAncestorsDescendantsMap() {

	for (CHGraph::const_iterator it = this->begin(), eit = this->end();
			it != eit; ++it) {
		const CHNode *node = it->second;
		WorkList worklist;
		CHNodeSetTy visitedNodes;
		worklist.push(node);
		while (!worklist.empty()) {
			const CHNode *curnode = worklist.pop();
			if (visitedNodes.find(curnode) == visitedNodes.end()) {
				for (CHEdge::CHEdgeSetTy::const_iterator it =
						curnode->getOutEdges().begin(), eit =
						curnode->getOutEdges().end(); it != eit; ++it) {
					if ((*it)->getEdgeType() == CHEdge::INHERITANCE) {
						CHNode *succnode = (*it)->getDstNode();
						classNameToAncestorsMap[node->getName()].insert(succnode);
						classNameToDescendantsMap[succnode->getName()].insert(node);
						worklist.push(succnode);
					}
				}
				visitedNodes.insert(curnode);
			}
		}
	}
}


const CHGraph::CHNodeSetTy& CHGraph::getInstancesAndDescendants(const string className) {

	NameToCHNodesMap::const_iterator it = classNameToInstAndDescsMap.find(className);
	if (it != classNameToInstAndDescsMap.end()) {
		return it->second;
	} else {
		classNameToInstAndDescsMap[className] = getDescendants(className);
		if (getNode(className)->isTemplate()) {
			const CHNodeSetTy& instances = getInstances(className);
			for (CHNodeSetTy::const_iterator it = instances.begin(), eit = instances.end(); it != eit; ++it) {
				const CHNode *node = *it;
				classNameToInstAndDescsMap[className].insert(node);
				const CHNodeSetTy& instance_descendants = getDescendants(node->getName());
				for (CHNodeSetTy::const_iterator dit =
						instance_descendants.begin(), deit =
						instance_descendants.end(); dit != deit; ++dit) {
					classNameToInstAndDescsMap[className].insert(*dit);
				}
			}
		}
		return classNameToInstAndDescsMap[className];
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
void CHGraph::analyzeVTables(const Module &M) {
    for (Module::const_global_iterator I = M.global_begin(),
            E = M.global_end(); I != E; ++I) {
        const GlobalValue *globalvalue = dyn_cast<const GlobalValue>(I);
        if (isValVtbl(globalvalue) && globalvalue->getNumOperands() > 0) {
            const ConstantStruct *vtblStruct =
                dyn_cast<ConstantStruct>(globalvalue->getOperand(0));
            assert(vtblStruct && "Initializer of a vtable not a struct?");

            string vtblClassName = getClassNameFromVtblObj(globalvalue);
            CHNode *node = getNode(vtblClassName);
            assert(node && "node not found?");

            node->setVTable(globalvalue);

            for (int ei = 0; ei < vtblStruct->getNumOperands(); ++ei) {
                const ConstantArray *vtbl =
                    dyn_cast<ConstantArray>(vtblStruct->getOperand(ei));
                assert(vtbl && "Element of initializer not an array?");

                /*
                 * items in vtables can be classified into 3 categories:
                 * 1. i8* null
                 * 2. i8* inttoptr xxx
                 * 3. i8* bitcast xxx
                 */
                bool pure_abstract = true;
                u32_t i = 0;
                while (i < vtbl->getNumOperands()) {
                    CHNode::FuncVector virtualFunctions;
                    bool is_virtual = false; // virtual inheritance
                    int null_ptr_num = 0;
                    for (; i < vtbl->getNumOperands(); ++i) {
                        if (isa<ConstantPointerNull>(vtbl->getOperand(i))) {
                            if (i > 0 && !isa<ConstantPointerNull>(vtbl->getOperand(i-1))) {
                                const ConstantExpr *ce =
                                    dyn_cast<ConstantExpr>(vtbl->getOperand(i-1));
                                if (ce->getOpcode() == Instruction::BitCast) {
                                    const Value *bitcastValue = ce->getOperand(0);
                                    string bitcastValueName = bitcastValue->getName().str();
                                    if (bitcastValueName.compare(0, ztiLabel.size(), ztiLabel) == 0) {
                                        is_virtual = true;
                                        null_ptr_num = 1;
                                        while (i+null_ptr_num < vtbl->getNumOperands()) {
                                            if (isa<ConstantPointerNull>(vtbl->getOperand(i+null_ptr_num)))
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
                            dyn_cast<ConstantExpr>(vtbl->getOperand(i));
                        assert(ce != NULL && "item in vtable not constantexp or null");
                        u32_t opcode = ce->getOpcode();
                        assert(opcode == Instruction::IntToPtr ||
                               opcode == Instruction::BitCast);
                        assert(ce->getNumOperands() == 1 &&
                               "inttptr or bitcast operand num not 1");
                        if (opcode == Instruction::IntToPtr) {
                            node->setMultiInheritance();
                            ++i;
                            break;
                        }
                        if (opcode == Instruction::BitCast) {
                            const Value *bitcastValue = ce->getOperand(0);
                            string bitcastValueName = bitcastValue->getName().str();
                            /*
                             * value in bitcast:
                             * _ZTIXXX
                             * Function
                             * GlobalAlias (alias to other function)
                             */
                            assert(isa<Function>(bitcastValue) ||
                                   isa<GlobalValue>(bitcastValue));
                            if (const Function *func = dyn_cast<Function>(bitcastValue)) {
                                virtualFunctions.push_back(func);
                                if (func->getName().str().compare(pureVirtualFunName) == 0) {
                                    pure_abstract &= true;
                                } else {
                                    pure_abstract &= false;
                                }
                                struct DemangledName dname = demangle(func->getName().str());
                                if (dname.className.size() > 0 &&
                                        vtblClassName.compare(dname.className) != 0) {
                                    addEdge(vtblClassName, dname.className, CHEdge::INHERITANCE);
                                }
                            } else {
                                if (const GlobalAlias *alias =
                                            dyn_cast<GlobalAlias>(bitcastValue)) {
                                    const Constant *aliasValue = alias->getAliasee();
                                    if (const Function *aliasFunc =
                                                dyn_cast<Function>(aliasValue)) {
                                        virtualFunctions.push_back(aliasFunc);
                                    } else if (const ConstantExpr *aliasconst =
                                                   dyn_cast<ConstantExpr>(aliasValue)) {
                                        u32_t aliasopcode = aliasconst->getOpcode();
                                        assert(aliasopcode == Instruction::BitCast &&
                                               "aliased constantexpr in vtable not a bitcast");
                                        const Function *aliasbitcastfunc =
                                            dyn_cast<Function>(aliasconst->getOperand(0));
                                        assert(aliasbitcastfunc &&
                                               "aliased bitcast in vtable not a function");
                                        virtualFunctions.push_back(aliasbitcastfunc);
                                    } else {
                                        assert(false && "alias not function or bitcast");
                                    }

                                    pure_abstract &= false;
                                } else if (bitcastValueName.compare(0, ztiLabel.size(),
                                                                    ztiLabel) == 0) {
                                } else {
                                    assert("what else can be in bitcast of a vtable?");
                                }
                            }
                        }
                    }
                    if (is_virtual && virtualFunctions.size() > 0) {
                        for (int i = 0; i < null_ptr_num; ++i) {
                            const Function *fun = virtualFunctions[i];
                            virtualFunctions.insert(virtualFunctions.begin(), fun);
                        }
                    }
                    if (virtualFunctions.size() > 0)
                        node->addVirtualFunctionVector(virtualFunctions);
                }
                if (pure_abstract == true) {
                    node->setPureAbstract();
                }
            }
        }
    }
}


void CHGraph::buildVirtualFunctionToIDMap() {
    /*
     * 1. Divide classes into groups
     * 2. Get all virtual functions in a group
     * 3. Assign consecutive IDs to virtual functions that have
     * the same name (after demangling) in a group
     */
    CHNodeSetTy visitedNodes;
    for (CHGraph::const_iterator nit = this->begin(),
            neit = this->end(); nit != neit; ++nit) {
        CHNode *node = nit->second;
        if (visitedNodes.find(node) != visitedNodes.end())
            continue;

        string className = node->getName();

        /*
         * get all the classes in a specific group
         */
        CHNodeSetTy group;
        stack<const CHNode*> nodeStack;
        nodeStack.push(node);
        while (!nodeStack.empty()) {
            const CHNode *curnode = nodeStack.top();
            nodeStack.pop();
            group.insert(curnode);
            if (visitedNodes.find(curnode) != visitedNodes.end())
                continue;
            for (CHEdge::CHEdgeSetTy::const_iterator it = curnode->getOutEdges().begin(),
                    eit = curnode->getOutEdges().end(); it != eit; ++it) {
                CHNode *tmpnode = (*it)->getDstNode();
                nodeStack.push(tmpnode);
                group.insert(tmpnode);
            }
            for (CHEdge::CHEdgeSetTy::const_iterator it = curnode->getInEdges().begin(),
                    eit = curnode->getInEdges().end(); it != eit; ++it) {
                CHNode *tmpnode = (*it)->getSrcNode();
                nodeStack.push(tmpnode);
                group.insert(tmpnode);
            }
            visitedNodes.insert(curnode);
        }

        /*
         * get all virtual functions in a specific group
         */
        set<const Function*> virtualFunctions;
        for (CHNodeSetTy::iterator it = group.begin(),
                eit = group.end(); it != eit; ++it) {
            const vector<CHNode::FuncVector> &vecs = (*it)->getVirtualFunctionVectors();
            for (vector<CHNode::FuncVector>::const_iterator vit = vecs.begin(),
                    veit = vecs.end(); vit != veit; ++vit) {
                for (vector<const Function*>::const_iterator fit = (*vit).begin(),
                        feit = (*vit).end(); fit != feit; ++fit) {
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
        set<pair<string, const Function*>> fNameSet;
        for (set<const Function*>::iterator fit = virtualFunctions.begin(),
                feit = virtualFunctions.end(); fit != feit; ++fit) {
            const Function *f = *fit;
            struct DemangledName dname = demangle(f->getName().str());
            fNameSet.insert(pair<string, const Function*>(dname.funcName, f));
        }
        for (set<pair<string, const Function*>>::iterator it = fNameSet.begin(),
                eit = fNameSet.end(); it != eit; ++it) {
            virtualFunctionToIDMap[it->second] = vfID++;
        }
    }
}

const CHGraph::CHNodeSetTy& CHGraph::getCSClasses(CallSite cs) {
	assert(isVirtualCallSite(cs) && "not virtual callsite!");

	CallSiteToCHNodesMap::const_iterator it = csToClassesMap.find(cs);
	if (it != csToClassesMap.end()) {
		return it->second;
	} else {
		string thisPtrClassName = getClassNameOfThisPtr(cs);
		if (const CHNode* thisNode = getNode(thisPtrClassName)) {
			const CHNodeSetTy& instAndDesces = getInstancesAndDescendants(thisPtrClassName);
			csToClassesMap[cs].insert(thisNode);
			for (CHNodeSetTy::const_iterator it = instAndDesces.begin(), eit = instAndDesces.end(); it != eit; ++it)
				csToClassesMap[cs].insert(*it);
		}
		return csToClassesMap[cs];
	}
}

/*
 * Get virtual functions for callsite "cs" based on vtbls (calculated
 * based on pointsto set)
 */
void CHGraph::getVFnsFromVtbls(llvm::CallSite cs, VTableSet &vtbls, VFunSet &virtualFunctions) const {

    /// get target virtual functions
    size_t idx = getVCallIdx(cs);
    /// get the function name of the virtual callsite
    string funName = getFunNameOfVCallSite(cs);
    for (VTableSet::iterator it = vtbls.begin(), eit = vtbls.end(); it != eit; ++it) {
        const CHNode *child = getNode(getClassNameFromVtblObj(*it));
        if (child == NULL)
            continue;
        CHNode::FuncVector vfns;
        child->getVirtualFunctions(idx, vfns);
        for (CHNode::FuncVector::const_iterator fit = vfns.begin(),
                feit = vfns.end(); fit != feit; ++fit) {
            const Function* callee = *fit;
            if (cs.arg_size() == callee->arg_size() ||
                    (cs.getFunctionType()->isVarArg() && callee->isVarArg())) {
                DemangledName dname = demangle(callee->getName().str());
                string calleeName = dname.funcName;

                /*
                 * The compiler will add some special suffix (e.g.,
                 * "[abi:cxx11]") to the end of some virtual function:
                 * In dealII
                 * function: FE_Q<3>::get_name
                 * will be mangled as: _ZNK4FE_QILi3EE8get_nameB5cxx11Ev
                 * after demangling: FE_Q<3>::get_name[abi:cxx11]
                 * The special suffix ("[abi:cxx11]") needs to be removed
                 */
                const std::string suffix("[abi:cxx11]");
                size_t suffix_pos = calleeName.rfind(suffix);
                if (suffix_pos != string::npos)
                  calleeName.erase(suffix_pos, suffix.size());

                /*
                 * if we can't get the function name of a virtual callsite, all virtual
                 * functions calculated by idx will be valid
                 */
                if (funName.size() == 0) {
                    virtualFunctions.insert(callee);
                } else if (funName[0] == '~') {
                    /*
                     * if the virtual callsite is calling a destructor, then all
                     * destructors in the ch will be valid
                     * class A { virtual ~A(){} };
                     * class B: public A { virtual ~B(){} };
                     * int main() {
                     *   A *a = new B;
                     *   delete a;  /// the function name of this virtual callsite is ~A()
                     * }
                     */
                    if (calleeName[0] == '~') {
                        virtualFunctions.insert(callee);
                    }
                } else {
                    /*
                     * for other virtual function calls, the function name of the callsite
                     * and the function name of the target callee should match exactly
                     */
                    if (funName.compare(calleeName) == 0) {
                        virtualFunctions.insert(callee);
                    }
                }
            }
        }
    }
}

void CHGraph::buildCSToCHAVtblsAndVfnsMap() {

	for (SymbolTableInfo::CallSiteSet::const_iterator it =
			SymbolTableInfo::Symbolnfo()->getCallSiteSet().begin(), eit =
			SymbolTableInfo::Symbolnfo()->getCallSiteSet().end(); it != eit; ++it) {
		CallSite cs = *it;
		if (!cppUtil::isVirtualCallSite(cs))
			continue;
		VTableSet vtbls;
		const CHNodeSetTy& chClasses = getCSClasses(cs);
		for (CHNodeSetTy::const_iterator it = chClasses.begin(), eit = chClasses.end(); it != eit; ++it) {
			const CHNode *child = *it;
			const GlobalValue *vtbl = child->getVTable();
			if (vtbl != NULL) {
				vtbls.insert(vtbl);
			}
		}
		if (vtbls.size() > 0) {
			csToCHAVtblsMap[cs] = vtbls;
			VFunSet virtualFunctions;
			getVFnsFromVtbls(cs, vtbls, virtualFunctions);
			if (virtualFunctions.size() > 0)
				csToCHAVFnsMap[cs] = virtualFunctions;
		}
	}
}

void CHGraph::printCH() {
	for (CHGraph::const_iterator it = this->begin(), eit = this->end();
			it != eit; ++it) {
		const CHNode *node = it->second;
		outs() << "class: " << node->getName() << "\n";
		for (CHEdge::CHEdgeSetTy::const_iterator it = node->OutEdgeBegin();
				it != node->OutEdgeEnd(); ++it) {
			if ((*it)->getEdgeType() == CHEdge::INHERITANCE)
				outs() << (*it)->getDstNode()->getName() << " --inheritance--> "
						<< (*it)->getSrcNode()->getName() << "\n";
			else
				outs() << (*it)->getSrcNode()->getName() << " --instance--> "
						<< (*it)->getDstNode()->getName() << "\n";
		}
	}
	outs() << '\n';
}

/*!
 * Dump call graph into dot file
 */
void CHGraph::dump(const std::string& filename) {
    GraphPrinter::WriteGraphToFile(llvm::outs(), filename, this);
    printCH();
}


namespace llvm {

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<CHGraph*> : public DefaultDOTGraphTraits {

    typedef CHNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(CHGraph *graph) {
        return "Class Hierarchy Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(CHNode *node, CHGraph *graph) {
        return node->getName();
    }

    static std::string getNodeAttributes(CHNode *node, CHGraph *CHGraph) {
        if (node->isPureAbstract()) {
            return "shape=Mcircle";
        } else
            return "shape=circle";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(CHNode *node, EdgeIter EI, CHGraph *CHGraph) {

        CHEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (edge->getEdgeType() == CHEdge::INHERITANCE) {
            return "style=solid";
        } else {
            return "style=dashed";
        }
    }
};
}
