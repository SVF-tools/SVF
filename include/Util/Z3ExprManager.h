//===- LeakChecker.h -- Detecting memory leaks--------------------------------//
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
 * Z3ExprManager.h
 *
 *  Created on: Jul 15, 2022
 *      Author: Yulei, Xiao and Jiawei
 */

#ifndef SVF_Z3EXPRMANAGER_H
#define SVF_Z3EXPRMANAGER_H

#include "Util/BasicTypes.h"
#include "Util/Z3Expr.h"
#include <cstdio>

namespace SVF {
    class Z3ExprManager {
    public:
        typedef Map<u32_t, const Instruction *> IndexToTermInstMap;
        typedef Z3Expr Condition;

    private:
        static Z3ExprManager *z3ExprMgr;
        IndexToTermInstMap idToTermInstMap; //key: z3 expression id, value: instruction
        NodeBS negConds;
        z3::solver sol;
        std::vector<Z3Expr> z3ExprVec;

        // get Z3 sovler
        z3::solver getSolver() {
            return sol;
        }

        // get Z3 context
        static z3::context &getContext() {
            return Z3Expr::getContext();
        }

        // Constructor
        Z3ExprManager();

    public:
        static u32_t totalCondNum; // a counter for fresh condition
        static Z3ExprManager *getZ3ExprMgr(); // get menager

        /// Destructor
        virtual ~Z3ExprManager();


        static void releaseZ3ExprMgr() {
            delete z3ExprMgr;
            z3ExprMgr = nullptr;
        }

        /// Create a fresh condition to encode each program branch
        virtual Z3Expr createFreshBranchCond(const Instruction *inst);


        static inline bool classof(const Z3Expr *) {
            return true;
        }

        /// Return the number of condition expressions
        virtual inline u32_t getCondNumber() {
            return this->totalCondNum;
        }

        /// Return the unique true condition
        inline Z3Expr getTrueCond() const {
            return Z3Expr::getContext().bool_val(true);
        }

        // get the number of subexpression of a Z3 expression
        u32_t getExprSize(const Z3Expr &z3Expr);

        /// Return the unique false condition
        inline Z3Expr getFalseCond() const {
            return Z3Expr::getContext().bool_val(false);
        }

        /// Get/Set instruction based on Z3 expression id
        //{@
        inline const Instruction *getCondInst(u32_t id) const {
            IndexToTermInstMap::const_iterator it = idToTermInstMap.find(id);
            assert(it != idToTermInstMap.end() && "this should be a fresh condition");
            return it->second;
        }

        inline void setCondInst(const Z3Expr &z3Expr, const Instruction *inst) {
            assert(idToTermInstMap.find(z3Expr.id()) == idToTermInstMap.end() && "this should be a fresh condition");
            idToTermInstMap[z3Expr.id()] = inst;
        }
        //@}

        // mark neg Z3 expression
        inline void setNegCondInst(const Z3Expr &z3Expr, const Instruction *inst) {
            setCondInst(z3Expr, inst);
            negConds.set(z3Expr.id());
        }

        /// Operations on conditions.

        //@{
        virtual Z3Expr AND(const Z3Expr &lhs, const Z3Expr &rhs);

        virtual Z3Expr OR(const Z3Expr &lhs, const Z3Expr &rhs);

        virtual Z3Expr NEG(const Z3Expr &z3Expr);
        //@}

        virtual bool isNegCond(u32_t id) const {
            return negConds.test(id);
        }

        /// Whether the condition is satisfiable
        virtual bool isSatisfiable(const Z3Expr &z3Expr);

        /// Whether lhs and rhs are equivalent branch conditions
        virtual bool isEquivalentBranchCond(const Z3Expr &lhs, const Z3Expr &rhs);

        /// Whether **All Paths** are reachable
        bool isAllPathReachable(const Z3Expr &z3Expr);


        inline std::string getMemUsage() {
            return "";
        }

        // extract subexpression from a Z3 expression
        void extractSubConds(const Z3Expr &z3Expr, NodeBS &support) const;

        // output Z3 expression as a string
        std::string dumpStr(const Z3Expr &z3Expr) const;


    };

}


#endif //SVF_Z3EXPRMANAGER_H
