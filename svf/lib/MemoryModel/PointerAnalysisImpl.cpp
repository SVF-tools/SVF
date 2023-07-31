//===- PointerAnalysisImpl.cpp -- Pointer analysis implementation--------------------//
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
 * PointerAnalysisImpl.cpp
 *
 *  Created on: 28Mar.,2020
 *      Author: yulei
 */


#include "MemoryModel/PointerAnalysisImpl.h"
#include "Util/Options.h"
#include "Util/CppUtil.h"
#include <fstream>
#include <sstream>

using namespace SVF;
using namespace SVFUtil;
using namespace cppUtil;
using namespace std;

/*!
 * Constructor
 */
BVDataPTAImpl::BVDataPTAImpl(SVFIR* p, PointerAnalysis::PTATY type, bool alias_check) :
    PointerAnalysis(p, type, alias_check), ptCache()
{
    if (type == Andersen_BASE || type == Andersen_WPA || type == AndersenWaveDiff_WPA
            || type == TypeCPP_WPA || type == FlowS_DDA
            || type == AndersenSCD_WPA || type == AndersenSFR_WPA || type == CFLFICI_WPA || type == CFLFSCS_WPA)
    {
        // Only maintain reverse points-to when the analysis is field-sensitive, as objects turning
        // field-insensitive is all it is used for.
        bool maintainRevPts = Options::MaxFieldLimit() != 0;
        if (Options::ptDataBacking() == PTBackingType::Mutable) ptD = std::make_unique<MutDiffPTDataTy>(maintainRevPts);
        else if (Options::ptDataBacking() == PTBackingType::Persistent) ptD = std::make_unique<PersDiffPTDataTy>(getPtCache(), maintainRevPts);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else if (type == Steensgaard_WPA)
    {
        // Steensgaard is only field-insensitive (for now?), so no reverse points-to.
        if (Options::ptDataBacking() == PTBackingType::Mutable) ptD = std::make_unique<MutDiffPTDataTy>(false);
        else if (Options::ptDataBacking() == PTBackingType::Persistent) ptD = std::make_unique<PersDiffPTDataTy>(getPtCache(), false);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else if (type == FSSPARSE_WPA)
    {
        if (Options::INCDFPTData())
        {
            if (Options::ptDataBacking() == PTBackingType::Mutable) ptD = std::make_unique<MutIncDFPTDataTy>(false);
            else if (Options::ptDataBacking() == PTBackingType::Persistent) ptD = std::make_unique<PersIncDFPTDataTy>(getPtCache(), false);
            else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
        }
        else
        {
            if (Options::ptDataBacking() == PTBackingType::Mutable) ptD = std::make_unique<MutDFPTDataTy>(false);
            else if (Options::ptDataBacking() == PTBackingType::Persistent) ptD = std::make_unique<PersDFPTDataTy>(getPtCache(), false);
            else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
        }
    }
    else if (type == VFS_WPA)
    {
        if (Options::ptDataBacking() == PTBackingType::Mutable) ptD = std::make_unique<MutVersionedPTDataTy>(false);
        else if (Options::ptDataBacking() == PTBackingType::Persistent) ptD = std::make_unique<PersVersionedPTDataTy>(getPtCache(), false);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else assert(false && "no points-to data available");

    ptaImplTy = BVDataImpl;
}

void BVDataPTAImpl::finalize()
{
    normalizePointsTo();
    PointerAnalysis::finalize();

    if (Options::ptDataBacking() == PTBackingType::Persistent && print_stat)
    {
        std::string moduleName(pag->getModule()->getModuleIdentifier());
        std::vector<std::string> names = SVFUtil::split(moduleName,'/');
        if (names.size() > 1)
            moduleName = names[names.size() - 1];

        std::string subtitle;

        if(ptaTy >= Andersen_BASE && ptaTy <= Steensgaard_WPA)
            subtitle = "Andersen's analysis bitvector";
        else if(ptaTy >=FSDATAFLOW_WPA && ptaTy <=FSCS_WPA)
            subtitle = "flow-sensitive analysis bitvector";
        else if(ptaTy >=CFLFICI_WPA && ptaTy <=CFLFSCS_WPA)
            subtitle = "CFL analysis bitvector";
        else if(ptaTy == TypeCPP_WPA)
            subtitle = "Type analysis bitvector";
        else if(ptaTy >=FieldS_DDA && ptaTy <=Cxt_DDA)
            subtitle = "DDA bitvector";
        else
            subtitle = "bitvector";

        SVFUtil::outs() << "\n****Persistent Points-To Cache Statistics: " << subtitle << "****\n";
        SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
        SVFUtil::outs().flags(std::ios::left);
        ptCache.printStats("bitvector");
        SVFUtil::outs() << "#######################################################" << std::endl;
        SVFUtil::outs().flush();
    }

}

/*!
 * Expand all fields of an aggregate in all points-to sets
 */
void BVDataPTAImpl::expandFIObjs(const PointsTo& pts, PointsTo& expandedPts)
{
    expandedPts = pts;;
    for(PointsTo::iterator pit = pts.begin(), epit = pts.end(); pit!=epit; ++pit)
    {
        if (pag->getBaseObjVar(*pit) == *pit || isFieldInsensitive(*pit))
        {
            expandedPts |= pag->getAllFieldsObjVars(*pit);
        }
    }
}

void BVDataPTAImpl::expandFIObjs(const NodeBS& pts, NodeBS& expandedPts)
{
    expandedPts = pts;
    for (const NodeID o : pts)
    {
        if (pag->getBaseObjVar(o) == o || isFieldInsensitive(o))
        {
            expandedPts |= pag->getAllFieldsObjVars(o);
        }
    }
}

void BVDataPTAImpl::remapPointsToSets(void)
{
    getPTDataTy()->remapAllPts();
}

void BVDataPTAImpl::writeObjVarToFile(const string& filename)
{
    outs() << "Storing ObjVar to '" << filename << "'...";
    error_code err;
    std::fstream f(filename.c_str(), std::ios_base::out);
    if (!f.good())
    {
        outs() << "  error opening file for writing!\n";
        return;
    }

    // Write BaseNodes insensitivity to file
    NodeBS NodeIDs;
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        PAGNode* pagNode = it->second;
        if (!isa<ObjVar>(pagNode)) continue;
        NodeID n = pag->getBaseObjVar(it->first);
        if (NodeIDs.test(n)) continue;
        f << n << " ";
        f << isFieldInsensitive(n) << "\n";
        NodeIDs.set(n);
    }

    f << "------\n";

    f.close();
    if (f.good())
    {
        outs() << "\n";
        return;
    }


}

/*!
 * Store pointer analysis result into a file.
 * It includes the points-to relations, and all SVFIR nodes including those
 * created when solving Andersen's constraints.
 */
void BVDataPTAImpl::writeToFile(const string& filename)
{

    outs() << "Storing pointer analysis results to '" << filename << "'...";

    error_code err;
    std::fstream f(filename.c_str(), std::ios_base::app);
    if (!f.good())
    {
        outs() << "  error opening file for writing!\n";
        return;
    }

    // Write analysis results to file
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        NodeID var = it->first;
        const PointsTo &pts = getPts(var);

        stringstream ss;
        f << var << " -> { ";
        if (pts.empty())
        {
            f << " ";
        }
        else
        {
            for (NodeID n: pts)
            {
                f << n << " ";
            }
        }
        f << "}\n";
    }

    f << "------\n";

    // Write GepPAGNodes to file
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        PAGNode* pagNode = it->second;
        if (GepObjVar *gepObjPN = SVFUtil::dyn_cast<GepObjVar>(pagNode))
        {
            f << it->first << " ";
            f << pag->getBaseObjVar(it->first) << " ";
            f << gepObjPN->getConstantFieldIdx() << "\n";
        }
    }

    f << "------\n";

    // Write BaseNodes insensitivity to file
    NodeBS NodeIDs;
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        PAGNode* pagNode = it->second;
        if (!isa<ObjVar>(pagNode)) continue;
        NodeID n = pag->getBaseObjVar(it->first);
        if (NodeIDs.test(n)) continue;
        f << n << " ";
        f << isFieldInsensitive(n) << "\n";
        NodeIDs.set(n);
    }

    // Job finish and close file
    f.close();
    if (f.good())
    {
        outs() << "\n";
        return;
    }
}

