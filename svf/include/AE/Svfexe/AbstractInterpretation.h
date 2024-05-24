//===- AbstractInterpretation.h -- Abstract Execution----------//
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
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)
//===----------------------------------------------------------------------===//


//
// Created by Jiawei Wang on 2024/1/10.
//

#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/SVFIR2AbsState.h"
#include "Util/SVFBugReport.h"
#include "WPA/Andersen.h"

namespace SVF
{
class AbstractInterpretation;
class AEStat;
class AEAPI;

template<typename T> class FILOWorkList;

enum class AEKind
{
    AbstractExecution,
    BufOverflowChecker,
};

/// AEStat: Statistic for AE
class AEStat : public SVFStat
{
public:
    void countStateSize();
    AEStat(AbstractInterpretation* ae) : _ae(ae)
    {
        startTime = getClk(true);
    }
    ~AEStat()
    {
    }
    inline std::string getMemUsage()
    {
        u32_t vmrss, vmsize;
        return SVFUtil::getMemoryUsageKB(&vmrss, &vmsize) ? std::to_string(vmsize) + "KB" : "cannot read memory usage";
    }

    void finializeStat();
    void performStat() override;
    void reportBug();

public:
    AbstractInterpretation* _ae;
    s32_t count{0};
    std::string memory_usage;
    std::string memUsage;
    std::string bugStr;


    u32_t& getFunctionTrace()
    {
        if (generalNumMap.count("Function_Trace") == 0)
        {
            generalNumMap["Function_Trace"] = 0;
        }
        return generalNumMap["Function_Trace"];
    }
    u32_t& getBlockTrace()
    {
        if (generalNumMap.count("Block_Trace") == 0)
        {
            generalNumMap["Block_Trace"] = 0;
        }
        return generalNumMap["Block_Trace"];
    }
    u32_t& getICFGNodeTrace()
    {
        if (generalNumMap.count("ICFG_Node_Trace") == 0)
        {
            generalNumMap["ICFG_Node_Trace"] = 0;
        }
        return generalNumMap["ICFG_Node_Trace"];
    }
};

/// AbstractInterpretation is same as Abstract Execution
class AbstractInterpretation
{
    friend class AEStat;
    friend class AEAPI;

public:
    enum ExtAPIType { UNCLASSIFIED, MEMCPY, MEMSET, STRCPY, STRCAT };
    typedef SCCDetection<PTACallGraph*> CallGraphSCC;
    /// Constructor
    AbstractInterpretation();

    virtual void runOnModule(ICFG* icfg);

    /// Destructor
    virtual ~AbstractInterpretation();

    /// Program entry
    void analyse();

    static bool classof(const AbstractInterpretation* ae)
    {
        return ae->getKind() == AEKind::AbstractExecution;
    }

    AEKind getKind() const
    {
        return _kind;
    }

protected:
    /// Global ICFGNode is handled at the entry of the program,
    virtual void handleGlobalNode();

    /// mark recursive functions by detecting SCC in callgraph
    void markRecursiveFuns();

    /**
     * Check if execution state exist by merging states of predecessor nodes
     *
     * @param curNode The ICFGNode to analyse
     * @return if this node has preceding execution state
     */
    bool propagateStateIfFeasible(const ICFGNode* curNode);

    /**
     * Check if execution state exist at the branch edge
     *
     * @param intraEdge the edge from CmpStmt to the next node
     * @return if this edge is feasible
     */
    bool isBranchFeasible(const IntraCFGEdge* intraEdge, AbstractState& as);

    /**
     * handle instructions in ICFGSingletonWTO
     *
     * @param block basic block that has one instruction or a series of instructions
     */
    virtual void handleWTONode(const ICFGSingletonWTO *icfgSingletonWto);

    /**
     * handle one instruction in ICFGNode
     *
     * @param node ICFGNode which has a single instruction
     */
    virtual void handleICFGNode(const ICFGNode* node);

