//===- PointerAnalysis.h -- Base class of pointer analyses--------------------//
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
 * PointerAnalysis.h
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#ifndef POINTERANALYSIS_H_
#define POINTERANALYSIS_H_

#include <unistd.h>
#include <signal.h>

#include "Graphs/PAG.h"
#include "MemoryModel/ConditionalPT.h"
#include "MemoryModel/AbstractPointsToDS.h"
#include "MemoryModel/MutablePointsToDS.h"
#include "MemoryModel/PersistentPointsToDS.h"
#include "Graphs/PTACallGraph.h"
#include "Util/SCC.h"
#include "MemoryModel/PointsTo.h"

namespace SVF
{

class CommonCHGraph;

class TypeSystem;
class SVFModule;
class ICFG;
class PTAStat;
/*
 * Pointer Analysis Base Class
 */
class PointerAnalysis
{

public:
    /// Pointer analysis type list
    enum PTATY
    {
        // Whole program analysis
        Andersen_BASE,		///< Base Andersen PTA
        Andersen_WPA,		///< Andersen PTA
        AndersenLCD_WPA,	///< Lazy cycle detection andersen-style WPA
        AndersenHCD_WPA,    ///< Hybird cycle detection andersen-style WPA
        AndersenHLCD_WPA,   ///< Hybird lazy cycle detection andersen-style WPA
        AndersenSCD_WPA,    ///< Selective cycle detection andersen-style WPA
        AndersenSFR_WPA,    ///< Stride-based field representation
        AndersenWaveDiff_WPA,	///< Diff wave propagation andersen-style WPA
        AndersenWaveDiffWithType_WPA,	///< Diff wave propagation with type info andersen-style WPA
        Steensgaard_WPA,      ///< Steensgaard PTA
        CSCallString_WPA,	///< Call string based context sensitive WPA
        CSSummary_WPA,		///< Summary based context sensitive WPA
        FSDATAFLOW_WPA,	///< Traditional Dataflow-based flow sensitive WPA
        FSSPARSE_WPA,		///< Sparse flow sensitive WPA
        FSTBHC_WPA,		///< Sparse flow-sensitive type-based heap cloning WPA
        VFS_WPA,		///< Versioned sparse flow-sensitive WPA
        FSCS_WPA,			///< Flow-, context- sensitive WPA
        FSCSPS_WPA,		///< Flow-, context-, path- sensitive WPA
        ADAPTFSCS_WPA,		///< Adaptive Flow-, context-, sensitive WPA
        ADAPTFSCSPS_WPA,	///< Adaptive Flow-, context-, path- sensitive WPA
        TypeCPP_WPA, ///<  Type-based analysis for C++

        // Demand driven analysis
        FieldS_DDA,		///< Field sensitive DDA
        FlowS_DDA,		///< Flow sensitive DDA
        PathS_DDA,		///< Guarded value-flow DDA
        Cxt_DDA,		///< context sensitive DDA


        Default_PTA		///< default pta without any analysis
    };

    /// Implementation type: BVDataPTAImpl or CondPTAImpl.
    enum PTAImplTy
    {
        BaseImpl,   ///< Represents PointerAnalaysis.
        BVDataImpl, ///< Represents BVDataPTAImpl.
        CondImpl,   ///< Represents CondPTAImpl.
    };

    /// Indirect call edges type, map a callsite to a set of callees
    //@{
    typedef llvm::AliasAnalysis AliasAnalysis;
    typedef Set<const CallBlockNode*> CallSiteSet;
    typedef PAG::CallSiteToFunPtrMap CallSiteToFunPtrMap;
    typedef Set<const SVFFunction*> FunctionSet;
    typedef OrderedMap<const CallBlockNode*, FunctionSet> CallEdgeMap;
    typedef SCCDetection<PTACallGraph*> CallGraphSCC;
    typedef Set<const GlobalValue*> VTableSet;
    typedef Set<const SVFFunction*> VFunSet;
    //@}

    static const std::string aliasTestMayAlias;
    static const std::string aliasTestMayAliasMangled;
    static const std::string aliasTestNoAlias;
    static const std::string aliasTestNoAliasMangled;
    static const std::string aliasTestPartialAlias;
    static const std::string aliasTestPartialAliasMangled;
    static const std::string aliasTestMustAlias;
    static const std::string aliasTestMustAliasMangled;
    static const std::string aliasTestFailMayAlias;
    static const std::string aliasTestFailMayAliasMangled;
    static const std::string aliasTestFailNoAlias;
    static const std::string aliasTestFailNoAliasMangled;

private:
    /// Release the memory
    void destroy();

protected:

    /// User input flags
    //@{
    /// Flag for printing the statistic results
    bool print_stat;
    /// Flag for validating points-to/alias results
    bool alias_validation;
    /// Flag for iteration budget for on-the-fly statistics
    u32_t OnTheFlyIterBudgetForStat;
    //@}

