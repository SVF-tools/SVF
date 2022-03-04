/* -------------------- CFLGraph.h ------------------ */
//
// Created by kisslune on 3/3/22.
//

#ifndef SVF_CFLGRAPH_H
#define SVF_CFLGRAPH_H


#include "Util/WorkList.h"

#define MAX_SYMBOL_LIMIT 32


namespace SVF
{
typedef char LabelType;      // edge labels
typedef short LabelIdx;    // indices of labels
typedef std::pair<LabelType, LabelIdx> Label;

/*!
 *
 */
class CFLGraph
{
public:
    typedef std::unordered_map<NodeID, NodeBS> AdjacencyList;
    typedef std::unordered_map<LabelIdx, AdjacencyList> IndexList;

protected:
    IndexList *predMap[MAX_SYMBOL_LIMIT];   // four layers: label-->index-->adjacency list
    IndexList *succMap[MAX_SYMBOL_LIMIT];   // TODO: vectorize
    const NodeBS emptyNodeBS;
    NodeBS diff;

    // union/add data
    //@{
    inline bool addPred(const NodeID key, const NodeID val, const LabelType ty, const LabelIdx idx)
    {
        return (*predMap[ty])[idx][key].test_and_set(val);
    };

    inline bool addSucc(const NodeID key, const NodeID val, const LabelType ty, const LabelIdx idx)
    {
        return (*succMap[ty])[idx][key].test_and_set(val);
    };

    inline bool addPreds(const NodeID key, const NodeBS &data, const LabelType ty, const LabelIdx idx)
    {
        if (data.empty())
            return false;
        return (*predMap[ty])[idx][key] |= data;
    }

    inline bool addSuccs(const NodeID key, const NodeBS &data, const LabelType ty, const LabelIdx idx)
    {
        if (data.empty())
            return false;
        return (*succMap[ty])[idx][key] |= data;
    }

    inline void removePred(const NodeID key, const NodeID val, const LabelType ty, const LabelIdx idx)
    {
        if ((*predMap[ty])[idx].find(key) == (*predMap[ty])[idx].end())
            return;
        (*predMap[ty])[idx][key].reset(val);
    }

    inline void removeSucc(const NodeID key, const NodeID val, const LabelType ty, const LabelIdx idx)
    {
        if ((*succMap[ty])[idx].find(key) == (*succMap[ty])[idx].end())
            return;
        (*succMap[ty])[idx][key].reset(val);
    }

    inline void removePreds(const NodeID key, const NodeBS &data, const LabelType ty, const LabelIdx idx)
    {
        if ((*predMap[ty])[idx].find(key) == (*predMap[ty])[idx].end())
            return;
        (*predMap[ty])[idx][key].intersectWithComplement(data);
    }

    inline void removeSuccs(const NodeID key, const NodeBS &data, const LabelType ty, const LabelIdx idx)
    {
        if ((*succMap[ty])[idx].find(key) == (*succMap[ty])[idx].end())
            return;
        (*succMap[ty])[idx][key].intersectWithComplement(data);
    }
    //@}

public:
    // Constructor
    CFLGraph()
    {
        for (char i = 0; i < MAX_SYMBOL_LIMIT; ++i) {
            predMap[i] = new IndexList();
            succMap[i] = new IndexList();
        }
    }

    // Destructor
    virtual ~CFLGraph()
    {
        for (char i = 0; i < MAX_SYMBOL_LIMIT; ++i) {
            delete predMap[i];
            delete succMap[i];
        }
    }

    // lookups
    //@{
    inline IndexList& getPredIndList(char ty)
    {
        return (*predMap[ty]);
    }

    inline IndexList& getSuccIndList(char ty)
    {
        return (*succMap[ty]);
    }

    inline const NodeBS &getPreds(const NodeID key, const LabelType ty, const LabelIdx idx)
    {
        if ((*predMap[ty])[idx].find(key) == (*predMap[ty])[idx].end())
            return emptyNodeBS;
        return (*predMap[ty])[idx][key];
    }

    inline const NodeBS &getSuccs(const NodeID key, const LabelType ty, const LabelIdx idx)
    {
        if ((*succMap[ty])[idx].find(key) == (*succMap[ty])[idx].end())
            return emptyNodeBS;
        return (*succMap[ty])[idx][key];
    }
    //@}

    // Edge operations
    //@{
    inline bool hasEdge(const NodeID src, const NodeID dst, const LabelType ty, const LabelIdx idx)
    {
        auto it = (*succMap[ty])[idx].find(src);
        if (it == (*succMap[ty])[idx].end())
            return false;
        return (*succMap[ty])[idx][src].test(dst);
    }

    inline bool addEdge(const NodeID src, const NodeID dst, const LabelType ty, const LabelIdx idx = 0)
    {
        addPred(dst, src, ty, idx);
        return addSucc(src, dst, ty, idx);
    }

    inline NodeBS addEdges(const NodeID src, const NodeBS &dstNodeBS, const LabelType ty, const LabelIdx idx = 0)
    {
        NodeBS newDsts;
        if (addSuccs(src, dstNodeBS, ty, idx)) {
            for (const NodeID datum: dstNodeBS)
                if (addPred(datum, src, ty, idx))
                    newDsts.set(datum);
        }
        return newDsts;
    }

    inline NodeBS addEdges(const NodeBS &srcNodeBS, const NodeID dst, const LabelType ty, const LabelIdx idx = 0)
    {
        NodeBS newSrcs;
        if (addPreds(dst, srcNodeBS, ty, idx)) {
            for (const NodeID datum: srcNodeBS)
                if (addSucc(datum, dst, ty, idx))
                    newSrcs.set(datum);
        }
        return newSrcs;
    }
    //@}
};


}   // end namespace

#endif //SVF_CFLGRAPH_H
