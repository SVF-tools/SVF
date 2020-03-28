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

class SVFModule {
public:
    typedef std::vector<Function*> FunctionSetType;
    typedef std::vector<GlobalVariable*> GlobalSetType;
    typedef std::vector<GlobalAlias*> AliasSetType;


    /// Iterators type def
    typedef FunctionSetType::iterator iterator;
    typedef FunctionSetType::const_iterator const_iterator;
    typedef GlobalSetType::iterator global_iterator;
    typedef GlobalSetType::const_iterator const_global_iterator;
    typedef AliasSetType::iterator alias_iterator;
    typedef AliasSetType::const_iterator const_alias_iterator;

private:
    static std::string pagReadFromTxt;
    std::string moduleIdentifier;
    FunctionSetType FunctionSet;  ///< The Functions in the module
    GlobalSetType GlobalSet;      ///< The Global Variables in the module
    AliasSetType AliasSet;        ///< The Aliases in the module

public:
    /// Constructors
    SVFModule(std::string moduleName = "") : moduleIdentifier(moduleName) {
    }


    static inline void setPagFromTXT(std::string txt) {
        pagReadFromTxt = txt;
    }

    static inline std::string pagFileName() {
        return pagReadFromTxt;
    }

    static inline bool pagReadFromTXT() {
    		if(pagReadFromTxt.empty())
    			return false;
    		else
    			return true;
    }

    ///@{
    inline void addFunctionSet(Function* fun){
    	FunctionSet.push_back(fun);
    }
    inline void addGlobalSet(GlobalVariable* glob){
    	GlobalSet.push_back(glob);
    }
    inline void addAliasSet(GlobalAlias* alias){
    	AliasSet.push_back(alias);
    }
    ///@}

    /// Iterators
    ///@{
    iterator begin() {
        return FunctionSet.begin();
    }
    const_iterator begin() const {
        return FunctionSet.begin();
    }
    iterator end() {
        return FunctionSet.end();
    }
    const_iterator end() const {
        return FunctionSet.end();
    }

    global_iterator global_begin() {
        return GlobalSet.begin();
    }
    const_global_iterator global_begin() const {
        return GlobalSet.begin();
    }
    global_iterator global_end() {
        return GlobalSet.end();
    }
    const_global_iterator global_end() const {
        return GlobalSet.end();
    }

    alias_iterator alias_begin() {
        return AliasSet.begin();
    }
    const_alias_iterator alias_begin() const {
        return AliasSet.begin();
    }
    alias_iterator alias_end() {
        return AliasSet.end();
    }
    const_alias_iterator alias_end() const {
        return AliasSet.end();
    }
    ///@}

	const std::string& getModuleIdentifier() const {
		if (pagReadFromTxt.empty()) {
			assert(moduleIdentifier.empty()==false && "No LLVM module found! Are you reading from a file other than LLVM-IR?");
			return moduleIdentifier;
		} else {
			return pagReadFromTxt;
		}
	}

};


#endif /* SVFMODULE_H_ */
