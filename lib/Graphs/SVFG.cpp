//===- SVFG.cpp -- Sparse value-flow graph-----------------------------------//
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
 * SVFG.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#include "Util/SVFModule.h"
#include "SVF-FE/LLVMUtil.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGOPT.h"
#include "Graphs/SVFGStat.h"
#include "Graphs/ICFG.h"
#include "Util/Options.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include <fstream>
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

const NodeBS MRSVFGNode::getDefSVFVars() const {
    return getPointsTo();
}

const std::string MRSVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "MRSVFGNode ID: " << getId();
    return rawstr.str();
}

const std::string FormalINSVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FormalINSVFGNode ID: " << getId() << " {fun: " << getFun()->getName() << "}";
    rawstr << getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() <<
            " = ENCHI(MR_" << getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() << ")\n";
    rawstr << getMRVer()->getMR()->dumpStr() << "\n";
    return rawstr.str();
}

const std::string FormalOUTSVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FormalOUTSVFGNode ID: " << getId() << " {fun: " << getFun()->getName() << "}";
    rawstr << "RETMU(" << getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() << ")\n";
                rawstr  << getMRVer()->getMR()->dumpStr() << "\n";
    return rawstr.str();
}

const std::string ActualINSVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ActualINSVFGNode ID: " << getId() << " at callsite: " <<  *getCallSite()->getCallSite() << " {fun: " << getFun()->getName() << "}";
    rawstr << "CSMU(" << getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() << ")\n";
                rawstr << getMRVer()->getMR()->dumpStr() << "\n";
                rawstr << "CS[" << getSourceLoc(getCallSite()->getCallSite()) << "]";
    return rawstr.str();
}

const std::string ActualOUTSVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ActualOUTSVFGNode ID: " << getId() << " at callsite: " <<  *getCallSite()->getCallSite() << " {fun: " << getFun()->getName() << "}";
    rawstr <<  getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() <<
           " = CSCHI(MR_" << getMRVer()->getMR()->getMRID() << "V_" << getMRVer()->getSSAVersion() << ")\n";
    rawstr << getMRVer()->getMR()->dumpStr() << "\n";
    rawstr << "CS[" << getSourceLoc(getCallSite()->getCallSite()) << "]" ;
    return rawstr.str();
}

const std::string MSSAPHISVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "MSSAPHISVFGNode ID: " << getId() << " {fun: " << getFun()->getName() << "}";
    rawstr << "MR_" << getResVer()->getMR()->getMRID()
           << "V_" << getResVer()->getSSAVersion() << " = PHI(";
    for (MemSSA::PHI::OPVers::const_iterator it = opVerBegin(), eit = opVerEnd();
            it != eit; it++)
        rawstr << "MR_" << it->second->getMR()->getMRID() << "V_" << it->second->getSSAVersion() << ", ";
    rawstr << ")\n";

    rawstr << getResVer()->getMR()->dumpStr();
    rawstr << getSourceLoc(&getICFGNode()->getBB()->back());
    return rawstr.str();
}

const std::string IntraMSSAPHISVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraMSSAPHISVFGNode ID: " << getId() << " {fun: " << getFun()->getName() << "}";
    rawstr << MSSAPHISVFGNode::toString();
    return rawstr.str();
}

const NodeBS DummyVersionPropSVFGNode::getDefSVFVars() const {
    NodeBS nb;
    nb.set(object);
    return nb;
}

const std::string InterMSSAPHISVFGNode::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    if(isFormalINPHI())
        rawstr << "FormalINPHISVFGNode ID: " << getId() << " {fun: " << getFun()->getName() << "}";
    else
        rawstr << "ActualOUTPHISVFGNode ID: " << getId() << " at callsite: " <<  *getCallSite()->getCallSite() << " {fun: " << getFun()->getName() << "}";
    rawstr << MSSAPHISVFGNode::toString();
    return rawstr.str();
}

const std::string IndirectSVFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IndirectSVFGEdge: " << getDstID() << "<--" << getSrcID() << "\n";
    return rawstr.str();
}

const std::string IntraIndSVFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "IntraIndSVFGEdge: " << getDstID() << "<--" << getSrcID() << "\n";
    return rawstr.str();
}

const std::string CallIndSVFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallIndSVFGEdge CallSite ID: " << getCallSiteId() << " ";
    rawstr << getDstID() << "<--" << getSrcID() << "\n";
    return rawstr.str();
}

const std::string RetIndSVFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetIndSVFGEdge CallSite ID: " << getCallSiteId() << " ";
    rawstr << getDstID() << "<--" << getSrcID() << "\n";
    return rawstr.str();
}


const std::string ThreadMHPIndSVFGEdge::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ThreadMHPIndSVFGEdge: " << getDstID() << "<--" << getSrcID() << "\n";
    return rawstr.str();
}


FormalOUTSVFGNode::FormalOUTSVFGNode(NodeID id, const MRVer* mrVer, const FunExitICFGNode* funExit): MRSVFGNode(id, FPOUT)
{
    cpts = mrVer->getMR()->getPointsTo();
    ver = mrVer;
    funExitNode = funExit; 
}

/*!
 * Constructor
 */
SVFG::SVFG(MemSSA* _mssa, VFGK k): VFG(_mssa->getPTA()->getPTACallGraph(),k),mssa(_mssa), pta(mssa->getPTA())
{
    stat = new SVFGStat(this);
}

/*!
 * Memory has been cleaned up at GenericGraph
 */
void SVFG::destroy()
{
    delete stat;
    stat = nullptr;
    clearMSSA();
}

/*!
 * Build SVFG
 * 1) build SVFG nodes
 *    a) statements for top level pointers (PAGEdges)
 *    b) operators of address-taken variables (MSSAPHI and MSSACHI)
 * 2) connect SVFG edges
 *    a) between two statements (PAGEdges)
 *    b) between two memory SSA operators (MSSAPHI MSSAMU and MSSACHI)
 */
