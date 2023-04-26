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

namespace SVF
{

/*!
 * Bug Detector Recoder
 */


class GenericEvent
{
public:
    enum EventType {Branch, Caller, CallSite, Loop, SourceInst};

protected:
    EventType eventType;

public:
    GenericEvent(EventType eventType): eventType(eventType) { };
    virtual ~GenericEvent() = default;

    inline EventType getEventType() const
    {
        return eventType;
    }
    virtual const std::string getEventDescription() const = 0;
    virtual const std::string getFuncName() const = 0;
    virtual const std::string getEventLoc() const = 0;
};

class BranchEvent: public GenericEvent
{
    /// branch statement and branch condition true or false
protected:
    const SVFInstruction *branchInst;
    bool branchSuccessFlg;

public:
    BranchEvent(const SVFInstruction *branchInst, bool branchSuccessFlg):
        GenericEvent(GenericEvent::Branch), branchInst(branchInst), branchSuccessFlg(branchSuccessFlg) { }

    const std::string getEventDescription() const;
    const std::string getFuncName() const;
    const std::string getEventLoc() const;

    /// ClassOf
    static inline bool classof(const GenericEvent* event)
    {
        return event->getEventType() == GenericEvent::Branch;
    }
};

class CallSiteEvent: public GenericEvent
{
protected:
    const CallICFGNode *callSite;

public:
    CallSiteEvent(const CallICFGNode *callSite): GenericEvent(GenericEvent::CallSite), callSite(callSite) { }
    const std::string getEventDescription() const;
    const std::string getFuncName() const;
    const std::string getEventLoc() const;

    /// ClassOf
    static inline bool classof(const GenericEvent* event)
    {
        return event->getEventType() == GenericEvent::CallSite;
    }
};

class SourceInstEvent : public GenericEvent
{
protected:
    const SVFInstruction *sourceSVFInst;

public:
    SourceInstEvent(const SVFInstruction *sourceSVFInst):
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

class GenericBug
{
public:
    typedef std::vector<const GenericEvent *> EventStack;

public:
    enum BugType {FULLBUFOVERFLOW, PARTIALBUFOVERFLOW, NEVERFREE, PARTIALLEAK, DOUBLEFREE, FILENEVERCLOSE, FILEPARTIALCLOSE};

protected:
    BugType bugType;
    EventStack bugEventStack;

public:
    /// note: should be initialized with a bugEventStack
    GenericBug(BugType bugType, EventStack bugEventStack):
        bugType(bugType), bugEventStack(bugEventStack)
    {
        assert(bugEventStack.size() != 0 && "bugEventStack should NOT be empty!");
    }
    virtual ~GenericBug() = default;
    //GenericBug(const GenericBug &) = delete;
    /// returns bug type
    inline BugType getBugType() const
    {
        return bugType;
    }
    /// returns bug location as json format string
    const std::string getLoc() const;
    /// return bug source function name
    const std::string getFuncName() const;

    inline const EventStack& getEventStack() const
    {
        return bugEventStack;
    }

    virtual cJSON *getBugDescription() const = 0;
    virtual void printBugToTerminal() const = 0;
};

class BufferOverflowBug: public GenericBug
{
protected:
    s64_t allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound;

public:
    BufferOverflowBug(GenericBug::BugType bugType, const EventStack &eventStack,
                      s64_t allocLowerBound, s64_t allocUpperBound,
                      s64_t accessLowerBound, s64_t accessUpperBound):
        GenericBug(bugType, eventStack), allocLowerBound(allocLowerBound),
        allocUpperBound(allocUpperBound), accessLowerBound(accessLowerBound),
        accessUpperBound(accessUpperBound) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALBUFOVERFLOW || bug->getBugType() == GenericBug::FULLBUFOVERFLOW;
    }
};

class FullBufferOverflowBug: public BufferOverflowBug
{
public:
    FullBufferOverflowBug(const EventStack &eventStack,
                          s64_t allocLowerBound, s64_t allocUpperBound,
                          s64_t accessLowerBound, s64_t accessUpperBound):
        BufferOverflowBug(GenericBug::FULLBUFOVERFLOW, eventStack, allocLowerBound,
                          allocUpperBound, accessLowerBound, accessUpperBound) { }

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FULLBUFOVERFLOW;
    }
};

