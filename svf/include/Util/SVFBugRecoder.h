//===- SVFBugRecoder.h -- Base class for statistics---------------------------------//
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

namespace SVF{

/*!
 * Bug Detector Recoder
 */


class GenericEvent{
public:
    enum EventType{Branch, Caller, CallSite, Loop};
    static std::map<EventType, std::string> eventType2Str;
private:
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
private:
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
//
//class CallerEvent: public GenericEvent{
//    //function name
//};

class CallSiteEvent: public GenericEvent{
    //call ICFGNode
private:
    const CallICFGNode *callSite;
public:
    inline CallSiteEvent(const CallICFGNode *callSite): GenericEvent(GenericEvent::CallSite), callSite(callSite){ }
    std::string getEventDescription();
    std::string getFuncName();
    std::string getEventLoc();
};

//class LoopEvent: public GenericEvent{
//    //loop num
//    //loop header position
//};

class GenericBug{
public:
    enum BugType{BOA};
    static std::map<BugType, std::string> bugType2Str;
private:
    BugType bugType;
    const SVFInstruction * bugInst;
public:
    inline GenericBug(BugType bugType, const SVFInstruction * bugInst): bugType(bugType), bugInst(bugInst){ };
    virtual ~GenericBug() = default;

    inline BugType getBugType(){ return bugType; }
    std::string getLoc();
    std::string getFuncName();

    virtual std::string getBugDescription() = 0;
};

class BufferOverflowBug: public GenericBug{
private:
    long long allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;
public:
    inline BufferOverflowBug(const SVFInstruction * bugInst, long long allocLowerBound,
                             long long allocUpperBound, long long accessLowerBound,
                             long long accessUpperBound):
          GenericBug(GenericBug::BOA, bugInst), allocLowerBound(allocLowerBound),
          allocUpperBound(allocUpperBound), accessLowerBound(accessLowerBound),
          accessUpperBound(accessUpperBound){ }
    std::string getBugDescription();
};

class SVFBugRecoder{
public:
    SVFBugRecoder() = default;
    ~SVFBugRecoder();
    typedef std::vector<GenericEvent *> EventStack;
    typedef std::vector<GenericBug *> BugVector;
    typedef std::vector<EventStack *> EventStackVector;
private:
    EventStack eventStack;  //maintain current execution events
    BugVector bugVector;    //maintain bugs
    EventStackVector eventStackVector;  //maintain each bug's events
public:
    //push callsite
    void pushCallSite(const CallICFGNode *callSite);

    //pop callsite
    void popCallSite();

//    //push caller
//    void pushCaller();
//
//    //pop caller
//    void popCaller();
//
    //push branch
    void pushBranch();

    //pop branch
    void popBranch();
//
//    //push loop
//    void pushLoop();
//
//    //pop loop
//    void popLoop();

    // add a bug
    template <typename T>
    void addBug(T bug)
    {
        T *newBug = new T(bug);
        bugVector.push_back(newBug);

        EventStack *newEventStack = new EventStack(eventStack);
        eventStackVector.push_back(newEventStack);
    }

    //dump to string
    std::string dumpBug();
};
}

#endif