void SVFG::buildSVFG()
{
    DBOUT(DGENERAL, outs() << pasMsg("Build Sparse Value-Flow Graph \n"));

    stat->startClk();
    if (!Options::ReadSVFG.empty())
    {       
        readFile(Options::ReadSVFG);
    } 
    else 
    {
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Addr-taken Node\n"));
    stat->ATVFNodeStart();
    addSVFGNodesForAddrTakenVars();
    stat->ATVFNodeEnd();
        DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Indirect Edge\n"));
        stat->indVFEdgeStart();
        connectIndirectSVFGEdges();
        stat->indVFEdgeEnd();
        if (!Options::WriteSVFG.empty())
            writeToFile(Options::WriteSVFG);
    }
}

// Format of file 
// __Nodes__
// SVFGNodeID: <id> >= <node type> >= MVER: {MRVERID: <id> MemRegion: pts{<pts> } MRVERSION: <version> MSSADef: <version>, pts{<pts> }} >= ICFGNodeID: <id>
// __Edges__
// srcSVFGNodeID: <id> => dstSVFGNodeID: <id> >= <edge type> | MVER: {MRVERID: <id> MemRegion: pts{<pts> } MRVERSION: <version> MSSADef: <version>, pts{<pts> }}
void SVFG::writeToFile(const string& filename) 
{
    outs() << "Writing SVFG analysis to '" << filename << "'...";
    error_code err;
    ToolOutputFile F(filename.c_str(), err, llvm::sys::fs::OF_None);
    if (err)
    {
        outs() << "  error opening file for writing!\n";
        F.os().clear_error();
        return;
    }
    F.os() << "__Nodes__\n";
    // Iterate over nodes and write to file
    for(iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const FormalINSVFGNode* formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            //node
            F.os() << "SVFGNodeID: " << nodeId << " >= " << "FormalINSVFGNode";
            F.os() << " >= MVER: {";
            F.os() << *formalIn->getMRVer() << "} >= ICFGNodeID: " << formalIn->getFunEntryNode()->getId() << "\n";
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            //node
            F.os() << "SVFGNodeID: " << nodeId << " >= " << "FormalOUTSVFGNode";
            F.os() << " >= MVER: {";
            F.os() << *formalOut->getMRVer() << "} >= ICFGNodeID: " <<  formalOut->getFunExitNode()->getId() << "\n";
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            //node
            F.os() << "SVFGNodeID: " << nodeId << " >= " << "ActualINSVFGNode";
            F.os() << " >= MVER: {";
            F.os() << *actualIn->getMRVer() << "} >= ICFGNodeID: " << actualIn->getCallSite()->getId() << "\n";
        }
        else if(const ActualOUTSVFGNode* actualOut = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
        {
            //node
            F.os() << "SVFGNodeID: " << nodeId << " >= " << "ActualOUTSVFGNode" << " >= MVER: {";
            F.os()  << *actualOut->getMRVer() << "} >= ICFGNodeID: "  <<  actualOut->getCallSite()->getId() << "\n";
        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            //node
            F.os() << "SVFGNodeID: " << nodeId << " >= " << "PHISVFGNode";
            unordered_map<u32_t,const MRVer*> opvers;
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++)
            {
                opvers.insert(make_pair(it->first, it->second));
            }              
                // opvers
            F.os() << " >= MVER: {";
            F.os() << *phiNode->getResVer();
            F.os() << "} >= ICFGNodeID: " << pag->getICFG()->getBlockICFGNode(&(phiNode->getICFGNode()->getBB()->front()))->getId();
            F.os() << " >= OPVers: {";
            for (auto x: opvers) 
            {
                const MRVer* op = x.second;
                F.os() << "{" << *op << "}" << ",";
            }
            F.os() << "}\n";
        }
    }

    F.os() << "\n\n__Edges__\n";
    // Iterate over edges and write to file
    for(iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const LoadSVFGNode* loadNode = SVFUtil::dyn_cast<LoadSVFGNode>(node))
        {
            // F.os() << nodeId << ">=" << "LoadNode"<< ">=Edges: (";
            MUSet& muSet = mssa->getMUSet(SVFUtil::cast<LoadPE>(loadNode->getPAGEdge()));
            for(MUSet::iterator it = muSet.begin(), eit = muSet.end(); it!=eit; ++it)
            {
                if(LOADMU* mu = SVFUtil::dyn_cast<LOADMU>(*it))
                {
                    NodeID def = getDef(mu->getMRVer());
                    F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= LoadNode | MVER: {"<< *mu->getMRVer() << "}" << "\n";
                }
            }
        }
        else if(const StoreSVFGNode* storeNode = SVFUtil::dyn_cast<StoreSVFGNode>(node))
        {
            CHISet& chiSet = mssa->getCHISet(SVFUtil::cast<StorePE>(storeNode->getPAGEdge()));
            for(CHISet::iterator it = chiSet.begin(), eit = chiSet.end(); it!=eit; ++it)
            {
                if(STORECHI* chi = SVFUtil::dyn_cast<STORECHI>(*it))
                {
                    NodeID def = getDef(chi->getOpVer());
                    F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= StoreNode | MVER: {"<< *chi->getOpVer() << "}" << "\n";
                }
            }
        }
        else if(const FormalINSVFGNode* formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            PTACallGraphEdge::CallInstSet callInstSet;
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(formalIn->getFun(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it)
            {
                const CallBlockNode* cs = *it;
                if(!mssa->hasMU(cs))
                    continue;
                ActualINSVFGNodeSet& actualIns = getActualINSVFGNodes(cs);
                for(ActualINSVFGNodeSet::iterator ait = actualIns.begin(), aeit = actualIns.end(); ait!=aeit; ++ait)
                {
                    const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ait));
                    F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << actualIn->getId() << " >= FormalINSVFGNode" << "\n";
                }
            }
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            PTACallGraphEdge::CallInstSet callInstSet;
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(formalOut->getFun(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it)
            {
                const CallBlockNode* cs = *it;
                if(!mssa->hasCHI(cs))
                    continue;
                ActualOUTSVFGNodeSet& actualOuts = getActualOUTSVFGNodes(cs);
                for(ActualOUTSVFGNodeSet::iterator ait = actualOuts.begin(), aeit = actualOuts.end(); ait!=aeit; ++ait)
                {
                    const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ait));
                    F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << actualOut->getId() << " >= FormalOUTSVFGNode" << "\n";
                }
            }
            NodeID def = getDef(formalOut->getMRVer());
            F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= FormalOUTSVFGNode | intra" << "\n";
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            NodeID def = getDef(actualIn->getMRVer());
            F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= ActualINSVFGNode" << "\n";

        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++)
            {
                const MRVer* op = it->second;
                NodeID def = getDef(op);
                F.os() << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= PHISVFGNode | MVER: {"<< *op << "}" << "\n";
            }
        }
    }
    // Job finish and close file
    F.os().close();
    if (!F.os().has_error())
    {
        outs() << "\n";
        F.keep();
        return;       
    }
}


