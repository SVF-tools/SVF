//
// Created by Jiawei Wang on 6/14/22.
//

#ifndef SVF_SVFLOOP_H
#define SVF_SVFLOOP_H

#include "Util/SVFBasicTypes.h"

namespace SVF {
    class SVFLoop {
        typedef Set<const ICFGEdge *> ICFGEdgeSet;
        typedef Set<const ICFGNode *> ICFGNodeSet;
    private:
        ICFGEdgeSet entryICFGEdges, backICFGEdges, inICFGEdges, outICFGEdges;
        ICFGNodeSet icfgNodes;
        u32_t loopBound;

    public:
        SVFLoop(const ICFGNodeSet &_nodes, u32_t _bound) :
                icfgNodes(_nodes), loopBound(_bound) {

        }

        inline ICFGNodeSet::iterator ICFGNodesBegin() {
            return icfgNodes.begin();
        }

        inline ICFGNodeSet::iterator ICFGNodesEnd() {
            return icfgNodes.end();
        }

        inline bool isInLoop(const ICFGNode *icfgNode) const {
            return icfgNodes.find(icfgNode) != icfgNodes.end();
        }

        inline bool isEntryICFGEdge(const ICFGEdge *edge) const {
            return entryICFGEdges.find(edge) != entryICFGEdges.end();
        }

        inline bool isBackICFGEdge(const ICFGEdge *edge) const {
            return backICFGEdges.find(edge) != entryICFGEdges.end();
        }

        inline bool isInICFGEdge(const ICFGEdge *edge) const {
            return inICFGEdges.find(edge) != entryICFGEdges.end();
        }

        inline bool isOutICFGEdge(const ICFGEdge *edge) const {
            return outICFGEdges.find(edge) != entryICFGEdges.end();
        }

        inline void addEntryICFGEdge(const ICFGEdge *edge) {
            entryICFGEdges.insert(edge);
        }

        inline ICFGEdgeSet::iterator entryICFGEdgesBegin() {
            return entryICFGEdges.begin();
        }

        inline ICFGEdgeSet::iterator entryICFGEdgesEnd() {
            return entryICFGEdges.end();
        }

        inline void addOutICFGEdge(const ICFGEdge *edge) {
            outICFGEdges.insert(edge);
        }

        inline ICFGEdgeSet::iterator outICFGEdgesBegin() {
            return outICFGEdges.begin();
        }

        inline ICFGEdgeSet::iterator outICFGEdgesEnd() {
            return outICFGEdges.end();
        }

        inline void addBackICFGEdge(const ICFGEdge *edge) {
            backICFGEdges.insert(edge);
        }

        inline ICFGEdgeSet::iterator backICFGEdgesBegin() {
            return backICFGEdges.begin();
        }

        inline ICFGEdgeSet::iterator backICFGEdgesEnd() {
            return backICFGEdges.end();
        }

        inline void addInICFGEdge(const ICFGEdge *edge) {
            inICFGEdges.insert(edge);
        }

        inline ICFGEdgeSet::iterator inEdgesBegin() {
            return inICFGEdges.begin();
        }

        inline ICFGEdgeSet::iterator inEdgesEnd() {
            return inICFGEdges.end();
        }

        inline void setLoopBound(u32_t _bound) { loopBound = _bound; }

        inline u32_t getLoopBound() const { return loopBound; }
    };

};

#endif //SVF_SVFLOOP_H
