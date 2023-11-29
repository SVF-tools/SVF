//
// Created by rjw on 11/28/23.
//
//
// Created by rjw on 11/28/23.
//
#include <stdio.h>
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/Z3Expr.h"
#include "AbstractExecution/RelationSolver.h"
#include "AbstractExecution/RelExeState.h"
#include "AbstractExecution/IntervalExeState.h"
using namespace SVF;
using namespace SVFUtil;

//class RelExeStateExample : public RelExeState, public IntervalExeState {
//public:
//
//    RelExeStateExample() = default;
//
//    ~RelExeStateExample() override{
//        RelExeState::_varToVal.clear();
//        IntervalExeState::_varToItvVal.clear();
//    }
//    static z3::context &getContext() {
//        return Z3Expr::getContext();
//    }
//
//    void testRelExeState() {
//        // var0 := [0, 10];
//        RelExeState::_varToVal[0] = getContext().int_const("0");
//        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 10);
//        // var1 := var0 + 1;
//        RelExeState::_varToVal[1] = getContext().int_const("0") + 1;
//        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0] + IntervalValue(1);
//        // var2 := var1 < 5;
//        RelExeState::_varToVal[2] = ite(getContext().int_const("1") < 5, Z3Expr(1), Z3Expr(0));
//        IntervalExeState::_varToItvVal[2] = IntervalExeState::_varToItvVal[1] < IntervalValue(5);
//
//        // Test extract sub vars
//        Set<u32_t> res;
//        extractSubVars(RelExeState::_varToVal[1], res);
//        assert(res == Set<u32_t>({0}));
//        // Test extract cmp vars
//        Set<u32_t> cmpRes;
//        extractCmpVars(RelExeState::_varToVal[2], cmpRes);
//        assert(cmpRes == Set<u32_t>({0, 1}));
//        // br var2, br1
//        // Test branch
//        Set<u32_t> vars, initVars;
//        const Z3Expr &relExpr = buildRelZ3Expr(2, 1, vars, initVars); /// id, true, all candidate, initial value
//        IntervalExeState inv = std::move(sliceState(vars));
//        const Z3Expr &initExpr = inv.gamma_hat(*initVars.begin());
//        const Z3Expr &phi = (relExpr && initExpr).simplify();
//        IntervalExeState resRSY = RelationSolver::bilateral(inv, phi);
//        /// 0:[0,3] 1:[1,4] 2:[1,1]
//        for(auto &item :resRSY.getVarToVal()){
//            std::cout << item.first << ": " << item.second <<"\n";
//        }
//        //        IntervalExeState::VarToValMap intendedRes = Map<u32_t, IntervalValue>({{0, IntervalValue(0,3)},{1, IntervalValue(1,4)},{2,IntervalValue(1,1)}});
//        //        for(auto &item :intendedRes){
//        //            std::cout << item.first << item.second <<"\n";
//        //        }
//        //        assert(resRSY.getVarToVal() == intendedRes);
//        Z3Expr gt = ((toZ3Expr(1) == getZ3Expr(1))
//                     && toZ3Expr(2) == getZ3Expr(2)
//                     && getZ3Expr(2) == 1).simplify();
//        Z3Expr::getSolver().add((gt != relExpr).getExpr());
//        assert(vars == Set<u32_t>({1, 2, 0}) && initVars == Set<u32_t>({1}) &&
//               Z3Expr::getSolver().check() == z3::unsat);
//
//        Z3Expr::releaseSolver();
//        Z3Expr::releaseContext();
//
//
//    }
//};

void test1()
{
    outs() << "hello\n";
    IntervalExeState::VarToValMap m;
    // var0 : [0,1]
    m[0] = IntervalValue(0,1);
    Z3Expr x0 = RelExeState::getContext().int_const("0");
    // var1 : [0,1], var1 = var0 + 1
    m[1] = IntervalValue(0,1);
    Z3Expr x1 = RelExeState::getContext().int_const("0") + 1;
    outs() << "Interval operator: " << ( m[0] - m[1]).toString() << "\n";
    for (auto i :  m){
        outs() << i.first << ": " << i.second.toString() << "\n";
    }
    IntervalExeState es(m, m);
    outs() << "Interval operator: " << (es[0] - es[1]).toString() << "\n";
//    Z3Expr x3 = Z3Expr::getContext().int_const("2");

    RelExeState relExeState;
//    Set<u32_t> res;
//    relExeState.extractSubVars(x1, res);
//    assert(res == Set<u32_t>({0}));
//    outs() << "check extract success\n";
//    Set<u32_t> res2;
//    relExeState.extractSubVars(x0, res2);
//    assert(res2 == Set<u32_t>({0}));
//    outs() << "check extract success\n";
    Set<u32_t> res3;
    relExeState.extractCmpVars(x1, res3);
    assert(res3 == Set<u32_t>({0,1}));
    outs() << "check cmp success\n";
//    Z3Expr expr = x1;
//    outs() << expr.to_string() << "\n";

//    RelationSolver rs;
//    auto res = rs.RSY(es, expr);
//    for (auto i : res.getLocToVal()){
//        outs() << i.first << i.second.toString()<< "\n";
//    }
}
int main(int argc, char ** argv)
{
    test1();

    return 1;
}