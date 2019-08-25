//===- AndersenTest.h -- Only for test!! -------------//
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
 * AndersenTest.h
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#ifndef PROJECT_ANDERSENTEST_H
#define PROJECT_ANDERSENTEST_H

#include "Andersen.h"


/*!
 *
 */
class AndersenTest : public Andersen {
public:
    AndersenTest(PTATY type = AndersenTest_WPA): Andersen(type) {}

    void analyze(SVFModule svfModule);
    bool intersect(NodeBS& a, NodeBS& b);
};

#endif //PROJECT_ANDERSENTEST_H
