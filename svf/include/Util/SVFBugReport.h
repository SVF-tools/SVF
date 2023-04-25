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
    enum BugType{BOA, NEVERFREE, PARTIALLEAK, DOUBLEFREE, FILENEVERCLOSE, FILEPARTIALCLOSE};

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

class NeverFreeBug : public GenericBug{
public:
    NeverFreeBug(const SVFInstruction *bugInst):
          GenericBug(GenericBug::NEVERFREE, bugInst){  };

    cJSON *getBugDescription();
    void printBugToTerminal();
};

class PartialLeakBug : public GenericBug{
protected:
    Set<std::string> conditionalFreePath;
public:
    PartialLeakBug(const SVFInstruction *bugInst, Set<std::string> conditionalFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), conditionalFreePath(conditionalFreePath){ }

    cJSON *getBugDescription();
    void printBugToTerminal();
};

class DoubleFreeBug : public GenericBug{
protected:
    Set<std::string> doubleFreePath;

public:
    DoubleFreeBug(const SVFInstruction *bugInst, Set<std::string> doubleFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), doubleFreePath(doubleFreePath){ }

    cJSON *getBugDescription();
    void printBugToTerminal();
};

class FileNeverCloseBug : public GenericBug{
public:
    FileNeverCloseBug(const SVFInstruction *bugInst):
          GenericBug(GenericBug::NEVERFREE, bugInst){  };

    cJSON *getBugDescription();
    void printBugToTerminal();
};

class PartialFileCloseBug : public GenericBug{
protected:
    Set<std::string> conditionalFileClosePath;

public:
    PartialFileCloseBug(const SVFInstruction *bugInst, Set<std::string> conditionalFileClosePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), conditionalFileClosePath(conditionalFileClosePath){ }

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
    EventStack eventStack;  // maintain current execution events
    EventSet eventSet;      // maintain all added events
    BugVector bugVector;    // maintain bugs
    EventStackVector eventStackVector;  // maintain each bug's events

public:
    // push callsite to event stack
    void pushToEventStack(const CallICFGNode *callSite);

    // pop callsite from event stack
    void popFromEventStack();

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
    std::string toString();
};
}

#endif


