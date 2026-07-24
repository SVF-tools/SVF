//===- WPAPass.cpp -- Whole program analysis pass------------------------------//
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
//===-----------------------------------------------------------------------===//

/*
 * @file: WPA.cpp
 * @author: yesen
 * @date: 10/06/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAPass.h"
#include "WPA/Andersen.h"
#include "WPA/AndersenPWC.h"
#include "WPA/FlowSensitive.h"
#include "WPA/VersionedFlowSensitive.h"
#include "WPA/TypeAnalysis.h"
#include "WPA/Steensgaard.h"
#include <fstream>
#include <string>
#include <memory>
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <cctype>
#include <cstring>
#include <ctime>
#include <omp.h>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <libpq-fe.h>
#include <vector>
#include <set>
#include <thread>
// #include <omp.h>
using namespace SVF;
using namespace sql;
#define MEM_OP_ALIAS
// #define INTERPROCEDURAL_MEM_CALL_ALIAS
// #define PRINT_DEBUG_INFO
#define WRITE_TO_DB
#define WRITE_LOAD_LOAD_ALIAS_TO_DB
#define LOG_compile_time
// #define PRINT_CALL_DEBUG_INFO
// #define FILE_Write
// #define USE_POSTGRESQL
// #define USE_MYSQL
// #define DEFAULT_SVF
char WPAPass::ID = 0;

/*!
 * Destructor
 */
WPAPass::~WPAPass()
{
    PTAVector::const_iterator it = ptaVector.begin();
    PTAVector::const_iterator eit = ptaVector.end();
    for (; it != eit; ++it)
    {
        PointerAnalysis* pta = *it;
        delete pta;
    }
    ptaVector.clear();
}

/*!
 * We start from here
 */
void WPAPass::runOnModule(SVFIR* pag)
{
    for (u32_t i = 0; i<= PointerAnalysis::Default_PTA; i++)
    {
        PointerAnalysis::PTATY iPtaTy = static_cast<PointerAnalysis::PTATY>(i);
        if (Options::PASelected(iPtaTy))
            runPointerAnalysis(pag, i);
    }
    assert(!ptaVector.empty() && "No pointer analysis is specified.\n");
}

/*!
 * Create pointer analysis according to a specified kind and then analyze the module.
 */
void WPAPass::runPointerAnalysis(SVFIR* pag, u32_t kind)
{
    /// Initialize pointer analysis.
    switch (kind)
    {
    case PointerAnalysis::Andersen_WPA:
        _pta = new Andersen(pag);
        break;
    case PointerAnalysis::AndersenSCD_WPA:
        _pta = new AndersenSCD(pag);
        break;
    case PointerAnalysis::AndersenSFR_WPA:
        _pta = new AndersenSFR(pag);
        break;
    case PointerAnalysis::AndersenWaveDiff_WPA:
        _pta = new AndersenWaveDiff(pag);
        break;
    case PointerAnalysis::Steensgaard_WPA:
        _pta = new Steensgaard(pag);
        break;
    case PointerAnalysis::FSSPARSE_WPA:
        _pta = new FlowSensitive(pag);
        break;
    case PointerAnalysis::VFS_WPA:
        _pta = new VersionedFlowSensitive(pag);
        break;
    case PointerAnalysis::TypeCPP_WPA:
        _pta = new TypeAnalysis(pag);
        break;
    default:
        assert(false && "This pointer analysis has not been implemented yet.\n");
        return;
    }

    ptaVector.push_back(_pta);
    _pta->analyze();
    if (Options::AnderSVFG())
    {
        SVFGBuilder memSSA(true);
        assert(SVFUtil::isa<AndersenBase>(_pta) && "supports only andersen/steensgaard for pre-computed SVFG");
        SVFG *svfg = memSSA.buildFullSVFG((BVDataPTAImpl*)_pta);
        /// support mod-ref queries only for -ander
        if (Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA))
            _svfg = svfg;
        if (Options::PrintAliases())
        {
            PrintAliasPairs(_pta, svfg);
        }
    }
    else
    {
        // If SVFG is not enabled, still call PrintAliasPairs but with nullptr SVFG
        if (Options::PrintAliases())
        {
            // std::cout << "Warning: SVFG is not enabled, printing alias pairs without context sensitivity.\n";
            PrintAliasPairs(_pta, nullptr);
        }
    }

}

