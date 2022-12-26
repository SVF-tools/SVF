//===- SVFStat.h -- Base class for statistics---------------------------------//
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
 * SVFStat.h
 *
 *  Created on: Sep 1, 2022
 *      Author: Xiao Cheng
 */

#ifndef SVF_SVFSTAT_H
#define SVF_SVFSTAT_H

namespace SVF
{


/*!
 * Pointer Analysis Statistics
 */
class SVFStat
{
public:

    typedef OrderedMap<const char *, u32_t> NUMStatMap;

    typedef OrderedMap<const char *, double> TIMEStatMap;

    enum ClockType
    {
        Wall,
        CPU,
    };

    SVFStat();

    virtual ~SVFStat() {}

    virtual inline void startClk()
    {
        startTime = getClk(true);
    }

    virtual inline void endClk()
    {
        endTime = getClk(true);
    }

    /// When mark is true, real clock is always returned. When mark is false, it is
    /// only returned when Options::MarkedClocksOnly is not set.
    /// Default call for getClk is unmarked, while MarkedClocksOnly is false by default.
    static double getClk(bool mark = false);

    NUMStatMap generalNumMap;
    NUMStatMap PTNumStatMap;
    TIMEStatMap timeStatMap;

    double startTime;
    double endTime;

    virtual void performStat() = 0;

    virtual void printStat(std::string str = "");

    virtual void performStatPerQuery(NodeID) {}

    virtual void printStatPerQuery(NodeID, const PointsTo &) {}

    virtual void callgraphStat() {}

    static double timeOfBuildingLLVMModule;
    static double timeOfBuildingSymbolTable;
    static double timeOfBuildingSVFIR;

private:
    void branchStat();
    std::string moduleName;
}; // End class SVFStat

} // End namespace SVF
#endif //SVF_SVFSTAT_H
