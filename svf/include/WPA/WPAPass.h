//===- WPAPass.h -- Whole program analysis------------------------------------//
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
 * @file: WPA.h
 * @author: yesen
 * @date: 10/06/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#ifndef WPA_H_
#define WPA_H_

#include "MemoryModel/PointerAnalysisImpl.h"

namespace SVF
{

class SVFG;
class SVFModule;

/*!
 * Whole program pointer analysis.
 * This class performs various pointer analysis on the given module.
 */
// excised ", public llvm::AliasAnalysis" as that has a very light interface
// and I want to see what breaks.
class WPAPass
{
    typedef std::vector<PointerAnalysis*> PTAVector;

public:
    /// Pass ID
    static char ID;

    enum AliasCheckRule
    {
        Conservative,	///< return MayAlias if any pta says alias
        Veto,			///< return NoAlias if any pta says no alias
        Precise			///< return alias result by the most precise pta
    };

    /// Constructor needs TargetLibraryInfo to be passed to the AliasAnalysis
    WPAPass()
    {

    }

    /// Destructor
    virtual ~WPAPass();

    /// Retrieve points-to set information
    virtual const PointsTo& getPts(NodeID var);

    /// Print all alias pairs
    virtual void PrintAliasPairs(PointerAnalysis* pta, SVFG* svfg);

    /// Add load/store pointer alias information to database
    virtual void addLoadStoreAliasInfoToDatabase(PointerAnalysis* pta, SVFG* svfg);

    /// Perform interprocedural analysis between memory operations and call instructions
    // virtual void performInterproceduralMemoryCallAnalysis(PointerAnalysis* pta, SVFG* svfg, const std::string& outputPrefix);

    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref any memory location
    virtual ModRefInfo getModRefInfo(const CallICFGNode* callInst);

    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref a specific memory location, given Location infos
    // virtual inline ModRefInfo getModRefInfo(const CallSite callInst, const MemoryLocation& Loc)
    // {
    //     return getModRefInfo(callInst, Loc.Ptr);
    // }
    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref a specific memory location, given Location infos
    // virtual inline ModRefInfo getModRefInfo(const CallICFGNode* callInst, const SVFValue* Loc);
    /// Interface of mod-ref analysis between two CallSite instructions
    virtual ModRefInfo getModRefInfo(const CallICFGNode* callInst1, const CallICFGNode* callInst2);

    /// Run pointer analysis on SVFModule
    virtual void runOnModule(SVFIR* svfModule);

    /// PTA name
    virtual inline std::string getPassName() const
    {
        return "WPAPass";
    }

private:
    /// Create pointer analysis according to specified kind and analyze the module.
    struct TestRow {
    std::string ptr_ptr_func;
    std::string aa;
    };
    struct Row {
    int table_id;
    std::string ptr_ptr_func;
    std::string aa;
    };
    void runPointerAnalysis(SVFIR* pag, u32_t kind);
    void bulkInsertNewTests(const std::string& tableName, const std::vector<TestRow>& data, int batchSize = 100);
    void CreateMultipleTables(const std::string& baseTableName, const std::vector<TestRow>& data, int batchSize = 100);
    // void bulkInsertPostgres(const char* conninfo, const std::string& tableName, const std::vector<TestRow>& data, int batchSize = 100);
    // void createMultipleTablesPostgres(const std::string& baseTableName, const std::vector<TestRow>& data, int batchSizePerTable, int batchSizeInsert, const char* conninfo);
    PTAVector ptaVector;	///< all pointer analysis to be executed.
    PointerAnalysis* _pta;	///<  pointer analysis to be executed.
    SVFG* _svfg;  ///< svfg generated through -ander pointer analysis
};

} // End namespace SVF

#endif /* WPA_H_ */