/*!
 * Load pointer analysis result form a file.
 * It populates BVDataPTAImpl with the points-to data, and updates SVFIR with
 * the SVFIR offset nodes created during Andersen's solving stage.
 */
bool BVDataPTAImpl::readFromFile(const string& filename)
{

    outs() << "Loading pointer analysis results from '" << filename << "'...";

    ifstream F(filename.c_str());
    if (!F.is_open())
    {
        outs() << "  error opening file for reading!\n";
        return false;
    }

    // Read ObjVar
    string line;
    while (F.good())
    {
        getline(F, line);
        if (line == "------")     break;
        // Parse a single line in the form of "baseNodeID insensitive"
        istringstream ss(line);
        NodeID base;
        bool insensitive;
        ss >> base >> insensitive;

        if (insensitive)
            setObjFieldInsensitive(base);
    }

    // Read analysis results from file
    PTDataTy *ptD = getPTDataTy();

    // Read points-to sets
    string delimiter1 = " -> { ";
    string delimiter2 = " }";
    map<NodeID, string> nodePtsMap;
    map<string, PointsTo> strPtsMap;

    while (F.good())
    {
        // Parse a single line in the form of "var -> { obj1 obj2 obj3 }"
        getline(F, line);
        if (line == "------")     break;
        size_t pos = line.find(delimiter1);
        if (pos == string::npos)    break;
        if (line.back() != '}')     break;

        // var
        NodeID var = atoi(line.substr(0, pos).c_str());

        // objs
        pos = pos + delimiter1.length();
        size_t len = line.length() - pos - delimiter2.length();
        string objs = line.substr(pos, len);
        PointsTo dstPts;

        if (!objs.empty())
        {
            // map the variable ID to its unique string pointer set
            nodePtsMap[var] = objs;
            if (strPtsMap.count(objs)) continue;

            istringstream ss(objs);
            NodeID obj;
            while (ss.good())
            {
                ss >> obj;
                dstPts.set(obj);
            }
            // map the string pointer set to the parsed PointsTo set
            strPtsMap[objs] = dstPts;
        }
    }

    // map the variable ID to its pointer set
    for (auto t: nodePtsMap)
        ptD->unionPts(t.first, strPtsMap[t.second]);

    // Read BaseNode insensitivity
    while (F.good())
    {
        getline(F, line);
        if (line == "------")     break;
        // Parse a single line in the form of "ID baseNodeID offset"
        istringstream ss(line);
        NodeID id;
        NodeID base;
        size_t offset;
        ss >> id >> base >> offset;
        NodeID n = pag->getGepObjVar(base, offset);
        bool matched = (id == n);
        (void)matched;
        assert(matched && "Error adding GepObjNode into SVFIR!");
    }

    // //update ObjVar status
    while (F.good())
    {
        getline(F, line);
        if (line.empty())
            break;
        // Parse a single line in the form of "baseNodeID insensitive"
        istringstream ss(line);
        NodeID base;
        bool insensitive;
        ss >> base >> insensitive;

        if (insensitive)
            setObjFieldInsensitive(base);
    }

    // Update callgraph
    updateCallGraph(pag->getIndirectCallsites());

    F.close();
    outs() << "\n";

    return true;
}

