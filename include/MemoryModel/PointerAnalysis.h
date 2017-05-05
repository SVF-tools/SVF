//===- PointerAnalysis.h -- Base class of pointer analyses--------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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

#include "MemoryModel/PAG.h"
#include "MemoryModel/ConditionalPT.h"
#include "MemoryModel/PointsToDS.h"
#include "Util/PTACallGraph.h"
#include "Util/SCC.h"
#include "Util/PathCondAllocator.h"
#include "MemoryModel/PointsToDFDS.h"

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CallGraph.h>	// call graph

class CHGraph;
class CHNode;

class TypeSystem;

class PTAStat;
/*
 * Pointer Analysis Base Class
 */
class PointerAnalysis {

public:
    /// Pointer analysis type list
    enum PTATY {
        // Whole program analysis
        Andersen_WPA,		///< Andersen PTA
        AndersenLCD_WPA,	///< Lazy cycle detection andersen-style WPA
        AndersenWave_WPA,	///< Wave propagation andersen-style WPA
        AndersenWaveDiff_WPA,	///< Diff wave propagation andersen-style WPA
        CSCallString_WPA,	///< Call string based context sensitive WPA
        CSSummary_WPA,		///< Summary based context sensitive WPA
        FSDATAFLOW_WPA,	///< Traditional Dataflow-based flow sensitive WPA
        FSSPARSE_WPA,		///< Sparse flow sensitive WPA
        FSCS_WPA,			///< Flow-, context- sensitive WPA
        FSCSPS_WPA,		///< Flow-, context-, path- sensitive WPA
        ADAPTFSCS_WPA,		///< Adaptive Flow-, context-, sensitive WPA
        ADAPTFSCSPS_WPA,	///< Adaptive Flow-, context-, path- sensitive WPA

        // Demand driven analysis
        Insensitive_DDA, ///<  Flow-, context- insensitive DDA (POPL '08)
        Regular_DDA,	///< Flow-, context- insensitive DDA (OOPSLA '05)
        FieldS_DDA,		///< Field sensitive DDA
        FlowS_DDA,		///< Flow sensitive DDA
        PathS_DDA,		///< Guarded value-flow DDA
        Cxt_DDA,		///< context sensitive DDA

        Default_PTA		///< default pta without any analysis
    };

    /// Indirect call edges type, map a callsite to a set of callees
    //@{
    typedef llvm::AliasAnalysis AliasAnalysis;
    typedef std::set<llvm::CallSite> CallSiteSet;
    typedef PAG::CallSiteToFunPtrMap CallSiteToFunPtrMap;
    typedef	std::set<const llvm::Function*> FunctionSet;
    typedef std::map<llvm::CallSite, FunctionSet> CallEdgeMap;
    typedef SCCDetection<PTACallGraph*> CallGraphSCC;
    //@}

    /// Statistic numbers
    //@{
    Size_t numOfIteration;
    //@}

private:
    /// Release the memory
    void destroy();

protected:

    /// User input flags
    //@{
    /// Flag for printing the statistic results
    bool print_stat;
    /// Flag for iteration budget for on-the-fly statistics
    Size_t OnTheFlyIterBudgetForStat;
    //@}

    /// PAG
    static PAG* pag;
    /// Module
    static llvm::Module* mod;
    /// Pointer analysis Type
    PTATY ptaTy;
    /// Statistics
    PTAStat* stat;
    /// Call graph used for pointer analysis
    PTACallGraph* ptaCallGraph;
    /// SCC for CallGraph
    CallGraphSCC* callGraphSCC;
    /// CHGraph
    static CHGraph *chgraph;
    /// TypeSystem
    TypeSystem *typeSystem;

public:
    /// Return number of resolved indirect call edges
    inline Size_t getNumOfResolvedIndCallEdge() const {
        return getPTACallGraph()->getNumOfResolvedIndCallEdge();
    }
    /// Return call graph
    inline PTACallGraph* getPTACallGraph() const {
        return ptaCallGraph;
    }
    /// Return call graph SCC
    inline CallGraphSCC* getCallGraphSCC() const {
        return callGraphSCC;
    }

