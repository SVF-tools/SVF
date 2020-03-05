// #include "Util/SVFModule.h"
// #include "Util/SVFPTACallGraph.h"
// #include "Util/SVFFunction.h"

// using namespace SVFUtil;
// static llvm::cl::opt<bool> CallGraphDotGraph("dump-callgraph", llvm::cl::init(false),
//                                        llvm::cl::desc("Dump dot graph of Call Graph"));

// SVFPTACallGraph::CallSiteToIdMap SVFPTACallGraph::csToIdMap;
// SVFPTACallGraph::IdToCallSiteMap SVFPTACallGraph::idToCSMap;
// CallSiteID SVFPTACallGraph::totalCallSiteNum = 1;


// /// Constructor
// SVFPTACallGraph::SVFPTACallGraph(SVFModule svfModule, CGEK k): kind(k) {
//     svfMod = svfModule;
//     callGraphNodeNum = 0;
//     numOfResolvedIndCallEdge = 0;
//     buildCallGraph(svfModule);
// }

// // /*!
// //  * Build call graph, connect direct call edge only
// //  */
// // void PTACallGraph::buildCallGraph(SVFModule svfModule) {

// //     /// create nodes
// //     for (SVFModule::iterator F = svfModule.begin(), E = svfModule.end(); F != E; ++F) {
// //         addCallGraphNode(*F);
// //     }

// //     /// create edges
// //     for (SVFModule::iterator F = svfModule.begin(), E = svfModule.end(); F != E; ++F) {
// //         Function *fun = *F;
// //         for (inst_iterator II = inst_begin(*fun), E = inst_end(*fun); II != E; ++II) {
// //             const Instruction *inst = &*II;
// //             if (isNonInstricCallSite(inst)) {
// //                 if(getCallee(inst))
// //                     addDirectCallGraphEdge(inst);
// //             }
// //         }
// //     }

// //     dump("callgraph_initial");
// // }

// /*!
//  *  Memory has been cleaned up at GenericGraph
//  */
// void SVFPTACallGraph::destroy() {
// }

// /*!
//  * Add call graph node
//  */
// void SVFPTACallGraph::addCallGraphNode(const SVFFunction* fun) {
//     NodeID id = callGraphNodeNum;
//     PTACallGraphNode* callGraphNode = new PTACallGraphNode(id, fun);
//     addGNode(id,callGraphNode);
//     funToCallGraphNodeMap[fun] = callGraphNode;
//     callGraphNodeNum++;
// }

// /*!
//  *  Whether we have already created this call graph edge
//  */
// PTACallGraphEdge* SVFPTACallGraph::hasGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) const {
//     PTACallGraphEdge edge(src,dst,kind,csId);
//     PTACallGraphEdge* outEdge = src->hasOutgoingEdge(&edge);
//     PTACallGraphEdge* inEdge = dst->hasIncomingEdge(&edge);
//     if (outEdge && inEdge) {
//         assert(outEdge == inEdge && "edges not match");
//         return outEdge;
//     }
//     else
//         return NULL;
// }

// /*!
//  * get CallGraph edge via nodes
//  */
// PTACallGraphEdge* SVFPTACallGraph::getGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) {
//     for (PTACallGraphEdge::CallGraphEdgeSet::iterator iter = src->OutEdgeBegin();
//             iter != src->OutEdgeEnd(); ++iter) {
//         PTACallGraphEdge* edge = (*iter);
//         if (edge->getEdgeKind() == kind && edge->getDstID() == dst->getId())
//             return edge;
//     }
//     return NULL;
// }

// /*!
//  * Add direct call edges
//  */
// void SVFPTACallGraph::addDirectCallGraphEdge(const std::string* call, PTACallGraphNode* caller,PTACallGraphNode* callee, CallSiteID csID) {
//     // assert(getCallee(call) && "no callee found");

//     // PTACallGraphNode* caller = getCallGraphNode(call->getParent()->getParent());
//     // PTACallGraphNode* callee = getCallGraphNode(getCallee(call));

//     // CallSite cs = SVFUtil::getLLVMCallSite(call);
//     // CallSiteID csId = addCallSite(cs, callee->getFunction());

//     if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csID)) {
//         // assert(call->getParent()->getParent() == caller->getFunction()
//         //        && "callee instruction not inside caller??");

//         PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee,PTACallGraphEdge::CallRetEdge,csID);
//         edge->addDirectCallSite(call);
//         addEdge(edge);
//         callinstToCallGraphEdgesMap[call].insert(edge);
//     }
// }

// /*!
//  * Add indirect call edge to update call graph
//  */
// void SVFPTACallGraph::addIndirectCallGraphEdge(const std::string* call, PTACallGraphNode* caller,PTACallGraphNode* callee, CallSiteID csID) {
//     // PTACallGraphNode* caller = getCallGraphNode(call->getParent()->getParent());
//     // PTACallGraphNode* callee = getCallGraphNode(calleefun);

//     numOfResolvedIndCallEdge++;

//     // CallSite cs = SVFUtil::getLLVMCallSite(call);
//     // CallSiteID csId = addCallSite(csID, callee->getFunction());

//     if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csID)) {
//         // assert(call->getParent()->getParent() == caller->getFunction()
//         //        && "callee instruction not inside caller??");

//         PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee,PTACallGraphEdge::CallRetEdge, csID);
//         edge->addInDirectCallSite(call);
//         addEdge(edge);
//         callinstToCallGraphEdgesMap[call].insert(edge);
//     }
// }

// /*!
//  * Get all callsite invoking this callee
//  */
// void SVFPTACallGraph::getAllCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet) {
//     PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
//     for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
//             it!=eit; ++it) {
//         for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->directCallsBegin(),
//                 ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit) {
//             csSet.insert((*cit));
//         }
//         for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->indirectCallsBegin(),
//                 ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit) {
//             csSet.insert((*cit));
//         }
//     }
// }

// /*!
//  * Get direct callsite invoking this callee
//  */
// void SVFPTACallGraph::getDirCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet) {
//     PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
//     for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
//             it!=eit; ++it) {
//         for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->directCallsBegin(),
//                 ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit) {
//             csSet.insert((*cit));
//         }
//     }
// }

// /*!
//  * Get indirect callsite invoking this callee
//  */
// void SVFPTACallGraph::getIndCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet) {
//     PTACallGraphNode* callGraphNode = getCallGraphNode(callee);
//     for(PTACallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
//             it!=eit; ++it) {
//         for(PTACallGraphEdge::CallInstSet::iterator cit = (*it)->indirectCallsBegin(),
//                 ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit) {
//             csSet.insert((*cit));
//         }
//     }
// }
