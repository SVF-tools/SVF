//===- ICFGStat.h ----------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFGStat.h
 *
 *  Created on: 12Sep.,2018
 *      Author: yulei
 */

#ifndef INCLUDE_UTIL_ICFGSTAT_H_
#define INCLUDE_UTIL_ICFGSTAT_H_

#include "Util/PTAStat.h"
class ICFGNode;

class ICFGStat : public PTAStat{


public:
    typedef std::set<const ICFGNode*> ICFGNodeSet;

    ICFGStat() : PTAStat(NULL){

	}

private:
    ICFGNodeSet forwardSlice;
    ICFGNodeSet backwardSlice;
    ICFGNodeSet	sources;
    ICFGNodeSet	sinks;

public:
    inline void addToSources(const ICFGNode* node) {
        sources.insert(node);
    }
    inline void addToSinks(const ICFGNode* node) {
        sinks.insert(node);
    }
    inline void addToForwardSlice(const ICFGNode* node) {
        forwardSlice.insert(node);
    }
    inline void addToBackwardSlice(const ICFGNode* node) {
        backwardSlice.insert(node);
    }
    inline bool inForwardSlice(const ICFGNode* node) const {
        return forwardSlice.find(node)!=forwardSlice.end();
    }
    inline bool inBackwardSlice(const ICFGNode* node) const {
        return backwardSlice.find(node)!=backwardSlice.end();
    }
    inline bool isSource(const ICFGNode* node) const {
        return sources.find(node)!=sources.end();
    }
    inline bool isSink(const ICFGNode* node) const {
        return sinks.find(node)!=sinks.end();
    }
};



#endif /* INCLUDE_UTIL_ICFGSTAT_H_ */