void checkResult(PGresult* res, PGconn* conn)
{
    if (PQresultStatus(res) != PGRES_COMMAND_OK &&
        PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        std::cerr << "Error: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }
}

// Extract metadata unique ID from instruction string representation
// Looks for patterns like "!mem_id4" where "4" is the metadata ID we want to extract
int extractMetadataId(const SVFStmt* stmt) {
    // Get the string representation of the statement
    std::string stmtStr = stmt->toString();
    std::string metadataName = "mem_id";
    // Look for the metadata pattern: !metadataName followed by digits
    // Example: "!mem_id4" where "4" is the unique ID we want to extract
    std::string pattern = "!" + metadataName;
    size_t pos = stmtStr.find(pattern);
    if (pos == std::string::npos) {
        // No metadata found
        return -1;
    }

    // Extract the LLVM metadata ID (e.g., "4" from "!mem_id4")
    size_t numberStart = pos + pattern.length();
    size_t numberEnd = numberStart;

    // Make sure we have digits after the pattern
    while (numberEnd < stmtStr.length() && std::isdigit(stmtStr[numberEnd])) {
        numberEnd++;
    }

    if (numberEnd == numberStart) {
        // No number found after the pattern
        return -1;
    }

    int llvmMetadataId = std::stoi(stmtStr.substr(numberStart, numberEnd - numberStart));

    // If not found in cache, return the direct metadata ID
    // This handles cases where the metadata ID is used directly without mapping
    #ifdef PRINT_DEBUG_INFO
    std::cout << "found for metadata ID " << llvmMetadataId << std::endl;
    #endif
    if( llvmMetadataId == -1 ) {
        #ifdef PRINT_DEBUG_INFO
        std::cout << "No metadata ID found in statement: " << stmtStr << std::endl;
        #endif
        assert(false && "No metadata ID found in statement");
        return -1;
    }
    #ifdef PRINT_DEBUG_INFO
    std::cout << "mapping found for metadata ID " << llvmMetadataId << ", using direct ID" << std::endl;
    #endif
    return llvmMetadataId;
}

// Extract metadata unique ID from instruction string representation
// Looks for patterns like "!call_id4" where "4" is the metadata ID we want to extract
int extractCallMetadataId(const CallICFGNode* call, const std::string& metadataName) {
    // Get the string representation of the statement
    std::string callStr = call->toString();

    // Look for the metadata pattern: !metadataName followed by digits
    // Example: "!mem_id4" where "4" is the unique ID we want to extract
    std::string pattern = "!" + metadataName;
    size_t pos = callStr.find(pattern);
    if (pos == std::string::npos) {
        // No metadata found
        return -1;
    }

    // Extract the LLVM metadata ID (e.g., "4" from "!mem_id4")
    size_t numberStart = pos + pattern.length();
    size_t numberEnd = numberStart;

    // Make sure we have digits after the pattern
    while (numberEnd < callStr.length() && std::isdigit(callStr[numberEnd])) {
        numberEnd++;
    }

    if (numberEnd == numberStart) {
        // No number found after the pattern
        return -1;
    }

    int llvmMetadataId = std::stoi(callStr.substr(numberStart, numberEnd - numberStart));

    // If not found in cache, return the direct metadata ID
    // This handles cases where the metadata ID is used directly without mapping
    #ifdef PRINT_DEBUG_INFO
    std::cout << "found for metadata ID " << llvmMetadataId << std::endl;
    #endif
    if( llvmMetadataId == -1 ) {
        #ifdef PRINT_DEBUG_INFO
        std::cout << "No metadata ID found in statement: " << callStr << std::endl;
        #endif
        assert(false && "No metadata ID found in statement");
        return -1;
    }
    #ifdef PRINT_DEBUG_INFO
    std::cout << "mapping found for metadata ID " << llvmMetadataId << ", using direct ID" << std::endl;
    #endif
    return llvmMetadataId;
}

void WPAPass::PrintAliasPairs(PointerAnalysis* pta, SVFG* svfg)
{
    // SVFIR* pag = pta->getPAG();
    #ifdef DEFAULT_SVF
    // Use the passed SVFG parameter instead of assigning to member variable
    for (SVFIR::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit)
    {
        PAGNode* node1 = lit->second;
        PAGNode* node2 = node1;
        for (SVFIR::iterator rit = lit, erit = pag->end(); rit != erit; ++rit)
        {
            node2 = rit->second;
            if(node1==node2)
                continue;
            const FunObjVar* fun1 = node1->getFunction();
            const FunObjVar* fun2 = node2->getFunction();
            // std::cout << "Dump" << std::endl;
            // node1->dump();
            // node2->dump();
            AliasResult result = pta->alias(node1->getId(), node2->getId());
            // Write to both file and stdout
            std::string aliasLine = (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")
                            + std::string(" var") + std::to_string(node1->getId()) + "[" + node1->getName()
                            + "@" + (fun1==nullptr?"":fun1->getName()) + "] --"
                            + " var" + std::to_string(node2->getId()) + "[" + node2->getName()
                            + "@" + (fun2==nullptr?"":fun2->getName()) + "]\n";
            //aliasFile << aliasLine;
            SVFUtil::outs() << aliasLine;
            //std::string key = "&" + registerName_1 + "@" + (fun1==nullptr?"":fun1->getName()) + "&" + registerName_2 + "@" + (fun2==nullptr?"":fun2->getName());
            //data.push_back({key, (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")});
        }
    }
    #endif
    #ifdef MEM_OP_ALIAS
    // Add load/store pointers alias information to database
    // std::cout << "Adding load/store pointers alias information to database...\n";
    addLoadStoreAliasInfoToDatabase(pta, svfg);
    #endif
    // Perform interprocedural analysis between memory operations and call instructions
    //bulkInsertNewTests(baseFilename, data);
}

const size_t BUFFER_SIZE = 1 << 20;   // 1 MB
const int NUM_ROWS = 10000000;

void flushBuffer(PGconn* conn, char* buffer, size_t& pos) {

    if (pos == 0) return;

    if (PQputCopyData(conn, buffer, pos) != 1) {
        std::cerr << "COPY write failed\n";
    }

    pos = 0;
}

// ---- Fast string append helper ----
inline void appendBytes(char* buffer, size_t& pos, const std::string& s) {
    for (char c : s) buffer[pos++] = c;
}

// ---- Fast int append helper ----
inline void appendInt(char* buffer, size_t& pos, int x) {
    char tmp[16];
    int len = 0;
    if (x == 0) {
        buffer[pos++] = '0';
        return;
    }
    while (x > 0) {
        tmp[len++] = '0' + (x % 10);
        x /= 10;
    }
    for (int i = len - 1; i >= 0; i--) buffer[pos++] = tmp[i];
}
//write a function to add load/store pointers, alias information to a database

// ---- Example struct for alias results ----
struct AliasEntry {
    int maxId;
    int minId;
    bool isNoAlias;
};

void WPAPass::addLoadStoreAliasInfoToDatabase(PointerAnalysis* pta, SVFG* svfg) {
    #ifdef LOG_compile_time
    const auto buildDatabaseStart = std::chrono::steady_clock::now();
    #endif

    std::cout << "Adding load/store pointers alias information to database...\n";
    SVFIR* pag = pta->getPAG();
    int AliasCount_ll = 0;
    int AliasCount_ss = 0;
    int AliasCount_ls = 0;
    int totalAliasCount = 0;
    // Use the passed SVFG parameter
    // No need to assign to _svfg member variable here
    // Get the input filename from module identifier
    std::string moduleId = pag->getModuleIdentifier();

    // Extract filename without path and extension, add suffix
    std::string baseFilename = moduleId;
    size_t lastSlash = baseFilename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseFilename = baseFilename.substr(lastSlash + 1);
    }

    // Remove file extension (.ll, .bc, etc.)
    size_t lastDot = baseFilename.find_last_of(".");
    if (lastDot != std::string::npos) {
        baseFilename = baseFilename.substr(0, lastDot);
    }

    std::string suffix = "_onnx_re_O3_unroll_O3_annotated";
    if (baseFilename.size() >= suffix.size() && baseFilename.compare(baseFilename.size() - suffix.size(), suffix.size(), suffix) == 0) {
        baseFilename.erase(baseFilename.size() - suffix.size());
    }
    std::regex ihen("-");
    std::regex suffix2("_OR3_unroll_O3");
    baseFilename = std::regex_replace(baseFilename, ihen, "__");
    baseFilename = std::regex_replace(baseFilename, suffix2, "");
    std::replace(baseFilename.begin(), baseFilename.end(), '.', '_');
    // std::vector<WPAPass::Row> data;
    // Collect load and store pointer nodes separately
    struct LoadpointersWithMetadata
    {
        const SVFStmt* node;
        int metadataId;

        // // Comparison operator for std::set
        bool operator<(const LoadpointersWithMetadata& other) const {
            if (node != other.node) {
                return node < other.node;
            }
            return metadataId < other.metadataId;
        }

        // Equality operator for find operations
        bool operator==(const LoadpointersWithMetadata& other) const {
            return node == other.node && metadataId == other.metadataId;
        }
    };
    struct StorePointersWithMetadata
    {
        const SVFStmt* node;
        int metadataId;

        // Comparison operator for std::set
        bool operator<(const StorePointersWithMetadata& other) const {
            if (node != other.node) {
                return node < other.node;
            }
            return metadataId < other.metadataId;
        }

        // Equality operator for find operations
        bool operator==(const StorePointersWithMetadata& other) const {
            return node == other.node && metadataId == other.metadataId;
        }
    };

    std::set<LoadpointersWithMetadata> loads;
    std::set<StorePointersWithMetadata> stores;
    int totalLoadPointers = 0;
    int totalStorePointers = 0;



    // Add symbols for all of the functions and the instructions in them.
    for (const auto& item : *PAG::getPAG()->getCallGraph())
    {
        const FunObjVar* F = item.second->getFunction();
        // collect and create symbols inside the function body
        for (auto it : *F)
        {
            const SVFBasicBlock* svfbb = it.second;
            for (const ICFGNode* icfgNode : svfbb->getICFGNodeList())
            {
                for(const SVFStmt* stmt : pag->getSVFStmtList(icfgNode))
                {
                    if (const LoadStmt* l = SVFUtil::dyn_cast<LoadStmt>(stmt))
                    {
                        totalLoadPointers++;
                        int metadataId = extractMetadataId(stmt);
                        #ifdef PRINT_DEBUG_INFO
                        std::cout << "Processing load statement: " << stmt->toString() << ", extracted metadata ID: " << metadataId << std::endl;
                        #endif
                        if(metadataId == -1) {
                            #ifdef PRINT_DEBUG_INFO
                            std::cout << "Failed to extract metadata ID from load statement: " << stmt->toString() << std::endl;
                            #endif
                            // l->toString(); // For debugging: print the statement that failed to extract metadata
                            // assert(false && "Failed to extract metadata ID from load statement");
                            continue; // Skip if no metadata ID found
                        }
                        loads.insert(LoadpointersWithMetadata{l, metadataId});
                    }
                    else if (const StoreStmt* s = SVFUtil::dyn_cast<StoreStmt>(stmt))
                    {
                        totalStorePointers++;
                        int metadataId = extractMetadataId(stmt);
                        #ifdef PRINT_DEBUG_INFO
                        std::cout << "Processing store statement: " << stmt->toString() << ", extracted metadata ID: " << metadataId << std::endl;
                        #endif
                        if(metadataId == -1) {
                            #ifdef PRINT_DEBUG_INFO
                            s->toString(); // For debugging: print the statement that failed to extract metadata
                            // assert(false && "Failed to extract metadata ID from store statement");
                            std::cout << "Failed to extract metadata ID from store statement: " << stmt->toString() << std::endl;
                            #endif
                            continue; // Skip if no metadata ID found
                        }
                        stores.insert(StorePointersWithMetadata{s, metadataId});
                    }
                }
            }
        }
    }
    std::cout << "Finished collecting load/store pointers with metadata\n";
    std::vector<StorePointersWithMetadata> storePointersVec(stores.begin(), stores.end());
    std::vector<LoadpointersWithMetadata> loadPointersVec(loads.begin(), loads.end());
    // std::cout << " Total unique load pointers with metadata: " << loads.size() << "\n";
    // std::cout << " Total unique store pointers with metadata: " << stores.size() << "\n";
    std::cout << " Total load pointers: " << totalLoadPointers << "\n";
    std::cout << " Total store pointers: " << totalStorePointers << "\n";
    // std::cout << "Checking alias relationships\n";s

    auto collectLoadStoreAliasRows = [&](const std::vector<StorePointersWithMetadata>& leftVec,
                                         const std::vector<LoadpointersWithMetadata>& rightVec,
                                         int& noAliasCount) {
        std::string rows;
        std::vector<std::string> threadRows(omp_get_max_threads());
        std::cout << "Checking load-store alias relationships\n";
        #pragma omp parallel
        {
            std::string localRows;
            std::vector<char> buf(1 << 16); // 64KB chunk buffer per thread
            size_t bufPos = 0;
            int localAliasCount = 0;

            // #pragma omp for collapse(2) schedule(dynamic)
            // #pragma omp for schedule(static)
            #pragma omp for collapse(2) schedule(dynamic)
            for (size_t i = 0; i < leftVec.size(); ++i) {
                for (size_t j = 0; j < rightVec.size(); ++j) {
                    const auto& storePtr = leftVec[i];
                    const auto& loadPtr = rightVec[j];
                    const StoreStmt* storeStmt = SVFUtil::dyn_cast<StoreStmt>(storePtr.node);
                    const LoadStmt* loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadPtr.node);
                    AliasResult result = pta->alias(storeStmt->getLHSVarID(), loadStmt->getRHSVarID());
                    int maxMetadataId = std::max(storePtr.metadataId, loadPtr.metadataId);
                    int minMetadataId = std::min(storePtr.metadataId, loadPtr.metadataId);

                    // Ensure there is room for a worst-case row (~64 bytes)
                    if (bufPos + 128 > buf.size()) {
                        localRows.append(buf.data(), bufPos);
                        bufPos = 0;
                    }

                    appendInt(buf.data(), bufPos, maxMetadataId);
                    appendBytes(buf.data(), bufPos, ":");
                    appendInt(buf.data(), bufPos, minMetadataId);
                    appendBytes(buf.data(), bufPos, "\t");
                    appendInt(buf.data(), bufPos, result == AliasResult::NoAlias ? 2 : 1);
                    appendBytes(buf.data(), bufPos, "\n");

                    ++localAliasCount;
                }
            }

            if (bufPos > 0) localRows.append(buf.data(), bufPos);

            threadRows[omp_get_thread_num()] = std::move(localRows);

            #pragma omp atomic
            noAliasCount += localAliasCount;
        }

        for (auto& r : threadRows) rows += r;

        return rows;
    };

    auto collectStoreStoreAliasRows = [&](const std::vector<StorePointersWithMetadata>& leftVec,
                                          const std::vector<StorePointersWithMetadata>& rightVec,
                                          int& noAliasCount) {
        std::string rows;
        std::vector<std::string> threadRows(omp_get_max_threads());
        std::cout << "Checking store-store alias relationships\n";
        #pragma omp parallel
        {
            std::string localRows;
            std::vector<char> buf(1 << 16);
            size_t bufPos = 0;
            int localAliasCount = 0;

            // #pragma omp for schedule(static)
            #pragma omp for schedule(dynamic, 8)
            for (size_t i = 0; i < leftVec.size(); ++i) {
                for (size_t j = i + 1; j < rightVec.size(); ++j) { // Avoid redundant pairs and self-pairs
                    const auto& storePtr = leftVec[i];
                    const auto& storePtr1 = rightVec[j];

                    const StoreStmt* storeStmt = SVFUtil::dyn_cast<StoreStmt>(storePtr.node);
                    const StoreStmt* storeStmt1 = SVFUtil::dyn_cast<StoreStmt>(storePtr1.node);

                    AliasResult result = pta->alias(storeStmt->getLHSVarID(), storeStmt1->getLHSVarID());
                    int maxMetadataId = std::max(storePtr.metadataId, storePtr1.metadataId);
                    int minMetadataId = std::min(storePtr.metadataId, storePtr1.metadataId);

                    if (bufPos + 128 > buf.size()) {
                        localRows.append(buf.data(), bufPos);
                        bufPos = 0;
                    }

                    appendInt(buf.data(), bufPos, maxMetadataId);
                    appendBytes(buf.data(), bufPos, ":");
                    appendInt(buf.data(), bufPos, minMetadataId);
                    appendBytes(buf.data(), bufPos, "\t");
                    appendInt(buf.data(), bufPos, result == AliasResult::NoAlias ? 2 : 1);
                    appendBytes(buf.data(), bufPos, "\n");

                    ++localAliasCount;
                }
            }

            if (bufPos > 0) localRows.append(buf.data(), bufPos);

            threadRows[omp_get_thread_num()] = std::move(localRows);

            #pragma omp atomic
            noAliasCount += localAliasCount;
        }

        for (auto& r : threadRows) rows += r;

        return rows;
    };

    #ifdef WRITE_LOAD_LOAD_ALIAS_TO_DB
    auto collectLoadLoadAliasRows = [&](const std::vector<LoadpointersWithMetadata>& leftVec,
                                        const std::vector<LoadpointersWithMetadata>& rightVec,
                                        int& noAliasCount) {
        std::string rows;
        std::vector<std::string> threadRows(omp_get_max_threads());
        std::cout << "Checking load-load alias relationships\n";
        #pragma omp parallel
        {
            std::string localRows;
            std::vector<char> buf(1 << 16);
            size_t bufPos = 0;
            int localAliasCount = 0;

            // #pragma omp for schedule(static)
            #pragma omp for schedule(dynamic, 8)
            for (size_t i = 0; i < leftVec.size(); ++i) {
                for (size_t j = i + 1; j < rightVec.size(); ++j) { // Avoid redundant pairs and self-pairs
                    const auto& loadPtr = leftVec[i];
                    const auto& loadPtr1 = rightVec[j];

                    const LoadStmt* loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadPtr.node);
                    const LoadStmt* loadStmt1 = SVFUtil::dyn_cast<LoadStmt>(loadPtr1.node);

                    AliasResult result = pta->alias(loadStmt->getRHSVarID(), loadStmt1->getRHSVarID());
                    int maxMetadataId = std::max(loadPtr.metadataId, loadPtr1.metadataId);
                    int minMetadataId = std::min(loadPtr.metadataId, loadPtr1.metadataId);

                    if (bufPos + 128 > buf.size()) {
                        localRows.append(buf.data(), bufPos);
                        bufPos = 0;
                    }

                    appendInt(buf.data(), bufPos, maxMetadataId);
                    appendBytes(buf.data(), bufPos, ":");
                    appendInt(buf.data(), bufPos, minMetadataId);
                    appendBytes(buf.data(), bufPos, "\t");
                    appendInt(buf.data(), bufPos, result == AliasResult::NoAlias ? 2 : 1);
                    appendBytes(buf.data(), bufPos, "\n");

                    ++localAliasCount;
                }
            }

            if (bufPos > 0) localRows.append(buf.data(), bufPos);

            threadRows[omp_get_thread_num()] = std::move(localRows);

            #pragma omp atomic
            noAliasCount += localAliasCount;
        }

        for (auto& r : threadRows) rows += r;

        return rows;
    };
    #endif

    const auto loadStoreRows = collectLoadStoreAliasRows(
        storePointersVec,
        loadPointersVec,
        AliasCount_ls
    );

    const auto storeStoreRows = collectStoreStoreAliasRows(
        storePointersVec,
        storePointersVec,
        AliasCount_ss
    );

    #ifdef WRITE_LOAD_LOAD_ALIAS_TO_DB
    const auto loadLoadRows = collectLoadLoadAliasRows(
        loadPointersVec,
        loadPointersVec,
        AliasCount_ll
    );
    #endif
    // std::cout << "Finished checking alias relationships\n";
    // Write the collected rows sequentially to PostgreSQL. The expensive alias checks above run in parallel.
    const char* conninfo = "host=localhost dbname=mydb user=appuser password=secret";
    PGconn* conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection failed\n";
        assert(false && "Database connection failed");
    }

    std::string createTable = "CREATE UNLOGGED TABLE IF NOT EXISTS " + baseFilename +
                            " (ptr_ptr_func TEXT, aa INTEGER);";
    PGresult* res = PQexec(conn, createTable.c_str());
    PQclear(res);

    // std::string partitionCmd =
    //     "DO $$\n"
    //     "BEGIN\n"
    //     "FOR i IN 0..15 LOOP\n"
    //     "    EXECUTE format(\n"
    //     "        'CREATE UNLOGGED TABLE IF NOT EXISTS " + baseFilename + "_p%s PARTITION OF " + baseFilename +
    //     " FOR VALUES WITH (MODULUS 16, REMAINDER %s);',\n"
    //     "        i, i\n"
    //     "    );\n"
    //     "END LOOP;\n"
    //     "END $$;";
    // res = PQexec(conn, partitionCmd.c_str());
    // PQclear(res);

    res = PQexec(conn, "BEGIN");
    PQclear(res);

    res = PQexec(conn,
        "SET LOCAL synchronous_commit = off;"
        "SET LOCAL work_mem = '128MB';"
        "SET LOCAL maintenance_work_mem = '1GB';"
        // "SET LOCAL wal_compression = on;"
    );
    PQclear(res);
    // std::cout << "Inserting alias results into database...\n";
    auto copyRows = [&](const std::string& copyData, const std::string& copyCmd) {
        PGresult* copyResult = PQexec(conn, copyCmd.c_str());
        if (PQresultStatus(copyResult) != PGRES_COPY_IN) {
            std::cerr << "COPY init failed\n";
            PQclear(copyResult);
            PQfinish(conn);
            assert(false && "COPY init failed");
        }

        PQclear(copyResult);

        constexpr size_t COPY_BUFFER_SIZE = 1 << 20;  // 1 MB
        constexpr size_t FLUSH_THRESHOLD = COPY_BUFFER_SIZE - 256;  // Flush before reaching limit
        char batch[COPY_BUFFER_SIZE];
        size_t pos = 0;

        auto flushBatch = [&]() {
            if (pos == 0) {
                return;
            }
            if (PQputCopyData(conn, batch, static_cast<int>(pos)) != 1) {
                std::cerr << "PQputCopyData failed\n";
            }
            pos = 0;
        };

        if (!copyData.empty()) {
            size_t offset = 0;
            while (offset < copyData.size()) {
                if (pos > FLUSH_THRESHOLD) {
                    flushBatch();
                }

                const size_t spaceLeft = COPY_BUFFER_SIZE - pos;
                const size_t chunkSize = std::min(spaceLeft, copyData.size() - offset);
                std::memcpy(batch + pos, copyData.data() + offset, chunkSize);
                pos += chunkSize;
                offset += chunkSize;
            }
        }

        flushBatch();

        if (PQputCopyEnd(conn, NULL) != 1) {
            std::cerr << "PQputCopyEnd failed\n";
        }

        while ((copyResult = PQgetResult(conn)) != NULL) {
            PQclear(copyResult);
        }
    };
    // std::cout << "Inserting load-store alias results...\n";
    copyRows(loadStoreRows,
             "COPY " + baseFilename + " (ptr_ptr_func, aa) FROM STDIN");
    copyRows(storeStoreRows,
             "COPY " + baseFilename + " (ptr_ptr_func, aa) FROM STDIN");
    #ifdef WRITE_LOAD_LOAD_ALIAS_TO_DB
    copyRows(loadLoadRows,
             "COPY " + baseFilename + " (ptr_ptr_func, aa) FROM STDIN");
    #endif
    // std::cout << "Finished inserting alias results into database\n";
    res = PQexec(conn, "COMMIT");
    PQclear(res);
    // std::cout << "Finished inserting alias results into database\n";
    std::string createIndex = "CREATE INDEX idx_ptr_ptr_func ON " + baseFilename + " USING HASH (ptr_ptr_func);";
    res = PQexec(conn, createIndex.c_str());
    PQclear(res);
    PQfinish(conn);

    // #pragma omp taskwait
    // totalMayAliasCount = mayAliasCount_ll + mayAliasCount_ss + mayAliasCount_ls;
    totalAliasCount = AliasCount_ll + AliasCount_ss + AliasCount_ls;
    // SVFUtil::outs() << "Total MayAlias pairs after store: " << totalMayAliasCount << "\n";
    SVFUtil::outs() << "Total Alias pairs: " << totalAliasCount << "\n";
    #ifdef LOG_compile_time
    const auto buildDatabaseEnd = std::chrono::steady_clock::now();
    const std::chrono::duration<double> buildDatabaseElapsed = buildDatabaseEnd - buildDatabaseStart;
    std::cout << "Time to build database: " << buildDatabaseElapsed.count() << " seconds\n";
    #endif
