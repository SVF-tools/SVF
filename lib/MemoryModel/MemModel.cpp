//===- MemModel.cpp -- Memory model for pointer analysis----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * MemModel.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/MemModel.h"
#include "MemoryModel/LocMemModel.h"
#include "Util/AnalysisUtil.h"
#include "Util/CPPUtil.h"
#include "Util/BreakConstantExpr.h"
#include "Util/GEPTypeBridgeIterator.h" // include bridge_gep_iterator 
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Support/raw_ostream.h>	// for output
#include <llvm/IR/ValueSymbolTable.h>	// for valueSymbolTable
#include <llvm/IR/InstIterator.h>	// for inst iteration
#include <llvm/IR/CallSite.h>		//callsite
#include <llvm/Support/CommandLine.h> // for tool output file
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator

using namespace llvm;
using namespace std;
using namespace analysisUtil;

llvm::DataLayout* SymbolTableInfo::dl = NULL;
SymbolTableInfo* SymbolTableInfo::symlnfo = NULL;
u32_t SymbolTableInfo::maxFieldLimit = 0;
SymID SymbolTableInfo::totalSymNum = 0;
llvm::Module* SymbolTableInfo::mod = NULL;

static cl::opt<unsigned> maxFieldNumLimit("fieldlimit",  cl::init(10000),
        cl::desc("Maximum field number for field sensitive analysis"));

static cl::opt<bool> LocMemModel("locMM", cl::init(false),
                                 cl::desc("Bytes/bits modeling of memory locations"));

static cl::opt<bool> modelConsts("modelConsts", cl::init(false),
                                 cl::desc("Modeling individual constant objects"));

/*!
 * Get the symbol table instance
 */
SymbolTableInfo* SymbolTableInfo::Symbolnfo() {
    if (symlnfo == NULL) {
        if(LocMemModel)
            symlnfo = new LocSymTableInfo();
        else
            symlnfo = new SymbolTableInfo();
        symlnfo->setModelConstants(modelConsts);
    }
    return symlnfo;
}

/*!
 * Collect a LLVM type info
 */
void SymbolTableInfo::collectTypeInfo(const llvm::Type* ty) {
    assert(typeToFieldInfo.find_as(ty) == typeToFieldInfo.end() && "this type has been collected before");

    if (const ArrayType* aty = dyn_cast<ArrayType>(ty))
        collectArrayInfo(aty);
    else if (const StructType* sty = dyn_cast<StructType>(ty))
        collectStructInfo(sty);
    else
        collectSimpleTypeInfo(ty);
}


/*!
 * Fill in StInfo for an array type.
 */
void SymbolTableInfo::collectArrayInfo(const ArrayType* ty) {
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    /// Array itself only has one field which is the inner most element
    stinfo->getFieldOffsetVec().push_back(0);

    llvm::Type* elemTy = ty->getElementType();
    while (const ArrayType* aty = dyn_cast<ArrayType>(elemTy))
        elemTy = aty->getElementType();

    /// Array's flatten field infor is the same as its element's
    /// flatten infor.
    StInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getFlattenFieldInfoVec().size();
    for (u32_t j = 0; j < nfE; j++) {
        u32_t off = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
        const Type* fieldTy = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
        FieldInfo::ElemNumStridePairVec pair = elemStInfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
        /// append the additional number
        pair.push_back(std::make_pair(1, 0));
        FieldInfo field(off, fieldTy, pair);
        stinfo->getFlattenFieldInfoVec().push_back(field);
    }
}


/*!
 * Fill in struct_info for T.
 * Given a Struct type, we recursively extend and record its fields and types.
 */