    /// Constructor
    PointerAnalysis(PTATY ty = Default_PTA);

    /// Type of pointer analysis
    inline PTATY getAnalysisTy() const {
        return ptaTy;
    }

    /// Get/set PAG
    ///@{
    inline PAG* getPAG() const {
        return pag;
    }
    static inline void setPAG(PAG* g) {
        pag = g;
    }
    //@}

    /// Get PTA stat
    inline PTAStat* getStat() const {
        return stat;
    }
    /// Module
    inline llvm::Module* getModule() const {
        return mod;
    }
    /// Get all Valid Pointers for resolution
    inline NodeBS& getAllValidPtrs() {
        return pag->getAllValidPtrs();
    }

    /// Destructor
    virtual ~PointerAnalysis();

    /// Initialization of a pointer analysis, including building symbol table and PAG etc.
    virtual void initialize(llvm::Module& module);

    /// Finalization of a pointer analysis, including checking alias correctness
    virtual void finalize();

    /// Start Analysis here (main part of pointer analysis). It needs to be implemented in child class
    virtual void analyze(llvm::Module& module) = 0;

    /// Compute points-to results on-demand, overridden by derived classes
    virtual void computeDDAPts(NodeID id) {}

    /// Interface exposed to users of our pointer analysis, given Location infos
    virtual llvm::AliasResult alias(const llvm::MemoryLocation &LocA,
                                    const llvm::MemoryLocation &LocB) = 0;

    /// Interface exposed to users of our pointer analysis, given Value infos
    virtual llvm::AliasResult alias(const llvm::Value* V1,
                                    const llvm::Value* V2) = 0;

    /// Interface exposed to users of our pointer analysis, given PAGNodeID
    virtual llvm::AliasResult alias(NodeID node1, NodeID node2) = 0;

protected:
    /// Return all indirect callsites
    inline const CallSiteToFunPtrMap& getIndirectCallsites() const {
        return pag->getIndirectCallsites();
    }
    /// Return function pointer PAGNode at a callsite cs
    inline NodeID getFunPtr(const llvm::CallSite& cs) const {
        return pag->getFunPtr(cs);
    }
    /// Alias check functions to verify correctness of pointer analysis
    //@{
    virtual void validateTests();
    virtual void validateSuccessTests(const char* fun);
    virtual void validateExpectedFailureTests(const char* fun);
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
    inline bool containBlackHoleNode(PointsTo& pts) {
        return pts.test(pag->getBlackHoleNode());
    }
    inline bool containConstantNode(PointsTo& pts) {
        return pts.test(pag->getConstantNode());
    }
    inline bool isBlkObjOrConstantObj(NodeID ptd) const {
        return pag->isBlkObjOrConstantObj(ptd);
    }
    inline bool isNonPointerObj(NodeID ptd) const {
        return pag->isNonPointerObj(ptd);
    }
    //@}

    /// Whether this object is heap or array
    //@{
    inline bool isHeapMemObj(NodeID id) const {
        const MemObj* mem = pag->getObject(id);
        assert(mem && "memory object is null??");
        return mem->isHeap();
    }

    inline bool isArrayMemObj(NodeID id) const {
        const MemObj* mem = pag->getObject(id);
        assert(mem && "memory object is null??");
        return mem->isArray();
    }
    //@}

