//===- SliceCondAllocator.h -- Path condition manipulation---------------------//
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

//
// Created by Xiao on 2021/10/21.
//

#include "Util/PathCondAllocator.h"

#ifndef SVF_SLICEDPATHCONDALLOCATOR_H
#define SVF_SLICEDPATHCONDALLOCATOR_H

namespace SVF {
    class SliceCondAllocator : public PathCondAllocator {
    public:
        typedef Set<const ICFGNode *> ICFGNodeSet;
        /// Define worklist
        typedef FIFOWorkList<const ICFGNode *> WorkList;
        typedef Map<const ICFGNode *, u32_t> ICFGNodeVisitNum;

        SliceCondAllocator() = default;

        /// Compute intra-procedural guards between two SVFGNodes (inside same function)
        /// Do program slicing first to only compute condition in the slice between src and dst
        Condition *ComputeIntraVFGGuard(const ICFGNode *src, const ICFGNode *dst) override {
            slicing(src, dst);
            return PathCondAllocator::ComputeIntraVFGGuard(src, dst);
        }

        void clearCFCond() override {
            icfgNodeVisitNum.clear();
            icfgNodePredNum.clear();
            PathCondAllocator::clearCFCond();
        }

    private:

        /// Count incoming intra edges of the node on the slice
        inline u32_t getIntraIncomingEdgeNum(const ICFGNode *icfgNode) {
            ICFGNodeVisitNum::const_iterator it = icfgNodePredNum.find(icfgNode);
            if (it != icfgNodeVisitNum.end())
                return it->second;
            u32_t ct = 0;
            for (const auto &edge: icfgNode->getInEdges()) {
                if (edge->isIntraCFGEdge() && inBackwardSlice(edge->getSrcNode()))
                    ++ct;
            }
            return icfgNodePredNum.emplace(icfgNode, ct).first->second;
        }

        /// When having multiple predecessors, to avoid redundant condition unions after this node,
        /// before the last visit, we only update condition without pushing into worklist.
        /// Except for the following cases:
        ///
        /// (1) the incoming edge is directly from branch and the condition is a True value,
        ///     for the first time visit, we should push it into worklist
        /// e.g.
        ///      1 -> 2 -> 4
        ///      1 -> 4
        ///        1->4 is a True cond because 4 pdom 1.
        ///        1->2->4 will meet a fixed point (every condition OR with True will be True).
        ///        So we should push 4 into worklist at 1->4 to avoid early terminate
        /// (2) cur node is a loop header
        bool setCFCond(const ICFGNode *icfgNode, Condition *cond, bool directFromBranch) override {
            auto it = icfgNodeToCondMap.find(icfgNode);
            if (it != icfgNodeToCondMap.end() && isEquivalentBranchCond(it->second, cond))
                return false;
            // first time updating
            if (it == icfgNodeToCondMap.end())
                icfgNodeVisitNum[icfgNode] = 0;
            icfgNodeVisitNum[icfgNode]++;
            bool toRet = true;
            const Function *fun = icfgNode->getBB()->getParent();
            const LoopInfo *loopInfo = getLoopInfo(fun);
            if (!directFromBranch && !loopInfo->isLoopHeader(icfgNode->getBB()) &&
                getIntraIncomingEdgeNum(icfgNode) > icfgNodeVisitNum[icfgNode])
                toRet = false;

            icfgNodeToCondMap[icfgNode] = cond;
            return toRet;
        }

        /// Forward and backward slice operations
        //@{
        inline void addToForwardSlice(const ICFGNode *node) {
            forwardslice.insert(node);
        }

        inline void addToBackwardSlice(const ICFGNode *node) {
            backwardslice.insert(node);
        }

        inline bool inForwardSlice(const ICFGNode *node) const {
            return forwardslice.find(node) != forwardslice.end();
        }

        inline bool inBackwardSlice(const ICFGNode *node) const {
            return backwardslice.find(node) != backwardslice.end();
        }
        //@}

        inline bool isValidSucc(const ICFGNode *node) const override {
            return inBackwardSlice(node);
        }

        /// forward and backward slicing
        void slicing(const ICFGNode *src, const ICFGNode *dst) {
            clearSlice();
            clearVisited();
            forwardTraverse(src);
            backwardTraverse(dst);
        }

        /// Worklist operations
        //@{
        inline const ICFGNode *popFromWorklist() {
            return workList.pop();
        }

        inline bool pushIntoWorklist(const ICFGNode *item) {
            return workList.push(item);
        }

        inline bool isWorklistEmpty() {
            return workList.empty();
        }
        //@}

        /// Whether has been visited or not, in order to avoid recursion on ICFG
        //@{
        inline bool forwardVisited(const ICFGNode *node) const {
            return forVisited.find(node) != forVisited.end();
        }

        inline void addToForwardVisited(const ICFGNode *node) {
            forVisited.insert(node);
        }

        inline bool backwardVisited(const ICFGNode *node) const {
            return bkVisited.find(node) != bkVisited.end();
        }

        inline void addToBackwardVisited(const ICFGNode *node) {
            bkVisited.insert(node);
        }
        //@}

        /// forward traverse
        virtual void forwardTraverse(const ICFGNode *src) {
            pushIntoWorklist(src);

            while (!isWorklistEmpty()) {
                const ICFGNode *item = popFromWorklist();
                addToForwardSlice(item);
                for (auto it = item->directOutEdgeBegin(); it != item->directOutEdgeEnd(); ++it) {
                    const ICFGNode *succ = (*it)->getDstNode();
                    if (!forwardVisited(succ) && (*it)->isIntraCFGEdge()) {
                        addToForwardVisited(succ);
                        pushIntoWorklist(succ);
                    }
                }
            }
        }

        /// backward traverse solve
        virtual void backwardTraverse(const ICFGNode *dstBB) {
            pushIntoWorklist(dstBB);

            while (!isWorklistEmpty()) {
                const ICFGNode *item = popFromWorklist();
                if (inForwardSlice(item))
                    addToBackwardSlice(item);
                for (auto it = item->directInEdgeBegin(); it != item->directInEdgeEnd(); ++it) {
                    const ICFGNode *pred = (*it)->getSrcNode();
                    if (!backwardVisited(pred) && (*it)->isIntraCFGEdge()) {
                        addToBackwardVisited(pred);
                        pushIntoWorklist(pred);
                    }
                }
            }
        }

        /// clear forward and backward slice
        inline void clearSlice() {
            forwardslice.clear();
            backwardslice.clear();
        };

        /// clear visited set
        inline void clearVisited() {
            forVisited.clear();
            bkVisited.clear();
        };

        ICFGNodeSet forwardslice;
        ICFGNodeSet backwardslice;
        ICFGNodeSet bkVisited;
        ICFGNodeSet forVisited;
        WorkList workList;

        ICFGNodeVisitNum icfgNodeVisitNum;
        ICFGNodeVisitNum icfgNodePredNum;
    };
}
#endif //SVF_SLICEDPATHCONDALLOCATOR_H
