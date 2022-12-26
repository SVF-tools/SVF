//===- DDAPass.h -- Demand-driven analysis driver pass-------------//
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
 * @file: DDAPass.h
 * @author: Yulei Sui
 * @date: 01/07/2014
 * @version: 1.0
 *
 */


#ifndef DDAPASS_H_
#define DDAPASS_H_

#include "MemoryModel/PointerAnalysisImpl.h"
#include "DDA/DDAClient.h"
#include "Util/SCC.h"

namespace SVF
{

/*!
 * Demand-Driven Pointer Analysis.
 * This class performs various pointer analysis on the given module.
 */
class DDAPass
{

public:
    /// Pass ID
    static char ID;
    typedef SCCDetection<SVFG*> SVFGSCC;
    typedef OrderedSet<const SVFGEdge*> SVFGEdgeSet;
    typedef std::vector<PointerAnalysis*> PTAVector;

    DDAPass() : _pta(nullptr), _client(nullptr) {}
    ~DDAPass();

    /// Interface expose to users of our pointer analysis, given Value infos
    virtual AliasResult alias(const SVFValue* V1,	const SVFValue* V2);

    /// Interface expose to users of our pointer analysis, given PAGNodes
    virtual AliasResult alias(NodeID V1, NodeID V2);

    /// We start from here
    virtual void runOnModule(SVFIR* module);

    /// Select a client
    virtual void selectClient(SVFModule* module);

    /// Pass name
    virtual inline std::string getPassName() const
    {
        return "DDAPass";
    }

private:
    /// Print queries' pts
    void printQueryPTS();
    /// Create pointer analysis according to specified kind and analyze the module.
    void runPointerAnalysis(SVFIR* module, u32_t kind);
    /// Context insensitive Edge for DDA
    void initCxtInsensitiveEdges(PointerAnalysis* pta, const SVFG* svfg,const SVFGSCC* svfgSCC, SVFGEdgeSet& insensitveEdges);
    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    bool edgeInSVFGSCC(const SVFGSCC* svfgSCC,const SVFGEdge* edge);
    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    bool edgeInCallGraphSCC(PointerAnalysis* pta,const SVFGEdge* edge);

    void collectCxtInsenEdgeForRecur(PointerAnalysis* pta, const SVFG* svfg,SVFGEdgeSet& insensitveEdges);
    void collectCxtInsenEdgeForVFCycle(PointerAnalysis* pta, const SVFG* svfg,const SVFGSCC* svfgSCC, SVFGEdgeSet& insensitveEdges);

    std::unique_ptr<PointerAnalysis> _pta;	///<  pointer analysis to be executed.
    DDAClient* _client;		///<  DDA client used

};

} // End namespace SVF

#endif /* WPA_H_ */