void SVFG::readFile(const string& filename){
    outs() << "Loading SVFG analysis results from '" << filename << "'...";
    ifstream F(filename.c_str());
    if (!F.is_open())
    {
        outs() << " error opening file for reading!\n";
        return;
    }

    PAGEdge::PAGEdgeSetTy& stores = getPAGEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        StorePE* store = SVFUtil::cast<StorePE>(*iter);
        const StmtSVFGNode* sNode = getStmtVFGNode(store);
        for(CHISet::iterator pi = mssa->getCHISet(store).begin(), epi = mssa->getCHISet(store).end(); pi!=epi; ++pi)
            setDef((*pi)->getResVer(),sNode);
    }
    //outer loop through each line in the file
    string line;
    // add nodes
    stat->ATVFNodeStart();
    while (F.good())
    {
        getline(F, line);
        if (line.empty())
            continue;
        if (line.find("__Edges__") != std::string::npos)
            break;

        std::string s = line; 
        std::string delimiter = " >= ";
        string temp; 
        int index = 0;
        //implement delimiter to split string using ">="
        size_t next = 0; size_t last = 0; size_t outer_last = 0; 
        size_t nextTemp; size_t lastTemp;
        NodeID id; 
        string type;
        string MR;
        string basicBlock; 
        string opVer;
        //inner loop through to get each element in the line
        while ((next = s.find(delimiter, last)) != string::npos) 
        {   
            temp = s.substr(last, next-last); 
            last = next + 4; 
            outer_last = next + 4; 
            if(index == 0) 
            {
                nextTemp = temp.find("SVFGNodeID: ") + 12;
                id = atoi(temp.substr(nextTemp).c_str());
            }
            if(index == 1) {type = temp;}
            if(index > 1)
            {
                if(index == 2) {MR = temp;}
                if(index == 3) {basicBlock = temp;}
            }
            index++; 
        }
        MRVer* tempMRVer; 
        if(!MR.empty())
        {
            tempMRVer = getMRVERFromString(MR);
        } 
        //add nodes using the variables we extracted
        if(type == "FormalINSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID FunID = atoi(s.substr(outer_last).c_str());
            addFormalINSVFGNode(SVFUtil::dyn_cast<FunEntryBlockNode>(pag->getICFG()->getICFGNode(FunID)), tempMRVer, id);
        } else if(type == "FormalOUTSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID FunID =  atoi(s.substr(outer_last).c_str());
            addFormalOUTSVFGNode(SVFUtil::dyn_cast<FunExitBlockNode>(pag->getICFG()->getICFGNode(FunID)), tempMRVer, id);
        } else if(type == "ActualINSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID CallSiteID = atoi(s.substr(outer_last).c_str());
            addActualINSVFGNode(SVFUtil::dyn_cast<CallBlockNode>(pag->getICFG()->getICFGNode(CallSiteID)), tempMRVer, id);
        } else if(type == "ActualOUTSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID CallSiteID = atoi(s.substr(outer_last).c_str());
            addActualOUTSVFGNode(SVFUtil::dyn_cast<CallBlockNode>(pag->getICFG()->getICFGNode(CallSiteID)), tempMRVer, id);
        } else if (type == "PHISVFGNode") 
        {
            opVer =  s.substr(outer_last); 
            next = opVer.find("{") + 1;
            last = opVer.find(",}");
            temp = opVer.substr(next, last);
            Map<u32_t,const MRVer*> OPVers;
            int index = 0; 
            while ((next = temp.find("{") + 1) != string::npos) {
                if (temp == ",}")
                    break;
                last = temp.find("},");
                string temp1;
                temp1 = temp.substr(next, last-next);
                MRVer* tempOPVer = getMRVERFromString(temp1);
                OPVers.insert(make_pair(index, tempOPVer));
                temp = temp.substr(last + 1);
                index++;
            }
            next = basicBlock.find("ICFGNodeID: ") + 12;
            temp = basicBlock.substr(next);
            addIntraMSSAPHISVFGNode(pag->getICFG()->getICFGNode(atoi(temp.c_str())), OPVers.begin(), OPVers.end(), tempMRVer, id);
        } else 
        {
        }

        if (totalVFGNode < id)
            totalVFGNode = id + 1;
    }
    stat->ATVFNodeEnd();

    stat->indVFEdgeStart();
    // Edges 
    while (F.good())
    {   
        getline(F, line);
        if (line.empty())
            continue;

        std::string s = line; 
        std::string delimiter = " >= ";
        string temp; 
        int index = 0;
        size_t last = 0; size_t next = 0; size_t outer_last = 0;
        string edge;
        string attributes;
        
        next = s.find(delimiter);

        edge = s.substr(0, next);
        attributes = s.substr(next + 4);

        // extract nodeIDs for src and dst nodes
        NodeID src; NodeID dst;
        next = edge.find("srcSVFGNodeID: ") + 15;
        last = edge.find(" => ");
        src = atoi(edge.substr(next, last-next).c_str());
        next = edge.find("dstSVFGNodeID: ") + 15;
        dst = atoi(edge.substr(next).c_str());

        string type; string attribute;
        if (attributes.find(" | ") == string::npos)
            type = attributes;
        else 
        {
            next = attributes.find(" | ");
            type = attributes.substr(0, next);
            attribute = attributes.substr(next + 3);
        }

        if(type == "FormalINSVFGNode")
        {
            const FormalINSVFGNode* formalIn = SVFUtil::cast<FormalINSVFGNode>(getSVFGNode(src));
            const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(dst));
            addInterIndirectVFCallEdge(actualIn,formalIn, getCallSiteID(actualIn->getCallSite(), formalIn->getFun()));
        } else if(type == "FormalOUTSVFGNode")
        {
            const FormalOUTSVFGNode* formalOut = SVFUtil::cast<FormalOUTSVFGNode>(getSVFGNode(src));
            if (attribute.find("intra") != string::npos) 
            {
                addIntraIndirectVFEdge(dst, src, formalOut->getMRVer()->getMR()->getPointsTo());
            } 
            else
            {
                const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(dst));
                addInterIndirectVFRetEdge(formalOut,actualOut,getCallSiteID(actualOut->getCallSite(), formalOut->getFun()));
            }
        } else if(type == "ActualINSVFGNode")
        {
            const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(src));
            addIntraIndirectVFEdge(dst,src, actualIn->getMRVer()->getMR()->getPointsTo());
        } else if(type == "ActualOUTSVFGNode")
        {
            // There's no need to connect actual out node to its definition site in the same function.
        } else if (type == "StoreNode" || type == "LoadNode" || type == "PHISVFGNode")
        {
            MRVer* tempMRVer; 
            tempMRVer = getMRVERFromString(attribute);
            addIntraIndirectVFEdge(dst,src, tempMRVer->getMR()->getPointsTo());
        } else {
        }
    }
    stat->indVFEdgeEnd();
    connectFromGlobalToProgEntry();
}
    
