//===- ExternalPAG.cpp -- Program assignment graph ---------------------------//

/*
 * ExternalPAG.cpp
 *
 *  Created on: Sep 22, 2018
 *      Author: Mohamad Barbar
 */

#include <iostream>
#include <fstream>
#include <sstream>

#include "MemoryModel/PAG.h"
#include "MemoryModel/ExternalPAG.h"
#include "Util/BasicTypes.h"
#include "Util/SVFUtil.h"
#include "Util/SVFModule.h"

using namespace SVFUtil;

llvm::cl::list<std::string> ExternalPAGArgs("extpags",
                                              llvm::cl::desc("ExternalPAGs to use during PAG construction (format: func1@/path/to/graph,func2@/foo,..."),
                                              llvm::cl::CommaSeparated);

std::vector<std::pair<std::string, std::string>>
    ExternalPAG::parseExternalPAGs(llvm::cl::list<std::string> &extpagsArgs) {
    std::vector<std::pair<std::string, std::string>> parsedExternalPAGs;
    for (auto arg = extpagsArgs.begin(); arg != extpagsArgs.end();
         ++arg) {
        std::stringstream ss(*arg);
        std::string functionName;
        getline(ss, functionName, '@');
        std::string path;
        getline(ss, path);
        parsedExternalPAGs.push_back(
            std::pair<std::string, std::string>(functionName, path));
    }

    return parsedExternalPAGs;
}

void ExternalPAG::initialise(SVFModule svfModule) {
    PAG *pag = PAG::getPAG();

    std::vector<std::pair<std::string, std::string>> parsedExternalPAGs
        = ExternalPAG::parseExternalPAGs(ExternalPAGArgs);

    // Build ext PAGs (and add them) first to use them in PAG construction.
    for (auto extpagPair= parsedExternalPAGs.begin();
         extpagPair != parsedExternalPAGs.end(); ++extpagPair) {
        std::string fname = extpagPair->first;
        std::string path = extpagPair->second;

        ExternalPAG extpag = ExternalPAG(fname);
        extpag.readFromFile(path);
        extpag.addExternalPAG(svfModule.getFunction(fname));
    }
}

bool ExternalPAG::connectCallsiteToExternalPAG(CallSite *cs) {
    PAG *pag = PAG::getPAG();

    Function *function = cs->getCalledFunction();
    std::string functionName = function->getName();
    if (!pag->hasExternalPAG(function)) return false;

    std::map<int, PAGNode*> argNodes =
        pag->getFunctionToExternalPAGEntriesMap()[function];
    PAGNode *retNode = pag->getFunctionToExternalPAGReturnNodes()[function];

    // Handle the return.
    if (llvm::isa<PointerType>(cs->getType())) {
        NodeID dstrec = pag->getValueNode(cs->getInstruction());
        // Does it actually return a pointer?
        if (SVFUtil::isa<PointerType>(function->getReturnType())) {
            if (retNode != NULL) {
                pag->addRetEdge(retNode->getId(), dstrec, cs->getInstruction());
            }
        } else {
            // This is a int2ptr cast during parameter passing
            pag->addBlackHoleAddrEdge(dstrec);
        }
    }

    // Handle the arguments;
    // Actual arguments.
    CallSite::arg_iterator itA = cs->arg_begin(), ieA = cs->arg_end();
    Function::const_arg_iterator itF = function->arg_begin(),
                                 ieF = function->arg_end();
    // Formal arguments.
    size_t formalNodeIndex = 0;

    for (; itF != ieF ; ++itA, ++itF, ++formalNodeIndex) {
        if (itA == ieA) {
            // When unneeded args are left empty, e.g. Linux kernel.
            break;
        }

        // Formal arg node is from the extpag, actual arg node would come from
        // the main pag.
        PAGNode *formalArgNode = argNodes[formalNodeIndex];
        NodeID actualArgNodeId = pag->getValueNode(*itA);

        const llvm::Value *formalArg = &*itF;
        if (!SVFUtil::isa<PointerType>(formalArg->getType())) continue;

        if (SVFUtil::isa<PointerType>((*itA)->getType())) {
            pag->addCallEdge(actualArgNodeId, formalArgNode->getId(),
                             cs->getInstruction());
        } else {
            // This is a int2ptr cast during parameter passing
            pag->addFormalParamBlackHoleAddrEdge(formalArgNode->getId(), &*itF);
        }
        // TODO proofread.
    }

    return false;
}

