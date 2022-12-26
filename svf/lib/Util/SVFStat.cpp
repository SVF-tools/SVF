//===- SVFStat.cpp -- Base class for statistics---------------------------------//
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
 * SVFStat.cpp
 *
 *  Created on: Sep 1, 2022
 *      Author: Xiao Cheng
 */

#include "Util/Options.h"
#include "Util/SVFStat.h"

using namespace SVF;
using namespace std;

double SVFStat::timeOfBuildingLLVMModule = 0;
double SVFStat::timeOfBuildingSVFIR = 0;
double SVFStat::timeOfBuildingSymbolTable = 0;


SVFStat::SVFStat() : startTime(0), endTime(0)
{
    assert((Options::ClockType() == ClockType::Wall || Options::ClockType() == ClockType::CPU)
           && "PTAStat: unknown clock type!");
}

double SVFStat::getClk(bool mark)
{
    if (Options::MarkedClocksOnly() && !mark) return 0.0;

    if (Options::ClockType() == ClockType::Wall)
    {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        return (double)(time.tv_nsec + time.tv_sec * 1000000000) / 1000000.0;
    }
    else if (Options::ClockType() == ClockType::CPU)
    {
        return CLOCK_IN_MS();
    }

    assert(false && "PTAStat::getClk: unknown clock type");
    abort();
}

