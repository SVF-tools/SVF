/*
 * Annotator.h
 *
 *  Created on: March 22, 2021
 *      Author: Nicholas Tsom
 */

#ifndef IRANNOTATOR_H_
#define IRANNOTATOR_H_

#include "MemoryModel/SVFIR.h"
#include "MemoryModel/PointerAnalysisImpl.h"
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
        ptsTo(nullptr)
    {}
    ~IRAnnotator() {}

    void processAndersenResults(SVFIR *pag, BVDataPTAImpl *ptsTo, bool writeFlag)
    {
        this->ptsTo = ptsTo;
        mainModule = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();

        // Add a named metadata node used to check whether or not
        // this IR has been annotated with Andersen information
        if (writeFlag)
            mainModule->getOrInsertNamedMetadata("SVFIR-Annotated");

        for (auto it = pag->begin(); it != pag->end(); ++it)
        {
            auto nodeId = it->first;
            auto pagNode = it->second;
            auto gepNode = SVFUtil::dyn_cast<GepObjVar>(pagNode);

            if (gepNode && writeFlag)
            {
                writePAGgepNode(nodeId, gepNode);
            }
            else if (pagNode->hasValue())
            {
                processPAGNode(pagNode->getValue(), nodeId, writeFlag);
            }
        }

        if (!writeFlag)
            readPAGgepNodes(pag);
    }

