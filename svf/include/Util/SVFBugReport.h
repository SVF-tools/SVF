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

    inline EventType getEventType() const { return eventType; }
    virtual const std::string getEventDescription() const = 0;
    virtual const std::string getFuncName() const = 0;
    virtual const std::string getEventLoc() const = 0;
};

class BranchEvent: public GenericEvent{
    /// branch statement and branch condition true or false
protected:
    const BranchStmt *branchStmt;
    std::string description;

public:
    inline BranchEvent(const BranchStmt *branchStmt): GenericEvent(GenericEvent::Branch), branchStmt(branchStmt){ }

    inline BranchEvent(const BranchStmt *branchStmt, std::string description):
          GenericEvent(GenericEvent::Branch), branchStmt(branchStmt), description(description){ }

    const std::string getEventDescription() const;
    const std::string getFuncName() const;
    const std::string getEventLoc() const;

    /// ClassOf
    static inline bool classof(const GenericEvent* event)
    {
        return event->getEventType() == GenericEvent::Branch;
    }
};

class CallSiteEvent: public GenericEvent{
protected:
    const CallICFGNode *callSite;

public:
    inline CallSiteEvent(const CallICFGNode *callSite): GenericEvent(GenericEvent::CallSite), callSite(callSite){ }
    const std::string getEventDescription() const;
    const std::string getFuncName() const;
    const std::string getEventLoc() const;

    /// ClassOf
    static inline bool classof(const GenericEvent* event)
    {
        return event->getEventType() == GenericEvent::CallSite;
    }
};

class GenericBug{
public:
    typedef std::vector<GenericEvent *> EventStack;

public:
    enum BugType{FULLBUFOVERFLOW, PARTIALBUFOVERFLOW, NEVERFREE, PARTIALLEAK, DOUBLEFREE, FILENEVERCLOSE, FILEPARTIALCLOSE};

protected:
    BugType bugType;
    const SVFInstruction * bugInst;
    EventStack bugEventStack;

public:
    inline GenericBug(BugType bugType, const SVFInstruction * bugInst, EventStack bugEventStack): bugType(bugType), bugInst(bugInst), bugEventStack(bugEventStack){ };
    inline GenericBug(BugType bugType, const SVFInstruction * bugInst): bugType(bugType), bugInst(bugInst){ };

    virtual ~GenericBug() = default;

    inline BugType getBugType() const { return bugType; }
    const std::string getLoc() const;
    const std::string getFuncName() const;

    inline const EventStack& getEventStack() const { return bugEventStack; }

    virtual cJSON *getBugDescription() = 0;
    virtual void printBugToTerminal() = 0;
};

class BufferOverflowBug: public GenericBug{
protected:
    long long allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;

public:
    inline BufferOverflowBug(GenericBug::BugType bugType, const SVFInstruction * bugInst, EventStack eventStack,
                             long long allocLowerBound, long long allocUpperBound,
                             long long accessLowerBound, long long accessUpperBound):
          GenericBug(bugType, bugInst, eventStack), allocLowerBound(allocLowerBound),
          allocUpperBound(allocUpperBound), accessLowerBound(accessLowerBound),
          accessUpperBound(accessUpperBound){ }

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALBUFOVERFLOW || bug->getBugType() == GenericBug::FULLBUFOVERFLOW;
    }
};

class FullBufferOverflowBug: public BufferOverflowBug{
public:
    inline FullBufferOverflowBug(const SVFInstruction * bugInst, EventStack eventStack,
                             long long allocLowerBound, long long allocUpperBound,
                             long long accessLowerBound, long long accessUpperBound):
          BufferOverflowBug(GenericBug::FULLBUFOVERFLOW, bugInst, eventStack, allocLowerBound,
                            allocUpperBound, accessLowerBound, accessUpperBound){ }

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FULLBUFOVERFLOW;
    }
};

class PartialBufferOverflowBug: public BufferOverflowBug{
protected:
    long long allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;

public:
    inline PartialBufferOverflowBug(const SVFInstruction * bugInst, EventStack eventStack,
                                 long long allocLowerBound, long long allocUpperBound,
                                 long long accessLowerBound, long long accessUpperBound):
          BufferOverflowBug(GenericBug::PARTIALBUFOVERFLOW, bugInst, eventStack, allocLowerBound,
                            allocUpperBound, accessLowerBound, accessUpperBound){ }

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALBUFOVERFLOW;
    }
};

class NeverFreeBug : public GenericBug{
public:
    NeverFreeBug(const SVFInstruction *bugInst):
          GenericBug(GenericBug::NEVERFREE, bugInst){  };

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::NEVERFREE;
    }
};

class PartialLeakBug : public GenericBug{
protected:
    Set<std::string> conditionalFreePath;
public:
    PartialLeakBug(const SVFInstruction *bugInst, Set<std::string> conditionalFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), conditionalFreePath(conditionalFreePath){ }

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALLEAK;
    }
};

class DoubleFreeBug : public GenericBug{
protected:
    Set<std::string> doubleFreePath;

public:
    DoubleFreeBug(const SVFInstruction *bugInst, Set<std::string> doubleFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), doubleFreePath(doubleFreePath){ }

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::DOUBLEFREE;
    }
};

class FileNeverCloseBug : public GenericBug{
public:
    FileNeverCloseBug(const SVFInstruction *bugInst):
          GenericBug(GenericBug::NEVERFREE, bugInst){  };

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FILENEVERCLOSE;
    }
};

class FilePartialCloseBug : public GenericBug{
protected:
    Set<std::string> conditionalFileClosePath;

public:
    FilePartialCloseBug(const SVFInstruction *bugInst, Set<std::string> conditionalFileClosePath):
          GenericBug(GenericBug::PARTIALLEAK, bugInst), conditionalFileClosePath(conditionalFileClosePath){ }

    cJSON *getBugDescription();
    void printBugToTerminal();

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FILEPARTIALCLOSE;
    }
};

class SVFBugReport
{
public:
    SVFBugReport() = default;
    ~SVFBugReport();
    typedef std::vector<GenericBug *> BugVector;

protected:
    BugVector bugVector;    // maintain bugs

public:
    /*
     * function: pass bug type (i.e., GenericBug::BOA) and bug object as parameter,
     *      it will add the bug into bugQueue.
     * usage: addBug<GenericBug::BOA>(BufferOverflowBug(bugInst, 0, 10, 1, 11))
     */
    template <typename T>
    void addBug(T bug)
    {
        T *newBug = new T(bug);
        bugVector.push_back(newBug);

        // when add a bug, also print it to terminal
        newBug->printBugToTerminal();
    }

    //dump all bugs to string, in Json format
    std::string toString();
};
}

#endif