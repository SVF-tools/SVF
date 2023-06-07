//===----- CFLVF.h -- CFL Value-Flow Client--------------//
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
 * CFLVF.h
 *
 *  Created on: September 5, 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFLVF_H_
#define INCLUDE_CFL_CFLVF_H_


#include "CFL/CFLBase.h"
#include "CFL/CFLStat.h"
#include "SABER/SaberSVFGBuilder.h"
#include "WPA/Andersen.h"

namespace SVF
{
class CFLVF : public CFLBase
{

public:
    CFLVF(SVFIR* ir) : CFLBase(ir, PointerAnalysis::CFLFSCS_WPA)
    {
    }

    /// Parameter Checking
    virtual void checkParameter();

    /// Initialize the grammar, graph, solver
    virtual void initialize();

    /// Print grammar and graph
    virtual void finalize();

    /// Build CFLGraph via VFG
    void buildCFLGraph();

private:
    SaberSVFGBuilder memSSA;
    SVFG* svfg;
};

} // End namespace SVF

#endif /* INCLUDE_CFL_CFLVF_H_*/