// }
}
// // Perform interprocedural analysis between memory operations and call instructions
// void WPAPass::performInterproceduralMemoryCallAnalysis(PointerAnalysis* pta, SVFG* svfg, const std::string& outputPrefix) {
//     SVFIR* pag = pta->getPAG();

//     // Use the passed SVFG parameter instead of relying on member variable
//     if (svfg == nullptr) {
//         SVFUtil::outs() << "Error: SVFG parameter is null, cannot perform mod-ref analysis.\n";
//         return;
//     }

//     // Add detailed debugging for SVFG components
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Performing interprocedural memory-call analysis...\n";
//     SVFUtil::outs() << "SVFG Debug Info:\n";
//     SVFUtil::outs() << "- SVFG pointer: " << svfg << "\n";
//     SVFUtil::outs() << "- SVFG node count: " << svfg->getTotalNodeNum() << "\n";
//     SVFUtil::outs() << "- SVFG edge count: " << svfg->getTotalEdgeNum() << "\n";
//     #endif
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Pointer Analysis Debug Info:\n";
//     if (svfg->getMSSA() != nullptr) {
//         SVFUtil::outs() << "- MSSA is available\n";
//         if (svfg->getMSSA()->getMRGenerator() != nullptr) {
//             SVFUtil::outs() << "- MRGenerator is available\n";
//         } else {
//             SVFUtil::outs() << "- MRGenerator is NULL\n";
//         }
//     } else {
//         SVFUtil::outs() << "- MSSA is NULL\n";
//     }
//     #endif
//     std::vector<WPAPass::TestRow> callData;
//     // Create output file for interprocedural analysis results
//     #ifdef FILE_Write
//     std::string filename = outputPrefix + "_analysis_results.txt";
//     std::ofstream outputFile(filename);
//     if (!outputFile.is_open()) {
//         SVFUtil::outs() << "Error: Could not open file " << filename << " for writing.\n";
//         return;
//     }
//     #endif
//     // Structure to hold call instruction information
//     struct CallInstructionInfo {
//         const CallICFGNode* callNode;
//         std::string metadataId;
//         std::string functionName;
//         std::vector<const ValVar*> arguments;
//         const SVFVar* returnValue;

