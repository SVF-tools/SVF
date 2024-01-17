//
// Created by Jiawei Ren on 11/28/23.
//
#include "AbstractExecution/IntervalExeState.h"
#include "AbstractExecution/RelExeState.h"
#include "AbstractExecution/RelationSolver.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/Z3Expr.h"
#include <chrono>

using namespace SVF;
using namespace SVFUtil;

class SymblicAbstractionTest
{
public:
    SymblicAbstractionTest() = default;

    ~SymblicAbstractionTest() = default;

    static z3::context& getContext()
    {
        return Z3Expr::getContext();
    }

    void test_print()
    {
        SVFUtil::outs() << "hello print\n";
    }

    IntervalExeState RSY_time(IntervalExeState& inv, const Z3Expr& phi,
                              RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        outs() << "running time of RSY      : " << duration.count()
               << " microseconds\n";
        return resRSY;
    }
    IntervalExeState Bilateral_time(IntervalExeState& inv, const Z3Expr& phi,
                                    RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        outs() << "running time of Bilateral: " << duration.count()
               << " microseconds\n";
        return resBilateral;
    }
    IntervalExeState BS_time(IntervalExeState& inv, const Z3Expr& phi,
                             RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        IntervalExeState resBS = rs.BS(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        outs() << "running time of BS       : " << duration.count()
               << " microseconds\n";
        return resBS;
    }

    void testRelExeState1_1()
    {
        SVFUtil::outs() << "test1_1 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 1];
        itv[0] = IntervalValue(0, 1);
        relation[0] = getContext().int_const("0");
        // var1 := var0 + 1;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0") + 1;
        itv[1] = itv[0] + IntervalValue(1);
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[1], res);
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        assert(res == Set<u32_t>({0, 1}));
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[1,2]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        IntervalExeState::VarToValMap intendedRes = Map<u32_t, IntervalValue>(
            {{0, IntervalValue(0, 1)}, {1, IntervalValue(1, 2)}});
    }

