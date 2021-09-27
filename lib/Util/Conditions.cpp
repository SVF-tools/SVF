//===- Conditions.cpp -- Context/path conditions in the form of BDDs----------//
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
 * Conditions.cpp
 *
 *  Created on: Sep 22, 2014
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "Util/Conditions.h"
#include "Util/SVFUtil.h"

using namespace SVF;

CondExpr* CondManager::trueCond = nullptr;
CondExpr* CondManager::falseCond = nullptr;

/// Constructor
CondManager::CondManager() : sol(cxt)
{
    const z3::expr &trueExpr = cxt.bool_val(true);
    trueCond = new CondExpr(trueExpr);
    allocatedConds[trueExpr.hash()] = trueCond;
    const z3::expr &falseExpr = cxt.bool_val(false);
    falseCond = new CondExpr(falseExpr);
    allocatedConds[falseExpr.hash()] = falseCond;
}

/// Destructor
CondManager::~CondManager()
{
    for (const auto& it : allocatedConds)
    {
        delete it.second;
    }
}

/// And operator for two expressions
CondExpr* CondManager::AND(const CondExpr *lhs, const CondExpr *rhs)
{
    if (lhs->getExpr().is_false() || rhs->getExpr().is_false())
        return falseCond;
    else if (lhs->getExpr().is_true()){
        assert(allocatedConds.count(rhs->getExpr().hash()) && "rhs not in allocated conds");
        return allocatedConds[rhs->getExpr().hash()];
    } else if (rhs->getExpr().is_true()){
        assert(allocatedConds.count(lhs->getExpr().hash()) && "lhs not in allocated conds");
        return allocatedConds[lhs->getExpr().hash()];
    } else {
        const z3::expr& expr = (lhs->getExpr() && rhs->getExpr()).simplify();
        if (allocatedConds.count(expr.hash()))
            return allocatedConds[expr.hash()];
        else{
            Map<u32_t, z3::expr> lhsBrMap = lhs->getBrExprMap();
            Map<u32_t, std::vector<uint64_t>> lhsSwitchValuesExprMap = lhs->getSwitchValuesExprMap();
            for (const auto &e: rhs->getBrExprMap())
                lhsBrMap.insert(e);
            for (const auto &e: rhs->getSwitchValuesExprMap())
                lhsSwitchValuesExprMap.insert(e);
            CondExpr *cond = new CondExpr(expr);
            cond->setBrExprMap(lhsBrMap);
            cond->setSwitchValuesMap(lhsSwitchValuesExprMap);
            allocatedConds[expr.hash()] = cond;
            return cond;
        }
    }
}

/// Or operator for two expressions
CondExpr* CondManager::OR(const CondExpr *lhs, const CondExpr *rhs)
{
    if (lhs->getExpr().is_true() || rhs->getExpr().is_true())
        return trueCond;
    else if (lhs->getExpr().is_false()){
        assert(allocatedConds.count(rhs->getExpr().hash()) && "rhs not in allocated conds");
        return allocatedConds[rhs->getExpr().hash()];
    } else if (rhs->getExpr().is_false()){
        assert(allocatedConds.count(lhs->getExpr().hash()) && "lhs not in allocated conds");
        return allocatedConds[lhs->getExpr().hash()];
    } else {
        const z3::expr& expr = (lhs->getExpr() || rhs->getExpr()).simplify();
        if (allocatedConds.count(expr.hash()))
            return allocatedConds[expr.hash()];
        else{
            Map<u32_t, z3::expr> lhsBrMap = lhs->getBrExprMap();
            Map<u32_t, std::vector<uint64_t>> lhsSwitchValuesExprMap = lhs->getSwitchValuesExprMap();
            for (const auto &e: rhs->getBrExprMap())
                lhsBrMap.insert(e);
            for (const auto &e: rhs->getSwitchValuesExprMap())
                lhsSwitchValuesExprMap.insert(e);
            CondExpr *cond = new CondExpr(expr);
            cond->setBrExprMap(lhsBrMap);
            cond->setSwitchValuesMap(lhsSwitchValuesExprMap);
            allocatedConds[expr.hash()] = cond;
            return cond;
        }
    }
}

/// Neg operator for an expression
CondExpr* CondManager::NEG(const CondExpr *lhs)
{
    if (lhs->getExpr().is_true())
        return falseCond;
    else if (lhs->getExpr().is_false())
        return trueCond;
    else{
        const z3::expr& expr = (!lhs->getExpr()).simplify();
        if (allocatedConds.count(expr.hash()))
            return allocatedConds[expr.hash()];
        else{
            CondExpr *cond = new CondExpr(expr);
            cond->setBrExprMap(lhs->getBrExprMap());
            cond->setSwitchValuesMap(lhs->getSwitchValuesExprMap());
            allocatedConds[expr.hash()] = cond;
            return cond;
        }
    }
}

/// Create a single condition
CondExpr* CondManager::createCond(u32_t i)
{
    const z3::expr &expr = cxt.bool_const(("c" + std::to_string(i)).c_str());
    if (allocatedConds.count(expr.hash()))
        return allocatedConds[expr.hash()];
    else{
        CondExpr *cond = new CondExpr(expr);
        allocatedConds[expr.hash()] = cond;
        return cond;
    }
}