//         bool operator<(const CallInstructionInfo& other) const {
//             return callNode < other.callNode;
//         }
//     };

//     // Structure to hold memory operation information
//     struct MemoryOpInfo {
//         PAGNode* memoryLocation_node;
//         std::string metadataId;
//         std::string opType; // "load" or "store"
//         const SVFStmt* stmt;

//         bool operator<(const MemoryOpInfo& other) const {
//             if (memoryLocation_node != other.memoryLocation_node) {
//                 return memoryLocation_node < other.memoryLocation_node;
//             }
//             return metadataId < other.metadataId;
//         }
//     };

//     std::set<CallInstructionInfo> callInstructions;
//     std::set<MemoryOpInfo> memoryOperations;

//     // Collect call instructions
//     ICFG* icfg = pag->getICFG();
//     for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
//         const ICFGNode* node = it->second;
//         if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
//             #ifdef PRINT_CALL_DEBUG_INFO
//             SVFUtil::outs() << "CallNode: " << callNode->toString() << "\n";
//             SVFUtil::outs() << "Call instruction string: " << callNode->getCaller()->toString() << "\n";
//             SVFUtil::outs() << "Called function: "
//                            << (callNode->getCalledFunction() ? callNode->getCalledFunction()->getName() : "indirect_call") << "\n";
//             #endif
//                            // SVFUtil::outs() << "Function name of virtual call: " << callNode->funNameOfVcall() << "\n";
//              // Extract metadata from call instruction
//             CallInstructionInfo callInfo;
//             callInfo.callNode = callNode;
//             callInfo.metadataId = extractCallMetadataId(callNode, "call_id");
//             // Get function name being called
//             if (callNode->getCalledFunction()) {
//                 callInfo.functionName = callNode->getCalledFunction()->getName();
//             } else {
//                 callInfo.functionName = "indirect_call";
//             }