    void testRelExeState1_2()
    {
        SVFUtil::outs() << "test1_2 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 1];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 1);
        // var1 := var0 + 1;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0") * 2;
        itv[1] = itv[0] * IntervalValue(2);

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[1], res);
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        assert(res == Set<u32_t>({0, 1}));
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[0,2]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        IntervalExeState::VarToValMap intendedRes = Map<u32_t, IntervalValue>(
            {{0, IntervalValue(0, 1)}, {1, IntervalValue(0, 2)}});
    }

    void testRelExeState2_1()
    {
        SVFUtil::outs() << "test2_1 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 10];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 10);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 - var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") - getContext().int_const("0");
        itv[2] = itv[1] - itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,10] 1:[0,10] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(0, 10)},
                                       {1, IntervalValue(0, 10)},
                                       {2, IntervalValue(0, 0)}});
    }

    void testRelExeState2_2()
    {
        SVFUtil::outs() << "test2_2 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 100];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 100);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 - var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") - getContext().int_const("0");
        itv[2] = itv[1] - itv[0];

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,100] 1:[0,100] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(0, 100)},
                                       {1, IntervalValue(0, 100)},
                                       {2, IntervalValue(0, 0)}});
    }

    void testRelExeState2_3()
    {
        SVFUtil::outs() << "test2_3 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 1000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 1000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 - var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") - getContext().int_const("0");
        itv[2] = itv[1] - itv[0];

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[0,1000] 1:[0,1000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(0, 1000)},
                                       {1, IntervalValue(0, 1000)},
                                       {2, IntervalValue(0, 0)}});
    }

    void testRelExeState2_4()
    {
        SVFUtil::outs() << "test2_4 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 10000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 10000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 - var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") - getContext().int_const("0");
        itv[2] = itv[1] - itv[0];

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[0,10000] 1:[0,10000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(0, 10000)},
                                       {1, IntervalValue(0, 10000)},
                                       {2, IntervalValue(0, 0)}});
    }

    void testRelExeState2_5()
    {
        SVFUtil::outs() << "test2_5 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 100000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 100000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 - var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") - getContext().int_const("0");
        itv[2] = itv[1] - itv[0];

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[0,100000] 1:[0,100000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(0, 100000)},
                                       {1, IntervalValue(0, 100000)},
                                       {2, IntervalValue(0, 0)}});
    }

    void testRelExeState3_1()
    {
        SVFUtil::outs() << "test3_1 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [1, 10];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(1, 10);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 / var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") / getContext().int_const("0");
        itv[2] = itv[1] / itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[1,10] 1:[1,10] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(1, 10)},
                                       {1, IntervalValue(1, 10)},
                                       {2, IntervalValue(1, 1)}});
    }

    void testRelExeState3_2()
    {
        SVFUtil::outs() << "test3_2 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [1, 1000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(1, 1000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 / var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") / getContext().int_const("0");
        itv[2] = itv[1] / itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = rs.RSY(inv, phi);
        IntervalExeState resBilateral = rs.bilateral(inv, phi);
        IntervalExeState resBS = rs.BS(inv, phi);
        // 0:[1,1000] 1:[1,1000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(1, 1000)},
                                       {1, IntervalValue(1, 1000)},
                                       {2, IntervalValue(1, 1)}});
    }

    void testRelExeState3_3()
    {
        SVFUtil::outs() << "test3_3 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [1, 10000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(1, 10000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 / var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") / getContext().int_const("0");
        itv[2] = itv[1] / itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[1,10000] 1:[1,10000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(1, 10000)},
                                       {1, IntervalValue(1, 10000)},
                                       {2, IntervalValue(1, 1)}});
    }

    void testRelExeState3_4()
    {
        SVFUtil::outs() << "test3_4 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [1, 100000];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(1, 100000);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 / var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") / getContext().int_const("0");
        itv[2] = itv[1] / itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        IntervalExeState resRSY = RSY_time(inv, phi, rs);
        IntervalExeState resBilateral = Bilateral_time(inv, phi, rs);
        IntervalExeState resBS = BS_time(inv, phi, rs);
        // 0:[1,100000] 1:[1,100000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(1, 100000)},
                                       {1, IntervalValue(1, 100000)},
                                       {2, IntervalValue(1, 1)}});
    }

    void testRelExeState4_1()
    {
        SVFUtil::outs() << "test4_1 start\n";
        IntervalExeState itv;
        RelExeState relation;
        // var0 := [0, 10];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 10);
        // var1 := var0;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0");
        itv[1] = itv[0];
        // var2 := var1 / var0;
        relation[2] = getContext().int_const("2") ==
                      getContext().int_const("1") / getContext().int_const("0");
        itv[2] = itv[1] / itv[0];
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}));
        for (auto index : res)
        {
            itv.cpyItvToLocal(index);
        }
        IntervalExeState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
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
        for (auto r : resBS.getVarToVal())
        {
            outs() << r.first << " " << r.second << "\n";
        }
        // ground truth
        IntervalExeState::VarToValMap intendedRes =
            Map<u32_t, IntervalValue>({{0, IntervalValue(1, 10)},
                                       {1, IntervalValue(1, 10)},
                                       {2, IntervalValue(1, 1)}});
    }
};

int main(int argc, char** argv)
{
    SVFUtil::outs() << "main\n";
    SymblicAbstractionTest saTest;
    saTest.testRelExeState1_1();
    saTest.testRelExeState1_2();

    saTest.testRelExeState2_1();
    saTest.testRelExeState2_2();
    saTest.testRelExeState2_3();
    saTest.testRelExeState2_4(); /// 10000
    saTest.testRelExeState2_5(); /// 100000

    saTest.testRelExeState3_1();
    saTest.testRelExeState3_2();
    saTest.testRelExeState3_3(); /// 10000
    saTest.testRelExeState3_4(); /// 100000

    outs() << "start top\n";
    saTest.testRelExeState4_1(); /// top
    return 0;
}