class PartialBufferOverflowBug: public BufferOverflowBug
{
public:
    PartialBufferOverflowBug( const EventStack &eventStack,
                              s64_t allocLowerBound, s64_t allocUpperBound,
                              s64_t accessLowerBound, s64_t accessUpperBound):
        BufferOverflowBug(GenericBug::PARTIALBUFOVERFLOW, eventStack, allocLowerBound,
                          allocUpperBound, accessLowerBound, accessUpperBound) { }

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALBUFOVERFLOW;
    }
};

class NeverFreeBug : public GenericBug
{
public:
    NeverFreeBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::NEVERFREE, bugEventStack) {  };

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::NEVERFREE;
    }
};

class PartialLeakBug : public GenericBug
{
public:
    PartialLeakBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::PARTIALLEAK, bugEventStack) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALLEAK;
    }
};

class DoubleFreeBug : public GenericBug
{
public:
    DoubleFreeBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::PARTIALLEAK, bugEventStack) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::DOUBLEFREE;
    }
};

class FileNeverCloseBug : public GenericBug
{
public:
    FileNeverCloseBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::NEVERFREE, bugEventStack) {  };

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FILENEVERCLOSE;
    }
};

class FilePartialCloseBug : public GenericBug
{
public:
    FilePartialCloseBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::PARTIALLEAK, bugEventStack) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

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
    typedef SVF::Set<const GenericBug *> BugSet;
    typedef SVF::Set<const GenericEvent *> EventSet;

protected:
    BugSet bugSet;    // maintain bugs
    EventSet eventSet;// maintain added events

public:
    /*
     * function: pass bug type (i.e., GenericBug::NEVERFREE) and eventStack as parameter,
     *      it will add the bug into bugQueue.
     * usage: addSaberBug(GenericBug::NEVERFREE, eventStack)
     */
    void addSaberBug(GenericBug::BugType bugType, const GenericBug::EventStack &eventStack)
    {
        /// resign added events
        for(auto eventPtr : eventStack)
        {
            eventSet.insert(eventPtr);
        }

        /// create and add the bug
        GenericBug *newBug = nullptr;
        switch(bugType)
        {
        case GenericBug::NEVERFREE:
        {
            newBug = new NeverFreeBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::PARTIALLEAK:
        {
            newBug = new PartialLeakBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::DOUBLEFREE:
        {
            newBug = new DoubleFreeBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::FILENEVERCLOSE:
        {
            newBug = new FileNeverCloseBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::FILEPARTIALCLOSE:
        {
            newBug = new FilePartialCloseBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        default:
        {
            assert(false && "saber does NOT have this bug type!");
            break;
        }
        }

        // when add a bug, also print it to terminal
        newBug->printBugToTerminal();
    }

    /*
     * function: pass bug type (i.e., GenericBug::FULLBUFOVERFLOW) and eventStack as parameter,
     *      it will add the bug into bugQueue.
     * usage: addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, 0, 10, 11, 11)
     */
    void addAbsExecBug(GenericBug::BugType bugType, const GenericBug::EventStack &eventStack,
                       s64_t allocLowerBound, s64_t allocUpperBound, s64_t accessLowerBound, s64_t accessUpperBound)
    {
        /// resign added events
        for(auto eventPtr : eventStack)
        {
            eventSet.insert(eventPtr);
        }

        /// add bugs
        GenericBug *newBug = nullptr;
        switch(bugType)
        {
        case GenericBug::FULLBUFOVERFLOW:
        {
            newBug = new FullBufferOverflowBug(eventStack, allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::PARTIALBUFOVERFLOW:
        {
            newBug = new PartialBufferOverflowBug(eventStack, allocLowerBound, allocUpperBound, accessLowerBound, accessUpperBound);
            bugSet.insert(newBug);
            break;
        }
        default:
        {
            assert(false && "Abstract Execution does NOT hava his bug type!");
            break;
        }
        }

        // when add a bug, also print it to terminal
        newBug->printBugToTerminal();
    }

    /*
     * function: pass file path, open the file and dump bug report as JSON format
     * usage: dumpToFile("/path/to/file")
     */
    void dumpToJsonFile(const std::string& filePath);
};
}

#endif