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
#include "AE/Svfexe/AbstractExecution.h"

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



private:

    // helper functions in hasInEdgesES
    bool isFunEntry(const ICFGNode* block);
    bool isGlobalEntry(const ICFGNode* block);

    // helper functions in handleCycle
    bool widenFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es);
    bool narrowFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es);

    // private data
    AndersenWaveDiff *_ander;
    Map<const ICFGNode*, IntervalExeState> _preES;
    Map<const ICFGNode*, IntervalExeState> _postES;
    Map<const SVFFunction*, ICFGWTO *> _funcToWTO;
    Set<const SVFFunction*> _recursiveFuns;
    std::string _moduleName;

};

}