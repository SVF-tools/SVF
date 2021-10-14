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

bool CondExpr::operator==(const CondExpr &condExpr) const {
    if(getExpr().id() == condExpr.getId())
        return true;
    CondManager *condMgr = CondManager::getCondMgr();
    return condMgr->isEquivalentCond(condMgr->getExistingCond(getExpr()), condMgr->getExistingCond(condExpr.getExpr()));
}

CondExpr* CondManager::trueCond = nullptr;
CondExpr* CondManager::falseCond = nullptr;
CondManager* CondManager::condMgr = nullptr;

/// Constructor
CondManager::CondManager() : sol(cxt)
{
    const z3::expr &trueExpr = cxt.bool_val(true);
    trueCond = new CondExpr(trueExpr);
    allocatedConds[trueExpr.id()] = trueCond;
    const z3::expr &falseExpr = cxt.bool_val(false);
    falseCond = new CondExpr(falseExpr);
    allocatedConds[falseExpr.id()] = falseCond;
}

/// Destructor
CondManager::~CondManager()
{
    for (const auto& it : allocatedConds)
    {
        delete it.second;
    }
}

/// Simplify
z3::expr CondManager::simplify(const z3::expr& expr) const{
    z3::goal g(expr.ctx());
    z3::tactic qe(expr.ctx(), "ctx-solver-simplify");
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



/// Create a single condition
CondExpr* CondManager::createCondForBranch(u32_t i)
{
    const z3::expr &expr = cxt.bool_const(("c" + std::to_string(i)).c_str());
    IDToCondExprMap::const_iterator it = allocatedConds.find(expr.id());
    if (it != allocatedConds.end())
        return it->second;
    else{
        auto *cond = new CondExpr(expr);
        return allocatedConds.emplace(expr.id(), cond).first->second;
    }
}

/// new a single condition
CondExpr* CondManager::getExistingCond(const z3::expr& e) const
{
    auto it = allocatedConds.find(e.id());
    if(it != allocatedConds.end())
        return it->second;
    else
        return nullptr;
}

/// new a single condition
CondExpr* CondManager::createNewCond(const z3::expr& e)
{
    IDToCondExprMap::const_iterator it = allocatedConds.find(e.id());
    if (it != allocatedConds.end())
        return it->second;
    else{
        auto *cond = new CondExpr(e);
        return allocatedConds.emplace(e.id(), cond).first->second;
    }
}

/// Return the number of condition expressions
u32_t CondManager::getCondNumber()
{
    return sol.get_model().size();
}

CondExpr* CondManager::AND(const CondExpr* lhs, const CondExpr* rhs){
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return getExistingCond(rhs->getExpr());
    else if (rhs == getTrueCond())
        return getExistingCond(lhs->getExpr());
    else {
        const z3::expr &expr = lhs->getExpr() && rhs->getExpr();
        if (CondExpr *cond = getExistingCond(expr))
            return cond;
        else
            return createNewCond(expr);
    }
}

CondExpr* CondManager::OR(const CondExpr* lhs, const CondExpr* rhs){
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return getExistingCond(rhs->getExpr());
    else if (rhs == getFalseCond())
        return getExistingCond(lhs->getExpr());
    else{
        const z3::expr &expr = lhs->getExpr() || rhs->getExpr();
        if (CondExpr *cond = getExistingCond(expr))
            return cond;
        else
            return createNewCond(expr);
    }
}
CondExpr* CondManager::NEG(const CondExpr* lhs){
    if (lhs == getTrueCond())
        return getFalseCond();
    else if (lhs == getFalseCond())
        return getTrueCond();
    else{
        const z3::expr &expr = !lhs->getExpr();
        if (CondExpr *cond = getExistingCond(expr))
            return cond;
        else
            return createNewCond(expr);
    }
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
void CondManager::extractSubConds(const z3::expr& e, NodeBS &support) const
{
    if (e.num_args() == 0)
        if (!e.is_true() && !e.is_false())
            support.set(e.id());
    for (u32_t i = 0; i < e.num_args(); ++i) {
        const z3::expr &expr = e.arg(i);
        extractSubConds(expr, support);
    }
}

/*!
 * whether z3 condition e is satisfiable
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

bool CondManager::isEquivalentCond(const CondExpr* lhs, const CondExpr* rhs){
    sol.reset();
    sol.add(lhs->getExpr() != rhs->getExpr());
    return sol.check() == z3::unsat;
}

/*!
 * whether the conditions of **All Paths** are satisfiable
 *
 * we first build a truth table of the unique boolean identifiers of the condition e,
 * and then check whether e is all satisfiable under each row of the truth table
 */
bool CondManager::isAllSatisfiable(const CondExpr* e){
    return isEquivalentCond(e, getTrueCond());
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

