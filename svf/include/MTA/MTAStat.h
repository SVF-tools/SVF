//===- MTAStat.h -- Statistics for MTA-------------//
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
 * MTAStat.h
 *
 *  Created on: Jun 23, 2015
 *      Author: Yulei Sui, Peng Di
 */

#ifndef MTASTAT_H_
#define MTASTAT_H_

#include "Util/PTAStat.h"
#include "Util/SVFUtil.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace SVF
{

class ScopedPhaseTimer
{
public:
    explicit ScopedPhaseTimer(const char* phaseName)
        : name(phaseName), start(std::chrono::steady_clock::now())
    {
        SVFUtil::outs() << "[TIMER] Phase: " << name << " - started\n";
        SVFUtil::outs().flush();
    }

    ~ScopedPhaseTimer()
    {
        const auto end = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
                end - start).count();
        std::ostringstream elapsed;
        elapsed << std::fixed << std::setprecision(2) << milliseconds << " ms";
        if (milliseconds >= 1000.0)
            elapsed << " (" << std::fixed << std::setprecision(2)
                    << (milliseconds / 1000.0) << " s)";
        SVFUtil::outs() << "[TIMER] Phase: " << name << " - finished in "
                        << elapsed.str() << "\n";
        SVFUtil::outs().flush();
    }

private:
    const char* name;
    std::chrono::steady_clock::time_point start;
};

class ThreadCallGraph;
class TCT;
class MHP;
class LockAnalysis;
class MTAAnnotator;
/*!
 * Statistics for MTA
 */
class MTAStat : public PTAStat
{

public:
    /// Constructor
    MTAStat():PTAStat(nullptr),TCTTime(0),MHPTime(0),AnnotationTime(0)
    {
    }
    /// Statistics for thread call graph
    void performThreadCallGraphStat(ThreadCallGraph* tcg);
    /// Statistics for thread creation tree
    void performTCTStat(TCT* tct);

    double TCTTime;
    double MHPTime;
    double AnnotationTime;
};

} // End namespace SVF

#endif /* MTASTAT_H_ */
