//===- AEStat.h -- Statistics for Abstract Execution----------//
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

#pragma once
#include "SVFIR/SVFType.h"
#include "Util/SVFUtil.h"
#include "Util/SVFStat.h"
#include <string>

namespace SVF
{

class AbstractInterpretation;

/// AEStat: Statistic for AE
class AEStat : public SVFStat
{
public:
    void countStateSize();
    AEStat(AbstractInterpretation* ae) : _ae(ae)
    {
        startTime = getClk(true);
    }
    ~AEStat()
    {
    }
    inline std::string getMemUsage()
    {
        u32_t vmrss, vmsize;
        return SVFUtil::getMemoryUsageKB(&vmrss, &vmsize) ? std::to_string(vmsize) + "KB" : "cannot read memory usage";
    }

    void finializeStat();
    void performStat() override;

public:
    AbstractInterpretation* _ae;
    s32_t count{0};
    std::string memory_usage;
    std::string memUsage;


    u32_t& getFunctionTrace()
    {
        if (generalNumMap.count("Function_Trace") == 0)
        {
            generalNumMap["Function_Trace"] = 0;
        }
        return generalNumMap["Function_Trace"];
    }
    u32_t& getBlockTrace()
    {
        if (generalNumMap.count("Block_Trace") == 0)
        {
            generalNumMap["Block_Trace"] = 0;
        }
        return generalNumMap["Block_Trace"];
    }
    u32_t& getICFGNodeTrace()
    {
        if (generalNumMap.count("ICFG_Node_Trace") == 0)
        {
            generalNumMap["ICFG_Node_Trace"] = 0;
        }
        return generalNumMap["ICFG_Node_Trace"];
    }
};

} // namespace SVF
