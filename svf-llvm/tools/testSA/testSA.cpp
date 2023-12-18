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
#include <chrono>
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
    IntervalExeState RSY_time(IntervalExeState& inv, const Z3Expr &phi, RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        outs() << "running time of RSY      : " << duration.count() << " microseconds\n";
        return resRSY;
    }
    IntervalExeState Bilateral_time(IntervalExeState& inv, const Z3Expr &phi, RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        outs() << "running time of Bilateral: " << duration.count() << " microseconds\n";
        return resBilateral;
    }
    IntervalExeState BS_time(IntervalExeState& inv, const Z3Expr &phi, RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resBS = rs.BS(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        outs() << "running time of BS       : " << duration.count() << " microseconds\n";
        return resBS;
    }

    void testRelExeState1_1()
    {
        SVFUtil::outs() << "test1_1 start\n";
        // var0 := [0, 1];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 1);
        // var1 := var0 + 1;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0") + 1;
        IntervalExeState::_varToItvVal[1] =
            IntervalExeState::_varToItvVal[0] + IntervalValue(1);
        // // var2 := var1 < 5;
        // RelExeState::_varToVal[2] =
        //     getContext().int_const("1") <= getContext().int_const("0") + 1;
        // IntervalExeState::_varToItvVal[2] =
        //     IntervalExeState::_varToItvVal[1] < IntervalValue(5);

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
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[1,2]
        assert(resRSY == resBS && resBS == resBilateral);
        IntervalExeState::VarToValMap intendedRes = Map<u32_t, IntervalValue>({{0, IntervalValue(0,1)},{1, IntervalValue(1,2)}});
    }

    void testRelExeState1_2()
    {
        SVFUtil::outs() << "test1_2 start\n";
        // var0 := [0, 1];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 1);
        // var1 := var0 + 1;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0") * 2;
        IntervalExeState::_varToItvVal[1] =
            IntervalExeState::_varToItvVal[0] * IntervalValue(2);

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
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[0,2]
        assert(resRSY == resBS && resBS == resBilateral);
        IntervalExeState::VarToValMap intendedRes = Map<u32_t, IntervalValue>({{0, IntervalValue(0,1)},{1, IntervalValue(0,2)}});
    }

    void testRelExeState2_1()
    {
        SVFUtil::outs() << "test2_1 start\n";
        // var0 := [0, 10];
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
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[0];

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
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,10] 1:[0,10] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(0,10)},{1,
        IntervalValue(0,10)},{2,IntervalValue(0,0)}});
    }

    void testRelExeState2_2()
    {
        SVFUtil::outs() << "test2_2 start\n";
        // var0 := [0, 100];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 100);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 - var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") -
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[0];

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
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,100] 1:[0,100] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(0,100)},{1,
        IntervalValue(0,100)},{2,IntervalValue(0,0)}});
    }

    void testRelExeState2_3()
    {
        SVFUtil::outs() << "test2_3 start\n";
        // var0 := [0, 1000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 1000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 - var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") -
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[0];

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
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1000] 1:[0,1000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(0,1000)},{1,
        IntervalValue(0,1000)},{2,IntervalValue(0,0)}});
    }

    void testRelExeState2_4()
    {
        SVFUtil::outs() << "test2_4 start\n";
        // var0 := [0, 10000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 10000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 - var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") -
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[0];

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
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[0,10000] 1:[0,10000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(0,10000)},{1,
        IntervalValue(0,10000)},{2,IntervalValue(0,0)}});
    }

    void testRelExeState2_5()
    {
        SVFUtil::outs() << "test2_5 start\n";
        // var0 := [0, 100000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 100000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 - var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") -
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] - IntervalExeState::_varToItvVal[0];

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
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[0,100000] 1:[0,100000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(0,100000)},{1,
        IntervalValue(0,100000)},{2,IntervalValue(0,0)}});
    }

    void testRelExeState3_1()
    {
        SVFUtil::outs() << "test3_1 start\n";
        // var0 := [1, 10];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(1, 10);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 / var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") /
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] / IntervalExeState::_varToItvVal[0];
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
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[1,10] 1:[1,10] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(1,10)},{1,
        IntervalValue(1,10)},{2,IntervalValue(1,1)}});
    }

    void testRelExeState3_2()
    {
        SVFUtil::outs() << "test3_2 start\n";
        // var0 := [1, 1000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(1, 1000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 / var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") /
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] / IntervalExeState::_varToItvVal[0];
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
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[1,1000] 1:[1,1000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(1,1000)},{1,
        IntervalValue(1,1000)},{2,IntervalValue(1,1)}});
    }

    void testRelExeState3_3()
    {
        SVFUtil::outs() << "test3_3 start\n";
        // var0 := [1, 10000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(1, 10000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 / var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") /
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] / IntervalExeState::_varToItvVal[0];
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
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[1,10000] 1:[1,10000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(1,10000)},{1,
        IntervalValue(1,10000)},{2,IntervalValue(1,1)}});
    }

    void testRelExeState3_4()
    {
        SVFUtil::outs() << "test3_4 start\n";
        // var0 := [1, 100000];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(1, 100000);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 / var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") /
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] / IntervalExeState::_varToItvVal[0];
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
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[1,100000] 1:[1,100000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(1,100000)},{1,
        IntervalValue(1,100000)},{2,IntervalValue(1,1)}});
    }

    void testRelExeState4_1()
    {
        SVFUtil::outs() << "test4_1 start\n";
        // var0 := [0, 10];
        RelExeState::_varToVal[0] = getContext().int_const("0");
        IntervalExeState::_varToItvVal[0] = IntervalValue(0, 10);
        // var1 := var0;
        RelExeState::_varToVal[1] =
            getContext().int_const("1") == getContext().int_const("0");
        IntervalExeState::_varToItvVal[1] = IntervalExeState::_varToItvVal[0];
        // var2 := var1 / var0;
        RelExeState::_varToVal[2] =
            getContext().int_const("2") == getContext().int_const("1") /
            getContext().int_const("0");
        IntervalExeState::_varToItvVal[2] =
            IntervalExeState::_varToItvVal[1] / IntervalExeState::_varToItvVal[0];
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
        outs() << "rsy done\n";
        // IntervalExeState resBilateral = rs.bilateral(inv, phi);
        outs() << "bilateral done\n";
        IntervalExeState resBS = rs.BS(inv, phi);
        outs() << "bs done\n";
        // 0:[0,10] 1:[0,10] 2:[-00,+00]
        // assert(resRSY == resBS && resBS == resBilateral);
        // ground truth
        IntervalExeState::VarToValMap intendedRes = Map<u32_t,
        IntervalValue>({{0, IntervalValue(1,10)},{1,
        IntervalValue(1,10)},{2,IntervalValue(1,1)}});
    }
};

int main(int argc, char** argv)
{
    SVFUtil::outs() << "main\n";
    RelExeStateExample relExeStateExample;
    relExeStateExample.testRelExeState1_1();
    relExeStateExample.testRelExeState1_2();

    relExeStateExample.testRelExeState2_1();
    relExeStateExample.testRelExeState2_2();
    relExeStateExample.testRelExeState2_3();
    relExeStateExample.testRelExeState2_4(); /// 10000
    relExeStateExample.testRelExeState2_5(); /// 100000

    relExeStateExample.testRelExeState3_1();
    relExeStateExample.testRelExeState3_2();
    relExeStateExample.testRelExeState3_3(); /// 10000
    relExeStateExample.testRelExeState3_4(); /// 100000

    outs() << "start top\n";
    relExeStateExample.testRelExeState4_1(); /// top
    return 0;
}
