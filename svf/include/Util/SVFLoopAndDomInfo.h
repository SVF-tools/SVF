//===- SVFLoopAndDomInfo.h -- ------------------------------//
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
* SVFLoopAndDomInfo.h
*
*  Created on: Feb 7, 2025
*      Author: Xiao Cheng
*
*/

#ifndef SVFLOOPANDDOMINFO_H
#define SVFLOOPANDDOMINFO_H

#include "SVFIR/SVFType.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"
#include "Graphs/BasicBlockG.h"

namespace SVF
{
class SVFLoopAndDomInfo
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class GraphDBClient;


public:
    typedef Set<const SVFBasicBlock*> BBSet;
    typedef std::vector<const SVFBasicBlock*> BBList;
    typedef BBList LoopBBs;

private:
    BBList reachableBBs;    ///< reachable BasicBlocks from the function entry.
    Map<const SVFBasicBlock*,BBSet> dtBBsMap;   ///< map a BasicBlock to BasicBlocks it Dominates
    Map<const SVFBasicBlock*,BBSet> pdtBBsMap;   ///< map a BasicBlock to BasicBlocks it PostDominates
    Map<const SVFBasicBlock*,BBSet> dfBBsMap;    ///< map a BasicBlock to its Dominate Frontier BasicBlocks
    Map<const SVFBasicBlock*, LoopBBs> bb2LoopMap;  ///< map a BasicBlock (if it is in a loop) to all the BasicBlocks in this loop
    Map<const SVFBasicBlock*, u32_t> bb2PdomLevel;  ///< map a BasicBlock to its level in pdom tree, used in findNearestCommonPDominator
    Map<const SVFBasicBlock*, const SVFBasicBlock*> bb2PIdom;  ///< map a BasicBlock to its immediate dominator in pdom tree, used in findNearestCommonPDominator
    
protected:
inline void setDomTreeMap(Map<const SVFBasicBlock*,BBSet>& dtMap)
{
    dtBBsMap = dtMap;
}

inline void setPostDomTreeMap(Map<const SVFBasicBlock*,BBSet>& pdtMap)
{
    pdtBBsMap = pdtMap;
}

inline void setDomFrontierMap(Map<const SVFBasicBlock*,BBSet>& dfMap)
{
    dfBBsMap = dfMap;
}

inline void setBB2LoopMap(Map<const SVFBasicBlock*, LoopBBs>& bb2Loop)
{
    bb2LoopMap = bb2Loop;
}

inline void setBB2PdomLevel(Map<const SVFBasicBlock*, u32_t>& bb2Pdom)
{
    bb2PdomLevel = bb2Pdom;
}

inline void setBB2PIdom(Map<const SVFBasicBlock*, const SVFBasicBlock*>& bb2PIdomMap)
{
    bb2PIdom = bb2PIdomMap;
}

public:
    SVFLoopAndDomInfo()
    {
    }

    virtual ~SVFLoopAndDomInfo() {}

    inline const Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap() const
    {
        return dfBBsMap;
    }

    inline Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap()
    {
        return dfBBsMap;
    }

    inline const Map<const SVFBasicBlock*, LoopBBs>& getBB2LoopMap() const
    {
        return bb2LoopMap;
    }
    
    inline bool hasLoopInfo(const SVFBasicBlock* bb) const
    {
        return bb2LoopMap.find(bb) != bb2LoopMap.end();
    }

    const LoopBBs& getLoopInfo(const SVFBasicBlock* bb) const;

    inline const SVFBasicBlock* getLoopHeader(const LoopBBs& lp) const
    {
        assert(!lp.empty() && "this is not a loop, empty basic block");
        return lp.front();
    }

    inline bool loopContainsBB(const LoopBBs& lp, const SVFBasicBlock* bb) const
    {
        return std::find(lp.begin(), lp.end(), bb) != lp.end();
    }

    inline void addToBB2LoopMap(const SVFBasicBlock* bb, const SVFBasicBlock* loopBB)
    {
        bb2LoopMap[bb].push_back(loopBB);
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getPostDomTreeMap() const
    {
        return pdtBBsMap;
    }

    inline Map<const SVFBasicBlock*,BBSet>& getPostDomTreeMap()
    {
        return pdtBBsMap;
    }

    inline const Map<const SVFBasicBlock*,u32_t>& getBBPDomLevel() const
    {
        return bb2PdomLevel;
    }

    inline Map<const SVFBasicBlock*,u32_t>& getBBPDomLevel()
    {
        return bb2PdomLevel;
    }

    inline const Map<const SVFBasicBlock*,const SVFBasicBlock*>& getBB2PIdom() const
    {
        return bb2PIdom;
    }

    inline Map<const SVFBasicBlock*,const SVFBasicBlock*>& getBB2PIdom()
    {
        return bb2PIdom;
    }


    inline Map<const SVFBasicBlock*,BBSet>& getDomTreeMap()
    {
        return dtBBsMap;
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomTreeMap() const
    {
        return dtBBsMap;
    }

    inline bool isUnreachable(const SVFBasicBlock* bb) const
    {
        return std::find(reachableBBs.begin(), reachableBBs.end(), bb) ==
               reachableBBs.end();
    }

    inline const BBList& getReachableBBs() const
    {
        return reachableBBs;
    }

    inline void setReachableBBs(BBList& bbs)
    {
        reachableBBs = bbs;
    }

    void getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const;

    bool isLoopHeader(const SVFBasicBlock* bb) const;

    bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

    bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

    /// find nearest common post dominator of two basic blocks
    const SVFBasicBlock *findNearestCommonPDominator(const SVFBasicBlock *A, const SVFBasicBlock *B) const;
};
}

#endif //SVFLOOPANDDOMINFO_H