void SymbolTableInfo::collectStructInfo(const StructType *sty) {
    /// The struct info should not be processed before
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[sty] = stinfo;

    // Number of fields have been placed in the expanded struct
    u32_t nf = 0;

    for (StructType::element_iterator it = sty->element_begin(), ie =
                sty->element_end(); it != ie; ++it) {
        //The offset is where this element will be placed in the exp. struct.
        stinfo->getFieldOffsetVec().push_back(nf);

        const Type *et = *it;
        if (isa<StructType>(et) || isa<ArrayType>(et)) {
            StInfo * subStinfo = getStructInfo(et);
            u32_t nfE = subStinfo->getFlattenFieldInfoVec().size();
            //Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfE; j++) {
                u32_t off = nf + subStinfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
                const Type* elemTy = subStinfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
                FieldInfo::ElemNumStridePairVec pair = subStinfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
                FieldInfo field(off,elemTy,pair);
                stinfo->getFlattenFieldInfoVec().push_back(field);
            }
            nf += nfE;
        } else { //simple type
            FieldInfo::ElemNumStridePairVec pair;
            pair.push_back(std::make_pair(1,0));
            FieldInfo field(nf,et,pair);
            stinfo->getFlattenFieldInfoVec().push_back(field);
            ++nf;
        }
    }

    //Record the size of the complete struct and update max_struct.
    if (nf > maxStSize) {
        maxStruct = sty;
        maxStSize = nf;
    }
}


/*!
 * Collect simple type (non-aggregate) info
 */
void SymbolTableInfo::collectSimpleTypeInfo(const llvm::Type* ty)
{
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    /// Only one field
    stinfo->getFieldOffsetVec().push_back(0);

    FieldInfo::ElemNumStridePairVec pair;
    pair.push_back(std::make_pair(1,0));
    FieldInfo field(0, ty, pair);
    stinfo->getFlattenFieldInfoVec().push_back(field);
}

/*!
 * Compute gep offset
 */
bool SymbolTableInfo::computeGepOffset(const llvm::User *V, LocationSet& ls) {
    assert(V);
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi) {

        // Handling array types, skipe array handling here
        // We treat whole array as one, then we can distinguish different field of an array of struct
        // e.g. s[1].f1 is differet from s[0].f2
        if(isa<ArrayType>(*gi))
	  continue;

        //The int-value object of the current index operand
        //  (may not be constant for arrays).
        ConstantInt *op = dyn_cast<ConstantInt>(gi.getOperand());

        /// given a gep edge p = q + i,
        if(!op) {
            return false;
        }
        //The actual index
        Size_t idx = op->getSExtValue();


        // Handling pointer types
        // These GEP instructions are simply making address computations from the base pointer address
        // e.g. idx1 = (char*) &MyVar + 4,  at this case gep only one offset index (idx)

        if (isa<PointerType>(*gi)) {
            // If this is a pointer, we're likely accessing an array through this pointer.
            // idx gives the array index of which element is being accessed. But since this
            // is a field-index based memory model, we consider array as containing one
            // element, so there's no need to get the new element with an offset from the base
            // pointer. This can be improved by LocMemModel as it can distinguish different
            // elements of the same array.
            //off += idx;
        }


        // Handling struct here
        if (const StructType *ST = dyn_cast<StructType>(*gi) ) {
            assert(op && "non-const struct index in GEP");
            const vector<u32_t> &so = SymbolTableInfo::Symbolnfo()->getStructOffsetVec(ST);
            if ((unsigned)idx >= so.size()) {
                outs() << "!! Struct index out of bounds" << idx << "\n";
                assert(0);
            }
            //add the translated offset
            ls.offset += so[idx];
        }
    }
    return true;
}


/*!
 * Replace fields with flatten fields of T if the number of its fields is larger than msz.
 */
u32_t SymbolTableInfo::getFields(std::vector<LocationSet>& fields, const llvm::Type* T, u32_t msz)
{
    if (!isa<PointerType>(T))
        return 0;

    T = T->getContainedType(0);
    const std::vector<FieldInfo>& stVec = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(T);
    u32_t sz = stVec.size();
    if (msz < sz) {
        /// Replace fields with T's flatten fields.
        fields.clear();
        for(std::vector<FieldInfo>::const_iterator it = stVec.begin(), eit = stVec.end(); it!=eit; ++it)
            fields.push_back(LocationSet(*it));
    }

    return sz;
}


/*!
 * Find the base type and the max possible offset for an object pointed to by (V).
 */
