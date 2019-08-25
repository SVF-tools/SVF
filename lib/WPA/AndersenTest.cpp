//===- AndersenSCD.cpp -- SCD based field-sensitive Andersen's analysis-------//
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

/*
 * AndersenSCD.cpp
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#include "WPA/AndersenTest.h"


/*!
 *
 */
void AndersenTest::analyze(SVFModule svfModule) {
    bool f;
    NodeBS a, b;
    for (unsigned i = 0; i < 10000; ++i)
        a.set(i);
    for (unsigned i = 0; i < 10000; ++i)
        b.set(i);

    f = intersect(a,b);
}


/*!
 * 
 */
bool AndersenTest::intersect(NodeBS& a, NodeBS& b) {
    return a &= b;
}
