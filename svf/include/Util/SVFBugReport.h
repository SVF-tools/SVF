//===- SVFBugReport.h -- Base class for statistics---------------------------------//
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
// Created by JoelYang on 2023/4/19.
//

#ifndef SVF_BUGRECODER_H
#define SVF_BUGRECODER_H

#include <vector>
#include "SVFIR/SVFValue.h"
#include "SVFIR/SVFVariables.h"
#include "SVFIR/SVFStatements.h"
#include "Graphs/ICFGNode.h"
#include <string>
#include <map>
#include "Util/cJSON.h"
#include <set>

namespace SVF{

/*!
 * Bug Detector Recoder
 */


class GenericEvent{
public:
    enum EventType{Branch, Caller, CallSite, Loop};
    static std::map<EventType, std::string> eventType2Str;

protected:
    EventType eventType;

public:
    inline GenericEvent(EventType eventType): eventType(eventType){ };
    virtual ~GenericEvent() = default;

    inline EventType getEventType() { return eventType; }
    virtual std::string getEventDescription() = 0;
    virtual std::string getFuncName() = 0;
    virtual std::string getEventLoc() = 0;
};

class BranchEvent: public GenericEvent{
    //branch statement and branch condition true or false
protected:
    const BranchStmt *branchStmt;
    std::string description;

public:
    inline BranchEvent(const BranchStmt *branchStmt): GenericEvent(GenericEvent::Branch), branchStmt(branchStmt){ }

    inline BranchEvent(const BranchStmt *branchStmt, std::string description):
          GenericEvent(GenericEvent::Branch), branchStmt(branchStmt), description(description){ }

    std::string getEventDescription();
    std::string getFuncName();
    std::string getEventLoc();
};

class CallSiteEvent: public GenericEvent{
protected:
    const CallICFGNode *callSite;

public:
    inline CallSiteEvent(const CallICFGNode *callSite): GenericEvent(GenericEvent::CallSite), callSite(callSite){ }
    std::string getEventDescription();
    std::string getFuncName();
    std::string getEventLoc();
};

class GenericBug{
public:
    typedef std::vector<GenericEvent *> EventStack;

public:
    enum BugType{BOA,MEMLEAK,DOUBLEFREE,FILENOTCLOSE};
    static std::map<BugType, std::string> bugType2Str;

protected:
    BugType bugType;
    const SVFInstruction * bugInst;
    EventStack *bugEventStack = nullptr;

public:
    inline GenericBug(BugType bugType, const SVFInstruction * bugInst): bugType(bugType), bugInst(bugInst){ };
    virtual ~GenericBug() = default;

    inline BugType getBugType(){ return bugType; }
    std::string getLoc();
    std::string getFuncName();
    inline void setEventStack(EventStack *eventStack) { bugEventStack = eventStack; }
    /// return nullptr if the bug is not path sensitive
    inline EventStack *getEventStack(){ return bugEventStack; }

    virtual cJSON *getBugDescription() = 0;
    virtual void printBugToTerminal() = 0;
};

class BufferOverflowBug: public GenericBug{
protected:
    long long allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;
    bool isFull;  // if the overflow is full overflow

public:
    inline BufferOverflowBug(const SVFInstruction * bugInst, bool isFull, long long allocLowerBound,
                             long long allocUpperBound, long long accessLowerBound,
                             long long accessUpperBound):
          GenericBug(GenericBug::BOA, bugInst), allocLowerBound(allocLowerBound),
          allocUpperBound(allocUpperBound), accessLowerBound(accessLowerBound),
          accessUpperBound(accessUpperBound), isFull(isFull){ }
    cJSON *getBugDescription();
    void printBugToTerminal();
};

class SaberBugReport : public GenericBug{
protected:
    bool isFull;  // if the leakage is full leakage
    Set<std::string> conditionPaths;

public:
    SaberBugReport(BugType bugType, const SVFInstruction *bugInst, bool isFull):
          GenericBug(bugType, bugInst), isFull(isFull){  };
    SaberBugReport(BugType bugType, const SVFInstruction *bugInst, bool isFull,
               Set<std::string> conditionPaths):
          GenericBug(bugType, bugInst), isFull(isFull), conditionPaths(conditionPaths){ }

    cJSON *getBugDescription();
    void printBugToTerminal();
};

class SVFBugReport
{
public:
    SVFBugReport() = default;
    ~SVFBugReport();
    typedef std::vector<GenericEvent *> EventStack;
    typedef std::vector<GenericEvent *> EventSet;
    typedef std::vector<GenericBug *> BugVector;
    typedef std::vector<EventStack *> EventStackVector;

protected:
    EventStack eventStack;  //maintain current execution events
    EventSet eventSet;      //for destruction
    BugVector bugVector;    //maintain bugs
    EventStackVector eventStackVector;  //maintain each bug's events

public:
    // push callsite to event stack
    void pushCallSite(const CallICFGNode *callSite);

    // pop callsite from event stack
    void popCallSite();

    /*
     * function: pass bug type (i.e., GenericBug::BOA) and bug object as parameter,
     *      it will add the bug into bugQueue.
     * usage: addBug<GenericBug::BOA>(BufferOverflowBug(bugInst, 0, 10, 1, 11))
     */
    template <typename T>
    void addBug(T bug, bool recordEvents)
    {
        T *newBug = new T(bug);
        bugVector.push_back(newBug);

        if(recordEvents)
        {
            EventStack* newEventStack = new EventStack(eventStack);
            eventStackVector.push_back(newEventStack);
            newBug->setEventStack(newEventStack);
        }
        // when add a bug, also print it to terminal
        newBug->printBugToTerminal();
    }

    //dump all bugs to string, in Json format
    std::string dumpBug();
};
}

#endif


