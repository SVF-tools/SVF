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
    enum EventType{Branch, Caller, CallSite, Loop, SourceInst};

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
    bool branchCondition;

public:
    inline BranchEvent(const BranchStmt *branchStmt, bool branchCondition):
          GenericEvent(GenericEvent::Branch), branchStmt(branchStmt), branchCondition(branchCondition){ }

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

class SourceInstructionEvent: public GenericEvent{
protected:
    const SVFInstruction *sourceSVFInst;

public:
    inline SourceInstructionEvent(const SVFInstruction *sourceSVFInst):
          GenericEvent(GenericEvent::SourceInst), sourceSVFInst(sourceSVFInst) { }

    const std::string getEventDescription() const;
    const std::string getFuncName() const;
    const std::string getEventLoc() const;

    /// ClassOf
    static inline bool classof(const GenericEvent* event)
    {
        return event->getEventType() == GenericEvent::SourceInst;
    }
};

class GenericBug{
public:
    typedef std::vector<GenericEvent *> EventStack;

public:
    enum BugType{FULLBUFOVERFLOW, PARTIALBUFOVERFLOW, NEVERFREE, PARTIALLEAK, DOUBLEFREE, FILENEVERCLOSE, FILEPARTIALCLOSE};

protected:
    BugType bugType;
    EventStack bugEventStack;

public:
    /// note: should be initialized with a bugEventStack
    inline GenericBug(BugType bugType, EventStack bugEventStack): bugType(bugType), bugEventStack(bugEventStack){ };

    virtual ~GenericBug() = default;

    /// returns bug type
    inline BugType getBugType() const { return bugType; }
    /// returns bug location as json format string
    const std::string getLoc() const;
    /// return bug source function name
    const std::string getFuncName() const;

    inline const EventStack& getEventStack() const { return bugEventStack; }

    virtual cJSON *getBugDescription() = 0;
    virtual void printBugToTerminal() = 0;
};

class BufferOverflowBug: public GenericBug{
protected:
    long long allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;

public:
    inline BufferOverflowBug(GenericBug::BugType bugType, EventStack eventStack,
                             long long allocLowerBound, long long allocUpperBound,
                             long long accessLowerBound, long long accessUpperBound):
          GenericBug(bugType, eventStack), allocLowerBound(allocLowerBound),
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
    inline FullBufferOverflowBug(EventStack eventStack,
                             long long allocLowerBound, long long allocUpperBound,
                             long long accessLowerBound, long long accessUpperBound):
          BufferOverflowBug(GenericBug::FULLBUFOVERFLOW, eventStack, allocLowerBound,
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
    inline PartialBufferOverflowBug( EventStack eventStack,
                                 long long allocLowerBound, long long allocUpperBound,
                                 long long accessLowerBound, long long accessUpperBound):
          BufferOverflowBug(GenericBug::PARTIALBUFOVERFLOW, eventStack, allocLowerBound,
                            allocUpperBound, accessLowerBound, accessUpperBound){ }

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALBUFOVERFLOW;
    }
};

class NeverFreeBug : public GenericBug{
public:
    NeverFreeBug(EventStack bugEventStack):
          GenericBug(GenericBug::NEVERFREE, bugEventStack){  };

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
    PartialLeakBug(EventStack bugEventStack, Set<std::string> conditionalFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugEventStack), conditionalFreePath(conditionalFreePath){ }

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
    DoubleFreeBug(EventStack bugEventStack, Set<std::string> doubleFreePath):
          GenericBug(GenericBug::PARTIALLEAK, bugEventStack), doubleFreePath(doubleFreePath){ }

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
    FileNeverCloseBug(EventStack bugEventStack):
          GenericBug(GenericBug::NEVERFREE, bugEventStack){  };

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
    FilePartialCloseBug(EventStack bugEventStack, Set<std::string> conditionalFileClosePath):
          GenericBug(GenericBug::PARTIALLEAK, bugEventStack), conditionalFileClosePath(conditionalFileClosePath){ }

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
    typedef SVF::Set<GenericBug *> BugSet;

protected:
    BugSet bugSet;    // maintain bugs

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
        bugSet.insert(newBug);

        // when add a bug, also print it to terminal
        newBug->printBugToTerminal();
    }

    /*
     * function: pass file path, open the file and dump bug report as JSON format
     * usage: dumpToFile("/path/to/file")
     */
    void dumpToFile(const std::string& filePath);
};
}

#endif