const Type *SymbolTableInfo::getBaseTypeAndFlattenedFields(const Value *V, std::vector<LocationSet> &fields) {
    assert(V);
    fields.push_back(LocationSet(0));

    const Type *T = V->getType();
    // Use the biggest struct type out of all operands.
    if (const User *U = dyn_cast<User>(V)) {
        u32_t msz = 1;      //the max size seen so far
        // In case of BitCast, try the target type itself
        if (isa<BitCastInst>(V)) {
            u32_t sz = getFields(fields, T, msz);
            if (msz < sz) {
                msz = sz;
            }
        }
        // Try the types of all operands
        for (User::const_op_iterator it = U->op_begin(), ie = U->op_end();
                it != ie; ++it) {
            const Type *operandtype = it->get()->getType();

            u32_t sz = getFields(fields, operandtype, msz);
            if (msz < sz) {
                msz = sz;
                T = operandtype;
            }
        }
    }
    // If V is a CE, the actual pointer type is its operand.
    else if (const ConstantExpr *E = dyn_cast<ConstantExpr>(V)) {
        T = E->getOperand(0)->getType();
        getFields(fields, T, 0);
    }
    // Handle Argument case
    else if (isa<Argument>(V)) {
        getFields(fields, T, 0);
    }

    while (const PointerType *ptype = dyn_cast<PointerType>(T))
        T = ptype->getElementType();
    return T;
}

/*!
 * Get modulus offset given the type information
 */
LocationSet SymbolTableInfo::getModulusOffset(ObjTypeInfo* tyInfo, const LocationSet& ls) {

    /// if the offset is negative, it's possible that we're looking for an obj node out of range
    /// of current struct. Make the offset positive so we can still get a node within current
    /// struct to represent this obj.

    Size_t offset = ls.getOffset();
    if(offset < 0) {
        wrnMsg("try to create a gep node with negative offset.");
        offset = abs(offset);
    }
    u32_t maxOffset = tyInfo->getMaxFieldOffsetLimit();
    if (maxOffset != 0)
        offset = offset % maxOffset;
    else
        offset = 0;

    return LocationSet(offset);
}

/*!
 * Analyse types of all flattened fields of this object
 */
void ObjTypeInfo::analyzeGlobalStackObjType(const llvm::Value* val) {

    const PointerType * refty = dyn_cast<PointerType>(val->getType());
    assert(isa<PointerType>(refty) && "this value should be a pointer type!");
    Type* elemTy = refty->getElementType();
    bool isPtrObj = false;
    // Find the inter nested array element
    while (const ArrayType *AT= dyn_cast<ArrayType>(elemTy)) {
        elemTy = AT->getElementType();
        if(elemTy->isPointerTy())
            isPtrObj = true;
        if(isa<GlobalVariable>(val) && cast<GlobalVariable>(val)->hasInitializer()
                && isa<ConstantArray>(cast<GlobalVariable>(val)->getInitializer())) {
            setFlag(CONST_ARRAY_OBJ);
        }
        else
            setFlag(VAR_ARRAY_OBJ);
    }
    if (const StructType *ST= dyn_cast<StructType>(elemTy)) {
        const std::vector<FieldInfo>& flattenFields = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(ST);
        for(std::vector<FieldInfo>::const_iterator it = flattenFields.begin(), eit = flattenFields.end();
                it!=eit; ++it) {
            if((*it).getFlattenElemTy()->isPointerTy())
                isPtrObj = true;
        }
        if(isa<GlobalVariable>(val) && cast<GlobalVariable>(val)->hasInitializer()
                && isa<ConstantStruct>(cast<GlobalVariable>(val)->getInitializer()))
            setFlag(CONST_STRUCT_OBJ);
        else
            setFlag(VAR_STRUCT_OBJ);
    } else if (elemTy->isPointerTy()) {
        isPtrObj = true;
    }

    if(isPtrObj)
        setFlag(HASPTR_OBJ);
}

/*!
 * Analyse types of heap and static objects
 */
void ObjTypeInfo::analyzeHeapStaticObjType(const Value* val) {
    // TODO: Heap and static objects are considered as pointers right now.
    //       Refine this function to get more details about heap and static objects.
    setFlag(HASPTR_OBJ);
}

/*!
 * Return size of this Object
 */
u32_t ObjTypeInfo::getObjSize(const Value* val) {

    Type* ety  = cast<PointerType>(val->getType())->getElementType();
    u32_t numOfFields = 1;
    if (isa<StructType>(ety) || isa<ArrayType>(ety)) {
        numOfFields = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(ety).size();
    }
    return numOfFields;
}