    /// PAG
    static PAG* pag;
    /// Module
    SVFModule* svfMod;
    /// Pointer analysis Type
    PTATY ptaTy;
    /// PTA implementation type.
    PTAImplTy ptaImplTy;
    /// Statistics
    PTAStat* stat;
    /// Call graph used for pointer analysis
    PTACallGraph* ptaCallGraph;
    /// SCC for CallGraph
    CallGraphSCC* callGraphSCC;
    /// Interprocedural control-flow graph
    ICFG* icfg;
    /// CHGraph
    static CommonCHGraph *chgraph;
    /// TypeSystem
    TypeSystem *typeSystem;

public:
	/// Get ICFG
	inline ICFG* getICFG() const {
		return pag->getICFG();
	}
    /// Return number of resolved indirect call edges
    inline Size_t getNumOfResolvedIndCallEdge() const
    {
        return getPTACallGraph()->getNumOfResolvedIndCallEdge();
    }
    /// Return call graph
    inline PTACallGraph* getPTACallGraph() const
    {
        return ptaCallGraph;
    }
    /// Return call graph SCC
    inline CallGraphSCC* getCallGraphSCC() const
    {
        return callGraphSCC;
    }

    /// Constructor
    PointerAnalysis(PAG* pag, PTATY ty = Default_PTA, bool alias_check = true);

    /// Type of pointer analysis
    inline PTATY getAnalysisTy() const
    {
        return ptaTy;
    }

    /// Return implementation type of the pointer analysis.
    inline PTAImplTy getImplTy() const
    {
        return ptaImplTy;
    }

    /// Get/set PAG
    ///@{
    inline PAG* getPAG() const
    {
        return pag;
    }
    //@}

    /// Get PTA stat
    inline PTAStat* getStat() const
    {
        return stat;
    }
    /// Module
    inline SVFModule* getModule() const
    {
        return svfMod;
    }
    /// Get all Valid Pointers for resolution
    inline OrderedNodeSet& getAllValidPtrs()
    {
        return pag->getAllValidPtrs();
    }

    /// Destructor
    virtual ~PointerAnalysis();

    /// Initialization of a pointer analysis, including building symbol table and PAG etc.
    virtual void initialize();

    /// Finalization of a pointer analysis, including checking alias correctness
    virtual void finalize();

    /// Start Analysis here (main part of pointer analysis). It needs to be implemented in child class
    virtual void analyze() = 0;

    /// Compute points-to results on-demand, overridden by derived classes
    virtual void computeDDAPts(NodeID) {}

    /// Interface exposed to users of our pointer analysis, given Location infos
    virtual AliasResult alias(const MemoryLocation &LocA,
                              const MemoryLocation &LocB) = 0;

    /// Interface exposed to users of our pointer analysis, given Value infos
    virtual AliasResult alias(const Value* V1,
                              const Value* V2) = 0;

    /// Interface exposed to users of our pointer analysis, given PAGNodeID
    virtual AliasResult alias(NodeID node1, NodeID node2) = 0;

    /// Get points-to targets of a pointer. It needs to be implemented in child class
    virtual const PointsTo& getPts(NodeID ptr) = 0;

    /// Given an object, get all the nodes having whose pointsto contains the object.
    /// Similar to getPts, this also needs to be implemented in child classes.
    virtual const NodeSet& getRevPts(NodeID nodeId) = 0;

    /// Clear points-to data
    virtual void clearPts()
    {
    }

    /// Print targets of a function pointer
    void printIndCSTargets(const CallBlockNode* cs, const FunctionSet& targets);

    // Debug purpose
    //@{
    virtual void dumpTopLevelPtsTo() {}
    virtual void dumpAllPts() {}
    virtual void dumpCPts() {}
    virtual void dumpPts(NodeID ptr, const PointsTo& pts);
    void printIndCSTargets();
    void dumpAllTypes();
    //@}

protected:
    /// Return all indirect callsites
    inline const CallSiteToFunPtrMap& getIndirectCallsites() const
    {
        return pag->getIndirectCallsites();
    }
    /// Return function pointer PAGNode at a callsite cs
    inline NodeID getFunPtr(const CallBlockNode* cs) const
    {
        return pag->getFunPtr(cs);
    }
    /// Alias check functions to verify correctness of pointer analysis
    //@{
    virtual void validateTests();
    virtual void validateSuccessTests(std::string fun);
    virtual void validateExpectedFailureTests(std::string fun);
    //@}

    /// Whether to dump the graph for debugging purpose
    bool dumpGraph();

    /// Reset all object node as field-sensitive.
    void resetObjFieldSensitive();

public:
    /// Dump the statistics
    void dumpStat();

    /// Determine whether a points-to contains a black hole or constant node
    //@{
    inline bool containBlackHoleNode(const PointsTo& pts)
    {
        return pts.test(pag->getBlackHoleNode());
    }
    inline bool containConstantNode(const PointsTo& pts)
    {
        return pts.test(pag->getConstantNode());
    }
    virtual inline bool isBlkObjOrConstantObj(NodeID ptd) const
    {
        return pag->isBlkObjOrConstantObj(ptd);
    }
    inline bool isNonPointerObj(NodeID ptd) const
    {
        return pag->isNonPointerObj(ptd);
    }
    //@}