//             // Collect arguments passed to the function
//             for (u32_t i = 0; i < callNode->getActualParms().size(); ++i) {
//                 const ValVar* argNode = callNode->getActualParms()[i];
//                 callInfo.arguments.push_back(argNode);
//             }

//             // Get return value from corresponding RetICFGNode if any
//             const RetICFGNode* retNode = callNode->getRetICFGNode();
//             if (retNode && retNode->getActualRet()) {
//                 callInfo.returnValue = retNode->getActualRet();
//             } else {
//                 callInfo.returnValue = nullptr;
//             }

//             callInstructions.insert(callInfo);
//         }
//     }

//     // Collect memory operations (loads and stores)
//     for (SVFIR::iterator it = pag->begin(), eit = pag->end(); it != eit; ++it) {
//         PAGNode* node = it->second;

//         // Check outgoing edges for loads/stores
//         for (PAGNode::iterator edgeIt = node->OutEdgeBegin();
//              edgeIt != node->OutEdgeEnd(); ++edgeIt) {
//             PAGEdge* edge = *edgeIt;

//             if (SVFUtil::isa<LoadStmt>(edge)) {
//                 LoadStmt* loadStmt = SVFUtil::cast<LoadStmt>(edge);
//                 std::string memId = extractMetadataId(loadStmt);
//                 if (!memId.empty()) {
//                     MemoryOpInfo memOp;
//                     memOp.memoryLocation_node = loadStmt->getRHSVar();
//                     memOp.metadataId = memId;
//                     memOp.opType = "load";
//                     memOp.stmt = loadStmt;
//                     memoryOperations.insert(memOp);
//                 }
//             }
//             else if (SVFUtil::isa<StoreStmt>(edge)) {
//                 StoreStmt* storeStmt = SVFUtil::cast<StoreStmt>(edge);
//                 std::string memId = extractMetadataId(storeStmt);
//                 if (!memId.empty()) {
//                     MemoryOpInfo memOp;
//                     memOp.memoryLocation_node = storeStmt->getLHSVar();
//                     memOp.metadataId = memId;
//                     memOp.opType = "store";
//                     memOp.stmt = storeStmt;
//                     memoryOperations.insert(memOp);
//                 }
//             }
//         }