/*!
 * Initialize the type info of an object
 */
void ObjTypeInfo::init(const Value* val) {

    Size_t objSize = 1;
    // Global variable
    if (isa<Function>(val)) {
        setFlag(FUNCTION_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if(isa<AllocaInst>(val)) {
        setFlag(STACK_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if(isa<GlobalVariable>(val)) {
        setFlag(GLOBVAR_OBJ);
        if(SymbolTableInfo::Symbolnfo()->isConstantObjSym(val))
            setFlag(CONST_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if (isa<Instruction>(val) && isHeapAllocExtCall(cast<Instruction>(val))) {
        setFlag(HEAP_OBJ);
        analyzeHeapStaticObjType(val);
        // Heap object, label its field as infinite here
        objSize = -1;
    }
    else if (isa<Instruction>(val) && isStaticExtCall(cast<Instruction>(val))) {
        setFlag(STATIC_OBJ);
        analyzeHeapStaticObjType(val);
        // static object allocated before main, label its field as infinite here
        objSize = -1;
    }
    else if(ArgInProgEntryFunction(val)) {
        setFlag(STATIC_OBJ);
        analyzeHeapStaticObjType(val);
        // user input data, label its field as infinite here
        objSize = -1;
    }
    else
        assert("what other object do we have??");

    // Reset maxOffsetLimit if it is over the total fieldNum of this object
    if(objSize > 0 && maxOffsetLimit > objSize)
        maxOffsetLimit = objSize;

}

/*!
 * Whether a location set is a pointer type or not
 */
bool ObjTypeInfo::isNonPtrFieldObj(const LocationSet& ls)
{
    if (isHeap() || isStaticObj())
        return false;

    llvm::Type* ety = getLLVMType();
    while (const ArrayType *AT= dyn_cast<ArrayType>(ety)) {
        ety = AT->getElementType();
    }

    if (isa<StructType>(ety) || isa<ArrayType>(ety)) {
        bool hasIntersection = false;
        const vector<FieldInfo> &infovec = SymbolTableInfo::Symbolnfo()->getFlattenFieldInfoVec(ety);
        vector<FieldInfo>::const_iterator it = infovec.begin();
        vector<FieldInfo>::const_iterator eit = infovec.end();
        for (; it != eit; ++it) {
            const FieldInfo& fieldLS = *it;
            if (ls.intersects(LocationSet(fieldLS))) {
                hasIntersection = true;
                if (fieldLS.getFlattenElemTy()->isPointerTy())
                    return false;
            }
        }
        assert(hasIntersection && "cannot find field of specified offset");
        return true;
    }
    else {
        if (isStaticObj() || isHeap()) {
            // TODO: Objects which cannot find proper field for a certain offset including
            //       arguments in main(), static objects allocated before main and heap
            //       objects. Right now they're considered to have infinite fields and we
            //       treat each field as pointers conservatively.
            //       Try to model static and heap objects more accurately in the future.
            return false;
        }
        else {
            // TODO: Using new memory model (locMM) may create objects with spurious offset
            //       as we simply return new offset by mod operation without checking its
            //       correctness in LocSymTableInfo::getModulusOffset(). So the following
            //       assertion may fail. Try to refine the new memory model.
            //assert(ls.getOffset() == 0 && "cannot get a field from a non-struct type");
            return (hasPtrObj() == false);
        }
    }
}

/*!
 * Constructor of a memory object
 */
MemObj::MemObj(const llvm::Value *val, SymID id) :
    refVal(val), GSymID(id), typeInfo(NULL), field_insensitive(false), isTainted(false) {
    init(val);
}

/*!
 * Constructor of a memory object
 */
MemObj::MemObj(SymID id) :
    refVal(NULL), GSymID(id), typeInfo(NULL), field_insensitive(false) {
    isTainted = !SymbolTableInfo::isBlkObjOrConstantObj(GSymID);
    init();
}

/*!
 * Whether it is a black hole object
 */
bool MemObj::isBlackHoleObj() const {
    return SymbolTableInfo::isBlkObj(GSymID);
}

/*!
 * Whether it is a constant object
 */
bool MemObj::isConstantObj() const {
    return SymbolTableInfo::isConstantObj(GSymID) || isTaintedObj();
}

/*!
 * Whether it is a black hole or constant object
 */
bool MemObj::isBlackHoleOrConstantObj() const {
    return isBlackHoleObj() || isConstantObj() ;
}

/*!
 * Set mem object to be field sensitive (up to maximum field limit)
 */
void MemObj::setFieldSensitive() {
    field_insensitive = false;
}
/*
 * Initial the memory object here
 */
void MemObj::init() {
    typeInfo = new ObjTypeInfo(maxFieldNumLimit);
}
/*
 * Initial the memory object here
 */
void MemObj::init(const Value *val) {
    const PointerType *refTy = NULL;

    const Instruction *I = dyn_cast<Instruction>(val);

    // We consider two types of objects:
    // (1) A heap/static object from a callsite
    if (I && isCallSite(I))
        refTy = getRefTypeOfHeapAllocOrStatic(I);
    // (2) Other objects (e.g., alloca, global, etc.)
    else
        refTy = dyn_cast<PointerType>(val->getType());

    if (refTy) {
        Type *objTy = refTy->getElementType();
        if(LocMemModel)
            typeInfo = new LocObjTypeInfo(val, objTy, maxFieldNumLimit);
        else
            typeInfo = new ObjTypeInfo(val, objTy, maxFieldNumLimit);
        typeInfo->init(val);
    } else {
        wrnMsg("try to create an object with a non-pointer type.");
        wrnMsg(val->getName());
        wrnMsg("(" + getSourceLoc(val) + ")");
        assert(false && "Memory object must be held by a pointer-typed ref value.");
    }
}

/*
 * Destroy the fields of the memory object
 */
void MemObj::destroy() {
    delete typeInfo;
    typeInfo = NULL;
}

/*!
 * Invoke llvm passes to modify module
 */
void SymbolTableInfo::prePassSchedule(llvm::Module& module)
{
    /// BreakConstantGEPs Pass
    BreakConstantGEPs* p1 = new BreakConstantGEPs();
    p1->runOnModule(module);

    /// MergeFunctionRets Pass
    UnifyFunctionExitNodes* p2 = new UnifyFunctionExitNodes();
    for (Module::iterator it = module.begin(), eit = module.end(); it != eit; ++it) {
        Function& fun = *it;
        if(fun.isDeclaration())
            continue;
        p2->runOnFunction(fun);
    }
}

/*!
 *  This method identify which is value sym and which is object sym
 */
void SymbolTableInfo::buildMemModel(llvm::Module& module) {
    analysisUtil::increaseStackSize();

    prePassSchedule(module);

    mod = &module;

    maxFieldLimit = maxFieldNumLimit;

    // Object #0 is black hole the object that may point to any object
    assert(totalSymNum == BlackHole && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, BlackHole));
    createBlkOrConstantObj(BlackHole);

    // Object #1 always represents the constant
    assert(totalSymNum == ConstantObj && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, ConstantObj));
    createBlkOrConstantObj(ConstantObj);

    // Pointer #2 always represents the pointer points-to black hole.
    assert(totalSymNum == BlkPtr && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, BlkPtr));

    // Pointer #3 always represents the null pointer.
    assert(totalSymNum == NullPtr && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum, NullPtr));

    // Add symbols for all the globals .
    for (Module::global_iterator I = module.global_begin(), E =
                module.global_end(); I != E; ++I) {
        collectSym(&*I);
    }

    // Add symbols for all the global aliases
    for (Module::alias_iterator I = module.alias_begin(), E =
                module.alias_end(); I != E; I++) {
        collectSym(&*I);
        collectSym((*I).getAliasee());
    }

    // Add symbols for all of the functions and the instructions in them.
    for (Module::iterator F = module.begin(), E = module.end(); F != E; ++F) {
        collectSym(&*F);
        collectRet(&*F);
        if (F->getFunctionType()->isVarArg())
            collectVararg(&*F);

        // Add symbols for all formal parameters.
        for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end();
                I != E; ++I) {
            collectSym(&*I);
        }

        // collect and create symbols inside the function body
        for (inst_iterator II = inst_begin(&*F), E = inst_end(&*F); II != E; ++II) {
            const Instruction *inst = &*II;
            collectSym(inst);

            // initialization for some special instructions
            //{@
            if (const StoreInst *st = dyn_cast<StoreInst>(inst)) {
                collectSym(st->getPointerOperand());
                collectSym(st->getValueOperand());
            }

            else if (const LoadInst *ld = dyn_cast<LoadInst>(inst)) {
                collectSym(ld->getPointerOperand());
            }

            else if (const PHINode *phi = dyn_cast<PHINode>(inst)) {
                for (u32_t i = 0; i < phi->getNumIncomingValues(); ++i) {
                    collectSym(phi->getIncomingValue(i));
                }
            }

            else if (const GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(
                    inst)) {
                collectSym(gep->getPointerOperand());
            }

            else if (const SelectInst *sel = dyn_cast<SelectInst>(inst)) {
                collectSym(sel->getTrueValue());
                collectSym(sel->getFalseValue());
            }

            else if (const CastInst *cast = dyn_cast<CastInst>(inst)) {
                collectSym(cast->getOperand(0));
            }
            else if (const ReturnInst *ret = dyn_cast<ReturnInst>(inst)) {
                if(ret->getReturnValue())
                    collectSym(ret->getReturnValue());
            }
            else if (isCallSite(inst) && isInstrinsicDbgInst(inst)==false) {

                CallSite cs = analysisUtil::getLLVMCallSite(inst);
                callSiteSet.insert(cs);
                for (CallSite::arg_iterator it = cs.arg_begin();
                        it != cs.arg_end(); ++it) {
                    collectSym(*it);
                }
                // Calls to inline asm need to be added as well because the callee isn't
                // referenced anywhere else.
                const Value *Callee = cs.getCalledValue();
                collectSym(Callee);

                //TODO handle inlineAsm
                ///if (isa<InlineAsm>(Callee))

            }
            //@}
        }
    }
}

/*!
 * Destroy the memory for this symbol table after use
 */
void SymbolTableInfo::destroy() {

    for (IDToMemMapTy::iterator iter = objMap.begin(); iter != objMap.end();
            ++iter) {
        if (iter->second)
            delete iter->second;
    }
    for (TypeToFieldInfoMap::iterator iter = typeToFieldInfo.begin();
            iter != typeToFieldInfo.end(); ++iter) {
        if (iter->second)
            delete iter->second;
    }
}

/*!
 * Collect symbols, including value and object syms
 */
void SymbolTableInfo::collectSym(const llvm::Value *val) {

    //TODO: filter the non-pointer type // if (!isa<PointerType>(val->getType()))  return;

    DBOUT(DMemModel, outs() << "collect sym from ##" << *val << " \n");

    // special sym here
    if (isNullPtrSym(val) || isBlackholeSym(val))
        return;

    //TODO handle constant expression value here??
    handleCE(val);

    // create a value sym
    collectVal(val);

    // create an object If it is a heap, stack, global, function.
    if (isObject(val)) {
        collectObj(val);
    }
}

/*!
 * Get value sym, if not available create a new one
 */
void SymbolTableInfo::collectVal(const llvm::Value *val) {
    ValueToIDMapTy::iterator iter = valSymMap.find(val);
    if (iter == valSymMap.end()) {
        // create val sym and sym type
        valSymMap.insert(std::make_pair(val, ++totalSymNum));
        symTyMap.insert(std::make_pair(totalSymNum, ValSym));
        DBOUT(DMemModel,
              outs() << "create a new value sym " << totalSymNum << "\n");
        ///  handle global constant expression here
        if (const GlobalVariable* globalVar = dyn_cast<GlobalVariable>(val))
            handleGlobalCE(globalVar);
    }
}

/*!
 * Get memory object sym, if not available create a new one
 */
void SymbolTableInfo::collectObj(const llvm::Value *val) {
    ValueToIDMapTy::iterator iter = objSymMap.find(val);
    if (iter == objSymMap.end()) {
        // if the object pointed by the pointer is a constant object (e.g. string)
        // then we treat them as one ConstantObj
        if(isConstantObjSym(val) && !getModelConstants()) {
            objSymMap.insert(std::make_pair(val, constantSymID()));
        }
        // otherwise, we will create an object for each abstract memory location
        else {
            // create obj sym and sym type
            objSymMap.insert(std::make_pair(val, ++totalSymNum));
            symTyMap.insert(std::make_pair(totalSymNum, ObjSym));
            DBOUT(DMemModel,
                  outs() << "create a new obj sym " << totalSymNum << "\n");

            // create a memory object
            MemObj* mem = new MemObj(val, totalSymNum);
            assert(objMap.find(totalSymNum) == objMap.end());
            objMap[totalSymNum] = mem;
        }
    }
}

/*!
 * Create unique return sym, if not available create a new one
 */
void SymbolTableInfo::collectRet(const llvm::Function *val) {
    FunToIDMapTy::iterator iter = returnSymMap.find(val);
    if (iter == returnSymMap.end()) {
        returnSymMap.insert(std::make_pair(val, ++totalSymNum));
        symTyMap.insert(std::make_pair(totalSymNum, RetSym));
        DBOUT(DMemModel,
              outs() << "create a return sym " << totalSymNum << "\n");
    }
}

/*!
 * Create vararg sym, if not available create a new one
 */
void SymbolTableInfo::collectVararg(const llvm::Function *val) {
    FunToIDMapTy::iterator iter = varargSymMap.find(val);
    if (iter == varargSymMap.end()) {
        varargSymMap.insert(std::make_pair(val, ++totalSymNum));
        symTyMap.insert(std::make_pair(totalSymNum, VarargSym));
        DBOUT(DMemModel,
              outs() << "create a vararg sym " << totalSymNum << "\n");
    }
}

/*!
 * Check whether this value is null pointer
 */
bool SymbolTableInfo::isNullPtrSym(const Value *val) {
    if (const Constant* v = dyn_cast<Constant>(val)) {
        return v->isNullValue() && v->getType()->isPointerTy();
    }
    return false;
}

/*!
 * Check whether this value is a black hole
 */
bool SymbolTableInfo::isBlackholeSym(const Value *val) {
    return (isa<UndefValue>(val));
}

/*!
 * Check whether this value points-to a constant object
 */
bool SymbolTableInfo::isConstantObjSym(const Value *val) {
    if (const GlobalVariable* v = dyn_cast<GlobalVariable>(val)) {
        if (cppUtil::isValVtbl(const_cast<GlobalVariable*>(v)))
            return false;
        else if (!v->hasInitializer())
            return true;
        else {
            StInfo *stInfo = getStructInfo(v->getInitializer()->getType());
            const std::vector<FieldInfo> &fields = stInfo->getFlattenFieldInfoVec();
            for (std::vector<FieldInfo>::const_iterator it = fields.begin(), eit = fields.end(); it != eit; ++it) {
                const FieldInfo &field = *it;
                const Type *elemTy = field.getFlattenElemTy();
                assert(!isa<FunctionType>(elemTy) && "Initializer of a global is a function?");
                if (isa<PointerType>(elemTy))
                    return false;
            }

            return v->isConstant();
        }
    }
    return false;
}


/*!
 * Handle constant expression
 */
void SymbolTableInfo::handleCE(const Value *val) {
    if (const Constant* ref = dyn_cast<Constant>(val)) {
        if (const ConstantExpr* ce = isGepConstantExpr(ref)) {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        } else if (const ConstantExpr* ce = isCastConstantExpr(ref)) {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        } else if (const ConstantExpr* ce = isSelectConstantExpr(ref)) {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            collectVal(ce->getOperand(1));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
            handleCE(ce->getOperand(1));
        }
        // remember to handle the constant bit cast opnd after stripping casts off
        else {
            collectVal(ref);
        }
    }
}

/*!
 * Handle global constant expression
 */
void SymbolTableInfo::handleGlobalCE(const GlobalVariable *G) {
    assert(G);

    //The type this global points to
    const Type *T = G->getType()->getContainedType(0);
    bool is_array = 0;
    //An array is considered a single variable of its type.
    while (const ArrayType *AT = dyn_cast<ArrayType>(T)) {
        T = AT->getElementType();
        is_array = 1;
    }

    if (isa<StructType>(T)) {
        //A struct may be used in constant GEP expr.
        for (Value::const_user_iterator it = G->user_begin(), ie = G->user_end();
                it != ie; ++it) {
            handleCE(*it);
        }
    } else {
        if (is_array) {
            for (Value::const_user_iterator it = G->user_begin(), ie =
                        G->user_end(); it != ie; ++it) {
                handleCE(*it);
            }
        }
    }

    if (G->hasInitializer()) {
        handleGlobalInitializerCE(G->getInitializer(), 0);
    }
}

/*!
 * Handle global variable initialization
 */
void SymbolTableInfo::handleGlobalInitializerCE(const Constant *C,
        u32_t offset) {

    if (C->getType()->isSingleValueType() && isa<PointerType>(C->getType())) {
        if (const ConstantExpr *E = dyn_cast<ConstantExpr>(C)) {
            handleCE(E);
        }
        else {
            collectVal(C);
        }
    } else if (isa<ConstantArray>(C)) {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++) {
            handleGlobalInitializerCE(cast<Constant>(C->getOperand(i)), offset);
        }
    } else if (isa<ConstantStruct>(C)) {
        const StructType *sty = cast<StructType>(C->getType());
        const std::vector<u32_t>& offsetvect =
            SymbolTableInfo::Symbolnfo()->getStructOffsetVec(sty);
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++) {
            u32_t off = offsetvect[i];
            handleGlobalInitializerCE(cast<Constant>(C->getOperand(i)),
                                      offset + off);
        }
    }
}


