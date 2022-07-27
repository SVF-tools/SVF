/*
 * TCT.h
 *
 *  Created on: Jun 24, 2015
 *      Author: Yulei Sui, Peng Di
 */

#ifndef TCTNodeDetector_H_
#define TCTNodeDetector_H_

#include "Util/SCC.h"
#include "Util/SVFUtil.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/CxtStmt.h"
#include "SVF-FE/DataFlowUtil.h"
#include <set>
#include <vector>

namespace SVF
{

class TCTNode;


/*
 * Thread creation edge represents a spawning relation between two context sensitive threads
 */
typedef GenericEdge<TCTNode> GenericTCTEdgeTy;
class TCTEdge: public GenericTCTEdgeTy
{
public:
    typedef Set<const Instruction*> CallInstSet;
    enum CEDGEK
    {
        ThreadCreateEdge
    };
    /// Constructor
    TCTEdge(TCTNode* s, TCTNode* d, CEDGEK kind) :
        GenericTCTEdgeTy(s, d, kind)
    {
    }
    /// Destructor
    virtual ~TCTEdge()
    {
    }
    /// Classof
    //@{
    static inline bool classof(const TCTEdge*)
    {
        return true;
    }
    static inline bool classof(const GenericTCTEdgeTy *edge)
    {
        return edge->getEdgeKind() == TCTEdge::ThreadCreateEdge;
    }
    ///@}
    typedef GenericNode<TCTNode, TCTEdge>::GEdgeSetTy ThreadCreateEdgeSet;

};

/*
 * Each node represents a context-sensitive thread
 */
typedef GenericNode<TCTNode, TCTEdge> GenericTCTNodeTy;
class TCTNode: public GenericTCTNodeTy
{

public:
    /// Constructor
    TCTNode(NodeID i, const CxtThread& cctx) :
        GenericTCTNodeTy(i, 0), ctx(cctx), multiforked(false)
    {
    }

    void dump()
    {
        SVFUtil::outs() << "---\ntid: " << this->getId() << "  inloop:" << ctx.isInloop() << "  incycle:" << ctx.isIncycle() << " multiforked:"<< isMultiforked();
    }

    /// Get CxtThread
    inline const CxtThread& getCxtThread() const
    {
        return ctx;
    }

    /// inloop, incycle attributes
    //@{
    inline bool isInloop() const
    {
        return ctx.isInloop();
    }
    inline bool isIncycle() const
    {
        return ctx.isIncycle();
    }
    inline void setMultiforked(bool value)
    {
        multiforked = value;
    }
    inline bool isMultiforked() const
    {
        return multiforked;
    }
    //@}

private:
    const CxtThread ctx;
    bool multiforked;
};

/*!
 * Pointer Analysis Call Graph used internally for various pointer analysis
 */
typedef GenericGraph<TCTNode, TCTEdge> GenericThreadCreateTreeTy;
class TCT: public GenericThreadCreateTreeTy
{

public:
    typedef TCTEdge::ThreadCreateEdgeSet ThreadCreateEdgeSet;
    typedef ThreadCreateEdgeSet::iterator TCTNodeIter;
    typedef Set<const Function*> FunSet;
    typedef std::vector<const Instruction*> InstVec;
    typedef Set<const Instruction*> InstSet;
    typedef Set<const PTACallGraphNode*> PTACGNodeSet;
    typedef Map<CxtThread,TCTNode*> CxtThreadToNodeMap;
    typedef Map<CxtThread,CallStrCxt> CxtThreadToForkCxt;
    typedef Map<CxtThread,const Function*> CxtThreadToFun;
    typedef Map<const Instruction*, const Loop*> InstToLoopMap;
    typedef FIFOWorkList<CxtThreadProc> CxtThreadProcVec;
    typedef Set<CxtThreadProc> CxtThreadProcSet;
    typedef SCCDetection<PTACallGraph*> ThreadCallGraphSCC;

    /// Constructor
    TCT(PointerAnalysis* p) :pta(p),TCTNodeNum(0),TCTEdgeNum(0),MaxCxtSize(0)
    {
        tcg = SVFUtil::cast<ThreadCallGraph>(pta->getPTACallGraph());
        tcg->updateCallGraph(pta);
        //tcg->updateJoinEdge(pta);
        tcgSCC = pta->getCallGraphSCC();
        tcgSCC->find();
        build();
    }

    /// Destructor
    virtual ~TCT()
    {
        destroy();
    }
    /// Get CallICFGNode given inst
    CallICFGNode* getCallICFGNode(const Instruction* inst)
    {
        return pta->getICFG()->getCallICFGNode(inst);
    }
    /// Get SVFFuntion given Function
    const SVFFunction* getSVFFun(const Function* fun) const
    {
        return LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
    }
    /// Get SVFFModule
    SVFModule* getSVFModule() const
    {
        return pta->getModule();
    }