    /// For field-sensitivity
    ///@{
    inline bool isFIObjNode(NodeID id) const {
        return (llvm::isa<FIObjPN>(pag->getPAGNode(id)));
    }
    inline NodeID getBaseObjNode(NodeID id) {
        return pag->getBaseObjNode(id);
    }
    inline NodeID getFIObjNode(NodeID id) {
        return pag->getFIObjNode(id);
    }
    inline NodeID getGepObjNode(NodeID id, const LocationSet& ls) {
        return pag->getGepObjNode(id,ls);
    }
    inline const NodeBS& getAllFieldsObjNode(NodeID id) {
        return pag->getAllFieldsObjNode(id);
    }
    inline void setObjFieldInsensitive(NodeID id) {
        MemObj* mem =  const_cast<MemObj*>(pag->getBaseObj(id));
        mem->setFieldInsensitive();
    }
    inline bool isFieldInsensitive(NodeID id) const {
        const MemObj* mem =  pag->getBaseObj(id);
        return mem->isFieldInsensitive();
    }
    ///@}

    /// Whether print statistics
    inline bool printStat() {
        return print_stat;
    }

    /// Whether print statistics
    inline void disablePrintStat() {
        print_stat = false;
    }

    /// Get callees from an indirect callsite
    //@{
    inline CallEdgeMap& getIndCallMap() {
        return getPTACallGraph()->getIndCallMap();
    }
    inline bool hasIndCSCallees(llvm::CallSite cs) const {
        return getPTACallGraph()->hasIndCSCallees(cs);
    }
    inline const FunctionSet& getIndCSCallees(llvm::CallSite cs) const {
        return getPTACallGraph()->getIndCSCallees(cs);
    }
    inline const FunctionSet& getIndCSCallees(llvm::CallInst* csInst) const {
        llvm::CallSite cs = analysisUtil::getLLVMCallSite(csInst);
        return getIndCSCallees(cs);
    }
    //@}

    /// Resolve indirect call edges
    virtual void resolveIndCalls(llvm::CallSite cs, const PointsTo& target, CallEdgeMap& newEdges,llvm::CallGraph* callgraph = NULL);
    /// Match arguments for callsite at caller and callee
    inline bool matchArgs(llvm::CallSite cs, const llvm::Function* callee) {
        if(ThreadAPI::getThreadAPI()->isTDFork(cs))
            return true;
        else
            return cs.arg_size() == callee->arg_size();
    }

    /// CallGraph SCC related methods
    //@{
    /// CallGraph SCC detection
    inline void callGraphSCCDetection() {
        if(callGraphSCC==NULL)
            callGraphSCC = new CallGraphSCC(ptaCallGraph);

        callGraphSCC->find();
    }
    /// Get SCC rep node of a SVFG node.
    inline NodeID getCallGraphSCCRepNode(NodeID id) const {
        return callGraphSCC->repNode(id);
    }
    /// Return TRUE if this edge is inside a CallGraph SCC, i.e., src node and dst node are in the same SCC on the SVFG.
    inline bool inSameCallGraphSCC(const llvm::Function* fun1,const llvm::Function* fun2) {
        const PTACallGraphNode* src = ptaCallGraph->getCallGraphNode(fun1);
        const PTACallGraphNode* dst = ptaCallGraph->getCallGraphNode(fun2);
        return (getCallGraphSCCRepNode(src->getId()) == getCallGraphSCCRepNode(dst->getId()));
    }
    inline bool isInRecursion(const llvm::Function* fun) const {
        return callGraphSCC->isInCycle(ptaCallGraph->getCallGraphNode(fun)->getId());
    }
    /// Whether a local variable is in function recursions
    bool isLocalVarInRecursiveFun(NodeID id) const;
    //@}

    /// Return PTA name
    virtual const std::string PTAName() const {
        return "Pointer Analysis";
    }

    /// Get points-to targets of a pointer. It needs to be implemented in child class
    virtual PointsTo& getPts(NodeID ptr) = 0;

    /// Clear points-to data
    virtual void clearPts() {
    }

    /// Print targets of a function pointer
    void printIndCSTargets(const llvm::CallSite cs, const FunctionSet& targets);

