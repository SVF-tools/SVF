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

/* Internal constructor. */
// static cITEM *cJSON_New_Item(const internal_hooks * const hooks)
// {
//     cITEM* node = (cITEM*)hooks->allocate(sizeof(cITEM));
//     if (node)
//     {
//         memset(node, '\0', sizeof(cITEM));
//     }

//     return node;
// }

// CITEM_PUBLIC(cITEM *) cJSON_CreateRaw(const char *raw)
// {
//     cITEM *item = cJSON_New_Item(&global_hooks);
//     if(item)
//     {
//         item->type = cITEM_Raw;
//         item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw, &global_hooks);
//         if(!item->valuestring)
//         {
//             cITEM_Delete(item);
//             return NULL;
//         }
//     }

//     return item;
// }

// CITEM_PUBLIC(cITEM *) cJSON_CreateArray(void)
// {
//     cITEM *item = cJSON_New_Item(&global_hooks);
//     if(item)
//     {
//         item->type=cITEM_Array;
//     }

//     return item;
// }

// CITEM_PUBLIC(cITEM *) cJSON_CreateObject(void)
// {
//     cITEM *item = cJSON_New_Item(&global_hooks);
//     if (item)
//     {
//         item->type = cITEM_Object;
//     }

//     return item;
// }

const char*  SVFIRDbWriter::generateDataBaseItems()
{
    // const IRGraph* const irGraph = svfIR;
    NodeIDAllocator* nodeIDAllocator = NodeIDAllocator::allocator;
    assert(nodeIDAllocator && "NodeIDAllocator is not initialized?");

    // autoITEM object = generateItems();
    // icfgWriter


    const char* str =  "generateDataBaseItems!\n";
    return str;
}



void SVFIRDbWriter::writeToDatabase(const SVFIR* svfir, const std::string& path)
{

    SVFIRDbWriter writer(svfir);
    writer.generateDataBaseItems();
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