    /// Whether this object is heap or array
    //@{
    inline bool isHeapMemObj(NodeID id) const
    {
        const MemObj* mem = pag->getObject(id);
        assert(mem && "memory object is null??");
        return mem->isHeap();
    }

    inline bool isArrayMemObj(NodeID id) const
    {
        const MemObj* mem = pag->getObject(id);
        assert(mem && "memory object is null??");
        return mem->isArray();
    }
    //@}

    /// For field-sensitivity
    ///@{
    inline bool isFIObjNode(NodeID id) const
    {
        return (SVFUtil::isa<FIObjPN>(pag->getPAGNode(id)));
    }
    inline NodeID getBaseObjNode(NodeID id)
    {
        return pag->getBaseObjNode(id);
    }
    inline NodeID getFIObjNode(NodeID id)
    {
        return pag->getFIObjNode(id);
    }
    inline NodeID getGepObjNode(NodeID id, const LocationSet& ls)
    {
        return pag->getGepObjNode(id,ls);
    }
    virtual inline const NodeBS& getAllFieldsObjNode(NodeID id)
    {
        return pag->getAllFieldsObjNode(id);
    }
    inline void setObjFieldInsensitive(NodeID id)
    {
        MemObj* mem =  const_cast<MemObj*>(pag->getBaseObj(id));
        mem->setFieldInsensitive();
    }
    inline bool isFieldInsensitive(NodeID id) const
    {
        const MemObj* mem =  pag->getBaseObj(id);
        return mem->isFieldInsensitive();
    }
    ///@}

    /// Whether print statistics
    inline bool printStat()
    {
        return print_stat;
    }

    /// Whether print statistics
    inline void disablePrintStat()
    {
        print_stat = false;
    }

    /// Get callees from an indirect callsite
    //@{
    inline CallEdgeMap& getIndCallMap()
    {
        return getPTACallGraph()->getIndCallMap();
    }
    inline bool hasIndCSCallees(const CallBlockNode* cs) const
    {
        return getPTACallGraph()->hasIndCSCallees(cs);
    }
    inline const FunctionSet& getIndCSCallees(const CallBlockNode* cs) const
    {
        return getPTACallGraph()->getIndCSCallees(cs);
    }
    //@}

    /// Resolve indirect call edges
    virtual void resolveIndCalls(const CallBlockNode* cs, const PointsTo& target, CallEdgeMap& newEdges,LLVMCallGraph* callgraph = nullptr);
    /// Match arguments for callsite at caller and callee
    bool matchArgs(const CallBlockNode* cs, const SVFFunction* callee);

    /// CallGraph SCC related methods
    //@{
    /// CallGraph SCC detection
    inline void callGraphSCCDetection()
    {
        if(callGraphSCC==nullptr)
            callGraphSCC = new CallGraphSCC(ptaCallGraph);

        callGraphSCC->find();
    }
    /// Get SCC rep node of a SVFG node.
    inline NodeID getCallGraphSCCRepNode(NodeID id) const
    {
        return callGraphSCC->repNode(id);
    }
    /// Return TRUE if this edge is inside a CallGraph SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    inline bool inSameCallGraphSCC(const SVFFunction* fun1,const SVFFunction* fun2)
    {
        const PTACallGraphNode* src = ptaCallGraph->getCallGraphNode(fun1);
        const PTACallGraphNode* dst = ptaCallGraph->getCallGraphNode(fun2);
        return (getCallGraphSCCRepNode(src->getId()) == getCallGraphSCCRepNode(dst->getId()));
    }
    inline bool isInRecursion(const SVFFunction* fun) const
    {
        return callGraphSCC->isInCycle(ptaCallGraph->getCallGraphNode(fun)->getId());
    }
    /// Whether a local variable is in function recursions
    bool isLocalVarInRecursiveFun(NodeID id) const;
    //@}

    /// Return PTA name
    virtual const std::string PTAName() const
    {
        return "Pointer Analysis";
    }

    /// get CHGraph
    CommonCHGraph *getCHGraph() const
    {
        return chgraph;
    }

    void getVFnsFromCHA(const CallBlockNode* cs, VFunSet &vfns);
    void getVFnsFromPts(const CallBlockNode* cs, const PointsTo &target, VFunSet &vfns);
    void connectVCallToVFns(const CallBlockNode* cs, const VFunSet &vfns, CallEdgeMap& newEdges);
    virtual void resolveCPPIndCalls(const CallBlockNode* cs,
                                    const PointsTo& target,
                                    CallEdgeMap& newEdges);

    /// get TypeSystem
    const TypeSystem *getTypeSystem() const
    {
        return typeSystem;
    }
};

} // End namespace SVF

#endif /* POINTERANALYSIS_H_ */
