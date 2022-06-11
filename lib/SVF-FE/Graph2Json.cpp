#include "llvm/Support/JSON.h"
#include "MemoryModel/SVFIR.h"
#include "SVF-FE/Graph2Json.h"

#include <fstream>	// for ICFGBuilderFromFile
#include <string>	// for ICFGBuilderFromFile
#include <sstream>	// for ICFGBuilderFromFile

#include <llvm/Support/ToolOutputFile.h>

using namespace std;
using namespace SVF;
using namespace SVFUtil;

ICFGPrinter::ICFGPrinter() {}
void ICFGPrinter::printICFGToJson(const std::string& filename)
{
    outs() << "write symbols to '" << filename << "'...";
    std::error_code err;
    llvm::ToolOutputFile F(filename.c_str(), err, llvm::sys::fs::OF_None);
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
        if(IntraICFGNode* bNode = SVFUtil::dyn_cast<IntraICFGNode>(node))
        {
            ICFGNode_Obj["Source Location"] = getSourceLoc(bNode->getInst());
            SVFIR::SVFStmtList&  edges = SVFIR::getPAG()->getPTASVFStmtList(bNode);
            llvm::json::Array PAGEdge_array;

            //dump pag edges
            for (SVFIR::SVFStmtList::iterator it = edges.begin(),
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
                if(edge->getEdgeKind()==SVFStmt::Gep)
                {
                    const GepStmt* gepEdge = SVFUtil::cast<GepStmt>(edge);
                    edge_obj["offset"] = gepEdge->getConstantFieldIdx();
                }
                llvm::json::Value edge_value = llvm::json::Object{edge_obj};
                PAGEdge_array.push_back(edge_value);
            }
            llvm::json::Value PagEdge_value = llvm::json::Array{PAGEdge_array};
            ICFGNode_Obj["SVFIR Edges"] = PagEdge_value;
        }
        else if(FunEntryICFGNode* entry = SVFUtil::dyn_cast<FunEntryICFGNode>(node))
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
        else if (FunExitICFGNode* exit = SVFUtil::dyn_cast<FunExitICFGNode>(node))
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
        else if (CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            ICFGNode_Obj["Source Location"] = getSourceLoc(call->getCallSite());
        }
        else if (RetICFGNode* ret = SVFUtil::dyn_cast<RetICFGNode>(node))
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
    case (SVFVar::ValNode):
        return "ValNode";
        break;
    case SVFVar::ObjNode:
        return "ObjNode";
        break;
    case SVFVar::RetNode:
        return "RetNode";
        break;
    case SVFVar::VarargNode:
        return "VarargNode";
        break;
    case SVFVar::GepValNode:
        return "GepValNode";
        break;
    case SVFVar::GepObjNode:
        return "GepObjNode";
        break;
    case SVFVar::FIObjNode:
        return "FIObjNode";
        break;
    case SVFVar::DummyValNode:
        return "DummyValNode";
        break;
    case SVFVar::DummyObjNode:
        return "DummyObjNode";
        break;
    }
    return "";
}

std::string ICFGPrinter::getPAGEdgeKindValue(int kind)
{
    switch(kind)
    {
    case (SVFStmt::Addr):
        return "Addr";
        break;
    case (SVFStmt::Copy):
        return "Copy";
        break;
    case (SVFStmt::Store):
        return "Store";
        break;
    case (SVFStmt::Load):
        return "Load";
        break;
    case (SVFStmt::Call):
        return "Call";
        break;
    case (SVFStmt::Ret):
        return "Ret";
        break;
    case (SVFStmt::Gep):
        return "NormalGep";
        break;
    case (SVFStmt::ThreadFork):
        return "ThreadFork";
        break;
    case (SVFStmt::ThreadJoin):
        return "ThreadJoin";
        break;
    case (SVFStmt::Cmp):
        return "Cmp";
        break;
    case (SVFStmt::BinaryOp):
        return "BinaryOp";
        break;
    case (SVFStmt::UnaryOp):
        return "UnaryOp";
        break;
    }
    return "";
}
