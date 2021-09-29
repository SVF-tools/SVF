/*
 * Annotator.h
 *
 *  Created on: March 22, 2021
 *      Author: Nicholas Tsom
 */

#ifndef IRANNOTATOR_H_
#define IRANNOTATOR_H_

#include "Graphs/PAG.h"
#include "WPA/Andersen.h"
#include "Util/SVFUtil.h"
#include "llvm/IR/Metadata.h"


namespace SVF
{
/*!
 * Program annotator to write metadata information on LLVM IR
 */
class IRAnnotator
{
public:
    IRAnnotator(): 
        mainModule(nullptr),
        ander(nullptr)
     {}
    ~IRAnnotator() {}

    void processAndersenResults(PAG *pag, AndersenBase *ander, bool writeFlag)
    {
        this->ander = ander;
        mainModule = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();

        if (writeFlag == true)
        {
            writeAndersenMetadata(pag, ander);
        }
        else
        {
            readAndersenMetadata(pag, ander);
        }
    }

private:
    void writeAndersenMetadata(PAG *pag, AndersenBase *ander)
    {
        mainModule->getOrInsertNamedMetadata("PAG-Annotated");
        LLVMContext &context = mainModule->getContext();

        for (auto it = pag->begin(); it != pag->end(); ++it)
        {
            auto nodeId = it->first;
            auto pagNode = it->second;

            // Write dynamically created nodes use in Andersen analysis
            if (auto gepNode = SVFUtil::dyn_cast<GepObjPN>(pagNode))
            {
                auto baseNodeId = gepNode->getBaseNode();
                auto locationSetOffset = gepNode->getLocationSet().getOffset();

                llvm::SmallVector<llvm::Metadata *, 32> operands;
                operands.push_back(llvm::MDString::get(context, std::to_string(baseNodeId)));
                operands.push_back(llvm::MDString::get(context, std::to_string(locationSetOffset)));

                llvm::MDTuple *metadata = llvm::MDTuple::get(context, operands);
                std::string label = "gepnode-" + std::to_string(nodeId);

                mainModule->getOrInsertNamedMetadata(label)->addOperand(metadata);
            }
            else if (pagNode->hasValue())
            {
                processPAGMetadata(pagNode->getValue(), nodeId, true);
            }
        }
    }

    void readAndersenMetadata(PAG *pag, AndersenBase *ander)
    {
        for (auto it = pag->begin(); it != pag->end(); ++it)
        {
            auto nodeId = it->first;
            auto pagNode = it->second;

            if (pagNode->hasValue())
            {
                processPAGMetadata(pagNode->getValue(), nodeId, false);
            }
        }

        for (auto it = mainModule->named_metadata_begin(); it != mainModule->named_metadata_end(); ++it)
        {
            std::string label = it->getName().str();
            std::string toErase = "gepnode-";
            SVF::Size_t pos = label.find(toErase);
            if (pos == std::string::npos)
            {
                continue;
            }
            label.erase(pos, toErase.length());

            SVF::NodeID nodeId = std::stoi(label);
            auto mdNode = it->getOperand(0);

            SVF::NodeID baseNodeId = std::stoi(llvm::cast<llvm::MDString>(mdNode->getOperand(0))->getString().str());
            SVF::Size_t locationSetOffset = std::stoi(llvm::cast<llvm::MDString>(mdNode->getOperand(1))->getString().str());

            LocationSet locationSet = LocationSet(locationSetOffset);
            SVF::NodeID gepnodeId = pag->getGepObjNode(baseNodeId, locationSet);

            assert(nodeId == gepnodeId && "nodeId != gepnodeId");
        }
    }

    void processPAGMetadata(const SVF::Value *value, const SVF::NodeID nodeId, bool writeFlag)
    {
        if (auto instruction = const_cast<Instruction *>(SVFUtil::dyn_cast<Instruction>(value)))
        {
            if (writeFlag)
                addInstructionMetadata(instruction, nodeId);
            else
                readInstructionMetadata(instruction, nodeId);
        }
        else if (auto argument = const_cast<Argument *>(SVFUtil::dyn_cast<Argument>(value)))
        {
            if (writeFlag)
                addArgumentMetadata(argument, nodeId);
            else
                readArgumentMetadata(argument, nodeId);
        }
        else if (auto function = const_cast<Function *>(SVFUtil::dyn_cast<Function>(value)))
        {
            if (writeFlag)
                addFunctionMetadata(function, nodeId);
            else
                readFunctionMetadata(function, nodeId);
        }
        else if (auto globalVar = const_cast<GlobalVariable *>(SVFUtil::dyn_cast<GlobalVariable>(value)))
        {
            if (writeFlag)
                addGlobalVarMetadata(globalVar, nodeId);
            else
                readGlobalVarMetadata(globalVar, nodeId);
        }
        else if (auto basicBlock = const_cast<BasicBlock *>(SVFUtil::dyn_cast<BasicBlock>(value)))
        {
            if (writeFlag)
                addBasicBlockMetadata(basicBlock, nodeId);
            else
                readBasicBlockMetadata(basicBlock, nodeId);
        }
        else if (auto constant = const_cast<Constant *>(SVFUtil::dyn_cast<Constant>(value)))
        {
            if (writeFlag)
                addConstantMetadata(constant, nodeId);
            else
                readConstantMetadata(nodeId);
        }
        else if (auto inlineAsm = const_cast<llvm::InlineAsm *>(SVFUtil::dyn_cast<llvm::InlineAsm>(value)))
        {
            if (writeFlag)
                addInlineAsmMetadata(inlineAsm, nodeId);
            else
                readInlineAsmMetadata(inlineAsm, nodeId);
        }
        else
        {
            std::cout << "Value is NOT a Instruction, Argument, Function, GlobalVariable, BasicBlock, Constant or InlineAsm" << std::endl;
            SVFUtil::outs() << *value << "\n";
        }
    }

