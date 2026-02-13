//===- AbsExtAPI.h -- Abstract Interpretation External API handler-----//
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


//  Created on: Sep 9, 2024
//      Author: Xiao Cheng, Jiawei Wang
//
#pragma once
#include "AE/Core/AbstractState.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/AEDetector.h"
#include "Util/SVFBugReport.h"

namespace SVF
{

// Forward declaration of AbstractInterpretation class
class AbstractInterpretation;

/**
 * @class AbsExtAPI
 * @brief Handles external API calls and manages abstract states.
 */
class AbsExtAPI
{
public:
    /**
     * @enum ExtAPIType
     * @brief Enumeration of external API types.
     */
    enum ExtAPIType { UNCLASSIFIED, MEMCPY, MEMSET, STRCPY, STRCAT };

    /**
     * @brief Constructor for AbsExtAPI.
     * @param abstractTrace Reference to a map of ICFG nodes to abstract states.
     */
    AbsExtAPI(Map<const ICFGNode*, AbstractState>& traces);

    /**
     * @brief Initializes the external function map.
     */
    void initExtFunMap();

    /**
     * @brief Reads a string from the abstract state.
     * @param as Reference to the abstract state.
     * @param rhs Pointer to the SVF variable representing the string.
     * @return The string value.
     */
    std::string strRead(AbstractState& as, const SVFVar* rhs);

    /**
     * @brief Handles an external API call.
     * @param call Pointer to the call ICFG node.
     */
    void handleExtAPI(const CallICFGNode *call);

    // --- Shared primitives used by string/memory handlers ---

    /// Get the byte size of each element for a pointer/array variable.
    u32_t getElementSize(AbstractState& as, const SVFVar* var);

    /// Check if an interval length is usable (not bottom, not unbounded).
    static bool isValidLength(const IntervalValue& len);

    /// Calculate the length of a null-terminated string in abstract state.
    IntervalValue getStrlen(AbstractState& as, const SVF::SVFVar *strValue);

    // --- String/memory operation handlers ---

    void handleStrcpy(const CallICFGNode *call);
    void handleStrcat(const CallICFGNode *call);
    void handleStrncat(const CallICFGNode *call);
    void handleMemcpy(AbstractState& as, const SVF::SVFVar *dst, const SVF::SVFVar *src, IntervalValue len, u32_t start_idx);
    void handleMemset(AbstractState& as, const SVFVar* dst, IntervalValue elem, IntervalValue len);

    /**
     * @brief Gets the range limit from a type.
     * @param type Pointer to the SVF type.
     * @return The interval value representing the range limit.
     */
    IntervalValue getRangeLimitFromType(const SVFType* type);

    /**
     * @brief Retrieves the abstract state from the trace for a given ICFG node.
     * @param node Pointer to the ICFG node.
     * @return Reference to the abstract state.
     * @throws Assertion if no trace exists for the node.
     */
    AbstractState& getAbsStateFromTrace(const ICFGNode* node);

protected:
    SVFIR* svfir; ///< Pointer to the SVF intermediate representation.
    ICFG* icfg; ///< Pointer to the interprocedural control flow graph.
    Map<const ICFGNode*, AbstractState>& abstractTrace; ///< Map of ICFG nodes to abstract states.
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map; ///< Map of function names to handlers.
};

} // namespace SVF
