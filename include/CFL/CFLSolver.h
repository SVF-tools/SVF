/* -------------------- CFLRSolver.h ------------------ */
//
// Created by kisslune on 3/4/22.
//

#ifndef SVF_CFLSOLVER_H
#define SVF_CFLSOLVER_H


#include "CFLGraph.h"

namespace SVF
{

/*!
 * CFL Edge Item
 */
class CFLItem
{
public:
    NodeID src;
    NodeID dst;
    Label lbl;

    // Constructor
    CFLItem(NodeID e1, NodeID e2, Label e3) :
            src(e1), dst(e2), lbl(e3)
    {
    }


    // Destructor
    ~CFLItem()
    {}

    inline bool operator<(const CFLItem &rhs) const
    {
        if (src < rhs.src)
            return true;
        else if (src == rhs.src) {
            if (dst < rhs.dst)
                return true;
            else if (dst == rhs.dst && lbl < rhs.lbl)
                return true;
        }
        return false;
    }

    inline bool operator==(const CFLItem &rhs) const
    {
        return src == rhs.src && dst == rhs.dst && lbl == rhs.lbl;
    }
};


/*
 * Generic CFL solver for demand-driven analysis based on different graphs (e.g. PAG, VFG, ThreadVFG)
 * Extend this class for sophisticated CFL-reachability resolution (e.g. field, flow, path)
 */
class CFLRSolver
{
public:
    typedef FIFOWorkList<CFLItem> WorkList;

protected:
    // Worklist for resolution
    WorkList worklist;
    // CFL graph
    CFLGraph *_graph;
    const NodeBS emptyNodeBS;
    char numOfTypes;

public:
    // Constructor
    CFLRSolver(char n) : _graph(nullptr), numOfTypes(n)
    {
        if (!_graph)
            _graph = new CFLGraph();
    }

    // Destructor
    virtual ~CFLRSolver()
    {
        delete _graph;
    }

    inline CFLGraph *graph()
    {
        return _graph;
    }

    // worklist operations
    //@{
    virtual inline CFLItem popFromWorklist()
    {
        return worklist.pop();
    }

    virtual inline bool pushIntoWorklist(CFLItem item)
    {
        return worklist.push(item);
    }

    virtual inline bool pushIntoWorklist(NodeID src, NodeID dst, LabelType ty, LabelIdx idx)
    {
        return pushIntoWorklist(CFLItem(src, dst, std::make_pair(ty, idx)));
    }

    virtual inline bool isInWorklist(CFLItem item)
    {
        return worklist.find(item);
    }

    virtual inline bool isInWorklist(NodeID src, NodeID dst, LabelType ty, LabelIdx idx)
    {
        return isInWorklist(CFLItem(src, dst, std::make_pair(ty, idx)));
    }

    virtual inline bool isWorklistEmpty()
    {
        return worklist.empty();
    }
    //@}

    // Context-free grammar operations
    //@{
    virtual std::set<Label> unaryDerivation(Label lbl) = 0;         // A ::= B
    virtual Label binaryDerivation(Label llbl, Label rlbl) = 0;     // A ::= B C
    //@}

    virtual void solveWorklist()
    {
        while (!isWorklistEmpty()) {
            CFLItem item = popFromWorklist();
            processCFLItem(item);
        }
    }

    virtual void processCFLItem(CFLItem item)   // item: v_1 --> v_2
    {
        for (auto lbl: unaryDerivation(item.lbl))
            if (graph()->addEdge(item.src, item.dst, lbl.first, lbl.second))
                pushIntoWorklist(item.src, item.dst, lbl.first, lbl.second);


        for (LabelType ty = 0; ty < numOfTypes; ++ty) {
            // process v_3 --> v_1 --> v_2
            for (auto it: graph()->getPredIndList(ty)) {
                Label lbl = std::make_pair(ty, it.first);
                Label newLbl = binaryDerivation(lbl, item.lbl);
                if (!newLbl.first)
                    continue;
                NodeBS newSrcs = graph()->addEdges(it.second[item.src], item.dst, newLbl.first, newLbl.second);
                for (NodeID src : newSrcs)
                    pushIntoWorklist(src, item.dst, newLbl.first, newLbl.second);
            }
            // process v_1 --> v_2 --> v_3
            for (auto &it: graph()->getSuccIndList(ty)) {
                Label lbl = std::make_pair(ty, it.first);
                Label newLbl = binaryDerivation(item.lbl, lbl);
                if (!newLbl.first)
                    continue;
                NodeBS newDsts = graph()->addEdges(item.src, it.second[item.dst], newLbl.first, newLbl.second);
                for (NodeID dst : newDsts)
                    pushIntoWorklist(item.src, dst, newLbl.first, newLbl.second);
            }
        }
    };

    // TODO:
    virtual void solve() = 0;
};

}   // end namespace


/*!
 * hash function
 */
template <> struct std::hash<SVF::CFLItem> {
    size_t operator()(const SVF::CFLItem &item) const {
        // Make sure our assumptions are sound: use u32_t
        // and u64_t. If NodeID is not actually u32_t or size_t
        // is not u64_t we should be fine since we get a
        // consistent result.
        uint32_t first = (uint32_t)(item.src);
        uint32_t second = (uint32_t)(item.dst);
        return ((uint64_t)(first) << 32) | (uint64_t)(second);
    }
};

#endif //SVF_CFLSOLVER_H