MRVer* SVFG::getMRVERFromString(const string& s) 
{
    string temp;
    size_t last = 0; size_t next = 0; 
    MRVer* tempMRVer; 
    MemRegion* tempMemRegion;
    MSSADEF* tempDef;
    //{create Memory Region object
    next = s.find("MemRegion: pts{") + 15;
    last = s.find("} MRVERSION: ");
    temp = s.substr(next, last-next);
    // convert string to PointsTo
    NodeBS dstPts;
    string point;
    stringstream ss(temp);
    while (getline(ss, point, ' ')) 
    {
        istringstream sss(point);
        NodeID obj;
        sss >> obj;
        dstPts.set(obj);
    }
    tempMemRegion = new MemRegion(dstPts);
    // create mssdef
    next = s.find("MSSADef: ") + 9;
    last = s.find("} >=");
    temp = s.substr(next, last-next);
    // convert string to deftype
    istringstream ss1(temp.substr(0, temp.find(", ")));
    int obj1;
    ss1 >> obj1;
    MSSADEF::DEFTYPE defType = static_cast<MSSADEF::DEFTYPE>(obj1);
    tempDef = new MSSADEF(defType, tempMemRegion);
    // mrversion
    next = s.find("MRVERSION: ") + 11;
    last = s.find(" MSSADef:");
    temp = s.substr(next, last-next);
    // convert mrversion to nodeid
    istringstream ss2(temp);
    NodeID obj2;
    ss2 >> obj2;
    // create mrver
    tempMRVer = new MRVer(tempMemRegion, obj2, tempDef);
    return tempMRVer;
}

/*
 * Create SVFG nodes for address-taken variables
 */
void SVFG::addSVFGNodesForAddrTakenVars()
{

    // set defs for address-taken vars defined at store statements
    SVFStmt::SVFStmtSetTy& stores = getPAGEdgeSet(SVFStmt::Store);
    for (SVFStmt::SVFStmtSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter)
    {
        StoreStmt* store = SVFUtil::cast<StoreStmt>(*iter);
        const StmtSVFGNode* sNode = getStmtVFGNode(store);
        for(CHISet::iterator pi = mssa->getCHISet(store).begin(), epi = mssa->getCHISet(store).end(); pi!=epi; ++pi)
            setDef((*pi)->getResVer(),sNode);
    }

    /// set defs for address-taken vars defined at phi/chi/call
    /// create corresponding def and use nodes for address-taken vars (a.k.a MRVers)
    /// initialize memory SSA phi nodes (phi of address-taken variables)
    for(MemSSA::BBToPhiSetMap::iterator it = mssa->getBBToPhiSetMap().begin(),
            eit = mssa->getBBToPhiSetMap().end(); it!=eit; ++it)
    {
        for(PHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi){
            MemSSA::PHI* phi =  *pi;
            addIntraMSSAPHISVFGNode(pag->getICFG()->getICFGNode(&(phi->getBasicBlock()->front())), phi->opVerBegin(), phi->opVerEnd(),phi->getResVer(), totalVFGNode++);
        }
    }
    /// initialize memory SSA entry chi nodes
    for(MemSSA::FunToEntryChiSetMap::iterator it = mssa->getFunToEntryChiSetMap().begin(),
            eit = mssa->getFunToEntryChiSetMap().end(); it!=eit; ++it)
    {
        for(CHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi){
            const MemSSA::ENTRYCHI* chi = SVFUtil::cast<ENTRYCHI>(*pi);
            addFormalINSVFGNode(pag->getICFG()->getFunEntryICFGNode(chi->getFunction()), chi->getResVer(), totalVFGNode++);
        }
    }
    /// initialize memory SSA return mu nodes
    for(MemSSA::FunToReturnMuSetMap::iterator it = mssa->getFunToRetMuSetMap().begin(),
            eit = mssa->getFunToRetMuSetMap().end(); it!=eit; ++it)
    {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi){
              const MemSSA::RETMU* mu = SVFUtil::cast<RETMU>(*pi);
              addFormalOUTSVFGNode(pag->getICFG()->getFunExitICFGNode(mu->getFunction()), mu->getMRVer(), totalVFGNode++);
        }
    }
    /// initialize memory SSA callsite mu nodes
    for(MemSSA::CallSiteToMUSetMap::iterator it = mssa->getCallSiteToMuSetMap().begin(),
            eit = mssa->getCallSiteToMuSetMap().end();
            it!=eit; ++it)
    {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi){
            const MemSSA::CALLMU* mu = SVFUtil::cast<CALLMU>(*pi);
            addActualINSVFGNode(mu->getCallSite(), mu->getMRVer(), totalVFGNode++);
        }
    }
    /// initialize memory SSA callsite chi nodes
    for(MemSSA::CallSiteToCHISetMap::iterator it = mssa->getCallSiteToChiSetMap().begin(),
            eit = mssa->getCallSiteToChiSetMap().end();
            it!=eit; ++it)
    {
        for(CHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi){
            const MemSSA::CALLCHI* chi = SVFUtil::cast<CALLCHI>(*pi);
            addActualOUTSVFGNode(chi->getCallSite(), chi->getResVer(), totalVFGNode++);
        }

    }
}

