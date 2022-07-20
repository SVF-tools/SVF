//
// Created by Jiawei Ren on 2022/7/15.
//

#ifndef SVF_Z3EXPRMANAGER_H
#define SVF_Z3EXPRMANAGER_H

#include "Util/BasicTypes.h"
#include "Util/Z3Expr.h"
#include <cstdio>

namespace SVF {
    class Z3ExprManager {
    public:
        typedef Map<u32_t, const Instruction *> IndexToTermInstMap;
//        typedef Map<u32_t, Z3Expr *> IndexToExprMap;
        typedef Z3Expr Condition;


    private:
        static Z3ExprManager *z3ExprMgr;
        IndexToTermInstMap idToTermInstMap;
//        IndexToExprMap idToExprMap;
        NodeBS negConds;

        static z3::context &getContext() {
            return Z3Expr::getContext();
        }

        // Constructor
        Z3ExprManager();

    public:
        static u32_t totalCondNum; // a counter for fresh condition
        static Z3ExprManager *getZ3ExprMgr();

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

        /// Return the unique false condition
        inline Z3Expr getFalseCond() const {
            return Z3Expr::getContext().bool_val(false);
        }

        /// Get/Set llvm conditional expression
        //{@
        inline const Instruction *getCondInst(u32_t id) const {
            IndexToTermInstMap::const_iterator it = idToTermInstMap.find(id);
            assert(it != idToTermInstMap.end() && "this should be a fresh condition");
            return it->second;
        }

        inline void setCondInst(const Z3Expr &z3Expr, const Instruction *inst) {
            assert(idToTermInstMap.find(z3Expr.hash()) == idToTermInstMap.end() && "this should be a fresh condition");
            idToTermInstMap[z3Expr.hash()] = inst;
        }
        //@}

        inline void setNegCondInst(const Z3Expr &z3Expr, const Instruction *inst) {
            setCondInst(z3Expr, inst);
            negConds.set(z3Expr.hash());
        }


        /// Operations on conditions.

        //@{
        virtual Z3Expr AND(const Z3Expr &lhs, const Z3Expr &rhs);

        virtual Z3Expr OR(const Z3Expr &lhs, const Z3Expr &rhs);

        virtual Z3Expr NEG(const Z3Expr &lhs);
        //@}

//        /// Given an id, get its condition
//        Z3Expr *getCond(u32_t i) const {
//            auto it = idToExprMap.find(i);
//            assert(it != idToExprMap.end() && "condition not found!");
//            return it->second;
//        }

        virtual bool isNegCond(u32_t id) const {
            return negConds.test(id);
        }

        /// Whether the condition is satisfiable
        virtual bool isSatisfiable(const Z3Expr &z3Expr);

        /// Whether lhs and rhs are equivalent branch conditions
        virtual bool isEquivalentBranchCond(const Z3Expr &lhs, const Z3Expr &rhs);

        /// Whether **All Paths** are reachable
        bool isAllPathReachable(const Z3Expr &z3Expr);

//        Z3Expr *getOrAddZ3Cond(const Z3Expr &z3Expr);

        inline std::string getMemUsage() {
            return "";
        }

        void extractSubConds(const Z3Expr &z3Expr, NodeBS &support) const;

        std::string dumpStr(const Z3Expr &z3Expr) const;


    };

}


#endif //SVF_Z3EXPRMANAGER_H