    /// Get TCG
    inline ThreadCallGraph* getThreadCallGraph() const
    {
        return tcg;
    }
    /// Get PTA
    inline PointerAnalysis* getPTA() const
    {
        return pta;
    }
    /// Get TCT node
    inline TCTNode* getTCTNode(NodeID id) const
    {
        return getGNode(id);
    }
    /// Whether we have aleady created this call graph edge
    TCTEdge* hasGraphEdge(TCTNode* src, TCTNode* dst, TCTEdge::CEDGEK kind) const;
    /// Get call graph edge via nodes
    TCTEdge* getGraphEdge(TCTNode* src, TCTNode* dst, TCTEdge::CEDGEK kind);

    /// Get children and parent nodes
    //@{
    inline ThreadCreateEdgeSet::const_iterator getChildrenBegin(const TCTNode* node) const
    {
        return node->OutEdgeBegin();
    }
    inline ThreadCreateEdgeSet::const_iterator getChildrenEnd(const TCTNode* node) const
    {
        return node->OutEdgeEnd();
    }
    inline ThreadCreateEdgeSet::const_iterator getParentsBegin(const TCTNode* node) const
    {
        return node->InEdgeBegin();
    }
    inline ThreadCreateEdgeSet::const_iterator getParentsEnd(const TCTNode* node) const
    {
        return node->InEdgeEnd();
    }
    //@}

    /// Get marked candidate functions
    inline const FunSet& getMakredProcs() const
    {
        return candidateFuncSet;
    }

    /// Get marked candidate functions
    inline const FunSet& getEntryProcs() const
    {
        return entryFuncSet;
    }

    /// Get Statistics
    //@{
    inline u32_t getTCTNodeNum() const
    {
        return TCTNodeNum;
    }
    inline u32_t getTCTEdgeNum() const
    {
        return TCTEdgeNum;
    }
    inline u32_t getMaxCxtSize() const
    {
        return MaxCxtSize;
    }
    //@}

    /// Find/Get TCT node
    //@{
    inline bool hasTCTNode(const CxtThread& ct) const
    {
        return ctpToNodeMap.find(ct)!=ctpToNodeMap.end();
    }
    inline TCTNode* getTCTNode(const CxtThread& ct) const
    {
        CxtThreadToNodeMap::const_iterator it = ctpToNodeMap.find(ct);
        assert(it!=ctpToNodeMap.end() && "TCT node not found??");
        return it->second;
    }
    //@}

    /// Whether it is a candidate function for indirect call
    inline bool isCandidateFun(const PTACallGraph::FunctionSet& callees) const
    {
        for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(),
                ecit = callees.end(); cit!=ecit; cit++)
        {
            if(candidateFuncSet.find((*cit)->getLLVMFun())!=candidateFuncSet.end())
                return true;
        }
        return false;
    }
    inline bool isCandidateFun(const Function* fun) const
    {
        return candidateFuncSet.find(fun)!=candidateFuncSet.end();
    }
    inline bool isCandidateFun(const SVFFunction* fun) const
    {
        return isCandidateFun(fun->getLLVMFun());
    }
    /// Whether two functions in the same callgraph scc
    inline bool inSameCallGraphSCC(const PTACallGraphNode* src,const PTACallGraphNode* dst)
    {
        return (tcgSCC->repNode(src->getId()) == tcgSCC->repNode(dst->getId()));
    }

    /// Get parent and sibling threads
    //@{
    /// Has parent thread
    inline bool hasParentThread(NodeID tid) const
    {
        const TCTNode* node = getTCTNode(tid);
        return node->getInEdges().size()==1;
    }
    /// Get parent thread
    inline NodeID getParentThread(NodeID tid) const
    {
        const TCTNode* node = getTCTNode(tid);
        assert(node->getInEdges().size()<=1 && "should have at most one parent thread");
        assert(node->getInEdges().size()==1 && "does not have a parent thread");
        const TCTEdge* edge = *(node->getInEdges().begin());
        return edge->getSrcID();
    }
    /// Get all ancestor threads
    const NodeBS getAncestorThread(NodeID tid) const
    {
        NodeBS tds;
        if(hasParentThread(tid) == false)
            return tds;

        FIFOWorkList<NodeID> worklist;
        worklist.push(getParentThread(tid));

        while(!worklist.empty())
        {
            NodeID t = worklist.pop();
            if(tds.test_and_set(t))
            {
                if(hasParentThread(t))
                    worklist.push(getParentThread(t));
            }
        }
        return tds;
    }
    /// Get sibling threads
    inline const NodeBS getSiblingThread(NodeID tid) const
    {
        NodeBS tds;
        if(hasParentThread(tid) == false)
            return tds;

        const TCTNode* node = getTCTNode(getParentThread(tid));
        for(ThreadCreateEdgeSet::const_iterator it = getChildrenBegin(node), eit = getChildrenEnd(node); it!=eit; ++it)
        {
            NodeID child = (*it)->getDstNode()->getId();
            if(child!=tid)
                tds.set(child);
        }

        return tds;
    }
    //@}

