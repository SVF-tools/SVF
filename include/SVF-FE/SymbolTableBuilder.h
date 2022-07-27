//===- SymbolTableBuilder.h -- Symbol Table builder-----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SymbolTableBuilder.h
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei
 */

#ifndef SymbolTableBuilder_H_
#define SymbolTableBuilder_H_

#include "SVF-FE/LLVMModule.h"
#include "MemoryModel/SymbolTableInfo.h"

/*
* This class is to build SymbolTableInfo, MemObjs and ObjTypeInfo
*/
namespace SVF
{

class SymbolTableBuilder
{

private:
    SymbolTableInfo* symInfo;

public:
    /// Constructor
    SymbolTableBuilder(SymbolTableInfo* si): symInfo(si)
    {
    }

    /// Start building memory model
    void buildMemModel(SVFModule* svfModule);

    /// collect the syms
    //@{
    void collectNullPtrBlackholeSyms(const Value *val);

    void collectSym(const Value *val);

    void collectVal(const Value *val);

    void collectObj(const Value *val);

    void collectRet(const Function *val);

    void collectVararg(const Function *val);
    //@}

    /// Handle constant expression
    // @{
    void handleGlobalCE(const GlobalVariable *G);
    void handleGlobalInitializerCE(const Constant *C);
    void handleCE(const Value *val);
    // @}

    /// Create an objectInfo based on LLVM value
    ObjTypeInfo* createObjTypeInfo(const Value *val);

    /// Initialize TypeInfo based on LLVM Value
    void initTypeInfo(ObjTypeInfo* typeinfo, const Value* value);
    /// Analyse types of all flattened fields of this object
    void analyzeObjType(ObjTypeInfo* typeinfo, const Value* val);
    /// Analyse types of heap and static objects
    void analyzeHeapObjType(ObjTypeInfo* typeinfo, const Value* val);
    /// Analyse types of heap and static objects
    void analyzeStaticObjType(ObjTypeInfo* typeinfo, const Value* val);

    /// Return size of this object based on LLVM value
    u32_t getObjSize(const Type* type);
};

}

#endif /* SymbolTableBuilder_H_ */
