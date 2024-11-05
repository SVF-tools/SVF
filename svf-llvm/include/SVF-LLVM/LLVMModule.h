//===- LLVMModule.h -- LLVM Module class-----------------------------------------//
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
 * LLVMModule.h
 *
 *  Created on: 23 Mar.2020
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVF_FE_LLVMMODULE_H_
#define INCLUDE_SVF_FE_LLVMMODULE_H_

#include "SVF-LLVM/BasicTypes.h"
#include "SVFIR/SVFValue.h"
#include "SVFIR/SVFModule.h"
#include "Util/Options.h"

namespace SVF
{

class SymbolTableInfo;
class ObjTypeInference;

class LLVMModuleSet
{
    friend class SVFIRBuilder;
    friend class ICFGBuilder;

public:

    typedef std::vector<const Function*> FunctionSetType;
    typedef Map<const Function*, const Function*> FunDeclToDefMapTy;
    typedef Map<const Function*, FunctionSetType> FunDefToDeclsMapTy;
    typedef Map<const GlobalVariable*, GlobalVariable*> GlobalDefToRepMapTy;

    typedef Map<const Function*, SVFFunction*> LLVMFun2SVFFunMap;
    typedef Map<const Function*, CallGraphNode*> LLVMFun2CallGraphNodeMap;
    typedef Map<const BasicBlock*, SVFBasicBlock*> LLVMBB2SVFBBMap;
    typedef Map<const Instruction*, SVFInstruction*> LLVMInst2SVFInstMap;
    typedef Map<const Argument*, SVFArgument*> LLVMArgument2SVFArgumentMap;
    typedef Map<const Constant*, SVFConstant*> LLVMConst2SVFConstMap;
    typedef Map<const Value*, SVFOtherValue*> LLVMValue2SVFOtherValueMap;
    typedef Map<const SVFValue*, const Value*> SVFValue2LLVMValueMap;
    typedef Map<const SVFBaseNode*, const Value*> SVFBaseNode2LLVMValueMap;
    typedef Map<const Type*, SVFType*> LLVMType2SVFTypeMap;
    typedef Map<const Type*, StInfo*> Type2TypeInfoMap;
    typedef Map<std::string, std::vector<std::string>> Fun2AnnoMap;

    typedef Map<const Instruction*, CallICFGNode *> CSToCallNodeMapTy;
    typedef Map<const Instruction*, RetICFGNode *> CSToRetNodeMapTy;
    typedef Map<const Instruction*, IntraICFGNode *> InstToBlockNodeMapTy;
    typedef Map<const Function*, FunEntryICFGNode *> FunToFunEntryNodeMapTy;
    typedef Map<const Function*, FunExitICFGNode *> FunToFunExitNodeMapTy;

private:
    static LLVMModuleSet* llvmModuleSet;
    static bool preProcessed;
    SymbolTableInfo* symInfo;
    SVFModule* svfModule; ///< Borrowed from singleton SVFModule::svfModule
    ICFG* icfg;
    std::unique_ptr<LLVMContext> owned_ctx;
    std::vector<std::unique_ptr<Module>> owned_modules;
    std::vector<std::reference_wrapper<Module>> modules;

    /// Record some "sse_" function declarations used in other ext function definition, e.g., svf_ext_foo(), and svf_ext_foo() used in app functions
    FunctionSetType ExtFuncsVec;
    /// Record annotations of function in extapi.bc
    Fun2AnnoMap ExtFun2Annotations;
    /// Global definition to a rep definition map
    GlobalDefToRepMapTy GlobalDefToRepMap;

    LLVMFun2SVFFunMap LLVMFunc2SVFFunc; ///< Map an LLVM Function to an SVF Function
    LLVMFun2CallGraphNodeMap LLVMFunc2CallGraphNode; ///< Map an LLVM Function to an CallGraph Node
    LLVMBB2SVFBBMap LLVMBB2SVFBB;
    LLVMInst2SVFInstMap LLVMInst2SVFInst;
    LLVMArgument2SVFArgumentMap LLVMArgument2SVFArgument;
    LLVMConst2SVFConstMap LLVMConst2SVFConst;
    LLVMValue2SVFOtherValueMap LLVMValue2SVFOtherValue;
    SVFValue2LLVMValueMap SVFValue2LLVMValue;
    LLVMType2SVFTypeMap LLVMType2SVFType;
    Type2TypeInfoMap Type2TypeInfo;
    ObjTypeInference* typeInference;

