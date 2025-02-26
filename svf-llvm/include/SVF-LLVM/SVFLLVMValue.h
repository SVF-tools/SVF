//===- BasicTypes.h -- Basic types used in SVF-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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

/*
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFLLVM_SVFVALUE_H_
#define INCLUDE_SVFLLVM_SVFVALUE_H_

#include "SVFIR/SVFType.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"
#include "Graphs/BasicBlockG.h"
#include "SVFIR/SVFValue.h"
#include "Util/SVFLoopAndDomInfo.h"

namespace SVF
{

/// LLVM Aliases and constants
typedef SVF::GraphPrinter GraphPrinter;

class CallGraphNode;
class SVFBasicBlock;
class SVFType;

class SVFLLVMValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class LLVMModuleSet;

public:
    typedef s64_t GNodeK;

    enum SVFValKind
    {
        SVFVal,
        SVFFunc,
    };

private:
    GNodeK kind;	///< used for classof
    bool ptrInUncalledFun;  ///< true if this pointer is in an uncalled function
    bool constDataOrAggData;    ///< true if this value is a ConstantData (e.g., numbers, string, floats) or a constantAggregate

protected:
    const SVFType* type;   ///< Type of this SVFValue
    std::string name;       ///< Short name of value for printing & debugging
    std::string sourceLoc;  ///< Source code information of this value
    /// Constructor without name
    SVFLLVMValue(const SVFType* ty, SVFValKind k = SVFVal)
        : kind(k), ptrInUncalledFun(false),
          constDataOrAggData(false), type(ty), sourceLoc("NoLoc")
    {
    }

    ///@{ attributes to be set only through Module builders e.g.,
    /// LLVMModule
    inline void setConstDataOrAggData()
    {
        constDataOrAggData = true;
    }
    inline void setPtrInUncalledFunction()
    {
        ptrInUncalledFun = true;
    }
    ///@}
public:
    SVFLLVMValue() = delete;
    virtual ~SVFLLVMValue() = default;

    /// Get the type of this SVFValue
    inline GNodeK getKind() const
    {
        return kind;
    }

    inline const std::string &getName() const
    {
        return name;
    }

    inline void setName(std::string&& n)
    {
        name = std::move(n);
    }

    inline virtual const SVFType* getType() const
    {
        return type;
    }
    inline bool isConstDataOrAggData() const
    {
        return constDataOrAggData;
    }
    inline bool ptrInUncalledFunction() const
    {
        return ptrInUncalledFun;
    }
    inline virtual void setSourceLoc(const std::string& sourceCodeInfo)
    {
        sourceLoc = sourceCodeInfo;
    }
    inline virtual const std::string getSourceLoc() const
    {
        return sourceLoc;
    }

    /// Needs to be implemented by a SVF front end
    std::string toString() const;

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<<(OutStream &os, const SVFLLVMValue &value)
    {
        return os << value.toString();
    }
    //@}
};


/// [FOR DEBUG ONLY, DON'T USE IT UNSIDE `svf`!]
/// Converts an SVFValue to corresponding LLVM::Value, then get the string
/// representation of it. Use it only when you are debugging. Don't use
/// it in any SVF algorithm because it relies on information stored in LLVM bc.
std::string dumpLLVMValue(const SVFLLVMValue* svfValue);



} // End namespace SVF

#endif /* INCLUDE_SVFLLVM_SVFVALUE_H_ */
