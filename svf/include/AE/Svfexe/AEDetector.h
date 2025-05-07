//===- AEDetector.h -- Vulnerability Detectors---------------------------------//
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
// Created by Jiawei Wang on 2024/8/20.
//
#pragma once
#include <SVFIR/SVFIR.h>
#include <AE/Core/AbstractState.h>
#include "Util/SVFBugReport.h"

namespace SVF
{
/**
 * @class AEDetector
 * @brief Base class for all detectors.
 */
class AEDetector
{
public:
    /**
     * @enum DetectorKind
     * @brief Enumerates the types of detectors available.
     */
    enum DetectorKind
    {
        BUF_OVERFLOW,   ///< Detector for buffer overflow issues.
        NULL_DEREF,  ///< Detector for nullptr dereference issues.
        UNKNOWN,    ///< Default type if the kind is not specified.
    };

    /**
     * @brief Constructor initializes the detector kind to UNKNOWN.
     */
    AEDetector(): kind(UNKNOWN) {}

    /**
     * @brief Virtual destructor for safe polymorphic use.
     */
    virtual ~AEDetector() = default;

    /**
     * @brief Check if the detector is of the UNKNOWN kind.
     * @param detector Pointer to the detector.
     * @return True if the detector is of type UNKNOWN, false otherwise.
     */
    static bool classof(const AEDetector* detector)
    {
        return detector->getKind() == AEDetector::UNKNOWN;
    }

    /**
     * @brief Pure virtual function for detecting issues within a node.
     * @param as Reference to the abstract state.
     * @param node Pointer to the ICFG node.
     */
    virtual void detect(AbstractState& as, const ICFGNode* node) = 0;

    /**
     * @brief Pure virtual function for handling stub external API calls. (e.g. UNSAFE_BUFACCESS)
     * @param call Pointer to the ext call ICFG node.
     */
    virtual void handleStubFunctions(const CallICFGNode* call) = 0;

    /**
     * @brief Pure virtual function to report detected bugs.
     */
    virtual void reportBug() = 0;

    /**
     * @brief Get the kind of the detector.
     * @return The kind of the detector.
     */
    DetectorKind getKind() const
    {
        return kind;
    }

protected:
    DetectorKind kind; ///< The kind of the detector.
};

/**
 * @class AEException
 * @brief Exception class for handling errors in Abstract Execution.
 */
class AEException : public std::exception
{
public:
    /**
     * @brief Constructor initializes the exception with a message.
     * @param message The error message.
     */
    AEException(const std::string& message)
        : msg_(message) {}

    /**
     * @brief Provides the error message.
     * @return The error message as a C-string.
     */
    virtual const char* what() const throw()
    {
        return msg_.c_str();
    }

private:
    std::string msg_; ///< The error message.
};

/**
 * @class BufOverflowDetector
 * @brief Detector for identifying buffer overflow issues.
 */
class BufOverflowDetector : public AEDetector
{
    friend class AbstractInterpretation;
public:
    /**
     * @brief Constructor initializes the detector kind to BUF_OVERFLOW and sets up external API buffer overflow rules.
     */
    BufOverflowDetector()
    {
        kind = BUF_OVERFLOW;
        initExtAPIBufOverflowCheckRules();
    }

    /**
     * @brief Destructor.
     */
    ~BufOverflowDetector() = default;

    /**
     * @brief Check if the detector is of the BUF_OVERFLOW kind.
     * @param detector Pointer to the detector.
     * @return True if the detector is of type BUF_OVERFLOW, false otherwise.
     */
    static bool classof(const AEDetector* detector)
    {
        return detector->getKind() == AEDetector::BUF_OVERFLOW;
    }

    /**
     * @brief Updates the offset of a GEP object from its base.
     * @param as Reference to the abstract state.
     * @param gepAddrs Address value for GEP.
     * @param objAddrs Address value for the object.
     * @param offset The interval value of the offset.
     */
    void updateGepObjOffsetFromBase(AbstractState& as,
                                    AddressValue gepAddrs,
                                    AddressValue objAddrs,
                                    IntervalValue offset);

    /**
     * @brief Detect buffer overflow issues within a node.
     * @param as Reference to the abstract state.
     * @param node Pointer to the ICFG node.
     */
    void detect(AbstractState& as, const ICFGNode*);


