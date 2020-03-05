// #ifndef SVFPTACALLGRAPH_H_
// #define SVFPTACALLGRAPH_H_

// #include "MemoryModel/GenericGraph.h"
// #include "Util/SVFUtil.h"
// #include "Util/BasicTypes.h"
// #include "Util/SVFFunction.h"
// #include <set>

// class PTACallGraphNode;
// class SVFModule;

// /*
//  * Call Graph edge representing a calling relation between two functions
//  * Multiple calls from function A to B are merged into one call edge
//  * Each call edge has a set of direct callsites and a set of indirect callsites
//  */
// typedef GenericEdge<PTACallGraphNode> GenericCallGraphEdgeTy;
// class PTACallGraphEdge : public GenericCallGraphEdgeTy {

// public:
//     typedef std::set<const std::string*> CallInstSet;
//     enum CEDGEK {
//         CallRetEdge,TDForkEdge,TDJoinEdge,HareParForEdge
//     };


// private:
//     CallInstSet directCalls;
//     CallInstSet indirectCalls;
//     CallSiteID csId;
// public:
//     /// Constructor
// 	PTACallGraphEdge(PTACallGraphNode* s, PTACallGraphNode* d, CEDGEK kind, CallSiteID cs) :
// 			GenericCallGraphEdgeTy(s, d, makeEdgeFlagWithInvokeID(kind, cs)), csId(cs) {
// 	}
//     /// Destructor
//     virtual ~PTACallGraphEdge() {
//     }
//     /// Compute the unique edgeFlag value from edge kind and CallSiteID.
//     static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs) {
//         return (cs << EdgeKindMaskBits) | k;
//     }
//     /// Get direct and indirect calls
//     //@{
// 	inline CallSiteID getCallSiteID() const {
// 		return csId;
// 	}
// 	inline bool isDirectCallEdge() const {
// 		return !directCalls.empty() && indirectCalls.empty();
// 	}
// 	inline bool isIndirectCallEdge() const {
// 		return directCalls.empty() && !indirectCalls.empty();
// 	}
// 	inline CallInstSet& getDirectCalls() {
// 		return directCalls;
// 	}
//     inline CallInstSet& getIndirectCalls() {
//         return indirectCalls;
//     }
//     inline const CallInstSet& getDirectCalls() const {
//         return directCalls;
//     }
//     inline const CallInstSet& getIndirectCalls() const {
//         return indirectCalls;
//     }
//     //@}

//     /// Add direct and indirect callsite
//     //@{
//     void addDirectCallSite(const std::string* call) {
//         // assert((SVFUtil::isa<CallInst>(call) || SVFUtil::isa<InvokeInst>(call)) && "not a call or inovke??");
//         // assert(SVFUtil::getCallee(call) && "not a direct callsite??");
//         directCalls.insert(call);
//     }

//     void addInDirectCallSite(const std::string* call) {
//         // assert((SVFUtil::isa<CallInst>(call) || SVFUtil::isa<InvokeInst>(call)) && "not a call or inovke??");
//         // assert((NULL == SVFUtil::getCallee(call) || NULL == SVFUtil::dyn_cast<Function> (SVFUtil::getForkedFun(call))) && "not an indirect callsite??");
//         indirectCalls.insert(call);
//     }
//     //@}

//     /// Iterators for direct and indirect callsites
//     //@{
//     inline CallInstSet::iterator directCallsBegin() const {
//         return directCalls.begin();
//     }
//     inline CallInstSet::iterator directCallsEnd() const {
//         return directCalls.end();
//     }

//     inline CallInstSet::iterator indirectCallsBegin() const {
//         return indirectCalls.begin();
//     }
//     inline CallInstSet::iterator indirectCallsEnd() const {
//         return indirectCalls.end();
//     }
//     //@}

//     /// ClassOf
//     //@{
//     static inline bool classof(const PTACallGraphEdge *edge) {
//         return true;
//     }
//     static inline bool classof(const GenericCallGraphEdgeTy *edge) {
//         return edge->getEdgeKind() == PTACallGraphEdge::CallRetEdge ||
//                edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge ||
//                edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
//     }
//     //@}

//     typedef GenericNode<PTACallGraphNode,PTACallGraphEdge>::GEdgeSetTy CallGraphEdgeSet;

// };

// /*
//  * Call Graph node representing a function
//  */
// typedef GenericNode<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphNodeTy;
// class PTACallGraphNode : public GenericCallGraphNodeTy {