/*
 * Print out the composite type information
 */
void SymbolTableInfo::printFlattenFields(const llvm::Type* type) {

    if(const ArrayType *at = dyn_cast<ArrayType> (type)) {
        outs() <<"  {Type: ";
        at->print(outs());
        outs() << "}\n";
        outs() << "\tarray type ";
        outs() << "\t [element size = " << getTypeSizeInBytes(at->getElementType()) << "]\n";
        outs() << "\n";
    }

    else if(const StructType *st = dyn_cast<StructType> (type)) {
        outs() <<"  {Type: ";
        st->print(outs());
        outs() << "}\n";
        std::vector<FieldInfo>& finfo = getStructInfo(st)->getFlattenFieldInfoVec();
        int field_idx = 0;
        for(std::vector<FieldInfo>::iterator it = finfo.begin(), eit = finfo.end();
                it!=eit; ++it, field_idx++) {
            outs() << " \tField_idx = " << field_idx << " [offset: " << (*it).getFlattenOffset();
            outs() << ", field type: ";
            (*it).getFlattenElemTy()->print(outs());
            outs() << ", field size: " << getTypeSizeInBytes((*it).getFlattenElemTy());
            outs() << ", field stride pair: ";
            for(FieldInfo::ElemNumStridePairVec::const_iterator pit = (*it).elemStridePairBegin(),
                    peit = (*it).elemStridePairEnd(); pit!=peit; ++pit) {
                outs() << "[ " << pit->first << ", " << pit->second << " ] ";
            }
            outs() << "\n";
        }
        outs() << "\n";
    }

    else if (const PointerType* pt= dyn_cast<PointerType> (type)) {
        u32_t sizeInBits = getTypeSizeInBytes(type);
        u32_t eSize = getTypeSizeInBytes(pt->getElementType());
        outs() << "  {Type: ";
        pt->print(outs());
        outs() << "}\n";
        outs() <<"\t [pointer size = " << sizeInBits << "]";
        outs() <<"\t [target size = " << eSize << "]\n";
        outs() << "\n";
    }

    else if ( const FunctionType* fu= dyn_cast<FunctionType> (type)) {
        outs() << "  {Type: ";
        fu->getReturnType()->print(outs());
        outs() << "(Function)}\n\n";
    }

    else {
        assert(type->isSingleValueType() && "not a single value type, then what else!!");
        /// All rest types are scalar type?
        u32_t eSize = getTypeSizeInBytes(type);
        outs() <<"  {Type: ";
        type->print(outs());
        outs() << "}\n";
        outs() <<"\t [object size = " << eSize << "]\n";
        outs() << "\n";
    }
}


/*
 * Get the type size given a target data layout
 */
u32_t SymbolTableInfo::getTypeSizeInBytes(const llvm::Type* type) {

    // if the type has size then simply return it, otherwise just return 0
    if(type->isSized())
        return  getDataLayout()->getTypeStoreSize(const_cast<Type*>(type));
    else
        return 0;
}