    inline void readInstructionMetadata(Instruction *inst, const SVF::NodeID nodeId)
    {
        std::string label = "inode-" + std::to_string(nodeId);
        auto mdNode = inst->getMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at instruction");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readArgumentMetadata(Argument *arg, const SVF::NodeID nodeId)
    {
        std::string label = "anode-" + std::to_string(nodeId);
        auto mdNode = arg->getParent()->getMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at argument");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readFunctionMetadata(Function *func, const SVF::NodeID nodeId)
    {
        std::string label = "fnode-" + std::to_string(nodeId);
        auto mdNode = func->getMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at function");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readConstantMetadata(const SVF::NodeID nodeId)
    {
        std::string label = "cnode-" + std::to_string(nodeId);
        auto mdNode = mainModule->getNamedMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at function");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readGlobalVarMetadata(GlobalVariable *gvar, const SVF::NodeID &nodeId)
    {
        std::string label = "gnode-" + std::to_string(nodeId);
        auto mdNode = gvar->getMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at function");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readBasicBlockMetadata(BasicBlock *bb, const SVF::NodeID &nodeId)
    {
        std::string label = "bnode-" + std::to_string(nodeId);
        auto mdNode = bb->getParent()->getMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at function");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void readInlineAsmMetadata(llvm::InlineAsm *inlineAsm, const SVF::NodeID nodeId)
    {
        std::string label = "iAsmnode-" + std::to_string(nodeId);
        auto mdNode = mainModule->getNamedMetadata(label);
        assert(mdNode != nullptr && "Failed to retrive valid node id at function");
        addPtsToInfo(nodeId, mdNode);
    }

    inline void addInstructionMetadata(Instruction *instruction, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = instruction->getContext();
        std::string label = "inode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        instruction->setMetadata(label, mdNodePts);
    }

    inline void addArgumentMetadata(Argument *arguments, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = arguments->getContext();
        std::string label = "anode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        arguments->getParent()->setMetadata(label, mdNodePts);
    }

    inline void addFunctionMetadata(Function *function, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = function->getContext();
        std::string label = "fnode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        function->setMetadata(label, mdNodePts);
    }

    inline void addConstantMetadata(const Constant *value, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = value->getContext();
        std::string label = "cnode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        mainModule->getOrInsertNamedMetadata(label)->addOperand(mdNodePts);
    }

    inline void addGlobalVarMetadata(GlobalVariable *globalVar, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = globalVar->getContext();
        std::string label = "gnode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        globalVar->setMetadata(label, mdNodePts);
    }

    inline void addBasicBlockMetadata(BasicBlock *basicBlock, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = basicBlock->getContext();
        std::string label = "bnode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        basicBlock->getParent()->setMetadata(label, mdNodePts);
    }

    inline void addInlineAsmMetadata(llvm::InlineAsm *inlineAsm, const SVF::NodeID &nodeId)
    {
        LLVMContext &context = mainModule->getContext();
        std::string label = "iAsmnode-" + std::to_string(nodeId);
        auto mdNodePts = getMdNodePts(nodeId, context);
        mainModule->getOrInsertNamedMetadata(label)->addOperand(mdNodePts);
    }

    inline llvm::MDTuple* getMdNodePts(const SVF::NodeID &nodeId, LLVMContext &context)
    {
        llvm::SmallVector<llvm::Metadata *, 32> operands;
        auto &ptsTo = ander->getPts(nodeId);
        for (const auto p : ptsTo)
        {
            operands.push_back(llvm::MDString::get(context, std::to_string(p)));
        }
        return llvm::MDTuple::get(context, operands);
    }

    inline void addPtsToInfo(const SVF::NodeID &nodeId, MDNode *mdNode)
    {
        if (mdNode)
        {
            for (unsigned int i = 0; i < mdNode->getNumOperands(); i++)
            {
                auto anderNodeIdStr = llvm::cast<llvm::MDString>(mdNode->getOperand(i))->getString().str();
                auto anderNodeId = SVF::NodeID(std::stoul(anderNodeIdStr));
                ander->addPts(nodeId, anderNodeId);
            }
        }
    }
    inline void addPtsToInfo(const SVF::NodeID &nodeId, NamedMDNode *mdNode)
    {
        if (mdNode)
        {
            for (unsigned int i = 0; i < mdNode->getNumOperands(); i++)
            {
                auto node = mdNode->getOperand(i);

                for (unsigned int j = 0; j < node->getNumOperands(); j++)
                {
                    auto anderNodeIdStr = llvm::cast<llvm::MDString>(node->getOperand(j))->getString().str();
                    auto anderNodeId = SVF::NodeID(std::stoul(anderNodeIdStr));
                    ander->addPts(nodeId, anderNodeId);
                }
            }
        }
    }
    Module* mainModule;
    AndersenBase* ander;
};

} // End namespace SVF

#endif /* IRANNOTATOR_H_ */