/*!
 * Dump points-to of each pag node
 */
void BVDataPTAImpl::dumpTopLevelPtsTo()
{
    for (OrderedNodeSet::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter)
    {
        const PAGNode* node = getPAG()->getGNode(*nIter);
        if (getPAG()->isValidTopLevelPtr(node))
        {
            const PointsTo& pts = this->getPts(node->getId());
            outs() << "\nNodeID " << node->getId() << " ";

            if (pts.empty())
            {
                outs() << "\t\tPointsTo: {empty}\n\n";
            }
            else
            {
                outs() << "\t\tPointsTo: { ";
                for (PointsTo::iterator it = pts.begin(), eit = pts.end();
                        it != eit; ++it)
                    outs() << *it << " ";
                outs() << "}\n\n";
            }
        }
    }

    outs().flush();
}


/*!
 * Dump all points-to including top-level (ValVar) and address-taken (ObjVar) variables
 */
void BVDataPTAImpl::dumpAllPts()
{
    OrderedNodeSet pagNodes;
    for(SVFIR::iterator it = pag->begin(), eit = pag->end(); it!=eit; it++)
    {
        pagNodes.insert(it->first);
    }

    for (NodeID n : pagNodes)
    {
        outs() << "----------------------------------------------\n";
        dumpPts(n, this->getPts(n));
    }

    outs() << "----------------------------------------------\n";
}


