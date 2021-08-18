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
    trueCond = new CondExpr(cxt.bool_val(true));
    falseCond = new CondExpr(cxt.bool_val(false));
}

/// Destructor
CondManager::~CondManager()
{
    delete trueCond;
    delete falseCond;
    for (CondExpr *cond : allocatedConds)
    {
        delete cond;
    }
}

/// And operator for two expressions
CondExpr* CondManager::AND(const CondExpr *lhs, const CondExpr *rhs)
{
    CondExpr *cond = new CondExpr(lhs->getExpr() && rhs->getExpr());
    allocatedConds.insert(cond);
    return cond;
}

/// Or operator for two expressions
CondExpr* CondManager::OR(const CondExpr *lhs, const CondExpr *rhs)
{
    CondExpr *cond = new CondExpr(lhs->getExpr() || rhs->getExpr());
    allocatedConds.insert(cond);
    return cond;
}

/// Neg operator for an expression
CondExpr* CondManager::NEG(const CondExpr *lhs)
{
    CondExpr *cond = new CondExpr(!lhs->getExpr());
    allocatedConds.insert(cond);
    return cond;
}

/// Create a single condition
CondExpr* CondManager::createCond(u32_t i)
{
    CondExpr *cond = new CondExpr(cxt.bv_const(std::to_string(i).c_str(), i));
    allocatedConds.insert(cond);
    return cond;
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

}

/// Print out one particular expression
inline void CondManager::printDbg(const CondExpr *e)
{
    //std::cout << e->getExpr() << "\n";
}

/// Return string format of this expression
std::string CondManager::dumpStr(const CondExpr *e) const
{
    //std::ostringstream out;
    //out << e->getExpr();
    //return out.str();
    return "";
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

