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
#include "AE/Core/ICFGWTO.h"

#include "WPA/Andersen.h"
#include "Util/SVFBugReport.h"
#include "AE/Svfexe/BufOverflowChecker.h"
namespace SVF
{
class AbstractExecutionICFG;


class AbstractExecutionICFG : public AbstractExecution
{
    friend class AEStat;
    friend class AEAPI;

public:
    /// Constructor
    AbstractExecutionICFG();

    virtual void runOnModule(SVFIR* svfModule);


    /// Destructor
    virtual ~AbstractExecutionICFG();

    /// Program entry
    void analyse();

    static bool classof(const AbstractExecutionICFG* ae)
    {
        return ae->getKind() == AEKind::AbstractExecutionICFG;
    }

    AEKind getKind() const
    {
        return _kind;
    }

protected:

    /**
     * Check if execution state exist by merging states of predecessor blocks
     *
     * @param block The basic block to analyse
     * @return if this block has preceding execution state
     */
    bool hasInEdgesES(const ICFGNode* node);

    /**
     * handle one instruction in svf basic blocks
     *
     * @param node ICFGNode which has a single instruction
     */
    virtual void handleICFGNode(const ICFGNode *node);

    /**
     * handle wto cycle (loop)
     *
     * @param cycle WTOCycle which has weak topo order of basic blocks and nested cycles
     */
    void handleICFGCycle(const ICFGWTOCycle *cycle);

    /**
     * handle user defined function, ext function is not included.
     *
     * @param func SVFFunction which has a series of basic blocks
     */
    void handleFunc(const SVFFunction *func);

    virtual bool isExtCall(const CallICFGNode* callNode);
    virtual void extCallPass(const CallICFGNode* callNode);
    virtual bool isRecursiveCall(const CallICFGNode* callNode);
    virtual void recursiveCallPass(const CallICFGNode* callNode);
    virtual bool isDirectCall(const CallICFGNode* callNode);
    virtual void directCallFunPass(const CallICFGNode* callNode);
    virtual bool isIndirectCall(const CallICFGNode* callNode);
    virtual void indirectCallFunPass(const CallICFGNode* callNode);



private:

    // helper functions in hasInEdgesES
    bool isFunEntry(const ICFGNode* block);
    bool isGlobalEntry(const ICFGNode* block);

    // helper functions in handleCycle
    bool widenFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es);
    bool narrowFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es);

    // private data
    Map<const ICFGNode*, IntervalExeState> _preES;
    Map<const ICFGNode*, IntervalExeState> _postES;
    Map<const SVFFunction*, ICFGWTO *> _funcToICFGWTO;
    std::string _moduleName;

};
class BufOverflowCheckerICFGAPI: public AEAPI
{
public:
    BufOverflowCheckerICFGAPI() = delete;
    BufOverflowCheckerICFGAPI(AbstractExecutionICFG * ae, AEStat * stat): AEAPI(ae, stat)
    {
        initExtFunMap();
        initExtAPIBufOverflowCheckRules();
        _kind = AEKind::BufOverflowChecker;
    }
    static bool classof(const AEAPI* api)
    {
        return api->getKind() == AEKind::BufOverflowChecker;
    }

    /**
    * the map of external function to its API type
    *
    * it initialize the ext apis about buffer overflow checking
     */
    virtual void initExtFunMap();

    /**
    * the map of ext apis of buffer overflow checking rules
    *
    * it initialize the rules of extapis about buffer overflow checking
    * e.g. memcpy(dst, src, sz) -> we check allocSize(dst)>=sz and allocSize(src)>=sz
     */
    void initExtAPIBufOverflowCheckRules();

    /**
    * handle external function call regarding buffer overflow checking
    * e.g. memcpy(dst, src, sz) -> we check allocSize(dst)>=sz and allocSize(src)>=sz
    *
    * @param call call node whose callee is external function
     */
    void handleExtAPI(const CallICFGNode *call) ;
    /**
    * detect buffer overflow from strcpy like apis
    * e.g. strcpy(dst, src), if dst is shorter than src, we will throw buffer overflow
    *
    * @param call call node whose callee is strcpy-like external function
    * @return true if the buffer overflow is detected
     */
    bool detectStrcpy(const CallICFGNode *call);
    /**
    * detect buffer overflow from strcat like apis
    * e.g. strcat(dst, src), if dst is shorter than src, we will throw buffer overflow
    *
    * @param call call node whose callee is strcpy-like external function
    * @return true if the buffer overflow is detected
     */
    bool detectStrcat(const CallICFGNode *call);

    /**
     * detect buffer overflow by giving a var and a length
     * e.g. int x[10]; x[10] = 1;
     * we call canSafelyAccessMemory(x, 11 * sizeof(int));
     *
     * @param value the value of the buffer overflow checkpoint
     * @param len the length of the buffer overflow checkpoint
     * @return true if the buffer overflow is detected
     */
    bool canSafelyAccessMemory(const SVFValue *value, const IntervalValue &len, const ICFGNode *curNode);


    Map<NodeID, const GepStmt*> _addrToGep;
    Map<std::string, std::vector<std::pair<u32_t, u32_t>>> _extAPIBufOverflowCheckRules;
};

class BufOverflowCheckerICFG: public AbstractExecutionICFG
{
    friend BufOverflowCheckerICFGAPI;

public:
    BufOverflowCheckerICFG() : AbstractExecutionICFG()
    {
        _kind = AEKind::BufOverflowCheckerICFG;
    }

    static bool classof(const AbstractExecution* ae)
    {
        return ae->getKind() == AEKind::BufOverflowCheckerICFG;
    }

    void initExtAPI() override
    {
        _api = new BufOverflowCheckerICFGAPI(this, _stat);
    }

private:
    /**
    * handle SVF statement regarding buffer overflow checking
    *
    * @param stmt SVF statement
     */
    virtual void handleSVFStatement(const SVFStmt *stmt) override;

    /**
    * handle ICFGNode regarding buffer overflow checking
    *
    * @param node ICFGNode
     */
    virtual void handleICFGNode(const SVF::ICFGNode *node) override;

    /**
    * check buffer overflow at ICFGNode which is a checkpoint
    *
    * @param node ICFGNode
    * @return true if the buffer overflow is detected
     */
    bool detectBufOverflow(const ICFGNode *node);

    /**
    * add buffer overflow bug to recoder
    *
    * @param e the exception that is thrown by BufOverflowChecker
    * @param node ICFGNode that causes the exception
     */
    void addBugToRecoder(const BufOverflowException& e, const ICFGNode* node);
};

}