/*!
 * On the fly call graph construction
 * callsites is candidate indirect callsites need to be analyzed based on points-to results
 * newEdges is the new indirect call edges discovered
 */
void BVDataPTAImpl::onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges)
{
    for(CallSiteToFunPtrMap::const_iterator iter = callsites.begin(), eiter = callsites.end(); iter!=eiter; ++iter)
    {
        const CallICFGNode* cs = iter->first;

        if (SVFUtil::getSVFCallSite(cs->getCallSite()).isVirtualCall())
        {
            const SVFValue* vtbl = SVFUtil::getSVFCallSite(cs->getCallSite()).getVtablePtr();
            assert(pag->hasValueNode(vtbl));
            NodeID vtblId = pag->getValueNode(vtbl);
            resolveCPPIndCalls(cs, getPts(vtblId), newEdges);
        }
        else
            resolveIndCalls(iter->first,getPts(iter->second),newEdges);
    }
}

/*!
 * Normalize points-to information for field-sensitive analysis
 */
void BVDataPTAImpl::normalizePointsTo()
{
    SVFIR::MemObjToFieldsMap &memToFieldsMap = pag->getMemToFieldsMap();
    SVFIR::NodeOffsetMap &GepObjVarMap = pag->getGepObjNodeMap();

    // collect each gep node whose base node has been set as field-insensitive
    NodeBS dropNodes;
    for (auto t: memToFieldsMap)
    {
        NodeID base = t.first;
        const MemObj* memObj = pag->getObject(base);
        assert(memObj && "Invalid memobj in memToFieldsMap");
        if (memObj->isFieldInsensitive())
        {
            for (NodeID id : t.second)
            {
                if (SVFUtil::isa<GepObjVar>(pag->getGNode(id)))
                {
                    dropNodes.set(id);
                }
                else
                    assert(id == base && "Not a GepObj Node or a baseObj Node?");
            }
        }
    }

    // remove the collected redundant gep nodes in each pointers's pts
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
        NodeID n = nIter->first;

        const PointsTo &tmpPts = getPts(n);
        for (NodeID obj : tmpPts)
        {
            if (!dropNodes.test(obj))
                continue;
            NodeID baseObj = pag->getBaseObjVar(obj);
            clearPts(n, obj);
            addPts(n, baseObj);
        }
    }

    // clear GepObjVarMap and memToFieldsMap for redundant gepnodes
    // and remove those nodes from pag
    for (NodeID n: dropNodes)
    {
        NodeID base = pag->getBaseObjVar(n);
        GepObjVar *gepNode = SVFUtil::dyn_cast<GepObjVar>(pag->getGNode(n));
        const APOffset apOffset = gepNode->getConstantFieldIdx();
        GepObjVarMap.erase(std::make_pair(base, apOffset));
        memToFieldsMap[base].reset(n);

        pag->removeGNode(gepNode);
    }
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
AliasResult BVDataPTAImpl::alias(const SVFValue* V1,
                                 const SVFValue* V2)
{
    return alias(pag->getValueNode(V1),pag->getValueNode(V2));
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
AliasResult BVDataPTAImpl::alias(NodeID node1, NodeID node2)
{
    return alias(getPts(node1),getPts(node2));
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
AliasResult BVDataPTAImpl::alias(const PointsTo& p1, const PointsTo& p2)
{

    PointsTo pts1;
    expandFIObjs(p1,pts1);
    PointsTo pts2;
    expandFIObjs(p2,pts2);

    if (containBlackHoleNode(pts1) || containBlackHoleNode(pts2) || pts1.intersects(pts2))
        return AliasResult::MayAlias;
    else
        return AliasResult::NoAlias;
}