//         // Check incoming edges for loads/stores
//         for (PAGNode::iterator edgeIt = node->InEdgeBegin();
//              edgeIt != node->InEdgeEnd(); ++edgeIt) {
//             PAGEdge* edge = *edgeIt;

//             if (SVFUtil::isa<LoadStmt>(edge)) {
//                 LoadStmt* loadStmt = SVFUtil::cast<LoadStmt>(edge);
//                 std::string memId = extractMetadataId(loadStmt);
//                 if (!memId.empty()) {
//                     MemoryOpInfo memOp;
//                     memOp.memoryLocation_node = loadStmt->getRHSVar();
//                     memOp.metadataId = memId;
//                     memOp.opType = "load";
//                     memOp.stmt = loadStmt;
//                     memoryOperations.insert(memOp);
//                 }
//             }
//             else if (SVFUtil::isa<StoreStmt>(edge)) {
//                 StoreStmt* storeStmt = SVFUtil::cast<StoreStmt>(edge);
//                 std::string memId = extractMetadataId(storeStmt);
//                 if (!memId.empty()) {
//                     MemoryOpInfo memOp;
//                     memOp.memoryLocation_node = storeStmt->getLHSVar();
//                     memOp.metadataId = memId;
//                     memOp.opType = "store";
//                     memOp.stmt = storeStmt;
//                     memoryOperations.insert(memOp);
//                 }
//             }
//         }
//     }
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Interprocedural Analysis: Found " << callInstructions.size()
//                     << " call instructions and " << memoryOperations.size()
//                     << " memory operations\n";
//     #endif

