#include <WPA/Debugger.h>
#include "llvm/IR/InstIterator.h"
#include <unordered_set>
#include "Util/Options.h"

void Debugger::instrumentPointer(Instruction* inst) {
    LLVMContext& C = inst->getContext();
    Type* longType = IntegerType::get(mod->getContext(), 64);
    Type* intType = IntegerType::get(mod->getContext(), 32);
    Type* ptrToLongType = PointerType::get(longType, 0);

    Value* pointer = nullptr;

    if (LoadInst* loadInst = SVFUtil::dyn_cast<LoadInst>(inst)) {
        pointer = loadInst;
    } else if (StoreInst* storeInst = SVFUtil::dyn_cast<StoreInst>(inst)) {
        pointer = storeInst->getPointerOperand();
    } /*else if (AllocaInst* allocaInst = SVFUtil::dyn_cast<AllocaInst>(inst)) {
        pointer = allocaInst;
    }
    */

    NodeID nodeId = pag->getValueNode(inst);
    bool isRelax = false;

    std::vector<int> dbgTgtIDs;
    for (PointsTo::iterator piter = _pta->getPts(nodeId).begin(), epiter =
            _pta->getPts(nodeId).end(); piter != epiter; ++piter) {
        NodeID ptd = *piter;
        PAGNode* ptdNode = pag->getPAGNode(ptd);

        
        if (ptdNode->hasValue()) {
            Value* ptdVal = const_cast<Value*>(ptdNode->getValue());
            if (ptdVal == pointer) continue;

            auto it = recorded.find(ptdVal);
            if (it == recorded.end()) {
                dbgTgtID++;
                recorded[ptdVal] = dbgTgtID;
                recordTarget(dbgTgtID, ptdVal, recordTargetFn);
            } else {
                dbgTgtID = it->second;
            }
            dbgTgtIDs.push_back(dbgTgtID);
        }
        if (SVFUtil::isa<GepObjPN>(ptdNode)) {
            isRelax = true;
        }
    }

    IRBuilder Builder(inst->getNextNode());

    // Call the ptdTargetCheckFn

    int len = dbgTgtIDs.size();
    Constant* clen = ConstantInt::get(longType, len);

    ArrayType* arrTy = ArrayType::get(longType, len);

    std::vector<Constant*> consIdVec;

    for (int i: dbgTgtIDs) {
        consIdVec.push_back(ConstantInt::get(longType, i));
    }

    llvm::ArrayRef<Constant*> consIdArr(consIdVec);

    Constant* dbgIdArr = ConstantArray::get(arrTy, consIdArr);
    
    GlobalVariable* dbgArrGvar = new GlobalVariable(*mod, 
            /*Type=*/arrTy,
            /*isConstant=*/true,
            /*Linkage=*/GlobalValue::ExternalLinkage,
            /*Initializer=*/0, // has initializer, specified below
            /*Name=*/"cons id");
    dbgArrGvar->setInitializer(dbgIdArr);


    Constant* Zero = ConstantInt::get(C, llvm::APInt(64, 0));
    llvm::ArrayRef<Constant*> zeroIdxs {Zero, Zero};
    Value* firstCons = ConstantExpr::getGetElementPtr(arrTy, dbgArrGvar, zeroIdxs);

    Value* arg1 = Builder.CreateBitOrPointerCast(pointer, ptrToLongType);

    Value* arg2 = clen;
    Value* arg3 = Builder.CreateBitOrPointerCast(firstCons, ptrToLongType);

    Constant* arg4 = ConstantInt::get(longType, isRelax);
    Builder.CreateCall(ptdTargetCheckFn, {arg1, arg2, arg3, arg4});
}

void Debugger::addFunctionDefs() {
    Type* voidType = Type::getVoidTy(mod->getContext());
    Type* longType = IntegerType::get(mod->getContext(), 64);
    Type* intType = IntegerType::get(mod->getContext(), 32);

    // Install the Record Routine
    std::vector<Type*> recordTypes;
    recordTypes.push_back(intType);
    recordTypes.push_back(longType);

    llvm::ArrayRef<Type*> recordTypeArr(recordTypes);

    FunctionType* recordFnTy = FunctionType::get(voidType, recordTypeArr, false);
    recordTargetFn = Function::Create(recordFnTy, Function::ExternalLinkage, "recordTarget", mod);

    svfMod->addFunctionSet(recordTargetFn);
    // Install the ptdTargetCheck Routine
    // ptdTargetCheck(unsigned long* valid_tgts, long len, unsigned long* tgt);
    std::vector<Type*> ptdTargetCheckType;

    Type* ptrToLongType = PointerType::get(longType, 0);
    ptdTargetCheckType.push_back(ptrToLongType);
    ptdTargetCheckType.push_back(longType);
    ptdTargetCheckType.push_back(ptrToLongType); 
    ptdTargetCheckType.push_back(longType);

    llvm::ArrayRef<Type*> ptdTargetCheckTypeArr(ptdTargetCheckType);

    FunctionType* ptdTargetCheckFnTy = FunctionType::get(intType, ptdTargetCheckTypeArr, false);
    ptdTargetCheckFn = Function::Create(ptdTargetCheckFnTy, Function::ExternalLinkage, "checkPtr", mod);

    svfMod->addFunctionSet(ptdTargetCheckFn);
}

void Debugger::init() { 
    dbgTgtID = 0;
    addFunctionDefs();
    std::unordered_set<std::string> DebugFuncNames(Options::DebugFuncsList.begin(),
            Options::DebugFuncsList.end());

    std::vector<Instruction*> instList;

    for (Function& F: mod->getFunctionList()) {
        if (std::find(DebugFuncNames.begin(), DebugFuncNames.end(), F.getName().str())
                != DebugFuncNames.end()) {
            for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                Instruction* inst = &*I;
                if (LoadInst* ldInst = SVFUtil::dyn_cast<LoadInst>(inst)) {
                    if (inst->getType()->isPointerTy()) {
                        Value* ptr = ldInst->getPointerOperand();
                        if (ptr->hasName() && ptr->getName().contains("argv")) {
                            continue;
                        }
                        instList.push_back(inst);
                    }
                }
            }
        }
    }

    for (Instruction* inst: instList) {
        instrumentPointer(inst);
    }
//    mod->dump();
}
