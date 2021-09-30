//===- Conditions.h -- Context/Path BDD conditions----------------------------//
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
 * BitVectorCond.h
 *
 *  Created on: 31/10/2013
 *      Author: yesen
 */

#ifndef BITVECTORCOND_H_
#define BITVECTORCOND_H_

#include "Util/BasicTypes.h"
#include <stdio.h>

#include "CUDD/cuddInt.h"
#include "z3++.h"

namespace SVF
{

class CondExpr{

public:

    CondExpr(const z3::expr& _e): e(_e){
    }
    const z3::expr& getExpr() const{
        return e;
    }

    /// get/insert/set condition hash to branch conditions map
    //{@
    Map<u32_t, z3::expr> getBrExprMap() const{
        return breMap;
    }
    void insertBrExpr(const z3::expr& er) {
        breMap.insert(std::make_pair(er.id(), er));
    }
    void setBrExprMap(Map<u32_t, z3::expr> _bre){
        breMap = std::move(_bre);
    }
    //@}

    /// get/insert/set switch values
    //{@
    Map<u32_t, std::vector<uint64_t>> getSwitchValuesExprMap() const{
        return switchValuesMap;
    }
    void insertSwitchValue(const z3::expr& er, const std::vector<uint64_t>& v) {
        switchValuesMap.insert(std::make_pair(er.id(), v));
    }
    void setSwitchValuesMap(Map<u32_t, std::vector<uint64_t>> _switchValuesMap){
        switchValuesMap = std::move(_switchValuesMap);
    }
    //@}

    // branch condition is the same
    bool operator==(const CondExpr &condExpr) const {
        return z3::eq(getExpr(), condExpr.getExpr());
    }

private:
    z3::expr e;
    Map<u32_t, z3::expr> breMap; ///< mapping branch condition's hash to branch condition (Note: only store left value for switch)
    Map<u32_t, std::vector<uint64_t>> switchValuesMap; ///< mapping switch condition's hash to all possible right values (aka cases)

};

class CondManager{

public:
    /// Constructor
    CondManager();

    /// Destructor
    ~CondManager();

    /// Create a single condition
    CondExpr* createCond(u32_t i);

    /// Create a single condition
    CondExpr* createCond(const z3::expr& i);

    /// Return the number of condition expressions
    u32_t getCondNumber();

    /// Return the unique true condition
    inline static CondExpr* getTrueCond() 
    {
        return trueCond;
    }
    /// Return the unique false condition
    inline static CondExpr* getFalseCond() 
    {
        return falseCond;
    }

    /// simplify
    z3::expr simplify(const z3::expr& expr);
    /// Operations on conditions.
    //@{
    CondExpr* AND(const CondExpr* lhs, const CondExpr* rhs);
    CondExpr* OR(const CondExpr* lhs, const CondExpr* rhs);
    CondExpr* NEG(const CondExpr* lhs);
    //@}

    /// Return memory usage for this condition manager
    std::string getMemUsage();

    /// Dump out all expressions
    void printModel();

    /// Print out one particular expression
    void printDbg(const CondExpr* e);

    /// Return string format of this expression
    std::string dumpStr(const CondExpr* e) const;

    /// Extract sub conditions of this expression
    void extractSubConds(const z3::expr& e,  NodeBS &support) const;

    /// whether condition is satisfiable
    bool isSatisfiable(const z3::expr& e);

    inline z3::context& getctx(){
        return cxt;
    }

    inline CondExpr* getCond(u32_t h) {
        if (allocatedConds.count(h))
            return allocatedConds[h];
        else
            return nullptr;
    }

private:
    z3::context cxt;
    z3::solver sol;
    static CondExpr* trueCond;
    static CondExpr* falseCond;
//    Set<CondExpr*> allocatedConds;
    Map<u32_t, CondExpr*> allocatedConds;
};


/**
 * Using Cudd as conditions.
 */
class BddCondManager
{
public:

    /// Constructor
    BddCondManager()
    {
        m_bdd_mgr = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    }

    /// Destructor
    ~BddCondManager()
    {
        Cudd_Quit(m_bdd_mgr);
    }

    DdNode* createCond(u32_t i)
    {
        return Cudd_bddIthVar(m_bdd_mgr, i);
    }
    inline DdNode* getTrueCond() const
    {
        return BddOne();
    }
    inline DdNode* getFalseCond() const
    {
        return BddZero();
    }

    inline std::string getMemUsage()
    {
        return std::to_string(Cudd_ReadMemoryInUse(m_bdd_mgr));
    }
    inline u32_t getCondNumber()
    {
        return Cudd_ReadNodeCount(m_bdd_mgr);
    }
    /// Operations on conditions.
    //@{
    DdNode* AND(DdNode* lhs, DdNode* rhs);
    DdNode* OR(DdNode* lhs, DdNode* rhs);
    DdNode* NEG(DdNode* lhs);
    //@}

    /**
     * Utilities for dumping conditions. These methods use global functions from CUDD
     * package and they can be removed outside this class scope to be used by others.
     */
    void ddClearFlag(DdNode * f) const;
    void BddSupportStep( DdNode * f,  NodeBS &support) const;
    void extractSubConds( DdNode * f,  NodeBS &support) const;
    void dump(DdNode* lhs, raw_ostream & O);
    std::string dumpStr(DdNode* lhs) const;

private:
    inline DdNode* BddOne() const
    {
        return Cudd_ReadOne(m_bdd_mgr);
    }
    inline DdNode* BddZero() const
    {
        return Cudd_ReadLogicZero(m_bdd_mgr);
    }

    DdManager *m_bdd_mgr;
};

} // End namespace SVF

#endif /* BITVECTORCOND_H_ */