    SVFBaseNode2LLVMValueMap SVFBaseNode2LLVMValue;
    CSToCallNodeMapTy CSToCallNodeMap; ///< map a callsite to its CallICFGNode
    CSToRetNodeMapTy CSToRetNodeMap; ///< map a callsite to its RetICFGNode
    InstToBlockNodeMapTy InstToBlockNodeMap; ///< map a basic block to its ICFGNode
    FunToFunEntryNodeMapTy FunToFunEntryNodeMap; ///< map a function to its FunExitICFGNode
    FunToFunExitNodeMapTy FunToFunExitNodeMap; ///< map a function to its FunEntryICFGNode
    CallGraph* callgraph;

    /// Constructor
    LLVMModuleSet();

    void build();

public:
    ~LLVMModuleSet();

    static inline LLVMModuleSet* getLLVMModuleSet()
    {
        if (!llvmModuleSet)
            llvmModuleSet = new LLVMModuleSet;
        return llvmModuleSet;
    }

    static void releaseLLVMModuleSet()
    {
        delete llvmModuleSet;
        llvmModuleSet = nullptr;
    }

    // Build an SVF module from a given LLVM Module instance (for use e.g. in a LLVM pass)
    static SVFModule* buildSVFModule(Module& mod);

    // Build an SVF module from the bitcode files provided in `moduleNameVec`
    static SVFModule* buildSVFModule(const std::vector<std::string>& moduleNameVec);

    inline SVFModule* getSVFModule()
    {
        return svfModule;
    }

    static void preProcessBCs(std::vector<std::string>& moduleNameVec);

    u32_t getModuleNum() const
    {
        return modules.size();
    }

    const std::vector<std::reference_wrapper<Module>>& getLLVMModules() const
    {
        return modules;
    }

    Module *getModule(u32_t idx) const
    {
        return &getModuleRef(idx);
    }

    Module &getModuleRef(u32_t idx) const
    {
        assert(idx < getModuleNum() && "Out of range.");
        return modules[idx];
    }

    // Dump modules to files
    void dumpModulesToFile(const std::string& suffix);

    inline void addFunctionMap(const Function* func, SVFFunction* svfFunc)
    {
        LLVMFunc2SVFFunc[func] = svfFunc;
        setValueAttr(func,svfFunc);
    }
    void addFunctionMap(const Function* func, CallGraphNode* svfFunc);

    inline void addBasicBlockMap(const BasicBlock* bb, SVFBasicBlock* svfBB)
    {
        LLVMBB2SVFBB[bb] = svfBB;
        setValueAttr(bb,svfBB);
    }
    inline void addInstructionMap(const Instruction* inst, SVFInstruction* svfInst)
    {
        LLVMInst2SVFInst[inst] = svfInst;
        setValueAttr(inst,svfInst);
    }
    inline void addArgumentMap(const Argument* arg, SVFArgument* svfArg)
    {
        LLVMArgument2SVFArgument[arg] = svfArg;
        setValueAttr(arg,svfArg);
    }
    inline void addGlobalValueMap(const GlobalValue* glob, SVFGlobalValue* svfglob)
    {
        if (auto glob_var = llvm::dyn_cast<llvm::GlobalVariable>(glob);
                hasGlobalRep(glob_var))
        {
            glob = getGlobalRep(glob_var);
        }
        LLVMConst2SVFConst[glob] = svfglob;
        setValueAttr(glob,svfglob);
    }
    inline void addConstantDataMap(const ConstantData* cd, SVFConstantData* svfcd)
    {
        LLVMConst2SVFConst[cd] = svfcd;
        setValueAttr(cd,svfcd);
    }
    inline void addOtherConstantMap(const Constant* cons, SVFConstant* svfcons)
    {
        LLVMConst2SVFConst[cons] = svfcons;
        setValueAttr(cons,svfcons);
    }
    inline void addOtherValueMap(const Value* ov, SVFOtherValue* svfov)
    {
        LLVMValue2SVFOtherValue[ov] = svfov;
        setValueAttr(ov,svfov);
    }

    SVFValue* getSVFValue(const Value* value);

