//===- SVFVariables.cpp -- SVF symbols and variables----------------------//
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
 * SVFVariables.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/SVFVariables.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * SVFVar constructor
 */
SVFVar::SVFVar(const Value* val, NodeID i, PNODEK k) :
    GenericPAGNodeTy(i,k), value(val)
{

    assert( ValNode <= k && k <= CloneDummyObjNode && "new SVFIR node kind?");

    switch (k)
    {
    case ValNode:
    case GepValNode:
    {
        assert(val != nullptr && "value is nullptr for ValPN or GepValNode");
        isTLPointer = val->getType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case RetNode:
    {
        assert(val != nullptr && "value is nullptr for RetNode");
        isTLPointer = SVFUtil::cast<Function>(val)->getReturnType()->isPointerTy();
        isATPointer = false;
        break;
    }

    case VarargNode:
    case DummyValNode:
    {
        isTLPointer = true;
        isATPointer = false;
        break;
    }

    case ObjNode:
    case GepObjNode:
    case FIObjNode:
    case DummyObjNode:
    case CloneGepObjNode:
    case CloneFIObjNode:
    case CloneDummyObjNode:
    {
        isTLPointer = false;
        isATPointer = true;
        break;
    }
    }
}

bool SVFVar::isIsolatedNode() const{
	if (getInEdges().empty() && getOutEdges().empty())
		return true;
	else if (isConstantData())
		return true;
	else if (value && SVFUtil::isa<Function>(value))
		return SVFUtil::isIntrinsicFun(SVFUtil::cast<Function>(value));
	else
		return false;
}


const std::string SVFVar::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "SVFVar ID: " << getId();
    return rawstr.str();
}

void SVFVar::dump() const {
    outs() << this->toString() << "\n";
}

const std::string ValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ValPN ID: " << getId();
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string ObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ObjPN ID: " << getId();
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string GepValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepValPN ID: " << getId() << " with offset_" + llvm::utostr(getOffset());
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string GepObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "GepObjPN ID: " << getId() << " with offset_" + llvm::itostr(ls.getOffset());
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string FIObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "FIObjPN ID: " << getId() << " (base object)";
    if (Options::PAGDotGraphShorter) {
        rawstr << "\n";
    }
    rawstr << value2String(value);
    return rawstr.str();
}

const std::string RetPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetPN ID: " << getId() << " unique return node for function " << SVFUtil::cast<Function>(value)->getName();
    return rawstr.str();
}

const std::string VarArgPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VarArgPN ID: " << getId() << " Var arg node for function " << SVFUtil::cast<Function>(value)->getName();
    return rawstr.str();
}

const std::string DummyValPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "DummyValPN ID: " << getId();
    return rawstr.str();
}

const std::string DummyObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "DummyObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneDummyObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneDummyObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneGepObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneGepObjPN ID: " << getId();
    return rawstr.str();
}

const std::string CloneFIObjPN::toString() const {
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "CloneFIObjPN ID: " << getId();
    return rawstr.str();
}