//     #ifdef FILE_Write
//     outputFile << "=== Interprocedural Memory-Call Analysis Results ===\n";
//     outputFile << "Call Instructions: " << callInstructions.size() << "\n";
//     outputFile << "Memory Operations: " << memoryOperations.size() << "\n";
//     outputFile << "=================================================\n\n";
//     #endif
//     // Analyze relationships between memory operations and call instructions
//     for (const auto& memOp : memoryOperations) {
//         for (const auto& callInfo : callInstructions) {
//             // Check if call arguments alias with memory operation location
//             std::string callfun1 = callInfo.functionName.empty() ? "unknown" : callInfo.functionName;
//             #ifdef PRINT_CALL_DEBUG_INFO
//             const FunObjVar* fun2 = memOp.memoryLocation_node->getFunction();
//             #endif
//             #ifdef PRINT_CALL_DEBUG_INFO
//             std::cout << "before interprocedural" << callInfo.callNode->toString() << " " << memOp.stmt->toString() << "\n";
//             #endif
//             assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");

//             // Check if svfg and its components are valid
//             if(svfg == nullptr) {
//                 #ifdef PRINT_CALL_DEBUG_INFO
//                 SVFUtil::outs() << "Error: SVFG is null, cannot perform mod-ref analysis.\n";
//                 #endif
//                 continue; // Skip this iteration instead of returning
//             }

//             if(svfg->getMSSA() == nullptr) {
//                 #ifdef PRINT_CALL_DEBUG_INFO
//                 SVFUtil::outs() << "Error: MSSA is null, cannot perform mod-ref analysis.\n";
//                 #endif
//                 continue;
//             }

//             if(svfg->getMSSA()->getMRGenerator() == nullptr) {
//                 #ifdef PRINT_CALL_DEBUG_INFO
//                 SVFUtil::outs() << "Error: MRGenerator is null, cannot perform mod-ref analysis.\n";
//                 #endif
//                 continue;
//             }

//             // Perform mod-ref analysis with additional safety checks
//             ModRefInfo modRef;
//             try {
//                 modRef = svfg->getMSSA()->getMRGenerator()->getModRefInfo(callInfo.callNode, memOp.memoryLocation_node);
//                 #ifdef PRINT_CALL_DEBUG_INFO
//                 std::cout << "after interprocedural\n";
//                 #endif
//             } catch (...) {
//                 std::cout << "Exception caught during mod-ref analysis, skipping this pair\n";
//                 continue;
//             }
//             // Create database key in format: call_id mem_id alias_result
//             if(callInfo.metadataId.empty() || memOp.metadataId.empty()) {
//                 #ifdef PRINT_CALL_DEBUG_INFO
//                 SVFUtil::outs() << "Metadata ID is empty for call or memory operation.\n";
//                 #endif
//                 continue; // Skip if metadata ID is empty
//             }

