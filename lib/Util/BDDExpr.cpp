//===- BDDExpr.cpp -- Context/path conditions in the form of BDDs----------//
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
#include "Util/BDDExpr.h"

using namespace SVF;
using namespace SVFUtil;


BDDExprManager::BDDExpr *BDDExprManager::trueCond = nullptr;
BDDExprManager::BDDExpr *BDDExprManager::falseCond = nullptr;
BDDExprManager *BDDExprManager::bddExprMgr = nullptr;
u32_t BDDExprManager::totalCondNum = 0;

BDDExprManager::BDDExprManager()
{
    m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    trueCond = BddOne();
    falseCond = BddZero();
}

BDDExprManager::~BDDExprManager()
{
    Cudd_Quit(m_bdd_mgr);
}

BDDExprManager *BDDExprManager::getBDDExprMgr()
{
    if (bddExprMgr == nullptr)
    {
        bddExprMgr = new BDDExprManager();
    }
    return bddExprMgr;
}

BDDExprManager::BDDExpr *BDDExprManager::createFreshBranchCond(const Instruction *inst)
{
    u32_t condCountIdx = totalCondNum++;
    BDDExpr *bddCond = createCond(condCountIdx);
    setCondInst(bddCond, inst);
    auto *negCond = NEG(bddCond);
    setCondInst(negCond, inst);
    return bddCond;
}

/// Operations on conditions.
//@{
/// use Cudd_bddAndLimit interface to avoid bdds blow up
BDDExprManager::BDDExpr *BDDExprManager::AND(BDDExpr *lhs, BDDExpr *rhs)
{
    if (lhs == getFalseCond() || rhs == getFalseCond())
        return getFalseCond();
    else if (lhs == getTrueCond())
        return rhs;
    else if (rhs == getTrueCond())
        return lhs;
    else
    {
        BDDExpr *tmp = Cudd_bddAndLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
        if (tmp == nullptr)
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
BDDExprManager::BDDExpr *BDDExprManager::OR(BDDExpr *lhs, BDDExpr *rhs)
{
    if (lhs == getTrueCond() || rhs == getTrueCond())
        return getTrueCond();
    else if (lhs == getFalseCond())
        return rhs;
    else if (rhs == getFalseCond())
        return lhs;
    else
    {
        BDDExpr *tmp = Cudd_bddOrLimit(m_bdd_mgr, lhs, rhs, Options::MaxBddSize);
        if (tmp == nullptr)
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

BDDExprManager::BDDExpr *BDDExprManager::NEG(BDDExpr *lhs)
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
 * Whether **All Paths** are reachable
 */
bool BDDExprManager::isAllPathReachable(const BDDExpr *e)
{
    return isEquivalentBranchCond(e, getTrueCond());
}

/*!
 * Utilities for dumping conditions. These methods use global functions from CUDD
 * package and they can be removed outside this class scope to be used by others.
 */
void BDDExprManager::ddClearFlag(BDDExpr *f) const
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

void BDDExprManager::BddSupportStep(BDDExpr *f, NodeBS &support) const
{
    if (cuddIsConstant(f) || Cudd_IsComplement(f->next))
        return;

    support.set(f->index);

    BddSupportStep(cuddT(f), support);
    BddSupportStep(Cudd_Regular(cuddE(f)), support);
    /* Mark as visited. */
    f->next = Cudd_Complement(f->next);
}

void BDDExprManager::extractSubConds(const BDDExpr *f, NodeBS &support) const
{
    BddSupportStep(Cudd_Regular(f), support);
    ddClearFlag(Cudd_Regular(f));
}

/*!
 * Dump BDD
 */
void BDDExprManager::dump(const BDDExpr *lhs, OutStream &O)
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
std::string BDDExprManager::dumpStr(const BDDExpr *e) const
{
    std::string str;
    if (e == getTrueCond())
        str += "T";
    else
    {
        NodeBS support;
        extractSubConds(e, support);
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


