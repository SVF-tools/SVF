//===- ExeState.cpp -- Execution State ----------------------------//
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
 * ExeState.cpp
 *
 *  Created on: April 29, 2022
 *      Author: Xiao
 */
#include "Util/ExeState.h"
#include "MemoryModel/SVFIR.h"
#include <iomanip>
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

z3::context *Z3Expr::ctx = nullptr;

/*!
 * Init Z3Expr for ValVar
 * @param valVar
 * @param e
 */
void ExeState::initValVar(const ValVar *valVar, Z3Expr &e)
{
    std::string str;
    raw_string_ostream rawstr(str);
    SVFIR *svfir = PAG::getPAG();

    rawstr << "ValVar" << valVar->getId();
    if (const Type *type = valVar->getType())
    {
        if (type->isIntegerTy() || type->isFloatingPointTy() || type->isPointerTy() || type->isFunctionTy()
                || type->isStructTy() || type->isArrayTy() || type->isVoidTy() || type->isLabelTy() || type->isMetadataTy())
            e = getContext().int_const(rawstr.str().c_str());
        else
        {
            SVFUtil::errs() << value2String(valVar->getValue()) << "\n" << " type: " << type2String(type) << "\n";
            assert(false && "what other types we have");
        }
    }
    else
    {
        if (svfir->getNullPtr() == valVar->getId())
            e = getContext().int_val(0);
        else
            e = getContext().int_const(rawstr.str().c_str());
        assert(SVFUtil::isa<DummyValVar>(valVar) && "not a DummValVar if it has no type?");
    }
}

/*!
 * Init Z3Expr for ObjVar
 * @param objVar
 * @param e
 */
void ExeState::initObjVar(const ObjVar *objVar, Z3Expr &e)
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ObjVar" << objVar->getId();

    if (objVar->hasValue())
    {
        const MemObj *obj = objVar->getMemObj();
        /// constant data
        if (obj->isConstantData() || obj->isConstantArray() || obj->isConstantStruct())
        {
            if (const ConstantInt *consInt = SVFUtil::dyn_cast<ConstantInt>(obj->getValue()))
            {
                if (consInt == llvm::ConstantInt::getTrue(LLVMModuleSet::getLLVMModuleSet()->getContext()))
                {
                    e = getContext().int_val(1);
                }
                else if (consInt == llvm::ConstantInt::getFalse(LLVMModuleSet::getLLVMModuleSet()->getContext()))
                    e = getContext().int_val(0);
                else
                {
                    e = getContext().int_val(consInt->getSExtValue());
                }
            }
            else if (const llvm::ConstantFP *consFP = SVFUtil::dyn_cast<llvm::ConstantFP>(obj->getValue()))
                e = getContext().int_val(static_cast<u32_t>(consFP->getValueAPF().convertToFloat()));
            else if (SVFUtil::isa<ConstantPointerNull>(obj->getValue()))
                e = getContext().int_val(0);
            else if (SVFUtil::isa<GlobalVariable>(obj->getValue()))
                e = getContext().int_val(getVirtualMemAddress(objVar->getId()));
            else if (SVFUtil::isa<ConstantAggregate>(obj->getValue()))
                assert(false && "implement this part");
            else
            {
                std::cerr << value2String(obj->getValue()) << "\n";
                assert(false && "what other types of values we have?");
            }
        }
        /// locations (address-taken variables)
        else
        {
            e = getContext().int_val(getVirtualMemAddress(objVar->getId()));
        }
    }
    else
    {
        assert(SVFUtil::isa<DummyObjVar>(objVar) &&
               "it should either be a blackhole or constant dummy if this obj has no value?");
        e = getContext().int_val(getVirtualMemAddress(objVar->getId()));
    }
}

/*!
 * Return Z3 expression based on SVFVar ID
 * @param varId SVFVar ID
 * @return
 */
