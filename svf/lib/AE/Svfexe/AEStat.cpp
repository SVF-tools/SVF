//===- AEStat.cpp -- Statistics for Abstract Execution----------//
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

#include "AE/Svfexe/AEStat.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVFIR/SVFIR.h"

using namespace SVF;
using namespace SVFUtil;

// count the size of memory map
void AEStat::countStateSize()
{
    if (count == 0)
    {
        generalNumMap["ES_Var_AVG_Num"] = 0;
        generalNumMap["ES_Loc_AVG_Num"] = 0;
        generalNumMap["ES_Var_Addr_AVG_Num"] = 0;
        generalNumMap["ES_Loc_Addr_AVG_Num"] = 0;
    }
    ++count;
}


void AEStat::finializeStat()
{
    memUsage = getMemUsage();
    if (count > 0)
    {
        generalNumMap["ES_Var_AVG_Num"] /= count;
        generalNumMap["ES_Loc_AVG_Num"] /= count;
        generalNumMap["ES_Var_Addr_AVG_Num"] /= count;
        generalNumMap["ES_Loc_Addr_AVG_Num"] /= count;
    }
    generalNumMap["SVF_STMT_NUM"] = count;

    u32_t totalICFGNodes = _ae->svfir->getICFG()->nodeNum;
    generalNumMap["ICFG_Node_Num"] = totalICFGNodes;

    // Calculate coverage: use allAnalyzedNodes which tracks all nodes across all entry points
    u32_t analyzedNodes = _ae->allAnalyzedNodes.size();
    generalNumMap["Analyzed_ICFG_Node_Num"] = analyzedNodes;

    // Coverage percentage (stored as integer percentage * 100 for precision)
    if (totalICFGNodes > 0)
    {
        double coveragePercent = (double)analyzedNodes / (double)totalICFGNodes * 100.0;
        generalNumMap["ICFG_Coverage_Percent"] = (u32_t)(coveragePercent * 100); // Store as percentage * 100
    }
    else
    {
        generalNumMap["ICFG_Coverage_Percent"] = 0;
    }

    u32_t callSiteNum = 0;
    u32_t extCallSiteNum = 0;
    Set<const FunObjVar *> funs;
    Set<const FunObjVar *> analyzedFuns;
    for (const auto &it: *_ae->svfir->getICFG())
    {
        if (it.second->getFun())
        {
            funs.insert(it.second->getFun());
            // Check if this node was analyzed (across all entry points)
            if (_ae->allAnalyzedNodes.find(it.second) != _ae->allAnalyzedNodes.end())
            {
                analyzedFuns.insert(it.second->getFun());
            }
        }
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(it.second))
        {
            if (!isExtCall(callNode))
            {
                callSiteNum++;
            }
            else
            {
                extCallSiteNum++;
            }
        }
    }
    generalNumMap["Func_Num"] = funs.size();
    generalNumMap["Analyzed_Func_Num"] = analyzedFuns.size();

    // Function coverage percentage
    if (funs.size() > 0)
    {
        double funcCoveragePercent = (double)analyzedFuns.size() / (double)funs.size() * 100.0;
        generalNumMap["Func_Coverage_Percent"] = (u32_t)(funcCoveragePercent * 100); // Store as percentage * 100
    }
    else
    {
        generalNumMap["Func_Coverage_Percent"] = 0;
    }

    generalNumMap["EXT_CallSite_Num"] = extCallSiteNum;
    generalNumMap["NonEXT_CallSite_Num"] = callSiteNum;
    timeStatMap["Total_Time(sec)"] = (double)(endTime - startTime) / TIMEINTERVAL;

}

void AEStat::performStat()
{
    std::string fullName(_ae->moduleName);
    std::string name;
    std::string moduleName;
    if (fullName.find('/') == std::string::npos)
    {
        std::string name = fullName;
        moduleName = name.substr(0, fullName.find('.'));
    }
    else
    {
        std::string name = fullName.substr(fullName.find('/'), fullName.size());
        moduleName = name.substr(0, fullName.find('.'));
    }

    SVFUtil::outs() << "\n************************\n";
    SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
    SVFUtil::outs().flags(std::ios::left);
    unsigned field_width = 30;
    for (NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it != eit; ++it)
    {
        // Special handling for percentage fields (stored as percentage * 100)
        if (it->first == "ICFG_Coverage_Percent" || it->first == "Func_Coverage_Percent")
        {
            double percent = (double)it->second / 100.0;
            std::cout << std::setw(field_width) << it->first << std::fixed << std::setprecision(2) << percent << "%\n";
        }
        else
        {
            std::cout << std::setw(field_width) << it->first << it->second << "\n";
        }
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it)
    {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "Memory usage: " << memUsage << "\n";

    SVFUtil::outs() << "#######################################################" << std::endl;
    SVFUtil::outs().flush();
}