    const Value* getLLVMValue(const SVFValue* value) const
    {
        SVFValue2LLVMValueMap::const_iterator it = SVFValue2LLVMValue.find(value);
        assert(it!=SVFValue2LLVMValue.end() && "can't find corresponding llvm value!");
        return it->second;
    }

    const Value* getLLVMValue(const SVFBaseNode* value) const
    {
        SVFBaseNode2LLVMValueMap ::const_iterator it = SVFBaseNode2LLVMValue.find(value);
        assert(it!=SVFBaseNode2LLVMValue.end() && "can't find corresponding llvm value!");
        return it->second;
    }

    inline SVFFunction* getSVFFunction(const Function* fun) const
    {
        LLVMFun2SVFFunMap::const_iterator it = LLVMFunc2SVFFunc.find(fun);
        assert(it!=LLVMFunc2SVFFunc.end() && "SVF Function not found!");
        return it->second;
    }

    inline CallGraphNode* getCallGraphNode(const Function* fun) const
    {
        LLVMFun2CallGraphNodeMap::const_iterator it = LLVMFunc2CallGraphNode.find(fun);
        assert(it!=LLVMFunc2CallGraphNode.end() && "CallGraph Node not found!");
        return it->second;
    }

    inline SVFBasicBlock* getSVFBasicBlock(const BasicBlock* bb) const
    {
        LLVMBB2SVFBBMap::const_iterator it = LLVMBB2SVFBB.find(bb);
        assert(it!=LLVMBB2SVFBB.end() && "SVF BasicBlock not found!");
        return it->second;
    }

    inline SVFInstruction* getSVFInstruction(const Instruction* inst) const
    {
        LLVMInst2SVFInstMap::const_iterator it = LLVMInst2SVFInst.find(inst);
        assert(it!=LLVMInst2SVFInst.end() && "SVF Instruction not found!");
        return it->second;
    }

    inline SVFArgument* getSVFArgument(const Argument* arg) const
    {
        LLVMArgument2SVFArgumentMap::const_iterator it = LLVMArgument2SVFArgument.find(arg);
        assert(it!=LLVMArgument2SVFArgument.end() && "SVF Argument not found!");
        return it->second;
    }

    inline SVFGlobalValue* getSVFGlobalValue(const GlobalValue* g) const
    {
        if (auto glob_var = llvm::dyn_cast<llvm::GlobalVariable>(g);
                hasGlobalRep(glob_var))
        {
            g = getGlobalRep(glob_var);
        }
        LLVMConst2SVFConstMap::const_iterator it = LLVMConst2SVFConst.find(g);
        assert(it!=LLVMConst2SVFConst.end() && "SVF Global not found!");
        assert(SVFUtil::isa<SVFGlobalValue>(it->second) && "not a SVFGlobal type!");
        return SVFUtil::cast<SVFGlobalValue>(it->second);
    }

    SVFConstantData* getSVFConstantData(const ConstantData* cd);
    SVFConstant* getOtherSVFConstant(const Constant* oc);

    SVFOtherValue* getSVFOtherValue(const Value* ov);

    /// Get the corresponding Function based on its name
    inline const SVFFunction* getSVFFunction(const std::string& name)
    {
        Function* fun = nullptr;

        for (u32_t i = 0; i < llvmModuleSet->getModuleNum(); ++i)
        {
            Module* mod = llvmModuleSet->getModule(i);
            fun = mod->getFunction(name);
            if (fun)
            {
                return llvmModuleSet->getSVFFunction(fun);
            }
        }
        return nullptr;
    }

    ICFGNode* getICFGNode(const Instruction* inst);

    bool hasICFGNode(const Instruction* inst);

    /// get a call node
    CallICFGNode* getCallICFGNode(const Instruction*  cs);
    /// get a return node
    RetICFGNode* getRetICFGNode(const Instruction*  cs);
    /// get a intra node
    IntraICFGNode* getIntraICFGNode(const Instruction* inst);

    /// Add a function entry node
    inline FunEntryICFGNode* getFunEntryICFGNode(const Function*  fun)
    {
        FunEntryICFGNode* b = getFunEntryBlock(fun);
        assert(b && "Function entry not created?");
        return b;
    }
    /// Add a function exit node
    inline FunExitICFGNode* getFunExitICFGNode(const Function*  fun)
    {
        FunExitICFGNode* b = getFunExitBlock(fun);
        assert(b && "Function exit not created?");
        return b;
    }


