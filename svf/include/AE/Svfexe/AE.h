//===- AE.cpp -- Abstract Execution---------------------------------//
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
// Created by Jiawei Wang on 2024/1/10.
//
#include <exception>
#include <string>
#include "AE/Svfexe/SVFIR2ItvExeState.h"
#include "Util/WorkList.h"
#include "MSSA/SVFGBuilder.h"
#include "AE/Core/CFBasicBlockGWTO.h"
#include "WPA/Andersen.h"
#include "Util/SVFBugReport.h"

namespace SVF
{
class AE;
class AEStat;
class AEAPI;


enum class AEKind
{
    AE,
    BufOverflowChecker
};

/// AEStat: Statistic for AE
class AEStat : public SVFStat
{
public:
    void countStateSize();
    AEStat(AE *ae): _ae(ae)
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
    AE *_ae;
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

// AE: Abstract Execution
class AE
{
    friend class AEStat;
    friend class AEAPI;

public:
    typedef SCCDetection<PTACallGraph *> CallGraphSCC;
    /// Constructor
    AE();

    virtual void initExtAPI();

    virtual void runOnModule(SVFIR* svfModule);


    /// Destructor
    virtual ~AE();

    /// Program entry
    void analyse();

    static bool classof(const AE* ae)
    {
        return ae->getKind() == AEKind::AE;
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
     * Check if execution state exist by merging states of predecessor blocks
     *
     * @param block The basic block to analyse
     * @return if this block has preceding execution state
     */
    bool hasInEdgesES(const CFBasicBlockNode *block);

    /**
     * Check if execution state exist at the branch edge
     *
     * @param intraEdge the edge from CmpStmt to the next Block
     * @return if this edge is feasible
     */
    bool hasBranchES(const IntraCFGEdge* intraEdge, IntervalExeState& es);

    /**
     * handle instructions in svf basic blocks
     *
     * @param block basic block that has a series of instructions
     */
    void handleBlock(const CFBasicBlockNode *block);

    /**
     * handle one instruction in svf basic blocks
     *
     * @param node ICFGNode which has a single instruction
     */
    virtual void handleICFGNode(const ICFGNode *node);

    /**
     * handle call node in svf basic blocks
     *
     * @param node ICFGNode which has a single CallICFGNode
     */
    virtual void handleCallSite(const ICFGNode* node);

    /**
     * handle wto cycle (loop)
     *
     * @param cycle WTOCycle which has weak topo order of basic blocks and nested cycles
     */
    virtual void handleCycle(const CFBasicBlockGWTOCycle *cycle);

    /**
     * handle user defined function, ext function is not included.
     *
     * @param func SVFFunction which has a series of basic blocks
     */
    virtual void handleFunc(const SVFFunction *func);

    /**
     * handle SVF Statement like CmpStmt, CallStmt, GepStmt, LoadStmt, StoreStmt, etc.
     *
     * @param stmt SVFStatement which is a value flow of instruction
     */
    virtual void handleSVFStatement(const SVFStmt *stmt);

    /**
     * Check if this callnode is recursive call and skip it.
     *
     * @param callnode CallICFGNode which calls a recursive function
     */
    virtual void SkipRecursiveCall(const CallICFGNode *callnode);

    /**
    * Check if this function is recursive function and skip it.
    *
    * @param func SVFFunction is a recursive function
    */
    virtual void SkipRecursiveFunc(const SVFFunction *func);

    /**
    * Check if this cmpStmt and succ are satisfiable to the execution state.
    *
    * @param cmpStmt CmpStmt is a conditional branch statement
    * @param succ the value of cmpStmt (True or False)
    * @return if this block has preceding execution state
    */
    bool hasCmpBranchES(const CmpStmt* cmpStmt, s64_t succ, IntervalExeState& es);

    /**
    * Check if this SwitchInst and succ are satisfiable to the execution state.
    *
    * @param var var in switch inst
    * @param succ the case value of switch inst
    * @return if this block has preceding execution state
    */
    bool hasSwitchBranchES(const SVFVar* var, s64_t succ, IntervalExeState& es);

    /// protected data members, also used in subclasses
    SVFIR* _svfir;
    PTACallGraph* _callgraph;
    /// Execution State, used to store the Interval Value of every SVF variable
    SVFIR2ItvExeState* _svfir2ExeState;
    AEAPI* _api{nullptr};

    ICFG* _icfg;
    AEStat* _stat;
    AEKind _kind;

    Set<std::string> _bugLoc;
    SVFBugReport _recoder;
    std::vector<const CallICFGNode*> _callSiteStack;
    Map<const ICFGNode *, std::string> _nodeToBugInfo;

private:
    // helper functions in handleCallSite
    bool isExtCall(const CallICFGNode* callNode);
    void extCallPass(const CallICFGNode* callNode);
    bool isRecursiveCall(const CallICFGNode* callNode);
    void recursiveCallPass(const CallICFGNode* callNode);
    bool isDirectCall(const CallICFGNode* callNode);
    void directCallFunPass(const CallICFGNode* callNode);
    bool isIndirectCall(const CallICFGNode* callNode);
    void indirectCallFunPass(const CallICFGNode* callNode);

    // helper functions in hasInEdgesES
    bool isFunEntry(const CFBasicBlockNode* block);
    bool isGlobalEntry(const CFBasicBlockNode* block);

    // helper functions in handleCycle
    bool widenFixpointPass(const CFBasicBlockNode* cycle_head, IntervalExeState& pre_es);
    bool narrowFixpointPass(const CFBasicBlockNode* cycle_head, IntervalExeState& pre_es);

    // private data
    CFBasicBlockGraph* _CFBlockG;
    AndersenWaveDiff *_ander;
    Map<const CFBasicBlockNode*, IntervalExeState> _preES;
    Map<const CFBasicBlockNode*, IntervalExeState> _postES;
    Map<const SVFFunction*, CFBasicBlockGWTO *> _funcToWTO;
    Set<const SVFFunction*> _recursiveFuns;
    std::string _moduleName;

};

class AEAPI
{
public:

    typedef ExeState::Addrs Addrs;
    enum ExtAPIType { UNCLASSIFIED, MEMCPY, MEMSET, STRCPY, STRCAT };
    static bool classof(const AEAPI* api)
    {
        return api->getKind() == AEKind::AE;
    }

    /**
    * Constructor of AEAPI
    *
    * @param ae Abstract Execution or its subclass
    * @param stat AEStat
    */
    AEAPI(AE* ae, AEStat* stat): _ae(ae), _stat(stat)
    {
        initExtFunMap();
        _kind = AEKind::AE;
    }

    virtual ~AEAPI() {}

    void setModule(SVFIR* svfModule)
    {
        _svfir = svfModule;
    }

    AEKind getKind() const
    {
        return _kind;
    }

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
    u32_t getAllocaInstByteSize(const AddrStmt *addr);

    /**
    * get byte size of alloca inst
    * e.g. source code str = "abc", there are str value, return "abc"
    *
    * @param rhs SVFValue of string
    * @return the string
    */
    std::string strRead(const SVFValue* rhs);

    /**
    * get length of string
    * e.g. source code str = "abc", return 3
    *
    * @param strValue SVFValue of string
    * @return IntervalValue of string length
    */
    IntervalValue getStrlen(const SVF::SVFValue *strValue);

    /**
    * get memory allocation size
    * e.g  arr = new int[10]
    *      ....
    *      memset(arr, 1, 10* sizeof(int))
    * when we trace the 'arr', we can get the alloc size [40, 40]
    * @param value to be traced
    * @return IntervalValue of allocation size
    */
    IntervalValue traceMemoryAllocationSize(const SVFValue *value);
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
    virtual void handleMemcpy(const SVFValue* dst, const SVFValue* src, IntervalValue len, u32_t start_idx);
    /**
    * execute memset in abstract execution
    * e.g  arr = new char[10]
    *      memset(arr, 'c', 2)
    * we can set arr[0]='c', arr[1]='c', arr[2]='\0'
    * @param call callnode of memset like api
    */
    virtual void handleMemset(const SVFValue* dst, IntervalValue elem, IntervalValue len);

    /**
    * if this NodeID in SVFIR is a pointer, get the pointee type
    * e.g  arr = (int*) malloc(10*sizeof(int))
    *      getPointeeType(arr) -> return int
    * we can set arr[0]='c', arr[1]='c', arr[2]='\0'
    * @param call callnode of memset like api
    */
    const SVFType* getPointeeElement(NodeID id);

    void collectCheckPoint();
    void checkPointAllSet();

protected:
    // helper functions for traceMemoryAllocationSize and canSafelyAccessMemory
    void AccessMemoryViaRetNode(const CallICFGNode *callnode, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaCopyStmt(const CopyStmt *copy, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaLoadStmt(const LoadStmt *load, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);
    void AccessMemoryViaCallArgs(const SVF::SVFArgument *arg, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited);


protected:
    AE* _ae;
    AEStat* _stat;
    SVFIR* _svfir;
    AEKind _kind;

    Map<std::string, std::function<void(const CallSite &)>> _func_map;

    Set<const CallICFGNode*> _checkpoints;
    Set<std::string> _checkpoint_names;
};
}