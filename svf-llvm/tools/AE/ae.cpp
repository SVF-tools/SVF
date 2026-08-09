//===- ae.cpp -- Abstract Execution -------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 // Abstract Execution
 //
 // Author: Jiawei Wang, Xiao Cheng, Jiawei Yang, Jiawei Ren, Yulei Sui
 */
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/WPAPass.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"
#include "WPA/Steensgaard.h"
#include "WPA/TypeAnalysis.h"

#include "AE/Core/RelExeState.h"
#include "AE/Core/RelationSolver.h"
#include "AE/Svfexe/AbstractInterpretation.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>

using namespace SVF;
using namespace SVFUtil;

// ---- lightweight phase + memory instrumentation (writes to stderr,
// flushed, so it is visible live even while SVF's buffered stdout is silent) ----
namespace
{
std::atomic<int> g_phase{0};
const char* phaseName(int p)
{
    static const char* n[] = {"startup", "svf-module-load", "svfir-build",
                              "pta", "ae(incl svfg-build)", "done"
                             };
    return (p >= 0 && p < 6) ? n[p] : "?";
}
double rssGB()
{
    std::ifstream f("/proc/self/status");
    std::string k;
    long v = 0;
    while (f >> k)
    {
        if (k == "VmRSS:")
        {
            f >> v;
            break;
        }
    }
    return v / 1048576.0;
}
void mark(const char* what)
{
    fprintf(stderr, "[PHASE] %-30s RSS=%.1fGB\n", what, rssGB());
    fflush(stderr);
}
void startHeartbeat(int periodSec)
{
    std::thread([periodSec]()
    {
        auto t0 = std::chrono::steady_clock::now();
        while (g_phase.load() < 5)
        {
            auto s = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::steady_clock::now() - t0).count();
            fprintf(stderr, "[HB +%llds] phase=%-22s RSS=%.1fGB\n",
                    (long long)s, phaseName(g_phase.load()), rssGB());
            fflush(stderr);
            std::this_thread::sleep_for(std::chrono::seconds(periodSec));
        }
    }).detach();
}

BVDataPTAImpl* runSelectedPTA(SVFIR* pag)
{
    const char* envMode = std::getenv("AE_PTA_MODE");
    const std::string mode = envMode ? envMode : "ander";

    if (mode == "steens")
    {
        std::fprintf(stderr, "[AE-PTA] mode=steens\n");
        return Steensgaard::createSteensgaard(pag);
    }
    if (mode == "type")
    {
        std::fprintf(stderr, "[AE-PTA] mode=type\n");
        TypeAnalysis* typePta = new TypeAnalysis(pag);
        typePta->analyze();
        return typePta;
    }

    if (mode != "ander" && !mode.empty())
        std::fprintf(stderr, "[AE-PTA] unknown AE_PTA_MODE=%s, fallback=ander\n",
                     mode.c_str());
    else
        std::fprintf(stderr, "[AE-PTA] mode=ander\n");
    return AndersenWaveDiff::createAndersenWaveDiff(pag);
}

void releaseSelectedPTA(BVDataPTAImpl* pta)
{
    const char* envMode = std::getenv("AE_PTA_MODE");
    const std::string mode = envMode ? envMode : "ander";

    if (mode == "steens")
        Steensgaard::releaseSteensgaard();
    else if (mode == "type")
        delete pta;
    else
        AndersenWaveDiff::releaseAndersenWaveDiff();
}
}


static Option<bool> SYMABS(
    "symabs",
    "symbolic abstraction test",
    false
);