    /**
     * @brief Handles external API calls related to buffer overflow detection.
     * @param call Pointer to the call ICFG node.
     */
    void handleStubFunctions(const CallICFGNode*);

    /**
     * @brief Adds an offset to a GEP object.
     * @param obj Pointer to the GEP object.
     * @param offset The interval value of the offset.
     */
    void addToGepObjOffsetFromBase(const GepObjVar* obj, const IntervalValue& offset)
    {
        gepObjOffsetFromBase[obj] = offset;
    }

    /**
     * @brief Checks if a GEP object has an associated offset.
     * @param obj Pointer to the GEP object.
     * @return True if the GEP object has an offset, false otherwise.
     */
    bool hasGepObjOffsetFromBase(const GepObjVar* obj) const
    {
        return gepObjOffsetFromBase.find(obj) != gepObjOffsetFromBase.end();
    }

    /**
     * @brief Retrieves the offset of a GEP object from its base.
     * @param obj Pointer to the GEP object.
     * @return The interval value of the offset.
     */
    IntervalValue getGepObjOffsetFromBase(const GepObjVar* obj) const
    {
        if (hasGepObjOffsetFromBase(obj))
            return gepObjOffsetFromBase.at(obj);
        else
            assert(false && "GepObjVar not found in gepObjOffsetFromBase");
    }

    /**
     * @brief Retrieves the access offset for a given object and GEP statement.
     * @param as Reference to the abstract state.
     * @param objId The ID of the object.
     * @param gep Pointer to the GEP statement.
     * @return The interval value of the access offset.
     */
    IntervalValue getAccessOffset(AbstractState& as, NodeID objId, const GepStmt* gep);

    /**
     * @brief Adds a bug to the reporter based on an exception.
     * @param e The exception that was thrown.
     * @param node Pointer to the ICFG node where the bug was detected.
     */
    void addBugToReporter(const AEException& e, const ICFGNode* node)
    {

        GenericBug::EventStack eventStack;
        SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, node);
        eventStack.push_back(sourceInstEvent); // Add the source instruction event to the event stack

        if (eventStack.empty())
        {
            return; // If the event stack is empty, return early
        }

        std::string loc = eventStack.back().getEventLoc(); // Get the location of the last event in the stack

        // Check if the bug at this location has already been reported
        if (bugLoc.find(loc) != bugLoc.end())
        {
            return; // If the bug location is already reported, return early
        }
        else
        {
            bugLoc.insert(loc); // Otherwise, mark this location as reported
        }

        // Add the bug to the recorder with details from the event stack
        recoder.addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, 0, 0, 0, 0);
        nodeToBugInfo[node] = e.what(); // Record the exception information for the node
    }

    /**
     * @brief Reports all detected buffer overflow bugs.
     */
    void reportBug()
    {
        if (!nodeToBugInfo.empty())
        {
            std::cerr << "######################Buffer Overflow (" + std::to_string(nodeToBugInfo.size())
                      + " found)######################\n";
            std::cerr << "---------------------------------------------\n";
            for (const auto& it : nodeToBugInfo)
            {
                std::cerr << it.second << "\n---------------------------------------------\n";
            }
        }
    }

    /**
     * @brief Initializes external API buffer overflow check rules.
     */
    void initExtAPIBufOverflowCheckRules();

    /**
     * @brief Handles external API calls related to buffer overflow detection.
     * @param as Reference to the abstract state.
     * @param call Pointer to the call ICFG node.
     */
    void detectExtAPI(AbstractState& as, const CallICFGNode *call);

    /**
     * @brief Checks if memory can be safely accessed.
     * @param as Reference to the abstract state.
     * @param value Pointer to the SVF var.
     * @param len The interval value representing the length of the memory access.
     * @return True if the memory access is safe, false otherwise.
     */
    bool canSafelyAccessMemory(AbstractState& as, const SVFVar *value, const IntervalValue &len);

private:
    /**
     * @brief Detects buffer overflow in 'strcat' function calls.
     * @param as Reference to the abstract state.
     * @param call Pointer to the call ICFG node.
     * @return True if a buffer overflow is detected, false otherwise.
     */
    bool detectStrcat(AbstractState& as, const CallICFGNode *call);

    /**
     * @brief Detects buffer overflow in 'strcpy' function calls.
     * @param as Reference to the abstract state.
     * @param call Pointer to the call ICFG node.
     * @return True if a buffer overflow is detected, false otherwise.
     */
    bool detectStrcpy(AbstractState& as, const CallICFGNode *call);