Z3Expr &ExeState::getZ3Expr(u32_t varId)
{
    assert(getInternalID(varId) == varId && "SVFVar idx overflow > 0x7f000000?");
    Z3Expr &e = varToVal[varId];
    if (eq(e, Z3Expr::nullExpr()))
    {
        std::string str;
        raw_string_ostream rawstr(str);
        SVFIR *svfir = PAG::getPAG();
        SVFVar *svfVar = svfir->getGNode(varId);
        if (const ValVar *valVar = dyn_cast<ValVar>(svfVar))
        {
            initValVar(valVar, e);
        }
        else if (const ObjVar *objVar = dyn_cast<ObjVar>(svfVar))
        {
            initObjVar(objVar, e);
        }
        else
        {
            assert(false && "var type not supported");
        }
    }
    return e;
}

ExeState &ExeState::operator=(const ExeState &rhs)
{
    if (*this != rhs)
    {
        varToVal = rhs.getVarToVal();
        locToVal = rhs.getLocToVal();
        pathConstraint = rhs.getPathConstraint();
    }
    return *this;
}

/*!
 * Overloading Operator==
 * @param rhs
 * @return
 */
bool ExeState::operator==(const ExeState &rhs) const
{
    return eq(pathConstraint, rhs.getPathConstraint()) && eqVarToValMap(varToVal, rhs.getVarToVal()) &&
           eqVarToValMap(locToVal, rhs.getLocToVal());
}

/*!
 * Overloading Operator<
 * @param rhs
 * @return
 */
bool ExeState::operator<(const ExeState &rhs) const
{
    // judge from path constraint
    if (!eq(pathConstraint, rhs.getPathConstraint()))
        return pathConstraint.id() < rhs.getPathConstraint().id();
    if (lessThanVarToValMap(varToVal, rhs.getVarToVal()) || lessThanVarToValMap(locToVal, rhs.getLocToVal()))
        return true;
    return false;
}

bool ExeState::eqVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const
{
    if(lhs.size() != rhs.size()) return false;
    for (const auto &item: lhs)
    {
        auto it = rhs.find(item.first);
        // return false if SVFVar not exists in rhs or z3Expr not equal
        if (it == rhs.end() || !eq(item.second, it->second))
            return false;
    }
    return true;
}

bool ExeState::lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs) const
{
    if(lhs.size() != rhs.size()) return lhs.size() < rhs.size();
    for (const auto &item: lhs)
    {
        auto it = rhs.find(item.first);
        // lhs > rhs if SVFVar not exists in rhs
        if (it == rhs.end())
            return false;
        // judge from expr id
        if (!eq(item.second, it->second))
            return item.second.id() < it->second.id();
    }
    return false;
}
/*!
 * Store value to location
 * @param loc location, e.g., int_val(0x7f..01)
 * @param value
 */
void ExeState::store(const Z3Expr &loc, const Z3Expr &value)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    store(getInternalID(virAddr), value);
}

/*!
 * Load value at location
 * @param loc location, e.g., int_val(0x7f..01)
 * @return
 */
Z3Expr &ExeState::load(const Z3Expr &loc)
{
    assert(loc.is_numeral() && "location must be numeral");
    s32_t virAddr = z3Expr2NumValue(loc);
    assert(isVirtualMemAddress(virAddr) && "Pointer operand is not a physical address?");
    u32_t objId = getInternalID(virAddr);
    assert(getInternalID(objId) == objId && "SVFVar idx overflow > 0x7f000000?");
    return load(objId);
}

/*!
 * Print values of all expressions
 */
void ExeState::printExprValues()
{
    std::cout.flags(std::ios::left);
    std::cout << "-----------Var and Value-----------\n";
    for (const auto &item: getVarToVal())
    {
        std::stringstream exprName;
        exprName << "Var" << item.first;
        std::cout << std::setw(25) << exprName.str();
        const Z3Expr &sim = item.second.simplify();
        if (sim.is_numeral() && isVirtualMemAddress(z3Expr2NumValue(sim)))
        {
            std::cout << "\t Value: " << std::hex << "0x" << z3Expr2NumValue(sim) << "\n";
        }
        else
        {
            std::cout << "\t Value: " << std::dec << sim << "\n";
        }
    }
    std::cout << "-----------------------------------------\n";
}
