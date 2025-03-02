//===- SymbolTableBuilder.h -- Symbol Table builder-----------------------//
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

/*
 * SymbolTableBuilder.h
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei
 */

#ifndef SymbolTableBuilder_H_
#define SymbolTableBuilder_H_

#include "SVF-LLVM/LLVMModule.h"
#include "SVFIR/ObjTypeInfo.h"

/*
* This class is to build SymbolTableInfo, MemObjs and ObjTypeInfo
*/
namespace SVF
{

class ObjTypeInference;

class SymbolTableBuilder
{
    friend class SVFIRBuilder;
private:
    SVFIR* svfir;

public:
    /// Constructor
    SymbolTableBuilder(SVFIR* ir): svfir(ir)
    {
    }

    /// Start building memory model
    void buildMemModel();

    /// Return size of this object based on LLVM value
    u32_t getNumOfElements(const Type* ety);


protected:

    /// collect the syms
    //@{
    void collectSVFTypeInfo(const Value* val);

    void collectSym(const Value* val);

    void collectVal(const Value* val);

    void collectObj(const Value* val);

    void collectRet(const Function* val);

    void collectVararg(const Function* val);
    //@}

    /// Handle constant expression
    // @{
    void handleGlobalCE(const GlobalVariable *G);
    void handleGlobalInitializerCE(const Constant *C);
    void handleCE(const Value* val);
    // @}

    inline LLVMModuleSet* llvmModuleSet()
    {
        return LLVMModuleSet::getLLVMModuleSet();
    }

    ObjTypeInference* getTypeInference();

    /// Forward collect all possible infer sites starting from a value
    const Type* inferObjType(const Value *startValue);

    /// Get the reference type of heap/static object from an allocation site.
    //@{
    const Type *inferTypeOfHeapObjOrStaticObj(const Instruction* inst);
    //@}


    /// Create an objectInfo based on LLVM value
    ObjTypeInfo* createObjTypeInfo(const Value* val);

    /// Initialize TypeInfo based on LLVM Value
    void initTypeInfo(ObjTypeInfo* typeinfo, const Value* value, const Type* ty);
    /// Analyse types of all flattened fields of this object
    void analyzeObjType(ObjTypeInfo* typeinfo, const Value* val);
    /// Analyse types of heap and static objects
    u32_t analyzeHeapObjType(ObjTypeInfo* typeinfo, const Value* val);
    /// Analyse types of heap and static objects
    void analyzeStaticObjType(ObjTypeInfo* typeinfo, const Value* val);

    /// Analyze byte size of heap alloc function (e.g. malloc/calloc/...)
    u32_t analyzeHeapAllocByteSize(const Value* val);

    ///Get a reference to the components of struct_info.
    /// Number of flattened elements of an array or struct
    u32_t getNumOfFlattenElements(const Type* T);

    ///Get a reference to StructInfo.
    StInfo* getOrAddSVFTypeInfo(const Type* T);

    ObjTypeInfo* createBlkObjTypeInfo(NodeID symId);

    ObjTypeInfo* createConstantObjTypeInfo(NodeID symId);
};

} // End namespace SVF

#endif /* SymbolTableBuilder_H_ */