    /**
     * handle call node in ICFGNode
     *
     * @param node ICFGNode which has a single CallICFGNode
     */
    virtual void handleCallSite(const ICFGNode* node);

    /**
     * handle wto cycle (loop)
     *
     * @param cycle WTOCycle which has weak topo order of basic blocks and nested cycles
     */
    virtual void handleCycle(const ICFGCycleWTO* cycle);

    /**
     * handle user defined function, ext function is not included.
     *
     * @param func SVFFunction which has a series of basic blocks
     */
    virtual void handleFunc(const SVFFunction* func);

    /**
     * handle SVF Statement like CmpStmt, CallStmt, GepStmt, LoadStmt, StoreStmt, etc.
     *
     * @param stmt SVFStatement which is a value flow of instruction
     */
    virtual void handleSVFStatement(const SVFStmt* stmt);

    /**
     * Check if this callnode is recursive call and skip it.
     *
     * @param callnode CallICFGNode which calls a recursive function
     */
    virtual void SkipRecursiveCall(const CallICFGNode* callnode);


    /**
    * Check if this cmpStmt and succ are satisfiable to the execution state.
    *
    * @param cmpStmt CmpStmt is a conditional branch statement
    * @param succ the value of cmpStmt (True or False)
    * @return if this ICFGNode has preceding execution state
    */
    bool isCmpBranchFeasible(const CmpStmt* cmpStmt, s64_t succ,
                             AbstractState& as);

    /**
    * Check if this SwitchInst and succ are satisfiable to the execution state.
    *
    * @param var var in switch inst
    * @param succ the case value of switch inst
    * @return if this ICFGNode has preceding execution state
    */
    bool isSwitchBranchFeasible(const SVFVar* var, s64_t succ,
                                AbstractState& as);


    /**
    * handle external function call
    *
    * @param call call node whose callee is external function
    */
    virtual void handleExtAPI(const CallICFGNode *call);

    /**
    * the map of external function to its API type
    *
    * In AEAPI, this function is mainly used for abstract explanation.
    * In subclasses, this function is mainly used to check specific bugs
    */
    virtual void initExtFunMap();

    /**
    * get byte size of alloca inst
    *
    * @param addr Address Stmt like malloc/calloc/ALLOCA/StackAlloc
    * @return the byte size e.g. int32_t a[10] -> return 40
    */
    u32_t getAllocaInstByteSize(AbstractState& as, const AddrStmt *addr);

    /**
    * get byte size of alloca inst
    * e.g. source code str = "abc", there are str value, return "abc"
    *
    * @param rhs SVFValue of string
    * @return the string
    */
    std::string strRead(AbstractState& as,const SVFValue* rhs);

    /**
    * get length of string
    * e.g. source code str = "abc", return 3
    *
    * @param strValue SVFValue of string
    * @return IntervalValue of string length
    */
    IntervalValue getStrlen(AbstractState& as, const SVF::SVFValue *strValue);

    /**
    * get memory allocation size
    * e.g  arr = new int[10]
    *      ....
    *      memset(arr, 1, 10* sizeof(int))
    * when we trace the 'arr', we can get the alloc size [40, 40]
    * @param value to be traced
    * @return IntervalValue of allocation size
    */
    IntervalValue traceMemoryAllocationSize(AbstractState& as, const SVFValue *value);
    /**
    * execute strcpy in abstract execution
    * e.g  arr = new char[10]
    *      str = "abc"
    *      strcpy(arr, str)
    * we can set arr[0]='a', arr[1]='b', arr[2]='c', arr[3]='\0'
    * @param call callnode of strcpy like api
    */
    virtual void handleStrcpy(const CallICFGNode *call);
    /**
    * execute strcpy in abstract execution
    * e.g  arr[10] = "abc"
    *      str = "de"
    *      strcat(arr, str)
    * we can set arr[3]='d', arr[4]='e', arr[5]='\0'
    * @param call callnode of strcat like api
    */
    virtual void handleStrcat(const CallICFGNode *call);
    /**
    * execute memcpy in abstract execution
    * e.g  arr = new char[10]
    *      str = "abcd"
    *      memcpy(arr, str, 5)
    * we can set arr[3]='d', arr[4]='e', arr[5]='\0'
    * @param call callnode of memcpy like api
    */
    virtual void handleMemcpy(AbstractState& as, const SVFValue* dst, const SVFValue* src, IntervalValue len, u32_t start_idx);
    /**
    * execute memset in abstract execution
    * e.g  arr = new char[10]
    *      memset(arr, 'c', 2)
    * we can set arr[0]='c', arr[1]='c', arr[2]='\0'
    * @param call callnode of memset like api
    */
    virtual void handleMemset(AbstractState& as, const SVFValue* dst, IntervalValue elem, IntervalValue len);