/*
 * Connect def-use chains for indirect value-flow, (value-flow of address-taken variables)
 */
void SVFG::connectIndirectSVFGEdges()
{
    
    for(iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const LoadSVFGNode* loadNode = SVFUtil::dyn_cast<LoadSVFGNode>(node))
        {
            MUSet& muSet = mssa->getMUSet(SVFUtil::cast<LoadStmt>(loadNode->getPAGEdge()));
            for(MUSet::iterator it = muSet.begin(), eit = muSet.end(); it!=eit; ++it)
            {
                if(LOADMU* mu = SVFUtil::dyn_cast<LOADMU>(*it))
                {
                    NodeID def = getDef(mu->getMRVer());
                    addIntraIndirectVFEdge(def,nodeId, mu->getMRVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const StoreSVFGNode* storeNode = SVFUtil::dyn_cast<StoreSVFGNode>(node))
        {
            CHISet& chiSet = mssa->getCHISet(SVFUtil::cast<StoreStmt>(storeNode->getPAGEdge()));
            for(CHISet::iterator it = chiSet.begin(), eit = chiSet.end(); it!=eit; ++it)
            {
                if(STORECHI* chi = SVFUtil::dyn_cast<STORECHI>(*it))
                {
                    NodeID def = getDef(chi->getOpVer());
                    addIntraIndirectVFEdge(def,nodeId, chi->getOpVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const FormalINSVFGNode* formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            PTACallGraphEdge::CallInstSet callInstSet;
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(formalIn->getFun(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it)
            {
                const CallICFGNode* cs = *it;
                if(!mssa->hasMU(cs))
                    continue;
                ActualINSVFGNodeSet& actualIns = getActualINSVFGNodes(cs);
                for(ActualINSVFGNodeSet::iterator ait = actualIns.begin(), aeit = actualIns.end(); ait!=aeit; ++ait)
                {
                    const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFCallEdge(actualIn,formalIn,getCallSiteID(cs, formalIn->getFun()));
                }
            }
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            PTACallGraphEdge::CallInstSet callInstSet;
            // const MemSSA::RETMU* retMu = formalOut->getRetMU();
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(formalOut->getFun(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it)
            {
                const CallICFGNode* cs = *it;
                if(!mssa->hasCHI(cs))
                    continue;
                ActualOUTSVFGNodeSet& actualOuts = getActualOUTSVFGNodes(cs);
                for(ActualOUTSVFGNodeSet::iterator ait = actualOuts.begin(), aeit = actualOuts.end(); ait!=aeit; ++ait)
                {
                    const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFRetEdge(formalOut,actualOut,getCallSiteID(cs, formalOut->getFun()));
                }
            }
            NodeID def = getDef(formalOut->getMRVer());
            addIntraIndirectVFEdge(def,nodeId, formalOut->getMRVer()->getMR()->getPointsTo());
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            const MRVer* ver = actualIn->getMRVer();
            NodeID def = getDef(ver);
            addIntraIndirectVFEdge(def,nodeId, ver->getMR()->getPointsTo());
        }
        else if(SVFUtil::isa<ActualOUTSVFGNode>(node))
        {
            /// There's no need to connect actual out node to its definition site in the same function.
        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++)
            {
                const MRVer* op = it->second;
                NodeID def = getDef(op);
                addIntraIndirectVFEdge(def,nodeId, op->getMR()->getPointsTo());
            }
        }
    }


    connectFromGlobalToProgEntry();
}


/*!
 * Connect indirect SVFG edges from global initializers (store) to main function entry
 */
void SVFG::connectFromGlobalToProgEntry()
{
    SVFModule* svfModule = mssa->getPTA()->getModule();
    const SVFFunction* mainFunc =
        SVFUtil::getProgEntryFunction(svfModule);
    FormalINSVFGNodeSet& formalIns = getFormalINSVFGNodes(mainFunc);
    if (formalIns.empty())
        return;

    for (GlobalVFGNodeSet::const_iterator storeIt = globalVFGNodes.begin(), storeEit = globalVFGNodes.end();
            storeIt != storeEit; ++storeIt)
    {
        if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(*storeIt))
        {
            /// connect this store to main function entry
            const NodeBS& storePts = mssa->getPTA()->getPts(store->getPAGDstNodeID()).toNodeBS();

            for (FormalINSVFGNodeSet::iterator fiIt = formalIns.begin(), fiEit =
                        formalIns.end(); fiIt != fiEit; ++fiIt)
            {
                NodeID formalInID = *fiIt;
                NodeBS formalInPts = ((FormalINSVFGNode*) getSVFGNode(formalInID))->getPointsTo();

                formalInPts &= storePts;
                if (formalInPts.empty())
                    continue;

                /// add indirect value flow edge
                addIntraIndirectVFEdge(store->getId(), formalInID, formalInPts);
            }
        }
    }
}

/*
 *  Add def-use edges of a memory region between two statements
 */
SVFGEdge* SVFG::addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    checkIntraEdgeParents(srcNode, dstNode);
    if(SVFGEdge* edge = hasIntraVFGEdge(srcNode,dstNode,SVFGEdge::IntraIndirectVF))
    {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : nullptr);
    }
    else
    {
        IntraIndSVFGEdge* indirectEdge = new IntraIndSVFGEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addSVFGEdge(indirectEdge) ? indirectEdge : nullptr);
    }
}


/*!
 * Add def-use edges of a memory region between two may-happen-in-parallel statements for multithreaded program
 */
SVFGEdge* SVFG::addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasThreadVFGEdge(srcNode,dstNode,SVFGEdge::TheadMHPIndirectVF))
    {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : nullptr);
    }
    else
    {
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addSVFGEdge(indirectEdge) ? indirectEdge : nullptr);
    }
}

