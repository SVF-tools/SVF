//===- SVFModule.h -- SVFModule* class-----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * SVFModule.h
 *
 *  Created on: Aug 4, 2017
 *      Author: Xiaokang Fan
 */

#ifndef SVFMODULE_H_
#define SVFMODULE_H_

#include "Util/BasicTypes.h"
#include "Util/ExtAPI.h"
#include "Util/NodeIDAllocator.h"
#include "Util/ThreadAPI.h"

namespace SVF
{

class SVFModule
{
public:
    typedef std::vector<const SVFFunction*> FunctionSetType;
    typedef std::vector<const Function*> LLVMFunctionSetType;
    typedef std::vector<SVFGlobalValue*> GlobalSetType;
    typedef std::vector<SVFGlobalValue*> AliasSetType;
    typedef std::vector<SVFConstantData*> ConstantDataType;
    typedef std::vector<SVFOtherValue*> OtherValueType;

    /// Iterators type def
    typedef FunctionSetType::iterator iterator;
    typedef FunctionSetType::const_iterator const_iterator;
    typedef GlobalSetType::iterator global_iterator;
    typedef GlobalSetType::const_iterator const_global_iterator;
    typedef AliasSetType::iterator alias_iterator;
    typedef AliasSetType::const_iterator const_alias_iterator;
    typedef ConstantDataType::iterator cdata_iterator;
    typedef ConstantDataType::const_iterator const_cdata_iterator;
    typedef OtherValueType::iterator ovalue_iterator;
    typedef OtherValueType::const_iterator const_ovalue_iterator;

private:
    static std::string pagReadFromTxt;
    std::string moduleIdentifier;
    FunctionSetType FunctionSet;  ///< The Functions in the module
    GlobalSetType GlobalSet;      ///< The Global Variables in the module
    AliasSetType AliasSet;        ///< The Aliases in the module
    ConstantDataType ConstantDataSet;        ///< The ConstantData in the module
    OtherValueType  OtherValueSet;   ///< All other values in the module

    Set<const Value*> argsOfUncalledFunction;
    Set<const Value*> blackholeSyms;
    Set<const Value*> ptrsInUncalledFunctions;
    Map<const PointerType*, const Type*> ptrElementTypeMap;

public:
    /// Constructors
    SVFModule(std::string moduleName = "") : moduleIdentifier(moduleName)
    {
    }

    ~SVFModule();

    static inline void setPagFromTXT(std::string txt)
    {
        pagReadFromTxt = txt;
    }

    static inline std::string pagFileName()
    {
        return pagReadFromTxt;
    }

    static inline bool pagReadFromTXT()
    {
        if(pagReadFromTxt.empty())
            return false;
        else
            return true;
    }

    void buildSymbolTableInfo();

    ///@{
    inline void addFunctionSet(SVFFunction* svfFunc)
    {
        FunctionSet.push_back(svfFunc);
    }
    inline void addGlobalSet(SVFGlobalValue* glob)
    {
        GlobalSet.push_back(glob);
    }
    inline void addAliasSet(SVFGlobalValue* alias)
    {
        AliasSet.push_back(alias);
    }
    inline void addConstantData(SVFConstantData* cd)
    {
        ConstantDataSet.push_back(cd);
    }
    inline void addOtherValue(SVFOtherValue* ov)
    {
        OtherValueSet.push_back(ov);
    }
    
    ///@}

    /// Iterators
    ///@{
    iterator begin()
    {
        return FunctionSet.begin();
    }
    const_iterator begin() const
    {
        return FunctionSet.begin();
    }
    iterator end()
    {
        return FunctionSet.end();
    }
    const_iterator end() const
    {
        return FunctionSet.end();
    }

    global_iterator global_begin()
    {
        return GlobalSet.begin();
    }
    const_global_iterator global_begin() const
    {
        return GlobalSet.begin();
    }
    global_iterator global_end()
    {
        return GlobalSet.end();
    }
    const_global_iterator global_end() const
    {
        return GlobalSet.end();
    }

    alias_iterator alias_begin()
    {
        return AliasSet.begin();
    }
    const_alias_iterator alias_begin() const
    {
        return AliasSet.begin();
    }
    alias_iterator alias_end()
    {
        return AliasSet.end();
    }
    const_alias_iterator alias_end() const
    {
        return AliasSet.end();
    }

    cdata_iterator cdata_begin()
    {
        return ConstantDataSet.begin();
    }
    const_cdata_iterator cdata_begin() const
    {
        return ConstantDataSet.begin();
    }
    cdata_iterator cdata_end()
    {
        return ConstantDataSet.end();
    }
    const_cdata_iterator cdata_end() const
    {
        return ConstantDataSet.end();
    }
    ///@}

    const std::string& getModuleIdentifier() const
    {
        if (pagReadFromTxt.empty())
        {
            assert(moduleIdentifier.empty()==false && "No module found! Are you reading from a file other than LLVM-IR?");
            return moduleIdentifier;
        }
        else
        {
            return pagReadFromTxt;
        }
    }

    inline const FunctionSetType& getFunctionSet() const
    {
        return FunctionSet;
    }
    inline const ConstantDataType& getConstantDataSet() const
    {
        return ConstantDataSet;
    }
    inline const GlobalSetType& getGlobalSet() const
    {
        return GlobalSet;
    }
    inline const AliasSetType& getAliasSet() const
    {
        return AliasSet;
    }
    inline const OtherValueType& getOtherValueSet() const
    {
        return OtherValueSet;
    }

    inline const Set<const Value*>& getBlackholeSyms() const
    {
        return blackholeSyms;
    }

    inline const Set<const Value*>& getArgsOfUncalledFunction() const
    {
        return argsOfUncalledFunction;
    }

    inline const Set<const Value*>& getPtrsInUncalledFunctions() const
    {
        return ptrsInUncalledFunctions;
    }

    inline const Map<const PointerType*, const Type*>& getPtrElementTypeMap()
    {
        return ptrElementTypeMap;
    }

    inline void addptrElementType(const PointerType* ptrType, const Type* type)
    {
        ptrElementTypeMap.insert({ptrType, type});
    }

    inline void addPtrInUncalledFunction (const Value*  value)
    {
        ptrsInUncalledFunctions.insert(value);
    }

    inline void addBlackholeSyms(const Value* val)
    {
        blackholeSyms.insert(val);
    }

    inline void addArgsOfUncalledFunction(const Value* val)
    {
        argsOfUncalledFunction.insert(val);
    }

};

} // End namespace SVF

#endif /* SVFMODULE_H_ */
