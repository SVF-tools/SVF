//===- SVFGReadWrite.cpp -- SVFG read & write-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SVFGReadWrite.cpp
 *
 *  Created on: May 21, 2022
 *      Author: Jeffrey Ma
 */

#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGStat.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include <fstream>
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;

// Format of file
// __Nodes__
// SVFGNodeID: <id> >= <node type> >= MVER: {MRVERID: <id> MemRegion: pts{<pts> } MRVERSION: <version> MSSADef: <version>, pts{<pts> }} >= ICFGNodeID: <id>
// __Edges__
// srcSVFGNodeID: <id> => dstSVFGNodeID: <id> >= <edge type> | MVER: {MRVERID: <id> MemRegion: pts{<pts> } MRVERSION: <version> MSSADef: <version>, pts{<pts> }}
void SVFG::writeToFile(const string& filename)
{
    outs() << "Writing SVFG analysis to '" << filename << "'...";
    error_code err;
    std::fstream f(filename.c_str(), std::ios_base::out);
    if (!f.good())
    {
        outs() << "  error opening file for writing!\n";
        return;
    }
    f << "__Nodes__\n";
    // Iterate over nodes and write to file
    for(iterator it = begin(), eit = end(); it!=eit; ++it)
    {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const FormalINSVFGNode* formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(node))
        {
            //node
            f << "SVFGNodeID: " << nodeId << " >= " << "FormalINSVFGNode";
            f << " >= MVER: {";
            f << *formalIn->getMRVer() << "} >= ICFGNodeID: " << formalIn->getFunEntryNode()->getId() << "\n";
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            //node
            f << "SVFGNodeID: " << nodeId << " >= " << "FormalOUTSVFGNode";
            f << " >= MVER: {";
            f << *formalOut->getMRVer() << "} >= ICFGNodeID: " <<  formalOut->getFunExitNode()->getId() << "\n";
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            //node
            f << "SVFGNodeID: " << nodeId << " >= " << "ActualINSVFGNode";
            f << " >= MVER: {";
            f << *actualIn->getMRVer() << "} >= ICFGNodeID: " << actualIn->getCallSite()->getId() << "\n";
        }
        else if(const ActualOUTSVFGNode* actualOut = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
        {
            //node
            f << "SVFGNodeID: " << nodeId << " >= " << "ActualOUTSVFGNode" << " >= MVER: {";
            f << *actualOut->getMRVer() << "} >= ICFGNodeID: "  <<  actualOut->getCallSite()->getId() << "\n";
        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            //node
            f << "SVFGNodeID: " << nodeId << " >= " << "PHISVFGNode";
            unordered_map<u32_t,const MRVer*> opvers;
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++)
            {
                opvers.insert(make_pair(it->first, it->second));
            }
            // opvers
            f << " >= MVER: {";
            f << *phiNode->getResVer();
            const SVFInstruction* inst = phiNode->getICFGNode()->getBB()->front();
            f << "} >= ICFGNodeID: " << pag->getICFG()->getICFGNode(inst)->getId();
            f << " >= OPVers: {";
            for (auto x: opvers)
            {
                const MRVer* op = x.second;
                f << "{" << *op << "}" << ",";
            }
            f << "}\n";
        }
    }

    f << "\n\n__Edges__\n";
    // Iterate over edges and write to file
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
                    f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= LoadNode | MVER: {" << *mu->getMRVer() << "}" << "\n";
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
                    f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= StoreNode | MVER: {" << *chi->getOpVer() << "}" << "\n";
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
                    f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << actualIn->getId() << " >= FormalINSVFGNode" << "\n";
                }
            }
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
        {
            PTACallGraphEdge::CallInstSet callInstSet;
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
                    f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << actualOut->getId() << " >= FormalOUTSVFGNode" << "\n";
                }
            }
            NodeID def = getDef(formalOut->getMRVer());
            f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= FormalOUTSVFGNode | intra" << "\n";
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node))
        {
            NodeID def = getDef(actualIn->getMRVer());
            f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= ActualINSVFGNode" << "\n";

        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node))
        {
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++)
            {
                const MRVer* op = it->second;
                NodeID def = getDef(op);
                f << "srcSVFGNodeID: " << nodeId << " => " << "dstSVFGNodeID: " << def << " >= PHISVFGNode | MVER: {" << *op << "}" << "\n";
            }
        }
    }
    // Job finish and close file
    f.close();
    if (f.good())
    {
        outs() << "\n";
        return;
    }
}