    /// Global to rep
    bool hasGlobalRep(const GlobalVariable* val) const
    {
        GlobalDefToRepMapTy::const_iterator it = GlobalDefToRepMap.find(val);
        return it != GlobalDefToRepMap.end();
    }

    GlobalVariable *getGlobalRep(const GlobalVariable* val) const
    {
        GlobalDefToRepMapTy::const_iterator it = GlobalDefToRepMap.find(val);
        assert(it != GlobalDefToRepMap.end() && "has no rep?");
        return it->second;
    }

    Module* getMainLLVMModule() const
    {
        assert(!empty() && "empty LLVM module!!");
        for (size_t i = 0; i < getModuleNum(); ++i)
        {
            Module& module = getModuleRef(i);
            if (module.getName().str() != ExtAPI::getExtAPI()->getExtBcPath())
            {
                return &module;
            }
        }
        assert(false && "no main module found!");
        return nullptr;
    }

    LLVMContext& getContext() const
    {
        assert(!empty() && "empty LLVM module!!");
        return getMainLLVMModule()->getContext();
    }

    bool empty() const
    {
        return getModuleNum() == 0;
    }

    /// Get or create SVFType and typeinfo
    SVFType* getSVFType(const Type* T);
    /// Get LLVM Type
    const Type* getLLVMType(const SVFType* T) const;

    ObjTypeInference* getTypeInference();

    inline ICFG* getICFG()
    {
        return icfg;
    }

private:
    /// Create SVFTypes
    SVFType* addSVFTypeInfo(const Type* t);
    /// Collect a type info
    StInfo* collectTypeInfo(const Type* ty);
    /// Collect the struct info and set the number of fields after flattening
    StInfo* collectStructInfo(const StructType* structTy, u32_t& numFields);
    /// Collect the array info
    StInfo* collectArrayInfo(const ArrayType* T);
    /// Collect simple type (non-aggregate) info
    StInfo* collectSimpleTypeInfo(const Type* T);

    std::vector<const Function*> getLLVMGlobalFunctions(const GlobalVariable* global);

    void loadModules(const std::vector<std::string>& moduleNameVec);
    // Loads ExtAPI bitcode file; uses LLVMContext made while loading module bitcode files or from Module
    void loadExtAPIModules();
    void addSVFMain();

    void createSVFDataStructure();
    void createSVFFunction(const Function* func);
    void initSVFFunction();
    void initSVFBasicBlock(const Function* func);
    void initDomTree(SVFFunction* func, const Function* f);
    void setValueAttr(const Value* val, SVFValue* value);
    void setValueAttr(const Value* val, SVFBaseNode* svfBaseNode);
    void buildFunToFunMap();
    void buildGlobalDefToRepMap();
    /// Invoke llvm passes to modify module
    void prePassSchedule();
    void buildSymbolTable() const;
    void collectExtFunAnnotations(const Module* mod);

    /// Get/Add a call node
    inline CallICFGNode* getCallBlock(const Instruction* cs)
    {
        CSToCallNodeMapTy::const_iterator it = CSToCallNodeMap.find(cs);
        if (it == CSToCallNodeMap.end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a return node
    inline RetICFGNode* getRetBlock(const Instruction* cs)
    {
        CSToRetNodeMapTy::const_iterator it = CSToRetNodeMap.find(cs);
        if (it == CSToRetNodeMap.end())
            return nullptr;
        return it->second;
    }

    inline IntraICFGNode* getIntraBlock(const Instruction* inst)
    {
        InstToBlockNodeMapTy::const_iterator it = InstToBlockNodeMap.find(inst);
        if (it == InstToBlockNodeMap.end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a function entry node
    inline FunEntryICFGNode* getFunEntryBlock(const Function* fun)
    {
        FunToFunEntryNodeMapTy::const_iterator it = FunToFunEntryNodeMap.find(fun);
        if (it == FunToFunEntryNodeMap.end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a function exit node
    inline FunExitICFGNode* getFunExitBlock(const Function* fun)
    {
        FunToFunExitNodeMapTy::const_iterator it = FunToFunExitNodeMap.find(fun);
        if (it == FunToFunExitNodeMap.end())
            return nullptr;
        return it->second;
    }
};

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_LLVMMODULE_H_ */