// public:
//     typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
//     typedef PTACallGraphEdge::CallGraphEdgeSet::iterator iterator;
//     typedef PTACallGraphEdge::CallGraphEdgeSet::const_iterator const_iterator;

// private:
//     const SVFFunction* fun;

// public:
//     /// Constructor
//     PTACallGraphNode(NodeID i, const SVFFunction* f) : GenericCallGraphNodeTy(i,0), fun(f) {

//     }

//     /// Get function of this call node
//     inline const SVFFunction* getFunction() const {
//         return fun;
//     }

//     /// Return TRUE if this function can be reached from main.
//     bool isReachableFromProgEntry() const;
// };

// /*!
//  * Pointer Analysis Call Graph used internally for various pointer analysis
//  */
// typedef GenericGraph<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphTy;
// class SVFPTACallGraph : public GenericCallGraphTy {

// public:
//     typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
//     typedef llvm::DenseMap<const SVFFunction*, PTACallGraphNode *> FunToCallGraphNodeMap;
//     typedef llvm::DenseMap<const std::string*, CallGraphEdgeSet> CallInstToCallGraphEdgesMap;
//     typedef std::pair<CallSite, const SVFFunction*> CallSitePair;
//     typedef std::map<CallSitePair, CallSiteID> CallSiteToIdMap;
//     typedef std::map<CallSiteID, CallSitePair> IdToCallSiteMap;
//     typedef	std::set<const SVFFunction*> FunctionSet;
//     typedef std::map<CallSite, FunctionSet> CallEdgeMap;
//     typedef CallGraphEdgeSet::iterator CallGraphNodeIter;

//     enum CGEK {
//         NormCallGraph, ThdCallGraph
//     };

// private:
//     CGEK kind;

//     SVFModule svfMod;

//     /// Indirect call map
//     CallEdgeMap indirectCallMap;

//     /// Call site information
//     static CallSiteToIdMap csToIdMap;	///< Map a pair of call instruction and callee to a callsite ID
//     static IdToCallSiteMap idToCSMap;	///< Map a callsite ID to a pair of call instruction and callee
//     static CallSiteID totalCallSiteNum;	///< CallSiteIDs, start from 1;

// protected:
//     FunToCallGraphNodeMap funToCallGraphNodeMap; ///< Call Graph node map
//     CallInstToCallGraphEdgesMap callinstToCallGraphEdgesMap; ///< Map a call instruction to its corresponding call edges

//     NodeID callGraphNodeNum;
//     Size_t numOfResolvedIndCallEdge;

//     /// Build Call Graph
//     void buildCallGraph(SVFModule svfModule);

//     /// Add callgraph Node
//     void addCallGraphNode(const SVFFunction* fun);

//     /// Clean up memory
//     void destroy();

// public:
//     /// Constructor
//     SVFPTACallGraph(SVFModule svfModule, CGEK k = NormCallGraph);

//     /// Destructor
//     virtual ~SVFPTACallGraph() {
//         destroy();
//     }

//     /// Return type of this callgraph
//     inline CGEK getKind() const {
//         return kind;
//     }

//     /// Get callees from an indirect callsite
//     //@{
//     inline CallEdgeMap& getIndCallMap() {
//         return indirectCallMap;
//     }
//     inline bool hasIndCSCallees(CallSite cs) const {
//         return (indirectCallMap.find(cs) != indirectCallMap.end());
//     }
//     inline const FunctionSet& getIndCSCallees(CallSite cs) const {
//         CallEdgeMap::const_iterator it = indirectCallMap.find(cs);
//         assert(it!=indirectCallMap.end() && "not an indirect callsite!");
//         return it->second;
//     }
//     inline const FunctionSet& getIndCSCallees(CallInst* csInst) const {
//         CallSite cs = SVFUtil::getLLVMCallSite(csInst);
//         return getIndCSCallees(cs);
//     }
//     //@}
//     inline u32_t getTotalCallSiteNumber() const {
//         return totalCallSiteNum;
//     }

//     inline Size_t getNumOfResolvedIndCallEdge() const {
//         return numOfResolvedIndCallEdge;
//     }

//     inline const CallInstToCallGraphEdgesMap& getCallInstToCallGraphEdgesMap() const {
//         return callinstToCallGraphEdgesMap;
//     }

//     /// Issue a warning if the function which has indirect call sites can not be reached from program entry.
//     void verifyCallGraph();