void SVFG::readFile(const string& filename)
{
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
        StoreStmt* store = SVFUtil::cast<StoreStmt>(*iter);
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
        size_t next = 0;
        size_t last = 0;
        size_t outer_last = 0;
        size_t nextTemp; //size_t lastTemp;
        NodeID id = 0;
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
            if(index == 1)
            {
                type = temp;
            }
            if(index > 1)
            {
                if(index == 2)
                {
                    MR = temp;
                }
                if(index == 3)
                {
                    basicBlock = temp;
                }
            }
            index++;
        }
        MRVer* tempMRVer;
        if(!MR.empty())
        {
            tempMRVer = getMRVERFromString(MR);
        }
        else
        {
            tempMRVer = getMRVERFromString("");
        }
        //add nodes using the variables we extracted
        if(type == "FormalINSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID FunID = atoi(s.substr(outer_last).c_str());
            addFormalINSVFGNode(SVFUtil::dyn_cast<FunEntryICFGNode>(pag->getICFG()->getICFGNode(FunID)), tempMRVer, id);
        }
        else if(type == "FormalOUTSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID FunID =  atoi(s.substr(outer_last).c_str());
            addFormalOUTSVFGNode(SVFUtil::dyn_cast<FunExitICFGNode>(pag->getICFG()->getICFGNode(FunID)), tempMRVer, id);
        }
        else if(type == "ActualINSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID CallSiteID = atoi(s.substr(outer_last).c_str());
            addActualINSVFGNode(SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getICFGNode(CallSiteID)), tempMRVer, id);
        }
        else if(type == "ActualOUTSVFGNode")
        {
            outer_last = s.find("ICFGNodeID: ") + 12;
            NodeID CallSiteID = atoi(s.substr(outer_last).c_str());
            addActualOUTSVFGNode(SVFUtil::dyn_cast<CallICFGNode>(pag->getICFG()->getICFGNode(CallSiteID)), tempMRVer, id);
        }
        else if (type == "PHISVFGNode")
        {
            opVer =  s.substr(outer_last);
            next = opVer.find("{") + 1;
            last = opVer.find(",}");
            temp = opVer.substr(next, last);
            Map<u32_t,const MRVer*> OPVers;
            int index = 0;
            while ((next = temp.find("{") + 1) != string::npos)
            {
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
        }
        else
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
        // int index = 0;
        size_t last = 0;
        size_t next = 0; // size_t outer_last = 0;
        string edge;
        string attributes;

        next = s.find(delimiter);

        edge = s.substr(0, next);
        attributes = s.substr(next + 4);

        // extract nodeIDs for src and dst nodes
        NodeID src;
        NodeID dst;
        next = edge.find("srcSVFGNodeID: ") + 15;
        last = edge.find(" => ");
        src = atoi(edge.substr(next, last-next).c_str());
        next = edge.find("dstSVFGNodeID: ") + 15;
        dst = atoi(edge.substr(next).c_str());

        string type;
        string attribute;
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
        }
        else if(type == "FormalOUTSVFGNode")
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
        }
        else if(type == "ActualINSVFGNode")
        {
            const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(src));
            addIntraIndirectVFEdge(dst,src, actualIn->getMRVer()->getMR()->getPointsTo());
        }
        else if(type == "ActualOUTSVFGNode")
        {
            // There's no need to connect actual out node to its definition site in the same function.
        }
        else if (type == "StoreNode" || type == "LoadNode" || type == "PHISVFGNode")
        {
            MRVer* tempMRVer;
            tempMRVer = getMRVERFromString(attribute);
            addIntraIndirectVFEdge(dst,src, tempMRVer->getMR()->getPointsTo());
        }
        else
        {
        }
    }
    stat->indVFEdgeEnd();
    connectFromGlobalToProgEntry();
}

MRVer* SVFG::getMRVERFromString(const string& s)
{
    if(s == "")
    {
        return NULL;
    }
    string temp;
    size_t last = 0;
    size_t next = 0;
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