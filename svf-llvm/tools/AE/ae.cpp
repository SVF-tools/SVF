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
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

#include "AE/Svfexe/AbstractInterpretation.h"

using namespace SVF;
using namespace SVFUtil;


static Option<bool> AETEST(
    "aetest",
    "abstract execution basic function test",
    false
);

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

};


int main(int argc, char** argv)
{
    int arg_num = 0;
    int extraArgc = 3;
    char **arg_value = new char *[argc + extraArgc];
    for (; arg_num < argc; ++arg_num)
    {
        arg_value[arg_num] = argv[arg_num];
    }
    // add extra options
    arg_value[arg_num++] = (char*) "-model-consts=true";
    arg_value[arg_num++] = (char*) "-model-arrays=true";
    arg_value[arg_num++] = (char*) "-pre-field-sensitive=false";
    assert(arg_num == (argc + extraArgc) && "more extra arguments? Change the value of extraArgc");

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        arg_num, arg_value, "Static Symbolic Execution", "[options] <input-bitcode...>"
                    );
    delete[] arg_value;
    if (AETEST())
    {
        AETest aeTest;
        aeTest.testBinaryOpStmt();
        return 0;
    }

    LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();
    // Run Andersen's to resolve indirect calls, then update SVFIR with resolved targets.
    // The Andersen singleton will be reused inside AbstractInterpretation::runOnModule().
    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    builder.updateCallGraph(ander->getCallGraph());
    AbstractInterpretation& ae = AbstractInterpretation::getAEInstance();
    if (Options::BufferOverflowCheck())
        ae.addDetector(std::make_unique<BufOverflowDetector>());
    if (Options::NullDerefCheck())
        ae.addDetector(std::make_unique<NullptrDerefDetector>());
    ae.runOnModule();

    AndersenWaveDiff::releaseAndersenWaveDiff();
    LLVMModuleSet::releaseLLVMModuleSet();

    return 0;
}