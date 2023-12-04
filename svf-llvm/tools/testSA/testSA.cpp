//
// Created by rjw on 11/28/23.
//
//
// Created by rjw on 11/28/23.
//
#include "AbstractExecution/IntervalExeState.h"
#include "AbstractExecution/RelExeState.h"
#include "AbstractExecution/RelationSolver.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/Z3Expr.h"
#include <stdio.h>
using namespace SVF;
using namespace SVFUtil;

class RelExeStateExample : public RelExeState, public IntervalExeState
{
public:
    RelExeStateExample() = default;

    ~RelExeStateExample() override
    {
        RelExeState::_varToVal.clear();
        IntervalExeState::_varToItvVal.clear();
    }

    static z3::context& getContext()
    {
        return Z3Expr::getContext();
    }

    void test_print()
    {
        SVFUtil::outs() << "hello print\n";
    }

    // void testRelExeState()
    // {
    //     SVFUtil::outs() << "test start\n";
    //     // var0 := [0, 10];
    //     RelExeState::_varToVal[0] = getContext().int_const("0");
    //     IntervalExeState::_varToItvVal[0] = IntervalValue(0, 10);
    //     // var1 := var0 + 1;
    //     RelExeState::_varToVal[1] = getContext().int_const("0") + 1;
    //     IntervalExeState::_varToItvVal[1] =
    //         IntervalExeState::_varToItvVal[0] + IntervalValue(1);
    //     // var2 := var1 < 5;
    //     RelExeState::_varToVal[2] =
    //         ite(getContext().int_const("1") < 5, Z3Expr(1), Z3Expr(0));
    //     IntervalExeState::_varToItvVal[2] =
    //         IntervalExeState::_varToItvVal[1] < IntervalValue(5);
    //
    //     // Test extract sub vars
    //     Set<u32_t> res;
    //     extractSubVars(RelExeState::_varToVal[1], res);
    //     assert(res == Set<u32_t>({0}));
    //     SVFUtil::outs() << "test start\n";
    //     // Test extract cmp vars
    //     Set<u32_t> cmpRes;
    //     extractCmpVars(RelExeState::_varToVal[2], cmpRes);
    //     assert(cmpRes == Set<u32_t>({0, 1}));
    //     SVFUtil::outs() << "test start\n";
    //     // br var2, br1
    //     // Test branch
    //     Set<u32_t> vars, initVars;
    //     const Z3Expr &relExpr = buildRelZ3Expr(2, 1, vars, initVars); /// id, true, all candidate, initial value
    //
    //     IntervalExeState inv = std::move(sliceState(vars));
    //     SVFUtil::outs() << "test start\n";
    //     // const Z3Expr &initExpr = inv.gamma_hat(*initVars.begin());
    //     // const Z3Expr &phi = (relExpr && initExpr).simplify();
    //     // IntervalExeState resRSY = RelationSolver::bilateral(inv, phi);
    //     //     /// 0:[0,3] 1:[1,4] 2:[1,1]
    //     //     for(auto &item :resRSY.getVarToVal()){
    //     //         std::cout << item.first << ": " << item.second <<"\n";
    //     //     }
    //     //     //        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
    //     //     IntervalValue>({{0, IntervalValue(0,3)},{1,
    //     //     IntervalValue(1,4)},{2,IntervalValue(1,1)}});
    //     //     //        for(auto &item :intendedRes){
    //     //     //            std::cout << item.first << item.second <<"\n";
    //     //     //        }
    //     //     //        assert(resRSY.getVarToVal() == intendedRes);
    //     //     Z3Expr gt = ((toZ3Expr(1) == getZ3Expr(1))
    //     //                  && toZ3Expr(2) == getZ3Expr(2)
    //     //                  && getZ3Expr(2) == 1).simplify();
    //     //     Z3Expr::getSolver().add((gt != relExpr).getExpr());
    //     //     assert(vars == Set<u32_t>({1, 2, 0}) && initVars ==
    //     //     Set<u32_t>({1}) &&
    //     //            Z3Expr::getSolver().check() == z3::unsat);
    //     //
    //     //     Z3Expr::releaseSolver();
    //     //     Z3Expr::releaseContext();
    //     //
    //     //
    // }
    void testRelExeState()
    {
        SVFUtil::outs() << "test start\n";
        // var0 := [0, 1];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 1);
        // var1 := var0 + 1;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0") + 1;
        IntervalExeState::_varToItvVal[1] =
            IntervalExeState::_varToItvVal[0] + IntervalValue(1);
        // var2 := var1 < 5;
        RelExeState::_varToVal[2] =
            getContext().int_const("1") <= getContext().int_const("0") + 1;
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] < IntervalValue(5);

