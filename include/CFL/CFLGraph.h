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

/*!
 *
 */
class CFGNodeBSTmpl
{
public:
    typedef char Type;      // edge labels
    typedef short Index;    // indices of labels
    typedef std::unordered_map<NodeID, NodeBS> AdjacencyList;
    typedef std::unordered_map<Index, AdjacencyList> IndexList;

protected:
    IndexList *predMap[MAX_SYMBOL_LIMIT];   // four layers: label-->index-->adjacency list
    IndexList *succMap[MAX_SYMBOL_LIMIT];
    const NodeBS emptyNodeBS;
    NodeBS diff;

    // union/add data
    //@{
    inline bool addPred(const NodeID key, const NodeID val, const Type ty, const Index ind)
    {
        return (*predMap[ty])[ind][key].test_and_set(val);
    };

    inline bool addSucc(const NodeID key, const NodeID val, const Type ty, const Index ind)
    {
        return (*succMap[ty])[ind][key].test_and_set(val);
    };

    inline bool addPreds(const NodeID key, const NodeBS &data, const Type ty, const Index ind)
    {
        if (data.empty())
            return false;
        return (*predMap[ty])[ind][key] |= data;
    }

    inline bool addSuccs(const NodeID key, const NodeBS &data, const Type ty, const Index ind)
    {
        if (data.empty())
            return false;
        return (*succMap[ty])[ind][key] |= data;
    }

    inline void removePred(const NodeID key, const NodeID val, const Type ty, const Index ind)
    {
        if ((*predMap[ty])[ind].find(key) == (*predMap[ty])[ind].end())
            return;
        (*predMap[ty])[ind][key].reset(val);
    }

    inline void removeSucc(const NodeID key, const NodeID val, const Type ty, const Index ind)
    {
        if ((*succMap[ty])[ind].find(key) == (*succMap[ty])[ind].end())
            return;
        (*succMap[ty])[ind][key].reset(val);
    }

    inline void removePreds(const NodeID key, const NodeBS &data, const Type ty, const Index ind)
    {
        if ((*predMap[ty])[ind].find(key) == (*predMap[ty])[ind].end())
            return;
        (*predMap[ty])[ind][key].intersectWithComplement(data);
    }

    inline void removeSuccs(const NodeID key, const NodeBS &data, const Type ty, const Index ind)
    {
        if ((*succMap[ty])[ind].find(key) == (*succMap[ty])[ind].end())
            return;
        (*succMap[ty])[ind][key].intersectWithComplement(data);
    }
    //@}

public:
    // Constructor
    CFGNodeBSTmpl()
    {
        for (char i = 0; i < MAX_SYMBOL_LIMIT; ++i) {
            predMap[i] = new IndexList();
            succMap[i] = new IndexList();
        }
    }

    // Destructor
    virtual ~CFGNodeBSTmpl()
    {
        for (char i = 0; i < MAX_SYMBOL_LIMIT; ++i) {
            delete predMap[i];
            delete succMap[i];
        }
    }

    // lookups
    //@{
    inline const NodeBS &getPreds(const NodeID key, const Type ty, const Index ind)
    {
        if ((*predMap[ty])[ind].find(key) == (*predMap[ty])[ind].end())
            return emptyNodeBS;
        return (*predMap[ty])[ind][key];
    }

    inline const NodeBS &getSuccs(const NodeID key, const Type ty, const Index ind)
    {
        if ((*succMap[ty])[ind].find(key) == (*succMap[ty])[ind].end())
            return emptyNodeBS;
        return (*succMap[ty])[ind][key];
    }
    //@}

    // Edge operations
    //@{
    inline void addEdge(const NodeID src, const NodeID dst, const Type ty, const Index ind = 0)
    {
        addPred(dst, src, ty, ind);
        addSucc(src, dst, ty, ind);
    }

    inline void addEdges(const NodeID src, const NodeBS &dstNodeBS, const Type ty, const Index ind = 0)
    {
        if (addSuccs(src, dstNodeBS, ty, ind)) {
            for (const NodeID datum: dstNodeBS)
                addPred(datum, src, ty, ind);
        }
    }

    inline void addEdges(const NodeBS &srcNodeBS, const NodeID dst, const Type ty, const Index ind = 0)
    {
        if (addPreds(dst, srcNodeBS, ty, ind)) {
            for (const NodeID datum: srcNodeBS)
                addSucc(datum, dst, ty, ind);
        }
    }

    inline bool hasEdge(const NodeID src, const NodeID dst, const Type ty, const Index ind)
    {
        auto it = (*succMap[ty])[ind].find(src);
        if (it == (*succMap[ty])[ind].end())
            return false;
        return (*succMap[ty])[ind][src].test(dst);
    }
    //@}

    /* Diff operations */
    //@{
    inline NodeBS &getDiffNodeBS()
    {
        diff.clear();
        return diff;
    }

    inline NodeBS &computeDiffNodeBS(const NodeID src, const NodeID dst, const Type ty, const Index ind)
    {
        diff.clear();
        if (!hasEdge(src, dst, ty, ind))
            diff.set(dst);
        return diff;
    }

    inline NodeBS &computeDiffNodeBS(const NodeID src, const NodeBS &dstNodeBS, const Type ty, const Index ind)
    {
        diff.clear();
        diff.intersectWithComplement(dstNodeBS, getSuccs(src, ty, ind));
        return diff;
    }

    inline NodeBS &computeDiffNodeBS(const NodeBS &srcNodeBS, const NodeID dst, const Type ty, const Index ind)
    {
        diff.clear();
        diff.intersectWithComplement(srcNodeBS, getPreds(dst, ty, ind));
        return diff;
    }
    //@}

};


}   // end namespace

#endif //SVF_CFLGRAPH_H
