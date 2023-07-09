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

#define BRANCHFLAGMASK 0x00000010
#define EVENTTYPEMASK 0x0000000f

namespace SVF
{

/*!
 * Bug Detector Recoder
 */


class SVFBugEvent
{
public:
    enum EventType
    {
        Branch = 0x1,
        Caller = 0x2,
        CallSite = 0x3,
        Loop = 0x4,
        SourceInst = 0x5
    };

protected:
    u32_t typeAndInfoFlag;
    const SVFInstruction *eventInst;

public:
    SVFBugEvent(u32_t typeAndInfoFlag, const SVFInstruction *eventInst): typeAndInfoFlag(typeAndInfoFlag), eventInst(eventInst) { };
    virtual ~SVFBugEvent() = default;

    inline u32_t getEventType() const
    {
        return typeAndInfoFlag & EVENTTYPEMASK;
    }
    virtual const std::string getEventDescription() const;
    virtual const std::string getFuncName() const;
    virtual const std::string getEventLoc() const;
};

class GenericBug
{
public:
    typedef std::vector<SVFBugEvent> EventStack;

public:
    enum BugType {FULLBUFOVERFLOW, PARTIALBUFOVERFLOW, NEVERFREE, PARTIALLEAK, DOUBLEFREE, FILENEVERCLOSE, FILEPARTIALCLOSE, FULLNULLPTRDEREFERENCE, PARTIALNULLPTRDEREFERENCE};
    static const std::map<GenericBug::BugType, std::string> BugType2Str;

protected:
    BugType bugType;
    const EventStack bugEventStack;

public:
    /// note: should be initialized with a bugEventStack
    GenericBug(BugType bugType, const EventStack &bugEventStack):
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

class FullNullPtrDereferenceBug : public GenericBug
{
public:
    FullNullPtrDereferenceBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::FULLNULLPTRDEREFERENCE, bugEventStack) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::FULLNULLPTRDEREFERENCE;
    }
};

class PartialNullPtrDereferenceBug : public GenericBug
{
public:
    PartialNullPtrDereferenceBug(const EventStack &bugEventStack):
        GenericBug(GenericBug::PARTIALNULLPTRDEREFERENCE, bugEventStack) { }

    cJSON *getBugDescription() const;
    void printBugToTerminal() const;

    /// ClassOf
    static inline bool classof(const GenericBug *bug)
    {
        return bug->getBugType() == GenericBug::PARTIALNULLPTRDEREFERENCE;
    }
};

class SVFBugReport
{
public:
    SVFBugReport() = default;
    ~SVFBugReport();
    typedef SVF::Set<const GenericBug *> BugSet;

protected:
    BugSet bugSet;    // maintain bugs

public:

    /*
     * function: pass bug type (i.e., GenericBug::NEVERFREE) and eventStack as parameter,
     *      it will add the bug into bugQueue.
     * usage: addSaberBug(GenericBug::NEVERFREE, eventStack)
     */
    void addSaberBug(GenericBug::BugType bugType, const GenericBug::EventStack &eventStack)
    {
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
        case GenericBug::FULLNULLPTRDEREFERENCE:
        {
            newBug = new FullNullPtrDereferenceBug(eventStack);
            bugSet.insert(newBug);
            break;
        }
        case GenericBug::PARTIALNULLPTRDEREFERENCE:
        {
            newBug = new PartialNullPtrDereferenceBug(eventStack);
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
    void dumpToJsonFile(const std::string& filePath) const;

    /*
     * function: get underlying bugset
     * usage: getBugSet()
     */
    const BugSet &getBugSet() const
    {
        return bugSet;
    }

};
}

#endif