        // Test extract sub vars
        Set<u32_t> res;
        extractSubVars(RelExeState::_varToVal[1], res);
        assert(res == Set<u32_t>({0,1}));
        IntervalExeState inv = sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = RelExeState::_varToVal[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        // 0:[0,1] 1:[1,2] 2:[1,2]
        for (auto& item : resRSY.getVarToVal())
        {
            std::cout << item.first << ": " << item.second << "\n";
        }
        for (auto& item : resBilateral.getVarToVal())
        {
            std::cout << item.first << ": " << item.second << "\n";
        }
        //     //        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        //     IntervalValue>({{0, IntervalValue(0,3)},{1,
        //     IntervalValue(1,4)},{2,IntervalValue(1,1)}});
        //     //        for(auto &item :intendedRes){
        //     //            std::cout << item.first << item.second <<"\n";
        //     //        }
        //     //        assert(resRSY.getVarToVal() == intendedRes);
        //     Z3Expr gt = ((toZ3Expr(1) == getZ3Expr(1))
        //                  && toZ3Expr(2) == getZ3Expr(2)
        //                  && getZ3Expr(2) == 1).simplify();
        //     Z3Expr::getSolver().add((gt != relExpr).getExpr());
        //     assert(vars == Set<u32_t>({1, 2, 0}) && initVars ==
        //     Set<u32_t>({1}) &&
        //            Z3Expr::getSolver().check() == z3::unsat);
        //
        // Z3Expr::releaseSolver();
        // Z3Expr::releaseContext();
        //
        //
    }

    void testRelExeState2()
    {
        SVFUtil::outs() << "test start\n";
        // var0 := [0, 1];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 10);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 - var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") -
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[
                0];

        // Test extract sub vars
        Set<u32_t> res;
        extractSubVars(RelExeState::_varToVal[2], res);
        assert(res == Set<u32_t>({0,1,2}));
        IntervalExeState inv = sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = RelExeState::_varToVal[2] &&
                                RelExeState::_varToVal[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        // IntervalExeState resRSY = rs.RSY(inv, phi);
        // IntervalExeState resBilateral = rs.bilateral(inv, phi);
        // 0:[0,1] 1:[1,2] 2:[1,2]
        // for (auto& item : resRSY.getVarToVal())
        // {
            // std::cout << item.first << ": " << item.second << "\n";
        // }
        // for (auto& item : resBilateral.getVarToVal())
        // {
            // std::cout << item.first << ": " << item.second << "\n";
        // }
        Map<u32_t, NumericLiteral> resBS = rs.BS(inv, phi);

        //     //        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        //     IntervalValue>({{0, IntervalValue(0,3)},{1,
        //     IntervalValue(1,4)},{2,IntervalValue(1,1)}});
        //     //        for(auto &item :intendedRes){
        //     //            std::cout << item.first << item.second <<"\n";
        //     //        }
        //     //        assert(resRSY.getVarToVal() == intendedRes);
        //     Z3Expr gt = ((toZ3Expr(1) == getZ3Expr(1))
        //                  && toZ3Expr(2) == getZ3Expr(2)
        //                  && getZ3Expr(2) == 1).simplify();
        //     Z3Expr::getSolver().add((gt != relExpr).getExpr());
        //     assert(vars == Set<u32_t>({1, 2, 0}) && initVars ==
        //     Set<u32_t>({1}) &&
        //            Z3Expr::getSolver().check() == z3::unsat);
        //
        // Z3Expr::releaseSolver();
        // Z3Expr::releaseContext();
        //
        //
    }
};

int main(int argc, char** argv)
{
    SVFUtil::outs() << "main";
    // test1();
    RelExeStateExample relExeStateExample;
    relExeStateExample.test_print();
    relExeStateExample.testRelExeState2();

    return 0;
}