    // Debug purpose
    //@{
    virtual void dumpTopLevelPtsTo() {}
    virtual void dumpAllPts() {}
    virtual void dumpCPts() {}
    virtual void dumpPts(NodeID ptr, const PointsTo& pts);
    void printIndCSTargets();
    void dumpAllTypes();
    //@}

    /// get CHGraph
    CHGraph *getCHGraph() const {
        return chgraph;
    }

    void getVFnsFromPts(llvm::CallSite cs,
                        const PointsTo &target,
                        std::set<const llvm::Function*> &vfns);
    void connectVCallToVFns(llvm::CallSite cs,
                            std::set<const llvm::Function*> &vfns,
                            CallEdgeMap& newEdges,
                            llvm::CallGraph* callgraph = NULL);
    virtual void resolveCPPIndCalls(llvm::CallSite cs,
                                    const PointsTo& target,
                                    CallEdgeMap& newEdges,
                                    llvm::CallGraph* callgraph = NULL);

    /// get TypeSystem
    const TypeSystem *getTypeSystem() const {
        return typeSystem;
    }
};

/*!
 * Pointer analysis implementation which uses bit vector based points-to data structure
 */
class BVDataPTAImpl : public PointerAnalysis {

public:
    typedef PTData<NodeID,PointsTo> PTDataTy;	/// Points-to data structure type
    typedef DiffPTData<NodeID,PointsTo,EdgeID> DiffPTDataTy;	/// Points-to data structure type
    typedef DFPTData<NodeID,PointsTo> DFPTDataTy;	/// Points-to data structure type
    typedef IncDFPTData<NodeID,PointsTo> IncDFPTDataTy;	/// Points-to data structure type

    /// Constructor
    BVDataPTAImpl(PointerAnalysis::PTATY type);

    /// Destructor
    virtual ~BVDataPTAImpl() {
        destroy();
    }

    /// Release memory
    inline void destroy() {
        delete ptD;
        ptD = NULL;
    }

    /// Get points-to and reverse points-to
    ///@{
    virtual inline PointsTo& getPts(NodeID id) {
        return ptD->getPts(id);
    }
    virtual inline PointsTo& getRevPts(NodeID nodeId) {
        return ptD->getRevPts(nodeId);
    }
    //@}

    /// Expand FI objects
    void expandFIObjs(const PointsTo& pts, PointsTo& expandedPts);

    /// Interface for analysis result storage on filesystem.
    //@{
    virtual void writeToFile(const std::string& filename);
    virtual bool readFromFile(const std::string& filename);
    //@}

protected:

    /// Update callgraph. This should be implemented by its subclass.
    virtual inline bool updateCallGraph(const CallSiteToFunPtrMap& callsites) {
        assert(false && "Virtual function not implemented!");
        return false;
    }

    /// Get points-to data structure
    inline PTDataTy* getPTDataTy() const {
        return ptD;
    }
    inline DiffPTDataTy* getDiffPTDataTy() const {
        return llvm::cast<DiffPTDataTy>(ptD);
    }
    inline IncDFPTDataTy* getDFPTDataTy() const {
        return llvm::cast<IncDFPTDataTy>(ptD);
    }

    /// Union/add points-to. Add the reverse points-to for node collapse purpose
    /// To be noted that adding reverse pts might incur 10% total overhead during solving
    //@{
    virtual inline bool unionPts(NodeID id, const PointsTo& target) {
        return ptD->unionPts(id, target);
    }
    virtual inline bool unionPts(NodeID id, NodeID ptd) {
        return ptD->unionPts(id,ptd);
    }
    virtual inline bool addPts(NodeID id, NodeID ptd) {
        return ptD->addPts(id,ptd);
    }
    //@}

    /// Clear all data
    virtual inline void clearPts() {
        ptD->clear();
    }

    /// On the fly call graph construction
    virtual void onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges,llvm::CallGraph* callgraph = NULL);

private:
    /// Points-to data
    PTDataTy* ptD;