//     /// Get call graph node
//     //@{
//     inline PTACallGraphNode* getCallGraphNode(NodeID id) const {
//         return getGNode(id);
//     }
//     inline PTACallGraphNode* getCallGraphNode(const SVFFunction* fun) const {
//         FunToCallGraphNodeMap::const_iterator it = funToCallGraphNodeMap.find(fun);
//         assert(it!=funToCallGraphNodeMap.end() && "call graph node not found!!");
//         return it->second;
//     }
//     //@}

//     /// Add/Get CallSiteID
//     //@{
//     inline CallSiteID addCallSite(CallSite cs, const SVFFunction* callee) {
//         std::pair<CallSite, const SVFFunction*> newCS(std::make_pair(cs, callee));
//         CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
//         //assert(it == csToIdMap.end() && "cannot add a callsite twice");
//         if(it == csToIdMap.end()) {
//             CallSiteID id = totalCallSiteNum++;
//             csToIdMap.insert(std::make_pair(newCS, id));
//             idToCSMap.insert(std::make_pair(id, newCS));
//             return id;
//         }
//         return it->second;
//     }
//     inline CallSiteID getCallSiteID(CallSite cs, const SVFFunction* callee) const {
//         CallSitePair newCS(std::make_pair(cs, callee));
//         CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
//         assert(it != csToIdMap.end() && "callsite id not found! This maybe a partially resolved callgraph, please check the indCallEdge limit");
//         return it->second;
//     }
//     inline bool hasCallSiteID(CallSite cs, const SVFFunction* callee) const {
//         CallSitePair newCS(std::make_pair(cs, callee));
//         CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
//         return it != csToIdMap.end();
//     }
//     inline const CallSitePair& getCallSitePair(CallSiteID id) const {
//         IdToCallSiteMap::const_iterator it = idToCSMap.find(id);
//         assert(it != idToCSMap.end() && "cannot find call site for this CallSiteID");
//         return (it->second);
//     }
//     inline CallSite getCallSite(CallSiteID id) const {
//         return getCallSitePair(id).first;
//     }
//     // inline const SVFFunction* getCallerOfCallSite(CallSiteID id) const {
//     //     return getCallSite(id).getCaller();
//     // }
//     inline const SVFFunction* getCalleeOfCallSite(CallSiteID id) const {
//         return getCallSitePair(id).second;
//     }
//     //@}
//     /// Get Module
//     inline SVFModule getModule() {
//         return svfMod;
//     }
//     inline SVFModule getSVFModule() {
//         return svfMod;
//     }
//     /// Whether we have aleady created this call graph edge
//     PTACallGraphEdge* hasGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) const;
//     /// Get call graph edge via nodes
//     PTACallGraphEdge* getGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId);
//     /// Get call graph edge via call instruction
//     //@{
//     /// whether this call instruction has a valid call graph edge
//     inline bool hasCallGraphEdge(const std::string* inst) const {
//         return callinstToCallGraphEdgesMap.find(inst)!=callinstToCallGraphEdgesMap.end();
//     }
//     inline CallGraphEdgeSet::const_iterator getCallEdgeBegin(const std::string* inst) const {
//         CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
//         assert(it!=callinstToCallGraphEdgesMap.end()
//                && "call instruction does not have a valid callee");
//         return it->second.begin();
//     }
//     inline CallGraphEdgeSet::const_iterator getCallEdgeEnd(const std::string* inst) const {
//         CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
//         assert(it!=callinstToCallGraphEdgesMap.end()
//                && "call instruction does not have a valid callee");
//         return it->second.end();
//     }
//     //@}
//     /// Add call graph edge
//     inline void addEdge(PTACallGraphEdge* edge) {
//         edge->getDstNode()->addIncomingEdge(edge);
//         edge->getSrcNode()->addOutgoingEdge(edge);
//     }

//     /// Add direct/indirect call edges
//     //@{
//     void addDirectCallGraphEdge(PTACallGraphNode* caller,PTACallGraphNode* callee, CallSiteID csID);
//     void addIndirectCallGraphEdge(PTACallGraphNode* caller,PTACallGraphNode* callee, CallSiteID csID);
//     //@}

//     /// Get callsites invoking the callee
//     //@{
//     void getAllCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
//     void getDirCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
//     void getIndCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
//     //@}

//     /// Dump the graph
//     void dump(const std::string& filename);
// };



// #endif /* PTACALLGRAPH_H_ */
