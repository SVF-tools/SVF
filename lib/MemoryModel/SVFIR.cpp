//===- SVFIR.cpp -- IR of SVF ---------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * SVFIR.cpp
 *
 *  Created on: 31, 12, 2021
 *      Author: Yulei Sui
 */

#include "MemoryModel/SVFIR.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

const std::string SVFStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "SVFStmt type : " << kind << "\t";
    return rawstr.str();
}

const std::string AllocStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "AllocStmt : " << dstVar << "<-" << srcVar << "\n";
    return rawstr.str();
}

const std::string CopyStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CopyStmt : " << dstVar << "<-" << srcVar << "\n";
    return rawstr.str();
}

const std::string PhiStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "PhiStmt : " << resVar << "<-(" << op1Var << "," << op2Var << ")" << "\n";
    return rawstr.str();
}

const std::string LoadStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "LoadStmt : " << dstVar << "<-" << srcVar << "\n";
    return rawstr.str();
}

const std::string StoreStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "StoreStmt : " << dstVar << "<-" << srcVar << "\n";
    return rawstr.str();
}

const std::string GepStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepStmt : " << resVar << "<-" << ptrVar << "+" << offsetVar << "\n";
    return rawstr.str();
}

const std::string CmpStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CmpStmt : " << resVar << "<-" << op1Var << " cmpoptor " << op2Var << "\n";
    return rawstr.str();
}

const std::string BinaryStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BinaryStmt : " << resVar << "<-" << op1Var << " binaryoptor " << op2Var << "\n";
    return rawstr.str();
}

const std::string UnaryStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "UnaryStmt : " << resVar << "<-" << "unaryoptor " << opVar << "\n";
    return rawstr.str();
}

const std::string BranchStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BranchStmt : " << "cond:" << condition << "\t" << " branch1:" << br1->toString() 
            << "\t" << " branch2:" << br2->toString()<< "\n";
    return rawstr.str();
}

const std::string CallStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CallStmt : " << value2String(callsite.getCalledValue()) << "\n";
    return rawstr.str();
}

const std::string FunRetStmt::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FunRetStmt : function name " << fun->getName() << "\n";
    return rawstr.str();
}