public:
    /// Interface expose to users of our pointer analysis, given Location infos
    virtual llvm::AliasResult alias(const llvm::MemoryLocation  &LocA,
                                    const llvm::MemoryLocation  &LocB);

    /// Interface expose to users of our pointer analysis, given Value infos
    virtual llvm::AliasResult alias(const llvm::Value* V1,
                                    const llvm::Value* V2);

    /// Interface expose to users of our pointer analysis, given PAGNodeID
    virtual llvm::AliasResult alias(NodeID node1, NodeID node2);

    /// Interface expose to users of our pointer analysis, given two pts
    virtual llvm::AliasResult alias(const PointsTo& pts1, const PointsTo& pts2);

    /// dump and debug, print out conditional pts
    //@{
    virtual void dumpCPts() {
        ptD->dumpPTData();
    }

    virtual void dumpTopLevelPtsTo();

    virtual void dumpAllPts();
    //@}
};

/*!
 * Pointer analysis implementation which uses conditional points-to map data structure (context/path sensitive analysis)
 */
template<class Cond>
class CondPTAImpl : public PointerAnalysis {

public:
    typedef CondVar<Cond> CVar;
    typedef CondStdSet<CVar>  CPtSet;
    typedef PTData<CVar,CPtSet> PTDataTy;	         /// Points-to data structure type
    typedef std::map<NodeID,PointsTo> PtrToBVPtsMap; /// map a pointer to its BitVector points-to representation
    typedef std::map<NodeID,CPtSet> PtrToCPtsMap;	 /// map a pointer to its conditional points-to set

    /// Constructor
    CondPTAImpl(PointerAnalysis::PTATY type) : PointerAnalysis(type), normalized(false) {
        if (type == PathS_DDA || type == Cxt_DDA)
            ptD = new PTDataTy();
        else
            assert(false && "no points-to data available");
    }

    /// Destructor
    virtual ~CondPTAImpl() {
        destroy();
    }

    /// Release memory
    inline void destroy() {
        delete ptD;
        ptD = NULL;
    }

    /// Get points-to data
    inline PTDataTy* getPTDataTy() const {
        return ptD;
    }

    /// Get points-to and reverse points-to
    ///@{
    virtual inline CPtSet& getPts(CVar id) {
        return ptD->getPts(id);
    }
    virtual inline CPtSet& getRevPts(CVar nodeId) {
        return ptD->getRevPts(nodeId);
    }
    //@}

    /// Clear all data
    virtual inline void clearPts() {
        ptD->clear();
    }

    /// Whether cpts1 and cpts2 have overlap points-to targets
    bool overlap(const CPtSet& cpts1, const CPtSet& cpts2) const {
        for (typename CPtSet::const_iterator it1 = cpts1.begin(); it1 != cpts1.end(); ++it1) {
            for (typename CPtSet::const_iterator it2 = cpts2.begin(); it2 != cpts2.end(); ++it2) {
                if(isSameVar(*it1,*it2))
                    return true;
            }
        }
        return false;
    }

    /// Expand all fields of an aggregate in all points-to sets
    void expandFIObjs(const CPtSet& cpts, CPtSet& expandedCpts) {
        expandedCpts = cpts;;
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit) {
            if(pag->getBaseObjNode(cit->get_id())==cit->get_id()) {
                NodeBS& fields = pag->getAllFieldsObjNode(cit->get_id());
                for(NodeBS::iterator it = fields.begin(), eit = fields.end(); it!=eit; ++it) {
                    CVar cvar(cit->get_cond(),*it);
                    expandedCpts.set(cvar);
                }
            }
        }
    }