//             std::string key = "$" + callInfo.metadataId + "@" + "Call" + "$" + memOp.metadataId + "@" + "Mem";
//             callData.push_back({key, (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef")});
//             #ifdef FILE_Write
//             outputFile << key << " " << (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef") << "\n";
//             #endif
//             // Debug output
//             #ifdef PRINT_CALL_DEBUG_INFO
//             SVFUtil::outs() << "Call pointer " << callfun1 << " " << callInfo.metadataId
//                            << " vs Mem pointer " << (fun2==nullptr?"nullptr":fun2->getName())
//                            << " " << memOp.metadataId << ": "
//                            << (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef") << "\n";
//             #endif
//         }
//     }
//     // for (const auto& CallOper : callInstructions) {
//     //     for (const auto& callInfo : callInstructions) {
//     //         // Check if call arguments alias with memory operation location
//     //         std::string callfun1 = callInfo.functionName.empty() ? "unknown" : callInfo.functionName;
//     //         std::string CallOperfun = CallOper.functionName.empty() ? "unknown" : CallOper.functionName;
//     //         #ifdef PRINT_CALL_DEBUG_INFO
//     //         std::cout << "before interprocedural" << callInfo.callNode->toString() << " " << CallOper->toString() << "\n";
//     //         #endif
//     //         assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");

//     //         // Check if svfg and its components are valid
//     //         if(svfg == nullptr) {
//     //             #ifdef PRINT_CALL_DEBUG_INFO
//     //             SVFUtil::outs() << "Error: SVFG is null, cannot perform mod-ref analysis.\n";
//     //             #endif
//     //             continue; // Skip this iteration instead of returning
//     //         }

//     //         if(svfg->getMSSA() == nullptr) {
//     //             #ifdef PRINT_CALL_DEBUG_INFO
//     //             SVFUtil::outs() << "Error: MSSA is null, cannot perform mod-ref analysis.\n";
//     //             #endif
//     //             continue;
//     //         }

//     //         if(svfg->getMSSA()->getMRGenerator() == nullptr) {
//     //             #ifdef PRINT_CALL_DEBUG_INFO
//     //             SVFUtil::outs() << "Error: MRGenerator is null, cannot perform mod-ref analysis.\n";
//     //             #endif
//     //             continue;
//     //         }

//     //         // Perform mod-ref analysis with additional safety checks
//     //         ModRefInfo modRef;
//     //         try {
//     //             modRef = svfg->getMSSA()->getMRGenerator()->getModRefInfo(callInfo.callNode, CallOper.callNode);
//     //             #ifdef PRINT_CALL_DEBUG_INFO
//     //             std::cout << "after interprocedural\n";
//     //             #endif
//     //         } catch (...) {
//     //             std::cout << "Exception caught during mod-ref analysis, skipping this pair\n";
//     //             continue;
//     //         }
//     //         // Create database key in format: call_id mem_id alias_result
//     //         if(callInfo.metadataId.empty() || CallOper.metadataId.empty()) {
//     //             #ifdef PRINT_CALL_DEBUG_INFO
//     //             SVFUtil::outs() << "Metadata ID is empty for call or memory operation.\n";
//     //             #endif
//     //             continue; // Skip if metadata ID is empty
//     //         }

//     //         std::string key = "$" + callInfo.metadataId + "@" + "Call" + "$" + CallOper.metadataId + "@" + "Call";
//     //         callData.push_back({key, (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef")});
//     //         #ifdef FILE_Write
//     //         outputFile << key << " " << (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef") << "\n";
//     //         #endif
//     //         // Debug output
//     //         #ifdef PRINT_CALL_DEBUG_INFO
//     //         SVFUtil::outs() << "Call pointer " << callfun1 << " " << callInfo.metadataId
//     //                        << " vs Mem pointer " << (fun2==nullptr?"nullptr":fun2->getName())
//     //                        << " " << CallOp.metadataId << ": "
//     //                        << (modRef == ModRefInfo::NoModRef ? "NoModRef" : "ModRef") << "\n";
//     //         #endif
//     //     }
//     // }
//     #ifdef FILE_Write
//     outputFile.close();
//     #endif
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Interprocedural analysis results written to: " << filename << "\n";
//     #endif

//     // Insert interprocedural analysis results into database
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Inserting " << callData.size()
//                     << " interprocedural relationships into database\n";
//     #endif
//     // bulkInsertNewTests(outputPrefix, callData, 10000);
//     #ifdef MYSQL_DB
//     CreateMultipleTables(outputPrefix, callData, 10000); // Create multiple tables if data size exceeds threshold
//     #elif defined(USE_POSTGRESQL)
//     const char* conninfo = "host=localhost dbname=mydb user=aadb_umu password=caps";
//     createMultipleTablesPostgres(outputPrefix, callData, 2000000, 10000, conninfo);
//     #endif
//     #ifdef PRINT_CALL_DEBUG_INFO
//     SVFUtil::outs() << "Interprocedural analysis data inserted into database\n";
//     #endif
// }

const PointsTo& WPAPass::getPts(NodeID var)
{
    assert(_pta && "initialize a pointer analysis first");
    return _pta->getPts(var);
}

/*!
 * Return mod-ref result of a Callsite
 */
ModRefInfo WPAPass::getModRefInfo(const CallICFGNode* callInst)
{
    assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(callInst);
}


// /*!
//  * Return mod-ref results of a CallInst to a specific memory location
//  */
// ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst, const Value* V) {
//     assert(PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && anderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
//     return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(SVFUtil::getLLVMCallSite(callInst), V);
// }
/*!
 * Return mod-ref result between two CallInsts
 */
ModRefInfo WPAPass::getModRefInfo(const CallICFGNode* callInst1, const CallICFGNode* callInst2)
{
    assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(callInst1, callInst2);
}

/*!
 * Return mod-ref results of a Callsite to a specific memory location
 */
// ModRefInfo getModRefInfo(const CallICFGNode* callInst, const SVFValue* Loc)
// {
//     assert(Options::PASelected(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG() && "mod-ref query is only support with -ander and -svfg turned on");
//     // ICFG* icfg = _svfg->getPAG()->getICFG();
//     // const CallICFGNode* cbn = icfg->getCallICFGNode(callInst.getInstruction());
//     return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(callInst, Loc);
// }