bool ExternalPAG::addExternalPAG(Function *function) {
    // The function does not exist in the module - bad arg?
    // TODO: maybe some warning?
    if (function == NULL) return false;

    PAG *pag = PAG::getPAG();
    if (pag->hasExternalPAG(function)) return false;

    outs() << "Adding extpag " << this->getFunctionName() << "\n";
    // Temporarily trick SVF Module into thinking we are reading from
    // file to avoid setting BBs/Values (as these nodes don't have any).
    std::string oldSVFModuleFileName = SVFModule::pagFileName();
    SVFModule::setPagFromTXT("tmp");

    // We need: the new nodes.
    //        : the new edges.
    //        : to map function names to the entry nodes.
    //        : to map function names to the return node.

    // To create the new edges.
    std::map<NodeID, PAGNode *> extToNewNodes;

    // Add the value nodes.
    for (auto extNodeIt = this->getValueNodes().begin();
         extNodeIt != this->getValueNodes().end(); ++extNodeIt) {
        NodeID newNodeId = pag->addDummyValNode();
        extToNewNodes[*extNodeIt] = pag->getPAGNode(newNodeId);
    }

    // Add the object nodes.
    for (auto extNodeIt = this->getObjectNodes().begin();
         extNodeIt != this->getObjectNodes().end(); ++extNodeIt) {
        // TODO: fix obj node - there's more to it?
        NodeID newNodeId = pag->addDummyObjNode();
        extToNewNodes[*extNodeIt] = pag->getPAGNode(newNodeId);
    }

    // Add the edges.
    for (auto extEdgeIt = this->getEdges().begin();
         extEdgeIt != this->getEdges().end(); ++extEdgeIt) {
        NodeID extSrcId = std::get<0>(*extEdgeIt);
        NodeID extDstId = std::get<1>(*extEdgeIt);
        std::string extEdgeType = std::get<2>(*extEdgeIt);
        int extOffsetOrCSId = std::get<3>(*extEdgeIt);

        PAGNode *srcNode = extToNewNodes[extSrcId];
        PAGNode *dstNode = extToNewNodes[extDstId];
        NodeID srcId = srcNode->getId();
        NodeID dstId = dstNode->getId();

        if (extEdgeType == "addr") {
            pag->addAddrEdge(srcId, dstId);
        } else if (extEdgeType == "copy") {
            pag->addCopyEdge(srcId, dstId);
        } else if (extEdgeType == "load") {
            pag->addLoadEdge(srcId, dstId);
        } else if (extEdgeType == "store") {
            pag->addStoreEdge(srcId, dstId);
        } else if (extEdgeType == "gep") {
            pag->addNormalGepEdge(srcId, dstId, LocationSet(extOffsetOrCSId));
        } else if (extEdgeType == "variant-gep") {
            pag->addVariantGepEdge(srcId, dstId);
        } else if (extEdgeType == "call") {
            pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, NULL));
        } else if (extEdgeType == "ret") {
            pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, NULL));
        } else {
            outs() << "Bad edge type found during extpag addition\n";
        }
    }

    // Record the arg nodes.
    std::map<int, PAGNode *> argNodes;
    for (auto argNodeIt = this->getArgNodes().begin();
         argNodeIt != this->getArgNodes().end(); ++argNodeIt) {
        int index = argNodeIt->first;
        NodeID extNodeId = argNodeIt->second;
        argNodes[index] = extToNewNodes[extNodeId];
    }

    pag->getFunctionToExternalPAGEntriesMap()[function] = argNodes;

    // Record the return node.
    if (this->hasReturnNode()) {
        pag->getFunctionToExternalPAGReturnNodes()[function] =
            extToNewNodes[this->getReturnNode()];
    }

    // Put it back as if nothing happened.
    SVFModule::setPagFromTXT(oldSVFModuleFileName);
}

// Very similar implementation to the PAGBuilderFromFile.
void ExternalPAG::readFromFile(std::string filename) {
    std::string line;
    std::ifstream pagFile(filename.c_str());

    if (!pagFile.is_open()) {
        outs() << "ExternalPAG::buildFromFile: could not open " << filename
               << "\n";
        return;
    }

    while (pagFile.good()) {
        std::getline(pagFile, line);

        Size_t tokenCount = 0;
        std::string tmps;
        std::istringstream ss(line);
        while (ss.good()) {
            ss >> tmps;
            tokenCount++;
        }

        if (tokenCount == 0) {
            // Empty line.
            continue;
        }

        if (tokenCount == 2 || tokenCount == 3) {
            // It's a node.
            NodeID nodeId;
            std::string nodeType;
            std::istringstream ss(line);
            ss >> nodeId;
            ss >> nodeType;

            if (nodeType == "v") {
                valueNodes.insert(nodeId);
            } else if (nodeType == "o") {
                objectNodes.insert(nodeId);
            } else {
                assert(false
                       && "format not supported, please specify node type");
            }


            if (tokenCount == 3) {
                // If there are 3 tokens, it should be 0, 1, 2, ... or ret.
                std::string argNoOrRet;
                ss >> argNoOrRet;

                if (argNoOrRet == "ret") {
                    returnNode = nodeId;
                } else if (std::all_of(argNoOrRet.begin(), argNoOrRet.end(),
                                       ::isdigit)) {
                    int argNo = std::stoi(argNoOrRet);
                    argNodes[argNo] = nodeId;
                } else {
                    assert(false
                           && "format not supported, not arg number or ret");
                }

            }
        } else if (tokenCount == 4) {
            // It's an edge
            NodeID nodeSrc;
            NodeID nodeDst;
            Size_t offsetOrCSId;
            std::string edge;
            std::istringstream ss(line);
            ss >> nodeSrc;
            ss >> edge;
            ss >> nodeDst;
            ss >> offsetOrCSId;

            edges.insert(
                std::tuple<NodeID, NodeID, std::string, int>(nodeSrc, nodeDst,
                                                             edge,
                                                             offsetOrCSId));
        } else {
            if (!line.empty()) {
                outs() << "format not supported, token count = "
                        << tokenCount << "\n";
                assert(false && "format not supported");
            }
        }
    }

    /*
    TODO: might need to include elsewhere.
    /// new gep node's id from lower bound, nodeNum may not reflect the total nodes
    u32_t lower_bound = 1000;
    for(u32_t i = 0; i < lower_bound; i++)
        pag->incNodeNum();
    */
}

