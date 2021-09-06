//===- SVFModule.cpp -- Helper functions for pointer analysis--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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


#include "Util/SVFModule.h"
#include "SVF-FE/SymbolTableInfo.h"
#include "Util/SVFUtil.h"

using namespace SVF;

void SVFModule::buildSymbolTableInfo()
{
    if (!pagReadFromTXT())
    {
        /// building symbol table
        DBOUT(DGENERAL, SVFUtil::outs() << SVFUtil::pasMsg("Building Symbol table ...\n"));
        SymbolTableInfo *symInfo = SymbolTableInfo::SymbolInfo();
        symInfo->buildMemModel(this);
    }
}