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

class SVFModule;
class SVFG;

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

    /// Interface expose to users of our pointer analysis, given Value infos
    virtual AliasResult alias(const SVFValue* V1,	const SVFValue* V2);

    /// Retrieve points-to set information
    virtual const PointsTo& getPts(const SVFValue* value);
    virtual const PointsTo& getPts(NodeID var);

    /// Print all alias pairs
    virtual void PrintAliasPairs(PointerAnalysis* pta);

    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref any memory location
    virtual ModRefInfo getModRefInfo(const CallSite callInst);

    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref a specific memory location, given Location infos
    // virtual inline ModRefInfo getModRefInfo(const CallSite callInst, const MemoryLocation& Loc)
    // {
    //     return getModRefInfo(callInst, Loc.Ptr);
    // }

    /// Interface of mod-ref analysis to determine whether a CallSite instruction can mod or ref a specific memory location, given Value infos
    virtual ModRefInfo getModRefInfo(const CallSite callInst, const SVFValue* V);

    /// Interface of mod-ref analysis between two CallSite instructions
    virtual ModRefInfo getModRefInfo(const CallSite callInst1, const CallSite callInst2);

    /// Run pointer analysis on SVFModule
    virtual void runOnModule(SVFIR* svfModule);

    /// PTA name
    virtual inline std::string getPassName() const
    {
        return "WPAPass";
    }

private:
    /// Create pointer analysis according to specified kind and analyze the module.
    void runPointerAnalysis(SVFIR* pag, u32_t kind);

    PTAVector ptaVector;	///< all pointer analysis to be executed.
    PointerAnalysis* _pta;	///<  pointer analysis to be executed.
    SVFG* _svfg;  ///< svfg generated through -ander pointer analysis
};

} // End namespace SVF

#endif /* WPA_H_ */
