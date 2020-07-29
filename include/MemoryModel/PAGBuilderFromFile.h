//===- PAGBuilderFromFile.h -- Building PAG from File--------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * PAGBuilderFromFile.h
 *
 *  Created on: 20 Sep. 2018
 *      Author: 136884
 */

#ifndef INCLUDE_MEMORYMODEL_PAGBUILDERFROMFILE_H_
#define INCLUDE_MEMORYMODEL_PAGBUILDERFROMFILE_H_

#include "Graphs/PAG.h"

namespace SVF
{

/*!
 * Build PAG from a user specified file (for debugging purpose)
 */
class PAGBuilderFromFile
{

private:
    PAG* pag;
    std::string file;
public:
    /// Constructor
    PAGBuilderFromFile(std::string f) :
        pag(PAG::getPAG(true)), file(f)
    {
    }
    /// Destructor
    ~PAGBuilderFromFile()
    {
    }

    /// Return PAG
    PAG* getPAG() const
    {
        return pag;
    }

    /// Return file name
    std::string getFileName() const
    {
        return file;
    }

    /// Start building
    PAG* build();

    // Add edges
    void addEdge(NodeID nodeSrc, NodeID nodeDst, Size_t offset,
                 std::string edge);
};

} // End namespace SVF

#endif /* INCLUDE_MEMORYMODEL_PAGBUILDERFROMFILE_H_ */