    /**
    * if this NodeID in SVFIR is a pointer, get the pointee type
    * e.g  arr = (int*) malloc(10*sizeof(int))
    *      getPointeeType(arr) -> return int
    * we can set arr[0]='c', arr[1]='c', arr[2]='\0'
    * @param call callnode of memset like api
    */
    const SVFType* getPointeeElement(AbstractState& as, NodeID id);

    void collectCheckPoint();
    void checkPointAllSet();
    // helper functions for traceMemoryAllocationSize and canSafelyAccessMemory
    void AccessMemoryViaRetNode(const CallICFGNode *callnode, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaCopyStmt(const CopyStmt *copy, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaLoadStmt(AbstractState& as, const LoadStmt *load, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaCallArgs(const SVF::SVFArgument *arg, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);


    /// protected data members, also used in subclasses
    SVFIR* _svfir;
    PTACallGraph* _callgraph;
    /// Execution State, used to store the Interval Value of every SVF variable
    SVFIR2AbsState* _svfir2AbsState;
    AEAPI* _api{nullptr};

    ICFG* _icfg;
    AEStat* _stat;
    AEKind _kind;

    Set<std::string> _bugLoc;
    SVFBugReport _recoder;
    std::vector<const CallICFGNode*> _callSiteStack;
    Map<const ICFGNode*, std::string> _nodeToBugInfo;
    AndersenWaveDiff* _ander;
    Map<const SVFFunction*, ICFGWTO*> _funcToWTO;
    Set<const SVFFunction*> _recursiveFuns;

private:
    // helper functions in handleCallSite
    virtual bool isExtCall(const CallICFGNode* callNode);
    virtual void extCallPass(const CallICFGNode* callNode);
    virtual bool isRecursiveCall(const CallICFGNode* callNode);
    virtual void recursiveCallPass(const CallICFGNode* callNode);
    virtual bool isDirectCall(const CallICFGNode* callNode);
    virtual void directCallFunPass(const CallICFGNode* callNode);
    virtual bool isIndirectCall(const CallICFGNode* callNode);
    virtual void indirectCallFunPass(const CallICFGNode* callNode);

protected:
    // helper functions in handleCycle
    bool isFixPointAfterWidening(const ICFGNode* cycle_head,
                                 AbstractState& pre_as);
    bool isFixPointAfterNarrowing(const SVF::ICFGNode* cycle_head,
                                  SVF::AbstractState& pre_as);

    AbstractState& getAbsState(const ICFGNode* node)
    {
        const ICFGNode* repNode = _icfg->getRepNode(node);
        if (_postAbsTrace.count(repNode) == 0)
        {
            assert(0 && "No preAbsTrace for this node");
        }
        else
        {
            return _postAbsTrace[repNode];
        }
    }

protected:
    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallSite &)>> _func_map;
    Set<const CallICFGNode*> _checkpoints;
    Set<std::string> _checkpoint_names;
    Map<const ICFGNode*, AbstractState> _preAbsTrace;
    Map<const ICFGNode*, AbstractState> _postAbsTrace;
    std::string _moduleName;
};
}