void SVFStat::printStat(string statname)
{

    std::string moduleName(SVFIR::getPAG()->getModule()->getModuleIdentifier());
    std::vector<std::string> names = SVFUtil::split(moduleName,'/');
    if (names.size() > 1)
    {
        moduleName = names[names.size() - 1];
    }

    SVFUtil::outs() << "\n*********" << statname << "***************\n";
    SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
    SVFUtil::outs().flags(std::ios::left);
    unsigned field_width = 20;
    for(NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for(TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
    }
    for(NUMStatMap::iterator it = PTNumStatMap.begin(), eit = PTNumStatMap.end(); it!=eit; ++it)
    {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
    }

    SVFUtil::outs() << "#######################################################" << std::endl;
    SVFUtil::outs().flush();
    generalNumMap.clear();
    PTNumStatMap.clear();
    timeStatMap.clear();
}

void SVFStat::performStat()
{

    SVFIR* pag = SVFIR::getPAG();
    u32_t numOfFunction = 0;
    u32_t numOfGlobal = 0;
    u32_t numOfStack = 0;
    u32_t numOfHeap = 0;
    u32_t numOfHasVarArray = 0;
    u32_t numOfHasVarStruct = 0;
    u32_t numOfHasConstArray = 0;
    u32_t numOfHasConstStruct = 0;
    u32_t numOfScalar = 0;
    u32_t numOfConstant = 0;
    u32_t fiObjNumber = 0;
    u32_t fsObjNumber = 0;
    Set<SymID> memObjSet;
    for(SVFIR::iterator it = pag->begin(), eit = pag->end(); it!=eit; ++it)
    {
        PAGNode* node = it->second;
        if(ObjVar* obj = SVFUtil::dyn_cast<ObjVar>(node))
        {
            const MemObj* mem = obj->getMemObj();
            if (memObjSet.insert(mem->getId()).second == false)
                continue;
            if(mem->isBlackHoleObj())
                continue;
            if(mem->isFunction())
                numOfFunction++;
            if(mem->isGlobalObj())
                numOfGlobal++;
            if(mem->isStack())
                numOfStack++;
            if(mem->isHeap())
                numOfHeap++;
            if(mem->isVarArray())
                numOfHasVarArray++;
            if(mem->isVarStruct())
                numOfHasVarStruct++;
            if(mem->isConstantArray())
                numOfHasConstArray++;
            if(mem->isConstantStruct())
                numOfHasConstStruct++;
            if(mem->hasPtrObj() == false)
                numOfScalar++;
            if(mem->isConstDataOrConstGlobal())
                numOfConstant++;

            if (mem->isFieldInsensitive())
                fiObjNumber++;
            else
                fsObjNumber++;
        }
    }



    generalNumMap["TotalPointers"] = pag->getValueNodeNum() + pag->getFieldValNodeNum();
    generalNumMap["TotalObjects"] = pag->getObjectNodeNum();
    generalNumMap["TotalFieldObjects"] = pag->getFieldObjNodeNum();
    generalNumMap["MaxStructSize"] = SymbolTableInfo::SymbolInfo()->getMaxStructSize();
    generalNumMap["TotalSVFStmts"] = pag->getPAGEdgeNum();
    generalNumMap["TotalPTASVFStmts"] = pag->getPTAPAGEdgeNum();
    generalNumMap["FIObjNum"] = fiObjNumber;
    generalNumMap["FSObjNum"] = fsObjNumber;

    generalNumMap["AddrsNum"] = pag->getSVFStmtSet(SVFStmt::Addr).size();
    generalNumMap["LoadsNum"] = pag->getSVFStmtSet(SVFStmt::Load).size();
    generalNumMap["StoresNum"] = pag->getSVFStmtSet(SVFStmt::Store).size();
    generalNumMap["CopysNum"] =  pag->getSVFStmtSet(SVFStmt::Copy).size();
    generalNumMap["GepsNum"] =  pag->getSVFStmtSet(SVFStmt::Gep).size();
    generalNumMap["CallsNum"] = pag->getSVFStmtSet(SVFStmt::Call).size();
    generalNumMap["ReturnsNum"] = pag->getSVFStmtSet(SVFStmt::Ret).size();

    generalNumMap["FunctionObjs"] = numOfFunction;
    generalNumMap["GlobalObjs"] = numOfGlobal;
    generalNumMap["HeapObjs"]  = numOfHeap;
    generalNumMap["StackObjs"] = numOfStack;

    generalNumMap["VarStructObj"] = numOfHasVarStruct;
    generalNumMap["VarArrayObj"] = numOfHasVarArray;
    generalNumMap["ConstStructObj"] = numOfHasConstStruct;
    generalNumMap["ConstArrayObj"] = numOfHasConstArray;
    generalNumMap["NonPtrObj"] = numOfScalar;
    generalNumMap["ConstantObj"] = numOfConstant;

    generalNumMap["IndCallSites"] = pag->getIndirectCallsites().size();
    generalNumMap["TotalCallSite"] = pag->getCallSiteSet().size();

    timeStatMap["LLVMIRTime"] = SVFStat::timeOfBuildingLLVMModule;
    timeStatMap["SymbolTableTime"] = SVFStat::timeOfBuildingSymbolTable;
    timeStatMap["SVFIRTime"] = SVFStat::timeOfBuildingSVFIR;

    // REFACTOR-TODO bitcastInstStat();
    branchStat();

    printStat("General Stats");

}


void SVFStat::branchStat()
{
    SVFModule* module = SVFIR::getPAG()->getModule();
    u32_t numOfBB_2Succ = 0;
    u32_t numOfBB_3Succ = 0;
    for (SVFModule::const_iterator funIter = module->begin(), funEiter = module->end();
            funIter != funEiter; ++funIter)
    {
        const SVFFunction* func = *funIter;
        for (SVFFunction::const_iterator bbIt = func->begin(), bbEit = func->end();
                bbIt != bbEit; ++bbIt)
        {
            const SVFBasicBlock* bb = *bbIt;
            u32_t numOfSucc = bb->getNumSuccessors();
            if (numOfSucc == 2)
                numOfBB_2Succ++;
            else if (numOfSucc > 2)
                numOfBB_3Succ++;
        }
    }

    generalNumMap["BBWith2Succ"] = numOfBB_2Succ;
    generalNumMap["BBWith3Succ"] = numOfBB_3Succ;
}

/* REFACTOR-TODO
void PTAStat::bitcastInstStat()
{
    SVFModule* module = pta->getModule();
    u32_t numberOfBitCast = 0;
    for (SVFModule::llvm_const_iterator funIter = module->llvmFunBegin(), funEiter = module->llvmFunEnd();
            funIter != funEiter; ++funIter)
    {
        const Function* func = *funIter;
        for (Function::const_iterator bbIt = func->begin(), bbEit = func->end();
                bbIt != bbEit; ++bbIt)
        {
            const BasicBlock& bb = *bbIt;
            for (BasicBlock::const_iterator instIt = bb.begin(), instEit = bb.end();
                    instIt != instEit; ++instIt)
            {
                const Instruction& inst = *instIt;
                if (const BitCastInst* bitcast = SVFUtil::dyn_cast<BitCastInst>(&inst))
                {
                    if (SVFUtil::isa<PointerType>(bitcast->getSrcTy()))
                        numberOfBitCast++;
                }
            }
        }
    }

    generalNumMap["BitCastNumber"] = numberOfBitCast;
}
*/