static Option<bool> AETEST(
    "aetest",
    "abstract execution basic function test",
    false
);

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
        outs() << "hello print\n";
    }

    AbstractState RSY_time(AbstractState& inv, const Z3Expr& phi,
                           RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        AbstractState resRSY = rs.RSY(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            end_time - start_time);
        outs() << "running time of RSY      : " << duration.count()
               << " microseconds\n";
        return resRSY;
    }
    AbstractState Bilateral_time(AbstractState& inv, const Z3Expr& phi,
                                 RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        AbstractState resBilateral = rs.bilateral(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            end_time - start_time);
        outs() << "running time of Bilateral: " << duration.count()
               << " microseconds\n";
        return resBilateral;
    }
    AbstractState BS_time(AbstractState& inv, const Z3Expr& phi,
                          RelationSolver& rs)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        AbstractState resBS = rs.BS(inv, phi);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                            end_time - start_time);
        outs() << "running time of BS       : " << duration.count()
               << " microseconds\n";
        return resBS;
    }

    void testRelExeState1_1()
    {
        outs() << sucMsg("\t SUCCESS :") << "test1_1 start\n";
        AbstractState itv;
        RelExeState relation;
        // var0 := [0, 1];
        itv[0] = IntervalValue(0, 1);
        relation[0] = getContext().int_const("0");
        // var1 := var0 + 1;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0") + 1;
        itv[1] = itv[0].getInterval() + IntervalValue(1);
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[1], res);
        assert(res == Set<u32_t>({0, 1}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[1,2]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 1)}, {1, IntervalValue(1, 2)}};
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState1_2()
    {
        outs() << "test1_2 start\n";
        AbstractState itv;
        RelExeState relation;
        // var0 := [0, 1];
        relation[0] = getContext().int_const("0");
        itv[0] = IntervalValue(0, 1);
        // var1 := var0 + 1;
        relation[1] =
            getContext().int_const("1") == getContext().int_const("0") * 2;
        itv[1] = itv[0].getInterval() * IntervalValue(2);

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[1], res);
        assert(res == Set<u32_t>({0, 1}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[0,1] 1:[0,2]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 1)}, {1, IntervalValue(0, 2)}};
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState2_1()
    {
        outs() << "test2_1 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() - itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[0,10] 1:[0,10] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 10)},
            {1, IntervalValue(0, 10)},
            {2, IntervalValue(0, 0)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState2_2()
    {
        outs() << "test2_2 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() - itv[0].getInterval();

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[0,100] 1:[0,100] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 100)},
            {1, IntervalValue(0, 100)},
            {2, IntervalValue(0, 0)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState2_3()
    {
        outs() << "test2_3 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() - itv[0].getInterval();

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[0,1000] 1:[0,1000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 1000)},
            {1, IntervalValue(0, 1000)},
            {2, IntervalValue(0, 0)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState2_4()
    {
        outs() << "test2_4 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() - itv[0].getInterval();

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = RSY_time(inv, phi, rs);
        AbstractState resBilateral = Bilateral_time(inv, phi, rs);
        AbstractState resBS = BS_time(inv, phi, rs);
        // 0:[0,10000] 1:[0,10000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 10000)},
            {1, IntervalValue(0, 10000)},
            {2, IntervalValue(0, 0)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState2_5()
    {
        outs() << "test2_5 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() - itv[0].getInterval();

        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = RSY_time(inv, phi, rs);
        AbstractState resBilateral = Bilateral_time(inv, phi, rs);
        AbstractState resBS = BS_time(inv, phi, rs);
        // 0:[0,100000] 1:[0,100000] 2:[0,0]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 100000)},
            {1, IntervalValue(0, 100000)},
            {2, IntervalValue(0, 0)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState3_1()
    {
        outs() << "test3_1 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() / itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[1,10] 1:[1,10] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(1, 10)},
            {1, IntervalValue(1, 10)},
            {2, IntervalValue(1, 1)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState3_2()
    {
        outs() << "test3_2 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() / itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = rs.RSY(inv, phi);
        AbstractState resBilateral = rs.bilateral(inv, phi);
        AbstractState resBS = rs.BS(inv, phi);
        // 0:[1,1000] 1:[1,1000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(1, 1000)},
            {1, IntervalValue(1, 1000)},
            {2, IntervalValue(1, 1)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState3_3()
    {
        outs() << "test3_3 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() / itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = RSY_time(inv, phi, rs);
        AbstractState resBilateral = Bilateral_time(inv, phi, rs);
        AbstractState resBS = BS_time(inv, phi, rs);
        // 0:[1,10000] 1:[1,10000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(1, 10000)},
            {1, IntervalValue(1, 10000)},
            {2, IntervalValue(1, 1)}
        };
    }

    void testRelExeState3_4()
    {
        outs() << "test3_4 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() / itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        AbstractState resRSY = RSY_time(inv, phi, rs);
        AbstractState resBilateral = Bilateral_time(inv, phi, rs);
        AbstractState resBS = BS_time(inv, phi, rs);
        // 0:[1,100000] 1:[1,100000] 2:[1,1]
        assert(resRSY == resBS && resBS == resBilateral && "inconsistency occurs");
        for (auto r : resRSY.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(1, 100000)},
            {1, IntervalValue(1, 100000)},
            {2, IntervalValue(1, 1)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testRelExeState4_1()
    {
        outs() << "test4_1 start\n";
        AbstractState itv;
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
        itv[2] = itv[1].getInterval() / itv[0].getInterval();
        // Test extract sub vars
        Set<u32_t> res;
        relation.extractSubVars(relation[2], res);
        assert(res == Set<u32_t>({0, 1, 2}) && "inconsistency occurs");
        AbstractState inv = itv.sliceState(res);
        RelationSolver rs;
        const Z3Expr& relExpr = relation[2] && relation[1];
        const Z3Expr& initExpr = rs.gamma_hat(inv);
        const Z3Expr& phi = (relExpr && initExpr).simplify();
        // IntervalExeState resRSY = rs.RSY(inv, phi);
        outs() << "rsy done\n";
        // IntervalExeState resBilateral = rs.bilateral(inv, phi);
        outs() << "bilateral done\n";
        AbstractState resBS = rs.BS(inv, phi);
        outs() << "bs done\n";
        // 0:[0,10] 1:[0,10] 2:[-00,+00]
        // assert(resRSY == resBS && resBS == resBilateral);
        for (auto r : resBS.getVarToVal())
        {
            outs() << r.first << " " << r.second.getInterval() << "\n";
        }
        // ground truth
        AbstractState::VarToAbsValMap intendedRes = {{0, IntervalValue(0, 10)},
            {1, IntervalValue(0, 10)},
            {2, IntervalValue(0, 10)}
        };
        assert(resBS.eqVarToValMap(resBS.getVarToVal(), intendedRes) && "inconsistency occurs");
    }

    void testsValidation()
    {
        SymblicAbstractionTest saTest;
        saTest.testRelExeState1_1();
        saTest.testRelExeState1_2();

        saTest.testRelExeState2_1();
        saTest.testRelExeState2_2();
        saTest.testRelExeState2_3();
        //        saTest.testRelExeState2_4(); /// 10000
        //        saTest.testRelExeState2_5(); /// 100000

        saTest.testRelExeState3_1();
        saTest.testRelExeState3_2();
        //        saTest.testRelExeState3_3(); /// 10000
        //        saTest.testRelExeState3_4(); /// 100000

        outs() << "start top\n";
        saTest.testRelExeState4_1(); /// top
    }
};

class AETest
{
public:
    AETest() = default;

    ~AETest() = default;

    void testBinaryOpStmt()
    {
        // test division /
        assert((IntervalValue(4) / IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() / IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() / IntervalValue(0)).equals(IntervalValue::bottom()));
        assert((IntervalValue(4) / IntervalValue(2)).equals(IntervalValue(2)));
        assert((IntervalValue(3) / IntervalValue(2)).equals(IntervalValue(1))); //
        assert((IntervalValue(-3) / IntervalValue(2)).equals(IntervalValue(-1))); //
        assert((IntervalValue(1, 3) / IntervalValue(2)).equals(IntervalValue(0, 1))); //
        assert((IntervalValue(2, 7) / IntervalValue(2)).equals(IntervalValue(1, 3))); //
        assert((IntervalValue(-3, 3) / IntervalValue(2)).equals(IntervalValue(-1, 1)));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) / IntervalValue(2)).equals(IntervalValue(-1, IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) / IntervalValue(2)).equals(IntervalValue(IntervalValue::minus_infinity(), 1)));
        assert((IntervalValue(1, 3) / IntervalValue(1, 2)).equals(IntervalValue(0, 3)));//
        assert((IntervalValue(-3, 3) / IntervalValue(1, 2)).equals(IntervalValue(-3, 3)));
        assert((IntervalValue(2, 7) / IntervalValue(-2, 3)).equals(IntervalValue(-7, 7))); //
        assert((IntervalValue(-2, 7) / IntervalValue(-2, 3)).equals(IntervalValue(-7, 7))); //
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) / IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) / IntervalValue(-2, 3)).equals(IntervalValue::top()));

        assert((IntervalValue(-2, 7) / IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue(-7, 7)));
        assert((IntervalValue(-2, 7) / IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue(-7, 7)));
        assert((IntervalValue(-6, -3) / IntervalValue(3, 9)).equals(IntervalValue(-2, 0)));
        assert((IntervalValue(-6, 6) / IntervalValue(3, 9)).equals(IntervalValue(-2, 2)));

        // test remainder %
        assert((IntervalValue(4) % IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() % IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() % IntervalValue(0)).equals(IntervalValue::top()));
        assert((IntervalValue(4) % IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(3) % IntervalValue(2)).equals(IntervalValue(1)));
        assert((IntervalValue(-3) % IntervalValue(2)).equals(IntervalValue(-1)));
        assert((IntervalValue(1, 3) % IntervalValue(2)).equals(IntervalValue(0, 1)));
        assert((IntervalValue(2, 7) % IntervalValue(2)).equals(IntervalValue(0, 1)));
        assert((IntervalValue(-3, 3) % IntervalValue(2)).equals(IntervalValue(-1, 1)));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) % IntervalValue(2)).equals(IntervalValue(-1, 1)));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) % IntervalValue(2)).equals(IntervalValue(-1, 1)));
        assert((IntervalValue(1, 3) % IntervalValue(1, 2)).equals(IntervalValue(0, 1)));
        assert((IntervalValue(-3, 3) % IntervalValue(1, 2)).equals(IntervalValue(-1, 1)));
        assert((IntervalValue(2, 7) % IntervalValue(-2, 3)).equals(IntervalValue::top())); //
        assert((IntervalValue(-2, 7) % IntervalValue(-2, 3)).equals(IntervalValue::top())); //
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) % IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) % IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) % IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) % IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue::top()));
        assert((IntervalValue(-6, -3) % IntervalValue(3, 9)).equals(IntervalValue(-6, 0)));
        assert((IntervalValue(-6, 6) % IntervalValue(3, 9)).equals(IntervalValue(-6, 6)));

        // shl  <<
        assert((IntervalValue(IntervalValue::plus_infinity()) << IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::top())));
        assert((IntervalValue(IntervalValue::plus_infinity()) << IntervalValue(2, 2)).equals(IntervalValue(IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity()) << IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::top())));
        assert((IntervalValue(IntervalValue::minus_infinity()) << IntervalValue(2, 2)).equals(IntervalValue(IntervalValue::minus_infinity())));
        assert((IntervalValue(2, 2) << IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::top())));
        assert((IntervalValue(0, 0) << IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(0, 0)));
        assert((IntervalValue(-2, -2) << IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::top())));
        assert((IntervalValue(0, 0) << IntervalValue(2, 2)).equals(IntervalValue(0, 0)));
        assert((IntervalValue(2, 2) << IntervalValue(3, 3)).equals(IntervalValue(16, 16)));
        assert((IntervalValue(-2, -2) << IntervalValue(3, 3)).equals(IntervalValue(-16, -16)));

        assert((IntervalValue(4) << IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() << IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() << IntervalValue(0)).equals(IntervalValue::top()));
        assert((IntervalValue(4) << IntervalValue(2)).equals(IntervalValue(16)));
        assert((IntervalValue(3) << IntervalValue(2)).equals(IntervalValue(12)));
        assert((IntervalValue(-3) << IntervalValue(2)).equals(IntervalValue(-12)));
        assert((IntervalValue(4) << IntervalValue(-2)).equals(IntervalValue::bottom()));
        assert((IntervalValue(1, 3) << IntervalValue(2)).equals(IntervalValue(4, 12)));
        assert((IntervalValue(2, 7) << IntervalValue(2)).equals(IntervalValue(8, 28)));
        assert((IntervalValue(-3, 3) << IntervalValue(2)).equals(IntervalValue(-12, 12)));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) << IntervalValue(2)).equals(IntervalValue(-12, IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) << IntervalValue(2)).equals(IntervalValue(IntervalValue::minus_infinity(), 12)));
        assert((IntervalValue(1, 3) << IntervalValue(1, 2)).equals(IntervalValue(2, 12)));
        assert((IntervalValue(-3, 3) << IntervalValue(1, 2)).equals(IntervalValue(-12, 12)));
        assert((IntervalValue(2, 7) << IntervalValue(-2, 3)).equals(IntervalValue(2, 56)));
        assert((IntervalValue(-2, 7) << IntervalValue(-2, 3)).equals(IntervalValue(-16, 56)));
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) << IntervalValue(-2, 3)).equals(IntervalValue(IntervalValue::minus_infinity(), 56)));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) << IntervalValue(-2, 3)).equals(IntervalValue(-16, IntervalValue::plus_infinity())));
        assert((IntervalValue(-2, 7) << IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue(-16, 56)));
        assert((IntervalValue(-2, 7) << IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue::top()));
        assert((IntervalValue(-6, -3) << IntervalValue(3, 9)).equals(IntervalValue(-3072, -24)));
        assert((IntervalValue(-6, 6) << IntervalValue(3, 9)).equals(IntervalValue(-3072, 3072)));
        assert((IntervalValue(-2, 7) << IntervalValue(IntervalValue::minus_infinity(), -1)).equals(IntervalValue::bottom()));
        assert((IntervalValue(0) << IntervalValue::top()).equals(IntervalValue(0)));


        // shr >>
        assert((IntervalValue(IntervalValue::plus_infinity()) >> IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::plus_infinity()) >> IntervalValue(2)).equals(IntervalValue(IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity()) >> IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(IntervalValue::minus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity()) >> IntervalValue(2)).equals(IntervalValue(IntervalValue::minus_infinity())));
        assert((IntervalValue(2) >> IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(0)));
        assert((IntervalValue(0) >> IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(0)));
        assert((IntervalValue(-2) >> IntervalValue(IntervalValue::plus_infinity())).equals(IntervalValue(-1)));
        assert((IntervalValue(0) >> IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(15) >> IntervalValue(2)).equals(IntervalValue(3)));
        assert((IntervalValue(-15) >> IntervalValue(2)).equals(IntervalValue(-4)));

        assert((IntervalValue(4) >> IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() >> IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() >> IntervalValue(0)).equals(IntervalValue::top()));
        assert((IntervalValue(15) >> IntervalValue(2)).equals(IntervalValue(3)));
        assert((IntervalValue(1) >> IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(-15) >> IntervalValue(2)).equals(IntervalValue(-4)));
        assert((IntervalValue(4) >> IntervalValue(-2)).equals(IntervalValue::bottom()));
        assert((IntervalValue(1, 3) >> IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(2, 7) >> IntervalValue(2)).equals(IntervalValue(0, 1)));
        assert((IntervalValue(-15, 15) >> IntervalValue(2)).equals(IntervalValue(-4, 3)));
        assert((IntervalValue(-15, IntervalValue::plus_infinity()) >> IntervalValue(2)).equals(IntervalValue(-4, IntervalValue::plus_infinity())));
        assert((IntervalValue(IntervalValue::minus_infinity(), 15) >> IntervalValue(2)).equals(IntervalValue(IntervalValue::minus_infinity(), 3)));
        assert((IntervalValue(0, 15) >> IntervalValue(1, 2)).equals(IntervalValue(0, 7)));
        assert((IntervalValue(-17, 15) >> IntervalValue(1, 2)).equals(IntervalValue(-9, 7)));
        assert((IntervalValue(2, 7) >> IntervalValue(-2, 3)).equals(IntervalValue(0, 7)));
        assert((IntervalValue(-2, 7) >> IntervalValue(-2, 3)).equals(IntervalValue(-2, 7)));
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) >> IntervalValue(-2, 3)).equals(IntervalValue(IntervalValue::minus_infinity(), 7)));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) >> IntervalValue(-2, 3)).equals(IntervalValue(-2, IntervalValue::plus_infinity())));
        assert((IntervalValue(-2, 7) >> IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue(-2, 7)));
        assert((IntervalValue(-2, 7) >> IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue(-2, 7)));
        assert((IntervalValue(-6, -3) >> IntervalValue(2, 3)).equals(IntervalValue(-2, -1)));
        assert((IntervalValue(-6, 6) >> IntervalValue(2, 3)).equals(IntervalValue(-2, 1)));
        assert((IntervalValue(-2, 7) >> IntervalValue(IntervalValue::minus_infinity(), -1)).equals(IntervalValue::bottom()));
        assert((IntervalValue(0) >> IntervalValue::top()).equals(IntervalValue(0)));

        // and &
        assert((IntervalValue(4) & IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() & IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() & IntervalValue(0)).equals(IntervalValue(0)));
        assert((IntervalValue(4) & IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(3) & IntervalValue(2)).equals(IntervalValue(2)));
        assert((IntervalValue(-3) & IntervalValue(2)).equals(IntervalValue(0)));
        assert((IntervalValue(1, 3) & IntervalValue(2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(2, 7) & IntervalValue(2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(-3, 3) & IntervalValue(2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) & IntervalValue(2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) & IntervalValue(2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(1, 3) & IntervalValue(1, 2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(-3, 3) & IntervalValue(1, 2)).equals(IntervalValue(0, 2)));
        assert((IntervalValue(2, 7) & IntervalValue(-2, 3)).equals(IntervalValue(0, 7)));
        assert((IntervalValue(-2, 7) & IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) & IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) & IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) & IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) & IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue::top()));
        assert((IntervalValue(-6, -3) & IntervalValue(3, 9)).equals(IntervalValue(0, 9)));
        assert((IntervalValue(-6, 6) & IntervalValue(3, 9)).equals(IntervalValue(0, 9)));

        // Or |
        assert((IntervalValue(4) | IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() | IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() | IntervalValue(-1)).equals(IntervalValue::top()));//
        assert((IntervalValue(-1) | IntervalValue::top()).equals(IntervalValue::top()));//
        assert((IntervalValue(4) | IntervalValue(2)).equals(IntervalValue(6)));
        assert((IntervalValue(3) | IntervalValue(2)).equals(IntervalValue(3)));
        assert((IntervalValue(-3) | IntervalValue(2)).equals(IntervalValue(-1)));
        assert((IntervalValue(1, 3) | IntervalValue(2)).equals(IntervalValue(0, 3)));
        assert((IntervalValue(2, 7) | IntervalValue(2)).equals(IntervalValue(0, 7)));
        assert((IntervalValue(-3, 3) | IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) | IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) | IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(1, 3) | IntervalValue(1, 2)).equals(IntervalValue(0, 3)));
        assert((IntervalValue(-3, 3) | IntervalValue(1, 2)).equals(IntervalValue::top()));
        assert((IntervalValue(2, 7) | IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) | IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) | IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) | IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) | IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) | IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue::top()));
        assert((IntervalValue(-6, -3) | IntervalValue(3, 9)).equals(IntervalValue::top()));
        assert((IntervalValue(-6, 6) | IntervalValue(3, 9)).equals(IntervalValue::top()));

        // Xor ^
        assert((IntervalValue(4) ^ IntervalValue::bottom()).equals(IntervalValue::bottom()));
        assert((IntervalValue::bottom() ^ IntervalValue(2)).equals(IntervalValue::bottom()));
        assert((IntervalValue::top() ^ IntervalValue(-1)).equals(IntervalValue::top()));
        assert((IntervalValue(-1) ^ IntervalValue::top()).equals(IntervalValue::top()));
        assert((IntervalValue(4) ^ IntervalValue(2)).equals(IntervalValue(6)));
        assert((IntervalValue(3) ^ IntervalValue(2)).equals(IntervalValue(1)));
        assert((IntervalValue(-3) ^ IntervalValue(2)).equals(IntervalValue(-1)));
        assert((IntervalValue(1, 3) ^ IntervalValue(2)).equals(IntervalValue(0, 3)));
        assert((IntervalValue(2, 7) ^ IntervalValue(2)).equals(IntervalValue(0, 7)));
        assert((IntervalValue(-3, 3) ^ IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(-3, IntervalValue::plus_infinity()) ^ IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(IntervalValue::minus_infinity(), 3) ^ IntervalValue(2)).equals(IntervalValue::top()));
        assert((IntervalValue(1, 3) ^ IntervalValue(1, 2)).equals(IntervalValue(0, 3)));
        assert((IntervalValue(-3, 3) ^ IntervalValue(1, 2)).equals(IntervalValue::top()));
        assert((IntervalValue(2, 7) ^ IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) ^ IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(IntervalValue::minus_infinity(), 7) ^ IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, IntervalValue::plus_infinity()) ^ IntervalValue(-2, 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) ^ IntervalValue(IntervalValue::minus_infinity(), 3)).equals(IntervalValue::top()));
        assert((IntervalValue(-2, 7) ^ IntervalValue(-2, IntervalValue::plus_infinity())).equals(IntervalValue::top()));
        assert((IntervalValue(-6, -3) ^ IntervalValue(3, 9)).equals(IntervalValue::top()));
        assert((IntervalValue(-6, 6) ^ IntervalValue(3, 9)).equals(IntervalValue::top()));
    }

    void testAbsState()
    {
        AbstractState as;
        as[1] = IntervalValue(1, 3);
        as[2] = IntervalValue(2, 7);
        as[3] = AddressValue(0x7f000007);
        as[4] = AddressValue(0x7f000008);
        // store: *as[3] = as[1], *as[4] = as[2]
        for (auto addr : as[3].getAddrs()) as.store(addr, as[1]);
        for (auto addr : as[4].getAddrs()) as.store(addr, as[2]);
        as.printAbstractState();
        // load: verify *as[3] == as[1] && *as[4] == as[2]
        AbstractValue v3, v4;
        for (auto addr : as[3].getAddrs()) v3.join_with(as.load(addr));
        for (auto addr : as[4].getAddrs()) v4.join_with(as.load(addr));
        assert(v3.equals(as[1]) && v4.equals(as[2]));
    }
};


