//===- SVFStat.cpp -- Base class for statistics---------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFStat.cpp
 *
 *  Created on: Sep 1, 2022
 *      Author: Xiao Cheng
 */

#include "Util/Options.h"
#include "Util/SVFStat.h"

using namespace SVF;

SVFStat::SVFStat() : startTime(0), endTime(0)
{
    assert((Options::ClockType == ClockType::Wall || Options::ClockType == ClockType::CPU)
           && "PTAStat: unknown clock type!");
}

double SVFStat::getClk(bool mark)
{
    if (Options::MarkedClocksOnly && !mark) return 0.0;

    if (Options::ClockType == ClockType::Wall)
    {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        return (double)(time.tv_nsec + time.tv_sec * 1000000000) / 1000000.0;
    }
    else if (Options::ClockType == ClockType::CPU)
    {
        return CLOCK_IN_MS();
    }

    assert(false && "PTAStat::getClk: unknown clock type");
    abort();
}