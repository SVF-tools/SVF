#include "SVFIR/SVF2Neo4J.h"
#include "Graphs/CHG.h"
#include "SVFIR/SVFIR.h"
#include "Util/CommandLine.h"
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <Python.h>


// static const Option<bool> humanReadableOption(
//     "human-readable", "Whether to output human-readable JSON", true);

namespace SVF
{
// TODO: 找个小的数据结构写进database


// SVFIRDbWriter::autoJSON SVFIRDbWriter::generateItems()
// {
//     const IRGraph* const irGraph = svfIR;
//     NodeIDAllocator* nodeIDAllocator = NodeIDAllocator::allocator;
//     assert(nodeIDAllocator && "NodeIDAllocator is not initialized?");

//     cJSON* root = jsonCreateObject();
// #define F(field) JSON_WRITE_FIELD(root, svfIR, field)
//     F(svfModule);
//     F(symInfo);
//     F(icfg);
//     F(chgraph);
//     jsonAddJsonableToObject(root, FIELD_NAME_ITEM(irGraph));
//     F(icfgNode2SVFStmtsMap);
//     F(icfgNode2PTASVFStmtsMap);
//     F(GepValObjMap);
//     F(typeLocSetsMap);
//     F(GepObjVarMap);
//     F(memToFieldsMap);
//     F(globSVFStmtSet);
//     F(phiNodeMap);
//     F(funArgsListMap);
//     F(callSiteArgsListMap);
//     F(callSiteRetMap);
//     F(funRetMap);
//     F(indCallSiteToFunPtrMap);
//     F(funPtrToCallSitesMap);
//     F(candidatePointers);
//     F(callSiteSet);
//     jsonAddJsonableToObject(root, FIELD_NAME_ITEM(nodeIDAllocator));
// #undef F

//     return {root, cJSON_Delete};
// }


const char*  SVFIRDbWriter::generateDataBaseItems()
{
    // autoITEM object = generateItems();
    // char* str;
    // if (humanReadableOption()){
    //     str = cITEM_Print(object.get());
    // }
    // else{
    //     SVFUtil::errs() << "Not human readable!\n";
    // }
    const char* str =  "generateDataBaseItems!\n";
    return str;
}


void SVFIRDbWriter::writeSVFIRToOstream(const SVFIR* svfir, std::ostream& os)
{
    // SVFIRDbWriter writer(svfir);
    // os << writer.generateDataBaseItems().get() << '\n'; // 这里是一次性写入 os , 但是我们需要写入到数据库
}

void SVFIRDbWriter::writeToDatabase(const SVFIR* svfir, const std::string& path)
{

    // SVFIRDbWriter writer(svfir);
    // if (jsonFile.is_open())
    // {
    //     writeSVFIRToOstream(svfir, jsonFile);
    //     jsonFile.close();
    // }
    // else
    // {
    //     SVFUtil::errs() << "Failed to connect to the dataset '" << path
    //                     << "' to write SVFIR\n";
    // }
}

SVFIRDbWriter::SVFIRDbWriter(const SVFIR* svfir)
    : svfIR(svfir), svfModuleWriter(svfir->svfModule), icfgWriter(svfir->icfg),
      chgWriter(SVFUtil::dyn_cast<CHGraph>(svfir->chgraph)),
      irGraphWriter(svfir)
{
    const char* uri = "neo4j+s://4291d08d.databases.neo4j.io";
    const char* username = "neo4j";
    const char* password = "ghfQKOPyySepbGDVuGgWsJLhhHP2R-ukd3tbvT9QNu8";
    db = new Neo4jClient(uri, username, password);
}

ICFGDbWriter::ICFGDbWriter(const ICFG* icfg) : GenericICFGDbWriter(icfg)
{
    for (const auto& pair : icfg->getIcfgNodeToSVFLoopVec())
    {
        for (const SVFLoop* loop : pair.second)
        {
            svfLoopPool.saveID(loop);
        }
    }
}

SVFModuleDbWriter::SVFModuleDbWriter(const SVFModule* svfModule)
{
    // TODO: SVFType & StInfo are managed by SymbolTableInfo. Refactor it?
    auto symTab = SymbolTableInfo::SymbolInfo();

    const auto& svfTypes = symTab->getSVFTypes();
    svfTypePool.reserve(svfTypes.size());
    for (const SVFType* type : svfTypes)
    {
        svfTypePool.saveID(type);
    }

    const auto& stInfos = symTab->getStInfos();
    stInfoPool.reserve(stInfos.size());
    for (const StInfo* stInfo : stInfos)
    {
        stInfoPool.saveID(stInfo);
    }

    svfValuePool.reserve(svfModule->getFunctionSet().size() +
                         svfModule->getConstantSet().size() +
                         svfModule->getOtherValueSet().size());
}



}