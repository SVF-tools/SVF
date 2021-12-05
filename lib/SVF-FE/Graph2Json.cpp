#include "llvm/Support/JSON.h"

#include "SVF-FE/Graph2Json.h"

#include <fstream>	// for ICFGBuilderFromFile
#include <string>	// for ICFGBuilderFromFile
#include <sstream>	// for ICFGBuilderFromFile

using namespace std;
using namespace SVF;
using namespace SVFUtil;

ICFGPrinter::ICFGPrinter() {}
void ICFGPrinter::printICFGToJson(const std::string& filename)
{
    outs() << "write symbols to '" << filename << "'...";
    std::error_code err;
    ToolOutputFile F(filename.c_str(), err, llvm::sys::fs::OF_None);
    if (err)
    {
        outs() << "  error opening file for writing!\n";
        F.os().clear_error();
        return;
    }

    llvm::json::Array root_array;
    for(ICFG::iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        ICFGNode* node = it->second;
        NodeID id = node->getId();
        llvm::json::Object ICFGNode_Obj;
        ICFGNode_Obj["ICFG_ID"] = id;
        ICFGNode_Obj["Node Type"] = getICFGKind(node->getNodeKind());
        if(IntraBlockNode* bNode = SVFUtil::dyn_cast<IntraBlockNode>(node))
        {
            ICFGNode_Obj["Source Location"] = getSourceLoc(bNode->getInst());
            PAG::PAGEdgeList&  edges = PAG::getPAG()->getInstPTAPAGEdgeList(bNode);
            llvm::json::Array PAGEdge_array;

            //dump pag edges
            for (PAG::PAGEdgeList::iterator it = edges.begin(),
                    eit = edges.end(); it != eit; ++it)
            {
                const PAGEdge* edge = *it;
                llvm::json::Object edge_obj;
                edge_obj["Source Node"] = edge->getSrcID();
                edge_obj["Destination Node"] = edge->getDstID();
                edge_obj["Source Type"] = getPAGNodeKindValue(edge->getSrcNode()->getNodeKind());
                edge_obj["Destination Type"] = getPAGNodeKindValue(edge->getDstNode()->getNodeKind());
                edge_obj["Edge Type"] = getPAGEdgeKindValue(edge->getEdgeKind());
                edge_obj["srcValueName"] = edge->getSrcNode()->getValueName();
                edge_obj["dstValueName"] = edge->getDstNode()->getValueName();
                if(edge->getEdgeKind()==PAGEdge::NormalGep)
                {
                    const NormalGepPE* gepEdge = SVFUtil::cast<NormalGepPE>(edge);
                    edge_obj["offset"] = gepEdge->getOffset();
                }
                llvm::json::Value edge_value = llvm::json::Object{edge_obj};
                PAGEdge_array.push_back(edge_value);
            }
            llvm::json::Value PagEdge_value = llvm::json::Array{PAGEdge_array};
            ICFGNode_Obj["PAG Edges"] = PagEdge_value;
        }
        else if(FunEntryBlockNode* entry = SVFUtil::dyn_cast<FunEntryBlockNode>(node))
        {
            if (isExtCall(entry->getFun()))
                ICFGNode_Obj["isExtCall"] = true;
            else
            {
                ICFGNode_Obj["isExtCall"] = false;
                ICFGNode_Obj["Source Location"] = getSourceLoc(entry->getFun()->getLLVMFun());
            }
            ICFGNode_Obj["Function Name"] = entry->getFun()->getName();
        }
        else if (FunExitBlockNode* exit = SVFUtil::dyn_cast<FunExitBlockNode>(node))
        {
            if (isExtCall(exit->getFun()))
                ICFGNode_Obj["isExtCall"] = true;
            else
            {
                ICFGNode_Obj["isExtCall"] = false;
                ICFGNode_Obj["Source Location"] = getSourceLoc(&(exit->getFun()->getLLVMFun()->back()));
            }
            ICFGNode_Obj["Function Name"] = exit->getFun()->getName();
        }
        else if (CallBlockNode* call = SVFUtil::dyn_cast<CallBlockNode>(node))
        {
            ICFGNode_Obj["Source Location"] = getSourceLoc(call->getCallSite());
        }
        else if (RetBlockNode* ret = SVFUtil::dyn_cast<RetBlockNode>(node))
        {
            ICFGNode_Obj["Source Location"] = getSourceLoc(ret->getCallSite());
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        llvm::json::Array ICFGEdges_array;
        for(ICFGNode::iterator sit = node->OutEdgeBegin(), esit = node->OutEdgeEnd(); sit!=esit; ++sit)
        {
            ICFGEdge* edge = *sit;
            llvm::json::Object ICFGEdge_obj;
            if (SVFUtil::isa<CallCFGEdge>(edge))
            {
                CallCFGEdge* call = SVFUtil::dyn_cast<CallCFGEdge>(edge);
                ICFGEdge_obj["ICFG Edge Type"] = "CallCFGEdge";
                ICFGEdge_obj["ICFGEdgeSrcID"] = call->getSrcID();
                ICFGEdge_obj["ICFGEdgeDstID"] = call->getDstID();
            }
            else if (SVFUtil::isa<RetCFGEdge>(edge))
            {
                RetCFGEdge* call = SVFUtil::dyn_cast<RetCFGEdge>(edge);
                ICFGEdge_obj["ICFG Edge Type"] = "RetCFGEdge";
                ICFGEdge_obj["ICFGEdgeSrcID"] = call->getSrcID();
                ICFGEdge_obj["ICFGEdgeDstID"] = call->getDstID();
            }
            else if(SVFUtil::isa<IntraCFGEdge>(edge))
            {
                IntraCFGEdge* intraCFGEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
                ICFGEdge_obj["ICFG Edge Type"] = "IntraCFGEdge";
                ICFGEdge_obj["ICFGEdgeSrcID"] = intraCFGEdge->getSrcID();
                ICFGEdge_obj["ICFGEdgeDstID"] = intraCFGEdge->getDstID();
            }
            llvm::json::Value ICFGEdge_Val = llvm::json::Object{ICFGEdge_obj};
            ICFGEdges_array.push_back(ICFGEdge_Val);
        }
        llvm::json::Value ICFGEdges_val = llvm::json::Array{ICFGEdges_array};
        ICFGNode_Obj["ICFGEdges"] = ICFGEdges_val;
        llvm::json::Value ICFGNode_val = llvm::json::Object{ICFGNode_Obj};
        root_array.push_back(ICFGNode_val);
    }
    llvm::json::Value root_value = llvm::json::Array{root_array};
    llvm::json::operator<<(F.os(),root_value);

    F.os().close();
    if (!F.os().has_error())
    {
        outs() << "\n";
        F.keep();
        return;
    }
}

std::string ICFGPrinter::getICFGKind(const int kind)
{
    switch(kind)
    {
    case ICFGNode::IntraBlock:
        return "IntraBlock";
        break;
    case ICFGNode::FunEntryBlock:
        return "FunEntryBlock";
        break;
    case ICFGNode::FunExitBlock:
        return "FunExitBlock";
        break;
    case ICFGNode::FunCallBlock:
        return "FunCallBlock";
        break;
    case ICFGNode::FunRetBlock:
        return "FunRetBlock";
        break;
    default:
        return "";
    }
    return "";
}

std::string ICFGPrinter::getPAGNodeKindValue(int kind)
{
    switch (kind)
    {
    case (PAGNode::ValNode):
        return "ValNode";
        break;
    case PAGNode::ObjNode:
        return "ObjNode";
        break;
    case PAGNode::RetNode:
        return "RetNode";
        break;
    case PAGNode::VarargNode:
        return "VarargNode";
        break;
    case PAGNode::GepValNode:
        return "GepValNode";
        break;
    case PAGNode::GepObjNode:
        return "GepObjNode";
        break;
    case PAGNode::FIObjNode:
        return "FIObjNode";
        break;
    case PAGNode::DummyValNode:
        return "DummyValNode";
        break;
    case PAGNode::DummyObjNode:
        return "DummyObjNode";
        break;
    }
    return "";
}

std::string ICFGPrinter::getPAGEdgeKindValue(int kind)
{
    switch(kind)
    {
    case (PAGEdge::Addr):
        return "Addr";
        break;
    case (PAGEdge::Copy):
        return "Copy";
        break;
    case (PAGEdge::Store):
        return "Store";
        break;
    case (PAGEdge::Load):
        return "Load";
        break;
    case (PAGEdge::Call):
        return "Call";
        break;
    case (PAGEdge::Ret):
        return "Ret";
        break;
    case (PAGEdge::NormalGep):
        return "NormalGep";
        break;
    case (PAGEdge::VariantGep):
        return "VariantGep";
        break;
    case (PAGEdge::ThreadFork):
        return "ThreadFork";
        break;
    case (PAGEdge::ThreadJoin):
        return "ThreadJoin";
        break;
    case (PAGEdge::Cmp):
        return "Cmp";
        break;
    case (PAGEdge::BinaryOp):
        return "BinaryOp";
        break;
    case (PAGEdge::UnaryOp):
        return "UnaryOp";
        break;
    }
    return "";
}