protected:

    /// Finalization of pointer analysis, and normalize points-to information to Bit Vector representation
    virtual void finalize() {
        NormalizePointsTo();
        PointerAnalysis::finalize();
    }
    /// Union/add points-to, and add the reverse points-to for node collapse purpose
    /// To be noted that adding reverse pts might incur 10% total overhead during solving
    //@{
    virtual inline bool unionPts(CVar id, const CPtSet& target) {
        return ptD->unionPts(id, target);
    }

    virtual inline bool unionPts(CVar id, CVar ptd) {
        return ptD->unionPts(id,ptd);
    }

    virtual inline bool addPts(CVar id, CVar ptd) {
        return ptD->addPts(id,ptd);
    }
    //@}

    /// Internal interface to be used for conditional points-to set queries
    //@{
    inline bool mustAlias(const CVar& var1, const CVar& var2) {
        if(isSameVar(var1,var2))
            return true;

        bool singleton = !(isHeapMemObj(var1.get_id()) || isLocalVarInRecursiveFun(var1.get_id()));
        if(isCondCompatible(var1.get_cond(),var2.get_cond(),singleton) == false)
            return false;

        const CPtSet& cpts1 = getPts(var1);
        const CPtSet& cpts2 = getPts(var2);
        return (contains(cpts1,cpts2) && contains(cpts2,cpts1));
    }

    //  Whether cpts1 contains all points-to targets of pts2
    bool contains(const CPtSet& cpts1, const CPtSet& cpts2) {
        if (cpts1.empty() || cpts2.empty())
            return false;

        for (typename CPtSet::const_iterator it2 = cpts2.begin(); it2 != cpts2.end(); ++it2) {
            bool hasObj = false;
            for (typename CPtSet::const_iterator it1 = cpts1.begin(); it1 != cpts1.end(); ++it1) {
                if(isSameVar(*it1,*it2)) {
                    hasObj = true;
                    break;
                }
            }
            if(hasObj == false)
                return false;
        }
        return true;
    }

    /// Whether two pointers/objects are the same one by considering their conditions
    bool isSameVar(const CVar& var1, const CVar& var2) const {
        if(var1.get_id() != var2.get_id())
            return false;

        /// we distinguish context sensitive memory allocation here
        bool singleton = !(isHeapMemObj(var1.get_id()) || isLocalVarInRecursiveFun(var1.get_id()));
        return isCondCompatible(var1.get_cond(),var2.get_cond(),singleton);
    }
    //@}

    /// Normalize points-to information to BitVector/conditional representation
    virtual void NormalizePointsTo() {
        normalized = true;
        const typename PTDataTy::PtsMap& ptsMap = getPTDataTy()->getPtsMap();
        for(typename PTDataTy::PtsMap::const_iterator it = ptsMap.begin(), eit=ptsMap.end(); it!=eit; ++it) {
            for(typename CPtSet::const_iterator cit = it->second.begin(), ecit=it->second.end(); cit!=ecit; ++cit) {
                ptrToBVPtsMap[(it->first).get_id()].set(cit->get_id());
                ptrToCPtsMap[(it->first).get_id()].set(*cit);
            }
        }
    }
    /// Points-to data
    PTDataTy* ptD;
    /// Normalized flag
    bool normalized;
    /// Normal points-to representation (without conditions)
    PtrToBVPtsMap ptrToBVPtsMap;
    /// Conditional points-to representation (with conditions)
    PtrToCPtsMap ptrToCPtsMap;
