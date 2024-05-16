//===- BufOverflowChecker.cpp -- BufOVerflowChecker Client for Abstract Execution---//
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
// Created by Jiawei Wang on 2024/1/12.
//

#include "AE/Svfexe/AbstractInterpretation.h"

namespace SVF
{

struct BufOverflowException: public std::exception
{
public:
    BufOverflowException(std::string msg, u32_t allocLb,
                         u32_t allocUb, u32_t accessLb, u32_t accessUb, const SVFValue* allocVal) :
        _msg(msg), _allocLb(allocLb), _allocUb(allocUb),
        _accessLb(accessLb), _accessUb(accessUb), _allocVar(allocVal)
    {
    }

    u32_t getAllocLb() const
    {
        return _allocLb;
    }

    void setAllocLb(u32_t allocLb)
    {
        _allocLb = allocLb;
    }

    u32_t getAllocUb() const
    {
        return _allocUb;
    }

    void setAllocUb(u32_t allocUb)
    {
        _allocUb = allocUb;
    }

    u32_t getAccessLb() const
    {
        return _accessLb;
    }

    void setAccessLb(u32_t accessLb)
    {
        _accessLb = accessLb;
    }

    u32_t getAccessUb() const
    {
        return _accessUb;
    }

    void setAccessUb(u32_t accessUb)
    {
        _accessUb = accessUb;
    }

    const SVFValue* getAllocVar() const
    {
        return _allocVar;
    }

    const char* what() const noexcept override
    {
        return _msg.c_str();
    }


protected:
    std::string _msg;
    u32_t _allocLb, _allocUb, _accessLb, _accessUb;
    const SVFValue* _allocVar;
};

class BufOverflowChecker: public AbstractInterpretation
{
public:
    BufOverflowChecker() : AbstractInterpretation()
    {
        initExtFunMap();
        _kind = AEKind::BufOverflowChecker;
        initExtAPIBufOverflowCheckRules();
    }

    static bool classof(const AbstractInterpretation* ae)
    {
        return ae->getKind() == AEKind::BufOverflowChecker;
    }

protected:
    /**
    * the map of external function to its API type
    *
    * it initialize the ext apis about buffer overflow checking
    */
    virtual void initExtFunMap() override;

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
    void handleExtAPI(const CallICFGNode *call) override;
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

private:
    Map<NodeID, const GepStmt*> _addrToGep;
    Map<std::string, std::vector<std::pair<u32_t, u32_t>>> _extAPIBufOverflowCheckRules;

};
}