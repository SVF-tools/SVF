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

#include "Graphs/PAG.h"
#include "Graphs/ExternalPAG.h"
#include "Util/BasicTypes.h"
#include "Graphs/ICFG.h"

using namespace SVF;
using namespace SVFUtil;

llvm::cl::list<std::string> ExternalPAGArgs("extpags",
        llvm::cl::desc("ExternalPAGs to use during PAG construction (format: func1@/path/to/graph,func2@/foo,..."),
        llvm::cl::CommaSeparated);

llvm::cl::list<std::string> DumpPAGFunctions("dump-function-pags",
        llvm::cl::desc("Dump PAG for functions"),
        llvm::cl::CommaSeparated);


Map<const SVFFunction*, Map<int, PAGNode *>>
        ExternalPAG::functionToExternalPAGEntries;
Map<const SVFFunction*, PAGNode *> ExternalPAG::functionToExternalPAGReturns;

std::vector<std::pair<std::string, std::string>>
        ExternalPAG::parseExternalPAGs(llvm::cl::list<std::string> &extpagsArgs)
{
    std::vector<std::pair<std::string, std::string>> parsedExternalPAGs;
    for (auto arg = extpagsArgs.begin(); arg != extpagsArgs.end();
            ++arg)
    {
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

void ExternalPAG::initialise(SVFModule*)
{
    std::vector<std::pair<std::string, std::string>> parsedExternalPAGs
            = ExternalPAG::parseExternalPAGs(ExternalPAGArgs);

    // Build ext PAGs (and add them) first to use them in PAG construction.
    for (auto extpagPair= parsedExternalPAGs.begin();
            extpagPair != parsedExternalPAGs.end(); ++extpagPair)
    {
        std::string fname = extpagPair->first;
        std::string path = extpagPair->second;

        ExternalPAG extpag = ExternalPAG(fname);
        extpag.readFromFile(path);
        extpag.addExternalPAG(getFunction(fname));
    }
}

bool ExternalPAG::connectCallsiteToExternalPAG(CallSite *cs)
{
    PAG *pag = PAG::getPAG();

    Function* function = cs->getCalledFunction();
    std::string functionName = function->getName();
    const SVFFunction* svfFun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(function);
    if (!hasExternalPAG(svfFun)) return false;

    Map<int, PAGNode*> argNodes =
        functionToExternalPAGEntries[svfFun];
    PAGNode *retNode = functionToExternalPAGReturns[svfFun];

    // Handle the return.
    if (llvm::isa<PointerType>(cs->getType()))
    {
        NodeID dstrec = pag->getValueNode(cs->getInstruction());
        // Does it actually return a pointer?
        if (SVFUtil::isa<PointerType>(function->getReturnType()))
        {
            if (retNode != nullptr)
            {
                CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(cs->getInstruction());
                pag->addRetPE(retNode->getId(), dstrec, icfgNode);
            }
        }
        else
        {
            // This is a int2ptr cast during parameter passing
            pag->addBlackHoleAddrPE(dstrec);
        }
    }

    // Handle the arguments;
    // Actual arguments.
    CallSite::arg_iterator itA = cs->arg_begin(), ieA = cs->arg_end();
    Function::const_arg_iterator itF = function->arg_begin(),
                                 ieF = function->arg_end();
    // Formal arguments.
    size_t formalNodeIndex = 0;

    for (; itF != ieF ; ++itA, ++itF, ++formalNodeIndex)
    {
        if (itA == ieA)
        {
            // When unneeded args are left empty, e.g. Linux kernel.
            break;
        }

        // Formal arg node is from the extpag, actual arg node would come from
        // the main pag.
        PAGNode *formalArgNode = argNodes[formalNodeIndex];
        NodeID actualArgNodeId = pag->getValueNode(*itA);

        const llvm::Value *formalArg = &*itF;
        if (!SVFUtil::isa<PointerType>(formalArg->getType())) continue;

        if (SVFUtil::isa<PointerType>((*itA)->getType()))
        {
            CallBlockNode* icfgNode = pag->getICFG()->getCallBlockNode(cs->getInstruction());
            pag->addCallPE(actualArgNodeId, formalArgNode->getId(), icfgNode);
        }
        else
        {
            // This is a int2ptr cast during parameter passing
            //addFormalParamBlackHoleAddrEdge(formalArgNode->getId(), &*itF);
            assert(false && "you need to set the current location of this PAGEdge");
            pag->addBlackHoleAddrPE(formalArgNode->getId());
        }
        // TODO proofread.
    }

    return true;
}

bool ExternalPAG::hasExternalPAG(const SVFFunction* function)
{
    bool ret = functionToExternalPAGEntries.find(function)
               != functionToExternalPAGEntries.end();
    return ret;
}

int getArgNo(const SVFFunction* function, const Value *arg)
{
    int argNo = 0;
    for (auto it = function->getLLVMFun()->arg_begin(); it != function->getLLVMFun()->arg_end();
            ++it, ++argNo)
    {
        if (arg->getName() == it->getName()) return argNo;
    }

    return -1;
}

static void outputPAGNodeNoNewLine(raw_ostream &o, PAGNode *pagNode)
{
    o << pagNode->getId() << " ";
    // TODO: is this check enough?
    if (!ObjPN::classof(pagNode)) o << "v";
    else o << "o";
}

static void outputPAGNode(raw_ostream &o, PAGNode *pagNode)
{
    outputPAGNodeNoNewLine(o, pagNode);
    o << "\n";
}

static void outputPAGNode(raw_ostream &o, PAGNode *pagNode, int argno)
{
    outputPAGNodeNoNewLine(o, pagNode);
    o << " " << argno;
    o << "\n";
}

static void outputPAGNode(raw_ostream &o, PAGNode *pagNode,
                          std::string trail)
{
    outputPAGNodeNoNewLine(o, pagNode);
    o << " " << trail;
    o << "\n";
}

static void outputPAGEdge(raw_ostream &o, PAGEdge *pagEdge)
{
    NodeID srcId = pagEdge->getSrcID();
    NodeID dstId = pagEdge->getDstID();
    u32_t offset = 0;
    std::string edgeKind = "-";

    switch (pagEdge->getEdgeKind())
    {
    case PAGEdge::Addr:
        edgeKind = "addr";
        break;
    case PAGEdge::Copy:
        edgeKind = "copy";
        break;
    case PAGEdge::Store:
        edgeKind = "store";
        break;
    case PAGEdge::Load:
        edgeKind = "load";
        break;
    case PAGEdge::Call:
        edgeKind = "call";
        break;
    case PAGEdge::Ret:
        edgeKind = "ret";
        break;
    case PAGEdge::NormalGep:
        edgeKind = "gep";
        break;
    case PAGEdge::VariantGep:
        edgeKind = "variant-gep";
        break;
    case PAGEdge::Cmp:
        edgeKind = "cmp";
        break;
    case PAGEdge::BinaryOp:
        edgeKind = "binary-op";
        break;
    case PAGEdge::UnaryOp:
        edgeKind = "unary-op";
        break;
    case PAGEdge::ThreadFork:
        outs() << "dump-function-pags: found ThreadFork edge.\n";
        break;
    case PAGEdge::ThreadJoin:
        outs() << "dump-function-pags: found ThreadJoin edge.\n";
        break;
    }

    if (NormalGepPE::classof(pagEdge)) offset =
            static_cast<NormalGepPE *>(pagEdge)->getOffset();

    o << srcId << " " << edgeKind << " " << dstId << " " << offset << "\n";
}


/*!
 * Dump PAGs for the functions
 */
void ExternalPAG::dumpFunctions(std::vector<std::string> functions)
{
    PAG *pag = PAG::getPAG();

    // Naive: first map functions to entries in PAG, then dump them.
    Map<const SVFFunction*, std::vector<PAGNode *>> functionToPAGNodes;

    Set<PAGNode *> callDsts;
    for (PAG::iterator it = pag->begin(); it != pag->end(); ++it)
    {
        PAGNode *currNode = it->second;
        if (!currNode->hasOutgoingEdges(PAGEdge::PEDGEK::Call)) continue;

        // Where are these calls going?
        for (PAGEdge::PAGEdgeSetTy::iterator it =
                    currNode->getOutgoingEdgesBegin(PAGEdge::PEDGEK::Call);
                it != currNode->getOutgoingEdgesEnd(PAGEdge::PEDGEK::Call); ++it)
        {
            CallPE *callEdge = static_cast<CallPE *>(*it);
            const Instruction *inst = callEdge->getCallInst()->getCallSite();
            :: Function* currFunction =
                static_cast<const CallInst *>(inst)->getCalledFunction();

            if (currFunction != nullptr)
            {
                // Otherwise, it would be an indirect call which we don't want.
                std::string currFunctionName = currFunction->getName();

                if (std::find(functions.begin(), functions.end(),
                              currFunctionName) != functions.end())
                {
                    // If the dst has already been added, we'd be duplicating
                    // due to multiple actual->arg call edges.
                    if (callDsts.find(callEdge->getDstNode()) == callDsts.end())
                    {
                        callDsts.insert(callEdge->getDstNode());
                        const SVFFunction* svfFun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(currFunction);
                        functionToPAGNodes[svfFun].push_back(callEdge->getDstNode());
                    }
                }
            }
        }
    }

    for (auto it = functionToPAGNodes.begin(); it != functionToPAGNodes.end();
            ++it)
    {
        const SVFFunction* function = it->first;
        std::string functionName = it->first->getName();

        // The final nodes and edges we will print.
        Set<PAGNode *> nodes;
        Set<PAGEdge *> edges;
        // The search stack.
        std::stack<PAGNode *> todoNodes;
        // The arguments to the function.
        std::vector<PAGNode *> argNodes = it->second;
        PAGNode *retNode = nullptr;


        outs() << "PAG for function: " << functionName << "\n";
        for (auto node = argNodes.begin(); node != argNodes.end(); ++node)
        {
            todoNodes.push(*node);
        }

        while (!todoNodes.empty())
        {
            PAGNode *currNode = todoNodes.top();
            todoNodes.pop();

            // If the node has been dealt with, ignore it.
            if (nodes.find(currNode) != nodes.end()) continue;
            nodes.insert(currNode);

            // Return signifies the end of a path.
            if (RetPN::classof(currNode))
            {
                retNode = currNode;
                continue;
            }

            auto outEdges = currNode->getOutEdges();
            for (auto outEdge = outEdges.begin(); outEdge != outEdges.end();
                    ++outEdge)
            {
                edges.insert(*outEdge);
                todoNodes.push((*outEdge)->getDstNode());
            }
        }

        for (auto node = nodes.begin(); node != nodes.end(); ++node)
        {
            // TODO: proper file.
            // Argument nodes use extra information: it's argument number.
            if (std::find(argNodes.begin(), argNodes.end(), *node)
                    != argNodes.end())
            {
                outputPAGNode(outs(), *node,
                              getArgNo(function, (*node)->getValue()));
            }
            else if (*node == retNode)
            {
                outputPAGNode(outs(), *node, "ret");
            }
            else
            {
                outputPAGNode(outs(), *node);
            }
        }

        for (auto edge = edges.begin(); edge != edges.end(); ++edge)
        {
            // TODO: proper file.
            outputPAGEdge(outs(), *edge);
        }

        outs() << "PAG for functionName " << functionName << " done\n";
    }
}

bool ExternalPAG::addExternalPAG(const SVFFunction* function)
{
    // The function does not exist in the module - bad arg?
    // TODO: maybe some warning?
    if (function == nullptr) return false;

    PAG *pag = PAG::getPAG();
    if (hasExternalPAG(function)) return false;

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
    Map<NodeID, PAGNode *> extToNewNodes;

    // Add the value nodes.
    for (auto extNodeIt = this->getValueNodes().begin();
            extNodeIt != this->getValueNodes().end(); ++extNodeIt)
    {
        NodeID newNodeId = pag->addDummyValNode();
        extToNewNodes[*extNodeIt] = pag->getPAGNode(newNodeId);
    }

    // Add the object nodes.
    for (auto extNodeIt = this->getObjectNodes().begin();
            extNodeIt != this->getObjectNodes().end(); ++extNodeIt)
    {
        // TODO: fix obj node - there's more to it?
        NodeID newNodeId = pag->addDummyObjNode();
        extToNewNodes[*extNodeIt] = pag->getPAGNode(newNodeId);
    }

    // Add the edges.
    for (auto extEdgeIt = this->getEdges().begin();
            extEdgeIt != this->getEdges().end(); ++extEdgeIt)
    {
        NodeID extSrcId = std::get<0>(*extEdgeIt);
        NodeID extDstId = std::get<1>(*extEdgeIt);
        std::string extEdgeType = std::get<2>(*extEdgeIt);
        int extOffsetOrCSId = std::get<3>(*extEdgeIt);

        PAGNode *srcNode = extToNewNodes[extSrcId];
        PAGNode *dstNode = extToNewNodes[extDstId];
        NodeID srcId = srcNode->getId();
        NodeID dstId = dstNode->getId();

        if (extEdgeType == "addr")
        {
            pag->addAddrPE(srcId, dstId);
        }
        else if (extEdgeType == "copy")
        {
            pag->addCopyPE(srcId, dstId);
        }
        else if (extEdgeType == "load")
        {
            pag->addLoadPE(srcId, dstId);
        }
        else if (extEdgeType == "store")
        {
            pag->addStorePE(srcId, dstId, nullptr);
        }
        else if (extEdgeType == "gep")
        {
            pag->addNormalGepPE(srcId, dstId, LocationSet(extOffsetOrCSId));
        }
        else if (extEdgeType == "variant-gep")
        {
            pag->addVariantGepPE(srcId, dstId);
        }
        else if (extEdgeType == "call")
        {
            pag->addEdge(srcNode, dstNode, new CallPE(srcNode, dstNode, nullptr));
        }
        else if (extEdgeType == "ret")
        {
            pag->addEdge(srcNode, dstNode, new RetPE(srcNode, dstNode, nullptr));
        }
        else if (extEdgeType == "cmp")
        {
            pag->addCmpPE(srcId, dstId);
        }
        else if (extEdgeType == "binary-op")
        {
            pag->addBinaryOPPE(srcId, dstId);
        }
        else if (extEdgeType == "unary-op")
        {
            pag->addUnaryOPPE(srcId, dstId);
        }
        else
        {
            outs() << "Bad edge type found during extpag addition\n";
        }
    }

    // Record the arg nodes.
    Map<int, PAGNode *> argNodes;
    for (auto argNodeIt = this->getArgNodes().begin();
            argNodeIt != this->getArgNodes().end(); ++argNodeIt)
    {
        int index = argNodeIt->first;
        NodeID extNodeId = argNodeIt->second;
        argNodes[index] = extToNewNodes[extNodeId];
    }

    functionToExternalPAGEntries[function] = argNodes;

    // Record the return node.
    if (this->hasReturnNode())
    {
        functionToExternalPAGReturns[function] =
            extToNewNodes[this->getReturnNode()];
    }

    // Put it back as if nothing happened.
    SVFModule::setPagFromTXT(oldSVFModuleFileName);

    return true;
}

// Very similar implementation to the PAGBuilderFromFile.
void ExternalPAG::readFromFile(std::string filename)
{
    std::string line;
    std::ifstream pagFile(filename.c_str());

    if (!pagFile.is_open())
    {
        outs() << "ExternalPAG::buildFromFile: could not open " << filename
               << "\n";
        return;
    }

    while (pagFile.good())
    {
        std::getline(pagFile, line);

        Size_t tokenCount = 0;
        std::string tmps;
        std::istringstream ss(line);
        while (ss.good())
        {
            ss >> tmps;
            tokenCount++;
        }

        if (tokenCount == 0)
        {
            // Empty line.
            continue;
        }

        if (tokenCount == 2 || tokenCount == 3)
        {
            // It's a node.
            NodeID nodeId;
            std::string nodeType;
            std::istringstream ss(line);
            ss >> nodeId;
            ss >> nodeType;

            if (nodeType == "v")
            {
                valueNodes.insert(nodeId);
            }
            else if (nodeType == "o")
            {
                objectNodes.insert(nodeId);
            }
            else
            {
                assert(false
                       && "format not supported, please specify node type");
            }


            if (tokenCount == 3)
            {
                // If there are 3 tokens, it should be 0, 1, 2, ... or ret.
                std::string argNoOrRet;
                ss >> argNoOrRet;

                if (argNoOrRet == "ret")
                {
                    setReturnNode(nodeId);
                }
                else if (std::all_of(argNoOrRet.begin(), argNoOrRet.end(),
                                     ::isdigit))
                {
                    int argNo = std::stoi(argNoOrRet);
                    argNodes[argNo] = nodeId;
                }
                else
                {
                    assert(false
                           && "format not supported, not arg number or ret");
                }

            }
        }
        else if (tokenCount == 4)
        {
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
        }
        else
        {
            if (!line.empty())
            {
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