public:
    /// Print out conditional pts
    virtual void dumpCPts() {
        ptD->dumpPTData();
    }
    /// Given a conditional pts return its bit vector points-to
    virtual inline PointsTo getBVPointsTo(const CPtSet& cpts) const {
        PointsTo pts;
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit)
            pts.set(cit->get_id());
        return pts;
    }
    /// Given a pointer return its bit vector points-to
    virtual inline PointsTo& getPts(NodeID ptr) {
        assert(normalized && "Pts of all context-var have to be merged/normalized. Want to use getPts(CVar cvar)??");
        return ptrToBVPtsMap[ptr];
    }
    /// Given a pointer return its conditional points-to
    virtual inline const CPtSet& getCondPointsTo(NodeID ptr) {
        assert(normalized && "Pts of all context-vars have to be merged/normalized. Want to use getPts(CVar cvar)??");
        return ptrToCPtsMap[ptr];
    }

    /// Interface expose to users of our pointer analysis, given Location infos
    virtual inline llvm::AliasResult alias(const llvm::MemoryLocation &LocA,
                                           const llvm::MemoryLocation  &LocB) {
        return alias(LocA.Ptr, LocB.Ptr);
    }
    /// Interface expose to users of our pointer analysis, given Value infos
    virtual inline llvm::AliasResult alias(const llvm::Value* V1, const llvm::Value* V2) {
        return  alias(pag->getValueNode(V1),pag->getValueNode(V2));
    }
    /// Interface expose to users of our pointer analysis, given two pointers
    virtual inline llvm::AliasResult alias(NodeID node1, NodeID node2) {
        return alias(getCondPointsTo(node1),getCondPointsTo(node2));
    }
    /// Interface expose to users of our pointer analysis, given conditional variables
    virtual llvm::AliasResult alias(const CVar& var1, const CVar& var2) {
        return alias(getPts(var1),getPts(var2));
    }
    /// Interface expose to users of our pointer analysis, given two conditional points-to sets
    virtual inline llvm::AliasResult alias(const CPtSet& pts1, const CPtSet& pts2) {
        CPtSet cpts1;
        expandFIObjs(pts1,cpts1);
        CPtSet cpts2;
        expandFIObjs(pts2,cpts2);
        if (containBlackHoleNode(cpts1) || containBlackHoleNode(cpts2))
            return llvm::MayAlias;
        else if(this->getAnalysisTy()==PathS_DDA && contains(cpts1,cpts2) && contains(cpts2,cpts1)) {
            return llvm::MustAlias;
        }
        else if(overlap(cpts1,cpts2))
            return llvm::MayAlias;
        else
            return llvm::NoAlias;
    }
    /// Test blk node for cpts
    inline bool containBlackHoleNode(const CPtSet& cpts) {
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit) {
            if(cit->get_id() == pag->getBlackHoleNode())
                return true;
        }
        return false;
    }
    /// Test constant node for cpts
    inline bool containConstantNode(const CPtSet& cpts) {
        for(typename CPtSet::const_iterator cit = cpts.begin(), ecit=cpts.end(); cit!=ecit; ++cit) {
            if(cit->get_id() == pag->getConstantNode())
                return true;
        }
        return false;
    }
    /// Whether two conditions are compatible (to be implemented by child class)
    virtual bool isCondCompatible(const Cond& cxt1, const Cond& cxt2, bool singleton) const = 0;

    /// Dump points-to information of top-level pointers
    void dumpTopLevelPtsTo() {
        for (NodeBS::iterator nIter = this->getAllValidPtrs().begin(); nIter != this->getAllValidPtrs().end(); ++nIter) {
            const PAGNode* node = this->getPAG()->getPAGNode(*nIter);
            if (this->getPAG()->isValidTopLevelPtr(node)) {
                if (llvm::isa<DummyObjPN>(node)) {
                    llvm::outs() << "##<Blackhole or constant> id:" << node->getId();
                }
                else if (!llvm::isa<DummyValPN>(node)) {
                    llvm::outs() << "##<" << node->getValue()->getName() << "> ";
                    llvm::outs() << "Souce Loc: " << analysisUtil::getSourceLoc(node->getValue());
                }

                const PointsTo& pts = getPts(node->getId());
                llvm::outs() << "\nNodeID " << node->getId() << " ";
                if (pts.empty()) {
                    llvm::outs() << "\t\tPointsTo: {empty}\n\n";
                } else {
                    llvm::outs() << "\t\tPointsTo: { ";
                    for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit; ++it)
                        llvm::outs() << *it << " ";
                    llvm::outs() << "}\n\n";
                }
            }
        }
    }
};
#endif /* POINTERANALYSIS_H_ */
