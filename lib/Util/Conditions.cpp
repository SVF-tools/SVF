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
 *  Created on: Oct 19, 2021
 *      Author: Yulei and Xiao
 */

#include "Util/Options.h"
#include "Util/Conditions.h"
#include "Util/SVFUtil.h"

using namespace SVF;


CondExpr* CondManager::trueCond = nullptr;
CondExpr* CondManager::falseCond = nullptr;
CondManager* CondManager::condMgr = nullptr;
u32_t CondManager::totalCondNum = 0;

/*!
 * Constructor
 */
CondManager::CondManager() : sol(cxt)
{
    const z3::expr &trueExpr = cxt.bool_val(true);
    trueCond = getOrAddBranchCond(trueExpr, branchCondManager.getTrueCond());
    const z3::expr &falseExpr = cxt.bool_val(false);
    falseCond = getOrAddBranchCond(falseExpr, branchCondManager.getFalseCond());
}

/*!
 * Destructor
 */
CondManager::~CondManager()
{
    for (const auto& it : allocatedConds)
    {
        delete it.second;
    }
}

/*!
 *  Preprocess the condition,
 *  e.g., Compressing using And-Inverter-Graph, Gaussian Elimination
 */
z3::expr CondManager::simplify(const z3::expr& expr) const{
    z3::goal g(expr.ctx());
    z3::tactic qe =
            z3::tactic(expr.ctx(), "aig");
    g.add(expr);
    z3::apply_result r = qe(g);
    z3::expr res(expr.ctx().bool_val(false));
    for (u32_t i = 0; i < r.size(); ++i) {
        if (res.is_false()) {
            res = r[i].as_expr();
        } else {
            res = res || r[i].as_expr();
        }
    }
    return res;
}



/*!
 * Create a fresh condition to encode each program branch
 */
CondExpr* CondManager::createFreshBranchCond(const Instruction* inst)
{
    u32_t condCountIdx = totalCondNum++;
    const z3::expr &expr = cxt.bool_const(("c" + std::to_string(condCountIdx)).c_str());
    IDToCondExprMap::const_iterator it = allocatedConds.find(expr.id());
    if (it != allocatedConds.end())
        return it->second;
    else{
        BranchCond *branchCond = branchCondManager.createCond(condCountIdx);
        auto *cond = new BranchCondExpr(expr, branchCond);
        auto *negCond = NEG(cond);
        setCondInst(cond, inst);
        setCondInst(negCond, inst);
        branchCondToCondExpr.emplace(branchCond, cond);
        return allocatedConds.emplace(expr.id(), cond).first->second;
    }
}

/*!
 * Get or add a single branch condition,
 * e.g., when doing condition conjunction
 */
CondExpr* CondManager::getOrAddBranchCond(const z3::expr& e, BranchCond* branchCond)
{
    auto it = branchCondToCondExpr.find(branchCond);
    if(it != branchCondToCondExpr.end())
        return it->second;
    else{
        auto *cond = new BranchCondExpr(e, branchCond);
        branchCondToCondExpr.emplace(branchCond, cond);
        return allocatedConds.emplace(e.id(), cond).first->second;
    }
}

/*!
 * Return the number of condition expressions
 */
u32_t CondManager::getCondNumber()
{
    return sol.get_model().size();
}
/// Operations on conditions.
//@{
CondExpr* CondManager::AND(CondExpr* lhs, CondExpr* rhs){
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return rhs;
    else if (rhs == getTrueCond())
        return lhs;
    else {
        BranchCond *branchCond = branchCondManager.AND(SVFUtil::dyn_cast<BranchCondExpr>(lhs)->getBranchCond(),
                                                       SVFUtil::dyn_cast<BranchCondExpr>(rhs)->getBranchCond());
        const z3::expr &expr = lhs->getExpr() && rhs->getExpr();
        return getOrAddBranchCond(expr, branchCond);
    }
}

