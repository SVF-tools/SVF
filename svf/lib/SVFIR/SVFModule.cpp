//===- SVFModule.cpp -- Helper functions for pointer analysis--------------//
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
//===----------------------------------------------------------------------===//


#include "SVFIR/SVFModule.h"
#include "SVFIR/SymbolTableInfo.h"
#include "Util/SVFUtil.h"
#include "Util/SVFStat.h"
#include "Util/Options.h"

using namespace SVF;

std::string SVFModule::pagReadFromTxt = "";
SVFModule* SVFModule::svfModule = nullptr;

SVFModule::~SVFModule()
{
    for (const SVFFunction* f : FunctionSet)
        delete f;
    for (const SVFConstant* c : ConstantSet)
        delete c;
    for (const SVFValue* o : OtherValueSet)
        delete o;
    NodeIDAllocator::unset();
    ThreadAPI::destroy();
    ExtAPI::destory();
}

const SVFFunction* SVFModule::getSVFFunction(const std::string& name)
{
    for (const SVFFunction* fun : getFunctionSet())
    {
        if (fun->getName() == name)
        {
            return fun;
        }
    }
    return nullptr;
}

SVFModule* SVFModule::getSVFModule()
{
    if (svfModule == nullptr)
    {
        svfModule = new SVFModule;
    }
    return svfModule;
}

void SVFModule::releaseSVFModule()
{
    assert(svfModule != nullptr && "SVFModule is not initialized?");
    delete svfModule;
    svfModule = nullptr;
}