    /// get the context of a thread at its spawning site (fork site)
    const CallStrCxt& getCxtOfCxtThread(const CxtThread& ct) const
    {
        CxtThreadToForkCxt::const_iterator it = ctToForkCxtMap.find(ct);
        assert(it!=ctToForkCxtMap.end() && "Cxt Thread not found!!");
        return it->second;
    }

    /// get the start routine function of a thread
    const Function* getStartRoutineOfCxtThread(const CxtThread& ct) const
    {
        CxtThreadToFun::const_iterator it = ctToRoutineFunMap.find(ct);
        assert(it!=ctToRoutineFunMap.end() && "Cxt Thread not found!!");
        return it->second;
    }

    /// Get loop for join site
    inline const Loop* getJoinLoop(const Instruction* join)
    {
        assert(tcg->getThreadAPI()->isTDJoin(join) && "not a join site");
        InstToLoopMap::const_iterator it = joinSiteToLoopMap.find(join);
        if(it!=joinSiteToLoopMap.end())
            return it->second;
        return nullptr;
    }
    /// Return true if a join instruction must be executed inside a loop
    bool isJoinMustExecutedInLoop(const Loop* lp,const Instruction* join);
    /// Get loop for an instruction
    const Loop* getLoop(const Instruction* inst);
    /// Get dominator for a function
    const DominatorTree* getDT(const Function* fun);
    /// Get dominator for a function
    const PostDominatorTree* getPostDT(const Function* fun);
    /// Get loop for fork/join site
    const Loop* getLoop(const BasicBlock* bb);
    /// Get SE for function
    ScalarEvolution* getSE(const Instruction* inst);

    /// Get the next instructions following control flow
    void getNextInsts(const Instruction* inst, InstVec& instSet);
    /// Push calling context
    void pushCxt(CallStrCxt& cxt, const Instruction* call, const Function* callee);
    /// Match context
    bool matchCxt(CallStrCxt& cxt, const Instruction* call, const Function* callee);

    inline void pushCxt(CallStrCxt& cxt, CallSiteID csId)
    {
        cxt.push_back(csId);
        if (cxt.size() > MaxCxtSize)
            MaxCxtSize = cxt.size();
    }
    /// Whether a join site is in recursion
    inline bool isJoinSiteInRecursion(const Instruction* join) const
    {
        assert(tcg->getThreadAPI()->isTDJoin(join) && "not a join site");
        return inRecurJoinSites.find(join)!=inRecurJoinSites.end();
    }
    /// Dump calling context
    void dumpCxt(CallStrCxt& cxt);

    /// Dump the graph
    void dump(const std::string& filename);

    /// Print TCT information
    void print() const;

private:
    ThreadCallGraph* tcg;
    PointerAnalysis* pta;
    u32_t TCTNodeNum;
    u32_t TCTEdgeNum;
    u32_t MaxCxtSize;

    /// Add TCT node
    inline TCTNode* addTCTNode(const CxtThread& ct)
    {
        assert(ctpToNodeMap.find(ct)==ctpToNodeMap.end() && "Already has this node!!");
        NodeID id = TCTNodeNum;
        TCTNode* node = new TCTNode(id, ct);
        addGNode(id, node);
        TCTNodeNum++;
        ctpToNodeMap[ct] = node;
        return node;
    }
    /// Add TCT edge
    inline bool addTCTEdge(TCTNode* src, TCTNode* dst)
    {
        if (!hasGraphEdge(src, dst, TCTEdge::ThreadCreateEdge))
        {
            TCTEdge* edge = new TCTEdge(src, dst, TCTEdge::ThreadCreateEdge);
            dst->addIncomingEdge(edge);
            src->addOutgoingEdge(edge);
            TCTEdgeNum++;
            return true;
        }
        return false;
    }

    /// Build TCT
    void build();

    /// Mark relevant procedures that are backward reachable from any fork/join site
    //@{
    void markRelProcs();
    void markRelProcs(const Function* fun);
    //@}

    /// Get entry functions that are neither called by other functions nor extern functions
    void collectEntryFunInCallGraph();

    /// Collect multi-forked threads whose 1, cxt is in loop or recursion;
    /// 2, parent thread is a multi-forked thread.
    void collectMultiForkedThreads();