/*
 *  Add def-use call edges of a memory region between two statements
 */
SVFGEdge* SVFG::addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts,CallSiteID csId)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterVFGEdge(srcNode,dstNode,SVFGEdge::CallIndVF,csId))
    {
        assert(SVFUtil::isa<CallIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<CallIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : nullptr);
    }
    else
    {
        CallIndSVFGEdge* callEdge = new CallIndSVFGEdge(srcNode,dstNode,csId);
        callEdge->addPointsTo(cpts);
        return (addSVFGEdge(callEdge) ? callEdge : nullptr);
    }
}

/*
 *  Add def-use return edges of a memory region between two statements
 */
SVFGEdge* SVFG::addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const NodeBS& cpts,CallSiteID csId)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterVFGEdge(srcNode,dstNode,SVFGEdge::RetIndVF,csId))
    {
        assert(SVFUtil::isa<RetIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<RetIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : nullptr);
    }
    else
    {
        RetIndSVFGEdge* retEdge = new RetIndSVFGEdge(srcNode,dstNode,csId);
        retEdge->addPointsTo(cpts);
        return (addSVFGEdge(retEdge) ? retEdge : nullptr);
    }
}

/*!
 *
 */
SVFGEdge* SVFG::addInterIndirectVFCallEdge(const ActualINSVFGNode* src, const FormalINSVFGNode* dst,CallSiteID csId)
{
    NodeBS cpts1 = src->getPointsTo();
    NodeBS cpts2 = dst->getPointsTo();
    if(cpts1.intersects(cpts2))
    {
        cpts1 &= cpts2;
        return addCallIndirectVFEdge(src->getId(),dst->getId(),cpts1,csId);
    }
    return nullptr;
}

/*!
 * Add inter VF edge from function exit mu to callsite chi
 */
SVFGEdge* SVFG::addInterIndirectVFRetEdge(const FormalOUTSVFGNode* src, const ActualOUTSVFGNode* dst,CallSiteID csId)
{

    NodeBS cpts1 = src->getPointsTo();
    NodeBS cpts2 = dst->getPointsTo();
    if(cpts1.intersects(cpts2))
    {
        cpts1 &= cpts2;
        return addRetIndirectVFEdge(src->getId(),dst->getId(),cpts1,csId);
    }
    return nullptr;
}

/*!
 * Dump SVFG
 */
void SVFG::dump(const std::string& file, bool simple)
{
    GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
}

std::set<const SVFGNode*> SVFG::fromValue(const llvm::Value* value) const
{
    SVFIR* pag = SVFIR::getPAG();
    std::set<const SVFGNode*> ret;
    // search for all PAGEdges first
    for (const PAGEdge* pagEdge : pag->getValueEdges(value)) {
        PAGEdgeToStmtVFGNodeMapTy::const_iterator it = PAGEdgeToStmtVFGNodeMap.find(pagEdge);
        if (it != PAGEdgeToStmtVFGNodeMap.end()) {
            ret.emplace(it->second);
        }
    }
    // add all PAGNodes
    PAGNode* pagNode = pag->getGNode(pag->getValueNode(value));
    if(hasDef(pagNode)) {
        ret.emplace(getDefSVFGNode(pagNode));
    }
    return ret;
}

/**
 * Get all inter value flow edges at this indirect call site, including call and return edges.
 */
void SVFG::getInterVFEdgesForIndirectCallSite(const CallICFGNode* callBlockNode, const SVFFunction* callee, SVFGEdgeSetTy& edges)
{
    CallSiteID csId = getCallSiteID(callBlockNode, callee);
    RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(callBlockNode->getCallSite());

    // Find inter direct call edges between actual param and formal param.
    if (pag->hasCallSiteArgsMap(callBlockNode) && pag->hasFunArgsList(callee))
    {
        const SVFIR::SVFVarList& csArgList = pag->getCallSiteArgsList(callBlockNode);
        const SVFIR::SVFVarList& funArgList = pag->getFunArgsList(callee);
        SVFIR::SVFVarList::const_iterator csArgIt = csArgList.begin(), csArgEit = csArgList.end();
        SVFIR::SVFVarList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        for (; funArgIt != funArgEit && csArgIt != csArgEit; funArgIt++, csArgIt++)
        {
            const PAGNode *cs_arg = *csArgIt;
            const PAGNode *fun_arg = *funArgIt;
            if (fun_arg->isPointer() && cs_arg->isPointer())
                getInterVFEdgeAtIndCSFromAPToFP(cs_arg, fun_arg, callBlockNode, csId, edges);
        }
        assert(funArgIt == funArgEit && "function has more arguments than call site");
        if (callee->getLLVMFun()->isVarArg())
        {
            NodeID varFunArg = pag->getVarargNode(callee);
            const PAGNode* varFunArgNode = pag->getGNode(varFunArg);
            if (varFunArgNode->isPointer())
            {
                for (; csArgIt != csArgEit; csArgIt++)
                {
                    const PAGNode *cs_arg = *csArgIt;
                    if (cs_arg->isPointer())
                        getInterVFEdgeAtIndCSFromAPToFP(cs_arg, varFunArgNode, callBlockNode, csId, edges);
                }
            }
        }
    }

    // Find inter direct return edges between actual return and formal return.
    if (pag->funHasRet(callee) && pag->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = pag->getFunRet(callee);
        if (cs_return->isPointer() && fun_return->isPointer())
            getInterVFEdgeAtIndCSFromFRToAR(fun_return, cs_return, csId, edges);
    }

    // Find inter indirect call edges between actual-in and formal-in svfg nodes.
    if (hasFuncEntryChi(callee) && hasCallSiteMu(callBlockNode))
    {
        SVFG::ActualINSVFGNodeSet& actualInNodes = getActualINSVFGNodes(callBlockNode);
        for(SVFG::ActualINSVFGNodeSet::iterator ai_it = actualInNodes.begin(),
                ai_eit = actualInNodes.end(); ai_it!=ai_eit; ++ai_it)
        {
            ActualINSVFGNode * actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            getInterVFEdgeAtIndCSFromAInToFIn(actualIn, callee, edges);
        }
    }

    // Find inter indirect return edges between actual-out and formal-out svfg nodes.
    if (hasFuncRetMu(callee) && hasCallSiteChi(callBlockNode))
    {
        SVFG::ActualOUTSVFGNodeSet& actualOutNodes = getActualOUTSVFGNodes(callBlockNode);
        for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it)
        {
            ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
            getInterVFEdgeAtIndCSFromFOutToAOut(actualOut, callee, edges);
        }
    }
}