private:
    Map<const GepObjVar*, IntervalValue> gepObjOffsetFromBase; ///< Maps GEP objects to their offsets from the base.
    Map<std::string, std::vector<std::pair<u32_t, u32_t>>> extAPIBufOverflowCheckRules; ///< Rules for checking buffer overflows in external APIs.
    Set<std::string> bugLoc; ///< Set of locations where bugs have been reported.
    SVFBugReport recoder; ///< Recorder for abstract execution bugs.
    Map<const ICFGNode*, std::string> nodeToBugInfo; ///< Maps ICFG nodes to bug information.
};
class NullptrDerefDetector : public AEDetector
{
    friend class AbstractInterpretation;
public:
    NullptrDerefDetector()
    {
        kind = NULL_DEREF;
    }

    ~NullptrDerefDetector() = default;

    static bool classof(const AEDetector* detector)
    {
        return detector->getKind() == AEDetector::NULL_DEREF;
    }

    /**
     * @brief Detects nullptr dereferences issues within a node.
     * @param as Reference to the abstract state.
     * @param node Pointer to the ICFG node.
     */
    void detect(AbstractState& as, const ICFGNode* node);

    /**
     * @brief Handles external API calls related to nullptr dereferences.
     * @param call Pointer to the call ICFG node.
     */
    void handleStubFunctions(const CallICFGNode* call);

    /**
     * @brief Checks if an Abstract Value is uninitialized.
     * @param v The Abstract Value to check.
     * @return True if the value is uninitialized, false otherwise.
     */
    bool isUninit(AbstractValue v)
    {
        // uninitialized value has neither interval value nor address value
        bool is = v.getAddrs().isBottom() && v.getInterval().isBottom();
        return is;
    }

    /**
    * @brief Adds a bug to the reporter based on an exception.
    * @param e The exception that was thrown.
    * @param node Pointer to the ICFG node where the bug was detected.
    */
    void addBugToReporter(const AEException& e, const ICFGNode* node)
    {
        GenericBug::EventStack eventStack;
        SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, node);
        eventStack.push_back(sourceInstEvent); // Add the source instruction event to the event stack

        if (eventStack.empty())
        {
            return; // If the event stack is empty, return early
        }
        std::string loc = eventStack.back().getEventLoc(); // Get the location of the last event in the stack

        // Check if the bug at this location has already been reported
        if (bugLoc.find(loc) != bugLoc.end())
        {
            return; // If the bug location is already reported, return early
        }
        else
        {
            bugLoc.insert(loc); // Otherwise, mark this location as reported
        }
        recoder.addAbsExecBug(GenericBug::FULLNULLPTRDEREFERENCE, eventStack, 0, 0, 0, 0);
        nodeToBugInfo[node] = e.what(); // Record the exception information for the node
    }

    /**
     * @brief Reports all detected nullptr dereference bugs.
     */
    void reportBug()
    {
        if (!nodeToBugInfo.empty())
        {
            std::cerr << "###################### Nullptr Dereference (" + std::to_string(nodeToBugInfo.size())
                      + " found)######################\n";
            std::cerr << "---------------------------------------------\n";
            for (const auto& it : nodeToBugInfo)
            {
                std::cerr << it.second << "\n---------------------------------------------\n";
            }
        }
    }

    /**
     * @brief Handle external API calls related to nullptr dereferences.
     * @param as Reference to the abstract state.
     * @param call Pointer to the call ICFG node.
     */
    void detectExtAPI(AbstractState& as, const CallICFGNode* call);


    /**
     * @brief Check if an Abstract Value is NULL (or uninitialized).
     *
     * @param v An Abstract Value of loaded from an address in an Abstract State.
     */
    bool isNull(AbstractValue v)
    {
        return !v.isAddr() && !v.isInterval();
    }

    bool canSafelyDerefPtr(AbstractState& as, const SVFVar* ptr);

private:
    Set<std::string> bugLoc; ///< Set of locations where bugs have been reported.
    SVFBugReport recoder; ///< Recorder for abstract execution bugs.
    Map<const ICFGNode*, std::string> nodeToBugInfo; ///< Maps ICFG nodes to bug information.
};
}