    /// Handle join site in loop
    //@{
    /// collect loop info for join sites
    void collectLoopInfoForJoin();
    /// Whether a given bb is a loop head of a inloop join site
    bool isLoopHeaderOfJoinLoop(const BasicBlock* bb);
    /// Whether a given bb is an exit of a inloop join site
    bool isLoopExitOfJoinLoop(const BasicBlock* bb);
    //@}

    /// Multi-forked threads
    //@{
    /// Whether an instruction is in a loop
    bool isInLoopInstruction(const Instruction* inst);
    /// Whether an instruction is in a recursion
    bool isInRecursion(const Instruction* inst) const;
    //@}

    /// Handle call relations
    void handleCallRelation(CxtThreadProc& ctp, const PTACallGraphEdge* cgEdge, CallSite call);

    /// Get or create a tct node based on CxtThread
    //@{
    inline TCTNode* getOrCreateTCTNode(const CallStrCxt& cxt, const CallInst* fork,const CallStrCxt& oldCxt, const Function* routine)
    {
        CxtThread ct(cxt,fork);
        CxtThreadToNodeMap::const_iterator it = ctpToNodeMap.find(ct);
        if(it!=ctpToNodeMap.end())
        {
            return it->second;
        }

        addCxtOfCxtThread(oldCxt,ct);
        addStartRoutineOfCxtThread(routine,ct);

        setMultiForkedAttrs(ct);
        return addTCTNode(ct);
    }
    //@}

    /// Set multi-forked thread attributes
    void setMultiForkedAttrs(CxtThread& ct)
    {
        /// non-main thread
        if(ct.getThread() != nullptr)
        {
            ct.setInloop(isInLoopInstruction(ct.getThread()));
            ct.setIncycle(isInRecursion(ct.getThread()));
        }
        /// main thread
        else
        {
            ct.setInloop(false);
            ct.setIncycle(false);
        }
    }

    /// Add context for a thread at its spawning site (fork site)
    void addCxtOfCxtThread(const CallStrCxt& cxt, const CxtThread& ct)
    {
        ctToForkCxtMap[ct] = cxt;
    }
    /// Add start routine function of a cxt thread
    void addStartRoutineOfCxtThread(const Function* fun, const CxtThread& ct)
    {
        ctToRoutineFunMap[ct] = fun;
    }

    /// WorkList helper functions
    //@{
    inline bool pushToCTPWorkList(const CxtThreadProc& ctp)
    {
        if(isVisitedCTPs(ctp)==false)
        {
            visitedCTPs.insert(ctp);
            return ctpList.push(ctp);
        }
        return false;
    }
    inline CxtThreadProc popFromCTPWorkList()
    {
        CxtThreadProc ctp = ctpList.pop();
        return ctp;
    }
    inline bool isVisitedCTPs(const CxtThreadProc& ctp) const
    {
        return visitedCTPs.find(ctp)!=visitedCTPs.end();
    }
    //@}
    /// Clean up memory
    inline void destroy()
    {
        if(tcgSCC)
            delete tcgSCC;
        tcgSCC=nullptr;
    }

    FunSet entryFuncSet; /// Procedures that are neither called by other functions nor extern functions
    FunSet candidateFuncSet; /// Procedures we care about during call graph traversing when creating TCT
    ThreadCallGraphSCC* tcgSCC; /// Thread call graph SCC
    CxtThreadProcVec ctpList;	/// CxtThreadProc List
    CxtThreadProcSet visitedCTPs; /// Record all visited ctps
    CxtThreadToNodeMap ctpToNodeMap; /// Map a ctp to its graph node
    CxtThreadToForkCxt ctToForkCxtMap; /// Map a CxtThread to the context at its spawning site (fork site).
    CxtThreadToFun ctToRoutineFunMap; /// Map a CxtThread to its start routine function.
    PTACFInfoBuilder loopInfoBuilder; ///< LoopInfo
    InstToLoopMap joinSiteToLoopMap; ///< map an inloop join to its loop class
    InstSet inRecurJoinSites;	///< Fork or Join sites in recursions
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for constraint graph so that they can be treated as
 * graphs by the generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::TCTNode*> : public GraphTraits<SVF::GenericNode<SVF::TCTNode,SVF::TCTEdge>*  >
{
};

/// Inverse GraphTraits specializations for Value flow node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::TCTNode *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::TCTNode,SVF::TCTEdge>* > >
{
};

template<> struct GraphTraits<SVF::TCT*> : public GraphTraits<SVF::GenericGraph<SVF::TCTNode,SVF::TCTEdge>* >
{
    typedef SVF::TCTNode *NodeRef;
};

} // End namespace llvm

#endif /* TCTNodeDetector_H_ */