/// Create a single condition
CondExpr* CondManager::createCond(const z3::expr& e)
{
    z3::expr es = e.simplify();
    if (allocatedConds.count(es.hash()))
        return allocatedConds[es.hash()];
    else{
        CondExpr *cond = new CondExpr(es);
        allocatedConds[es.hash()] = cond;
        return cond;
    }
}

/// Return the number of condition expressions
u32_t CondManager::getCondNumber()
{
    return sol.get_model().size();
}

/// Print the expressions in this model
void CondManager::printModel()
{
    std::cout << sol.check() << "\n";
    z3::model m = sol.get_model();
    for (u32_t i = 0; i < m.size(); i++)
    {
        z3::func_decl v = m[static_cast<s32_t>(i)];
        std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
    }
}

/// Return memory usage for this condition manager
std::string CondManager::getMemUsage()
{
    //std::ostringstream os;
    //memory::display_max_usage(os);
    //return os.str();
    return "";
}

/// Extract sub conditions of this expression
void CondManager::extractSubConds(const CondExpr *e, NodeBS &support) const
{
//    if (e->getExpr().num_args() == 0)
//        if (!e->getExpr().is_true() && !e->getExpr().is_false())
//            support.set(e->getExpr().hash());
//    for (u32_t i = 0; i < e->getExpr().num_args(); ++i) {
//        const z3::expr &expr = e->getExpr().arg(i);
//        if (!expr.is_true() && !expr.is_false())
//            support.set(expr.hash());
//    }
}

/// solve condition
bool CondManager::isSatisfiable(const z3::expr& e){
    sol.reset();
    sol.add(e);
    z3::check_result result = sol.check();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

/// Print out one particular expression
inline void CondManager::printDbg(const CondExpr *e)
{
    std::cout << e->getExpr() << "\n";
}

/// Return string format of this expression
std::string CondManager::dumpStr(const CondExpr *e) const
{
    std::ostringstream out;
    out << e->getExpr();
    return out.str();
}

/// Operations on conditions.
//@{
/// use Cudd_bddAndLimit interface to avoid bdds blow up
DdNode* BddCondManager::AND(DdNode* lhs, DdNode* rhs)
{
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return rhs;
    else if (rhs == getTrueCond())
        return lhs;
    else
    {
        DdNode* tmp = Cudd_bddAndLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
        if(tmp==nullptr)
        {
            SVFUtil::writeWrnMsg("exceeds max bdd size \n");
            ///drop the rhs condition
            return lhs;
        }
        else
        {
            Cudd_Ref(tmp);
            return tmp;
        }
    }
}

/*!
 * Use Cudd_bddOrLimit interface to avoid bdds blow up
 */
DdNode* BddCondManager::OR(DdNode* lhs, DdNode* rhs)
{
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return rhs;
    else if (rhs == getFalseCond())
        return lhs;
    else
    {
        DdNode* tmp = Cudd_bddOrLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
        if(tmp==nullptr)
        {
            SVFUtil::writeWrnMsg("exceeds max bdd size \n");
            /// drop the two conditions here
            return getTrueCond();
        }
        else
        {
            Cudd_Ref(tmp);
            return tmp;
        }
    }
}

DdNode* BddCondManager::NEG(DdNode* lhs)
{
    if (lhs == getTrueCond())
        return getFalseCond();
    else if (lhs == getFalseCond())
        return getTrueCond();
    else
        return Cudd_Not(lhs);
}
//@}

/*!
 * Utilities for dumping conditions. These methods use global functions from CUDD
 * package and they can be removed outside this class scope to be used by others.
 */
void BddCondManager::ddClearFlag(DdNode * f) const
{
    if (!Cudd_IsComplement(f->next))
        return;
    /* Clear visited flag. */
    f->next = Cudd_Regular(f->next);
    if (cuddIsConstant(f))
        return;
    ddClearFlag(cuddT(f));
    ddClearFlag(Cudd_Regular(cuddE(f)));
    return;
}

void BddCondManager::BddSupportStep(DdNode * f, NodeBS &support) const
{
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
        return;

    support.set(f->index);

    BddSupportStep(cuddT(f), support);
    BddSupportStep(Cudd_Regular(cuddE(f)), support);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);
}

void BddCondManager::extractSubConds(DdNode * f, NodeBS &support) const
{
    BddSupportStep( Cudd_Regular(f), support);
    ddClearFlag(Cudd_Regular(f));
}

/*!
 * Dump BDD
 */
void BddCondManager::dump(DdNode* lhs, raw_ostream & O)
{
    if (lhs == getTrueCond())
        O << "T";
    else
    {
        NodeBS support;
        extractSubConds(lhs, support);
        for (NodeBS::iterator iter = support.begin(); iter != support.end();
                ++iter)
        {
            unsigned rid = *iter;
            O << rid << " ";
        }
    }
}

/*!
 * Dump BDD
 */
std::string BddCondManager::dumpStr(DdNode* lhs) const
{
    std::string str;
    if (lhs == getTrueCond())
        str += "T";
    else
    {
        NodeBS support;
        extractSubConds(lhs, support);
        for (NodeBS::iterator iter = support.begin(); iter != support.end();
                ++iter)
        {
            unsigned rid = *iter;
            char int2str[16];
            sprintf(int2str, "%d", rid);
            str += int2str;
            str += " ";
        }
    }
    return str;
}

