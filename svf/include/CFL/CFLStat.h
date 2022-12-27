//===- CFLStat.h -- CFL statistics--------------------------------------------//
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


/*
 * CFLStat.h
 *
 *  Created on: 17/9/2022
 *      Author: Pei Xu
 */

#ifndef CFL_CFLSTAT_H_
#define CFL_CFLSTAT_H_

#include "Util/PTAStat.h"
#include "CFL/CFLAlias.h"
#include "CFL/CFLVF.h"

namespace SVF
{

/*!
 * Statistics of CFL's analysis
 */
class CFLStat : public PTAStat
{
private:
    CFLBase* pta;

public:
    CFLStat(CFLBase* p);

    virtual ~CFLStat()
    {
    }

    virtual void performStat();

    void CFLGraphStat();

    void CFLGrammarStat();

    void CFLSolverStat();
};

} // End namespace SVF

#endif /* CFL_CFLSTAT_H_ */
