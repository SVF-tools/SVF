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
    virtual AliasResult alias(const Value* V1,	const Value* V2);

    /// Interface expose to users of our pointer analysis, given PAGNodes
    virtual AliasResult alias(NodeID V1, NodeID V2);

    /// We start from here
    virtual void runOnModule(SVFModule* module);

    /// We start from here
    virtual bool runOnModule(Module& module);

    /// Select a client
    virtual void selectClient(SVFModule* module);

    /// Pass name
    virtual inline StringRef getPassName() const
    {
        return "DDAPass";
    }

private:
    /// Print queries' pts
    void printQueryPTS();
    /// Create pointer analysis according to specified kind and analyze the module.
    void runPointerAnalysis(SVFModule* module, u32_t kind);
    /// Context insensitive Edge for DDA
    void initCxtInsensitiveEdges(PointerAnalysis* pta, const SVFG* svfg,const SVFGSCC* svfgSCC, SVFGEdgeSet& insensitveEdges);
    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    bool edgeInSVFGSCC(const SVFGSCC* svfgSCC,const SVFGEdge* edge);
    /// Return TRUE if this edge is inside a SVFG SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    bool edgeInCallGraphSCC(PointerAnalysis* pta,const SVFGEdge* edge);

    void collectCxtInsenEdgeForRecur(PointerAnalysis* pta, const SVFG* svfg,SVFGEdgeSet& insensitveEdges);
    void collectCxtInsenEdgeForVFCycle(PointerAnalysis* pta, const SVFG* svfg,const SVFGSCC* svfgSCC, SVFGEdgeSet& insensitveEdges);

    PointerAnalysis* _pta;	///<  pointer analysis to be executed.
    DDAClient* _client;		///<  DDA client used

};

} // End namespace SVF

#endif /* WPA_H_ */
