//===- CFLSVFGBuilder.h -- Building SVFG for CFL--------------------------//
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

//
// Created by Xiao on 30/12/23.
//

#ifndef SVF_CFLSVFGBUILDER_H
#define SVF_CFLSVFGBUILDER_H

#include "SABER/SaberSVFGBuilder.h"

namespace SVF
{

class CFLSVFGBuilder: public SaberSVFGBuilder
{
public:
    typedef Set<const SVFGNode*> SVFGNodeSet;
    typedef Map<NodeID, PointsTo> NodeToPTSSMap;
    typedef FIFOWorkList<NodeID> WorkList;

    /// Constructor
    CFLSVFGBuilder() = default;

    /// Destructor
    virtual ~CFLSVFGBuilder() = default;

protected:
    /// Re-write create SVFG method
    virtual void buildSVFG();

    /// Remove Incoming Edge for strong-update (SU) store instruction
    /// Because the SU node does not receive indirect value
    virtual void rmIncomingEdgeForSUStore(BVDataPTAImpl* pta);
};

}
#endif //SVF_CFLSVFGBUILDER_H