int main(int argc, char** argv)
{
    bool hasUserExtMemFieldLimit = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "-ext-mem-field-limit" || arg.find("-ext-mem-field-limit=") == 0)
        {
            hasUserExtMemFieldLimit = true;
            break;
        }
    }
    const char* envExtMemFieldLimit = std::getenv("AE_EXT_MEM_FIELD_LIMIT");

    int arg_num = 0;
    int extraArgc = 3 + ((envExtMemFieldLimit && !hasUserExtMemFieldLimit) ? 1 : 0);
    char **arg_value = new char *[argc + extraArgc];
    for (; arg_num < argc; ++arg_num)
    {
        arg_value[arg_num] = argv[arg_num];
    }
    // add extra options
    const char* modelArraysArg = "-model-arrays=true";
    if (const char* envModelArrays = std::getenv("AE_MODEL_ARRAYS"))
    {
        std::string value(envModelArrays);
        if (value == "0" || value == "false" || value == "off" || value == "no")
            modelArraysArg = "-model-arrays=false";
        else if (!(value == "1" || value == "true" || value == "on" || value == "yes"))
            std::fprintf(stderr, "[AE-FRONTEND] unknown AE_MODEL_ARRAYS=%s, fallback=true\n",
                         envModelArrays);
    }
    std::fprintf(stderr, "[AE-FRONTEND] %s\n", modelArraysArg);
    arg_value[arg_num++] = (char*) "-model-consts=true";
    arg_value[arg_num++] = const_cast<char*>(modelArraysArg);
    arg_value[arg_num++] = (char*) "-pre-field-sensitive=false";
    std::string extMemFieldLimitArg;
    if (envExtMemFieldLimit && !hasUserExtMemFieldLimit)
    {
        extMemFieldLimitArg = std::string("-ext-mem-field-limit=") + envExtMemFieldLimit;
        arg_value[arg_num++] = const_cast<char*>(extMemFieldLimitArg.c_str());
    }
    assert(arg_num == (argc + extraArgc) && "more extra arguments? Change the value of extraArgc");

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        arg_num, arg_value, "Static Symbolic Execution", "[options] <input-bitcode...>"
                    );
    std::fprintf(stderr, "[AE-FRONTEND] -ext-mem-field-limit=%u\n", Options::ExtMemFieldLimit());
    delete[] arg_value;
    if (SYMABS())
    {
        SymblicAbstractionTest saTest;
        saTest.testsValidation();
        return 0;
    }

    if (AETEST())
    {
        AETest aeTest;
        aeTest.testBinaryOpStmt();
        aeTest.testAbsState();
        return 0;
    }

    startHeartbeat(15);
    g_phase = 1;
    mark("svf-module-load start");
    LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    g_phase = 2;
    mark("svfir-build start");
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();
    // Run PTA to resolve indirect calls, then update SVFIR with resolved targets.
    // Default remains Andersen. AE_PTA_MODE={steens,type} is a coarse fallback
    // for large harnesses where Andersen front-end time dominates the budget.
    g_phase = 3;
    mark("pta start");
    BVDataPTAImpl* pta = runSelectedPTA(pag);
    builder.updateCallGraph(pta->getCallGraph());
    mark("pta done");
    releaseSelectedPTA(pta);
    mark("front pta released");
    g_phase = 4;
    mark("ae start (svfg-build is inside)");
    AbstractInterpretation& ae = AbstractInterpretation::getAEInstance();
    if (Options::BufferOverflowCheck())
        ae.addDetector(std::make_unique<BufOverflowDetector>());
    if (Options::NullDerefCheck())
        ae.addDetector(std::make_unique<NullptrDerefDetector>());
    ae.runOnModule();
    g_phase = 5;
    mark("ae done");

    if (const char* envMode = std::getenv("AE_PTA_MODE"))
    {
        if (std::string(envMode) == "steens")
            Steensgaard::releaseSteensgaard();
        else
            AndersenWaveDiff::releaseAndersenWaveDiff();
    }
    else
    {
        AndersenWaveDiff::releaseAndersenWaveDiff();
    }
    LLVMModuleSet::releaseLLVMModuleSet();

    return 0;
}