/**
 * Connect actual params/return to formal params/return for top-level variables.
 * Also connect indirect actual in/out and formal in/out.
 */
void SVFG::connectCallerAndCallee(const CallICFGNode* cs, const SVFFunction* callee, SVFGEdgeSetTy& edges)
{
    VFG::connectCallerAndCallee(cs,callee,edges);

    CallSiteID csId = getCallSiteID(cs, callee);

    // connect actual in and formal in
    if (hasFuncEntryChi(callee) && hasCallSiteMu(cs))
    {
        SVFG::ActualINSVFGNodeSet& actualInNodes = getActualINSVFGNodes(cs);
        const SVFG::FormalINSVFGNodeSet& formalInNodes = getFormalINSVFGNodes(callee);
        for(SVFG::ActualINSVFGNodeSet::iterator ai_it = actualInNodes.begin(),
                ai_eit = actualInNodes.end(); ai_it!=ai_eit; ++ai_it)
        {
            const ActualINSVFGNode * actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            for(SVFG::FormalINSVFGNodeSet::iterator fi_it = formalInNodes.begin(),
                    fi_eit = formalInNodes.end(); fi_it!=fi_eit; ++fi_it)
            {
                const FormalINSVFGNode* formalIn = SVFUtil::cast<FormalINSVFGNode>(getSVFGNode(*fi_it));
                connectAInAndFIn(actualIn, formalIn, csId, edges);
            }
        }
    }

    // connect actual out and formal out
    if (hasFuncRetMu(callee) && hasCallSiteChi(cs))
    {
        // connect formal out and actual out
        const SVFG::FormalOUTSVFGNodeSet& formalOutNodes = getFormalOUTSVFGNodes(callee);
        SVFG::ActualOUTSVFGNodeSet& actualOutNodes = getActualOUTSVFGNodes(cs);
        for(SVFG::FormalOUTSVFGNodeSet::iterator fo_it = formalOutNodes.begin(),
                fo_eit = formalOutNodes.end(); fo_it!=fo_eit; ++fo_it)
        {
            const FormalOUTSVFGNode * formalOut = SVFUtil::cast<FormalOUTSVFGNode>(getSVFGNode(*fo_it));
            for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                    ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it)
            {
                const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
                connectFOutAndAOut(formalOut, actualOut, csId, edges);
            }
        }
    }
}


/*!
 * Whether this is an function entry SVFGNode (formal parameter, formal In)
 */
const SVFFunction* SVFG::isFunEntrySVFGNode(const SVFGNode* node) const
{
    if(const FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node))
    {
        return fp->getFun();
    }
    else if(const InterPHISVFGNode* phi = SVFUtil::dyn_cast<InterPHISVFGNode>(node))
    {
        if(phi->isFormalParmPHI())
            return phi->getFun();
    }
    else if(const FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
    {
        return fi->getFun();
    }
    else if(const InterMSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<InterMSSAPHISVFGNode>(node))
    {
        if(mphi->isFormalINPHI())
            return mphi->getFun();
    }
    return nullptr;
}

/*!
 * Whether this is an callsite return SVFGNode (actual return, actual out)
 */
const CallICFGNode* SVFG::isCallSiteRetSVFGNode(const SVFGNode* node) const
{
    if(const ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node))
    {
        return ar->getCallSite();
    }
    else if(const InterPHISVFGNode* phi = SVFUtil::dyn_cast<InterPHISVFGNode>(node))
    {
        if(phi->isActualRetPHI())
            return phi->getCallSite();
    }
    else if(const ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
    {
        return ao->getCallSite();
    }
    else if(const InterMSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<InterMSSAPHISVFGNode>(node))
    {
        if(mphi->isActualOUTPHI())
            return mphi->getCallSite();
    }
    return nullptr;
}

/*!
 * Perform Statistics
 */
void SVFG::performStat()
{
    stat->performStat();
}

/*!
 * GraphTraits specialization
 */
namespace llvm
{
template<>
struct DOTGraphTraits<SVFG*> : public DOTGraphTraits<SVFIR*>
{