private:
    // Write the PAGgepNode to the IR such that metadata name is the SVFIR node id and the operands
    // are its base node's id and location offset
    void writePAGgepNode(SVF::NodeID nodeId, GepObjVar* gepNode)
    {
        auto baseNodeId = gepNode->getBaseNode();
        auto locationSetOffset = gepNode->getConstantFieldIdx();

        LLVMContext &context = mainModule->getContext();
        llvm::SmallVector<llvm::Metadata *, 32> operands;
        operands.push_back(llvm::MDString::get(context, std::to_string(baseNodeId)));
        operands.push_back(llvm::MDString::get(context, std::to_string(locationSetOffset)));

        llvm::MDTuple *metadata = llvm::MDTuple::get(context, operands);
        std::string label = "gepnode-" + std::to_string(nodeId);

        mainModule->getOrInsertNamedMetadata(label)->addOperand(metadata);
    }

    // Reads the PAGGepNodes in the annotated IR and creates a new SVFIR Node based on the
    // data contained in the operands of the metadata node
    void readPAGgepNodes(SVFIR *pag)
    {
        for (auto it = mainModule->named_metadata_begin(); it != mainModule->named_metadata_end(); ++it)
        {
            std::string label = it->getName().str();
            std::string toErase = "gepnode-";
            std::size_t pos = label.find(toErase);
            if (pos == std::string::npos)
            {
                continue;
            }
            label.erase(pos, toErase.length());

            SVF::NodeID nodeId = std::stoi(label);
            auto mdNode = it->getOperand(0);

            SVF::NodeID baseNodeId = std::stoi(llvm::cast<llvm::MDString>(mdNode->getOperand(0))->getString().str());
            SVF::s32_t locationSetOffset = std::stoi(llvm::cast<llvm::MDString>(mdNode->getOperand(1))->getString().str());

            LocationSet locationSet = LocationSet(locationSetOffset);
            SVF::NodeID gepnodeId = pag->getGepObjVar(baseNodeId, locationSet);

            assert(nodeId == gepnodeId && "nodeId != gepnodeId");
        }
    }

    // Deduce the LLVM value's type, and process the metadata accordingly
    void processPAGNode(const Value *value, SVF::NodeID nodeId, bool writeFlag)
    {
        if (auto instruction = const_cast<Instruction *>(SVFUtil::dyn_cast<Instruction>(value)))
        {
            processInstructionNode(instruction, nodeId, writeFlag);
        }
        else if (auto argument = const_cast<Argument *>(SVFUtil::dyn_cast<Argument>(value)))
        {
            processArgumentNode(argument, nodeId, writeFlag);
        }
        else if (auto function = const_cast<Function *>(SVFUtil::dyn_cast<Function>(value)))
        {
            processFunctionNode(function, nodeId, writeFlag);
        }
        else if (auto globalVar = const_cast<GlobalVariable *>(SVFUtil::dyn_cast<GlobalVariable>(value)))
        {
            processGlobalVarNode(globalVar, nodeId, writeFlag);
        }
        else if (auto basicBlock = const_cast<BasicBlock *>(SVFUtil::dyn_cast<BasicBlock>(value)))
        {
            processBasicBlockNode(basicBlock, nodeId, writeFlag);
        }
        else if (SVFUtil::isa<Constant>(value))
        {
            processConstantNode(nodeId, writeFlag);
        }
        else if (auto inlineAsm = const_cast<llvm::InlineAsm *>(SVFUtil::dyn_cast<llvm::InlineAsm>(value)))
        {
            processInlineAsmNode(inlineAsm, nodeId, writeFlag);
        }
        else
        {
            SVFUtil::outs() << "Value is NOT a Instruction, Argument, Function, GlobalVariable, BasicBlock, Constant or InlineAsm" << std::endl;
            SVFUtil::outs() << SVFUtil::value2String(value) << "\n";
        }
    }

    void processInstructionNode(Instruction *instruction, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "inode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = instruction->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            instruction->setMetadata(label, mdNodePts);
        }
        else
        {
            auto mdNode = instruction->getMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at instruction");
            addAndersenMetadata(nodeId, mdNode);
        }
    }

    void processArgumentNode(Argument *argument, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "anode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = argument->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            argument->getParent()->setMetadata(label, mdNodePts);
        }
        else
        {
            auto mdNode = argument->getParent()->getMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at argument");
            addAndersenMetadata(nodeId, mdNode);
        }
    }

    void processFunctionNode(Function *function, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "fnode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = function->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            function->setMetadata(label, mdNodePts);
        }
        else
        {
            auto mdNode = function->getMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at function");
            addAndersenMetadata(nodeId, mdNode);
        }
    }

    void processGlobalVarNode(GlobalVariable *globalVar, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "gnode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = globalVar->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            globalVar->setMetadata(label, mdNodePts);
        }
        else
        {
            auto mdNode = globalVar->getMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at instruction");
            addAndersenMetadata(nodeId, mdNode);
        }
    }

    void processBasicBlockNode(BasicBlock *basicBlock, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "bnode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = basicBlock->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            basicBlock->getParent()->setMetadata(label, mdNodePts);
        }
        else
        {
            auto mdNode = basicBlock->getParent()->getMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at basicBlock");
            addAndersenMetadata(nodeId, mdNode);
        }
    }

    void processConstantNode(const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "cnode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = mainModule->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            mainModule->getOrInsertNamedMetadata(label)->addOperand(mdNodePts);
        }
        else
        {
            auto mdNode = mainModule->getNamedMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at instruction");
            addAndersenNamedMetdata(nodeId, mdNode);
        }
    }

    void processInlineAsmNode(llvm::InlineAsm *inlineAsm, const SVF::NodeID &nodeId, bool writeFlag)
    {
        std::string label = "iAsmnode-" + std::to_string(nodeId);

        if (writeFlag)
        {
            LLVMContext &context = mainModule->getContext();
            auto mdNodePts = getMdNodePts(nodeId, context);
            mainModule->getOrInsertNamedMetadata(label)->addOperand(mdNodePts);
        }
        else
        {
            auto mdNode = mainModule->getNamedMetadata(label);
            assert(mdNode != nullptr && "Failed to retrive valid node id at inlineAsm");
            addAndersenNamedMetdata(nodeId, mdNode);
        }
    }

    void addAndersenMetadata(const SVF::NodeID &nodeId, MDNode *mdNode)
    {
        if (mdNode)
        {
            for (unsigned int i = 0; i < mdNode->getNumOperands(); i++)
            {
                auto anderNodeIdStr = llvm::cast<llvm::MDString>(mdNode->getOperand(i))->getString().str();
                auto anderNodeId = SVF::NodeID(std::stoul(anderNodeIdStr));
                ptsTo->addPts(nodeId, anderNodeId);
            }
        }
    }

    void addAndersenNamedMetdata(const SVF::NodeID &nodeId, NamedMDNode *mdNode)
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
                    ptsTo->addPts(nodeId, anderNodeId);
                }
            }
        }
    }

    inline llvm::MDTuple* getMdNodePts(const SVF::NodeID &nodeId, LLVMContext &context)
    {
        llvm::SmallVector<llvm::Metadata *, 32> operands;
        auto &ptTo = ptsTo->getPts(nodeId);
        for (const auto p : ptTo)
        {
            operands.push_back(llvm::MDString::get(context, std::to_string(p)));
        }
        return llvm::MDTuple::get(context, operands);
    }

    Module* mainModule;
    BVDataPTAImpl *ptsTo;

};

} // End namespace SVF

#endif /* IRANNOTATOR_H_ */