CondExpr* CondManager::OR(CondExpr* lhs, CondExpr* rhs){
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return rhs;
    else if (rhs == getFalseCond())
        return lhs;
    else{
        BranchCond *branchCond = branchCondManager.OR(SVFUtil::dyn_cast<BranchCondExpr>(lhs)->getBranchCond(),
                                              SVFUtil::dyn_cast<BranchCondExpr>(rhs)->getBranchCond());
        const z3::expr &expr = lhs->getExpr() || rhs->getExpr();
        return getOrAddBranchCond(expr, branchCond);
    }
}
CondExpr* CondManager::NEG(CondExpr* lhs){
    if (lhs == getTrueCond())
        return getFalseCond();
    else if (lhs == getFalseCond())
        return getTrueCond();
    else{
        BranchCond *branchCond = branchCondManager.NEG(SVFUtil::dyn_cast<BranchCondExpr>(lhs)->getBranchCond());
        const z3::expr &expr = !lhs->getExpr();
        return getOrAddBranchCond(expr, branchCond);
    }
}
//@}

/*!
 * Print the expressions in this model
 */
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

/*!
 * Return memory usage for this condition manager
 */
std::string CondManager::getMemUsage()
{
    //std::ostringstream os;
    //memory::display_max_usage(os);
    //return os.str();
    return "";
}

/*!
 * Extract sub conditions of this expression
 */
void CondManager::extractSubConds(const CondExpr* cond, NodeBS &support) const
{
    if (cond->getExpr().num_args() == 0)
        if (!cond->getExpr().is_true() && !cond->getExpr().is_false())
            support.set(cond->getExpr().id());
    for (u32_t i = 0; i < cond->getExpr().num_args(); ++i) {
        const z3::expr &expr = cond->getExpr().arg(i);
        extractSubConds(getCond(expr.id()), support);
    }
}

/*!
 * Whether the condition is satisfiable
 */
bool CondManager::isSatisfiable(const CondExpr* cond){
    sol.reset();
    sol.add(cond->getExpr());
    z3::check_result result = sol.check();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

/*!
 * Whether **All Paths** are reachable
 */
bool CondManager::isAllPathReachable(const CondExpr* e){
    return isEquivalentBranchCond(e, getTrueCond());
}

/*!
 * Print out one particular expression
 */
inline void CondManager::printDbg(const CondExpr *e)
{
    std::cout << e->getExpr() << "\n";
}

/*!
 * Return string format of this expression
 */
std::string CondManager::dumpStr(const CondExpr *e) const
{
    std::ostringstream out;
    out << e->getExpr();
    return out.str();
}

/// Operations on conditions.
//@{
/// use Cudd_bddAndLimit interface to avoid bdds blow up
BranchCondManager::BranchCond* BranchCondManager::AND(BranchCond* lhs, BranchCond* rhs)
{
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return rhs;
    else if (rhs == getTrueCond())
        return lhs;
    else
    {
        BranchCond* tmp = Cudd_bddAndLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
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
BranchCondManager::BranchCond* BranchCondManager::OR(BranchCond* lhs, BranchCond* rhs)
{
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return rhs;
    else if (rhs == getFalseCond())
        return lhs;
    else
    {
        BranchCond* tmp = Cudd_bddOrLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
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

BranchCondManager::BranchCond* BranchCondManager::NEG(BranchCond* lhs)
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
void BranchCondManager::ddClearFlag(BranchCond * f) const
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

void BranchCondManager::BddSupportStep(BranchCond * f, NodeBS &support) const
{
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
        return;

    support.set(f->index);

    BddSupportStep(cuddT(f), support);
    BddSupportStep(Cudd_Regular(cuddE(f)), support);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);
}

void BranchCondManager::extractSubConds(BranchCond * f, NodeBS &support) const
{
    BddSupportStep( Cudd_Regular(f), support);
    ddClearFlag(Cudd_Regular(f));
}

/*!
 * Dump BDD
 */
void BranchCondManager::dump(BranchCond* lhs, raw_ostream & O)
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
std::string BranchCondManager::dumpStr(BranchCond* lhs) const
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