    typedef SVFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<SVFIR*>(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(SVFG*)
    {
        return "SVFG";
    }

    /// isNodeHidden - If the function returns true, the given node is not
    /// displayed in the graph
#if LLVM_VERSION_MAJOR >= 12
    static bool isNodeHidden(SVFGNode *node, SVFG *){
#else
    static bool isNodeHidden(SVFGNode *node) {
#endif
        if (Options::ShowHiddenNode) return false;
        else return node->getInEdges().empty() && node->getOutEdges().empty();
    }

    std::string getNodeLabel(NodeType *node, SVFG *graph)
    {
        if (isSimple())
            return getSimpleNodeLabel(node, graph);
        else
            return getCompleteNodeLabel(node, graph);
    }

    /// Return label of a VFG node without MemSSA information
    static std::string getSimpleNodeLabel(NodeType *node, SVFG*)
    {
        std::string str;
        raw_string_ostream rawstr(str);
        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node))
        {
            rawstr << stmtNode->toString();
        }
        else if(PHISVFGNode* tphi = SVFUtil::dyn_cast<PHISVFGNode>(node))
        {
            rawstr << tphi->toString();
        }
        else if(FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node))
        {
            rawstr << fp->toString();
        }
        else if(ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(node))
        {
            rawstr << ap->toString();
        }
        else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node))
        {
            rawstr << ar->toString();
        }
        else if (FormalRetSVFGNode* fr = SVFUtil::dyn_cast<FormalRetSVFGNode>(node))
        {
            rawstr << fr->toString();
        }
        else if(FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            rawstr << fi->toString();
        }
        else if(FormalOUTSVFGNode* fo = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            rawstr << fo->toString();
        }
        else if(ActualINSVFGNode* ai = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            rawstr << ai->toString();
        }
        else if(ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
        {
            rawstr << ao->toString();
        }
        else if(MSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            rawstr << mphi->toString();
        }
        else if(SVFUtil::isa<NullPtrSVFGNode>(node))
        {
            rawstr << "NullPtr";
        }
        else if(BinaryOPVFGNode* bop = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        {
            rawstr << bop->toString();
        }
        else if(UnaryOPVFGNode* uop = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        {
            rawstr << uop->toString();
        }
        else if(CmpVFGNode* cmp = SVFUtil::dyn_cast<CmpVFGNode>(node))
        {
            rawstr << cmp->toString();
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    /// Return label of a VFG node with MemSSA information
    static std::string getCompleteNodeLabel(NodeType *node, SVFG*)
    {

        std::string str;
        raw_string_ostream rawstr(str);
        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node))
        {
            rawstr << stmtNode->toString();
        }
        else if(BinaryOPVFGNode* bop = SVFUtil::dyn_cast<BinaryOPVFGNode>(node))
        {
            rawstr << bop->toString();
        }
        else if(UnaryOPVFGNode* uop = SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
        {
            rawstr << uop->toString();
        }
        else if(CmpVFGNode* cmp = SVFUtil::dyn_cast<CmpVFGNode>(node))
        {
            rawstr << cmp->toString();
        }
        else if(MSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            rawstr << mphi->toString();
        }
        else if(PHISVFGNode* tphi = SVFUtil::dyn_cast<PHISVFGNode>(node))
        {
            rawstr << tphi->toString();
        }
        else if(FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            rawstr	<< fi->toString();
        }
        else if(FormalOUTSVFGNode* fo = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            rawstr << fo->toString();
        }
        else if(FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node))
        {
            rawstr	<< fp->toString();
        }
        else if(ActualINSVFGNode* ai = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            rawstr << ai->toString();
        }
        else if(ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
        {
            rawstr <<  ao->toString();
        }
        else if(ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(node))
        {
            rawstr << ap->toString();
        }
        else if(NullPtrSVFGNode* nptr = SVFUtil::dyn_cast<NullPtrSVFGNode>(node))
        {
            rawstr << nptr->toString();
        }
        else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node))
        {
            rawstr << ar->toString();
        }
        else if (FormalRetSVFGNode* fr = SVFUtil::dyn_cast<FormalRetSVFGNode>(node))
        {
            rawstr << fr->toString();
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVFG *graph)
    {
        std::string str;
        raw_string_ostream rawstr(str);

        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node))
        {
            const PAGEdge* edge = stmtNode->getPAGEdge();
            if (SVFUtil::isa<AddrStmt>(edge))
            {
                rawstr <<  "color=green";
            }
            else if (SVFUtil::isa<CopyStmt>(edge))
            {
                rawstr <<  "color=black";
            }
            else if (SVFUtil::isa<RetPE>(edge))
            {
                rawstr <<  "color=black,style=dotted";
            }
            else if (SVFUtil::isa<GepStmt>(edge))
            {
                rawstr <<  "color=purple";
            }
            else if (SVFUtil::isa<StoreStmt>(edge))
            {
                rawstr <<  "color=blue";
            }
            else if (SVFUtil::isa<LoadStmt>(edge))
            {
                rawstr <<  "color=red";
            }
            else
            {
                assert(0 && "No such kind edge!!");
            }
            rawstr <<  "";
        }
        else if(SVFUtil::isa<MSSAPHISVFGNode>(node))
        {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<PHISVFGNode>(node))
        {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<NullPtrSVFGNode>(node))
        {
            rawstr <<  "color=grey";
        }
        else if(SVFUtil::isa<FormalINSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<FormalOUTSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<FormalParmSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<ActualINSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<ActualOUTSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if(SVFUtil::isa<ActualParmSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<ActualRetSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<FormalRetSVFGNode>(node))
        {
            rawstr <<  "color=yellow,penwidth=2";
        }
        else if (SVFUtil::isa<BinaryOPVFGNode>(node))
        {
            rawstr <<  "color=black,penwidth=2";
        }
        else if (SVFUtil::isa<CmpVFGNode>(node))
        {
            rawstr <<  "color=black,penwidth=2";
        }
        else if (SVFUtil::isa<UnaryOPVFGNode>(node))
        {
            rawstr <<  "color=black,penwidth=2";
        }
        else
            assert(false && "no such kind of node!!");

        /// dump slice information
        if(graph->getStat()->isSource(node))
        {
            rawstr << ",style=filled, fillcolor=red";
        }
        else if(graph->getStat()->isSink(node))
        {
            rawstr << ",style=filled, fillcolor=blue";
        }
        else if(graph->getStat()->inBackwardSlice(node))
        {
            rawstr << ",style=filled, fillcolor=yellow";
        }
        else if(graph->getStat()->inForwardSlice(node))
            rawstr << ",style=filled, fillcolor=gray";

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType*, EdgeIter EI, SVFG*)
    {
        SVFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<DirectSVFGEdge>(edge))
        {
            if (SVFUtil::isa<CallDirSVFGEdge>(edge))
                return "style=solid,color=red";
            else if (SVFUtil::isa<RetDirSVFGEdge>(edge))
                return "style=solid,color=blue";
            else
                return "style=solid";
        }
        else if (SVFUtil::isa<IndirectSVFGEdge>(edge))
        {
            if (SVFUtil::isa<CallIndSVFGEdge>(edge))
                return "style=dashed,color=red";
            else if (SVFUtil::isa<RetIndSVFGEdge>(edge))
                return "style=dashed,color=blue";
            else
                return "style=dashed";
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        SVFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallDirSVFGEdge* dirCall = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetDirSVFGEdge* dirRet = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();
        else if (CallIndSVFGEdge* indCall = SVFUtil::dyn_cast<CallIndSVFGEdge>(edge))
            rawstr << indCall->getCallSiteId();
        else if (RetIndSVFGEdge* indRet = SVFUtil::dyn_cast<RetIndSVFGEdge>(edge))
            rawstr << indRet->getCallSiteId();

        return rawstr.str();
    }
};
} // End namespace llvm
