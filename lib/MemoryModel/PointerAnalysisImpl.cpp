/*
 * PointerAnalysisImpl.cpp
 *
 *  Created on: 28Mar.,2020
 *      Author: yulei
 */


#include "MemoryModel/PointerAnalysisImpl.h"
#include "SVF-FE/CPPUtil.h"
#include "SVF-FE/DCHG.h"
#include "Util/Options.h"
#include "Util/IRAnnotator.h"
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
    if (type == Andersen_BASE || type == Andersen_WPA || type == AndersenWaveDiff_WPA || type == AndersenHCD_WPA || type == AndersenHLCD_WPA
            || type == AndersenLCD_WPA || type == TypeCPP_WPA || type == FlowS_DDA || type == AndersenWaveDiffWithType_WPA
            || type == AndersenSCD_WPA || type == AndersenSFR_WPA)
    {
        // Only maintain reverse points-to when the analysis is field-sensitive, as objects turning
        // field-insensitive is all it is used for.
        bool maintainRevPts = Options::MaxFieldLimit != 0;
        if (Options::ptDataBacking == PTBackingType::Mutable) ptD = new MutDiffPTDataTy(maintainRevPts);
        else if (Options::ptDataBacking == PTBackingType::Persistent) ptD = new PersDiffPTDataTy(getPtCache(), maintainRevPts);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else if (type == Steensgaard_WPA)
    {
        // Steensgaard is only field-insensitive (for now?), so no reverse points-to.
        if (Options::ptDataBacking == PTBackingType::Mutable) ptD = new MutDiffPTDataTy(false);
        else if (Options::ptDataBacking == PTBackingType::Persistent) ptD = new PersDiffPTDataTy(getPtCache(), false);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else if (type == FSSPARSE_WPA || type == FSTBHC_WPA)
    {
        if (Options::INCDFPTData)
        {
            if (Options::ptDataBacking == PTBackingType::Mutable) ptD = new MutIncDFPTDataTy(false);
            else if (Options::ptDataBacking == PTBackingType::Persistent) ptD = new PersIncDFPTDataTy(getPtCache(), false);
            else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
        }
        else
        {
            if (Options::ptDataBacking == PTBackingType::Mutable) ptD = new MutDFPTDataTy(false);
            else if (Options::ptDataBacking == PTBackingType::Persistent) ptD = new PersDFPTDataTy(getPtCache(), false);
            else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
        }
    }
    else if (type == VFS_WPA)
    {
        if (Options::ptDataBacking == PTBackingType::Mutable) ptD = new MutVersionedPTDataTy(false);
        else if (Options::ptDataBacking == PTBackingType::Persistent) ptD = new PersVersionedPTDataTy(getPtCache(), false);
        else assert(false && "BVDataPTAImpl::BVDataPTAImpl: unexpected points-to backing type!");
    }
    else assert(false && "no points-to data available");

    ptaImplTy = BVDataImpl;
}

void BVDataPTAImpl::finalize()
{
    normalizePointsTo();
    PointerAnalysis::finalize();
    if (Options::ptDataBacking == PTBackingType::Persistent && Options::PStat) ptCache.printStats("bv-finalize");
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

/*!
 * Store pointer analysis result into a file.
 * It includes the points-to relations, and all SVFIR nodes including those
 * created when solving Andersen's constraints.
 */
void BVDataPTAImpl::writeToFile(const string& filename)
{
    writeToModule();

    outs() << "Storing pointer analysis results to '" << filename << "'...";

    error_code err;
    ToolOutputFile F(filename.c_str(), err, llvm::sys::fs::OF_None);
    if (err)
    {
        outs() << "  error opening file for writing!\n";
        F.os().clear_error();
        return;
    }

    // Write analysis results to file
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it) {
        NodeID var = it->first;
        const PointsTo &pts = getPts(var);

        stringstream ss;
        F.os() << var << " -> { ";
        if (pts.empty()) {
            F.os() << " ";
        } else {
            for (NodeID n: pts) {
                F.os() << n << " ";
            }
        }
        F.os() << "}\n";
    }


    // Write GepPAGNodes to file
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        PAGNode* pagNode = it->second;
        if (GepObjVar *gepObjPN = SVFUtil::dyn_cast<GepObjVar>(pagNode))
        {
            F.os() << it->first << " ";
            F.os() << pag->getBaseObjVar(it->first) << " ";
            F.os() << gepObjPN->getConstantFieldIdx() << "\n";
        }
    }

    F.os() << "------\n";
    // Write BaseNodes insensitivity to file
    NodeBS NodeIDs;
    for (auto it = pag->begin(), ie = pag->end(); it != ie; ++it)
    {
        PAGNode* pagNode = it->second;
        if (!isa<ObjVar>(pagNode)) continue;
        NodeID n = pag->getBaseObjVar(it->first);
        if (NodeIDs.test(n)) continue;
        F.os() << n << " ";
        F.os() << isFieldInsensitive(n) << "\n";
        NodeIDs.set(n);
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

/*!
 * Load pointer analysis result form a file.
 * It populates BVDataPTAImpl with the points-to data, and updates SVFIR with
 * the SVFIR offset nodes created during Andersen's solving stage.
 */
bool BVDataPTAImpl::readFromFile(const string& filename)
{
    // If the module annotations are available, read from there instead
    auto mainModule = SVF::LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
    if (mainModule->getNamedMetadata("SVFIR-Annotated") != nullptr)
    {
        return readFromModule();
    }

    outs() << "Loading pointer analysis results from '" << filename << "'...";

    ifstream F(filename.c_str());
    if (!F.is_open())
    {
        outs() << "  error opening file for reading!\n";
        return false;
    }

    // Read analysis results from file
    PTDataTy *ptD = getPTDataTy();
    string line;

    // Read points-to sets
    string delimiter1 = " -> { ";
    string delimiter2 = " }";
    map<NodeID, string> nodePtsMap;
    map<string, PointsTo> strPtsMap;

    while (F.good())
    {
        // Parse a single line in the form of "var -> { obj1 obj2 obj3 }"
        getline(F, line);
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

    // Read SVFIR offset nodes
    while (F.good())
    {
        if (line == "------")     break;
        // Parse a single line in the form of "ID baseNodeID offset"
        istringstream ss(line);
        NodeID id;
        NodeID base;
        size_t offset;
        ss >> id >> base >> offset;

        NodeID n = pag->getGepObjVar(base, LocationSet(offset));
        assert(id == n && "Error adding GepObjNode into SVFIR!");

        getline(F, line);
    }

    // Read BaseNode insensitivity
    while (F.good())
    {
        getline(F, line);
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
 * Store pointer analysis result into the current LLVM module as metadata.
 * It includes the points-to relations, and all SVFIR nodes including those
 * created when solving Andersen's constraints.
 */
void BVDataPTAImpl::writeToModule()
{
    auto irAnnotator = std::make_unique<IRAnnotator>();

    irAnnotator->processAndersenResults(pag, this, true);
}

/*!
 * Load pointer analysis result from the metadata in the module.
 * It populates BVDataPTAImpl with the points-to data, and updates SVFIR with
 * the SVFIR offset nodes created during Andersen's solving stage.
 */
bool BVDataPTAImpl::readFromModule()
{
    auto irAnnotator = std::make_unique<IRAnnotator>();
    irAnnotator->processAndersenResults(pag, this, false);
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

        if (isVirtualCallSite(SVFUtil::getLLVMCallSite(cs->getCallSite())))
        {
            const Value *vtbl = getVCallVtblPtr(SVFUtil::getLLVMCallSite(cs->getCallSite()));
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
void BVDataPTAImpl::normalizePointsTo() {
    SVFIR::MemObjToFieldsMap &memToFieldsMap = pag->getMemToFieldsMap();
    SVFIR::NodeLocationSetMap &GepObjVarMap = pag->getGepObjNodeMap();

    // collect each gep node whose base node has been set as field-insensitive
    NodeBS dropNodes;
    for (auto t: memToFieldsMap){
        NodeID base = t.first;
        const MemObj* memObj = pag->getObject(base);
        assert(memObj && "Invalid memobj in memToFieldsMap");
        if (memObj->isFieldInsensitive()) {
            for (NodeID id : t.second) {
                if (SVFUtil::isa<GepObjVar>(pag->getGNode(id))) {
                    dropNodes.set(id);
                } else
                    assert(id == base && "Not a GepObj Node or a baseObj Node?");
            }
        }
    }

    // remove the collected redundant gep nodes in each pointers's pts
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter) {
        NodeID n = nIter->first;

        const PointsTo &tmpPts = getPts(n);
        for (NodeID obj : tmpPts) {
            if (!dropNodes.test(obj))
                continue;
            NodeID baseObj = pag->getBaseObjVar(obj);
            clearPts(n, obj);
            addPts(n, baseObj);
        }
    }

    // clear GepObjVarMap and memToFieldsMap for redundant gepnodes
    // and remove those nodes from pag
    for (NodeID n: dropNodes) {
        NodeID base = pag->getBaseObjVar(n);
        GepObjVar *gepNode = SVFUtil::dyn_cast<GepObjVar>(pag->getGNode(n));
        const LocationSet ls = gepNode->getLocationSet();
        GepObjVarMap.erase(std::make_pair(base, ls));
        memToFieldsMap[base].reset(n);

        pag->removeGNode(gepNode);
    }
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
AliasResult BVDataPTAImpl::alias(const Value* V1,
                                 const Value* V2)
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
